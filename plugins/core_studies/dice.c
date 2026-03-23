/*
 * dice -- Classic PNG dice test image, rendered as stochastic SDF glass.
 *
 * Four translucent colored dice with correct premultiplied alpha.
 * Rounded-box SDF geometry with sphere-traced ray marching,
 * Beer-Lambert absorption for the colored glass body, volume scatter
 * for pigmented body color, and Fresnel-split reflection/refraction.
 *
 * Each frame emits one stochastic sample per pixel; the temporal
 * accumulation framework averages them for convergence.
 *
 * Technique summary:
 *   - Rounded box SDF: length(max(|p| - b, 0)) + min(max_comp(|p| - b), 0) - r
 *   - Rodrigues rotation for die orientations
 *   - Beer-Lambert: transmittance = exp(-sigma * (1 - tint) * distance)
 *   - Schlick Fresnel at every glass interface
 *   - Soft shadow via SDF over-relaxation
 *   - Pip detection by face-projected distance checks
 */

#include "dice.h"

/* ------------------------------------------------------------------ */
/*  Tuning                                                             */
/* ------------------------------------------------------------------ */

#define MAX_BOUNCES      6
#define MAX_MARCH_STEPS  80
#define MARCH_EPS        0.0004f
#define MAX_DIST         50.0f
#define NORM_EPS         0.0002f

#define NUM_DICE         4
#define DIE_HALF         0.50f     /* half-edge before rounding          */
#define DIE_ROUND        0.07f     /* corner rounding radius             */
#define DIE_IOR          1.49f     /* crown glass                        */
#define PIP_RAD          0.14f     /* pip radius in face-normal coords   */
#define PIP_SPC          0.34f     /* pip center offset from face center */

/* Pre-normalized key light direction: (1, 1.8, -0.6) / 2.1448 */
static const vec3_t LIGHT_DIR = { 0.4663f, 0.8393f, -0.2798f };

/* ================================================================== */
/*  Die layout                                                         */
/* ================================================================== */

typedef struct {
    vec3_t pos;
    vec3_t rot_axis;      /* normalized internally by rodrigues()   */
    float  rot_angle;     /* radians                                */
    vec3_t tint;          /* body color -- higher = more transparent */
    float  sigma;         /* absorption density                     */
} DieSpec;

static const DieSpec g_dice[NUM_DICE] = {
    /* Red:    nearest, front-left */
    { {-0.70f, 0.52f, -1.10f},
      { 0.20f, 1.00f,  0.30f},  0.35f,
      { 0.97f, 0.12f,  0.08f},  2.0f },

    /* Yellow: near-right */
    { { 0.80f, 0.52f, -0.20f},
      { 0.10f, 1.00f,  0.40f},  2.15f,
      { 0.97f, 0.88f,  0.08f},  1.6f },

    /* Blue:   mid-right, near focal plane */
    { { 0.55f, 0.52f,  0.90f},
      { 0.30f, 1.00f, -0.20f},  1.05f,
      { 0.08f, 0.18f,  0.97f},  1.8f },

    /* Green:  farthest, back-left */
    { {-0.40f, 0.52f,  2.10f},
      {-0.40f, 1.00f,  0.10f},  0.70f,
      { 0.08f, 0.90f,  0.12f},  1.9f },
};

/*
 * Standard Western die face assignment.
 * Opposite faces sum to 7.
 * Index: 0=+X  1=-X  2=+Y  3=-Y  4=+Z  5=-Z
 */
static const int FACE_PIPS[6] = { 3, 4, 1, 6, 5, 2 };

/* ================================================================== */
/*  Rodrigues rotation                                                 */
/* ================================================================== */

static vec3_t rodrigues(vec3_t v, vec3_t axis, float angle)
{
    vec3_t k = v3_normalize(axis);
    float  c = cosf(angle), s = sinf(angle);
    /* v' = v*cos + (k x v)*sin + k*(k.v)*(1-cos) */
    return v3_add(v3_add(v3_mul1(v, c),
                         v3_mul1(v3_cross(k, v), s)),
                  v3_mul1(k, v3_dot(k, v) * (1.0f - c)));
}

/* ================================================================== */
/*  SDF: rounded box                                                   */
/* ================================================================== */

static float sdf_rbox(vec3_t p, float h, float r)
{
    float qx = fabsf(p.x) - h;
    float qy = fabsf(p.y) - h;
    float qz = fabsf(p.z) - h;
    float ox = fmaxf(qx, 0.0f);
    float oy = fmaxf(qy, 0.0f);
    float oz = fmaxf(qz, 0.0f);
    return sqrtf(ox * ox + oy * oy + oz * oz)
         + fminf(fmaxf(qx, fmaxf(qy, qz)), 0.0f) - r;
}

static float sdf_die(vec3_t p, int idx)
{
    vec3_t q = v3_sub(p, g_dice[idx].pos);
    q = rodrigues(q, g_dice[idx].rot_axis, -g_dice[idx].rot_angle);
    return sdf_rbox(q, DIE_HALF - DIE_ROUND, DIE_ROUND);
}

static float sdf_scene(vec3_t p, int *out_idx)
{
    float best = MAX_DIST;
    int   bi   = -1;
    for (int i = 0; i < NUM_DICE; i++) {
        float d = sdf_die(p, i);
        if (d < best) { best = d; bi = i; }
    }
    *out_idx = bi;
    return best;
}

/* ================================================================== */
/*  Surface normal (central-difference gradient of the SDF)            */
/* ================================================================== */

static vec3_t sdf_normal(vec3_t p, int idx)
{
    float e = NORM_EPS;
    float c = sdf_die(p, idx);
    return v3_normalize(vec3(
        sdf_die(vec3(p.x + e, p.y,     p.z    ), idx) - c,
        sdf_die(vec3(p.x,     p.y + e, p.z    ), idx) - c,
        sdf_die(vec3(p.x,     p.y,     p.z + e), idx) - c));
}

/* ================================================================== */
/*  Pip detection                                                      */
/* ================================================================== */

static bool pip_at(float u, float v, float pu, float pv, float r2)
{
    float du = u - pu, dv = v - pv;
    return du * du + dv * dv < r2;
}

/*
 * Given a point in die-local space (after inverse rotation), determine
 * which face it's nearest to and test all pip positions for that face.
 */
static bool is_pip(vec3_t loc)
{
    float ax = fabsf(loc.x), ay = fabsf(loc.y), az = fabsf(loc.z);
    int   face;
    float u, v;

    if (ay >= ax && ay >= az) {
        face = (loc.y > 0.0f) ? 2 : 3;
        u = loc.x / DIE_HALF;
        v = loc.z / DIE_HALF;
    } else if (ax >= az) {
        face = (loc.x > 0.0f) ? 0 : 1;
        u = loc.z / DIE_HALF;
        v = loc.y / DIE_HALF;
    } else {
        face = (loc.z > 0.0f) ? 4 : 5;
        u = loc.x / DIE_HALF;
        v = loc.y / DIE_HALF;
    }

    int   n  = FACE_PIPS[face];
    float d  = PIP_SPC;
    float r2 = PIP_RAD * PIP_RAD;

    switch (n) {
    case 1:
        return pip_at(u, v, 0, 0, r2);
    case 2:
        return pip_at(u, v, -d, d, r2) || pip_at(u, v, d, -d, r2);
    case 3:
        return pip_at(u, v, -d, d, r2) || pip_at(u, v, 0, 0, r2) ||
               pip_at(u, v, d, -d, r2);
    case 4:
        return pip_at(u, v, -d, -d, r2) || pip_at(u, v, -d, d, r2) ||
               pip_at(u, v, d, -d, r2)  || pip_at(u, v, d, d, r2);
    case 5:
        return pip_at(u, v, -d, -d, r2) || pip_at(u, v, -d, d, r2) ||
               pip_at(u, v, 0, 0, r2)   ||
               pip_at(u, v, d, -d, r2)  || pip_at(u, v, d, d, r2);
    case 6:
        return pip_at(u, v, -d, -d, r2) || pip_at(u, v, -d, 0, r2) ||
               pip_at(u, v, -d, d, r2)  ||
               pip_at(u, v, d, -d, r2)  || pip_at(u, v, d, 0, r2) ||
               pip_at(u, v, d, d, r2);
    }
    return false;
}

/* ================================================================== */
/*  Environment (reflection-only -- never seen directly)               */
/* ================================================================== */

/*
 * The sky dome provides the reflected environment on glass surfaces.
 * It is never seen directly; rays that miss all dice return alpha = 0.
 */
static vec3_t sky(vec3_t rd)
{
    float t = saturate(0.5f + 0.5f * rd.y);
    vec3_t col = v3_lerp(vec3(0.18f, 0.22f, 0.32f),
                         vec3(0.50f, 0.55f, 0.68f), t);

    /* Key light hot spot for specular highlights on glass. */
    float sun = saturate(v3_dot(rd, LIGHT_DIR));
    sun = powf(sun, 48.0f);
    col = v3_add(col, v3_mul1(vec3(6.0f, 5.5f, 4.5f), sun));

    return col;
}

/* ================================================================== */
/*  Ray marching                                                       */
/* ================================================================== */

/* March outside all dice toward the nearest surface. */
static float march_ext(vec3_t ro, vec3_t rd, int *out_idx)
{
    float t = 0.0f;
    for (int i = 0; i < MAX_MARCH_STEPS; i++) {
        int   idx;
        float d = sdf_scene(v3_add(ro, v3_mul1(rd, t)), &idx);
        if (d < MARCH_EPS) {
            *out_idx = idx;
            return t;
        }
        t += d;
        if (t > MAX_DIST) break;
    }
    *out_idx = -1;
    return MAX_DIST;
}

/* March inside a specific die to find the exit surface. */
static float march_int(vec3_t ro, vec3_t rd, int idx)
{
    float t = MARCH_EPS * 3.0f;
    for (int i = 0; i < MAX_MARCH_STEPS; i++) {
        float d = sdf_die(v3_add(ro, v3_mul1(rd, t)), idx);
        if (d > MARCH_EPS) return t;
        t += fmaxf(-d, MARCH_EPS * 0.5f);
        if (t > MAX_DIST) break;
    }
    return MAX_DIST;
}

/* Soft shadow: march from a surface point toward the light.
 * Returns 0 (fully occluded) to 1 (fully lit). */
static float soft_shadow(vec3_t ro, vec3_t rd, float max_t)
{
    float t = 0.02f, res = 1.0f;
    for (int i = 0; i < 40; i++) {
        int   dummy;
        float d = sdf_scene(v3_add(ro, v3_mul1(rd, t)), &dummy);
        if (d < MARCH_EPS) return 0.0f;
        res = fminf(res, 10.0f * d / t);
        t += clampf(d, 0.005f, 0.25f);
        if (t >= max_t) break;
    }
    return saturate(res);
}

/* ================================================================== */
/*  Main trace loop                                                    */
/* ================================================================== */

/*
 * Returns premultiplied-alpha RGBA.
 *
 * Stochastic Fresnel splits each ray into a reflect or transmit path.
 * Over many frames temporal accumulation averages these, producing
 * natural partial alpha at glass surfaces:
 *
 *   - Reflected samples  (alpha = 1) show die reflections and specular.
 *   - Scattered samples  (alpha = 1) show the colored glass body.
 *   - Transmitted samples (alpha = 0) are transparent background.
 *
 * Volume scatter inside the die body models the pigment that makes
 * real dice opaque-ish.  Without it, physically-based Fresnel alone
 * gives only ~8 % surface reflectance -- nearly invisible.
 */
static vec4_t trace(vec3_t ro, vec3_t rd, uint *rng)
{
    vec3_t throughput  = vec3_o(1.0f);
    vec3_t radiance    = vec3_o(0.0f);
    int    inside      = -1;
    bool   any_reflect = false;  /* did any bounce reflect off a die? */

    for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
        float d_die;
        int   die_idx;

        /* ---- Find nearest surface ---- */

        if (inside >= 0) {
            d_die   = march_int(ro, rd, inside);
            die_idx = inside;
        } else {
            d_die = march_ext(ro, rd, &die_idx);
        }

        /* ---- Miss ---- */

        if (d_die >= MAX_DIST) {
            if (any_reflect) {
                /* Reflected off a die surface -- sky is the die's
                 * reflected environment.  Opaque contribution. */
                radiance = v3_add(radiance,
                                  v3_mul(throughput, sky(rd)));
                return vec4(radiance.x, radiance.y, radiance.z, 1.0f);
            }
            /* Never reflected -- transparent background. */
            return vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        /* ---- Die surface hit ---- */

        vec3_t pt   = v3_add(ro, v3_mul1(rd, d_die));
        vec3_t outN = sdf_normal(pt, die_idx);
        bool   front = v3_dot(rd, outN) < 0.0f;
        vec3_t N     = front ? outN : v3_mul1(outN, -1.0f);

        /* ---- Volume interaction on exit ---- */

        if (!front && inside >= 0) {
            const DieSpec *sp = &g_dice[inside];

            /*
             * Volume scatter: pigmented glass scatters some light
             * before it reaches the far surface.  The probability
             * increases with traversal distance and absorption
             * density -- thicker / denser = more opaque body.
             */
            float scatter_p = 1.0f - expf(-sp->sigma * d_die * 0.5f);

            if (shader_rand_1(rng) < scatter_p) {
                /* Scattered inside -- terminate with body color.
                 * Apply partial Beer-Lambert (average half-distance)
                 * then illuminate with the die's tint color. */
                float hd = d_die * 0.5f;
                vec3_t atten = vec3(
                    expf(-sp->sigma * (1.0f - sp->tint.x) * hd),
                    expf(-sp->sigma * (1.0f - sp->tint.y) * hd),
                    expf(-sp->sigma * (1.0f - sp->tint.z) * hd));
                throughput = v3_mul(throughput, atten);

                /* Rough directional lighting using exit normal. */
                float nl = saturate(v3_dot(outN, LIGHT_DIR));
                vec3_t body = v3_mul1(sp->tint, 0.18f + 0.42f * nl);
                radiance = v3_add(radiance,
                                  v3_mul(throughput, body));

                return vec4(radiance.x, radiance.y, radiance.z, 1.0f);
            }

            /* No scatter -- full Beer-Lambert, continue exiting. */
            vec3_t atten = vec3(
                expf(-sp->sigma * (1.0f - sp->tint.x) * d_die),
                expf(-sp->sigma * (1.0f - sp->tint.y) * d_die),
                expf(-sp->sigma * (1.0f - sp->tint.z) * d_die));
            throughput = v3_mul(throughput, atten);
            inside = -1;
        }

        /* ---- Pip check (front-face entry only) ---- */

        if (front) {
            const DieSpec *sp = &g_dice[die_idx];
            vec3_t local = rodrigues(v3_sub(pt, sp->pos),
                                     sp->rot_axis, -sp->rot_angle);
            if (is_pip(local)) {
                /* Opaque white diffuse pip. */
                float nl = saturate(v3_dot(N, LIGHT_DIR));
                float sh = soft_shadow(
                    v3_add(pt, v3_mul1(N, 0.005f)), LIGHT_DIR, 10.0f);
                vec3_t pc = v3_add(
                    v3_mul1(vec3(0.93f, 0.93f, 0.96f), nl * sh),
                    vec3(0.12f, 0.12f, 0.14f));
                radiance = v3_add(radiance, v3_mul(throughput, pc));
                return vec4(radiance.x, radiance.y, radiance.z, 1.0f);
            }
        }

        /* ---- Fresnel decision: reflect or refract ---- */

        float eta_i = front ? 1.0f   : DIE_IOR;
        float eta_t = front ? DIE_IOR : 1.0f;
        float cos_i = saturate(v3_dot(v3_mul1(rd, -1.0f), N));
        float refl  = shader_schlick(cos_i, eta_i, eta_t);
        vec3_t refr = v3_refract(rd, N, eta_i / eta_t);
        bool   can  = v3_length_sq(refr) > 0.0f;

        if (!can || shader_rand_1(rng) < refl) {
            /* Reflect. */
            rd = v3_reflect(rd, N);
            ro = v3_add(pt, v3_mul1(N, MARCH_EPS * 4.0f));
            any_reflect = true;
        } else {
            /* Refract. */
            rd = refr;
            ro = v3_sub(pt, v3_mul1(N, MARCH_EPS * 4.0f));
            if (front) inside = die_idx;
        }
    }

    /* Ran out of bounces. */
    return any_reflect
        ? vec4(radiance.x, radiance.y, radiance.z, 1.0f)
        : vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

/* ================================================================== */
/*  Entry point                                                        */
/* ================================================================== */

/*
 * Thin-lens depth of field.
 *
 * The pinhole ray is aimed at the focal plane.  The eye is then
 * jittered on a disk of radius APERTURE in the image plane.  The
 * new ray passes from the jittered eye through the same focal point,
 * so objects at FOCUS_DIST are sharp and everything else blurs with
 * distance from that plane.  Each frame samples a different aperture
 * point; temporal accumulation integrates the disk.
 */

#define APERTURE    0.10f    /* lens radius -- larger = stronger blur   */
#define FOCUS_DIST  5.2f     /* distance to the sharp focal plane       */

vec4_t dice_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    vec2_t res   = uniforms->resolution;
    uint   frame = uniforms->frame;

    /* Per-pixel random state, seeded by position and frame. */
    uint rng = (uint)fragCoord.x
             + (uint)(fragCoord.y * res.x)
             + frame * 91873u;

    /* Sub-pixel jitter for anti-aliasing. */
    float jx = shader_rand_1(&rng) - 0.5f;
    float jy = shader_rand_1(&rng) - 0.5f;
    vec2_t px = vec2(fragCoord.x + 0.5f + jx,
                     fragCoord.y + 0.5f + jy);

    /* Camera basis. */
    vec3_t eye    = vec3(0.15f, 2.6f, -3.8f);
    vec3_t target = vec3(0.05f, 0.25f, 0.50f);
    vec3_t fwd    = v3_normalize(v3_sub(target, eye));
    vec3_t right  = v3_normalize(v3_cross(vec3(0.0f, 1.0f, 0.0f), fwd));
    vec3_t up     = v3_cross(fwd, right);

    float aspect = res.x / res.y;
    float fov    = 1.0f;
    vec2_t uv    = vec2((px.x / res.x - 0.5f) * aspect * fov,
                        (px.y / res.y - 0.5f) * fov);

    /* Pinhole ray direction. */
    vec3_t rd_pin = v3_normalize(
        v3_add(fwd, v3_add(v3_mul1(right, uv.x), v3_mul1(up, uv.y))));

    /* Focal point: where the pinhole ray crosses the focal plane. */
    float t_focus = FOCUS_DIST / v3_dot(rd_pin, fwd);
    vec3_t focal_pt = v3_add(eye, v3_mul1(rd_pin, t_focus));

    /* Jitter eye position on the aperture disk. */
    float a_angle = 2.0f * PI * shader_rand_1(&rng);
    float a_r     = APERTURE * sqrtf(shader_rand_1(&rng));
    vec3_t eye_dof = v3_add(eye,
        v3_add(v3_mul1(right, a_r * cosf(a_angle)),
               v3_mul1(up,    a_r * sinf(a_angle))));

    /* Ray from jittered eye through the focal point. */
    vec3_t rd = v3_normalize(v3_sub(focal_pt, eye_dof));

    return trace(eye_dof, rd, &rng);
}

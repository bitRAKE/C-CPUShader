//Vector math library
//define VMATH_IMPL before including this header in *one* C file to create the implementation.

#include <math.h>

#define PI 3.1415926f

typedef struct {
    float x;
    float y;
} vec2_t;

typedef struct {
    float x;
    float y;
    float z;
} vec3_t;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} vec4_t;

#define vec2_o(a) ((vec2_t){a, a})
#define vec2(a, b) ((vec2_t){a, b})

#define vec3_o(a) ((vec3_t){a, a, a})
#define vec3(a, b, c) ((vec3_t){a, b, c})

#define vec4_o(a) ((vec4_t){a, a, a, a})
#define vec4(a, b, c, d) ((vec4_t){a, b, c, d})

float clampf(float x, float min_value, float max_value);
float saturate(float x);
float lerpf(float a, float b, float t);
float smoothstepf(float edge0, float edge1, float x);

float v2_dot(vec2_t a, vec2_t b);
float v2_length_sq(vec2_t v);
float v2_length(vec2_t v);
float v2_distance(vec2_t a, vec2_t b);
vec2_t v2_add(vec2_t a, vec2_t b);
vec2_t v2_sub(vec2_t a, vec2_t b);
vec2_t v2_mul(vec2_t a, vec2_t b);
vec2_t v2_mul1(vec2_t a, float b);
vec2_t v2_div1(vec2_t a, float b);
vec2_t v2_lerp(vec2_t a, vec2_t b, float t);
vec2_t v2_fract(vec2_t a);
vec2_t v2_floor(vec2_t a);
vec2_t v2_abs(vec2_t a);
vec2_t v2_normalize(vec2_t v);
vec2_t v2_perp(vec2_t v);
vec2_t v2_reflect(vec2_t v, vec2_t n);
vec2_t v2_refract(vec2_t v, vec2_t n, float eta);

float v3_dot(vec3_t a, vec3_t b);
float v3_length_sq(vec3_t v);
float v3_length(vec3_t v);
float v3_distance(vec3_t a, vec3_t b);
vec3_t v3_add(vec3_t a, vec3_t b);
vec3_t v3_sub(vec3_t a, vec3_t b);
vec3_t v3_mul(vec3_t a, vec3_t b);
vec3_t v3_mul1(vec3_t a, float b);
vec3_t v3_div1(vec3_t a, float b);
vec3_t v3_lerp(vec3_t a, vec3_t b, float t);
vec3_t v3_fract(vec3_t a);
vec3_t v3_floor(vec3_t a);
vec3_t v3_abs(vec3_t a);
vec3_t v3_normalize(vec3_t v);
vec3_t v3_cross(vec3_t a, vec3_t b);
vec3_t v3_reflect(vec3_t v, vec3_t n);
vec3_t v3_refract(vec3_t v, vec3_t n, float eta);

float v4_dot(vec4_t a, vec4_t b);
float v4_length_sq(vec4_t v);
float v4_length(vec4_t v);
vec4_t v4_add(vec4_t a, vec4_t b);
vec4_t v4_sub(vec4_t a, vec4_t b);
vec4_t v4_mul(vec4_t a, vec4_t b);
vec4_t v4_mul1(vec4_t a, float b);
vec4_t v4_div1(vec4_t a, float b);
vec4_t v4_lerp(vec4_t a, vec4_t b, float t);
vec4_t v4_fract(vec4_t a);
vec4_t v4_floor(vec4_t a);
vec4_t v4_abs(vec4_t a);
vec4_t v4_sqrt(vec4_t v);
vec4_t v4_normalize(vec4_t v);

#ifdef VMATH_IMPL

float clampf(float x, float min_value, float max_value)
{
    return fmaxf(min_value, fminf(max_value, x));
}

float saturate(float x)
{
    return clampf(x, 0.0f, 1.0f);
}

float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

float smoothstepf(float edge0, float edge1, float x)
{
    float width = edge1 - edge0;

    if (fabsf(width) < 0.000001f) {
        return (x >= edge1) ? 1.0f : 0.0f;
    }

    float t = saturate((x - edge0) / width);
    return t * t * (3.0f - 2.0f * t);
}

float v2_dot(vec2_t a, vec2_t b)
{
    return a.x * b.x + a.y * b.y;
}

float v2_length_sq(vec2_t v)
{
    return v2_dot(v, v);
}

float v2_length(vec2_t v)
{
    return sqrtf(v2_length_sq(v));
}

float v2_distance(vec2_t a, vec2_t b)
{
    return v2_length(v2_sub(a, b));
}

vec2_t v2_add(vec2_t a, vec2_t b)
{
    return vec2(a.x + b.x, a.y + b.y);
}

vec2_t v2_sub(vec2_t a, vec2_t b)
{
    return vec2(a.x - b.x, a.y - b.y);
}

vec2_t v2_mul(vec2_t a, vec2_t b)
{
    return vec2(a.x * b.x, a.y * b.y);
}

vec2_t v2_mul1(vec2_t a, float b)
{
    return vec2(a.x * b, a.y * b);
}

vec2_t v2_div1(vec2_t a, float b)
{
    return vec2(a.x / b, a.y / b);
}

vec2_t v2_lerp(vec2_t a, vec2_t b, float t)
{
    return vec2(lerpf(a.x, b.x, t), lerpf(a.y, b.y, t));
}

vec2_t v2_fract(vec2_t a)
{
    return vec2(a.x - floorf(a.x), a.y - floorf(a.y));
}

vec2_t v2_floor(vec2_t a)
{
    return vec2(floorf(a.x), floorf(a.y));
}

vec2_t v2_abs(vec2_t a)
{
    return vec2(fabsf(a.x), fabsf(a.y));
}

vec2_t v2_normalize(vec2_t v)
{
    float len = v2_length(v);
    if (len <= 0.0f) {
        return vec2_o(0.0f);
    }

    return v2_div1(v, len);
}

vec2_t v2_perp(vec2_t v)
{
    return vec2(-v.y, v.x);
}

vec2_t v2_reflect(vec2_t v, vec2_t n)
{
    float dot = v2_dot(v, n);
    return v2_sub(v, v2_mul1(n, 2.0f * dot));
}

vec2_t v2_refract(vec2_t v, vec2_t n, float eta)
{
    float dot = v2_dot(v, n);
    float k = 1.0f - eta * eta * (1.0f - dot * dot);

    if (k < 0.0f) {
        return vec2_o(0.0f);
    }

    return vec2(eta * v.x - (eta * dot + sqrtf(k)) * n.x,
                eta * v.y - (eta * dot + sqrtf(k)) * n.y);
}

float v3_dot(vec3_t a, vec3_t b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float v3_length_sq(vec3_t v)
{
    return v3_dot(v, v);
}

float v3_length(vec3_t v)
{
    return sqrtf(v3_length_sq(v));
}

float v3_distance(vec3_t a, vec3_t b)
{
    return v3_length(v3_sub(a, b));
}

vec3_t v3_add(vec3_t a, vec3_t b)
{
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

vec3_t v3_sub(vec3_t a, vec3_t b)
{
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

vec3_t v3_mul(vec3_t a, vec3_t b)
{
    return vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

vec3_t v3_mul1(vec3_t a, float b)
{
    return vec3(a.x * b, a.y * b, a.z * b);
}

vec3_t v3_div1(vec3_t a, float b)
{
    return vec3(a.x / b, a.y / b, a.z / b);
}

vec3_t v3_lerp(vec3_t a, vec3_t b, float t)
{
    return vec3(lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t));
}

vec3_t v3_fract(vec3_t a)
{
    return vec3(a.x - floorf(a.x), a.y - floorf(a.y), a.z - floorf(a.z));
}

vec3_t v3_floor(vec3_t a)
{
    return vec3(floorf(a.x), floorf(a.y), floorf(a.z));
}

vec3_t v3_abs(vec3_t a)
{
    return vec3(fabsf(a.x), fabsf(a.y), fabsf(a.z));
}

vec3_t v3_normalize(vec3_t v)
{
    float len = v3_length(v);
    if (len <= 0.0f) {
        return vec3_o(0.0f);
    }

    return v3_div1(v, len);
}

vec3_t v3_cross(vec3_t a, vec3_t b)
{
    return vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

vec3_t v3_reflect(vec3_t v, vec3_t n)
{
    float dot = v3_dot(v, n);
    return v3_sub(v, v3_mul1(n, 2.0f * dot));
}

vec3_t v3_refract(vec3_t v, vec3_t n, float eta)
{
    float dot = v3_dot(v, n);
    float k = 1.0f - eta * eta * (1.0f - dot * dot);

    if (k < 0.0f) {
        return vec3_o(0.0f);
    }

    return v3_sub(v3_mul1(v, eta), v3_mul1(n, eta * dot + sqrtf(k)));
}

float v4_dot(vec4_t a, vec4_t b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float v4_length_sq(vec4_t v)
{
    return v4_dot(v, v);
}

float v4_length(vec4_t v)
{
    return sqrtf(v4_length_sq(v));
}

vec4_t v4_add(vec4_t a, vec4_t b)
{
    return vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

vec4_t v4_sub(vec4_t a, vec4_t b)
{
    return vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

vec4_t v4_mul(vec4_t a, vec4_t b)
{
    return vec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

vec4_t v4_mul1(vec4_t a, float b)
{
    return vec4(a.x * b, a.y * b, a.z * b, a.w * b);
}

vec4_t v4_div1(vec4_t a, float b)
{
    return vec4(a.x / b, a.y / b, a.z / b, a.w / b);
}

vec4_t v4_lerp(vec4_t a, vec4_t b, float t)
{
    return vec4(lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t), lerpf(a.w, b.w, t));
}

vec4_t v4_fract(vec4_t a)
{
    return vec4(a.x - floorf(a.x), a.y - floorf(a.y), a.z - floorf(a.z), a.w - floorf(a.w));
}

vec4_t v4_floor(vec4_t a)
{
    return vec4(floorf(a.x), floorf(a.y), floorf(a.z), floorf(a.w));
}

vec4_t v4_abs(vec4_t a)
{
    return vec4(fabsf(a.x), fabsf(a.y), fabsf(a.z), fabsf(a.w));
}

vec4_t v4_sqrt(vec4_t v)
{
    return vec4(sqrtf(v.x), sqrtf(v.y), sqrtf(v.z), sqrtf(v.w));
}

vec4_t v4_normalize(vec4_t v)
{
    float len = v4_length(v);
    if (len <= 0.0f) {
        return vec4_o(0.0f);
    }

    return v4_div1(v, len);
}

#endif

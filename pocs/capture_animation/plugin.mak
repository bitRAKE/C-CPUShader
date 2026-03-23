CAPTURE_ANIM_PLUGIN = pocs\capture_animation\animation_capture.dll
CAPTURE_ANIM_OBJS = \
    $(PLUGIN_OBJ_DIR)\capture_animation\collection.obj \
    $(PLUGIN_OBJ_DIR)\capture_animation\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\capture_animation\animated_sprite.obj \
    $(PLUGIN_OBJ_DIR)\capture_animation\crystal_dodecahedron.obj \
    $(PLUGIN_OBJ_DIR)\capture_animation\orbit_stars.obj

{pocs\capture_animation}.c{$(PLUGIN_OBJ_DIR)\capture_animation}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(CAPTURE_ANIM_PLUGIN): $(CAPTURE_ANIM_OBJS)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(CAPTURE_ANIM_OBJS)

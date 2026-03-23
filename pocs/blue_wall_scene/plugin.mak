BLUE_WALL_PLUGIN = pocs\blue_wall_scene\blue_wall_scene.dll
BLUE_WALL_RES = $(PLUGIN_OBJ_DIR)\blue_wall_scene\textures.res
BLUE_WALL_OBJS = \
    $(PLUGIN_OBJ_DIR)\blue_wall_scene\collection.obj \
    $(PLUGIN_OBJ_DIR)\blue_wall_scene\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\blue_wall_scene\blue_wall_v2_A.obj \
    $(PLUGIN_OBJ_DIR)\blue_wall_scene\blue_wall_v2_B.obj \
    $(PLUGIN_OBJ_DIR)\blue_wall_scene\blue_wall_v2_C.obj \
    $(PLUGIN_OBJ_DIR)\blue_wall_scene\blue_wall_v2_D.obj \
    $(PLUGIN_OBJ_DIR)\blue_wall_scene\blue_wall_v2_E.obj

{pocs\blue_wall_scene}.c{$(PLUGIN_OBJ_DIR)\blue_wall_scene}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(BLUE_WALL_RES): pocs\blue_wall_scene\textures.rc pocs\blue_wall_scene\blue_wall\textures\painting.jpg
    @if not exist "$(@D)" mkdir "$(@D)"
    $(RC) /fo $@ pocs\blue_wall_scene\textures.rc

$(BLUE_WALL_PLUGIN): $(BLUE_WALL_OBJS) $(BLUE_WALL_RES)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(BLUE_WALL_OBJS) $(BLUE_WALL_RES)

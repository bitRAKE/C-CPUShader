SDF_FIXED_PLUGIN = pocs\sdf_fixed\sdf_fixed.dll
SDF_FIXED_RES = $(PLUGIN_OBJ_DIR)\sdf_fixed\textures.res
SDF_FIXED_OBJS = \
    $(PLUGIN_OBJ_DIR)\sdf_fixed\collection.obj \
    $(PLUGIN_OBJ_DIR)\sdf_fixed\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\sdf_fixed\sdf_fixed_hello_world.obj

{pocs\sdf_fixed}.c{$(PLUGIN_OBJ_DIR)\sdf_fixed}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(SDF_FIXED_RES): pocs\sdf_fixed\textures.rc pocs\sdf_fixed\ascii_sdf_grid.png
    @if not exist "$(@D)" mkdir "$(@D)"
    $(RC) /fo $@ pocs\sdf_fixed\textures.rc

$(SDF_FIXED_PLUGIN): $(SDF_FIXED_OBJS) $(SDF_FIXED_RES)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(SDF_FIXED_OBJS) $(SDF_FIXED_RES)

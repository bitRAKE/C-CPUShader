MTSDF_PLUGIN = pocs\mtsdf\mtsdf_text.dll
MTSDF_RES = $(PLUGIN_OBJ_DIR)\mtsdf\textures.res
MTSDF_OBJS = \
    $(PLUGIN_OBJ_DIR)\mtsdf\collection.obj \
    $(PLUGIN_OBJ_DIR)\mtsdf\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\mtsdf\mtsdf_hello_world.obj

{pocs\mtsdf}.c{$(PLUGIN_OBJ_DIR)\mtsdf}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(MTSDF_RES): pocs\mtsdf\textures.rc pocs\mtsdf\ascii_mtsdf.png
    @if not exist "$(@D)" mkdir "$(@D)"
    $(RC) /fo $@ pocs\mtsdf\textures.rc

$(MTSDF_PLUGIN): $(MTSDF_OBJS) $(MTSDF_RES)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(MTSDF_OBJS) $(MTSDF_RES)

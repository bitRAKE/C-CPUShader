HSV_PICKER_PLUGIN = pocs\hsv_picker\hsv_picker.dll
HSV_PICKER_OBJS = \
    $(PLUGIN_OBJ_DIR)\hsv_picker\collection.obj \
    $(PLUGIN_OBJ_DIR)\hsv_picker\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\hsv_picker\hsv_picker.obj

{pocs\hsv_picker}.c{$(PLUGIN_OBJ_DIR)\hsv_picker}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(HSV_PICKER_PLUGIN): $(HSV_PICKER_OBJS)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(HSV_PICKER_OBJS)

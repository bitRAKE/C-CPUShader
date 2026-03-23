MONITOR_DIAG_PLUGIN = plugins\monitor_diagnostic\monitor_diagnostic.dll
MONITOR_DIAG_OBJS = \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\collection.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\diag_gradient.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\diag_hsv_wheel.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\diag_banding.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\diag_gamma_ramp.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\diag_motion.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\diag_strobe.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\diag_subpixel.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\diag_hdr_clipping.obj \
    $(PLUGIN_OBJ_DIR)\monitor_diagnostic\diag_hdr_gamut.obj

{plugins\monitor_diagnostic}.c{$(PLUGIN_OBJ_DIR)\monitor_diagnostic}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) -Iplugins\monitor_diagnostic $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(MONITOR_DIAG_PLUGIN): $(MONITOR_DIAG_OBJS)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(MONITOR_DIAG_OBJS)

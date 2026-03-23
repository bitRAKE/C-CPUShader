COLORSPACE_PLUGIN = plugins\colorspace_probes\colorspace_probes.dll
COLORSPACE_OBJS = \
    $(PLUGIN_OBJ_DIR)\colorspace_probes\collection.obj \
    $(PLUGIN_OBJ_DIR)\colorspace_probes\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\colorspace_probes\colorspace_sdr_ui.obj \
    $(PLUGIN_OBJ_DIR)\colorspace_probes\colorspace_hdr_linear.obj \
    $(PLUGIN_OBJ_DIR)\colorspace_probes\colorspace_hdr10_pq.obj

{plugins\colorspace_probes}.c{$(PLUGIN_OBJ_DIR)\colorspace_probes}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) -Iplugins\colorspace_probes $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(COLORSPACE_PLUGIN): $(COLORSPACE_OBJS)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(COLORSPACE_OBJS)

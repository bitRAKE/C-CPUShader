SHADER_VARS_PLUGIN = pocs\shader_variables\shader_variables.dll
SHADER_VARS_OBJS = \
    $(PLUGIN_OBJ_DIR)\shader_variables\collection.obj \
    $(PLUGIN_OBJ_DIR)\shader_variables\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\shader_variables\toggle_switch.obj \
    $(PLUGIN_OBJ_DIR)\shader_variables\radial_gauge.obj \
    $(PLUGIN_OBJ_DIR)\shader_variables\status_indicator.obj \
    $(PLUGIN_OBJ_DIR)\shader_variables\button_round_metal.obj \
    $(PLUGIN_OBJ_DIR)\shader_variables\button_template.obj \
    $(PLUGIN_OBJ_DIR)\shader_variables\variable_probe.obj

{pocs\shader_variables}.c{$(PLUGIN_OBJ_DIR)\shader_variables}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(SHADER_VARS_PLUGIN): $(SHADER_VARS_OBJS)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(SHADER_VARS_OBJS)

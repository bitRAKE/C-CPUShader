CORE_STUDIES_PLUGIN = plugins\core_studies\core_studies.dll
CORE_STUDIES_OBJS = \
    $(PLUGIN_OBJ_DIR)\core_studies\collection.obj \
    $(PLUGIN_OBJ_DIR)\core_studies\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\core_studies\three_fire.obj \
    $(PLUGIN_OBJ_DIR)\core_studies\sphere_tracing.obj \
    $(PLUGIN_OBJ_DIR)\core_studies\kinetic_orbs.obj \
    $(PLUGIN_OBJ_DIR)\core_studies\glass_lenses.obj \
    $(PLUGIN_OBJ_DIR)\core_studies\glass_disks.obj \
    $(PLUGIN_OBJ_DIR)\core_studies\crystal_hall.obj \
    $(PLUGIN_OBJ_DIR)\core_studies\dice.obj

{plugins\core_studies}.c{$(PLUGIN_OBJ_DIR)\core_studies}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) -Iplugins\core_studies $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(CORE_STUDIES_PLUGIN): $(CORE_STUDIES_OBJS)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(CORE_STUDIES_OBJS)

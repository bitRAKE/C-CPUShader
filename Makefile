!IFNDEF VULKAN_SDK
!ERROR VULKAN_SDK is not set. Run vcvars64.bat amd64 and ensure the Vulkan SDK is available.
!ENDIF

# Build profile: local (default) vs release
# Use: nmake RELEASE=1 for release builds
!IFDEF RELEASE
MARCH=-mavx2
!ELSE
MARCH=-march=native
!ENDIF

CC=clang
RC=llvm-rc

ROOT_BUILD_DIR=build
BUILD_DIR=$(ROOT_BUILD_DIR)
OBJ_DIR=$(BUILD_DIR)\obj
RES_DIR=$(BUILD_DIR)\res
OUT=$(BUILD_DIR)\bin.exe
APP_RES=$(RES_DIR)\app.res
DEP_MAKEFILE=$(BUILD_DIR)\deps.mk

COMMON_DEFINES=-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0A00
COMMON_INCLUDES=-I"$(VULKAN_SDK)\Include" -Isrc -Isdk
COMMON_WARNINGS=-Wall -Wextra -Werror -pedantic
COMMON_FLAGS=-std=c17 $(COMMON_WARNINGS) -target x86_64-pc-windows-msvc $(COMMON_DEFINES) $(COMMON_INCLUDES) $(MARCH) -ffunction-sections -fdata-sections -fno-stack-protector -ffreestanding -fno-builtin
COMMON_LIBS=-L"$(VULKAN_SDK)\Lib" -luser32 -lgdi32 -lvulkan-1 -lcomctl32 -lshell32 -lole32 -lwindowscodecs -ld3d12 -ldxgi -ldxguid -ld3dcompiler
LINKFLAGS_BASE=-target x86_64-pc-windows-msvc $(COMMON_LIBS) -fuse-ld=lld -Xlinker "/subsystem:windows,10.0"
CFLAGS=$(COMMON_FLAGS) -O3 -DNDEBUG -flto
DEPFLAGS=-MMD -MP
LINKFLAGS=$(LINKFLAGS_BASE) -flto

# Plugin builds use SDK headers only -- no -Isrc to enforce SDK boundary
PLUGIN_CFLAGS=-std=c17 $(COMMON_WARNINGS) -target x86_64-pc-windows-msvc $(COMMON_DEFINES) -DSHADER_PLUGIN_BUILD $(MARCH) -ffunction-sections -fdata-sections -fno-stack-protector -ffreestanding -fno-builtin -O3 -DNDEBUG -flto -Isdk
PLUGIN_LINKFLAGS=-target x86_64-pc-windows-msvc -fuse-ld=lld -flto -lkernel32
PLUGIN_OBJ_DIR=$(OBJ_DIR)\plugins

OBJS= \
    $(OBJ_DIR)\src\main.obj \
    $(OBJ_DIR)\src\display.obj \
    $(OBJ_DIR)\src\stats.obj \
    $(OBJ_DIR)\src\win.obj \
    $(OBJ_DIR)\src\runtime_session.obj \
    $(OBJ_DIR)\src\frame_timing.obj \
    $(OBJ_DIR)\src\capture_manager.obj \
    $(OBJ_DIR)\src\render_workers.obj \
    $(OBJ_DIR)\src\defines.obj \
    $(OBJ_DIR)\src\capture_wic.obj \
    $(OBJ_DIR)\src\capture_exr.obj \
    $(OBJ_DIR)\src\shader_buffers.obj \
    $(OBJ_DIR)\src\shader_variables.obj \
    $(OBJ_DIR)\src\shader_catalog_runtime.obj \
    $(OBJ_DIR)\src\plugin_loader.obj \
    $(OBJ_DIR)\src\host\host_options.obj \
    $(OBJ_DIR)\src\present\present_backend.obj \
    $(OBJ_DIR)\src\dxgi\dxgi_support.obj \
    $(OBJ_DIR)\src\dx12\backend_dx12.obj \
    $(OBJ_DIR)\src\dx12\dx12_present_path_upload_buffer_srv.obj \
    $(OBJ_DIR)\src\gdi\backend_gdi.obj \
    $(OBJ_DIR)\src\ogl\backend_ogl.obj \
    $(OBJ_DIR)\src\vk\backend_vk.obj \
    $(OBJ_DIR)\src\vk\backend_vk_support.obj \
    $(OBJ_DIR)\src\shaders\master_class.obj \
    $(OBJ_DIR)\src\shaders\master_class_scrgb.obj \
    $(OBJ_DIR)\src\shaders\master_class_hdr10.obj

all: $(OUT) plugins

DEPS=$(OBJS:.obj=.d)

!IF EXIST("$(DEP_MAKEFILE)")
!INCLUDE "$(DEP_MAKEFILE)"
!ENDIF

$(OUT): $(APP_RES) $(OBJS)
    @if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
    $(CC) $(LINKFLAGS) -o $@ $(OBJS) $(APP_RES)
    @powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "if (Test-Path '$(OBJ_DIR)') { @(Get-ChildItem -Path '$(OBJ_DIR)' -Filter *.d -Recurse | Sort-Object FullName) | Get-Content | Set-Content -Path '$(DEP_MAKEFILE)' -Encoding Ascii } elseif (Test-Path '$(DEP_MAKEFILE)') { Remove-Item '$(DEP_MAKEFILE)' -Force }"

$(APP_RES): src\app.rc src\app.manifest src\resource.h
    @if not exist "$(RES_DIR)" mkdir "$(RES_DIR)"
    $(RC) /fo $@ src\app.rc

# ---- Host inference rules (one per source subdirectory) ----
{src}.c{$(OBJ_DIR)\src}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

{src\host}.c{$(OBJ_DIR)\src\host}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

{src\present}.c{$(OBJ_DIR)\src\present}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

{src\dxgi}.c{$(OBJ_DIR)\src\dxgi}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

{src\dx12}.c{$(OBJ_DIR)\src\dx12}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

{src\gdi}.c{$(OBJ_DIR)\src\gdi}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

{src\ogl}.c{$(OBJ_DIR)\src\ogl}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

{src\vk}.c{$(OBJ_DIR)\src\vk}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

{src\shaders}.c{$(OBJ_DIR)\src\shaders}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

# ---- Plugins (shipped) ----
!INCLUDE plugins\core_studies\plugin.mak
!INCLUDE plugins\colorspace_probes\plugin.mak
!INCLUDE plugins\monitor_diagnostic\plugin.mak

# ---- Plugins (pocs) ----
!INCLUDE pocs\shader_variables\plugin.mak
!INCLUDE pocs\capture_animation\plugin.mak
!INCLUDE pocs\hsv_picker\plugin.mak
!INCLUDE pocs\sdf_fixed\plugin.mak
!INCLUDE pocs\mtsdf\plugin.mak
!INCLUDE pocs\blue_wall_scene\plugin.mak

plugins: \
    $(CORE_STUDIES_PLUGIN) \
    $(COLORSPACE_PLUGIN) \
    $(MONITOR_DIAG_PLUGIN) \
    $(SHADER_VARS_PLUGIN) \
    $(CAPTURE_ANIM_PLUGIN) \
    $(HSV_PICKER_PLUGIN) \
    $(SDF_FIXED_PLUGIN) \
    $(MTSDF_PLUGIN) \
    $(BLUE_WALL_PLUGIN)

run: $(OUT)
    $(OUT)

clean:
    @if exist "$(ROOT_BUILD_DIR)" rmdir /s /q "$(ROOT_BUILD_DIR)"
    @if exist "$(SHADER_VARS_PLUGIN)" del "$(SHADER_VARS_PLUGIN)"
    @if exist "$(CAPTURE_ANIM_PLUGIN)" del "$(CAPTURE_ANIM_PLUGIN)"
    @if exist "$(HSV_PICKER_PLUGIN)" del "$(HSV_PICKER_PLUGIN)"
    @if exist "$(CORE_STUDIES_PLUGIN)" del "$(CORE_STUDIES_PLUGIN)"
    @if exist "$(COLORSPACE_PLUGIN)" del "$(COLORSPACE_PLUGIN)"
    @if exist "$(SDF_FIXED_PLUGIN)" del "$(SDF_FIXED_PLUGIN)"
    @if exist "$(MTSDF_PLUGIN)" del "$(MTSDF_PLUGIN)"
    @if exist "$(BLUE_WALL_PLUGIN)" del "$(BLUE_WALL_PLUGIN)"
    @if exist "$(MONITOR_DIAG_PLUGIN)" del "$(MONITOR_DIAG_PLUGIN)"

rebuild: clean all

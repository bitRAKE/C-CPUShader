@echo off
setlocal enabledelayedexpansion
REM execute from this directory regardless of CWD invocation
pushd "%~dp0"
set BUILD_EXIT=0
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -property installationPath 2^>nul`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo Failed to locate Visual Studio via vswhere.
    set BUILD_EXIT=1
    goto cleanup
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" amd64 >nul || (
    echo Failed to initialize the amd64 Visual Studio build environment.
    set BUILD_EXIT=1
    goto cleanup
)

nmake /nologo /f Makefile %* || (
    set BUILD_EXIT=1
    goto cleanup
)

if exist build\bin.exe (
    copy /y build\bin.exe bin.exe >nul || (
        echo Failed to copy build\bin.exe to bin.exe.
        set BUILD_EXIT=1
        goto cleanup
    )
)

:cleanup
popd
exit /b %BUILD_EXIT%

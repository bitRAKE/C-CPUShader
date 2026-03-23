@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM  _release.cmd <tag>
REM
REM  General-purpose release utility.  Every step is automatic;
REM  new plugins under plugins\ are picked up without edits.
REM
REM  1. Clean build (host + all plugin targets)
REM  2. Auto-discover every DLL under plugins\
REM  3. Git add tracked sources, commit, annotated tag
REM  4. Stage release layout in build\release\
REM
REM  Output:
REM    build\release\CPU_Shader\CPU_Shader.exe
REM    build\release\CPU_Shader\CPU_Shader.md
REM    build\release\CPU_Shader\plugins\*.dll   (auto-discovered)
REM    build\release\sdk\*.h  sdk\SDK.md
REM ============================================================
pushd "%~dp0"

set RELEASE_EXIT=0
set RELEASE_DIR=build\release

REM --- Parse tag argument -------------------------------------
set "TAG=%~1"
if "%TAG%"=="" (
    echo Usage: _release.cmd ^<tag^>
    echo   e.g. _release.cmd v0.5.0
    set RELEASE_EXIT=1
    goto done
)
echo %TAG% | findstr /r "^v[0-9]" >nul || (
    echo ERROR: Tag must start with v followed by a digit, e.g. v0.5.0
    set RELEASE_EXIT=1
    goto done
)

REM --- Step 1: Clean build ------------------------------------
echo.
echo [1/6] Building host + plugins...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -property installationPath 2^>nul`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo ERROR: Failed to locate Visual Studio via vswhere.
    set RELEASE_EXIT=1
    goto done
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" amd64 >nul || (
    echo ERROR: Failed to initialize Visual Studio build environment.
    set RELEASE_EXIT=1
    goto done
)

nmake /nologo /f Makefile clean >nul 2>&1
nmake /nologo /f Makefile RELEASE=1 2>&1
if errorlevel 1 (
    echo ERROR: Build failed.
    set RELEASE_EXIT=1
    goto done
)

if not exist build\bin.exe (
    echo ERROR: build\bin.exe not found after successful build.
    set RELEASE_EXIT=1
    goto done
)
echo    Host build OK.

REM --- Step 2: Discover shipped plugin DLLs -------------------
echo.
echo [2/6] Discovering plugin DLLs under plugins\...
set PLUGIN_COUNT=0
set PLUGIN_LIST=
for /r plugins %%F in (*.dll) do (
    set /a PLUGIN_COUNT+=1
    set "PLUGIN_LIST=!PLUGIN_LIST! %%F"
    echo    Found: %%~nxF
)

if !PLUGIN_COUNT!==0 (
    echo WARNING: No plugin DLLs found under plugins\.
)
echo    !PLUGIN_COUNT! shipped plugin(s) discovered.

REM --- Step 3: Git add, commit, tag ---------------------------
echo.
echo [3/6] Git commit and tag %TAG%...

REM  Stage everything git already tracks (respects .gitignore)
git add -u
if errorlevel 1 (
    echo ERROR: git add -u failed.
    set RELEASE_EXIT=1
    goto done
)

REM  Stage new files in key directories (catches new plugins, SDK files)
git add src\       2>nul
git add sdk\       2>nul
git add plugins\   2>nul
git add pocs\      2>nul
git add docs\      2>nul
git add Makefile   2>nul
git add _build.cmd _release.cmd 2>nul
git add CPU_Shader.md 2>nul
git add .gitignore 2>nul

REM  Commit (non-fatal if tree is clean)
git diff --cached --quiet
if errorlevel 1 (
    git commit -m "release %TAG%"
    if errorlevel 1 (
        echo ERROR: git commit failed.
        set RELEASE_EXIT=1
        goto done
    )
) else (
    echo    Working tree clean, no commit needed.
)

REM  Tag (fatal if duplicate)
git tag -a %TAG% -m "Release %TAG%" 2>&1
if errorlevel 1 (
    echo ERROR: git tag failed. Tag %TAG% may already exist.
    set RELEASE_EXIT=1
    goto done
)
echo    Tagged as %TAG%.

REM --- Step 4: Stage CPU_Shader.zip contents ------------------
echo.
echo [4/6] Staging CPU_Shader.zip...

if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%\CPU_Shader\plugins"

copy /y build\bin.exe "%RELEASE_DIR%\CPU_Shader\CPU_Shader.exe" >nul
echo    CPU_Shader.exe

if exist CPU_Shader.md (
    copy /y CPU_Shader.md "%RELEASE_DIR%\CPU_Shader\CPU_Shader.md" >nul
    echo    CPU_Shader.md
)

REM  Copy every DLL discovered under plugins\ into the flat release dir
for /r plugins %%F in (*.dll) do (
    copy /y "%%F" "%RELEASE_DIR%\CPU_Shader\plugins\%%~nxF" >nul
    echo    plugins\%%~nxF
)

REM --- Step 5: Stage sdk.zip contents -------------------------
echo.
echo [5/6] Staging sdk.zip...

mkdir "%RELEASE_DIR%\sdk"
for %%F in (sdk\*.h) do (
    copy /y "%%F" "%RELEASE_DIR%\sdk\" >nul
    echo    %%~nxF
)
if exist sdk\SDK.md (
    copy /y sdk\SDK.md "%RELEASE_DIR%\sdk\" >nul
    echo    SDK.md
)

REM --- Step 6: Create zip archives --------------------------------
echo.
echo [6/6] Creating zip archives...

powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -Command ^
    "Compress-Archive -Path '%RELEASE_DIR%\CPU_Shader\*' -DestinationPath '%RELEASE_DIR%\CPU_Shader.zip' -Force"
if errorlevel 1 (
    echo WARNING: Failed to create CPU_Shader.zip.
) else (
    echo    CPU_Shader.zip
)

powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -Command ^
    "Compress-Archive -Path '%RELEASE_DIR%\sdk\*' -DestinationPath '%RELEASE_DIR%\sdk.zip' -Force"
if errorlevel 1 (
    echo WARNING: Failed to create sdk.zip.
) else (
    echo    sdk.zip
)

REM --- Verify: run --list-shaders from staged exe -------------
echo.
echo Verify:
"%RELEASE_DIR%\CPU_Shader\CPU_Shader.exe" --list-shaders 2>&1

REM --- Summary ------------------------------------------------
echo.
echo ============================================================
echo  Release %TAG% staged in %RELEASE_DIR%\
echo ============================================================
echo.
echo  Archives:
echo    %RELEASE_DIR%\CPU_Shader.zip
echo    %RELEASE_DIR%\sdk.zip
echo.
echo  Git tag: %TAG%
echo  To push: git push origin %TAG%
echo ============================================================

:done
popd
exit /b %RELEASE_EXIT%

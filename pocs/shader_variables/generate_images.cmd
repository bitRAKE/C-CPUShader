@echo off
setlocal
REM ---------------------------------------------------------------------------
REM generate_images.cmd -- Capture representative states for the shader
REM variables POC.  Run from the repository root after building.
REM
REM Produces PNG images in pocs\shader_variables\images\ linked by readme.md.
REM ---------------------------------------------------------------------------

set "ROOT=%~dp0..\.."
set "BIN=%ROOT%\build\bin.exe"
set "CAPTURES=%ROOT%\build\captures"
set "PLUGIN_DIR=%~dp0."
set "IMG_DIR=%~dp0images"

if not exist "%BIN%" (
    echo ERROR: build\bin.exe not found. Build the project first.
    exit /b 1
)

if not exist "%IMG_DIR%" mkdir "%IMG_DIR%"

echo.
echo === Generating shader variable showcase images ===
echo.

REM ---------------------------------------------------------------------------
REM Helper: capture one frame and copy the newest output to a named PNG.
REM   %1 = shader id
REM   %2 = output filename (no path, no extension)
REM   %3..%N = extra --var flags
REM ---------------------------------------------------------------------------

REM --- Toggle Switch ---

echo [1/12] toggle_switch off
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=toggle_switch --var state=0.0
call :copy_latest toggle_switch toggle_off

echo [2/12] toggle_switch mid
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=toggle_switch --var state=0.5
call :copy_latest toggle_switch toggle_mid

echo [3/12] toggle_switch on
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=toggle_switch --var state=1.0
call :copy_latest toggle_switch toggle_on

echo [4/12] toggle_switch purple
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=toggle_switch --var state=1.0 --var active_color=0.55,0.28,0.82
call :copy_latest toggle_switch toggle_purple

REM --- Radial Gauge ---

echo [5/12] radial_gauge low
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=radial_gauge --var value=0.15
call :copy_latest radial_gauge gauge_low

echo [6/12] radial_gauge default
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=radial_gauge
call :copy_latest radial_gauge gauge_default

echo [7/12] radial_gauge critical
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=radial_gauge --var value=0.95 --var arc_color=0.88,0.22,0.18
call :copy_latest radial_gauge gauge_critical

echo [8/12] radial_gauge gold
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=radial_gauge --var value=0.60 --var arc_color=0.92,0.72,0.15 --var thickness=0.18
call :copy_latest radial_gauge gauge_gold

REM --- Status Indicator ---

echo [9/12] status_indicator off
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=status_indicator --var brightness=0.0
call :copy_latest status_indicator led_off

echo [10/12] status_indicator amber
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=status_indicator --var brightness=0.55 --var color=0.92,0.65,0.10
call :copy_latest status_indicator led_amber

echo [11/12] status_indicator green
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=status_indicator --var brightness=1.0
call :copy_latest status_indicator led_green

echo [12/12] status_indicator red
"%BIN%" --plugin-dir "%PLUGIN_DIR%" --capture --shader=status_indicator --var brightness=1.0 --var color=0.88,0.15,0.12
call :copy_latest status_indicator led_red

echo.
echo === Done: %IMG_DIR% ===
dir /b "%IMG_DIR%\*.png" 2>nul
echo.
exit /b 0

REM ---------------------------------------------------------------------------
:copy_latest
REM   %1 = shader id (for filename glob)
REM   %2 = destination name (without .png)
REM ---------------------------------------------------------------------------
for /f "delims=" %%F in ('dir /b /o-d "%CAPTURES%\*%1*" 2^>nul') do (
    copy /y "%CAPTURES%\%%F" "%IMG_DIR%\%2.png" >nul
    exit /b
)
echo   WARNING: no capture found for %1
exit /b

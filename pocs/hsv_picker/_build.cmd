@echo off
REM execute from this directory regardless of CWD invocation
REM this means the reponse file paths are relative from here
pushd %~dp0
clang @hsv_picker_tool.response
popd
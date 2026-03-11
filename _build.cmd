@echo off
llvm-rc /fo src\app.res src\app.rc || exit /b 1
clang @main.response || exit /b 1
clang @pocs\hsv_picker_tool.response

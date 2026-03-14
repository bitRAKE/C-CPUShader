@echo off
if "%VULKAN_SDK%"=="" (
    echo VULKAN_SDK is not set.
    echo Expected a Vulkan SDK install exposed through %%VULKAN_SDK%%.
    exit /b 1
)
llvm-rc /fo src\app.res src\app.rc || exit /b 1
clang -I"%VULKAN_SDK%\Include" -L"%VULKAN_SDK%\Lib" @main.response || exit /b 1
clang @pocs\hsv_picker_tool.response

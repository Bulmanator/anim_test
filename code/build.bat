@echo off
setlocal

pushd "%~dp0" > nul

if not exist "..\build" (mkdir "..\build")
pushd "..\build" > nul

glslang -V "..\code\shaders\basic.vert" -o "shaders\basic.vert.spv"
glslang -V "..\code\shaders\basic.frag" -o "shaders\basic.frag.spv"

set cl_options=-nologo -Od -Zi -W4 -WX -wd4505 -wd4201 -I"libs\SDL2\include" -I"%VULKAN_SDK%\Include"
set link_options=-libpath:"libs\SDL2\lib" SDL2d.lib

cl %cl_options% "..\code\animation.cpp" -Fe"animation.exe" -link %link_options%

popd

popd

endlocal

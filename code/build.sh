#!/bin/sh
#

SRC_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
pushd $SRC_DIR > /dev/null

if [[ ! -d "../build" ]];
then
    mkdir "../build"
fi

pushd "../build" > /dev/null

if [[ ! -d "shaders" ]];
then
    mkdir "shaders"
fi

# compile shaders to spir-v
#
glslang -V "../code/shaders/basic.vert" -o "shaders/basic.vert.spv"
glslang -V "../code/shaders/basic.frag" -o "shaders/basic.frag.spv"

# compile application
#
COMPILER_OPTS="-O0 -g -ggdb -Wall -Werror -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-variable"
LINKER_OPTS="-lSDL2"

echo "../code/animation.cpp"

g++ $COMPILER_OPTS "../code/animation.cpp" -o "animation" $LINKER_OPTS

popd > /dev/null
popd > /dev/null

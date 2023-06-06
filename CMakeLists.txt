cmake_minimum_required(VERSION 3.10.0)
set(CMAKE_VERBOSE_MAKEFILE true)

# Project configuration start:

project(audioEngineWASM)

# Add sources:
set(CMAKE_OSX_SYSROOT "")

# Add main target: 

add_executable(libaudioengine.js test.cpp)

# Build/link configuration options:
# For emscripten 3.1.xx
set(COMMON_LINK_FLAGS "--bind -s SINGLE_FILE=1 -s WASM=1 -s MAXIMUM_MEMORY=4GB -s ALLOW_MEMORY_GROWTH=1 -s WASM_ASYNC_COMPILATION=0 -s EXPORTED_FUNCTIONS=\"['_malloc', '_free']\" -O3 --extern-post-js aw_processor.js")

# For emscripten 2.0.xx
# set(COMMON_LINK_FLAGS "--bind -s SINGLE_FILE=1 -s WASM=1 -s WASM_ASYNC_COMPILATION=0 -s ALLOW_MEMORY_GROWTH=1 -O3 --extern-post-js aw_processor.js")
set_target_properties(libaudioengine.js
        PROPERTIES LINK_FLAGS "-fwasm-exceptions ${COMMON_LINK_FLAGS}"
        COMPILE_FLAGS "-fwasm-exceptions -msimd128 -msse3"
)
        
set ( CMAKE_C_COMPILER emcc)
set ( CMAKE_CXX_COMPILER emcc)

set (CMAKE_OSX_DEPLOYMENT_TARGET "")  # to avoid [-Wunused-command-line-argument] warning caused by an OSX version flag
set (CMAKE_CXX_LINK_FLAGS "")   # clear the link flags to prevent wasm-ld errors for -search_paths_first -headerpad_max_install_names

set ( CMAKE_C_FLAGS "-O3" )
set ( CMAKE_CXX_FLAGS "-O3 -std=c++17" )



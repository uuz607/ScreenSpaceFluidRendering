# Copyright (c) 2016-2025, Sascha Willems
# SPDX-License-Identifier: MIT
#
# Modified by Liu Linqi (c) 2026

# Function for building single example
SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "Generating project file for example in ${EXAMPLE_FOLDER}")

# Main
file(GLOB SOURCE *.cpp ${BASE_HEADERS} ${EXAMPLE_FOLDER}/*.cpp  ${EXAMPLE_FOLDER}/*.h)
SET(MAIN_CPP ${EXAMPLE_FOLDER}/main.cpp)

# Add shaders
set(SHADER_DIR_HLSL "../Shaders")
file(GLOB SHADERS_HLSL "${SHADER_DIR_HLSL}/*.hlsl")
source_group("Shaders" FILES ${SHADERS_HLSL})

# Add simulator folder
set(SIMULATOR_DIR "${EXAMPLE_FOLDER}/Simulator")
file(GLOB_RECURSE  SIMULATOR_CPP "./Simulator/*.cpp")
file(GLOB_RECURSE  SIMULATOR_H "./Simulator/*.h")
set(SIMULATOR_FILES ${SIMULATOR_CPP} ${SIMULATOR_H})
source_group(TREE ${SIMULATOR_DIR} FILES ${SIMULATOR_FILES})

# Add renderer folder
set(RENDERER_DIR "${EXAMPLE_FOLDER}/Renderer")
file(GLOB_RECURSE  RENDER_CPP "./Renderer/*.cpp")
file(GLOB_RECURSE  RENDER_H "./Renderer/*.h")
set(RENDER_FILES ${RENDER_CPP} ${RENDER_H})
source_group(TREE ${RENDERER_DIR} FILES ${RENDER_FILES})


if(WIN32)
	add_executable(SSFR WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_HLSL} ${SIMULATOR_FILES} ${RENDER_FILES})
	target_link_libraries(SSFR base ${Vulkan_LIBRARY} ${WINLIBS})
else(WIN32)
	add_executable(SSFR ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_HLSL} ${SIMULATOR_FILES} ${RENDER_FILES})
	target_link_libraries(SSFR base )
endif(WIN32)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set_target_properties(SSFR PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

target_include_directories(SSFR PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SIMULATOR_DIR} ${RENDERER_DIR})

if(RESOURCE_INSTALL_DIR)
	install(TARGETS SSFR DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
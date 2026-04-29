cmake_minimum_required(VERSION 3.20)
project(DemoScene)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE DEMO_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
    src/*.hlsl
    src/*.hlsli
)

add_executable(DemoScene ${DEMO_SOURCES})

target_link_libraries(DemoScene PRIVATE
    DX12Engine
    Microsoft::DirectX-Headers
)

target_include_directories(DemoScene PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}/_deps/directx-headers-src/include/directx"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${DEMO_SOURCES})

set(COOKED_ASSET_DIR "${CMAKE_BINARY_DIR}/res")
set(DEMO_STAGE_ROOT_DIR "${CMAKE_BINARY_DIR}/DemoScene")
set(DEMO_STAGE_ROOT_RES_DIR "${DEMO_STAGE_ROOT_DIR}/res")
set(DEMO_STAGE_CONFIG_RES_DIR "${DEMO_STAGE_ROOT_DIR}/$<CONFIG>/res")
set(DEMO_EXE_RES_DIR "$<TARGET_FILE_DIR:DemoScene>/res")

file(GLOB_RECURSE ENGINE_SHADER_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/DX12Engine/Shaders/*.hlsl"
    "${CMAKE_SOURCE_DIR}/src/DX12Engine/Shaders/*.hlsli"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hlsli"
)

set(ENGINE_SHADER_ROOT "${CMAKE_SOURCE_DIR}/src/DX12Engine/Shaders")
set(DEMO_SHADER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_custom_target(CopyDemoResources ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${DEMO_STAGE_ROOT_RES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${DEMO_STAGE_CONFIG_RES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${COOKED_ASSET_DIR}"
        "${DEMO_STAGE_ROOT_RES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${COOKED_ASSET_DIR}"
        "${DEMO_STAGE_CONFIG_RES_DIR}"
    COMMENT "Staging cooked DemoScene resources in build output directories"
)

function(add_shader_copy_commands target_name shaders_output_dir)
    foreach(SHADER_FILE ${ENGINE_SHADER_FILES})
        file(RELATIVE_PATH SHADER_REL_ENGINE "${ENGINE_SHADER_ROOT}" "${SHADER_FILE}")
        if(NOT SHADER_REL_ENGINE MATCHES "^\\.\\.")
            set(SHADER_REL_PATH "${SHADER_REL_ENGINE}")
        else()
            file(RELATIVE_PATH SHADER_REL_PATH "${DEMO_SHADER_ROOT}" "${SHADER_FILE}")
        endif()

        get_filename_component(SHADER_REL_DIR "${SHADER_REL_PATH}" DIRECTORY)
        if(SHADER_REL_DIR STREQUAL "")
            set(SHADER_DEST_DIR "${shaders_output_dir}")
        else()
            set(SHADER_DEST_DIR "${shaders_output_dir}/${SHADER_REL_DIR}")
        endif()

        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_DEST_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SHADER_FILE}"
                "${SHADER_DEST_DIR}"
        )
    endforeach()
endfunction()

add_shader_copy_commands(CopyDemoResources "${DEMO_STAGE_ROOT_RES_DIR}/Shaders")
add_shader_copy_commands(CopyDemoResources "${DEMO_STAGE_CONFIG_RES_DIR}/Shaders")

add_dependencies(DemoScene CopyDemoResources)

add_custom_command(TARGET DemoScene POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR="${DEMO_STAGE_CONFIG_RES_DIR}"
        -DDEST_DIR="${DEMO_EXE_RES_DIR}"
        -P "${CMAKE_SOURCE_DIR}/cmake/CopyDirectoryIfDifferent.cmake"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${DEMO_EXE_RES_DIR}/Shaders"
    COMMENT "Copying DemoScene resources to executable directory"
)

add_shader_copy_commands(DemoScene "${DEMO_EXE_RES_DIR}/Shaders")

file(GLOB DXCOMPILER_DLL_CANDIDATES
    "$ENV{WindowsSdkDir}/bin/*/x64/dxcompiler.dll"
    "$ENV{WindowsSdkDir}/Redist/D3D/x64/dxcompiler.dll"
    "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxcompiler.dll"
    "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/x64/dxcompiler.dll"
)
file(GLOB DXIL_DLL_CANDIDATES
    "$ENV{WindowsSdkDir}/bin/*/x64/dxil.dll"
    "$ENV{WindowsSdkDir}/Redist/D3D/x64/dxil.dll"
    "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxil.dll"
    "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/x64/dxil.dll"
)

list(LENGTH DXCOMPILER_DLL_CANDIDATES DXCOMPILER_DLL_COUNT)
if(DXCOMPILER_DLL_COUNT GREATER 0)
    list(GET DXCOMPILER_DLL_CANDIDATES 0 DXCOMPILER_DLL)
    add_custom_command(TARGET DemoScene POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXCOMPILER_DLL}"
            "$<TARGET_FILE_DIR:DemoScene>"
        COMMENT "Copying dxcompiler.dll"
    )
endif()

list(LENGTH DXIL_DLL_CANDIDATES DXIL_DLL_COUNT)
if(DXIL_DLL_COUNT GREATER 0)
    list(GET DXIL_DLL_CANDIDATES 0 DXIL_DLL)
    add_custom_command(TARGET DemoScene POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXIL_DLL}"
            "$<TARGET_FILE_DIR:DemoScene>"
        COMMENT "Copying dxil.dll"
    )
endif()

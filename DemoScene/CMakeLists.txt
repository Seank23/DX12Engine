cmake_minimum_required(VERSION 3.20)
project(DemoScene)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE DEMO_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
    src/*.hlsl
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

set(DEMO_BUILD_ROOT_RES_DIR "${CMAKE_CURRENT_BINARY_DIR}/res")
set(DEMO_EXE_RES_DIR "$<TARGET_FILE_DIR:DemoScene>/res")

file(GLOB_RECURSE ENGINE_SHADER_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hlsl"
)

add_custom_target(CopyDemoResources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/res"
        "${DEMO_BUILD_ROOT_RES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${DEMO_BUILD_ROOT_RES_DIR}/Shaders"
    COMMENT "Copying DemoScene resources to build root"
)

foreach(SHADER_FILE ${ENGINE_SHADER_FILES})
    add_custom_command(TARGET CopyDemoResources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHADER_FILE}"
            "${DEMO_BUILD_ROOT_RES_DIR}/Shaders"
    )
endforeach()

add_dependencies(DemoScene CopyDemoResources)

add_custom_command(TARGET DemoScene POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/res"
        "${DEMO_EXE_RES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${DEMO_EXE_RES_DIR}/Shaders"
    COMMENT "Copying DemoScene resources to executable directory"
)

foreach(SHADER_FILE ${ENGINE_SHADER_FILES})
    add_custom_command(TARGET DemoScene POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHADER_FILE}"
            "${DEMO_EXE_RES_DIR}/Shaders"
    )
endforeach()

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

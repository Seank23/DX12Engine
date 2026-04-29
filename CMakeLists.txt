cmake_minimum_required(VERSION 3.20)
project(DX12Engine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# DirectX-Headers
FetchContent_Declare(
  DirectX-Headers
  GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(DirectX-Headers)

# DirectXTex
FetchContent_Declare(
  DirectXTex
  GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(DirectXTex)

# tinyobjloader
FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG        release
)
FetchContent_MakeAvailable(tinyobjloader)

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
FetchContent_Declare(
  tinygltf
  GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
  GIT_TAG        release
)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(
  meshoptimizer
  GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(meshoptimizer)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
    src/*.hlsl
    src/*.hlsli
)

# Your executable
add_library(DX12Engine STATIC ${ENGINE_SOURCES})

add_subdirectory(DemoScene)
add_subdirectory(tools/AssetCooker)

set(RAW_ASSET_DIR "${CMAKE_SOURCE_DIR}/res")
set(COOKED_ASSET_DIR "${CMAKE_BINARY_DIR}/res")

add_custom_target(CookAssets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${COOKED_ASSET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${RAW_ASSET_DIR}" "${COOKED_ASSET_DIR}"
    COMMAND $<TARGET_FILE:AssetCooker> --in "${RAW_ASSET_DIR}" --out "${COOKED_ASSET_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DCOOKED_DIR="${COOKED_ASSET_DIR}"
        -P "${CMAKE_SOURCE_DIR}/cmake/PruneCookedSourceTextures.cmake"
    DEPENDS AssetCooker
    COMMENT "Cooking assets into ${COOKED_ASSET_DIR}"
)

if(TARGET CopyDemoResources)
    add_dependencies(CopyDemoResources CookAssets)
endif()

if(TARGET DemoScene)
    add_dependencies(DemoScene CookAssets)
endif()

# Link dependencies
target_link_libraries(DX12Engine PUBLIC
    DirectXTex
    tinyobjloader
    nlohmann_json::nlohmann_json
    D3D12.lib
    d3d12.lib
    dxgi.lib
    dxguid.lib
    D3DCompiler.lib
    dxcompiler.lib
	Microsoft::DirectX-Headers
)

# Include headers
target_include_directories(DX12Engine PRIVATE
    ${directx-headers_SOURCE_DIR}/include/directx
    ${tinygltf_SOURCE_DIR}
)

foreach(SHADER ${ENGINE_SOURCES})
    get_filename_component(FILENAME ${SHADER} NAME)
    set(SHADER_TYPE Pixel)

    # Detect shader type from filename
    IF(FILENAME MATCHES ".*hlsl.*")
        if(FILENAME MATCHES ".*VS.*")
            set(SHADER_TYPE Vertex)
        elseif(FILENAME MATCHES ".*PS.*")
            set(SHADER_TYPE Pixel)
        else()
            message(WARNING "Skipping unknown shader type: ${SHADER}")
            continue()
        endif()
    endif()

    set_source_files_properties(${SHADER} PROPERTIES
        VS_SHADER_TYPE ${SHADER_TYPE}
        VS_SHADER_MODEL 6.0
    )

endforeach()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${ENGINE_SOURCES})


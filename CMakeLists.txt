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

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
    src/*.hlsl
)

# Your executable
add_library(DX12Engine STATIC ${ENGINE_SOURCES})

add_subdirectory(DemoScene)

# Link dependencies
target_link_libraries(DX12Engine PUBLIC
    DirectXTex
    tinyobjloader
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


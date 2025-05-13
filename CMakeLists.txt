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

file(GLOB_RECURSE RENDERER_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
    src/*.hlsl
)

# Your executable
add_executable(DX12Engine ${RENDERER_SOURCES})

# Link dependencies
target_link_libraries(DX12Engine
    DirectXTex
    tinyobjloader
    D3D12.lib
    d3d12.lib
    dxgi.lib
    dxguid.lib
    D3DCompiler.lib
    dxcompiler.lib
)

# Include headers
target_include_directories(DX12Engine PRIVATE
    ${directx-headers_SOURCE_DIR}/include/directx
)

foreach(SHADER ${RENDERER_SOURCES})
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

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${RENDERER_SOURCES})

file(COPY "${CMAKE_SOURCE_DIR}/res" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/res/Shaders")
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS src/*.hlsl)

foreach(SHADER_FILE ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)

    add_custom_command(
        OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SHADER_FILE}" "${SHADER_OUTPUT_DIR}/${SHADER_NAME}"
        DEPENDS "${SHADER_FILE}"
        COMMENT "Copying ${SHADER_NAME} to ${SHADER_OUTPUT_DIR}"
        VERBATIM
    )

    list(APPEND COPIED_SHADER_FILES "${SHADER_OUTPUT_DIR}/${SHADER_NAME}")
endforeach()

add_custom_target(CopyShaders ALL
    DEPENDS ${COPIED_SHADER_FILES}
)
add_dependencies(DX12Engine CopyShaders)


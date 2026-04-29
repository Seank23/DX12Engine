set(ASSET_COOKER_SOURCES
    src/main.cpp
)

add_executable(AssetCooker ${ASSET_COOKER_SOURCES})
target_link_libraries(AssetCooker PRIVATE DirectXTex meshoptimizer)
target_compile_features(AssetCooker PRIVATE cxx_std_20)
target_include_directories(AssetCooker PRIVATE
    ${tinygltf_SOURCE_DIR}
    ${meshoptimizer_SOURCE_DIR}/src
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${ASSET_COOKER_SOURCES})

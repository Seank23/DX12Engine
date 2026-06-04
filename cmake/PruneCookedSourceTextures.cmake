if(NOT DEFINED COOKED_DIR OR COOKED_DIR STREQUAL "")
    message(FATAL_ERROR "COOKED_DIR must be provided")
endif()

set(_patterns
    "*.png"
    "*.jpg"
    "*.jpeg"
    "*.bmp"
    "*.tga"
    "*.hdr"
    "*.tif"
    "*.tiff"
    "*.glb"
    "*.GLB"
)

foreach(_pattern IN LISTS _patterns)
    file(GLOB_RECURSE _files LIST_DIRECTORIES false "${COOKED_DIR}/${_pattern}")
    if(_files)
        file(REMOVE ${_files})
    endif()
endforeach()

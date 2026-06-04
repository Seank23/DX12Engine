if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "SOURCE_DIR must be provided")
endif()

if(NOT DEFINED DEST_DIR OR DEST_DIR STREQUAL "")
    message(FATAL_ERROR "DEST_DIR must be provided")
endif()

cmake_path(NORMAL_PATH SOURCE_DIR OUTPUT_VARIABLE _source_dir)
cmake_path(NORMAL_PATH DEST_DIR OUTPUT_VARIABLE _dest_dir)

if(_source_dir STREQUAL _dest_dir)
    message(STATUS "Skipping copy; source and destination are identical: ${_source_dir}")
    return()
endif()

if(NOT EXISTS "${_source_dir}")
    message(FATAL_ERROR "Source directory does not exist: ${_source_dir}")
endif()

get_filename_component(_dest_parent "${_dest_dir}" DIRECTORY)
file(MAKE_DIRECTORY "${_dest_parent}")

if(EXISTS "${_dest_dir}")
    file(REMOVE_RECURSE "${_dest_dir}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_source_dir}" "${_dest_dir}"
    RESULT_VARIABLE _copy_result
)

if(NOT _copy_result EQUAL 0)
    message(FATAL_ERROR "Failed to copy directory from '${_source_dir}' to '${_dest_dir}'")
endif()
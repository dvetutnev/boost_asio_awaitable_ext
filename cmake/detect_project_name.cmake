find_package(Git REQUIRED)

function(detect_project_name output_var)
    execute_process(COMMAND ${GIT_EXECUTABLE} "config" "--get" "remote.origin.url"
        RESULT_VARIABLE cmd_result
        OUTPUT_VARIABLE repository_url
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        #ERROR_QUIET)

    if(cmd_result AND NOT cdm_result EQUAL 0)
        message(FATAL_ERROR "Failed to obtain project name")
    endif()

    string(REGEX MATCH ".*/(.+)\\.git" repository_name ${repository_url})
    set(${output_var} ${CMAKE_MATCH_1} PARENT_SCOPE)
endfunction()

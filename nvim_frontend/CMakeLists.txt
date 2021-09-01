set(TARGET_NAME nvim_frontend)
add_library(${TARGET_NAME} "nvim_frontend.cpp" "nvim_pipe.cpp"
                           "nvim_redraw.cpp" "nvim_grid.cpp")
target_compile_definitions(${TARGET_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${TARGET_NAME} PRIVATE asio msgpackpp plog)

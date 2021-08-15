add_library(asio INTERFACE)
target_include_directories(asio
INTERFACE
${CMAKE_CURRENT_LIST_DIR}/asio-1.18.2/include
)

# add_executable(executors_pipeline
# ${CMAKE_CURRENT_LIST_DIR}/asio-1.18.2/src/examples/cpp11/executors/pipeline.cpp
# )
# target_link_libraries(executors_pipeline PRIVATE asio)

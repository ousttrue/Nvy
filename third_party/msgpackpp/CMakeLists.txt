add_library(msgpackpp INTERFACE)
target_include_directories(msgpackpp
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(msgpackpp
    INTERFACE
    NOMINMAX
)

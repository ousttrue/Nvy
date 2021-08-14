add_library(mpack
    mpack.c
)

target_compile_definitions(mpack PUBLIC
    MPACK_EXTENSIONS
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(mpack PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

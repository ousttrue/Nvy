set(TARGET_NAME coroutine_sample)

add_executable(${TARGET_NAME} main.cpp)
target_compile_options(${TARGET_NAME} PRIVATE /await)

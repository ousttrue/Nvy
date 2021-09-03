set(TARGET_NAME Nvy)
add_executable(${TARGET_NAME} WIN32)

target_sources(
  ${TARGET_NAME} PUBLIC main.cpp renderer/d3d.cpp renderer/swapchain.cpp
                        win32window.cpp nvim/nvim_icon.rc)
target_compile_definitions(${TARGET_NAME} PRIVATE UNICODE)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(
  ${TARGET_NAME}
  PUBLIC plog
         nvim_frontend
         nvim_renderer_d2d
         nvim_win32
         user32.lib
         d3d11.lib
         d2d1.lib
         dwrite.lib
         Shcore.lib
         Dwmapi.lib
         winmm.lib)

if(MSVC)
  string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  # string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
endif()

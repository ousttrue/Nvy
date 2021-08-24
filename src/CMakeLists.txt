add_executable(Nvy WIN32
${THIRDPARTY_DIR}/nvim/nvim_icon.rc
)

target_sources(Nvy PUBLIC
"main.cpp"
"nvim.cpp"
"renderer.cpp"
"grid.cpp"
)

target_link_libraries(Nvy PUBLIC 
    mpack
    plog
    msgpackpp
    user32.lib 
    d3d11.lib 
    d2d1.lib 
    dwrite.lib
    Shcore.lib
    Dwmapi.lib
    winmm.lib
)

target_compile_definitions(Nvy PUBLIC
    UNICODE
    NOMINMAX
)

if(MSVC)
	string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	# string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
endif()

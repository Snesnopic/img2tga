cmake_minimum_required(VERSION 3.16)
project(img2tga VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "LTO (IPO) enabled.")
else()
    message(WARNING "LTO (IPO) not supported by this toolchain.")
endif()

add_executable(img2tga
        src/main.cpp
)

if(MSVC)
    target_compile_options(img2tga PRIVATE /Gy)

    target_link_options(img2tga PRIVATE /OPT:REF)

else()
    target_compile_options(img2tga PRIVATE -ffunction-sections -fdata-sections)

    if(APPLE)
        target_link_options(img2tga PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(img2tga PRIVATE -Wl,--gc-sections)
    endif()
endif()


target_include_directories(img2tga PRIVATE
        ${PROJECT_SOURCE_DIR}/vendor
)

if(UNIX AND NOT APPLE)
    target_link_libraries(img2tga PRIVATE m)
endif()

install(TARGETS img2tga DESTINATION bin)

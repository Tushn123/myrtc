cmake_minimum_required (VERSION 3.5)

project(libice)

# 静态库输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# 动态库输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# 可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/${CMAKE_SYSTEM_NAME})

#option(USE_DTLS "use dtls or not" OFF)

set(USE_DTLS OFF)

if (USE_DTLS)
    add_definitions(-DUSE_DTLS)
endif()

include_directories(
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/third_party"
    "${PROJECT_SOURCE_DIR}/third_party/libev/include"
    "${PROJECT_SOURCE_DIR}/third_party/libwebrtc/include"
    "${PROJECT_SOURCE_DIR}/third_party/absl/include"
)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_definitions(-DUNICODE -D_UNICODE -DWIN32 -D_WIN32 -DWIN32_LEAN_AND_MEAN 
        -DNOMINMAX 
        -D_WINDOWS
        -D_USE_MATH_DEFINES
        -DWEBRTC_WIN
        -DWEBRTC_EXTERNAL_JSON
        -DICE_WIN)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions(-g -pipe -W -Wall -fPIC -std=gnu++14 
        -DWEBRTC_POSIX
        -DWEBRTC_LINUX 
        -DICE_POSIX
        -DICE_LINUX
        -DNDEBUG 
        -Wno-unused-parameter)
endif()

file(GLOB ice_src
    "./ice/*.cpp"
    "./ice/base/*.cpp"
)

file(GLOB dtls_src
    "./ice/dtls/*.cpp"
)

file(GLOB linux_src
    "./ice/base/linux/*.cpp"
)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    if (USE_DTLS)
        add_library(libice STATIC ${ice_src} ${dtls_src})
    else()
        add_library(libice STATIC ${ice_src})
    endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    if (USE_DTLS)
        add_library(libice ${ice_src} ${linux_src} ${dtls_src})
    else()
        add_library(libice ${ice_src} ${linux_src})
    endif()
endif()


install(TARGETS libice ARCHIVE DESTINATION lib)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/ice
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
)




cmake_minimum_required(VERSION 3.8)

project(xrtc-third_party)

if (CMAKE_CL_64) 
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(BUILD_TYPE "x64-Debug")
    else()
        set(BUILD_TYPE "x64-Release")
    endif()
else()
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(BUILD_TYPE "x86-Debug")
    else()
        set(BUILD_TYPE "x86-Release")
    endif()
endif()

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/bin)

set(ABSL_ENABLE_INSTALL true)
set(XRTC_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 设置安装目录
set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/${CMAKE_SYSTEM_NAME})

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    message(STATUS "current platform: windows")
    add_definitions(-DUNICODE -D_UNICODE -DWIN32 -D_WIN32 
        #-DWIN32_LEAN_AND_MEAN 
        #-DNOMINMAX 
        -D_WINDOWS
        -D_USE_MATH_DEFINES)
endif()

include_directories(
    ./libjpeg-turbo
    ./libjpeg-turbo/include
)

#add_subdirectory("./abseil-cpp")
#add_subdirectory("./libjpeg-turbo")
#add_subdirectory("./libyuv")
#add_subdirectory("./jsoncpp/source")
#add_subdirectory("./curl-7.81.0")
#add_subdirectory("./opus-1.3.1")
#add_subdirectory("./libice")
add_subdirectory("./libwebrtc")

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    #add_subdirectory("./NIM_Duilib_Framework")
endif()

install(DIRECTORY 
    # 安装openssl-1.1.1l
    ${XRTC_THIRD_PARTY_DIR}/openssl-1.1.1l/out/${BUILD_TYPE}
    # 安装x264-164
    ${XRTC_THIRD_PARTY_DIR}/x264-164/out/${BUILD_TYPE}
    # 安装x265-3.3
    #${SMT_THIRD_PARTY_DIR}/x265-3.3/out/${BUILD_TYPE}
    # 安装ffmpeg-4.4.1
    #${SMT_THIRD_PARTY_DIR}/ffmpeg-4.4.1/out/${BUILD_TYPE}
    DESTINATION ${XRTC_THIRD_PARTY_DIR}/out/install
)

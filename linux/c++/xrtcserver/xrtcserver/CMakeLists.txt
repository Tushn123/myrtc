cmake_minimum_required(VERSION 3.5)

project(xrtcserver)

# 添加这行，禁用位置无关可执行文件
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

add_compile_options(-no-pie)
add_link_options(-no-pie)
add_definitions("-g -pipe -W -Wall -fPIC -std=gnu++14 -DWEBRTC_POSIX
    -DWEBRTC_LINUX")

include_directories(
    "./src"
    "./third_party/include"
    "../rtcbase/src"
)

link_directories(
    "./third_party/lib"
    "../rtcbase/out"
)

file(GLOB all_src
    "./src/*.cpp"
    "./src/base/*.cpp"
    "./src/server/*.cpp"
    "./src/stream/*.cpp"
    "./src/pc/*.cpp"
    "./src/ice/*.cpp"
    "./src/module/rtp_rtcp/*.cpp"
)

add_executable(xrtcserver ${all_src})

target_link_libraries(xrtcserver libyaml-cpp.a librtcbase.a
    libabsl_strings.a libabsl_throw_delegate.a libabsl_bad_optional_access.a
    libev.a libjsoncpp.a libssl.a libcrypto.a libsrtp2.a
    -lpthread -ldl
)




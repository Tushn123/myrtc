// WebrtcFFmpegRecord.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

#include <api/create_peerconnection_factory.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}



int main()
{
    // 测试 FFmpeg
    std::cout << "=== FFmpeg Test ===" << std::endl;
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    std::cout << "AVCodec version: " << avcodec_version() << std::endl;

    rtc::InitializeSSL();
    auto thread = rtc::Thread::Create();
    if (thread) {
        std::cout << "WebRTC thread created successfully" << std::endl;
    }
    rtc::CleanupSSL();
}

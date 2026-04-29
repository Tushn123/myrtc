// 使用webrtc进行画面采集，ffmpeg进行录制

// main.cpp
// Windows console demo:
// - enumerate camera/microphone devices
// - select first devices (fixed in code)
// - capture video/audio via WebRTC
// - encode/mux to local MP4 via FFmpeg 5.1
//
// WebRTC side:
// - modern video capture API:
//   VideoCaptureFactory::CreateDeviceInfo()
//   VideoCaptureFactory::Create(unique_name)
//   RegisterCaptureDataCallback(rtc::VideoSinkInterface<VideoFrame>*)
// - video frames are converted through video_frame_buffer()->ToI420()
//
// FFmpeg side:
// - video: MPEG4 -> YUV420P
// - audio: AAC -> FLTP (resampled from WebRTC PCM S16 to encoder format)
// - audio resampling uses AVChannelLayout / swr_alloc_set_opts2
//
// Build notes:
// - C++17 or later
// - Link against WebRTC and FFmpeg libs:
//   avformat, avcodec, avutil, swscale, swresample
//
// This is a single-file test program, not a production architecture.

#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_sink_interface.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"

static std::string FfmpegErrorToString(int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(err, buf, sizeof(buf));
    return std::string(buf);
}

static void PrintFfmpegErr(const char* step, int ret) {
    std::cerr << "[FFmpeg] " << step << " failed: " << FfmpegErrorToString(ret) << "\n";
}

static void CopyPlane(const uint8_t* src, int src_stride,
    uint8_t* dst, int dst_stride,
    int width, int height) {
    for (int y = 0; y < height; ++y) {
        std::memcpy(dst + y * dst_stride, src + y * src_stride, static_cast<size_t>(width));
    }
}

class Recorder {
public:
    Recorder() = default;
    ~Recorder() { Finalize(); }

    bool Init(const std::string& output_path, int video_width, int video_height, int video_fps) {
        output_path_ = output_path;
        video_width_ = video_width;
        video_height_ = video_height;
        video_fps_ = video_fps;

        int ret = avformat_alloc_output_context2(&fmt_ctx_, nullptr, "mp4", output_path_.c_str());
        if (ret < 0 || !fmt_ctx_) {
            PrintFfmpegErr("avformat_alloc_output_context2", ret);
            return false;
        }

        if (!OpenVideoStream()) return false;
        if (!OpenAudioStream()) return false;

        if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&fmt_ctx_->pb, output_path_.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                PrintFfmpegErr("avio_open", ret);
                return false;
            }
        }

        ret = avformat_write_header(fmt_ctx_, nullptr);
        if (ret < 0) {
            PrintFfmpegErr("avformat_write_header", ret);
            return false;
        }

        std::cout << "[Recorder] output: " << output_path_ << "\n";
        std::cout << "[Recorder] video : " << video_width_ << "x" << video_height_
            << " @" << video_fps_ << " fps\n";
        std::cout << "[Recorder] audio : " << audio_sample_rate_ << " Hz, stereo\n";
        return true;
    }

    void OnVideoFrame(const webrtc::VideoFrame& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!fmt_ctx_ || !video_enc_ctx_) return;

        auto i420 = frame.video_frame_buffer()->ToI420();
        if (!i420) return;

        const int src_w = i420->width();
        const int src_h = i420->height();

        AVFrame* avf = av_frame_alloc();
        if (!avf) return;

        avf->format = video_enc_ctx_->pix_fmt;
        avf->width = video_width_;
        avf->height = video_height_;
        avf->pts = video_pts_++;

        int ret = av_frame_get_buffer(avf, 32);
        if (ret < 0) {
            av_frame_free(&avf);
            return;
        }

        ret = av_frame_make_writable(avf);
        if (ret < 0) {
            av_frame_free(&avf);
            return;
        }

        if (src_w == video_width_ && src_h == video_height_) {
            CopyPlane(i420->DataY(), i420->StrideY(), avf->data[0], avf->linesize[0], video_width_, video_height_);
            CopyPlane(i420->DataU(), i420->StrideU(), avf->data[1], avf->linesize[1], video_width_ / 2, video_height_ / 2);
            CopyPlane(i420->DataV(), i420->StrideV(), avf->data[2], avf->linesize[2], video_width_ / 2, video_height_ / 2);
        }
        else {
            sws_ctx_ = sws_getCachedContext(
                sws_ctx_,
                src_w, src_h, AV_PIX_FMT_YUV420P,
                video_width_, video_height_, AV_PIX_FMT_YUV420P,
                SWS_BILINEAR,
                nullptr, nullptr, nullptr);

            if (!sws_ctx_) {
                av_frame_free(&avf);
                std::cerr << "[Recorder] sws_getCachedContext failed\n";
                return;
            }

            const uint8_t* src_data[4] = {
                i420->DataY(),
                i420->DataU(),
                i420->DataV(),
                nullptr
            };
            int src_linesize[4] = {
                i420->StrideY(),
                i420->StrideU(),
                i420->StrideV(),
                0
            };

            sws_scale(sws_ctx_, src_data, src_linesize, 0, src_h, avf->data, avf->linesize);
        }

        EncodeAndWrite(video_enc_ctx_, video_stream_, avf);
        av_frame_free(&avf);
    }

    void OnAudioSamples(const void* audioSamples,
        size_t nSamples,
        size_t nBytesPerSample,
        size_t nChannels,
        uint32_t samplesPerSec) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!fmt_ctx_ || !audio_enc_ctx_ || !audio_fifo_) return;
        if (!audioSamples || nSamples == 0 || nBytesPerSample != 2) return;

        if (!EnsureAudioResampler(static_cast<int>(samplesPerSec), static_cast<int>(nChannels))) {
            return;
        }

        const int in_rate = static_cast<int>(samplesPerSec);
        const int out_rate = audio_enc_ctx_->sample_rate;
        const int out_channels = audio_enc_ctx_->ch_layout.nb_channels;

        const int64_t delay = swr_get_delay(swr_ctx_, in_rate);
        const int dst_nb_samples = static_cast<int>(
            av_rescale_rnd(delay + static_cast<int64_t>(nSamples), out_rate, in_rate, AV_ROUND_UP));

        AVFrame* converted = av_frame_alloc();
        if (!converted) return;

        converted->nb_samples = dst_nb_samples;
        converted->format = audio_enc_ctx_->sample_fmt;
        converted->sample_rate = out_rate;
        converted->ch_layout = audio_enc_ctx_->ch_layout;

        int ret = av_frame_get_buffer(converted, 0);
        if (ret < 0) {
            av_frame_free(&converted);
            return;
        }

        const uint8_t* in_data[1] = {
            reinterpret_cast<const uint8_t*>(audioSamples)
        };

        ret = swr_convert(
            swr_ctx_,
            converted->extended_data,
            dst_nb_samples,
            in_data,
            static_cast<int>(nSamples));

        if (ret < 0) {
            PrintFfmpegErr("swr_convert", ret);
            av_frame_free(&converted);
            return;
        }

        converted->nb_samples = ret;

        if (!EnsureAudioFifoCapacity(av_audio_fifo_size(audio_fifo_) + converted->nb_samples)) {
            av_frame_free(&converted);
            return;
        }

        ret = av_audio_fifo_write(audio_fifo_, reinterpret_cast<void**>(converted->extended_data), converted->nb_samples);
        if (ret < converted->nb_samples) {
            std::cerr << "[Recorder] av_audio_fifo_write failed\n";
            av_frame_free(&converted);
            return;
        }

        av_frame_free(&converted);

        DrainAudioFifo(false);
        (void)out_channels;
    }

    void Finalize() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!fmt_ctx_) return;

        FlushAudioResampler();
        DrainAudioFifo(true);

        if (video_enc_ctx_) {
            EncodeAndWrite(video_enc_ctx_, video_stream_, nullptr);
        }
        if (audio_enc_ctx_) {
            EncodeAndWrite(audio_enc_ctx_, audio_stream_, nullptr);
        }

        av_write_trailer(fmt_ctx_);

        if (video_enc_ctx_) {
            avcodec_free_context(&video_enc_ctx_);
        }
        if (audio_enc_ctx_) {
            avcodec_free_context(&audio_enc_ctx_);
        }
        if (sws_ctx_) {
            sws_freeContext(sws_ctx_);
            sws_ctx_ = nullptr;
        }
        if (swr_ctx_) {
            swr_free(&swr_ctx_);
        }
        if (audio_fifo_) {
            av_audio_fifo_free(audio_fifo_);
            audio_fifo_ = nullptr;
        }

        if (fmt_ctx_) {
            if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE) && fmt_ctx_->pb) {
                avio_closep(&fmt_ctx_->pb);
            }
            avformat_free_context(fmt_ctx_);
            fmt_ctx_ = nullptr;
        }

        video_stream_ = nullptr;
        audio_stream_ = nullptr;
    }

private:
    bool OpenVideoStream() {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
        if (!codec) {
            std::cerr << "[Recorder] MPEG4 encoder not found\n";
            return false;
        }

        video_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
        if (!video_stream_) {
            std::cerr << "[Recorder] avformat_new_stream(video) failed\n";
            return false;
        }

        video_enc_ctx_ = avcodec_alloc_context3(codec);
        if (!video_enc_ctx_) return false;

        video_enc_ctx_->codec_id = AV_CODEC_ID_MPEG4;
        video_enc_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
        video_enc_ctx_->bit_rate = 2'000'000;
        video_enc_ctx_->width = video_width_;
        video_enc_ctx_->height = video_height_;
        video_enc_ctx_->time_base = AVRational{ 1, video_fps_ };
        video_enc_ctx_->framerate = AVRational{ video_fps_, 1 };
        video_enc_ctx_->gop_size = 12;
        video_enc_ctx_->max_b_frames = 0;
        video_enc_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

        if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
            video_enc_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        int ret = avcodec_open2(video_enc_ctx_, codec, nullptr);
        if (ret < 0) {
            PrintFfmpegErr("avcodec_open2(video)", ret);
            return false;
        }

        ret = avcodec_parameters_from_context(video_stream_->codecpar, video_enc_ctx_);
        if (ret < 0) {
            PrintFfmpegErr("avcodec_parameters_from_context(video)", ret);
            return false;
        }

        video_stream_->time_base = video_enc_ctx_->time_base;
        return true;
    }

    bool OpenAudioStream() {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!codec) {
            std::cerr << "[Recorder] AAC encoder not found\n";
            return false;
        }

        audio_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
        if (!audio_stream_) {
            std::cerr << "[Recorder] avformat_new_stream(audio) failed\n";
            return false;
        }

        audio_enc_ctx_ = avcodec_alloc_context3(codec);
        if (!audio_enc_ctx_) return false;

        audio_enc_ctx_->codec_id = AV_CODEC_ID_AAC;
        audio_enc_ctx_->codec_type = AVMEDIA_TYPE_AUDIO;
        audio_enc_ctx_->sample_rate = audio_sample_rate_;
        audio_enc_ctx_->bit_rate = 128000;
        audio_enc_ctx_->time_base = AVRational{ 1, audio_sample_rate_ };

        if (codec->sample_fmts) {
            audio_enc_ctx_->sample_fmt = codec->sample_fmts[0];
            for (const enum AVSampleFormat* p = codec->sample_fmts; *p != AV_SAMPLE_FMT_NONE; ++p) {
                if (*p == AV_SAMPLE_FMT_FLTP) {
                    audio_enc_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
                    break;
                }
            }
        }
        else {
            audio_enc_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
        }

        av_channel_layout_default(&audio_enc_ctx_->ch_layout, 2);

        if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
            audio_enc_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        int ret = avcodec_open2(audio_enc_ctx_, codec, nullptr);
        if (ret < 0) {
            PrintFfmpegErr("avcodec_open2(audio)", ret);
            return false;
        }

        ret = avcodec_parameters_from_context(audio_stream_->codecpar, audio_enc_ctx_);
        if (ret < 0) {
            PrintFfmpegErr("avcodec_parameters_from_context(audio)", ret);
            return false;
        }

        audio_stream_->time_base = audio_enc_ctx_->time_base;
        audio_frame_size_ = audio_enc_ctx_->frame_size > 0 ? audio_enc_ctx_->frame_size : 1024;

        audio_fifo_ = av_audio_fifo_alloc(
            audio_enc_ctx_->sample_fmt,
            audio_enc_ctx_->ch_layout.nb_channels,
            audio_frame_size_ * 8);

        if (!audio_fifo_) {
            std::cerr << "[Recorder] av_audio_fifo_alloc failed\n";
            return false;
        }

        return true;
    }

    bool EnsureAudioResampler(int in_sample_rate, int in_channels) {
        if (in_sample_rate <= 0 || in_channels <= 0) return false;

        if (swr_ctx_ && in_sample_rate == input_sample_rate_ && in_channels == input_channels_) {
            return true;
        }

        if (swr_ctx_) {
            swr_free(&swr_ctx_);
            swr_ctx_ = nullptr;
        }

        input_sample_rate_ = in_sample_rate;
        input_channels_ = in_channels;

        AVChannelLayout in_ch_layout;
        av_channel_layout_default(&in_ch_layout, in_channels);

        int ret = swr_alloc_set_opts2(
            &swr_ctx_,
            &audio_enc_ctx_->ch_layout,
            audio_enc_ctx_->sample_fmt,
            audio_enc_ctx_->sample_rate,
            &in_ch_layout,
            AV_SAMPLE_FMT_S16,
            in_sample_rate,
            0,
            nullptr);

        av_channel_layout_uninit(&in_ch_layout);

        if (ret < 0) {
            PrintFfmpegErr("swr_alloc_set_opts2", ret);
            return false;
        }

        ret = swr_init(swr_ctx_);
        if (ret < 0) {
            PrintFfmpegErr("swr_init", ret);
            swr_free(&swr_ctx_);
            return false;
        }

        return true;
    }

    bool EnsureAudioFifoCapacity(int required_samples) {
        if (!audio_fifo_) return false;

        const int current_size = av_audio_fifo_size(audio_fifo_);
        const int current_space = av_audio_fifo_space(audio_fifo_);
        if (current_space >= required_samples - current_size) {
            return true;
        }

        const int new_size = required_samples + audio_frame_size_ * 4;
        int ret = av_audio_fifo_realloc(audio_fifo_, new_size);
        if (ret < 0) {
            PrintFfmpegErr("av_audio_fifo_realloc", ret);
            return false;
        }
        return true;
    }

    void FlushAudioResampler() {
        if (!swr_ctx_ || !audio_fifo_) return;

        while (true) {
            const int out_cap = audio_frame_size_ * 4;
            AVFrame* converted = av_frame_alloc();
            if (!converted) return;

            converted->nb_samples = out_cap;
            converted->format = audio_enc_ctx_->sample_fmt;
            converted->sample_rate = audio_enc_ctx_->sample_rate;
            converted->ch_layout = audio_enc_ctx_->ch_layout;

            int ret = av_frame_get_buffer(converted, 0);
            if (ret < 0) {
                av_frame_free(&converted);
                return;
            }

            ret = swr_convert(
                swr_ctx_,
                converted->extended_data,
                out_cap,
                nullptr,
                0);

            if (ret < 0) {
                PrintFfmpegErr("swr_convert(flush)", ret);
                av_frame_free(&converted);
                return;
            }

            if (ret == 0) {
                av_frame_free(&converted);
                break;
            }

            converted->nb_samples = ret;

            if (!EnsureAudioFifoCapacity(av_audio_fifo_size(audio_fifo_) + converted->nb_samples)) {
                av_frame_free(&converted);
                return;
            }

            ret = av_audio_fifo_write(audio_fifo_, reinterpret_cast<void**>(converted->extended_data), converted->nb_samples);
            av_frame_free(&converted);

            if (ret < 0) {
                std::cerr << "[Recorder] av_audio_fifo_write(flush) failed\n";
                return;
            }
        }
    }

    void DrainAudioFifo(bool final_flush) {
        if (!audio_fifo_ || !audio_enc_ctx_) return;

        while (av_audio_fifo_size(audio_fifo_) >= audio_frame_size_) {
            WriteOneAudioFrame(audio_frame_size_);
        }

        if (final_flush && av_audio_fifo_size(audio_fifo_) > 0) {
            WriteOneAudioFrame(av_audio_fifo_size(audio_fifo_));
        }
    }

    void WriteOneAudioFrame(int nb_samples) {
        AVFrame* frame = av_frame_alloc();
        if (!frame) return;

        frame->nb_samples = nb_samples;
        frame->format = audio_enc_ctx_->sample_fmt;
        frame->sample_rate = audio_enc_ctx_->sample_rate;
        frame->ch_layout = audio_enc_ctx_->ch_layout;
        frame->pts = audio_pts_;

        int ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            av_frame_free(&frame);
            return;
        }

        ret = av_audio_fifo_read(audio_fifo_, reinterpret_cast<void**>(frame->extended_data), nb_samples);
        if (ret < nb_samples) {
            av_frame_free(&frame);
            return;
        }

        audio_pts_ += nb_samples;

        EncodeAndWrite(audio_enc_ctx_, audio_stream_, frame);
        av_frame_free(&frame);
    }

    bool EncodeAndWrite(AVCodecContext* enc_ctx, AVStream* st, AVFrame* frame) {
        int ret = avcodec_send_frame(enc_ctx, frame);
        if (ret < 0) {
            PrintFfmpegErr("avcodec_send_frame", ret);
            return false;
        }

        while (true) {
            AVPacket* pkt = av_packet_alloc();
            if (!pkt) return false;

            ret = avcodec_receive_packet(enc_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_free(&pkt);
                return true;
            }
            if (ret < 0) {
                PrintFfmpegErr("avcodec_receive_packet", ret);
                av_packet_free(&pkt);
                return false;
            }

            pkt->stream_index = st->index;
            av_packet_rescale_ts(pkt, enc_ctx->time_base, st->time_base);

            ret = av_interleaved_write_frame(fmt_ctx_, pkt);
            av_packet_free(&pkt);
            if (ret < 0) {
                PrintFfmpegErr("av_interleaved_write_frame", ret);
                return false;
            }
        }
    }

private:
    std::mutex mutex_;

    std::string output_path_;

    AVFormatContext* fmt_ctx_ = nullptr;

    AVStream* video_stream_ = nullptr;
    AVCodecContext* video_enc_ctx_ = nullptr;
    int video_width_ = 0;
    int video_height_ = 0;
    int video_fps_ = 0;
    int64_t video_pts_ = 0;
    SwsContext* sws_ctx_ = nullptr;

    AVStream* audio_stream_ = nullptr;
    AVCodecContext* audio_enc_ctx_ = nullptr;
    SwrContext* swr_ctx_ = nullptr;
    AVAudioFifo* audio_fifo_ = nullptr;
    int audio_sample_rate_ = 48000;
    int audio_frame_size_ = 1024;
    int input_sample_rate_ = 0;
    int input_channels_ = 0;
    int64_t audio_pts_ = 0;
};

class WebRtcCaptureSink final : public rtc::VideoSinkInterface<webrtc::VideoFrame>,
    public webrtc::AudioTransport {
public:
    explicit WebRtcCaptureSink(Recorder* recorder) : recorder_(recorder) {}

    void OnFrame(const webrtc::VideoFrame& frame) override {
        if (recorder_) {
            recorder_->OnVideoFrame(frame);
        }
    }

    // Compatibility with the current AudioTransport form that includes optional
    // capture timestamp.
    int32_t RecordedDataIsAvailable(
        const void* audioSamples,
        size_t nSamples,
        size_t nBytesPerSample,
        size_t nChannels,
        uint32_t samplesPerSec,
        uint32_t totalDelayMS,
        int32_t clockDrift,
        uint32_t currentMicLevel,
        bool keyPressed,
        uint32_t& newMicLevel) override
    {
        newMicLevel = 0;

        if (recorder_) {
            recorder_->OnAudioSamples(
                audioSamples,
                nSamples,
                nBytesPerSample,
                nChannels,
                samplesPerSec);
        }
        return 0;
    }

    int32_t NeedMorePlayData(const size_t nSamples,
        const size_t nBytesPerSample,
        const size_t nChannels,
        const uint32_t samplesPerSec,
        void* audioSamples,
        size_t& nSamplesOut,
        int64_t* elapsed_time_ms,
        int64_t* ntp_time_ms) override {
        (void)nSamples;
        (void)nBytesPerSample;
        (void)nChannels;
        (void)samplesPerSec;

        if (audioSamples && nSamples > 0) {
            std::memset(audioSamples, 0, nSamples * nChannels * nBytesPerSample);
        }
        nSamplesOut = nSamples;
        if (elapsed_time_ms) *elapsed_time_ms = -1;
        if (ntp_time_ms) *ntp_time_ms = -1;
        return 0;
    }

    void PullRenderData(int bits_per_sample,
        int sample_rate,
        size_t number_of_channels,
        size_t number_of_frames,
        void* audio_data,
        int64_t* elapsed_time_ms,
        int64_t* ntp_time_ms) override {
        (void)bits_per_sample;
        (void)sample_rate;
        (void)number_of_channels;
        (void)number_of_frames;
        (void)audio_data;
        if (elapsed_time_ms) *elapsed_time_ms = -1;
        if (ntp_time_ms) *ntp_time_ms = -1;
    }

private:
    Recorder* recorder_ = nullptr;
};

static void PrintVideoDevices(webrtc::VideoCaptureModule::DeviceInfo* info) {
    const uint32_t count = info->NumberOfDevices();
    std::cout << "\n[Video devices] count = " << count << "\n";

    for (uint32_t i = 0; i < count; ++i) {
        char name[256] = { 0 };
        char unique[256] = { 0 };
        if (info->GetDeviceName(i, name, sizeof(name), unique, sizeof(unique)) == 0) {
            std::cout << "  [" << i << "] " << name << " | " << unique << "\n";
        }
    }
}

static void PrintAudioDevices(webrtc::AudioDeviceModule* adm) {
    const int16_t rec_count = adm->RecordingDevices();
    const int16_t play_count = adm->PlayoutDevices();

    std::cout << "\n[Audio recording devices] count = " << rec_count << "\n";
    for (int i = 0; i < rec_count; ++i) {
        char name[webrtc::kAdmMaxDeviceNameSize] = { 0 };
        char guid[webrtc::kAdmMaxGuidSize] = { 0 };
        if (adm->RecordingDeviceName(static_cast<uint16_t>(i), name, guid) == 0) {
            std::cout << "  [" << i << "] " << name << " | " << guid << "\n";
        }
    }

    std::cout << "\n[Audio playout devices] count = " << play_count << "\n";
    for (int i = 0; i < play_count; ++i) {
        char name[webrtc::kAdmMaxDeviceNameSize] = { 0 };
        char guid[webrtc::kAdmMaxGuidSize] = { 0 };
        if (adm->PlayoutDeviceName(static_cast<uint16_t>(i), name, guid) == 0) {
            std::cout << "  [" << i << "] " << name << " | " << guid << "\n";
        }
    }
}

static int PickClosestCapabilityIndex(webrtc::VideoCaptureModule::DeviceInfo* info,
    const char* unique_name,
    int req_w,
    int req_h,
    int req_fps) {
    const int caps = info->NumberOfCapabilities(unique_name);
    if (caps <= 0) return 0;

    int best_index = 0;
    int best_score = INT32_MAX;

    for (int i = 0; i < caps; ++i) {
        webrtc::VideoCaptureCapability cap;
        std::memset(&cap, 0, sizeof(cap));
        if (info->GetCapability(unique_name, static_cast<uint32_t>(i), cap) != 0) {
            continue;
        }

        const int score =
            std::abs(cap.width - req_w) +
            std::abs(cap.height - req_h) +
            std::abs(cap.maxFPS - req_fps) * 2;

        if (score < best_score) {
            best_score = score;
            best_index = i;
        }
    }

    return best_index;
}

int main() {
    // Fixed configuration in code.
    const std::string output_file = "webrtc_capture.mp4";
    const int cam_index = 0;
    const int mic_index = 0;
    const int req_w = 1280;
    const int req_h = 720;
    const int req_fps = 30;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)hr;

    av_log_set_level(AV_LOG_ERROR);

    // 1) Enumerate video devices
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> video_info(
        webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!video_info) {
        std::cerr << "CreateDeviceInfo failed.\n";
        CoUninitialize();
        return -1;
    }

    PrintVideoDevices(video_info.get());

    const uint32_t video_count = video_info->NumberOfDevices();
    if (video_count == 0) {
        std::cerr << "No camera found.\n";
        CoUninitialize();
        return -1;
    }

    int fixed_cam = cam_index;
    if (fixed_cam < 0 || fixed_cam >= static_cast<int>(video_count)) {
        std::cerr << "Camera index invalid, fallback to 0.\n";
        fixed_cam = 0;
    }

    char cam_name[256] = { 0 };
    char cam_unique[256] = { 0 };
    if (video_info->GetDeviceName(static_cast<uint32_t>(fixed_cam),
        cam_name, sizeof(cam_name),
        cam_unique, sizeof(cam_unique)) != 0) {
        std::cerr << "GetDeviceName(camera) failed.\n";
        CoUninitialize();
        return -1;
    }

    const int cap_index = PickClosestCapabilityIndex(video_info.get(), cam_unique, req_w, req_h, req_fps);

    webrtc::VideoCaptureCapability requested_capability;
    std::memset(&requested_capability, 0, sizeof(requested_capability));
    requested_capability.width = req_w;
    requested_capability.height = req_h;
    requested_capability.maxFPS = req_fps;
    requested_capability.videoType = webrtc::VideoType::kI420;

    webrtc::VideoCaptureCapability capability;
    std::memset(&capability, 0, sizeof(capability));

    if (video_info->GetCapability(cam_unique, static_cast<uint32_t>(cap_index), capability) != 0) {
        std::cerr << "GetCapability(camera) failed.\n";
        CoUninitialize();
        return -1;
    }

    capability.width = req_w;
    capability.height = req_h;
    capability.maxFPS = req_fps;
    capability.videoType = webrtc::VideoType::kI420;

    std::cout << "\nSelected camera: [" << fixed_cam << "] " << cam_name
        << " | " << cam_unique << "\n";
    std::cout << "Selected capability: " << capability.width << "x"
        << capability.height << " @ " << capability.maxFPS << " fps\n";

    // 2) Create video capture module
    rtc::scoped_refptr<webrtc::VideoCaptureModule> video_module(
        webrtc::VideoCaptureFactory::Create(cam_unique));
    if (!video_module) {
        std::cerr << "VideoCaptureFactory::Create failed.\n";
        CoUninitialize();
        return -1;
    }

    // 3) Create audio device module (TaskQueueFactory is required; nullptr crashes inside ADM)
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory =
        webrtc::CreateDefaultTaskQueueFactory();
    if (!task_queue_factory) {
        std::cerr << "CreateDefaultTaskQueueFactory failed.\n";
        CoUninitialize();
        return -1;
    }

    rtc::scoped_refptr<webrtc::AudioDeviceModule> audio_module(
        webrtc::AudioDeviceModule::Create(
            webrtc::AudioDeviceModule::kPlatformDefaultAudio,
            task_queue_factory.get()));
    if (!audio_module) {
        std::cerr << "AudioDeviceModule::Create failed.\n";
        CoUninitialize();
        return -1;
    }

    // ADM must be Init()'d before RecordingDevices/PlayoutDevices/Set*Device;
    // otherwise WebRTC returns -1 (see AudioDeviceModuleImpl::CHECKinitialized_).
    if (audio_module->Init() != 0) {
        std::cerr << "AudioDeviceModule::Init failed (needed before device enumeration).\n";
        CoUninitialize();
        return -1;
    }

    PrintAudioDevices(audio_module.get());

    const int16_t rec_count = audio_module->RecordingDevices();
    if (rec_count <= 0) {
        std::cerr << "No microphone found.\n";
        CoUninitialize();
        return -1;
    }

    int fixed_mic = mic_index;
    if (fixed_mic < 0 || fixed_mic >= rec_count) {
        std::cerr << "Microphone index invalid, fallback to 0.\n";
        fixed_mic = 0;
    }

    char mic_name[webrtc::kAdmMaxDeviceNameSize] = { 0 };
    char mic_guid[webrtc::kAdmMaxGuidSize] = { 0 };
    if (audio_module->RecordingDeviceName(static_cast<uint16_t>(fixed_mic), mic_name, mic_guid) != 0) {
        std::cerr << "RecordingDeviceName failed.\n";
        CoUninitialize();
        return -1;
    }

    std::cout << "\nSelected microphone: [" << fixed_mic << "] " << mic_name
        << " | " << mic_guid << "\n";

    // 4) Initialize recorder
    Recorder recorder;
    if (!recorder.Init(output_file, capability.width, capability.height, capability.maxFPS)) {
        std::cerr << "Recorder init failed.\n";
        CoUninitialize();
        return -1;
    }

    // 5) Hook callbacks
    WebRtcCaptureSink sink(&recorder);
    video_module->RegisterCaptureDataCallback(&sink);
    audio_module->RegisterAudioCallback(&sink);

    // 6) Select devices and start capture
    if (audio_module->SetRecordingDevice(static_cast<uint16_t>(fixed_mic)) != 0) {
        std::cerr << "SetRecordingDevice failed.\n";
        goto cleanup;
    }

    if (audio_module->InitRecording() != 0) {
        std::cerr << "InitRecording failed.\n";
        goto cleanup;
    }

    if (audio_module->StartRecording() != 0) {
        std::cerr << "StartRecording failed.\n";
        goto cleanup;
    }

    if (video_module->StartCapture(capability) != 0) {
        std::cerr << "StartCapture failed.\n";
        goto cleanup;
    }

    std::cout << "\nRecording started.\n";
    std::cout << "Press ENTER to stop...\n";
    std::cin.get();

cleanup:
    // 7) Stop and flush
    video_module->StopCapture();
    video_module->DeRegisterCaptureDataCallback();

    audio_module->StopRecording();
    audio_module->RegisterAudioCallback(nullptr);
    audio_module->Terminate();

    recorder.Finalize();

    CoUninitialize();
    std::cout << "Done.\n";
    return 0;
}
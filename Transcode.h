#pragma once
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <string>
#include <vector>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
#include "SDL.h"
#include "SDL_thread.h"
};
using namespace std;
class Transcode {
public:
    Transcode(string,string);

    string srcPath;
    string dstPath;
    AVCodecID dst_video_id;
    int dst_width;
    int dst_height;
    int dst_video_bit_rate;
    AVPixelFormat dst_pix_fmt;
    int dst_fps;
    bool video_need_transcode;

    // “Ù∆µ
    AVCodecID dst_audio_id;
    int dst_audio_bit_rate;
    int dst_sample_rate;
    AVSampleFormat dst_sample_fmt;
    int dst_channel_layout;
    bool audio_need_transcode;

    int video_in_stream_index;
    int audio_in_stream_index;
    AVFormatContext* ouFmtCtx;
    AVFormatContext* inFmtCtx;
    AVCodecID src_video_id;
    AVCodecID src_audio_id;
    int video_ou_stream_index;
    AVCodecContext* video_en_ctx;
    bool video_need_convert;
    int audio_ou_stream_index;
    AVCodecContext* audio_en_ctx;
    bool audio_need_convert;
    SwrContext* swr_ctx;
    AVCodecContext* video_de_ctx;
    AVFrame* video_de_frame;
    AVFrame* video_en_frame;
    SwsContext* sws_ctx;
    int64_t video_pts;
    int64_t audio_pts;
    int64_t last_video_pts;
    int64_t last_audio_pts;
    AVCodecContext* audio_de_ctx;
    AVFrame* audio_de_frame;
    AVFrame* audio_en_frame;
    vector<AVPacket*> videoCache;
    vector<AVPacket*> audioCache;



    void doTranscode();
    bool openFile();
    bool add_video_stream();
    bool add_audio_stream();
    void doDecodeVideo(AVPacket* inpacket);
    void doEncodeVideo(AVFrame* frame);
    void doDecodeAudio(AVPacket* packet);
    void doEncodeAudio(AVFrame* frame);
    void doWrite(AVPacket* packet, bool isVideo);
    AVFrame* get_audio_frame(enum AVSampleFormat smpfmt, int64_t ch_layout, int sample_rate, int nb_samples);
    AVFrame* get_video_frame(enum AVPixelFormat pixfmt, int width, int height);
    void releaseSources();
};

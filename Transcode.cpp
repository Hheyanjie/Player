#include "pch.h"
#include "Transcode.h"

#define LOGD(x) printf(x)

Transcode::Transcode(string srcPath, string dstPath):srcPath(srcPath), dstPath(dstPath)
{
    ouFmtCtx = NULL;
    inFmtCtx = NULL;
    video_en_ctx = NULL;
    audio_en_ctx = NULL;
    video_de_ctx = NULL;
    video_de_frame = NULL;
    video_en_frame = NULL;
    sws_ctx = NULL;
    audio_de_ctx = NULL;
    audio_de_frame = NULL;
    audio_en_frame = NULL;
}
/** ��ת��(�������뷽ʽ�����ʣ��ֱ��ʣ������ʣ�������ʽ�ȵȵ�ת��)
 */
void Transcode::doTranscode()
{
    //string curFile(__FILE__);
    //unsigned long pos = curFile.find("2-video_audio_advanced");
    //if (pos == string::npos) {
    //    LOGD("cannot find file");
    //    return;
    //}
    int ret = 0;
    //string srcDic = curFile.substr(0, pos) + "filesources/";
    // �����뷽ʽ��ת�룬Ŀ���װ��ʽҪ֧�� ��һ�����������Ƿ�����Դ�ļ��Ĳ��������Ϊtrue����Եڶ�������������ʹ�õڶ�������
    // ��Ƶ
    dst_video_id = AV_CODEC_ID_MPEG4;
    dst_width = 1280;
    dst_height = 720;
    dst_video_bit_rate = 9000000;       // 0.9Mb/s
    dst_pix_fmt = AV_PIX_FMT_YUV420P;
    dst_fps = 30;  //fps
    video_need_transcode = true;

    // ��Ƶ
    dst_audio_id = AV_CODEC_ID_MP3;
    dst_audio_bit_rate = 128 * 1000;
    dst_sample_rate = 44100;    // 44.1khz
    dst_sample_fmt = AV_SAMPLE_FMT_FLTP;
    dst_channel_layout = AV_CH_LAYOUT_STEREO;   // ˫����
    audio_need_transcode = true;

    // �������ļ�������ļ���������
    if (!openFile()) {
        LOGD("openFile()");
        return;
    }

    // Ϊ����ļ������Ƶ��
    if (video_in_stream_index != -1 && video_need_transcode) {
        if (!add_video_stream()) {
            LOGD("add_video_stream fai");
            return;
        }
    }

    // Ϊ����ļ������Ƶ��
    if (audio_in_stream_index != -1 && audio_need_transcode) {
        if (!add_audio_stream()) {
            LOGD("add_audio_stream()");
            return;
        }
    }

    av_dump_format(ouFmtCtx, 0, dstPath.c_str(), 1);

    // �򿪽��װ��������
    if (!(ouFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ouFmtCtx->pb, dstPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            LOGD("avio_open fail");
            releaseSources();
            return;
        }
    }


    // д��ͷ��Ϣ
    /** �������⣺avformat_write_header()����
     *  ����ԭ��û�е���avio_open()����
     *  ���������д�� !(ouFmtCtx->flags & AVFMT_NOFILE)����
     */
    if ((ret = avformat_write_header(ouFmtCtx, NULL)) < 0) {
        LOGD("avformat_write_header fail %d", ret);
        releaseSources();
        return;
    }

    // ��ȡԴ�ļ��е�����Ƶ���ݽ��н���
    AVPacket* inPacket = av_packet_alloc();
    while (av_read_frame(inFmtCtx, inPacket) == 0) {
        // ˵����ȡ����Ƶ����
        if (inPacket->stream_index == video_in_stream_index && video_need_transcode) {
            doDecodeVideo(inPacket);
        }

        // ˵����ȡ����Ƶ����
        if (inPacket->stream_index == audio_in_stream_index && audio_need_transcode) {
            doDecodeAudio(inPacket);
        }

        // ��Ϊÿһ�ζ�ȡ��AVpacket�����ݴ�С��һ������������֮��Ҫ�ͷ�
        av_packet_unref(inPacket);
    }
    LOGD("�ļ���ȡ���");

    // ˢ�½��뻺��������ȡ������������
    if (video_in_stream_index != -1 && video_need_transcode) {
        doDecodeVideo(NULL);
    }
    if (audio_in_stream_index != -1 && audio_need_transcode) {
        doDecodeAudio(NULL);
    }

    // д��β����Ϣ
    if (ouFmtCtx) {
        av_write_trailer(ouFmtCtx);
    }
    LOGD("����");

    // �ͷ���Դ
    releaseSources();
}

bool Transcode::openFile()
{
    avformat_network_init();
    if (!srcPath.length()) {
        return false;
    }
    int ret = 0;
    // �������ļ�
    if ((ret = avformat_open_input(&inFmtCtx, srcPath.c_str(), NULL, NULL)) < 0) {
        LOGD("avformat_open_input() fail");
        releaseSources();
        return false;
    }
    if ((ret = avformat_find_stream_info(inFmtCtx, NULL)) < 0) {
        LOGD("avformat_find_stream_info fail %d", ret);
        releaseSources();
        return false;
    }
    // ��������ļ���Ϣ
    av_dump_format(inFmtCtx, 0, srcPath.c_str(), 0);
    video_in_stream_index = -1;
    audio_in_stream_index = -1;
    for (int i = 0; i < inFmtCtx->nb_streams; i++) {
        AVCodecParameters* codecpar = inFmtCtx->streams[i]->codecpar;
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_in_stream_index == -1) {
            src_video_id = codecpar->codec_id;
            video_in_stream_index = i;
        }
        if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_in_stream_index == -1) {
            src_audio_id = codecpar->codec_id;
            audio_in_stream_index = i;
        }
    }

    // �������
    if ((ret = avformat_alloc_output_context2(&ouFmtCtx, NULL, NULL, dstPath.c_str())) < 0) {
        LOGD("avformat_alloc_context fail");
        releaseSources();
        return false;
    }
    // ���Ŀ����뷽ʽ�Ƿ�֧��
    if (video_in_stream_index != -1 && av_codec_get_tag(ouFmtCtx->oformat->codec_tag, dst_video_id) == 0) {
        LOGD("video tag not found");
        releaseSources();
        return false;
    }
    if (audio_in_stream_index != -1 && av_codec_get_tag(ouFmtCtx->oformat->codec_tag, dst_audio_id) == 0) {
        LOGD("audio tag not found");
        releaseSources();
        return false;
    }

    return true;
}

static int select_sample_rate(const AVCodec* codec, int rate)
{
    int best_rate = 0;
    int deft_rate = 44100;
    bool surport = false;
    const int* p = codec->supported_samplerates;
    while (*p) {
        best_rate = *p;
        if (*p == rate) {
            surport = true;
            break;
        }
        p++;
    }

    if (best_rate != rate && best_rate != 0 && best_rate != deft_rate) {
        return deft_rate;
    }

    return best_rate;
}

static enum AVSampleFormat select_sample_format(const AVCodec* codec, enum AVSampleFormat fmt)
{
    enum AVSampleFormat retfmt = AV_SAMPLE_FMT_NONE;
    enum AVSampleFormat deffmt = AV_SAMPLE_FMT_FLTP;
    const enum AVSampleFormat* fmts = codec->sample_fmts;
    while (*fmts != AV_SAMPLE_FMT_NONE) {
        retfmt = *fmts;
        if (retfmt == fmt) {
            break;
        }
        fmts++;
    }

    if (retfmt != fmt && retfmt != AV_SAMPLE_FMT_NONE && retfmt != deffmt) {
        return deffmt;
    }

    return retfmt;
}

static int64_t select_channel_layout(const AVCodec* codec, int64_t ch_layout)
{
    int64_t retch = 0;
    int64_t defch = AV_CH_LAYOUT_STEREO;
    if (!codec->channel_layouts)
        return defch;
    const uint64_t* chs = codec->channel_layouts;
    while (*chs) {
        retch = *chs;
        if (retch == ch_layout) {
            break;
        }
        chs++;
    }

    if (retch != ch_layout && retch != AV_SAMPLE_FMT_NONE && retch != defch) {
        return defch;
    }

    return retch;
}

static enum AVPixelFormat select_pixel_format(const AVCodec* codec, enum AVPixelFormat fmt) {
    enum AVPixelFormat retpixfmt = AV_PIX_FMT_NONE;
    enum AVPixelFormat defaltfmt = AV_PIX_FMT_YUV420P;
    const enum AVPixelFormat* fmts = codec->pix_fmts;
    while (*fmts != AV_PIX_FMT_NONE) {
        retpixfmt = *fmts;
        if (retpixfmt == fmt) {
            break;
        }
        fmts++;
    }

    if (retpixfmt != fmt && retpixfmt != AV_PIX_FMT_NONE && retpixfmt != defaltfmt) {
        return defaltfmt;
    }

    return retpixfmt;
}

bool Transcode::add_video_stream()
{
    const AVCodec* codec = avcodec_find_encoder(dst_video_id);
    if (codec == NULL) {
        LOGD("video code find fail");
        releaseSources();
        return false;
    }

    AVStream* stream = avformat_new_stream(ouFmtCtx, NULL);
    if (!stream) {
        LOGD("avformat_new_stream fail");
        return false;
    }
    video_ou_stream_index = stream->index;

    video_en_ctx = avcodec_alloc_context3(codec);
    if (video_en_ctx == NULL) {
        releaseSources();
        LOGD("video codec not found");
        return false;
    }

    AVCodecParameters* inpar = inFmtCtx->streams[video_in_stream_index]->codecpar;
    // ���ñ�����ز���
    video_en_ctx->codec_id = codec->id;
    // ��������
    video_en_ctx->bit_rate = inpar->bit_rate;
    // ������Ƶ��
    video_en_ctx->width = inpar->width;
    //video_en_ctx->width = 1920;
    // ������Ƶ��
    video_en_ctx->height = inpar->height;
    //video_en_ctx->height = 1080;
    // ����֡��
    int fps = inFmtCtx->streams[video_in_stream_index]->r_frame_rate.num;
    video_en_ctx->framerate = { fps,1 };
    // ����ʱ���;
    stream->time_base = { 1,video_en_ctx->framerate.num };
    video_en_ctx->time_base = stream->time_base;
    // I֡�����������ѹ����
    video_en_ctx->gop_size = 12;
    // ������Ƶ���ظ�ʽ
    enum AVPixelFormat want_pix_fmt = (AVPixelFormat)inpar->format;
    enum AVPixelFormat result_pix_fmt = select_pixel_format(codec, want_pix_fmt);
    if (result_pix_fmt == AV_PIX_FMT_NONE) {
        LOGD("can not surport pix fmt");
        releaseSources();
        return false;
    }
    video_en_ctx->pix_fmt = result_pix_fmt;
    // ÿ��I֡��B֡��������
    if (codec->id == AV_CODEC_ID_MPEG2VIDEO) {
        video_en_ctx->max_b_frames = 2;
    }
    /* Needed to avoid using macroblocks in which some coeffs overflow.
    * This does not happen with normal video, it just happens here as
    * the motion of the chroma plane does not match the luma plane. */
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO) {
        video_en_ctx->mb_decision = 2;
    }

    // �ж��Ƿ���Ҫ���и�ʽת��
    if (result_pix_fmt != (enum AVPixelFormat)inpar->format || video_en_ctx->width != inpar->width || video_en_ctx->height != inpar->height) {
        video_need_convert = true;
    }

    // ��ӦһЩ��װ����Ҫ���������
    /** �������⣺���ɵ�mp4����mov�ļ�û����ʾ����Ԥ����СͼƬ
     *  ����ԭ��֮ǰ����������flags������õ�ΪAVFMT_GLOBALHEADER(����)����ȷ��ֵӦ����AV_CODEC_FLAG_GLOBAL_HEADER
     *  ���˼·��ʹ�����´�������AV_CODEC_FLAG_GLOBAL_HEADER
     */
    if (ouFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        //        video_en_ctx->flags |= AVFMT_GLOBALHEADER;
        video_en_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // x264�������в���
    if (video_en_ctx->codec_id == AV_CODEC_ID_H264) {
        av_opt_set(video_en_ctx->priv_data, "preset", "slow", 0);
        video_en_ctx->flags |= AV_CODEC_FLAG2_LOCAL_HEADER;
    }

    int ret = 0;
    if ((ret = avcodec_open2(video_en_ctx, video_en_ctx->codec, NULL)) < 0) {
        LOGD("video encode open2() fail %d", ret);
        releaseSources();
        return false;
    }

    if ((ret = avcodec_parameters_from_context(stream->codecpar, video_en_ctx)) < 0) {
        releaseSources();
        LOGD("video avcodec_parameters_from_context fail");
        return false;
    }

    return true;
}

bool Transcode::add_audio_stream()
{
    if (ouFmtCtx == NULL) {
        LOGD("audio outformat NULL");
        releaseSources();
        return false;
    }

    // ���һ����Ƶ��
    AVStream* stream = avformat_new_stream(ouFmtCtx, NULL);
    if (!stream) {
        LOGD("avformat_new_stream fail");
        return false;
    }
    audio_ou_stream_index = stream->index;

    AVCodecParameters* incodecpar = inFmtCtx->streams[audio_in_stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_encoder(dst_audio_id);
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (ctx == NULL) {
        LOGD("audio codec ctx NULL");
        releaseSources();
        return false;
    }
    // ������Ƶ�������
    // ������
    int want_sample_rate = incodecpar->sample_rate;
    int relt_sample_rate = select_sample_rate(codec, want_sample_rate);
    if (relt_sample_rate == 0) {
        LOGD("cannot surpot sample_rate");
        releaseSources();
        return false;
    }
    ctx->sample_rate = relt_sample_rate;
    // ������ʽ
    enum AVSampleFormat want_sample_fmt = (enum AVSampleFormat)incodecpar->format;
    enum AVSampleFormat relt_sample_fmt = select_sample_format(codec, want_sample_fmt);
    if (relt_sample_fmt == AV_SAMPLE_FMT_NONE) {
        LOGD("cannot surpot sample_fmt");
        releaseSources();
        return false;
    }
    ctx->sample_fmt = relt_sample_fmt;
    // ������ʽ
    int64_t want_ch = incodecpar->channel_layout;
    int64_t relt_ch = select_channel_layout(codec, want_ch);
    if (!relt_ch) {
        LOGD("cannot surpot channel_layout");
        releaseSources();
        return false;
    }
    ctx->channel_layout = relt_ch;
    // ������
    ctx->channels = av_get_channel_layout_nb_channels(ctx->channel_layout);
    // ����������
    ctx->bit_rate = incodecpar->bit_rate;
    // frame_size ���ﲻ�����ã�����avcodec_open2()���������ݱ��������Զ�����
//    ctx->frame_size = incodecpar->frame_size;
    // ����ʱ���
    ctx->time_base = { 1,ctx->sample_rate };
    stream->time_base = ctx->time_base;

    // ����
    audio_en_ctx = ctx;

    // ���ñ������ر�ǣ��������з�װ��ʱ�򲻻�©��һЩԪ��Ϣ
    if (ouFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        audio_en_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // ��ʼ��������
    if (avcodec_open2(ctx, codec, NULL) < 0) {
        LOGD("audio avcodec_open2() fail");
        releaseSources();
        return false;
    }

    if (audio_en_ctx->frame_size != incodecpar->frame_size || relt_sample_fmt != incodecpar->format || relt_ch != incodecpar->channel_layout || relt_sample_rate != incodecpar->sample_rate) {
        audio_need_convert = true;
    }

    int ret = 0;
    // ��������Ϣ���õ���Ƶ����
    if ((ret = avcodec_parameters_from_context(stream->codecpar, ctx)) < 0) {
        LOGD("audio copy audio stream fail");
        releaseSources();
        return false;
    }

    // ��ʼ���ز���
    if (audio_need_convert) {
        swr_ctx = swr_alloc_set_opts(NULL, ctx->channel_layout, ctx->sample_fmt, ctx->sample_rate, incodecpar->channel_layout, (enum AVSampleFormat)incodecpar->format, incodecpar->sample_rate, 0, NULL);
        if ((ret = swr_init(swr_ctx)) < 0) {
            LOGD("swr_alloc_set_opts() fail %d", ret);
            releaseSources();
            return false;
        }
    }

    return true;
}

void Transcode::doDecodeVideo(AVPacket* inpacket)
{
    if (src_video_id == AV_CODEC_ID_NONE) {
        LOGD("src has not video");
        return;
    }

    int ret = 0;
    if (video_de_ctx == NULL) {
        const AVCodec* codec = avcodec_find_decoder(src_video_id);
        video_de_ctx = avcodec_alloc_context3(codec);
        if (video_de_ctx == NULL) {
            LOGD("video decodec context create fail");
            releaseSources();
            return;
        }

        // ���ý������;������ֱ�Ӵ�AVCodecParameters�п���
        AVCodecParameters* codecpar = inFmtCtx->streams[video_in_stream_index]->codecpar;
        if ((ret = avcodec_parameters_to_context(video_de_ctx, codecpar)) < 0) {
            LOGD("avcodec_parameters_to_context fail %d", ret);
            releaseSources();
            return;
        }
        // ��ʼ��������
        if (avcodec_open2(video_de_ctx, codec, NULL) < 0) {
            releaseSources();
            LOGD("video avcodec_open2 fail");
            return;
        }
    }

    if (video_de_frame == NULL) {
        video_de_frame = av_frame_alloc();
    }

    if ((ret = avcodec_send_packet(video_de_ctx, inpacket)) < 0) {
        LOGD("video avcodec_send_packet fail %s", av_err2str(ret));
        releaseSources();
        return;
    }
    while (true) {
        // �ӽ��뻺�������ս���������
        if ((ret = avcodec_receive_frame(video_de_ctx, video_de_frame)) < 0) {
            if (ret == AVERROR_EOF) {
                // ���뻺���������ˣ���ôҲҪflush���뻺����
                doEncodeVideo(NULL);
            }
            break;
        }

        // �����Ҫ��ʽת�� ����������и�ʽת��
        if (video_en_frame == NULL) {
            video_en_frame = get_video_frame(video_en_ctx->pix_fmt, video_en_ctx->width, video_en_ctx->height);
            if (video_en_frame == NULL) {
                LOGD("cannot create vdioe fram");
                releaseSources();
                return;
            }
        }

        if (video_need_convert) {
            // ��������ת��������
            if (sws_ctx == NULL) {
                AVCodecParameters* inpar = inFmtCtx->streams[video_in_stream_index]->codecpar;
                int dst_width = video_en_ctx->width;
                int dst_height = video_en_ctx->height;
                enum AVPixelFormat dst_pix_fmt = video_en_ctx->pix_fmt;
                sws_ctx = sws_getContext(inpar->width, inpar->height, (enum AVPixelFormat)inpar->format,
                    dst_width, dst_height, dst_pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
                if (!sws_ctx) {
                    LOGD("sws_getContext fail");
                    releaseSources();
                    return;
                }
            }

            // ����ת��;����Ŀ��height
            ret = sws_scale(sws_ctx, video_de_frame->data, video_de_frame->linesize, 0, video_de_frame->height, video_en_frame->data, video_en_frame->linesize);
            if (ret < 0) {
                LOGD("sws_scale fail");
                releaseSources();
                return;
            }
        }
        else {
            // ֱ�ӿ�������
            av_frame_copy(video_en_frame, video_de_frame);
        }


        // ���ݿ��������ڱ����
        /** �������⣺warning, too many B-frames in a row
         *  ����ԭ��֮ǰ����doEncodeVideo()�������ݵĲ���Ϊvideo_de_frame,����ǽ���֮��ֱ�ӵõ���AVFrame��������Ͱ�����
         *  ��غͱ��벻���ϵĲ���
         *  ������������´���һ��AVFrame���������õ���AVFrame��data���ݿ�����ȥ��Ȼ���������Ϊ�����AVFrame
         */
        video_en_frame->pts = video_pts;
        video_pts++;
        doEncodeVideo(video_en_frame);
    }
}

/** �������⣺���ɵ�MOV��MP4�ļ����Բ��ţ����ǿ�����С��Ԥ��ͼ
 */
void Transcode::doEncodeVideo(AVFrame* frame)
{
    int ret = 0;
    // ����˽��������ݣ�Ȼ��������±���
    if ((ret = avcodec_send_frame(video_en_ctx, frame)) < 0) {
        LOGD("video encode avcodec_send_frame fail");
        return;
    }

    while (true) {
        AVPacket* pkt = av_packet_alloc();
        if ((ret = avcodec_receive_packet(video_en_ctx, pkt)) < 0) {
            break;
        }

        // ������������д���ļ�
        /** �������⣺���ɵ��ļ�֡�ʲ���
         *  ����ԭ�򣺵�����avformat_write_header()���ڲ���ı��������time_base������������û�����µ���packet��pts,dts,duration��ֵ
         *  ���������ʹ��av_packet_rescale_ts���µ���packet��pts,dts,duration��ֵ
         */
        AVStream* stream = ouFmtCtx->streams[video_ou_stream_index];
        av_packet_rescale_ts(pkt, video_en_ctx->time_base, stream->time_base);
        pkt->stream_index = video_ou_stream_index;

        doWrite(pkt, true);
    }
}

void Transcode::doDecodeAudio(AVPacket* packet)
{
    int ret = 0;
    if (audio_de_ctx == NULL) {
        const AVCodec* codec = avcodec_find_decoder(src_audio_id);
        if (!codec) {
            LOGD("audio decoder not found");
            releaseSources();
            return;
        }
        audio_de_ctx = avcodec_alloc_context3(codec);
        if (!audio_de_ctx) {
            LOGD("audio decodec_ctx fail");
            releaseSources();
            return;
        }

        // ������Ƶ���������Ĳ���;����������Դ�ļ��Ŀ���
        if (avcodec_parameters_to_context(audio_de_ctx, inFmtCtx->streams[audio_in_stream_index]->codecpar) < 0) {
            LOGD("audio set decodec ctx fail");
            releaseSources();
            return;
        }

        if ((avcodec_open2(audio_de_ctx, codec, NULL)) < 0) {
            LOGD("audio avcodec_open2 fail");
            releaseSources();
            return;
        }
    }

    // ���������õ�AVFrame
    if (!audio_de_frame) {
        audio_de_frame = av_frame_alloc();
    }

    if ((ret = avcodec_send_packet(audio_de_ctx, packet)) < 0) {
        LOGD("audio avcodec_send_packet fail %d", ret);
        releaseSources();
        return;
    }

    while (true) {
        if ((ret = avcodec_receive_frame(audio_de_ctx, audio_de_frame)) < 0) {
            if (ret == AVERROR_EOF) {
                LOGD("audio decode finish");
                // ���뻺���������ˣ���ôҲҪflush���뻺����
                doEncodeAudio(NULL);
            }
            break;
        }

        // �����������õ�AVFrame
        if (audio_en_frame == NULL) {
            audio_en_frame = get_audio_frame(audio_en_ctx->sample_fmt, audio_en_ctx->channel_layout, audio_en_ctx->sample_rate, audio_en_ctx->frame_size);
            if (audio_en_frame == NULL) {
                LOGD("can not create audio frame ");
                releaseSources();
                return;
            }
        }

        // ����ɹ���Ȼ�������½��б���;
        // Ϊ�˱���������Ⱦ����������ֻ��ҪҪ�������õ���AVFrame��data���ݿ����������õ�audio_en_frame�У�������������������
        int pts_num = 0;
        if (audio_need_convert) {
            int dst_nb_samples = (int)av_rescale_rnd(swr_get_delay(swr_ctx, audio_de_frame->sample_rate) + audio_de_frame->nb_samples, audio_en_ctx->sample_rate, audio_de_frame->sample_rate, AV_ROUND_UP);
            if (dst_nb_samples != audio_en_frame->nb_samples) {
                av_frame_free(&audio_en_frame);
                audio_en_frame = get_audio_frame(audio_en_ctx->sample_fmt, audio_en_ctx->channel_layout, audio_en_ctx->sample_rate, dst_nb_samples);
                if (audio_en_frame == NULL) {
                    LOGD("can not create audio frame2 ");
                    releaseSources();
                    return;
                }
            }
            /** �������⣺����Ƶ���뷽ʽ��һ��ʱת���������
             *  ����ԭ����Ϊÿ�����뷽ʽ��Ӧ��AVFrame�е�nb_samples��һ���������ٽ��б���ǰҪ����AVFrame��ת��
             *  ������������б���ǰ��ת��
             */
             // ����ת��
            ret = swr_convert(swr_ctx, audio_en_frame->data, dst_nb_samples, (const uint8_t**)audio_de_frame->data, audio_de_frame->nb_samples);
            if (ret < 0) {
                LOGD("swr_convert() fail %d", ret);
                releaseSources();
                doEncodeAudio(NULL);
                break;
            }
            pts_num = ret;
        }
        else {
            av_frame_copy(audio_en_frame, audio_de_frame);
            pts_num = audio_en_frame->nb_samples;
        }


        /** �������⣺�õ����ļ�����ʱ������ͬ��
         *  ����ԭ��������Ƶ��AVFrame��ptsû�����öԣ�pts�ǻ���AVCodecContext��ʱ�����ʱ�䣬����pts�����ù�ʽ��
         *  ��Ƶ��pts  = (timebase.den/sample_rate)*nb_samples*index��  indexΪ��ǰ�ڼ�����ƵAVFrame(������0��ʼ)��nb_samplesΪÿ��AVFrame�еĲ�����
         *  ��Ƶ��pts = (timebase.den/fps)*index��
         *  ����������������´��뷽ʽ����AVFrame��pts
        */
        //        audio_en_frame->pts = audio_pts++;    // �����������ͬ��������
        audio_en_frame->pts = av_rescale_q(audio_pts, { 1, audio_en_frame->sample_rate }, audio_de_ctx->time_base);
        audio_pts += pts_num;
        doEncodeAudio(audio_en_frame);
    }
}

void Transcode::doEncodeAudio(AVFrame* frame)
{
    int ret = 0;
    if ((ret = avcodec_send_frame(audio_en_ctx, frame)) < 0) {
        LOGD("audio avcodec_send_frame fail %d", ret);
        releaseSources();
    }


    while (true) {
        AVPacket* pkt = av_packet_alloc();
        if ((ret = avcodec_receive_packet(audio_en_ctx, pkt)) < 0) {
            break;
        }

        // ˵������õ���һ��������֡
        // ��Ϊ����avformat_write_header()�������ڲ���ı�ouFmtCtx->streams[audio_ou_stream_index]->time_base��ֵ����
        // pkt�ǰ���audio_en_ctx->time_base�����б���ģ���������д���ļ�֮ǰҪ����ʱ���ת��
        AVStream* stream = ouFmtCtx->streams[audio_ou_stream_index];
        av_packet_rescale_ts(pkt, audio_en_ctx->time_base, stream->time_base);
        pkt->stream_index = audio_ou_stream_index;

        doWrite(pkt, false);
    }
}

void Transcode::doWrite(AVPacket* packet, bool isVideo)
{
    if (!packet) {
        LOGD("packet is null");
        return;
    }

    AVPacket* a_pkt = NULL;
    AVPacket* v_pkt = NULL;
    AVPacket* w_pkt = NULL;
    if (isVideo) {
        v_pkt = packet;
    }
    else {
        a_pkt = packet;
    }

    // Ϊ�˾�ȷ�ıȽ�ʱ�䣬�Ȼ���һ�������
    if (last_video_pts == 0 && isVideo && videoCache.size() == 0) {
        videoCache.push_back(packet);
        last_video_pts = packet->pts;
        LOGD(" ��û�� %s ����", audio_pts == 0 ? "audio" : "video");
        return;
    }

    if (last_audio_pts == 0 && !isVideo && audioCache.size() == 0) {
        audioCache.push_back(packet);
        last_audio_pts = packet->pts;
        LOGD(" ��û�� %s ����", video_pts == 0 ? "video" : "audio");
        return;
    }

    if (videoCache.size() > 0) {    // �����л���������
        v_pkt = videoCache.front();
        if (isVideo) {
            videoCache.push_back(packet);
        }
    }

    if (audioCache.size() > 0) {    // �����л���������
        a_pkt = audioCache.front();
        if (!isVideo) {
            audioCache.push_back(packet);
        }
    }

    if (v_pkt) {
        last_video_pts = v_pkt->pts;
    }

    if (a_pkt) {
        last_audio_pts = a_pkt->pts;
    }


    if (a_pkt && v_pkt) {   // �������� �����ʱ��ıȽ�
        AVStream* a_stream = ouFmtCtx->streams[audio_ou_stream_index];
        AVStream* v_stream = ouFmtCtx->streams[video_ou_stream_index];
        if (av_compare_ts(last_audio_pts, a_stream->time_base, last_video_pts, v_stream->time_base) <= 0) { // ��Ƶ�ں�
            w_pkt = a_pkt;
            if (audioCache.size() > 0) {
                vector<AVPacket*>::iterator begin = audioCache.begin();
                audioCache.erase(begin);
            }
        }
        else {
            w_pkt = v_pkt;
            if (videoCache.size() > 0) {
                vector<AVPacket*>::iterator begin = videoCache.begin();
                videoCache.erase(begin);
            }
        }

    }
    else if (a_pkt) {
        w_pkt = a_pkt;
        if (audioCache.size() > 0) {
            vector<AVPacket*>::iterator begin = audioCache.begin();
            audioCache.erase(begin);
        }
    }
    else if (v_pkt) {
        w_pkt = v_pkt;
        if (videoCache.size() > 0) {
            vector<AVPacket*>::iterator begin = videoCache.begin();
            videoCache.erase(begin);
        }
    }

    static int sum = 0;
    if (!isVideo) {
        sum++;
    }
    //LOGD("index %d pts %d dts %d du %d a_size %d v_size %d sum %d", w_pkt->stream_index, w_pkt->pts, w_pkt->dts, w_pkt->duration, audioCache.size(), videoCache.size(), sum);
    if ((av_interleaved_write_frame(ouFmtCtx, w_pkt)) < 0) {
        LOGD("av_write_frame fail");
    }

    av_packet_unref(w_pkt);
}

AVFrame* Transcode::get_audio_frame(enum AVSampleFormat smpfmt, int64_t ch_layout, int sample_rate, int nb_samples)
{
    AVFrame* audio_en_frame = av_frame_alloc();
    // ���ݲ�����ʽ�������ʣ����������Լ�����������һ��AVFrame
    audio_en_frame->sample_rate = sample_rate;
    audio_en_frame->format = smpfmt;
    audio_en_frame->channel_layout = ch_layout;
    audio_en_frame->nb_samples = nb_samples;
    int ret = 0;
    if ((ret = av_frame_get_buffer(audio_en_frame, 0)) < 0) {
        LOGD("audio get frame buffer fail %d", ret);
        return NULL;
    }

    if ((ret = av_frame_make_writable(audio_en_frame)) < 0) {
        LOGD("audio av_frame_make_writable fail %d", ret);
        return NULL;
    }

    return audio_en_frame;
}

AVFrame* Transcode::get_video_frame(enum AVPixelFormat pixfmt, int width, int height)
{
    AVFrame* video_en_frame = av_frame_alloc();
    video_en_frame->format = pixfmt;
    video_en_frame->width = width;
    video_en_frame->height = height;
    int ret = 0;
    if ((ret = av_frame_get_buffer(video_en_frame, 0)) < 0) {
        LOGD("video get frame buffer fail %d", ret);
        return NULL;
    }

    if ((ret = av_frame_make_writable(video_en_frame)) < 0) {
        LOGD("video av_frame_make_writable fail %d", ret);
        return NULL;
    }

    return video_en_frame;
}

void Transcode::releaseSources()
{
    if (inFmtCtx) {
        avformat_close_input(&inFmtCtx);
        inFmtCtx = NULL;
    }

    if (ouFmtCtx) {
        avformat_free_context(ouFmtCtx);
        ouFmtCtx = NULL;
    }

    if (video_en_ctx) {
        avcodec_free_context(&video_en_ctx);
        video_en_ctx = NULL;
    }

    if (audio_en_ctx) {
        avcodec_free_context(&audio_en_ctx);
        audio_en_ctx = NULL;
    }

    if (video_de_ctx) {
        avcodec_free_context(&video_de_ctx);
        video_de_ctx = NULL;
    }

    if (audio_de_ctx) {
        avcodec_free_context(&audio_de_ctx);
        audio_de_ctx = NULL;
    }

    if (video_de_frame) {
        av_frame_free(&video_de_frame);
        video_de_frame = NULL;
    }

    if (audio_de_frame) {
        av_frame_free(&audio_de_frame);
    }

    if (video_en_frame) {
        av_frame_free(&video_en_frame);
        video_en_frame = NULL;
    }

    if (audio_en_frame) {
        av_frame_free(&audio_en_frame);
        audio_en_frame = NULL;
    }
}
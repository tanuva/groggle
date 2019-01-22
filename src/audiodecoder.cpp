#include "audiodecoder.h"

extern "C"
{
// TODO Check which are needed
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <iostream>

static const int OUTPUT_CHANNEL_COUNT = 1;
static const int OUTPUT_SAMPLERATE = 44100; // TODO Duplicated from main
static const AVSampleFormat OUTPUT_FORMAT = AV_SAMPLE_FMT_S16;
static struct SwrContext *swr = nullptr;

static inline void printSendPacketError(const int e)
{
    switch(e) {
    case AVERROR_EOF:
        std::cerr << "avcodec_send_packet(): EOF\n";
        break;
    case EAGAIN:
        std::cerr << "avcodec_send_packet(): EAGAIN\n";
        break;
    case EINVAL:
        std::cerr << "avcodec_send_packet(): EINVAL\n";
        break;
    case ENOMEM:
        std::cerr << "avcodec_send_packet(): ENOMEM\n";
        break;
    default:
        std::cerr << "avcodec_send_packet() decoding error: " << av_err2str(e) << std::endl;
        break;
    }
}

static inline void printReceiveFrameError(const int e)
{
    switch(e) {
    case AVERROR_EOF:
        std::cerr << "avcodec_receive_frame(): EOF\n";
        break;
    case EAGAIN:
        std::cerr << "avcodec_receive_frame(): EAGAIN\n";
        break;
    case EINVAL:
        std::cerr << "avcodec_receive_frame(): EINVAL\n";
        break;
    default:
        std::cerr << "avcodec_receive_frame() decoding error: " << av_err2str(e) << std::endl;
        break;
    }
}

bool initResampler(const AVCodecContext *codecCtx)
{
    swr = swr_alloc();
    // Hardcode to stereo for now. The codecCtx properties apparently aren't available yet at this point.
    //av_opt_set_int(swr, "in_channel_count",  codecCtx->channels, 0);
    //av_opt_set_int(swr, "in_channel_layout",  codecCtx->channel_layout, 0);
    av_opt_set_int(swr, "in_channel_count",  2, 0);
    av_opt_set_int(swr, "in_channel_layout",  av_get_default_channel_layout(2), 0);
    av_opt_set_int(swr, "in_sample_rate", codecCtx->sample_rate, 0);
    av_opt_set_int(swr, "out_channel_count", OUTPUT_CHANNEL_COUNT, 0);
    av_opt_set_int(swr, "out_channel_layout", av_get_default_channel_layout(OUTPUT_CHANNEL_COUNT), 0);
    av_opt_set_int(swr, "out_sample_rate", OUTPUT_SAMPLERATE, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt",  codecCtx->sample_fmt, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", OUTPUT_FORMAT, 0);
    if (swr_init(swr) != 0) {
        std::cerr << "swr_init failed\n";
        return false;
    }

    // TODO Is this still needed?
    if (!swr_is_initialized(swr)) {
        std::cerr << "Resampler has not been properly initialized\n";
        return false;
    }

    return true;
}

int receiveFrames(AVCodecContext *codecCtx, int16_t **data, size_t *size)
{
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "av_frame_alloc() failed\n";
        return -1;
    }

    int result = 0;
    do {
        result = avcodec_receive_frame(codecCtx, frame);
        if (result < 0) {
            printReceiveFrameError(result);
            av_frame_free(&frame);
            return result;
        }

        // resample frames
        int16_t* buffer;
        av_samples_alloc((uint8_t**) &buffer, NULL, 1, frame->nb_samples, OUTPUT_FORMAT, 0);
        int frame_count = swr_convert(swr, (uint8_t**) &buffer, frame->nb_samples, (const uint8_t**) frame->data, frame->nb_samples);

        // append resampled frames to data
        *data = (int16_t*) realloc(*data, (*size + frame->nb_samples) * sizeof(int16_t));
        memcpy(*data + *size, buffer, frame_count * sizeof(int16_t));
        *size += frame_count;
    } while (result == 0);

    av_frame_free(&frame);
    return result;
}

int decode_audio_file(const char *path, int16_t **data, size_t *size, float *duration)
{
    // initialize all muxers, demuxers and protocols for libavformat
    // (does nothing if called twice during the course of one program execution)
    // FIXME Not needed anymore?
    //av_register_all();

    // get format from audio file
    AVFormatContext* format = avformat_alloc_context();
    if (avformat_open_input(&format, path, NULL, NULL) != 0) {
        std::cerr << "Could not open file " << path << std::endl;
        return -1;
    }
    if (avformat_find_stream_info(format, NULL) < 0) {
        std::cerr << "Could not retrieve stream info from file " << path << std::endl;
        return -1;
    }

    *duration = format->duration / (float)AV_TIME_BASE;

    // Find the index of the first audio stream
    unsigned int stream_index = -1;
    for (unsigned int i = 0; i < format->nb_streams; i++) {

        if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream_index = i;
            break;
        }
    }

    if (stream_index == -1) {
        std::cerr << "Could not retrieve audio stream from file " << path << std::endl;
        return -1;
    }

    // find & open codec
    AVStream* stream = format->streams[stream_index];
    AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);

    // The CodecContext must be configured before opening it
    if (avcodec_parameters_to_context(codecCtx, format->streams[stream_index]->codecpar) < 0) {
        std::cerr << "avcodec_parameters_to_context() failed\n";
    }

    std::cout << "SR: " << codecCtx->sample_rate
              << " SFMT: " << codecCtx->sample_fmt
              << " Chans: " << codecCtx->channels
              << " CLyt: " << codecCtx->channel_layout
              << std::endl;

    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        std::cerr << "Failed to open decoder for stream #" << stream_index << " in file '" << path << "'\n";
        return -1;
    }

    if (!initResampler(codecCtx)) {
        return -1;
    }

    // prepare to read data
    AVPacket packet;
    av_init_packet(&packet);
    int receiveResult = 0;
    bool flushing = false;

    do {
        // Read compressed packed from the stream
        if (av_read_frame(format, &packet) < 0) {
            // EOF or error. Stop working.
            std::cout << "av_read_frame(): EOF or error, stopping\n";
            flushing = true;
        }

        // Drop the compressed packet into the decoder
        int sendResult = avcodec_send_packet(codecCtx, flushing ? NULL : &packet);
        if (sendResult < 0) {
            printSendPacketError(sendResult);
            break;
        }

        // Receive one or more decompressed frames from the decoder, collect them in "data"
        receiveResult = receiveFrames(codecCtx, data, size);
        if (receiveResult < 0) {
            break;
        }
    } while (!flushing);

    if (receiveResult != AVERROR_EOF) {
        return -1;
    }

    swr_free(&swr);
    avcodec_close(codecCtx);
    avformat_free_context(format);
    return 0;
}

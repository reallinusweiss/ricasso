#ifndef FFMPEG_DYN_H
#define FFMPEG_DYN_H

/* ============================================================================
 * ffmpeg_dyn.h — Runtime dynamic loader for optional FFmpeg libraries.
 *
 * Patterned after unicorn_dyn.h:
 * Allows SwordigoDesktop to dynamically load system FFmpeg at runtime via
 * dlopen() on Linux or LoadLibraryA() on Windows without requiring static
 * libraries, complex linker hacks, or compile-time dependencies.
 *
 * If FFmpeg is not installed on the user's system, video backgrounds (VBG)
 * gracefully fall back to vanilla static background textures with zero crashes.
 * ============================================================================ */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations of opaque FFmpeg structs
struct AVFormatContext;
struct AVCodecContext;
struct AVCodecParameters;
struct AVCodec;
struct SwsContext;
struct AVFrame;
struct AVPacket;
struct AVStream;

#ifndef SWSCALE_SWSCALE_H
#ifndef SWS_BILINEAR
#define SWS_BILINEAR 2
#endif
#endif

#ifndef AVUTIL_AVUTIL_H
#ifndef AVMEDIA_TYPE_VIDEO
#define AVMEDIA_TYPE_VIDEO 0
#endif

#ifndef AV_PIX_FMT_RGBA
#define AV_PIX_FMT_RGBA 26
#endif
#endif

#ifndef AVSEEK_FLAG_BACKWARD
#define AVSEEK_FLAG_BACKWARD 1
#endif

#ifndef AVERROR
#define AVERROR(e) (-(e))
#endif

#ifndef EAGAIN
#define EAGAIN 11
#endif

#ifndef AVERROR_EOF
#define AVERROR_EOF (-(int)(('E') | ('O' << 8) | ('F' << 16) | (' ' << 24)))
#endif

// ── Function pointer dispatch table ─────────────────────────────────────────
struct FFmpegApi {
    // libavformat
    int (*avformat_open_input)(struct AVFormatContext **ps, const char *url, const void *fmt, void **options);
    void (*avformat_close_input)(struct AVFormatContext **s);
    int (*avformat_find_stream_info)(struct AVFormatContext *ic, void **options);
    int (*av_read_frame)(struct AVFormatContext *s, struct AVPacket *pkt);
    int (*av_seek_frame)(struct AVFormatContext *s, int stream_index, int64_t timestamp, int flags);

    // libavutil
    int (*av_strerror)(int errnum, char *errbuf, size_t errbuf_size);

    // libavcodec
    const struct AVCodec* (*avcodec_find_decoder)(int id);
    struct AVCodecContext* (*avcodec_alloc_context3)(const struct AVCodec *codec);
    void (*avcodec_free_context)(struct AVCodecContext **avctx);
    int (*avcodec_parameters_to_context)(struct AVCodecContext *codec, const struct AVCodecParameters *par);
    int (*avcodec_open2)(struct AVCodecContext *avctx, const struct AVCodec *codec, void **options);
    int (*avcodec_send_packet)(struct AVCodecContext *avctx, const struct AVPacket *avpkt);
    int (*avcodec_receive_frame)(struct AVCodecContext *avctx, struct AVFrame *frame);
    void (*avcodec_flush_buffers)(struct AVCodecContext *avctx);

    // libswscale
    struct SwsContext* (*sws_getContext)(int srcW, int srcH, int srcFormat,
                                         int dstW, int dstH, int dstFormat,
                                         int flags, void *srcFilter, void *dstFilter, const double *param);
    int (*sws_scale)(struct SwsContext *c, const uint8_t *const srcSlice[],
                     const int srcStride[], int srcSliceY, int srcSliceH,
                     uint8_t *const dst[], const int dstStride[]);
    void (*sws_freeContext)(struct SwsContext *swsContext);

    // Memory / Frame / Packet management
    struct AVFrame* (*av_frame_alloc)(void);
    void (*av_frame_free)(struct AVFrame **frame);
    struct AVPacket* (*av_packet_alloc)(void);
    void (*av_packet_free)(struct AVPacket **pkt);
    void (*av_packet_unref)(struct AVPacket *pkt);
};

extern struct FFmpegApi ffmpeg_api;

/// Attempt to load FFmpeg dynamic libraries and resolve function pointers.
/// Returns true if all required libraries and symbols were successfully loaded.
bool ffmpeg_dyn_init(void);

/// Returns true if FFmpeg was successfully loaded and is ready for use.
bool ffmpeg_dyn_available(void);

/// Returns human-readable failure reason if FFmpeg failed to load.
const char* ffmpeg_dyn_fail_reason(void);

#ifdef __cplusplus
}
#endif

#ifndef FFMPEG_DYN_NO_MACROS
// ── Convenient dispatch macros for transparent drop-in usage ────────────────
#define avformat_open_input           ffmpeg_api.avformat_open_input
#define avformat_close_input          ffmpeg_api.avformat_close_input
#define avformat_find_stream_info     ffmpeg_api.avformat_find_stream_info
#define av_read_frame                 ffmpeg_api.av_read_frame
#define av_seek_frame                 ffmpeg_api.av_seek_frame
#define av_strerror                   ffmpeg_api.av_strerror
#define avcodec_find_decoder          ffmpeg_api.avcodec_find_decoder
#define avcodec_alloc_context3        ffmpeg_api.avcodec_alloc_context3
#define avcodec_free_context          ffmpeg_api.avcodec_free_context
#define avcodec_parameters_to_context ffmpeg_api.avcodec_parameters_to_context
#define avcodec_open2                 ffmpeg_api.avcodec_open2
#define avcodec_send_packet           ffmpeg_api.avcodec_send_packet
#define avcodec_receive_frame         ffmpeg_api.avcodec_receive_frame
#define avcodec_flush_buffers         ffmpeg_api.avcodec_flush_buffers
#define sws_getContext                ffmpeg_api.sws_getContext
#define sws_scale                     ffmpeg_api.sws_scale
#define sws_freeContext               ffmpeg_api.sws_freeContext
#define av_frame_alloc                ffmpeg_api.av_frame_alloc
#define av_frame_free                 ffmpeg_api.av_frame_free
#define av_packet_alloc               ffmpeg_api.av_packet_alloc
#define av_packet_free                ffmpeg_api.av_packet_free
#define av_packet_unref               ffmpeg_api.av_packet_unref
#endif

#endif // FFMPEG_DYN_H

/* LavaVu VideoEncoder
 * Owen Kaluza 2015, (c) Monash University
 * Based on code from libavformat/output-example.c
 ****************************************************
 * Libavformat API example: Output a media file in any supported
 * libavformat format. The default codecs are used.
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef VideoEncoder__
#define VideoEncoder__

#include "OutputInterface.h"

#ifdef HAVE_LIBAVCODEC
extern "C"
{
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <stdint.h>
#include <assert.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
#ifdef HAVE_SWSCALE
#include <libswscale/swscale.h>
#endif
}
#undef exit

#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 52, 0))
#define avcodec_free_context av_freep
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,45,101)
#define av_frame_alloc avcodec_alloc_frame
#endif

//Compatibility hacks for old versions of avcodec
#if LIBAVCODEC_VERSION_MAJOR < 54
#define AVPixelFormat PixelFormat
#define AV_PIX_FMT_YUV420P PIX_FMT_YUV420P
#define AV_PIX_FMT_YUV444P PIX_FMT_YUV444P
#define AV_PIX_FMT_RGB24 PIX_FMT_RGB24
#endif

#if LIBAVCODEC_VERSION_MAJOR < 52 || (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR < 64)
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif

#if LIBAVFORMAT_VERSION_MAJOR < 52 || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR < 45)
#define av_guess_format guess_format
#endif

#if LIBAVFORMAT_VERSION_MAJOR < 52 || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR < 101)
#define av_dump_format dump_format
#endif

#if LIBAVFORMAT_VERSION_MAJOR < 52 || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR < 105)
#define avio_open url_fopen
#define avio_tell url_ftell
#define avio_close url_fclose
#endif

#ifndef AVIO_FLAG_WRITE
#define AVIO_FLAG_WRITE URL_WRONLY
#endif

#ifndef AV_PKT_FLAG_KEY
#define AV_PKT_FLAG_KEY PKT_FLAG_KEY
#endif

#ifndef AV_CODEC_FLAG_GLOBAL_HEADER
#define AV_CODEC_FLAG_GLOBAL_HEADER CODEC_FLAG_GLOBAL_HEADER
#endif

#ifndef AV_CODEC_FLAG_LOOP_FILTER
#define AV_CODEC_FLAG_LOOP_FILTER CODEC_FLAG_LOOP_FILTER
#endif

#if LIBAVFORMAT_VERSION_MAJOR < 54 || (LIBAVFORMAT_VERSION_MAJOR == 54 && LIBAVFORMAT_VERSION_MINOR < 25)
#define AVCodecID CodecID
#define AV_CODEC_ID_NONE CODEC_ID_NONE
#define AV_CODEC_ID_MPEG1VIDEO CODEC_ID_MPEG1VIDEO
#define AV_CODEC_ID_MPEG2VIDEO CODEC_ID_MPEG2VIDEO
#define AV_CODEC_ID_H264 CODEC_ID_H264
#endif

#endif

#define VIDEO_HIGHQ 3
#define VIDEO_MEDQ 2
#define VIDEO_LOWQ 1

class VideoEncoder : public OutputInterface
{
public:
  std::string filename = "";
  VideoEncoder(const std::string& fn, int fps, int quality=VIDEO_HIGHQ);
  ~VideoEncoder();

  //OutputInterface
  virtual void open(unsigned int w, unsigned int h);
  virtual void close();
  virtual void resize(unsigned int new_width, unsigned int new_height);
  virtual void display();

  void copyframe(unsigned char* array, int len);

protected:
  int fps;
  int quality;
#ifdef HAVE_LIBAVCODEC
  AVFormatContext *oc;
  AVStream *video_st;
  AVCodecContext *video_enc;
#ifdef HAVE_SWSCALE
  SwsContext * ctx;
#endif
  AVFrame *picture;
  uint8_t *video_outbuf;
  int frame_count, video_outbuf_size;

  AVStream* add_video_stream(enum AVCodecID codec_id);
  AVFrame* alloc_picture(enum AVPixelFormat pix_fmt);
  void open_video();
  void write_video_frame();
  void close_video();
#else
  int frame = 0;
#endif //HAVE_LIBAVCODEC
};

#endif //VideoEncoder__

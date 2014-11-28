#ifndef VideoEncoder__
#define VideoEncoder__

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
}
#undef exit

class VideoEncoder
{
  public:
   VideoEncoder(const char *filename, int width, int height);
   ~VideoEncoder();
   void frame(unsigned char* pixels, int channels=3);
   AVOutputFormat *defaultCodec(const char *filename);

  protected:
   int width, height;
   AVFormatContext *oc;
   AVStream *video_st;

   AVFrame *picture;
   uint8_t *video_outbuf;
   int frame_count, video_outbuf_size;

   AVStream* add_video_stream(enum CodecID codec_id);
   AVFrame* alloc_picture(enum PixelFormat pix_fmt);
   void open_video();
   void write_video_frame();
   void close_video();
};

#endif //HAVE_LIBAVCODEC
#endif //VideoEncoder__

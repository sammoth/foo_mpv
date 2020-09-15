#pragma once
#include "stdafx.h"
// PCH ^

#include "../SDK/foobar2000.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include "libswscale/swscale.h"
}

namespace mpv {
class frame_extractor {
  pfc::string8 filename;
  bool found;

  AVFormatContext* p_format_context;
  AVCodec* codec;
  AVCodecParameters* params;
  AVCodecContext* p_codec_context;
  AVPacket* p_packet;
  AVFrame* p_frame;

  AVPixelFormat output_pixelformat;
  AVCodec* output_encoder;
  AVCodecContext* output_codeccontext;
  AVPacket* output_packet;
  AVFrame* outputformat_frame;

  int stream_index;

 public:
  enum art_image_format {
    jpeg = 1,
    bitmap = 2,
  };

  frame_extractor(art_image_format format, const char* path);
  ~frame_extractor();
  void seek_percent(float percent);

  album_art_data_ptr get_art();
};
}  // namespace mpv

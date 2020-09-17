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
void clear_thumbnail_cache();
void clean_thumbnail_cache();
void regenerate_thumbnail_cache();
void compact_thumbnail_cache();

class thumbnailer {
  metadb_handle_ptr metadb;
  float time_start_in_file;
  float time_end_in_file;

  bool set_output_quality;

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
  thumbnailer(metadb_handle_ptr p_metadb);
  ~thumbnailer();
  void seek_default();
  void seek_percent(float percent);

  album_art_data_ptr get_art();
};
}  // namespace mpv

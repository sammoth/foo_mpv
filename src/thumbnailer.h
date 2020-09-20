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
  double time_start_in_file;
  double time_end_in_file;

  AVFormatContext* p_format_context;
  AVCodec* codec;
  AVCodecParameters* params;
  AVCodecContext* p_codec_context;
  AVPacket* p_packet;
  AVFrame* p_frame;

  void init_measurement_context();
  int rgb_buf_size = 0;
  SwsContext* measurement_context = NULL;
  AVFrame* measurement_frame;

  AVCodec* output_encoder;
  AVCodecContext* output_codeccontext;
  AVPacket* output_packet;
  AVFrame* output_frame;

  int stream_index;

  bool decode_frame();
  album_art_data_ptr encode_output();
  double frame_quality();

 public:
  thumbnailer(metadb_handle_ptr p_metadb);
  ~thumbnailer();
  bool seek(double percent);

  album_art_data_ptr get_art();
};
}  // namespace mpv

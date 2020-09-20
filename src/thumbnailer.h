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
void remove_from_cache(metadb_handle_ptr metadb);

bool thumb_time_store_get(metadb_handle_ptr metadb, double& out);
void thumb_time_store_set(metadb_handle_ptr metadb, const double pos);

metadb_handle_ptr get_thumbnail_item_from_items(metadb_handle_list items);

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
  AVRational p_frame_time_base;

  void init_measurement_context();
  int rgb_buf_size = 0;
  SwsContext* measurement_context = NULL;
  AVFrame* measurement_frame;

  AVCodec* output_encoder;
  AVCodecContext* output_codeccontext;
  AVPacket* output_packet;
  AVFrame* output_frame;

  int stream_index;

  bool seek(double percent);
  bool seek_exact_and_decode(double percent);
  double get_frame_time();
  bool decode_frame();
  album_art_data_ptr encode_output();
  double frame_quality();

 public:
  thumbnailer(metadb_handle_ptr p_metadb);
  ~thumbnailer();

  album_art_data_ptr get_art();
};
}  // namespace mpv

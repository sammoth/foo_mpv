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
void compact_thumbnail_cache();
void remove_from_cache(metadb_handle_ptr metadb);

bool thumb_time_store_get(metadb_handle_ptr metadb, double& out);
void thumb_time_store_set(metadb_handle_ptr metadb, const double pos);

metadb_handle_ptr get_thumbnail_item_from_items(metadb_handle_list items);

class thumbnailer {
  metadb_handle_ptr metadb;
  pfc::string8 filename;
  abort_callback& abort;
  double time_start_in_file;
  double time_end_in_file;

  AVFormatContext* p_format_context = NULL;
  AVCodec* codec = NULL;
  AVCodecParameters* params = NULL;
  AVCodecContext* p_codec_context = NULL;
  AVPacket* p_packet = NULL;
  AVFrame* p_frame = NULL;
  AVRational p_frame_time_base;
  AVRational p_stream_time_base;
  int64_t p_format_start_time;

  void init_measurement_context();
  int rgb_buf_size = 0;
  SwsContext* measurement_context = NULL;
  AVFrame* measurement_frame = NULL;
  AVFrame* best_frame = NULL;

  AVCodec* output_encoder = NULL;
  AVCodecContext* output_codeccontext = NULL;
  AVPacket* output_packet = NULL;
  AVFrame* output_frame = NULL;

  int stream_index;

  bool seek(double percent);
  bool seek_exact_and_decode(double percent);
  double get_frame_time();
  bool decode_frame(bool to_keyframe);
  album_art_data_ptr encode_output();
  double frame_quality();

 public:
  thumbnailer(pfc::string8 p_filename, metadb_handle_ptr p_metadb, abort_callback& p_abort);
  ~thumbnailer();

  album_art_data_ptr get_art();
};
}  // namespace mpv

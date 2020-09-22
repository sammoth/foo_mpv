#pragma once
#include "stdafx.h"
// PCH ^

#include <atomic>
#include <mutex>
#include <sstream>

#include "../SDK/foobar2000.h"
#include "preferences.h"
#include "thumbnailer.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

#include "SQLiteCpp/SQLiteCpp.h"
#include "include/sqlite3.h"

namespace mpv {

extern cfg_uint cfg_thumb_size, cfg_thumb_seek, cfg_thumb_cache_quality,
    cfg_thumb_cache_format, cfg_thumb_cache_size, cfg_thumb_cover_type;
extern cfg_bool cfg_thumbs, cfg_thumb_group_longest, cfg_thumb_histogram,
    cfg_thumb_group_override;
extern advconfig_checkbox_factory cfg_logging;

static const char* query_create_str =
    "CREATE TABLE IF NOT EXISTS thumbs(location TEXT NOT "
    "NULL, subsong INT NOT NULL, created INT, thumb BLOB, PRIMARY "
    "KEY(location, subsong))";
static const char* query_get_str =
    "SELECT thumb FROM thumbs WHERE location = ? AND subsong = ?";
static const char* query_put_str =
    "INSERT INTO thumbs VALUES (?, ?, CURRENT_TIMESTAMP, ?)";
static const char* query_dbsize_str =
    "SELECT sum(pgsize) FROM dbstat WHERE name='thumbs';";
static const char* query_dbtrim_str =
    "DELETE FROM thumbs WHERE created < (SELECT created FROM thumbs "
    "ORDER BY created LIMIT 1 OFFSET (SELECT 2*count(1)/3 from "
    "thumbs))";
static const char* query_delete_str =
    "DELETE FROM thumbs WHERE location = ? AND subsong = ?";

static std::unique_ptr<std::timed_mutex[]> art_generation_mutexes(
    new std::timed_mutex[256]);
static titleformat_object::ptr art_generation_mutex_formatter;

static std::timed_mutex* get_mutex_for_item(metadb_handle_ptr metadb) {
  if (art_generation_mutex_formatter.is_empty()) {
    static_api_ptr_t<titleformat_compiler>()->compile_force(
        art_generation_mutex_formatter, "%path% | %subsong%");
  }

  pfc::string_formatter str;
  metadb->format_title(NULL, str, art_generation_mutex_formatter, NULL);
  t_uint64 hash =
      static_api_ptr_t<hasher_md5>()->process_single_string(str).xorHalve();

  return &art_generation_mutexes[hash % 256];
}

static std::mutex db_mutex;
static std::unique_ptr<SQLite::Database> db_ptr;
static std::unique_ptr<SQLite::Statement> query_get;
static std::unique_ptr<SQLite::Statement> query_put;
static std::unique_ptr<SQLite::Statement> query_size;
static std::unique_ptr<SQLite::Statement> query_trim;
static std::unique_ptr<SQLite::Statement> query_delete;
static std::atomic_int64_t db_size;

class db_loader : public initquit {
 public:
  void on_init() override {
    pfc::string8 db_path = core_api::get_profile_path();
    db_path.add_filename("thumbcache.db");
    db_path.remove_chars(0, 7);

    try {
      db_ptr.reset(new SQLite::Database(
          db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE));

      try {
        db_ptr->exec(query_create_str);
        query_get.reset(new SQLite::Statement(*db_ptr, query_get_str));
        query_put.reset(new SQLite::Statement(*db_ptr, query_put_str));
        query_size.reset(new SQLite::Statement(*db_ptr, query_dbsize_str));
        query_trim.reset(new SQLite::Statement(*db_ptr, query_dbtrim_str));
        query_delete.reset(new SQLite::Statement(*db_ptr, query_delete_str));
      } catch (SQLite::Exception e) {
        console::error(
            "mpv: Error reading thumbnail table, attempting to recreate "
            "database");
        db_ptr->exec("DROP TABLE thumbs");
        db_ptr->exec(query_create_str);
        query_get.reset(new SQLite::Statement(*db_ptr, query_get_str));
        query_put.reset(new SQLite::Statement(*db_ptr, query_put_str));
        query_size.reset(new SQLite::Statement(*db_ptr, query_dbsize_str));
        query_trim.reset(new SQLite::Statement(*db_ptr, query_dbtrim_str));
        query_delete.reset(new SQLite::Statement(*db_ptr, query_delete_str));
      }

      query_size->reset();
      query_size->executeStep();
      db_size = query_size->getColumn(0).getInt64();
    } catch (SQLite::Exception e) {
      db_ptr.reset();
      std::stringstream msg;
      msg << "mpv: Error accessing thumbnail cache: " << e.what();
      console::error(msg.str().c_str());
    }
  }
};

static initquit_factory_t<db_loader> g_db_loader;

void clear_thumbnail_cache() {
  if (db_ptr) {
    try {
      std::lock_guard<std::mutex> lock(db_mutex);
      int deletes = db_ptr->exec("DELETE FROM thumbs");

      query_size->reset();
      query_size->executeStep();
      db_size = query_size->getColumn(0).getInt64();

      std::stringstream msg;
      msg << "mpv: Deleted " << deletes << " thumbnails from database";
      console::info(msg.str().c_str());
    } catch (SQLite::Exception e) {
      std::stringstream msg;
      msg << "mpv: Error clearing thumbnail cache: " << e.what();
      console::error(msg.str().c_str());
    }

  } else {
    console::error("mpv: Thumbnail cache not loaded");
  }
}

void sqlitefunction_missing(sqlite3_context* context, int num,
                            sqlite3_value** val) {
  const char* location = (const char*)sqlite3_value_text(val[0]);
  try {
    if (filesystem::g_exists(location, fb2k::noAbort)) {
      sqlite3_result_int(context, 1);
      return;
    }
  } catch (exception_io e) {
  }
  sqlite3_result_int(context, 0);
}

void clean_thumbnail_cache() {
  if (db_ptr) {
    try {
      std::lock_guard<std::mutex> lock(db_mutex);
      db_ptr->createFunction("missing", 1, true, nullptr,
                             sqlitefunction_missing);
      int deletes =
          db_ptr->exec("DELETE FROM thumbs WHERE missing(location) IS 0");

      query_size->reset();
      query_size->executeStep();
      db_size = query_size->getColumn(0).getInt64();

      std::stringstream msg;
      msg << "mpv: Deleted " << deletes << " dead thumbnails from database";
      console::info(msg.str().c_str());
    } catch (SQLite::Exception e) {
      std::stringstream msg;
      msg << "mpv: Error clearing thumbnail cache: " << e.what();
      console::error(msg.str().c_str());
    }
  } else {
    console::error("mpv: Thumbnail cache not loaded");
  }
}

void compact_thumbnail_cache() {
  if (db_ptr) {
    try {
      std::lock_guard<std::mutex> lock(db_mutex);
      query_get->reset();
      query_put->reset();
      query_size->reset();
      query_trim->reset();
      db_ptr->exec("VACUUM");
      console::info("mpv: Thumbnail database compacted");
    } catch (SQLite::Exception e) {
      std::stringstream msg;
      msg << "mpv: Error clearing thumbnail cache: " << e.what();
      console::error(msg.str().c_str());
    }
  } else {
    console::error("mpv: Thumbnail cache not loaded");
  }
}

void trim_db(int64_t newbytes) {
  db_size += newbytes;

  int64_t max_bytes = 0;
  switch (cfg_thumb_cache_size) {
    case 0:
      max_bytes = 1024 * 1024 * 200;
      break;
    case 1:
      max_bytes = 1024 * 1024 * 500;
      break;
    case 2:
      max_bytes = 1024 * 1024 * 1000;
      break;
    case 3:
      max_bytes = 1024 * 1024 * 2000;
      break;
    case 4:
      return;
    default:
      console::error("mpv: Unknown cache size setting");
      return;
  }

  if (db_size > max_bytes) {
    if (db_ptr) {
      try {
        std::lock_guard<std::mutex> lock(db_mutex);
        query_trim->reset();
        query_trim->exec();
        query_size->reset();
        query_size->executeStep();
        db_size = query_size->getColumn(0).getInt64();
        console::info("mpv: Shrunk thumbnail cache");
      } catch (SQLite::Exception e) {
        std::stringstream msg;
        msg << "mpv: Error shrinking thumbnail cache: " << e.what();
        console::error(msg.str().c_str());
      }
    } else {
      console::error("mpv: Thumbnail cache not loaded");
    }
  }
}

void remove_from_cache(metadb_handle_ptr metadb) {
  if (db_ptr) {
    try {
      std::lock_guard<std::mutex> lock(db_mutex);
      query_delete->reset();
      query_delete->bind(1, metadb->get_path());
      query_delete->bind(2, metadb->get_subsong_index());
      query_delete->exec();
    } catch (SQLite::Exception e) {
      std::stringstream msg;
      msg << "mpv: Error deleting entry from thumbnail cache: " << e.what();
      console::error(msg.str().c_str());
    }
  } else {
    console::error("mpv: Thumbnail cache not loaded");
  }
}

metadb_handle_ptr get_thumbnail_item_from_items(metadb_handle_list items) {
  metadb_handle_ptr item;

  if (cfg_thumb_group_longest) {
    for (unsigned i = 0; i < items.get_size(); i++) {
      if (item.is_empty() || items[i]->get_length() > item->get_length()) {
        item = items[i];
      }
    }
  } else {
    item = items.get_item(0);
  }

  double time;
  if (!thumb_time_store_get(item, time) && cfg_thumb_group_override) {
    for (unsigned i = 0; i < items.get_size(); i++) {
      if (thumb_time_store_get(item, time)) {
        item = items[i];
        break;
      }
    }
  }

  return item;
}

static void libavtry(int error, const char* cmd) {
  if (error < 0) {
    char* error_str = new char[500];
    av_strerror(error, error_str, 500);
    std::stringstream msg;
    msg << "mpv: libav error for " << cmd << ": " << error_str;
    console::error(msg.str().c_str());
    delete[] error_str;

    throw exception_album_art_unsupported_entry();
  }
}

thumbnailer::thumbnailer(metadb_handle_ptr p_metadb, abort_callback& p_abort)
    : metadb(p_metadb),
      abort(p_abort),
      time_start_in_file(0.0),
      time_end_in_file(metadb->get_length()) {
  // get filename
  pfc::string8 filename;
  filename.add_filename(metadb->get_path());
  if (filename.has_prefix("\\file://")) {
    filename.remove_chars(0, 8);

    if (filename.is_empty()) {
      throw exception_album_art_not_found();
    }
  } else {
    throw exception_album_art_not_found();
  }

  // get start/end time of track
  if (metadb->get_subsong_index() > 1) {
    for (t_uint32 s = 0; s < metadb->get_subsong_index(); s++) {
      abort.check();
      playable_location_impl tmp = metadb->get_location();
      tmp.set_subsong(s);
      metadb_handle_ptr subsong = metadb::get()->handle_create(tmp);
      if (subsong.is_valid()) {
        time_start_in_file += subsong->get_length();
      }
    }
  }
  time_end_in_file = time_start_in_file + metadb->get_length();

  p_format_context = avformat_alloc_context();
  p_packet = av_packet_alloc();
  p_frame = av_frame_alloc();

  // open file and find video stream and codec
  abort.check();
  libavtry(avformat_open_input(&p_format_context, filename.c_str(), NULL, NULL),
           "open input");
  libavtry(avformat_find_stream_info(p_format_context, NULL),
           "find stream info");

  stream_index = -1;
  for (unsigned i = 0; i < p_format_context->nb_streams; i++) {
    AVCodecParameters* local_codec_params =
        p_format_context->streams[i]->codecpar;

    if (local_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
      codec = avcodec_find_decoder(local_codec_params->codec_id);
      if (codec == NULL) continue;

      stream_index = i;
      params = local_codec_params;
      break;
    }
  }

  if (stream_index == -1) {
    throw exception_album_art_not_found();
  }

  p_format_start_time = p_format_context->streams[stream_index]->start_time;
  p_stream_time_base = p_format_context->streams[stream_index]->time_base;

  p_codec_context = avcodec_alloc_context3(codec);
  libavtry(avcodec_parameters_to_context(p_codec_context, params),
           "make codec context");
  libavtry(avcodec_open2(p_codec_context, codec, NULL), "open codec");

  // init output encoding
  output_packet = av_packet_alloc();
  output_frame = av_frame_alloc();
  if (cfg_thumb_cache_format == 0) {
    output_frame->format = AV_PIX_FMT_YUVJ444P;
    output_encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    output_codeccontext = avcodec_alloc_context3(output_encoder);
    output_codeccontext->flags |= AV_CODEC_FLAG_QSCALE;
    output_codeccontext->global_quality = FF_QP2LAMBDA;
    output_frame->quality = output_codeccontext->global_quality;
  } else if (cfg_thumb_cache_format == 1) {
    output_frame->format = AV_PIX_FMT_RGB24;
    output_encoder = avcodec_find_encoder(AV_CODEC_ID_PNG);
    output_codeccontext = avcodec_alloc_context3(output_encoder);
  } else {
    console::error("mpv: Could not determine target thumbnail format");
    throw exception_album_art_not_found();
  }
}

thumbnailer::~thumbnailer() {
  av_frame_free(&output_frame);
  av_packet_free(&output_packet);
  avcodec_free_context(&output_codeccontext);

  sws_freeContext(measurement_context);
  av_frame_free(&measurement_frame);
  av_frame_free(&best_frame);

  av_packet_free(&p_packet);
  av_frame_free(&p_frame);
  avcodec_free_context(&p_codec_context);
  avformat_close_input(&p_format_context);
}

void thumbnailer::init_measurement_context() {
  best_frame = av_frame_alloc();

  measurement_frame = av_frame_alloc();
  measurement_frame->format = AV_PIX_FMT_RGB24;
  if (p_frame->width > p_frame->height) {
    measurement_frame->width = 40;
    measurement_frame->height = (40 * p_frame->height) / p_frame->width;
  } else {
    measurement_frame->width = (40 * p_frame->width) / p_frame->height;
    measurement_frame->height = 40;
  }

  libavtry(av_frame_get_buffer(measurement_frame, 32),
           "get measurement frame buffer");
  abort.check();
  measurement_context = sws_getContext(
      p_frame->width, p_frame->height, (AVPixelFormat)p_frame->format,
      measurement_frame->width, measurement_frame->height,
      (AVPixelFormat)measurement_frame->format, SWS_POINT, 0, 0, 0);

  if (measurement_context == NULL) {
    throw exception_album_art_not_found();
  }

  rgb_buf_size = av_image_get_buffer_size(
      (AVPixelFormat)measurement_frame->format, measurement_frame->width,
      measurement_frame->height, 1);
}

bool thumbnailer::seek(double fraction) {
  double seek_time =
      time_start_in_file + fraction * (time_end_in_file - time_start_in_file);

  int64_t seek_pts = p_format_start_time +
                     (int64_t)(seek_time * (double)p_stream_time_base.den /
                               (double)p_stream_time_base.num);

  return avformat_seek_file(p_format_context, stream_index, INT64_MIN, seek_pts,
                            INT64_MAX, 0) >= 0;
}

double thumbnailer::get_frame_time() {
  int64_t pts = p_frame->pts;
  if (p_format_start_time != AV_NOPTS_VALUE) {
    pts -= p_format_start_time;
  }

  return ((double)pts) * p_frame_time_base.num / p_frame_time_base.den;
}

bool thumbnailer::seek_exact_and_decode(double time) {
  double target = time_start_in_file + time;

  abort.check();
  int64_t seek_time = (int64_t)(target * (double)p_stream_time_base.den /
                                (double)p_stream_time_base.num);
  seek_time = seek_time + p_format_start_time;
  int64_t min_seek_time = seek_time - (int64_t)((double)p_stream_time_base.den /
                                                (double)p_stream_time_base.num);
  if (avformat_seek_file(p_format_context, stream_index, min_seek_time,
                         seek_time, seek_time, 0) < 0 ||
      !decode_frame(false)) {
    return false;
  }

  if (cfg_logging) {
    FB2K_console_formatter() << "mpv: Custom thumbnail set at " << target;
    FB2K_console_formatter() << "mpv: Seeking to timestamp " << seek_time;
  }

  for (int i = 0; i < 10; i++) {
    abort.check();
    if (get_frame_time() < target) {
      if (cfg_logging) {
        FB2K_console_formatter()
            << "mpv: OK, got frame at " << get_frame_time();
      }
      break;
    } else {
      seek_time -= (int64_t)(2 * (double)p_stream_time_base.den /
                             (double)p_stream_time_base.num);
      min_seek_time = seek_time - (int64_t)((double)p_stream_time_base.den /
                                            (double)p_stream_time_base.num);
      if (cfg_logging) {
        FB2K_console_formatter()
            << "mpv: Seek unsuccessful, got " << get_frame_time() << ", trying "
            << seek_time;
      }
      if (seek_time < 0 ||
          avformat_seek_file(p_format_context, stream_index, min_seek_time,
                             seek_time, seek_time, 0) < 0 ||
          !decode_frame(false)) {
        return false;
      }
    }
  }

  if (cfg_logging) {
    FB2K_console_formatter()
        << "mpv: Seek successful, decoding to chosen frame";
  }

  while (decode_frame(false)) {
    abort.check();
    if (get_frame_time() > target) {
      if (cfg_logging) {
        FB2K_console_formatter() << "mpv: Frame found at " << get_frame_time();
      }
      return true;
    }
  }

  if (cfg_logging) {
    FB2K_console_formatter()
        << "mpv: Unsuccessful, reached end of file or failed to decode";
  }
  return false;
}

album_art_data_ptr thumbnailer::encode_output() {
  AVRational aspect_ratio = av_guess_sample_aspect_ratio(
      p_format_context, p_format_context->streams[stream_index], p_frame);

  int scale_to_width = p_frame->width;
  int scale_to_height = p_frame->height;
  if (aspect_ratio.num != 0) {
    scale_to_width = scale_to_width * aspect_ratio.num / aspect_ratio.den;
  }

  int target_size = 1;
  switch (cfg_thumb_size) {
    case 0:
      target_size = 200;
      break;
    case 1:
      target_size = 400;
      break;
    case 2:
      target_size = 600;
      break;
    case 3:
      target_size = 1000;
      break;
    case 4:
      target_size = max(scale_to_width, scale_to_height);
      break;
  }

  if (scale_to_width > scale_to_height) {
    scale_to_height = target_size * scale_to_height / scale_to_width;
    scale_to_width = target_size;
  } else {
    scale_to_width = target_size * scale_to_width / scale_to_height;
    scale_to_height = target_size;
  }

  output_frame->width = scale_to_width;
  output_frame->height = scale_to_height;
  libavtry(av_frame_get_buffer(output_frame, 32), "get output frame buffer");

  abort.check();
  SwsContext* swscontext = sws_getContext(
      p_frame->width, p_frame->height, (AVPixelFormat)p_frame->format,
      output_frame->width, output_frame->height,
      (AVPixelFormat)output_frame->format, SWS_LANCZOS, 0, 0, 0);

  if (swscontext == NULL) {
    throw exception_album_art_not_found();
  }

  sws_scale(swscontext, p_frame->data, p_frame->linesize, 0, p_frame->height,
            output_frame->data, output_frame->linesize);

  sws_freeContext(swscontext);

  output_codeccontext->width = output_frame->width;
  output_codeccontext->height = output_frame->height;
  output_codeccontext->pix_fmt = (AVPixelFormat)output_frame->format;
  output_codeccontext->time_base = AVRational{1, 1};

  libavtry(avcodec_open2(output_codeccontext, output_encoder, NULL),
           "open output codec");

  libavtry(avcodec_send_frame(output_codeccontext, output_frame),
           "send output frame");

  abort.check();
  libavtry(avcodec_receive_packet(output_codeccontext, output_packet),
           "receive output packet");

  return album_art_data_impl::g_create(output_packet->data,
                                       output_packet->size);
}

bool thumbnailer::decode_frame(bool to_keyframe) {
  while (true) {
    av_packet_unref(p_packet);
    abort.check();
    if (av_read_frame(p_format_context, p_packet) < 0) continue;

    if (p_packet->stream_index != stream_index) continue;

    if (avcodec_send_packet(p_codec_context, p_packet) < 0) continue;

    av_frame_unref(p_frame);
    if (avcodec_receive_frame(p_codec_context, p_frame) < 0) continue;

    // if (to_keyframe && p_frame->pict_type != AV_PICTURE_TYPE_I) {
    //  continue;
    //}

    p_frame_time_base =
        p_format_context->streams[p_packet->stream_index]->time_base;

    break;
  }

  if (p_frame->width == 0 || p_frame->height == 0) {
    return false;
  }

  if (output_frame == NULL) {
    return false;
  }

  return true;
}

double thumbnailer::frame_quality() {
  sws_scale(measurement_context, p_frame->data, p_frame->linesize, 0,
            p_frame->height, measurement_frame->data,
            measurement_frame->linesize);

  abort.check();
  auto rgb_buf = std::make_unique<unsigned char[]>(rgb_buf_size);
  av_image_copy_to_buffer(rgb_buf.get(), rgb_buf_size, measurement_frame->data,
                          measurement_frame->linesize, AV_PIX_FMT_RGB24,
                          measurement_frame->width, measurement_frame->height,
                          1);

#define BUCKETS 10
  int64_t hist[BUCKETS] = {};

  for (int i = 0; i < rgb_buf_size / 3; i++) {
    unsigned brightness = (rgb_buf.get()[3 * i] + rgb_buf.get()[3 * i + 1] +
                           rgb_buf.get()[3 * i + 2]) /
                          3;

    hist[(BUCKETS - 1) * brightness / 255]++;
    abort.check();
  }

  // TODO maybe calculate something smarter
  double max_bucket = 0.0;
  for (int i = 0; i < BUCKETS; i++) {
    max_bucket = max(((double)hist[i]) * BUCKETS / rgb_buf_size, max_bucket);
  }

  return 1.0 - max_bucket;
}

album_art_data_ptr thumbnailer::get_art() {
  double override_time = 0.0;
  if (thumb_time_store_get(metadb, override_time)) {
    if (!seek_exact_and_decode(override_time)) {
      throw exception_album_art_not_found();
    }
  } else {
    if (cfg_thumb_histogram) {
      unsigned l_seektime = 10;
      // find a good frame, keyframe version
      abort.check();
      if (!seek(0.01 * l_seektime)) throw exception_album_art_not_found();
      abort.check();
      if (!decode_frame(true)) throw exception_album_art_not_found();
      // init after decoding first frame
      init_measurement_context();
      av_frame_ref(best_frame, p_frame);

      const int tries = 12;
      double max_quality = frame_quality();
      if (cfg_logging) {
        FB2K_console_formatter()
            << "mpv: Searching for good thumbnail frame, attempts: " << tries;
        FB2K_console_formatter() << "mpv: Found frame at " << l_seektime
                                 << ", quality: " << max_quality;
      }
      unsigned best_seektime = l_seektime;
      for (int i = 0; i < tries; i++) {
        abort.check();
        l_seektime = (l_seektime + 5) % 100;

        if (!seek(0.01 * (double)l_seektime) || !decode_frame(true)) {
          throw exception_album_art_not_found();
        }

        double quality = frame_quality();
        if (cfg_logging) {
          FB2K_console_formatter() << "mpv: Found frame at " << l_seektime
                                   << ", quality: " << quality;
        }
        if (quality > max_quality) {
          max_quality = quality;
          best_seektime = l_seektime;
          av_frame_unref(best_frame);
          av_frame_ref(best_frame, p_frame);
        }
      }

      av_frame_unref(p_frame);
      av_frame_ref(p_frame, best_frame);
      av_frame_unref(best_frame);
      if (cfg_logging && cfg_thumb_histogram) {
        FB2K_console_formatter() << "mpv: Quality is now " << frame_quality();
      }
    } else {
      abort.check();
      if (cfg_logging) {
        FB2K_console_formatter()
            << "mpv: Using frame at default time " << cfg_thumb_seek;
      }
      if (!seek(0.01 * (double)cfg_thumb_seek))
        throw exception_album_art_not_found();
      if (!decode_frame(false)) throw exception_album_art_not_found();
    }
  }

  return encode_output();
}  // namespace mpv

class empty_album_art_path_list_impl : public album_art_path_list {
 public:
  empty_album_art_path_list_impl() {}
  const char* get_path(t_size index) const { return NULL; }
  t_size get_count() const { return 0; }

 private:
};

class thumbnail_extractor : public album_art_extractor_instance_v2 {
 private:
  metadb_handle_list_cref items;

  album_art_data_ptr cache_get(metadb_handle_ptr metadb) {
    if (query_get) {
      try {
        std::lock_guard<std::mutex> lock(db_mutex);
        query_get->reset();
        query_get->bind(1, metadb->get_path());
        query_get->bind(2, metadb->get_subsong_index());
        if (query_get->executeStep()) {
          SQLite::Column blobcol = query_get->getColumn(0);

          if (blobcol.getBlob() == NULL) {
            std::stringstream msg;
            msg << "mpv: Image was null when fetching cached thumbnail: "
                << metadb->get_path() << "[" << metadb->get_subsong_index()
                << "]";
            console::error(msg.str().c_str());
            return album_art_data_ptr();
          }

          if (cfg_logging) {
            std::stringstream msg;
            msg << "mpv: Fetch from thumbnail cache: " << metadb->get_path()
                << "[" << metadb->get_subsong_index() << "]";
            console::info(msg.str().c_str());
          }
          return album_art_data_impl::g_create(blobcol.getBlob(),
                                               blobcol.getBytes());
        }
      } catch (std::exception e) {
        std::stringstream msg;
        msg << "mpv: Error accessing thumbnail cache: " << e.what();
        console::error(msg.str().c_str());
      }
    }
    return album_art_data_ptr();
  }

  void cache_put(metadb_handle_ptr metadb, album_art_data_ptr data) {
    if (query_put) {
      try {
        {
          std::lock_guard<std::mutex> lock(db_mutex);
          query_put->reset();
          query_put->bind(1, metadb->get_path());
          query_put->bind(2, metadb->get_subsong_index());
          query_put->bind(3, data->get_ptr(), data->get_size());
          query_put->exec();
        }

        if (cfg_logging) {
          std::stringstream msg;
          msg << "mpv: Write to thumbnail cache: " << metadb->get_path() << "["
              << metadb->get_subsong_index() << "]";
          console::info(msg.str().c_str());
        }

        trim_db(data->get_size());
      } catch (SQLite::Exception e) {
        std::stringstream msg;
        msg << "mpv: Error writing to thumbnail cache: " << e.what();
        console::error(msg.str().c_str());
      }
    }
  }

 public:
  thumbnail_extractor(metadb_handle_list_cref items) : items(items) {}

  album_art_data_ptr query(const GUID& p_what,
                           abort_callback& p_abort) override {
    if (!cfg_thumbs || items.get_size() == 0) {
      throw exception_album_art_not_found();
    }

    if (cfg_thumb_cover_type == 0 && p_what != album_art_ids::cover_front ||
        cfg_thumb_cover_type == 1 && p_what != album_art_ids::cover_back ||
        cfg_thumb_cover_type == 2 && p_what != album_art_ids::disc ||
        cfg_thumb_cover_type == 3 && p_what != album_art_ids::artist) {
      throw exception_album_art_not_found();
    }

    metadb_handle_ptr item = get_thumbnail_item_from_items(items);
    // does this item match the configured thumbnail search query
    if (!test_thumb_pattern(item)) throw exception_album_art_not_found();

    // check for cached image
    album_art_data_ptr ret = cache_get(item);
    if (!ret.is_empty()) return ret;

    p_abort.check();
    std::timed_mutex* mut = get_mutex_for_item(item);
    while (!mut->try_lock_for(std::chrono::milliseconds(50))) {
      p_abort.check();
    };
    try {
      // check no other thread just generated the thumbnail
      p_abort.check();
      ret = cache_get(item);
      p_abort.check();
      if (ret.is_empty()) {
        if (cfg_logging) {
          FB2K_console_formatter()
              << "mpv: Generating thumbnail: " << item->get_path() << "["
              << item->get_subsong_index() << "]";
        }

        thumbnailer ex(item, p_abort);
        ret = ex.get_art();
        if (ret.is_empty()) throw exception_album_art_not_found();
        cache_put(item, ret);
      }
    } catch (exception_album_art_not_found e) {
      mut->unlock();
      throw e;
    } catch (exception_aborted e) {
      mut->unlock();
      throw e;
    } catch (...) {
      mut->unlock();
      throw exception_album_art_not_found();
    }

    mut->unlock();
    return ret;
  }

  album_art_path_list::ptr query_paths(const GUID& p_what,
                                       foobar2000_io::abort_callback& p_abort) {
    empty_album_art_path_list_impl* my_list =
        new service_impl_single_t<empty_album_art_path_list_impl>();
    return my_list;
  }
};

class mpv_album_art_fallback : public album_art_fallback {
 public:
  album_art_extractor_instance_v2::ptr open(
      metadb_handle_list_cref items, pfc::list_base_const_t<GUID> const& ids,
      abort_callback& abort) {
    return new service_impl_t<thumbnail_extractor>(items);
  }
};

static service_factory_single_t<mpv_album_art_fallback>
    g_mpv_album_art_fallback;
}  // namespace mpv

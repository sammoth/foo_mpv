#pragma once
#include "stdafx.h"
// PCH ^

#include <sstream>

#include "../SDK/foobar2000.h"
#include "thumbnailer.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include "libswscale/swscale.h"
}

#include "SQLiteCpp/SQLiteCpp.h"
#include "include/sqlite3.h"

namespace mpv {

extern cfg_uint cfg_thumb_size, cfg_thumb_scaling, cfg_thumb_avoid_dark,
    cfg_thumb_seektype, cfg_thumb_seek, cfg_thumb_cache_quality,
    cfg_thumb_cache_format;
extern cfg_bool cfg_thumb_cache, cfg_thumbs;
extern advconfig_checkbox_factory cfg_logging;

static std::unique_ptr<SQLite::Database> db_ptr;
static std::unique_ptr<SQLite::Statement> query_get;
static std::unique_ptr<SQLite::Statement> query_put;

class db_loader : public initquit {
 public:
  void on_init() override {
    try {
      pfc::string8 db_path = core_api::get_profile_path();
      db_path.add_filename("thumbcache.db");
      db_path.remove_chars(0, 7);

      db_ptr.reset(new SQLite::Database(
          db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE));

      db_ptr->exec(
          "CREATE TABLE IF NOT EXISTS thumbs(location TEXT PRIMARY KEY NOT "
          "NULL, subsong INT, created INT, thumb BLOB)");

      query_get.reset(new SQLite::Statement(
          *db_ptr,
          "SELECT thumb FROM thumbs WHERE location = ? AND subsong = ?"));
      query_put.reset(new SQLite::Statement(*db_ptr,
                                            "INSERT INTO thumbs VALUES (?, ?, "
                                            "CURRENT_TIMESTAMP, ?)"));

    } catch (SQLite::Exception e) {
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
      int deletes = db_ptr->exec("DELETE FROM thumbs");
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

  compact_thumbnail_cache();
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
      db_ptr->createFunction("missing", 1, true, nullptr,
                             sqlitefunction_missing);
      int deletes =
          db_ptr->exec("DELETE FROM thumbs WHERE missing(location) IS 0");
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

  compact_thumbnail_cache();
}

void regenerate_thumbnail_cache() {
  if (db_ptr) {
    try {
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
      query_get.reset();
      query_put.reset();
      db_ptr->exec("VACUUM");
      query_get.reset(new SQLite::Statement(
          *db_ptr,
          "SELECT thumb FROM thumbs WHERE location = ? AND subsong = ?"));
      query_put.reset(new SQLite::Statement(*db_ptr,
                                            "INSERT INTO thumbs VALUES (?, ?, "
                                            "CURRENT_TIMESTAMP, ?)"));
    } catch (SQLite::Exception e) {
      std::stringstream msg;
      msg << "mpv: Error clearing thumbnail cache: " << e.what();
      console::error(msg.str().c_str());
    }
  } else {
    console::error("mpv: Thumbnail cache not loaded");
  }
}

static void libavtry(int error) {
  if (error < 0) {
    char* error_str = new char[500];
    av_strerror(error, error_str, 500);
    console::error(error_str);
    delete[] error_str;

    throw exception_album_art_unsupported_entry();
  }
}

thumbnailer::thumbnailer(metadb_handle_ptr p_metadb) : metadb(p_metadb) {
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

  p_format_context = avformat_alloc_context();
  libavtry(
      avformat_open_input(&p_format_context, filename.c_str(), NULL, NULL));

  libavtry(avformat_find_stream_info(p_format_context, NULL));

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

  p_codec_context = avcodec_alloc_context3(codec);
  libavtry(avcodec_parameters_to_context(p_codec_context, params));
  libavtry(avcodec_open2(p_codec_context, codec, NULL));

  if (!cfg_thumb_cache || !db_ptr || !query_get || !query_put ||
      cfg_thumb_cache_format == 2) {
    output_pixelformat = AV_PIX_FMT_BGR24;
    output_encoder = avcodec_find_encoder(AV_CODEC_ID_BMP);
    output_codeccontext = avcodec_alloc_context3(output_encoder);
    set_output_quality = false;
  } else if (cfg_thumb_cache_format == 0) {
    output_pixelformat = AV_PIX_FMT_YUVJ444P;
    output_encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    output_codeccontext = avcodec_alloc_context3(output_encoder);
    output_codeccontext->flags |= AV_CODEC_FLAG_QSCALE;
    set_output_quality = true;
  } else if (cfg_thumb_cache_format == 1) {
    output_pixelformat = AV_PIX_FMT_RGB24;
    output_encoder = avcodec_find_encoder(AV_CODEC_ID_PNG);
    output_codeccontext = avcodec_alloc_context3(output_encoder);
    set_output_quality = false;
  } else {
    console::error("mpv: Could not determine target thumbnail format");
    throw exception_album_art_not_found();
  }

  output_packet = av_packet_alloc();
  outputformat_frame = av_frame_alloc();

  p_packet = av_packet_alloc();
  p_frame = av_frame_alloc();
}

thumbnailer::~thumbnailer() {
  av_frame_free(&outputformat_frame);
  av_packet_free(&output_packet);
  avcodec_free_context(&output_codeccontext);

  av_packet_free(&p_packet);
  av_frame_free(&p_frame);
  avcodec_free_context(&p_codec_context);
  avformat_close_input(&p_format_context);
}

void thumbnailer::seek_default() {
  if (cfg_thumb_seektype == 0) {
    seek_percent((float)cfg_thumb_seek);
  } else {
    seek_percent(30);
  }
}

void thumbnailer::seek_percent(float percent) {
  int64_t timebase_pos = (int64_t)(0.01 * percent * p_format_context->duration);
  libavtry(av_seek_frame(p_format_context, -1, timebase_pos, AVSEEK_FLAG_ANY));
}

album_art_data_ptr thumbnailer::get_art() {
  while (av_read_frame(p_format_context, p_packet) >= 0) {
    if (p_packet->stream_index != stream_index) continue;

    if (avcodec_send_packet(p_codec_context, p_packet) < 0) continue;

    if (avcodec_receive_frame(p_codec_context, p_frame) < 0) continue;

    break;
  }

  if (p_frame->width == 0 || p_frame->height == 0) {
    return album_art_data_ptr();
  }

  if (outputformat_frame == NULL) {
    return album_art_data_ptr();
  }

  AVRational aspect_ratio = av_guess_sample_aspect_ratio(
      p_format_context, p_format_context->streams[stream_index], p_frame);

  int scale_to_width = p_frame->width;
  int scale_to_height = p_frame->height;
  if (aspect_ratio.num != 0) {
    scale_to_width = scale_to_width * aspect_ratio.num / aspect_ratio.den;
  }

  int target_dimension = 1;
  switch (cfg_thumb_size) {
    case 0:
      target_dimension = 200;
      break;
    case 1:
      target_dimension = 400;
      break;
    case 2:
      target_dimension = 600;
      break;
    case 3:
      target_dimension = 1000;
      break;
    case 4:
      target_dimension = max(scale_to_width, scale_to_height);
      break;
  }

  if (scale_to_width > scale_to_height) {
    scale_to_height = target_dimension * scale_to_height / scale_to_width;
    scale_to_width = target_dimension;
  } else {
    scale_to_width = target_dimension * scale_to_width / scale_to_height;
    scale_to_height = target_dimension;
  }

  outputformat_frame->format = output_pixelformat;
  outputformat_frame->width = scale_to_width;
  outputformat_frame->height = scale_to_height;
  libavtry(av_frame_get_buffer(outputformat_frame, 32));

  int flags = 0;
  switch (cfg_thumb_scaling) {
    case 0:
      flags = SWS_BILINEAR;
      break;
    case 1:
      flags = SWS_BICUBIC;
      break;
    case 2:
      flags = SWS_LANCZOS;
      break;
    case 3:
      flags = SWS_SINC;
      break;
    case 4:
      flags = SWS_SPLINE;
      break;
  }

  SwsContext* swscontext = sws_getContext(
      p_frame->width, p_frame->height, (AVPixelFormat)p_frame->format,
      outputformat_frame->width, outputformat_frame->height, output_pixelformat,
      flags, 0, 0, 0);

  if (swscontext == NULL) {
    return album_art_data_ptr();
  }

  sws_scale(swscontext, p_frame->data, p_frame->linesize, 0, p_frame->height,
            outputformat_frame->data, outputformat_frame->linesize);

  output_codeccontext->width = outputformat_frame->width;
  output_codeccontext->height = outputformat_frame->height;
  output_codeccontext->pix_fmt = output_pixelformat;
  output_codeccontext->time_base = AVRational{1, 1};

  if (set_output_quality) {
    output_codeccontext->global_quality =
        (31 - cfg_thumb_cache_quality) * FF_QP2LAMBDA;
    outputformat_frame->quality = output_codeccontext->global_quality;
  }

  libavtry(avcodec_open2(output_codeccontext, output_encoder, NULL));

  libavtry(avcodec_send_frame(output_codeccontext, outputformat_frame));

  if (output_packet == NULL) {
    return album_art_data_ptr();
  }

  libavtry(avcodec_receive_packet(output_codeccontext, output_packet));

  auto pic =
      album_art_data_impl::g_create(output_packet->data, output_packet->size);

  return pic;
}

class empty_album_art_path_list_impl : public album_art_path_list {
 public:
  empty_album_art_path_list_impl() {}
  const char* get_path(t_size index) const { return NULL; }
  t_size get_count() const { return 0; }

 private:
};

class ffmpeg_thumbnailer : public album_art_extractor_instance_v2 {
 private:
  metadb_handle_list_cref items;

 public:
  ffmpeg_thumbnailer(metadb_handle_list_cref items) : items(items) {}

  album_art_data_ptr query(const GUID& p_what,
                           abort_callback& p_abort) override {
    if (!cfg_thumbs || items.get_size() == 0)
      throw exception_album_art_not_found();

    metadb_handle_ptr item = items.get_item(0);

    if (cfg_thumb_cache && query_get) {
      try {
        query_get->reset();
        query_get->bind(1, item->get_path());
        query_get->bind(2, item->get_subsong_index());
        if (query_get->executeStep()) {
          SQLite::Column blobcol = query_get->getColumn(0);
          auto pic = album_art_data_impl::g_create(blobcol.getBlob(),
                                                   blobcol.getBytes());

          if (cfg_logging) {
            std::stringstream msg;
            msg << "mpv: Fetch from thumbnail cache: " << item->get_path()
                << "[" << item->get_subsong_index() << "]";
            console::info(msg.str().c_str());
          }

          return pic;
        }
      } catch (std::exception e) {
        std::stringstream msg;
        msg << "mpv: Error accessing thumbnail cache: " << e.what();
        console::error(msg.str().c_str());
      }
    }

    thumbnailer ex(item);
    ex.seek_default();
    album_art_data_ptr res = ex.get_art();

    if (res == NULL) {
      throw exception_album_art_not_found();
    }

    if (cfg_thumb_cache && query_put) {
      try {
        query_put->reset();
        query_put->bind(1, item->get_path());
        query_put->bind(2, item->get_subsong_index());
        query_put->bind(3, res->get_ptr(), res->get_size());
        query_put->exec();

        if (cfg_logging) {
          std::stringstream msg;
          msg << "mpv: Write to thumbnail cache: " << item->get_path() << "["
              << item->get_subsong_index() << "]";
          console::info(msg.str().c_str());
        }
      } catch (SQLite::Exception e) {
        console::error(e.getErrorStr());
      }
    }

    return res;
  }

  album_art_path_list::ptr query_paths(const GUID& p_what,
                                       foobar2000_io::abort_callback& p_abort) {
    empty_album_art_path_list_impl* my_list =
        new service_impl_single_t<empty_album_art_path_list_impl>();
    return my_list;
  }
};

class my_album_art_fallback : public album_art_fallback {
 public:
  album_art_extractor_instance_v2::ptr open(
      metadb_handle_list_cref items, pfc::list_base_const_t<GUID> const& ids,
      abort_callback& abort) {
    return new service_impl_t<ffmpeg_thumbnailer>(items);
  }
};

static service_factory_single_t<my_album_art_fallback> g_my_album_art_fallback;
}  // namespace mpv

#include "stdafx.h"
// PCH ^

#include <atomic>
#include <mutex>
#include <sstream>

#include "../SDK/foobar2000.h"
#include "include/libjpeg-turbo/turbojpeg.h"
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

extern cfg_uint cfg_thumb_size, cfg_thumb_avoid_dark, cfg_thumb_seektype,
    cfg_thumb_seek, cfg_thumb_cache_quality, cfg_thumb_cache_format,
    cfg_thumb_cache_size, cfg_thumb_cover_type;
extern cfg_bool cfg_thumb_cache, cfg_thumbs, cfg_thumb_group_longest;
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

static std::mutex mut;
static std::unique_ptr<SQLite::Database> db_ptr;
static std::unique_ptr<SQLite::Statement> query_get;
static std::unique_ptr<SQLite::Statement> query_put;
static std::unique_ptr<SQLite::Statement> query_size;
static std::unique_ptr<SQLite::Statement> query_trim;
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
      }

      query_size->reset();
      query_size->executeStep();
      db_size = query_size->getColumn(0).getInt64();
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
      std::lock_guard<std::mutex> lock(mut);
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
      std::lock_guard<std::mutex> lock(mut);
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

void regenerate_thumbnail_cache() {
  if (db_ptr) {
    try {
      std::lock_guard<std::mutex> lock(mut);
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
      std::lock_guard<std::mutex> lock(mut);
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

static void trim_db(int64_t newbytes) {
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
        std::lock_guard<std::mutex> lock(mut);
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

thumbnailer::thumbnailer(metadb_handle_ptr p_metadb)
    : metadb(p_metadb),
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

  // open file and find video stream
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

  p_codec_context = avcodec_alloc_context3(codec);
  libavtry(avcodec_parameters_to_context(p_codec_context, params),
           "make codec context");
  libavtry(avcodec_open2(p_codec_context, codec, NULL), "open codec");

  scaled_frame = av_frame_alloc();
}

thumbnailer::~thumbnailer() {
  av_frame_free(&scaled_frame);

  av_packet_free(&p_packet);
  av_frame_free(&p_frame);
  avcodec_free_context(&p_codec_context);
  avformat_close_input(&p_format_context);
}

void thumbnailer::seek(double fraction) {
  double seek_time =
      time_start_in_file + fraction * (time_end_in_file - time_start_in_file);

  libavtry(av_seek_frame(p_format_context, -1,
                         (int64_t)(seek_time * (double)AV_TIME_BASE),
                         AVSEEK_FLAG_ANY),
           "seek");
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

  scaled_frame->format = AV_PIX_FMT_0RGB;
  scaled_frame->width = scale_to_width;
  scaled_frame->height = scale_to_height;
  libavtry(av_frame_get_buffer(scaled_frame, 0), "get output frame buffer");

  SwsContext* swscontext = sws_getContext(
      p_frame->width, p_frame->height, (AVPixelFormat)p_frame->format,
      scaled_frame->width, scaled_frame->height,
      (AVPixelFormat)scaled_frame->format, SWS_LANCZOS, 0, 0, 0);

  if (swscontext == NULL) {
    throw exception_album_art_not_found();
  }

  sws_scale(swscontext, p_frame->data, p_frame->linesize, 0, p_frame->height,
            scaled_frame->data, scaled_frame->linesize);

  int rgb_buf_size = av_image_get_buffer_size(
      AV_PIX_FMT_0RGB, scaled_frame->width, scaled_frame->height, 1);
  auto rgb_buf = std::make_unique<unsigned char[]>(rgb_buf_size);
  av_image_copy_to_buffer(rgb_buf.get(), rgb_buf_size, scaled_frame->data,
                          scaled_frame->linesize, AV_PIX_FMT_0RGB,
                          scaled_frame->width, scaled_frame->height, 1);

  tjhandle tj_instance = NULL;
  unsigned char* jpeg_buf = NULL;
  unsigned long jpeg_size = 0;
  if ((tj_instance = tjInitCompress()) == NULL ||
      tjCompress2(tj_instance, rgb_buf.get(), scaled_frame->width, 0,
                  scaled_frame->height, TJPF_XRGB, &jpeg_buf, &jpeg_size,
                  TJSAMP_444, cfg_thumb_cache ? cfg_thumb_cache_quality : 100,
                  0) < 0) {
    console::error("mpv: Error encoding JPEG");
    if (tj_instance != NULL) tjDestroy(tj_instance);
    if (jpeg_buf != NULL) tjFree(jpeg_buf);
    throw exception_album_art_not_found();
  }

  tjDestroy(tj_instance);
  tj_instance = NULL;

  album_art_data_ptr output_ptr =
      album_art_data_impl::g_create(jpeg_buf, jpeg_size);

  tjFree(jpeg_buf);
  jpeg_buf = NULL;

  return output_ptr;
}

void thumbnailer::decode_frame() {
  while (av_read_frame(p_format_context, p_packet) >= 0) {
    if (p_packet->stream_index != stream_index) continue;

    if (avcodec_send_packet(p_codec_context, p_packet) < 0) continue;

    if (avcodec_receive_frame(p_codec_context, p_frame) < 0) continue;

    break;
  }

  if (p_frame->width == 0 || p_frame->height == 0) {
    throw exception_album_art_not_found();
  }

  if (scaled_frame == NULL) {
    throw exception_album_art_not_found();
  }
}

album_art_data_ptr thumbnailer::get_art() {
  if (cfg_thumb_seektype == 0) {
    // fixed absolute seek
    seek(0.01 * (double)cfg_thumb_seek);
    decode_frame();
  } else {
    // choose a good frame
    seek(0.3);
    decode_frame();
  }

  return encode_output();
}

class thumbnailer_art_provider : public album_art_extractor_instance_v2 {
 private:
  metadb_handle_list_cref items;

  album_art_data_ptr cache_get(metadb_handle_ptr metadb) {
    if (cfg_thumb_cache && query_get) {
      try {
        std::lock_guard<std::mutex> lock(mut);
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
    if (cfg_thumb_cache && query_put) {
      try {
        {
          std::lock_guard<std::mutex> lock(mut);
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
  thumbnailer_art_provider(metadb_handle_list_cref items) : items(items) {}

  album_art_data_ptr query(const GUID& p_what,
                           abort_callback& p_abort) override {
    if (!cfg_thumbs || items.get_size() == 0)
      throw exception_album_art_not_found();

    if (cfg_thumb_cover_type == 0 && p_what != album_art_ids::cover_front ||
        cfg_thumb_cover_type == 1 && p_what != album_art_ids::cover_back ||
        cfg_thumb_cover_type == 2 && p_what != album_art_ids::disc ||
        cfg_thumb_cover_type == 3 && p_what != album_art_ids::artist

    ) {
      throw exception_album_art_not_found();
    }

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

    album_art_data_ptr ret = cache_get(item);

    if (!ret.is_empty()) {
      return ret;
    }

    thumbnailer ex(item);
    ret = ex.get_art();

    if (ret.is_empty()) {
      throw exception_album_art_not_found();
    }

    cache_put(item, ret);

    return ret;
  }

  album_art_path_list::ptr query_paths(const GUID& p_what,
                                       foobar2000_io::abort_callback& p_abort) {
    service_ptr_t<album_art_path_list_dummy> instance =
        new service_impl_t<album_art_path_list_dummy>();
    return instance;
  }
};

class my_album_art_fallback : public album_art_fallback {
 public:
  album_art_extractor_instance_v2::ptr open(
      metadb_handle_list_cref items, pfc::list_base_const_t<GUID> const& ids,
      abort_callback& abort) {
    return new service_impl_t<thumbnailer_art_provider>(items);
  }
};

static service_factory_single_t<my_album_art_fallback> g_my_album_art_fallback;
}  // namespace mpv

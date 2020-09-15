#include "stdafx.h"
// PCH ^

#include "../SDK/foobar2000.h"
#include "frame_extractor.h"

namespace mpv {
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
    if (items.get_size() == 0) throw exception_album_art_not_found();

    metadb_handle_ptr first_item = items.get_item(0);
    const char* path = first_item->get_path();

    frame_extractor extractor =
        frame_extractor(frame_extractor::art_image_format::bitmap, path);
    extractor.seek_percent(20);

    album_art_data_ptr res = extractor.get_art();
    if (res == NULL) {
      throw exception_album_art_not_found();
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
}  // namespace

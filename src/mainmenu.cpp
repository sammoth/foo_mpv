#include "stdafx.h"
// PCH ^

#include <thread>

#include "player.h"
#include "popup_window.h"
#include "resource.h"
#include "thumbnailer.h"

namespace mpv {
extern cfg_bool cfg_video_enabled, cfg_autopopup;

static const GUID guid_thumbnails_group = {
    0x41ede16, 0xaf85, 0x408d, {0x88, 0xcc, 0x4, 0x5f, 0xcb, 0x29, 0x62, 0x88}};
static const GUID guid_player_group = {
    0x1e18e391,
    0x9b9f,
    0x4ca2,
    {0xb0, 0xb4, 0x7a, 0x69, 0xf, 0xc7, 0x32, 0x99}};

static const GUID guid_video_enable = {
    0x516e4c09,
    0x23d2,
    0x4e1f,
    {0x9c, 0xc6, 0x87, 0x1e, 0x7a, 0x1d, 0xf3, 0xad}};

static const GUID guid_mpv_popup = {
    0xfd1d7b8c,
    0xeb77,
    0x42b5,
    {0x93, 0x85, 0x2c, 0x95, 0xd3, 0xe6, 0x71, 0xba}};
static const GUID guid_mpv_restart = {
    0xf81f68d8,
    0xdbcf,
    0x4a52,
    {0x9a, 0x7b, 0xae, 0x5e, 0x87, 0x26, 0x77, 0x3a}};

static const GUID guid_thumbs_clear = {
    0xcdcb9b1c,
    0xb2b7,
    0x46bc,
    {0x99, 0xeb, 0xcd, 0x1f, 0xc4, 0x6d, 0xd8, 0xd8}};
static const GUID guid_thumbs_removedead = {
    0x21fb5ddb,
    0xa340,
    0x4c2d,
    {0x86, 0x2c, 0x18, 0xb2, 0xe4, 0x9b, 0xf1, 0xd4}};
static const GUID guid_thumbs_compact = {
    0x668d7424,
    0x9a84,
    0x4504,
    {0x8c, 0x45, 0xac, 0xc, 0xd7, 0xe2, 0x70, 0xfb}};

static const GUID guid_fullscreen = {
    0x189f7f5e,
    0xbb72,
    0x4b94,
    {0x80, 0x38, 0x6a, 0xac, 0x66, 0x73, 0xf, 0xbb}};

static const GUID guid_fullscreen_monitor_1 = {
    0x7bc543f7,
    0x707d,
    0x42ef,
    {0xba, 0xdb, 0x66, 0x56, 0x1f, 0x3f, 0xcb, 0xe2}};
static const GUID guid_fullscreen_monitor_2 = {
    0xa9c07229,
    0xcf22,
    0x4485,
    {0x80, 0xd8, 0xa5, 0x4e, 0x6d, 0xec, 0x67, 0xd2}};
static const GUID guid_fullscreen_monitor_3 = {
    0x3a7f8ef0,
    0x86e,
    0x4f1b,
    {0xa3, 0x97, 0x43, 0x3f, 0xf2, 0xc2, 0x14, 0x30}};
static const GUID guid_fullscreen_monitor_4 = {
    0xb0c52c15,
    0xa3a,
    0x446c,
    {0x9f, 0x7b, 0x1f, 0xaf, 0x32, 0xd1, 0x57, 0x70}};
static const GUID guid_fullscreen_monitor_5 = {
    0xaa38c3eb,
    0xb544,
    0x4135,
    {0x9f, 0x22, 0xaf, 0x9, 0xa8, 0x76, 0xa1, 0xd9}};
static const GUID guid_fullscreen_monitor_6 = {
    0x6341d162,
    0xabde,
    0x4ff3,
    {0x8b, 0x84, 0x94, 0xb7, 0xe0, 0x16, 0x8d, 0x53}};
static const GUID guid_fullscreen_monitor_7 = {
    0x3145fa24,
    0xed6b,
    0x4f34,
    {0x9e, 0x98, 0x21, 0x3, 0x96, 0x8d, 0xdc, 0x5d}};
static const GUID guid_fullscreen_monitor_8 = {
    0xa2208edc,
    0x3c7e,
    0x4445,
    {0xb2, 0xdc, 0xd6, 0x7e, 0x81, 0x2b, 0x40, 0x36}};
static const GUID guid_fullscreen_monitor_9 = {
    0xfd55890f,
    0x54eb,
    0x4e93,
    {0x97, 0x5f, 0xfa, 0xb7, 0xa6, 0x87, 0x43, 0x8}};
static const GUID guid_fullscreen_monitor_10 = {
    0xf17db764,
    0x2397,
    0x4e93,
    {0xaa, 0x14, 0x2, 0x11, 0xd2, 0x37, 0x9e, 0xa8}};
static const GUID guid_auto_popup = {
    0xdbd44123,
    0x702f,
    0x4cd3,
    {0xbf, 0x1c, 0xe9, 0xbe, 0xfd, 0xc5, 0xa9, 0x89}};

static mainmenu_group_popup_factory g_thumbnails_group(
    guid_thumbnails_group, mainmenu_groups::library,
    mainmenu_commands::sort_priority_base, "Video thumbnails");

static mainmenu_group_popup_factory g_player_group(
    guid_player_group, mainmenu_groups::view,
    mainmenu_commands::sort_priority_base, "mpv Commands");

class mainmenu_mpv : public mainmenu_commands {
  enum { cmd_popup = 0, cmd_total };
  t_uint32 get_command_count() override { return cmd_total; }
  GUID get_command(t_uint32 p_index) override {
    switch (p_index) {
      case cmd_popup:
        return guid_mpv_popup;
      default:
        uBugCheck();  // should never happen unless somebody called us with
                      // invalid parameters - bail
    }
  }
  void get_name(t_uint32 p_index, pfc::string_base& p_out) override {
    switch (p_index) {
      case cmd_popup:
        p_out = "mpv";
        break;
      default:
        uBugCheck();  // should never happen unless somebody called us with
                      // invalid parameters - bail
    }
  }
  bool get_description(t_uint32 p_index, pfc::string_base& p_out) override {
    switch (p_index) {
      case cmd_popup:
        p_out = "Open popup video window";
        return true;
      default:
        return false;
    }
  }
  bool get_display(t_uint32 p_index, pfc::string_base& p_text,
                   t_uint32& p_flags) override {
    switch (p_index) {
      case cmd_popup:
        p_flags = 0;
        break;
      default:
        return false;
    }
    get_name(p_index, p_text);
    return true;
  }
  GUID get_parent() override { return mainmenu_groups::view; }
  void execute(t_uint32 p_index,
               service_ptr_t<service_base> p_callback) override {
    switch (p_index) {
      case cmd_popup:
        popup_window::open();
        break;
      default:
        uBugCheck();  // should never happen unless somebody called us with
                      // invalid parameters - bail
    }
  }
};
static service_factory_single_t<mainmenu_mpv> g_mainmenu_mpv;

class mainmenu_mpv_thumbs : public mainmenu_commands {
  enum { cmd_clear, cmd_compact, cmd_removedead, cmd_total };
  t_uint32 get_command_count() override { return cmd_total; }
  GUID get_command(t_uint32 p_index) override {
    switch (p_index) {
      case cmd_clear:
        return guid_thumbs_clear;
      case cmd_compact:
        return guid_thumbs_compact;
      case cmd_removedead:
        return guid_thumbs_removedead;
      default:
        uBugCheck();  // should never happen unless somebody called us with
                      // invalid parameters - bail
    }
  }
  void get_name(t_uint32 p_index, pfc::string_base& p_out) override {
    switch (p_index) {
      case cmd_clear:
        p_out = "Remove all cached thumbnails";
        break;
      case cmd_compact:
        p_out = "Compact thumbnail database";
        break;
      case cmd_removedead:
        p_out = "Remove dead thumbnails";
        break;
      default:
        uBugCheck();  // should never happen unless somebody called us with
                      // invalid parameters - bail
    }
  }
  bool get_description(t_uint32 p_index, pfc::string_base& p_out) override {
    switch (p_index) {
      case cmd_clear:
        p_out = "Remove all thumbnails from the cache";
        return true;
      case cmd_compact:
        p_out = "Compact thumbnail database to free unused space";
        return true;
      case cmd_removedead:
        p_out = "Remove thumbnails for files which have been deleted";
        return true;
      default:
        return false;
    }
  }
  GUID get_parent() override { return guid_thumbnails_group; }
  void execute(t_uint32 p_index,
               service_ptr_t<service_base> p_callback) override {
    switch (p_index) {
      case cmd_clear:
        std::thread([]() { mpv::clear_thumbnail_cache(); }).detach();
        break;
      case cmd_compact:
        std::thread([]() { mpv::compact_thumbnail_cache(); }).detach();
        break;
      case cmd_removedead:
        std::thread([]() { mpv::clean_thumbnail_cache(); }).detach();
        break;
      default:
        uBugCheck();
    }
  }
};
static service_factory_single_t<mainmenu_mpv_thumbs> g_mainmenu_mpv_thumbs;

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor,
                                     LPRECT lprcMonitor, LPARAM dwData) {
  unsigned* count = reinterpret_cast<unsigned*>(dwData);
  (*count) = (*count) + 1;
  return true;
}

class mainmenu_mpv_playercontrol : public mainmenu_commands {
  enum {
    cmd_restart = 0,
    cmd_enable = 1,
    cmd_auto_popup = 2,
    cmd_fullscreen = 3,
    cmd_total
  };
  t_uint32 get_command_count() override {
    unsigned monitors = 0;
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc,
                        reinterpret_cast<LPARAM>(&monitors));
    unsigned monitor_count = min(10, monitors);
    return cmd_total + monitor_count;
  }
  GUID get_command(t_uint32 p_index) override {
    switch (p_index) {
      case cmd_restart:
        return guid_mpv_restart;
      case cmd_enable:
        return guid_video_enable;
      case cmd_auto_popup:
        return guid_auto_popup;
      case cmd_fullscreen:
        return guid_fullscreen;
      case cmd_fullscreen + 1:
        return guid_fullscreen_monitor_1;
      case cmd_fullscreen + 2:
        return guid_fullscreen_monitor_2;
      case cmd_fullscreen + 3:
        return guid_fullscreen_monitor_3;
      case cmd_fullscreen + 4:
        return guid_fullscreen_monitor_4;
      case cmd_fullscreen + 5:
        return guid_fullscreen_monitor_5;
      case cmd_fullscreen + 6:
        return guid_fullscreen_monitor_6;
      case cmd_fullscreen + 7:
        return guid_fullscreen_monitor_7;
      case cmd_fullscreen + 8:
        return guid_fullscreen_monitor_8;
      case cmd_fullscreen + 9:
        return guid_fullscreen_monitor_9;
      case cmd_fullscreen + 10:
        return guid_fullscreen_monitor_10;
      default:
        uBugCheck();
    }
  }
  void get_name(t_uint32 p_index, pfc::string_base& p_out) override {
    switch (p_index) {
      case cmd_restart:
        p_out = "Restart mpv";
        break;
      case cmd_enable:
        p_out = "Enable video";
        break;
      case cmd_auto_popup:
        p_out = "Automatically open popup";
        break;
      case cmd_fullscreen:
        p_out = "Toggle fullscreen";
        break;
      default:
        p_out = "Full screen (Monitor ";
        p_out << (p_index - cmd_total + 1) << ")";
    }
  }
  bool get_description(t_uint32 p_index, pfc::string_base& p_out) override {
    switch (p_index) {
      case cmd_restart:
        p_out =
            "Restarts any running mpv player instance and reloads all "
            "configuration";
        return true;
      case cmd_enable:
        return false;
      case cmd_auto_popup:
        p_out =
            "Automatically open and close the video player popup when the "
            "current track has a video stream";
        return true;
      case cmd_fullscreen:
        p_out = "Toggle fullscreen mode";
        return true;
      default:
        p_out = "Open video player full screen on monitor ";
        p_out << (p_index - cmd_total + 1);
        return true;
    }
  }
  bool get_display(t_uint32 p_index, pfc::string_base& p_text,
                   t_uint32& p_flags) override {
    p_flags = flag_defaulthidden;
    if (p_index == cmd_enable && cfg_video_enabled) {
      p_flags |= flag_checked;
    }
    if (p_index == cmd_auto_popup && cfg_autopopup) {
      p_flags |= flag_checked;
    }
    get_name(p_index, p_text);
    return true;
  }
  GUID get_parent() override { return guid_player_group; }
  void execute(t_uint32 p_index,
               service_ptr_t<service_base> p_callback) override {
    switch (p_index) {
      case cmd_restart:
        mpv::player::restart();
        break;
      case cmd_enable:
        cfg_video_enabled = !cfg_video_enabled;
        mpv::player::on_containers_change();
        break;
      case cmd_auto_popup:
        cfg_autopopup = !cfg_autopopup;
        break;
      case cmd_fullscreen:
        mpv::player::toggle_fullscreen();
        break;
      default:
        mpv::player::fullscreen_on_monitor(p_index - cmd_total);
    }
  }
};
static service_factory_single_t<mainmenu_mpv_playercontrol>
    g_mainmenu_mpv_playercontrol;

class video_enable_button : public uie::button_v2 {
  const GUID& get_item_guid() const { return guid_video_enable; };
  uie::t_button_guid get_guid_type() const {
    return uie::BUTTON_GUID_MENU_ITEM_MAIN;
  }
  unsigned get_button_state() const {
    return cfg_video_enabled ? uie::BUTTON_STATE_PRESSED
                             : uie::BUTTON_STATE_DEFAULT;
  }

  HANDLE get_item_bitmap(unsigned command_state_index, COLORREF cr_btntext,
                         unsigned cx_hint, unsigned cy_hint,
                         unsigned& handle_type) const override {
    auto icon = (HICON)LoadImage(core_api::get_my_instance(),
                                 MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
                                 cx_hint, cy_hint, NULL);
    handle_type = uie::button_v2::handle_type_icon;
    return (HANDLE)icon;
  }
};
uie::button_factory<video_enable_button> g_cui_video_enabled_button;
}  // namespace mpv

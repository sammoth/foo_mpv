#include "stdafx.h"
// PCH ^

#include "thumbnailer.h"

void RunMpvPopupWindow();

namespace {
static const GUID guid_mainmenu_group = {
    0x41ede16, 0xaf85, 0x408d, {0x88, 0xcc, 0x4, 0x5f, 0xcb, 0x29, 0x62, 0x88}};

static const GUID guid_mpv_popup = {
    0xfd1d7b8c,
    0xeb77,
    0x42b5,
    {0x93, 0x85, 0x2c, 0x95, 0xd3, 0xe6, 0x71, 0xba}};
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
static const GUID guid_thumbs_regenerate = {
    0xbf193e5e,
    0xf7fd,
    0x408c,
    {0xaf, 0x21, 0x1e, 0x5e, 0x9f, 0xf9, 0x42, 0x12}};

static mainmenu_group_popup_factory g_mainmenu_group(
    guid_mainmenu_group, mainmenu_groups::library,
    mainmenu_commands::sort_priority_dontcare, "Video thumbnails");

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
  GUID get_parent() override { return mainmenu_groups::view; }
  void execute(t_uint32 p_index,
               service_ptr_t<service_base> p_callback) override {
    switch (p_index) {
      case cmd_popup:
        RunMpvPopupWindow();
        break;
      default:
        uBugCheck();  // should never happen unless somebody called us with
                      // invalid parameters - bail
    }
  }
};
static service_factory_single_t<mainmenu_mpv> g_mainmenu_mpv;

class mainmenu_mpv_thumbs : public mainmenu_commands {
  enum { cmd_clear, cmd_compact, cmd_removedead, cmd_regenerate, cmd_total };
  t_uint32 get_command_count() override { return cmd_total; }
  GUID get_command(t_uint32 p_index) override {
    switch (p_index) {
      case cmd_clear:
        return guid_thumbs_clear;
      case cmd_compact:
        return guid_thumbs_compact;
      case cmd_removedead:
        return guid_thumbs_removedead;
      case cmd_regenerate:
        return guid_thumbs_regenerate;
      default:
        uBugCheck();  // should never happen unless somebody called us with
                      // invalid parameters - bail
    }
  }
  void get_name(t_uint32 p_index, pfc::string_base& p_out) override {
    switch (p_index) {
      case cmd_clear:
        p_out = "Clear thumbnail database";
        break;
      case cmd_compact:
        p_out = "Compact thumbnail database";
        break;
      case cmd_removedead:
        p_out = "Remove dead thumbnails";
        break;
      case cmd_regenerate:
        p_out = "Regenerate all thumbnails";
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
      case cmd_regenerate:
        p_out =
            "Regenerate thumbnails for all items in the database using current "
            "thumbnail settings";
        return true;
      default:
        return false;
    }
  }
  GUID get_parent() override { return guid_mainmenu_group; }
  void execute(t_uint32 p_index,
               service_ptr_t<service_base> p_callback) override {
    switch (p_index) {
      case cmd_clear:
        mpv::clear_thumbnail_cache();
        break;
      case cmd_compact:
        mpv::compact_thumbnail_cache();
        break;
      case cmd_removedead:
        mpv::clean_thumbnail_cache();
        break;
      case cmd_regenerate:
        mpv::regenerate_thumbnail_cache();
        break;
      default:
        uBugCheck();  // should never happen unless somebody called us with
                      // invalid parameters - bail
    }
  }
};
static service_factory_single_t<mainmenu_mpv_thumbs> g_mainmenu_mpv_thumbs;
}  // namespace

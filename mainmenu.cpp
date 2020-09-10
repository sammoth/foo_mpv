#include "stdafx.h"
// PCH ^

static const GUID guid_mainmenu_mpv_group = {
    0x7ea6c308, 0x1429, 0x47f4, {0xb1, 0x8e, 0x61, 0x5, 0xbb, 0x2e, 0x0, 0x51}};
static const GUID guid_mpv_popup = {
    0xfd1d7b8c,
    0xeb77,
    0x42b5,
    {0x93, 0x85, 0x2c, 0x95, 0xd3, 0xe6, 0x71, 0xba}};

static mainmenu_group_popup_factory g_mainmenu_group(
    guid_mainmenu_mpv_group, mainmenu_groups::view,
    mainmenu_commands::sort_priority_dontcare, "mpv");

void RunMpvPopupWindow();

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
        p_out = "Video";
        break;
      default:
        uBugCheck();  // should never happen unless somebody called us with
                      // invalid parameters - bail
    }
  }
  bool get_description(t_uint32 p_index, pfc::string_base& p_out) override {
    switch (p_index) {
      case cmd_popup:
        p_out = "Open standalone video window";
        return true;
      default:
        return false;
    }
  }
  GUID get_parent() override { return guid_mainmenu_mpv_group; }
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

static mainmenu_commands_factory_t<mainmenu_mpv>
    g_mainmenu_commands_sample_factory;

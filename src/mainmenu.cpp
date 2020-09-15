#include "stdafx.h"
// PCH ^

void RunMpvPopupWindow();

namespace {
static const GUID guid_mpv_popup = {
    0xfd1d7b8c,
    0xeb77,
    0x42b5,
    {0x93, 0x85, 0x2c, 0x95, 0xd3, 0xe6, 0x71, 0xba}};

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
}  // namespace

static service_factory_single_t<mainmenu_mpv> g_mainmenu_mpv;

#pragma once

/******************************************************************************

Component uses file_info to exchange information about Sources and updates.
Meta fields are user-visible and can affect acfu behavior. Info fields are
retained but unused by acfu, so Source can use they for its own needs (e.g.
GitHub helper (./utils/github.h) stores there JSON of releases and assets.

Recognized meta fields (none of them are required):

"name" - name of the Source to be displayed in Sources list. Source GUID is
displayed there if name is not provided.

"module" - component providing this Source. Particularly useful if Source does
not represent a component itself.

"version" - version (current or latest). Note, acfu does not decide if updates
are available basing on version (it is up to source::is_newer(), and it can use
any other way, not involving "version") but acfu will try to display version if
updates are available.

"download_page" - URL to download page. If provided, Source context menu has
item "Go to Download Page".

"download_url" - URL for Source download. If provided, Source context menu has
item "Download in Browser".

Also all meta fields are displayed by "Properties" context menu item.

Context menu of the Source is also can be extended using context_menu_build()
and context_menu_command() methods.

******************************************************************************/

namespace acfu {

class NOVTABLE authorization : public service_base {
  FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(authorization);

 public:
  virtual void authorize(const char* url, http_request::ptr request,
                         abort_callback& abort) = 0;
};

class NOVTABLE request : public service_base {
  FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(request);

 public:
  virtual void run(file_info& info, abort_callback& abort) = 0;
};

class NOVTABLE source : public service_base {
  FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(source);

 public:
  virtual GUID get_guid() = 0;
  virtual void get_info(file_info& info) = 0;
  virtual bool is_newer(const file_info& info) = 0;
  virtual request::ptr create_request() = 0;

  virtual void context_menu_build(HMENU menu, unsigned id_base) {}
  virtual void context_menu_command(unsigned id, unsigned id_base) {}

  static ptr g_get(const GUID& guid);
};

class NOVTABLE updates : public service_base {
  FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(updates);

 public:
  class callback {
   public:
    virtual void on_info_changed(const GUID& guid, const file_info& info) {}
    virtual void on_updates_available(const pfc::list_base_const_t<GUID>& ids) {
    }
  };

  // Should be called from main thread.
  // Being invoked, may call callback::on_updates_available()
  virtual void register_callback(callback* callback) = 0;
  // Should be called from main thread
  virtual void unregister_callback(callback* callback) = 0;

  virtual bool get_info(const GUID& guid, file_info& info) = 0;
  virtual void set_info(const GUID& guid, const file_info& info) = 0;
};

// {3C245A11-6EAE-4742-929F-1D02BC513C46}
FOOGUIDDECL const GUID authorization::class_guid = {
    0x3c245a11,
    0x6eae,
    0x4742,
    {0x92, 0x9f, 0x1d, 0x2, 0xbc, 0x51, 0x3c, 0x46}};

// {4E88EA57-ABDD-49AD-B72B-7C198DA27DBE}
FOOGUIDDECL const GUID request::class_guid = {
    0x4e88ea57,
    0xabdd,
    0x49ad,
    {0xb7, 0x2b, 0x7c, 0x19, 0x8d, 0xa2, 0x7d, 0xbe}};

// {9A5442D9-77F9-4918-BAE3-F9D059F4681B}
FOOGUIDDECL const GUID source::class_guid = {
    0x9a5442d9,
    0x77f9,
    0x4918,
    {0xba, 0xe3, 0xf9, 0xd0, 0x59, 0xf4, 0x68, 0x1b}};

// {556AA4ED-471F-4324-A386-68502391A830}
FOOGUIDDECL const GUID updates::class_guid = {
    0x556aa4ed,
    0x471f,
    0x4324,
    {0xa3, 0x86, 0x68, 0x50, 0x23, 0x91, 0xa8, 0x30}};

inline source::ptr source::g_get(const GUID& guid) {
  service_enum_t<source> e;
  for (ptr p; e.next(p);) {
    if (p->get_guid() == guid) {
      return p;
    }
  }
  throw pfc::exception_invalid_params(PFC_string_formatter()
                                      << "unregistered source: "
                                      << pfc::print_guid(guid));
}

}  // namespace acfu

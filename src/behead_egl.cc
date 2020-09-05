/*
 * SPDX-FileCopyrightText: 2020 Piotr Rak <piotr.rak@gamil.com>
 * SPDX-License-Identifier: MIT
 */
#include "bhd/behead_egl.hh"

#include "minidrm.hh"
#include "tokenize_sv.hh"

#include <atomic>
#include <cassert>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

#include <iostream>

// We don't want X11 headers
#define EGL_NO_X11
#include <EGL/eglplatform.h>
#include <EGL/egl.h>

#include <EGL/eglext.h>

namespace bhdi = behead_egl::internal;
namespace bhd = behead_egl;

namespace {

bool has_extension(std::string_view extensions, std::string_view ext) noexcept
{
   using bhdi::foreach_token_sv;

   bool ext_found = false;

   // XXX: strstr() + strlen() + check for ' ' or '\0' at after token
   // reads memory once, this does it twice.
   // Ie. oldschool C approach is better slightly beterr, but i don't care.
   foreach_token_sv(extensions, ' ', [=, &ext_found] (auto checked) mutable {
       ext_found = (ext == checked);
       return ext_found;
   });

   return ext_found;
}

using list_sv = std::initializer_list<std::string_view>;

bool has_all_extensions(std::string_view ext_str, list_sv extensions) noexcept
{
   bool all_ext = true;

   // NB: brute-force O(n*m)
   for (auto e : extensions)
   {
      all_ext = all_ext && has_extension(ext_str, e);

      if (!all_ext) break;
   }

   return all_ext;
}

void debug_report_first_missing(std::string_view ext_str, list_sv ext_list)
{
#ifndef NDEBUG
   for (auto ext : ext_list)
   {
      if (!has_extension(ext_str, ext))
      {
         std::cerr << "Missing: " << ext << std::endl;
         break;
      }
   }
#endif
}

struct runtime_egl_error : public std::runtime_error
{
   template <typename... ArgsTy_>
   runtime_egl_error(ArgsTy_... args):
      std::runtime_error(args...), egl_error(eglGetError()) {}

   EGLint egl_error;
};

using std::runtime_error;

template <typename FnTy_>
bool set_egl_proc(FnTy_ &fn, const char *proc_name)
{
   fn = reinterpret_cast<FnTy_>((void*)eglGetProcAddress(proc_name));
   return fn != nullptr;
}

template <typename...Args>
inline bool all(Args... args)
{
   return (... && args);
}

} // namespace anonymous

using bhd::DeviceEXT_Info;
using VecDevEXT = std::vector<EGLDeviceEXT>;
using VecDevInfos = std::vector<DeviceEXT_Info>;

// This selects the first found CUDA device that supports EGL_EXT_device_drm
// If not found first non-CUDA device with EGL_EXT_device_drm
//
// maybe TODO: more control over selection strategy
static const DeviceEXT_Info*
pick_display_device_ext(const VecDevInfos &device_infos)
{
   const DeviceEXT_Info *first_with_cuda = nullptr;
   const DeviceEXT_Info *first_with_drm = nullptr;

   std::size_t cuda_dev_count = 0;
   std::size_t drm_dev_count = 0;

   for (const auto &cap : device_infos)
   {
      // Count as CUDA device
      if (cap.has_NV_device_cuda && cap.has_EXT_device_drm)
      {
         // Remember first one
         if (first_with_cuda == nullptr)
            first_with_cuda = &cap;

         ++cuda_dev_count;
      }

      // Count as DRM device
      if (cap.has_EXT_device_drm)
      {
         // Remember first one
         if (first_with_drm == nullptr)
            first_with_drm = &cap;

         ++drm_dev_count;
      }
   }

   const DeviceEXT_Info *picked_dev = nullptr;

   (void) drm_dev_count;
   (void) cuda_dev_count;

   picked_dev = first_with_cuda;

   if (picked_dev == nullptr)
      picked_dev = first_with_drm;

   return picked_dev;
}

struct BeheadEGL final
{
   // Public API
   static bool check_support();
   static EGLDisplay create_headless_display();

public:
   // NB: This is not class, it is a module.
   // It's declared as class for convinience of providing scope for EGL extension
   // functions.

   BeheadEGL() = delete;
   ~BeheadEGL() = delete;

   BeheadEGL(const BeheadEGL &) = delete;
   BeheadEGL(BeheadEGL &&) = delete;

   BeheadEGL operator=(const BeheadEGL &) = delete;
   BeheadEGL operator=(BeheadEGL &&) = delete;
private:
   // {{{ EGL extension functions setup

   // NB: Called only with _do_init_egl_client_procs once_flag
   static void _do_init_egl_client_procs(bool (*_assert_caller)());

   static bool _ensure_client_extensions();

   // }}}

   /// {{{ EGLDeviceEXT enumeration and extensions query

   static VecDevEXT _enumerate_devices_ext();

   static DeviceEXT_Info _query_device_info(EGLDeviceEXT dev_ext);

   static VecDevInfos _collect_device_ext_infos(const VecDevEXT &devices);

   /// }}}

   // Creates platform_device EGLDisplay using file descriptor for device dev
   //
   // may throw runtime_egl_error
   static EGLDisplay _create_platform_device_display_fd(const bhdi::unique_fd &fd, EGLDeviceEXT dev);

private:
   // Protects EGL extension function pointers
   static inline std::once_flag _client_egl_procs_flag;

   // Since we also assert on this variable in debug mode, some of our static functions
   // wouldn't be thread-safe.
   static inline std::atomic_bool _client_procs_ok = false;

   // EXT_device_enumeration
   static inline PFNEGLQUERYDEVICESEXTPROC _eglQueryDevicesEXT = nullptr;

   // EXT_device_query
   static inline PFNEGLQUERYDEVICEATTRIBEXTPROC _eglQueryDeviceAttribEXT = nullptr;
   static inline PFNEGLQUERYDEVICESTRINGEXTPROC _eglQueryDeviceStringEXT = nullptr;

   // EGL_EXT_platform_base
   static inline PFNEGLGETPLATFORMDISPLAYEXTPROC _eglGetPlatformDisplayEXT = nullptr;

   // EGL client extensions that are mandatory for us.
   static constexpr list_sv EXT_CLIENT_REQUIRED = {
      "EGL_EXT_platform_base",
      "EGL_EXT_device_base",
      "EGL_EXT_device_query",
      "EGL_EXT_device_enumeration",
      "EGL_EXT_platform_device",
   };
};

void BeheadEGL::_do_init_egl_client_procs(bool (*_assert_caller)())
{
   assert(_assert_caller == &_ensure_client_extensions);
   (void) _assert_caller;

   // Check if world is happy place and we talk to EGL 1.5 or better
   // and we can query client extensions.
   const char *client_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

   // We can't obtain extensions EGL client extension
   if (client_extensions == nullptr)
   {
      // NB: we don't have to carry egl procedure stores, since we failed.
      _client_procs_ok.store(false, std::memory_order_relaxed);
      return;
   }
   // Check for all mandatory extensions for it to work
   bool all_client_required = has_all_extensions(client_extensions, EXT_CLIENT_REQUIRED);

   if (!all_client_required)
   {
       // NB: we don't have to carry egl procedure stores, since we failed.
       _client_procs_ok.store(false, std::memory_order_relaxed);
       debug_report_first_missing(client_extensions, EXT_CLIENT_REQUIRED);
       return;
   }

   bool ok = all(
#define _egl_proc(name) set_egl_proc(_ ## name, #name)
      _egl_proc(eglQueryDevicesEXT),
      _egl_proc(eglQueryDeviceAttribEXT),
      _egl_proc(eglQueryDeviceStringEXT),
      _egl_proc(eglGetPlatformDisplayEXT)
#undef _egl_proc
   );

   // we carry dependency one _set_egl_proc stores
   _client_procs_ok.store(ok, std::memory_order_release);
}

bool BeheadEGL::_ensure_client_extensions()
{
   // This is one way route, if we fail to query and initialize here,
   // it is game over for us.
   std::call_once(_client_egl_procs_flag,
                  _do_init_egl_client_procs,
                  &_ensure_client_extensions);

   return _client_procs_ok.load(std::memory_order_acquire);
}

VecDevEXT BeheadEGL::_enumerate_devices_ext()
{
   assert(_client_procs_ok); // NB: mo:acquire would suffice

   EGLint num_devices = 0;

   // Count number of devices
   if (_eglQueryDevicesEXT(0, nullptr, &num_devices) != EGL_TRUE)
      throw runtime_egl_error("Failed to enumerate available EGLDeviceEXT");

   // Spec says implementation should provide at least :
   // see (EXT_device_enumeration)
   // [https://www.khronos.org/registry/EGL/extensions/EXT/EGL_EXT_device_enumeration.txt]
   if (num_devices == 0)
      throw std::runtime_error("No available EGLDeviceEXT's ");

   // Allocate space for devices
   VecDevEXT devices_ext{std::size_t(num_devices), nullptr};

   if (_eglQueryDevicesEXT(devices_ext.size(), devices_ext.data(), &num_devices) != EGL_TRUE)
       throw runtime_egl_error("Failed to enumerate available EGLDeviceEXT.");

   devices_ext.resize(num_devices);

   return devices_ext;
}

DeviceEXT_Info BeheadEGL::_query_device_info(EGLDeviceEXT dev_ext)
{
   assert(_client_procs_ok);
   assert(dev_ext);

   const char *extensions = _eglQueryDeviceStringEXT(dev_ext, EGL_EXTENSIONS);

   // This device is useless for us, lets continue
   if (extensions == nullptr)
      throw runtime_egl_error("Failed to query EGLDeviceEXT extensions.");

   DeviceEXT_Info info;

   info.egl_device_ext = dev_ext;
   info.device_extensions = extensions;

   info.has_NV_device_cuda = has_extension(extensions, "EGL_NV_device_cuda");
   info.has_EXT_device_drm = has_extension(extensions, "EGL_EXT_device_drm");
   info.has_MESA_device_software = has_extension(extensions, "EGL_MESA_device_software");

   if (info.has_EXT_device_drm)
   {
      const char *drm_path = _eglQueryDeviceStringEXT(dev_ext, EGL_DRM_DEVICE_FILE_EXT);

      // EGL_EXT_device_drm extentions contract is violated!
      // Something really wrong going on here, but lets just ignore this device.
      if (drm_path == nullptr)
         throw runtime_egl_error("Failed to obtain device drm path from EGL!");

      info.drm_path = drm_path;
   }

   if (info.has_NV_device_cuda)
   {
      int cuda_id = -1;

      EGLAttrib *cuda_attrib = reinterpret_cast<EGLAttrib*>(&cuda_id);

      if(_eglQueryDeviceAttribEXT(dev_ext, EGL_CUDA_DEVICE_NV, cuda_attrib) != EGL_TRUE)
         throw runtime_egl_error("Failed to query CUDA device id attribute.");

      info.cuda_dev_id = cuda_id;
   }

   return info;
}

VecDevInfos BeheadEGL::_collect_device_ext_infos(const VecDevEXT &devices)
{
   VecDevInfos device_infos;

   unsigned count = 0;

   for (auto dev_ext : devices)
   {
      ++count;

      try
      {
         auto info = _query_device_info(dev_ext);
         device_infos.push_back(info);
      }
      catch (const runtime_egl_error &e)
      {
          std::cerr << "Failed to query device capabilities" << std::endl;
          std::cerr << e.what() << " (EGLError: " << e.egl_error << ")" << std::endl;

          // Skipping device, those are might be API violations
          // Advertised extension did not return result;
          assert(false && "Bogus EGLDeviceEXT from EGL");

          // TODO: WARN or rethrow?
          continue;
      }
   }

   assert(count == device_infos.size());
   (void)count;

   return device_infos;
}

EGLDisplay
BeheadEGL::_create_platform_device_display_fd(const bhdi::unique_fd& fd, EGLDeviceEXT dev)
{
   assert(_client_procs_ok);

   assert(dev != nullptr);
   assert(fd >= 0);

   EGLint attribs[] = { EGL_DRM_MASTER_FD_EXT, fd.get(), EGL_NONE };

   EGLDisplay dpy = _eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, dev, attribs);

   if (dpy == EGL_NO_DISPLAY)
      throw runtime_egl_error("Failed to create platform display.");

   return dpy;
}

bool BeheadEGL::check_support()
{
   try
   {
      return _ensure_client_extensions();
   }
   catch (...)
   {
      // TODO: something very wrong
   }

   return false;
}

EGLDisplay BeheadEGL::create_headless_display()
{
   if (!_ensure_client_extensions())
       return EGL_NO_DISPLAY;

   VecDevEXT devices;
   VecDevInfos device_infos;

   try
   {
      // Enumerate all EGLDeviceEXT
      // see EXT_device_enumeration
      devices = _enumerate_devices_ext();

      // Collect capabilites of those devices
      device_infos = _collect_device_ext_infos(devices);
   }
   catch (const runtime_egl_error &e)
   {
      // WARNING
      std::cerr << "Couldn't query any device capabilities" << std::endl;
      std::cerr << e.what() << " (EGLError: " << e.egl_error << ")" << std::endl;
      return EGL_NO_DISPLAY;
   }
   catch (...)
   {
      // WARNING
      std::cerr << "Couldn't query any device capabilities" << std::endl;
      return EGL_NO_DISPLAY;
   }

   const DeviceEXT_Info *picked = pick_display_device_ext(device_infos);

   if (picked == nullptr)
   {
      // ERROR
      std::cerr << "Couldn't find suitable EGLDeviceEXT" << std::endl;
      return EGL_NO_DISPLAY;
   }

   EGLDisplay dpy = EGL_NO_DISPLAY;

   //bhdi::unique_fd render_fd;
   //bhdi::unique_fd primary_fd;

   try
   {
      auto nodes = bhdi::open_drm_nodes(picked->drm_path);

      //render_fd = std::move(nodes.render_fd);
      //primary_fd = std::move(nodes.primary_fd);

      assert(nodes.ok());

      EGLDeviceEXT device = picked->egl_device_ext;

      assert(device);

      try
      {
         // Try create display for render node
         dpy = _create_platform_device_display_fd(nodes.render_fd, device);
      }
      catch (const runtime_egl_error &e)
      {
         // WARNING
         std::cerr << "Failed to create EGLDisplay for render node" << std::endl;
         std::cerr << e.what() << " (EGLError: " << e.egl_error << ")" << std::endl;

         try
         {
            // Retry for primary node
            dpy = _create_platform_device_display_fd(nodes.primary_fd, device);
         }
         catch (const runtime_egl_error &e)
         {
            std::cerr << "Failed to create EGLDisplay for primary node" << std::endl;
            std::cerr << e.what() << " (EGLError: " << e.egl_error << ")" << std::endl;

            // Give up
            return EGL_NO_DISPLAY;
         }
      }
   }
   catch (const runtime_error &e)
   {
      std::cerr << "Failed to create EGLDisplay" << std::endl;
      std::cerr << e.what() << std::endl;

      return EGL_NO_DISPLAY;
   }

   assert(dpy != EGL_NO_DISPLAY);

   return dpy;
}

namespace behead_egl
{

bool check_headless_display_support()
{
   return BeheadEGL::check_support();
}

EGLDisplay create_headless_display()
{
   try
   {
      return BeheadEGL::create_headless_display();
   }
   catch (const runtime_egl_error &e)
   {
      assert(false && "Leaked exception");
   }
   catch (const runtime_error &e)
   {
      assert(false && "Leaked exception");
   }

   return EGL_NO_DISPLAY;
}

} // namespace behead_egl


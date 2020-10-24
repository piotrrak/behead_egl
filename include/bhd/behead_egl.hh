/*
 * SPDX-FileCopyrightText: 2020 Piotr Rak <piotr.rak@gamil.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef BEHEAD_EGL_include_bhd_behead_egl_hh_included_
#define BEHEAD_EGL_include_bhd_behead_egl_hh_included_ 1

#include <memory>
#include <functional>
#include <optional>

#define EGL_NO_X11
#define MESA_EGL_NO_X11_HEADERS
#include <EGL/eglplatform.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#undef MESA_EGL_NO_X11_HEADERS
#undef EGL_NO_X11

#define BHD_EXPORT [[gnu::visibility("default")]]

namespace behead_egl
{

enum class DrmNodeUsage
{
   UsePrimary,
   UseRender,
   UsePrimaryFallbackToRender,
   UseRenderFallbackToPrimary,
};

constexpr DrmNodeUsage DefaultDrmNodeUsage = DrmNodeUsage::UseRenderFallbackToPrimary;

enum class EnumerateOpt
{
   All,
   Usable,
};

const EnumerateOpt DefaultEnumerateOpt = EnumerateOpt::All;

struct DeviceEXT_Info
{
private:
   using opt_int = std::optional<int>;

public:
   EGLDeviceEXT egl_device_ext            = nullptr;

   const char  *device_extensions         = nullptr;

   bool         has_NV_device_cuda        = false;
   bool         has_EXT_device_drm        = false;
   bool         has_MESA_device_software  = false;

   const char  *drm_path                  = nullptr;
   opt_int      cuda_dev_id               = std::nullopt;
};


using device_enumeration_cb_t = std::function<void (const DeviceEXT_Info &)>;


BHD_EXPORT bool check_headless_display_support();

BHD_EXPORT EGLDisplay create_headless_display(DrmNodeUsage = DefaultDrmNodeUsage);

BHD_EXPORT bool enumerate_display_devices(const device_enumeration_cb_t &cb, EnumerateOpt = DefaultEnumerateOpt);

// BEWARE: This function has very long name for a reason!
// It may ever work for EGLDisplay initialized by eglInitialize() and before eglTerminate().
// See EGL_EXT_device_query specification eglQueryDisplayAttribEXT for more details.
BHD_EXPORT DeviceEXT_Info get_initialized_display_device_info(EGLDisplay dpy);

}

#endif // !defined(BEHEAD_EGL_include_bhd_behead_egl_hh_included_)

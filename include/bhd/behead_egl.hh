/*
 * SPDX-FileCopyrightText: 2020 Piotr Rak <piotr.rak@gamil.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <memory>
#include <optional>

#define EGL_NO_X11
#include <EGL/eglplatform.h>
#include <EGL/egl.h>

#include <EGL/eglext.h>

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

BHD_EXPORT bool check_headless_display_support();

BHD_EXPORT EGLDisplay create_headless_display(DrmNodeUsage = DefaultDrmNodeUsage);

}

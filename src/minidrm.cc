/*
 * SPDX-FileCopyrightText: 2020 Piotr Rak <piotr.rak@gamil.com>
 * SPDX-License-Identifier: MIT
 */

#include "minidrm.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysmacros.h>

#include <array>
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <utility>

#include <iostream>

#ifndef __linux__
#error "Not implemented for other platforms"
#endif

namespace bhdi = behead_egl::internal;

namespace {

template <std::size_t Sz_>
using buffer = std::array<char, Sz_>;

template <std::size_t Sz_>
buffer<Sz_> bprintf(const char *fmt, ...)
{
   va_list args;
   buffer<Sz_> buffer;

   va_start(args, fmt);
   vsnprintf(buffer.data(), Sz_, fmt, args);
   va_end(args);

   return buffer;
}

// It must be directory, closed on exec, it is only for a path.
constexpr int DIR_OPEN_FLAGS = O_PATH | O_DIRECTORY | O_CLOEXEC;

// Closed on exec, gpu driver will ioctl it, thus open read write
constexpr int NODE_OPEN_FLAGS = O_RDWR | O_CLOEXEC;

// Path to drm device directory
constexpr const char *DRM_DIR = "/dev/dri/";

struct DeviceId
{
   unsigned _major;
   unsigned _minor;

   static DeviceId from_stat(struct stat st)
   {
      return {major(st.st_rdev), minor(st.st_rdev)};
   }
};

buffer<64> make_sysfs_path(DeviceId id)
{
   return bprintf<64>("/sys/dev/char/%d:%d/device/drm/", id._major, id._minor);
}

buffer<16> make_drm_path(bhdi::DrmNodeFlag f, unsigned _minor)
{
   using bhdi::DrmNodeFlag;

   assert(f == DrmNodeFlag::Primary || f == DrmNodeFlag::Render);

   switch (f)
   {
   case bhdi::DrmNodeFlag::Primary:
      return bprintf<16>("card%d", _minor);
   case DrmNodeFlag::Render:
      return bprintf<16>("renderD%d", _minor + 128);

   case DrmNodeFlag::None:
      break;
   };

   throw std::logic_error("Invalid argument make_drm_path");
}

}

namespace behead_egl::internal {

const char* to_string(DrmNodeFlag f)
{
   switch (f)
   {
   case DrmNodeFlag::Primary:
      return "primary";
   case DrmNodeFlag::Render:
      return "render";
   case BothDrmNodes:
      return "both";
   case DrmNodeFlag::None:
      break;
   }

   return "";
}

DrmNodeFds open_drm_nodes(const char *dev, DrmNodeFlag nodes)
{
   using namespace std::literals::string_literals;
   using std::runtime_error;

   // No point calling it without any node specified
   assert(has<DrmNodeFlag::Primary>(nodes) || has<DrmNodeFlag::Render>(nodes));

   // ::stat() the char device to ensure:
   // - ensure it exits
   // - ensure it is special character device
   // - obtain its major and minor device number
   struct stat st;

   int ret = ::stat(dev, &st);

   if (ret != 0)
      throw std::runtime_error("Couldn't stat "s + dev);

   if (!S_ISCHR(st.st_mode))
      throw runtime_error("Device "s + dev +" isn't character device");

   DeviceId dev_id = DeviceId::from_stat(st);

   // Each drm device shoud have sysfs entry based on its major and minor device number:
   auto sys_path = make_sysfs_path(dev_id);

   // Open this directory; then we can easily access card{minor} and renderD{minor+128}
   // its subdirectories
   unique_fd sys_drm_dir{::open(sys_path.data(), DIR_OPEN_FLAGS , 0)};

   // If this fails it is not drm device
   if (!sys_drm_dir.ok())
      throw runtime_error("Failed to open sysfs for "s + dev);

   // Easy access to /dev/dri directory.
   unique_fd dev_drm_dir{::open(DRM_DIR, DIR_OPEN_FLAGS, 0)};

   if (!dev_drm_dir.ok())
      throw runtime_error("Failed to open drm directory");

   auto access_sysfs_open_dri = [&] (const char *node_name) {
      // If we can access entry in sysfs
      if (::faccessat(sys_drm_dir.get(), node_name, F_OK, 0) != 0)
         throw runtime_error("Failed to access "s + sys_path.data() + node_name);

      // It is safe to open node (primary or render node).
      unique_fd node_fd{::openat(dev_drm_dir.get(), node_name, NODE_OPEN_FLAGS, 0)};

      if (!node_fd.ok())
         throw runtime_error("Failed to open "s + DRM_DIR + node_name);

      return node_fd;
   };

   DrmNodeFds result;

   if (has<DrmNodeFlag::Primary>(nodes))
   {
      auto dev_card = make_drm_path(DrmNodeFlag::Primary, dev_id._minor);
      // Check for /sys/dev/char/<maj>:<min>/device/drm/card<min>
      // Open /dev/dri/card<min
      auto tmp_primary = access_sysfs_open_dri(dev_card.data());
      assert(tmp_primary.ok());
      result.primary_fd = std::move(tmp_primary);
   }

   if (has<DrmNodeFlag::Render>(nodes))
   {
      auto dev_render = make_drm_path(DrmNodeFlag::Render, dev_id._minor);

      // Check for /sys/dev/char/<maj>:<min>/device/drm/renderD<min+128>
      // Open /dev/dri/renderD<min+128>
      auto tmp_render = access_sysfs_open_dri(dev_render.data());
      assert(tmp_render.ok());
      result.render_fd = std::move(tmp_render);
   }

   return result;
}

} // namespace behead_egl::internal

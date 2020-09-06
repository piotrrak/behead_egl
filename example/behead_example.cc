#include <cassert>
#include <cstddef>
#include <iostream>

#include <bhd/behead_egl.hh>

namespace bhd = behead_egl;

int main()
{
   std::cout << "Available devices are: " << std::endl;

   bool ok = bhd::enumerate_display_devices(
      [num=0] (const bhd::DeviceEXT_Info &info) mutable {

         std::cout << "Card #" << num++ << std::endl;
         std::cout << "\tsupports: " << info.device_extensions << std::endl;
   });

   if (ok)
      std::cout << std::endl;
   else
      std::cerr << "Failed to enumerate available cards" << std::endl;

   EGLDisplay dpy1 = bhd::create_headless_display();

   assert(dpy1 != EGL_NO_DISPLAY);

   EGLDisplay dpy2 = bhd::create_headless_display(bhd::DrmNodeUsage::UsePrimary);
   assert(dpy2 != EGL_NO_DISPLAY);

   int egl_major, egl_minor;
   if (!eglInitialize(dpy1, &egl_major, &egl_minor))
   {
      std::cerr << "Failed to initialize EGLDisplay" << std::endl;
      return 1;
   }

   std::cout << "EGL initialized" << std::endl;

   auto info = bhd::get_initialized_display_device_info(dpy1);

   if (info.egl_device_ext == EGL_NO_DEVICE_EXT)
   {
      std::cerr << "Failed to query device info for EGLDisplay" << std::endl;
   }
   else
   {
      if (info.has_NV_device_cuda)
      {
         std::cout << "Device associated with this EGLDisplay"
                      " supports CUDA (id: " << info.cuda_dev_id.value() << ")" << std::endl;
      }
      else
      {
         std::cout << "Device associated with this EGLDisplay doesn't support CUDA" << std::endl;
      }
   }


   eglTerminate(dpy1);

   std::cout << "EGL terminated" << std::endl;
}


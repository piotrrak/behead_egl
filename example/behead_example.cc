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

   eglTerminate(dpy1);

   std::cout << "EGL terminated" << std::endl;
}


#include <cassert>
#include <cstddef>
#include <iostream>

#include <bhd/behead_egl.hh>

namespace bhd = behead_egl;

int main()
{
   //EGLDisplay dpy1 = bhd::create_headless_display();
   EGLDisplay dpy2 = bhd::create_headless_display();

   //assert(dpy1 != EGL_NO_DISPLAY);
   assert(dpy2 != EGL_NO_DISPLAY);

   int egl_major, egl_minor;
   if (!eglInitialize(dpy2, &egl_major, &egl_minor))
   {
      std::cerr << "Failed to initialize EGLDisplay" << std::endl;
      return 1;
   }

   std::cout << "EGL initialized" << std::endl;

   eglTerminate(dpy2);

   std::cout << "EGL terminated" << std::endl;
}


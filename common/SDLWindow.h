
#ifndef __SDL_WINDOW_H__
#define __SDL_WINDOW_H__ 1

// OpenGL include
#include <GL/glx.h>
#include <GL/gl.h>

// SDL2 include
#include <SDL2/SDL.h>

#include <GlobalTypes.h>

namespace SE {

struct WindowSettings {

  int32_t     width,
              height;
  int32_t     bpp;
  //std::string sTitle;
  std::string title;
  bool        fullscreen;
  bool        vsync;
};

template <class ResizeHandler,  class DrawHandler> class SDLWindow {

  SDL_Window          * pWindow;
  SDL_GLContext         pGLContext;

  WindowSettings        oSettings;

  ResizeHandler      & oResizeHandler;
  DrawHandler        & oDrawHandler;


  void CreateWindow();
  //void DestroyWindow();

  public:

  // resize handler, draw handler, settings
  SDLWindow(ResizeHandler & oNewResizeHandler, DrawHandler & oNewDrawHandler, const WindowSettings & oSettings);
  ~SDLWindow() noexcept;

  uint32_t GetWindowID() const;
  void Loop();

};

} // namespace SE

#include <SDLWindow.tcc>

#endif


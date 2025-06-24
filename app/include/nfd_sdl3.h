/*
  Native File Dialog Extended
  Repository: https://github.com/btzy/nativefiledialog-extended
  License: Zlib
  Original Author: Bernard Teo
  Modifications: Jackson Enright

  This header contains a function to convert an SDL window handle to a native window handle for
  passing to NFDe.

  This has been modified from its original version to support SDL3.
 */

#include <nfd.h>
#include <SDL3/SDL.h>
#include <stdbool.h>

/**
 *  Converts an SDL window handle to a native window handle that can be passed to NFDe.
 *  @param sdlWindow The SDL window handle.
 *  @param[out] nativeWindow The output native window handle, populated if and only if this function
 *  returns true.
 *  @return Either true to indicate success, or false to indicate failure.  If false is returned,
 * you can call SDL_GetError() for more information.  However, it is intended that users ignore the
 * error and simply pass a value-initialized nfdwindowhandle_t to NFDe if this function fails. */
static inline bool NFD_GetNativeWindowFromSDLWindow(SDL_Window *sdl_window, nfdwindowhandle_t *native_window)
{
    SDL_PropertiesID sdl_properties = SDL_GetWindowProperties(sdl_window);
    if (!sdl_properties)
    {
        return false;
    }
    
    void *win32_hwnd_pointer = SDL_GetPointerProperty(sdl_properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (win32_hwnd_pointer)
    {
        native_window->type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
        native_window->handle = win32_hwnd_pointer;
        return true;
    }

    void *cocoa_window_pointer = SDL_GetPointerProperty(sdl_properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    if (cocoa_window_pointer)
    {
        native_window->type = NFD_WINDOW_HANDLE_TYPE_COCOA;
        native_window->handle = cocoa_window_pointer;
        return true;
    }

    Sint64 window_id = SDL_GetNumberProperty(sdl_properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, NULL);
    if (window_id != 0)
    {
        native_window->type = NFD_WINDOW_HANDLE_TYPE_X11;
        native_window->handle = (void *)(uintptr_t)window_id;
        return true;
    }

    void *wayland_surface_pointer = SDL_GetPointerProperty(sdl_properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
    if (wayland_surface_pointer)
    {
        native_window->type = NFD_WINDOW_HANDLE_TYPE_WAYLAND;
        native_window->handle = wayland_surface_pointer;
        return true;
    }

    // Silence the warning in case we are not using a supported backend.
    (void)native_window;
    SDL_SetError("Unsupported native window type.");
    return false;
}

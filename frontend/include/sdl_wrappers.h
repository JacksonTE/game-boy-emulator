#pragma once

#include <cstdint>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>

namespace SdlResourceAcquisitionIsInitialization
{
    class Initializer
    {
    public:
        Initializer(uint32_t flags)
        {
            if (!SDL_Init(flags))
                throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        }

        ~Initializer()
        { 
            SDL_Quit();
        }

        Initializer(const Initializer &) = delete;
        Initializer &operator=(const Initializer &) = delete;

        Initializer(Initializer &&) = delete;
        Initializer &operator=(Initializer &&) = delete;
    };

    class Window
    {
    public:
        Window(const char* title, int width, int height, uint32_t flags)
            : window(SDL_CreateWindow(title, width, height, flags))
        {
            if (!window)
                throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        }

        ~Window()
        {
            if (window)
                SDL_DestroyWindow(window);
        }

        SDL_Window *get() const
        {
            return window;
        }

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        Window(Window &&) = delete;
        Window &operator=(Window &&) = delete;

    private:
        SDL_Window *window;
    };

    class Renderer {
    public:
        Renderer(Window &window, const char *rendering_driver_name = nullptr)
            : renderer(SDL_CreateRenderer(window.get(), rendering_driver_name))
        {
            if (!renderer)
                throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        }

        ~Renderer()
        {
            if (renderer)
                SDL_DestroyRenderer(renderer);
        }

        SDL_Renderer* get() const
        {
            return renderer;
        }

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;

        Renderer(Renderer &&) = delete;
        Renderer &operator=(Renderer &&) = delete;

    private:
        SDL_Renderer *renderer;
    };

    class Texture {
    public:
        Texture(Renderer &renderer, SDL_PixelFormat format, SDL_TextureAccess access, int width, int height)
            : texture(SDL_CreateTexture(renderer.get(), format, access, width, height))
        {
            if (!texture)
                throw std::runtime_error(std::string("SDL_CreateTexture failed: ") + SDL_GetError());
        }

        ~Texture()
        {
            if (texture)
                SDL_DestroyTexture(texture);
        }

        SDL_Texture *get() const
        {
            return texture;
        }

        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;

        Texture(Texture &&) = delete;
        Texture &operator=(Texture &&) = delete;

    private:
        SDL_Texture *texture;
    };

} // namespace SDLResourceAcquisitionIsInitialization

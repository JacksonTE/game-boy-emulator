#pragma once

#include <cstdint>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>

namespace ResourceAcquisitionIsInitialization
{

class SdlInitializerRaii
{
public:
    SdlInitializerRaii(uint32_t flags)
    {
        if (!SDL_Init(flags))
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    ~SdlInitializerRaii()
    { 
        SDL_Quit();
    }

    SdlInitializerRaii(const SdlInitializerRaii &) = delete;
    SdlInitializerRaii &operator=(const SdlInitializerRaii &) = delete;

    SdlInitializerRaii(SdlInitializerRaii &&) = delete;
    SdlInitializerRaii &operator=(SdlInitializerRaii &&) = delete;
};

class SdlWindowRaii
{
public:
    SdlWindowRaii(const char *title, int width, int height, uint32_t flags)
        : window{SDL_CreateWindow(title, width, height, flags)}
    {
        if (!window)
            throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    ~SdlWindowRaii()
    {
        if (window)
            SDL_DestroyWindow(window);
    }

    SDL_Window *get() const
    {
        return window;
    }

    SdlWindowRaii(const SdlWindowRaii &) = delete;
    SdlWindowRaii &operator=(const SdlWindowRaii &) = delete;

    SdlWindowRaii(SdlWindowRaii &&) = delete;
    SdlWindowRaii &operator=(SdlWindowRaii &&) = delete;

private:
    SDL_Window *window{};
};

class SdlRendererRaii
{
public:
    SdlRendererRaii(SdlWindowRaii &window, const char *rendering_driver_name = nullptr)
        : renderer{SDL_CreateRenderer(window.get(), rendering_driver_name)}
    {
        if (!renderer)
        {
            throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        }
        SDL_SetRenderLogicalPresentation
        (
            renderer,
            DISPLAY_WIDTH_PIXELS,
            DISPLAY_HEIGHT_PIXELS,
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
        );
        if (!SDL_SetRenderVSync(renderer, 1))
        {
            std::cerr << "VSync unable to be used: " << SDL_GetError() << "\n";
        }
    }

    ~SdlRendererRaii()
    {
        if (renderer)
            SDL_DestroyRenderer(renderer);
    }

    SDL_Renderer *get() const
    {
        return renderer;
    }

    SdlRendererRaii(const SdlRendererRaii &) = delete;
    SdlRendererRaii &operator=(const SdlRendererRaii &) = delete;

    SdlRendererRaii(SdlRendererRaii &&) = delete;
    SdlRendererRaii &operator=(SdlRendererRaii &&) = delete;

private:
    SDL_Renderer *renderer{};
};

class SdlTextureRaii
{
public:
    SdlTextureRaii(SdlRendererRaii &renderer, SDL_PixelFormat format, SDL_TextureAccess access, int width, int height)
        : texture{SDL_CreateTexture(renderer.get(), format, access, width, height)}
    {
        if (!texture)
        {
            throw std::runtime_error(std::string("SDL_CreateTexture failed: ") + SDL_GetError());
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    }

    ~SdlTextureRaii()
    {
        if (texture)
            SDL_DestroyTexture(texture);
    }

    SDL_Texture *get() const
    {
        return texture;
    }

    SdlTextureRaii(const SdlTextureRaii &) = delete;
    SdlTextureRaii &operator=(const SdlTextureRaii &) = delete;

    SdlTextureRaii(SdlTextureRaii &&) = delete;
    SdlTextureRaii &operator=(SdlTextureRaii &&) = delete;

private:
    SDL_Texture *texture;
};

class ImGuiContextRaii
{
public:
    ImGuiContextRaii(SDL_Window *window, SDL_Renderer *renderer)
    {
        IMGUI_CHECKVERSION();

        ImGuiContext *context = ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.FontGlobalScale = 1.45f;

        ImGuiStyle &style = ImGui::GetStyle();
        style.ItemSpacing.x = 11.0f;
        style.FramePadding.y += 4.0f;

        ImGui::StyleColorsDark();

        if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer))
        {
            ImGui::DestroyContext(context);
            throw std::runtime_error("ImGui_ImplSDL3_InitForSDLRenderer() failed");
        }

        if (!ImGui_ImplSDLRenderer3_Init(renderer))
        {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext(context);
            throw std::runtime_error("ImGui_ImplSDLRenderer3_Init() failed");
        }

        imgui_context = context;
    }

    ~ImGuiContextRaii()
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(imgui_context);
    }

    ImGuiContextRaii(const ImGuiContextRaii &) = delete;
    ImGuiContextRaii &operator=(const ImGuiContextRaii &) = delete;

    ImGuiContextRaii(ImGuiContextRaii &&) = delete;
    ImGuiContextRaii &operator=(ImGuiContextRaii &&) = delete;

private:
    ImGuiContext *imgui_context{};
};

} // namespace ResourceAcquisitionIsInitialization

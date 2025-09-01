include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE)

FetchContent_Declare(
    nativefiledialog
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG 0537750528db0cea592247be92a56f05a37c9471
    GIT_SHALLOW TRUE)
set(NFD_PORTAL OFF  CACHE BOOL "" FORCE)
set(NFD_GTK    ON   CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.12
    GIT_SHALLOW TRUE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(
    imgui
    nativefiledialog
    SDL3)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)

target_link_libraries(imgui PUBLIC
    SDL3::SDL3)

add_executable(emulator-gui
    "src/display_utilities.cpp"
    "src/imgui_rendering.cpp"
    "src/input_events.cpp"
    "src/main.cpp")

target_include_directories(emulator-gui PRIVATE
    "include")

if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

    target_include_directories(emulator-gui PRIVATE
        ${GTK3_INCLUDE_DIRS})

    target_link_libraries(emulator-gui PRIVATE
        ${GTK3_LIBRARIES})

    target_compile_options(emulator-gui PRIVATE
        ${GTK3_CFLAGS_OTHER})
endif()

target_link_libraries(emulator-gui PRIVATE
    game-boy-emulator
    imgui
    nfd
    SDL3::SDL3)

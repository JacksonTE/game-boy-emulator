find_package(ImGui)
find_package(NativeFileDialogExtended)
find_package(SDL3)

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
    ImGui
    nfd
    SDL3::SDL3)

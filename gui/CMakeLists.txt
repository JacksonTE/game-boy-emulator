find_package(Carlito REQUIRED)
find_package(ImGui REQUIRED)
find_package(NativeFileDialogExtended REQUIRED)
find_package(SDL3 REQUIRED)

add_executable(emulator-gui
    "src/display_utilities.cpp"
    "src/imgui_rendering.cpp"
    "src/input_events.cpp"
    "src/main.cpp"
    "src/persistent_settings.cpp")

target_include_directories(emulator-gui PRIVATE
    "include"
    ${CARLITO_INCLUDE_DIR})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building emulator-gui on Linux.")

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

install(TARGETS emulator-gui RUNTIME
        DESTINATION .
        COMPONENT gui)

find_package(Carlito REQUIRED)
find_package(ImGui REQUIRED)
find_package(SDL3 REQUIRED)

if(EMSCRIPTEN)
    ###############################################################################
    # Web Builds
    ###############################################################################
    add_executable(emulator-web
        "src/display_utilities.cpp"
        "src/imgui_rendering.cpp"
        "src/input_events.cpp"
        "src/main.cpp"
        "src/persistent_settings.cpp")

    target_include_directories(emulator-web PRIVATE
        "include"
        ${CARLITO_INCLUDE_DIR})

    target_link_libraries(emulator-web PRIVATE
        game-boy-emulator
        ImGui
        SDL3::SDL3)

    target_compile_options(emulator-web PRIVATE
        -pthread)

    target_link_options(emulator-web PRIVATE
        -pthread
        -sPTHREAD_POOL_SIZE=1
        -sINITIAL_MEMORY=67108864
        -sFORCE_FILESYSTEM=1
        --shell-file "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")

    set_target_properties(emulator-web PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html")

    # This generated file is for Cloudflare Pages to read and attach the specified HTTP headers to every response
    # These headers are required for Emscripten pthread support
    file(GENERATE
        OUTPUT "${CMAKE_BINARY_DIR}/bin/_headers"
        CONTENT [[
/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
]])

else()
    ###############################################################################
    # Desktop Builds
    ###############################################################################
    find_package(NativeFileDialogExtended REQUIRED)

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
endif()

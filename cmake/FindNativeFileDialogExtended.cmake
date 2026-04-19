include(FetchContent)

FetchContent_Declare(
    NativeFileDialogExtended
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG e092bbb4578583c6fd0edc2cd14fb9c658194a4d
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(NativeFileDialogExtended)

set(NFD_PORTAL OFF  CACHE BOOL "" FORCE)
set(NFD_GTK    ON   CACHE BOOL "" FORCE)

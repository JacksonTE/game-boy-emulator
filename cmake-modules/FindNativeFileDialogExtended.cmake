include(FetchContent)

FetchContent_Declare(
    NativeFileDialogExtended
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG 0537750528db0cea592247be92a56f05a37c9471
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(NativeFileDialogExtended)

set(NFD_PORTAL OFF  CACHE BOOL "" FORCE)
set(NFD_GTK    ON   CACHE BOOL "" FORCE)

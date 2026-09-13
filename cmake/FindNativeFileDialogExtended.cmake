include(FetchContent)

FetchContent_Declare(
    NativeFileDialogExtended
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG v1.3.0
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(NativeFileDialogExtended)

set(NFD_PORTAL OFF  CACHE BOOL "" FORCE)
set(NFD_GTK    ON   CACHE BOOL "" FORCE)

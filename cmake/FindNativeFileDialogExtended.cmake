include(FetchContent)

FetchContent_Declare(
    NativeFileDialogExtended
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG 3cd252a8f7ca32419b1ca235c2990ba6a0ecba7c
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(NativeFileDialogExtended)

set(NFD_PORTAL OFF  CACHE BOOL "" FORCE)
set(NFD_GTK    ON   CACHE BOOL "" FORCE)

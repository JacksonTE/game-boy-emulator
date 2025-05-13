include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.12
)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(
    SDL3
)

add_executable(emulate-game-boy-frontend
    "src/main.cpp"
)

target_include_directories(emulate-game-boy-frontend
PRIVATE
    "${CMAKE_SOURCE_DIR}/frontend/include"
)

target_link_libraries(emulate-game-boy-frontend
PRIVATE
    emulate-game-boy-core-library
)

target_link_libraries(emulate-game-boy-frontend
PRIVATE
    SDL3::SDL3
)

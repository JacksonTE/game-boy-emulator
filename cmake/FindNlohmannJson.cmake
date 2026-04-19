include(FetchContent)

FetchContent_Declare(
    nlohmann-json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(nlohmann-json)

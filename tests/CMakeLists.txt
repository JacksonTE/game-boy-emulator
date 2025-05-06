include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    nlohmann-json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)

FetchContent_MakeAvailable(
    googletest
    nlohmann-json
)

add_executable(emulate-game-boy-tests
    "src/test_gbmicrotest.cpp"
    "src/test_mooneye_test_suite.cpp"
    "src/test_single_instructions_json.cpp"
)

target_link_libraries(emulate-game-boy-tests
PRIVATE
    emulate-game-boy-core-library
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

include(GoogleTest)
gtest_discover_tests(emulate-game-boy-tests)

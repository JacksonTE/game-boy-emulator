include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    nlohmann-json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(
    googletest
    nlohmann-json)

add_executable(game-boy-tests
    "src/gbmicrotest_harness.cpp"
    "src/mooneye_test_suite_harness.cpp"
    "src/single_step_tests_harness.cpp")

target_link_libraries(game-boy-tests PRIVATE
    game-boy-emulator
    GTest::gtest_main
    nlohmann_json::nlohmann_json)

include(GoogleTest)
gtest_discover_tests(game-boy-tests)

find_package(GoogleTest REQUIRED)
find_package(NlohmannJson REQUIRED)

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

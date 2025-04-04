include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/52204f78f94d7512df1f0f3bea1d47437a2c3a58.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  nlohmann-json
  URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
)

FetchContent_MakeAvailable(
	googletest
	nlohmann-json
)

add_executable(emulate-game-boy-tests
    "test_single_instructions_json.cpp"
)

target_link_libraries(emulate-game-boy-tests PRIVATE
	emulate-game-boy-core-library
	GTest::gtest_main
	nlohmann_json::nlohmann_json
)

include(GoogleTest)
gtest_discover_tests(emulate-game-boy-tests)

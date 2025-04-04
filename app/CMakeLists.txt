add_executable(emulate-game-boy-app
    "main.cpp"
)

target_link_libraries(emulate-game-boy-app PUBLIC
    emulate-game-boy-core-library
)

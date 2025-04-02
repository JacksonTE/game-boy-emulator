add_executable(emulate-game-boy
    "main.cpp"
)

target_link_libraries(emulate-game-boy PUBLIC
    emulate-game-boy-lib
)

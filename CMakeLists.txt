###############################################################################
# Project Setup
###############################################################################
cmake_minimum_required(VERSION 3.22)

project(game-boy-emulator-project
    VERSION 1.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set CMake module path so calls to find_package(PackageName) can be routed to cmake-modules/Find<PackageName>.cmake files
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules" ${CMAKE_MODULE_PATH})

###############################################################################
# Testing Setup
###############################################################################
# Set PROJECT_ROOT so the path to the test data folder within the test harnesses is cleaner
add_compile_definitions(PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

enable_testing()

###############################################################################
# Add a Subdirectory for Each Component
###############################################################################
add_subdirectory(emulator)
add_subdirectory(gui)
add_subdirectory(tests)

###############################################################################
# CPack Packaging Setup
###############################################################################
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_STRIP_FILES TRUE)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Setting up CPack packaging on Windows.")
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_FILE_NAME "GameBoyEmulator-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-Windows-x64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Setting up CPack packaging on Linux.")
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "GameBoyEmulator-${PROJECT_VERSION}-Linux-x64")
endif()

install(FILES "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_SOURCE_DIR}/LICENSE.txt"
        DESTINATION .
        COMPONENT documentation)

include(CPack)

include(FetchContent)

FetchContent_Declare(
    Carlito
    GIT_REPOSITORY https://github.com/googlefonts/carlito.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Carlito)

###############################################################################
# Generating header file with byte array representation of Carlito-Regular.ttf
###############################################################################
set(CARLITO_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/carlito")
file(MAKE_DIRECTORY ${CARLITO_INCLUDE_DIR})

set(CARLITO_TTF_PATH "${carlito_SOURCE_DIR}/fonts/ttf/Carlito-Regular.ttf")
file(SIZE ${CARLITO_TTF_PATH} TTF_FILE_SIZE)
file(READ ${CARLITO_TTF_PATH} TTF_FILE_CONTENT HEX)

string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," TTF_BYTE_ARRAY_VALUES ${TTF_FILE_CONTENT})
string(REGEX REPLACE ",$" "" TTF_BYTE_ARRAY_VALUES ${TTF_BYTE_ARRAY_VALUES})
file(WRITE ${CARLITO_INCLUDE_DIR}/carlito_embedded.h
"#pragma once
#include <cstdint>

constexpr unsigned int carlito_ttf_len = ${TTF_FILE_SIZE};
constexpr unsigned char carlito_ttf[] = {
    ${TTF_BYTE_ARRAY_VALUES}
};
")
message(STATUS "Generated carlito_embedded.h at: ${CARLITO_INCLUDE_DIR}")

###############################################################################
# CPack Packaging Setup
###############################################################################
install(FILES "${carlito_SOURCE_DIR}/OFL.txt"
        DESTINATION third-party-licenses
        RENAME carlito_license.txt
        COMPONENT documentation)

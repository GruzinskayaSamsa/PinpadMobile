# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name.
project("fclient")

# Debug: Print ABI being built
message(STATUS "Building for ABI: ${ANDROID_ABI}")

# Add the native library
add_library(${CMAKE_PROJECT_NAME} SHARED
    native-lib.cpp
)

# Add spdlog as static library
add_library(spdlog STATIC IMPORTED)
set_target_properties(spdlog PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/../../../../../libs/spdlog/build/${ANDROID_ABI}/libspdlog.a
)

# Add mbedcrypto as shared library - FIXED: libmbedcrypto.so (not limbedcrypto.so)
add_library(mbedcrypto SHARED IMPORTED)
set_target_properties(mbedcrypto PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/../../../../../libs/mbedtls/build/${ANDROID_ABI}/library/libmbedcrypto.so
)

# Include directories
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/../../../../../libs/spdlog/spdlog/include
    ${CMAKE_SOURCE_DIR}/../../../../../libs/mbedtls/mbedtls/include
)

# Link libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    android
    log
    spdlog
    mbedcrypto
)
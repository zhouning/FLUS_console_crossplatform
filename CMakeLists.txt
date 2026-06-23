cmake_minimum_required(VERSION 3.15)
project(FLUS_console VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find GDAL (required)
# Note: CONFIG mode is preferred — it works uniformly with vcpkg / Homebrew /
# system packages on all platforms. If your distro only provides FindGDAL.cmake
# (module mode), drop the CONFIG keyword.
find_package(GDAL CONFIG REQUIRED)

# Optional: OpenCV for _RANK feature (disabled by default)
# Uncomment to enable:
# find_package(OpenCV REQUIRED)
# add_definitions(-D_RANK)

# ALGLIB sources
file(GLOB ALGLIB_SOURCES "src/alglib/*.cpp")

# Core FLUS sources
set(FLUS_CORE_SOURCES
    src/core/TiffDataRead.cpp
    src/core/TiffDataWrite.cpp
    src/core/nntrain.cpp
    src/core/simulationprocess.cpp
)

# Main executable
add_executable(flus_console
    src/main.cpp
    ${FLUS_CORE_SOURCES}
    ${ALGLIB_SOURCES}
)

# Include directories
target_include_directories(flus_console PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/alglib
    ${GDAL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(flus_console PRIVATE
    GDAL::GDAL
)

# Optional: Link OpenCV if _RANK is enabled
# if(OpenCV_FOUND)
#     target_link_libraries(flus_console PRIVATE ${OpenCV_LIBS})
# endif()

# Platform-specific settings
if(APPLE)
    # macOS: suppress warnings from ALGLIB
    target_compile_options(flus_console PRIVATE -Wno-narrowing)
endif()

if(MSVC)
    # MSVC: silence the deprecation noise on strcpy / atoi / etc.
    target_compile_definitions(flus_console PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
    # Match the ALGLIB / OpenCV-era code that pre-dates char8_t and strict UTF-8 literals.
    target_compile_options(flus_console PRIVATE /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(flus_console PRIVATE -Wno-narrowing -Wno-deprecated-declarations)
endif()

# Installation
install(TARGETS flus_console DESTINATION bin)

# Print configuration summary
message(STATUS "FLUS Console Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  GDAL Version: ${GDAL_VERSION}")
message(STATUS "  GDAL Include: ${GDAL_INCLUDE_DIRS}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")

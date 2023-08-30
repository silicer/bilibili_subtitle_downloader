cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 20)
set(VCPKG_TARGET_TRIPLET "x64-mingw-static")
project(Subtitle VERSION 0.1.0 LANGUAGES C CXX)
message("-----------SHOWING-------------")
message(${CMAKE_PREFIX_PATH})
message(${CMAKE_CXX_COMPILER})
message(${CMAKE_C_COMPILER})
message(${CMAKE_GENERATOR})
message("-----------ENDING--------------")


include(CTest)
enable_testing()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_subdirectory(src/utils)
add_executable(Subtitle src/subtitle.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

find_package(cpr CONFIG REQUIRED)
target_link_libraries(Subtitle PRIVATE cpr::cpr)

find_package(nlohmann_json CONFIG REQUIRED)
set(nlohmann-json_IMPLICIT_CONVERSIONS OFF)
target_link_libraries(Subtitle PRIVATE nlohmann_json::nlohmann_json)

find_package(argparse CONFIG REQUIRED)
target_link_libraries(Subtitle PRIVATE argparse::argparse)

target_link_libraries(Subtitle PRIVATE CookieUtils)

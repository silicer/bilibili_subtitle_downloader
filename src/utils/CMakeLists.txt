cmake_minimum_required(VERSION 3.0.0)
aux_source_directory(. DIR_LIB_SRCS)

add_library(CookieUtils ${DIR_LIB_SRCS})

find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(CookieUtils PRIVATE unofficial::sqlite3::sqlite3)

find_package(cryptopp CONFIG REQUIRED)
target_link_libraries(CookieUtils PRIVATE cryptopp::cryptopp)

find_package(nlohmann_json CONFIG REQUIRED)
set(nlohmann-json_IMPLICIT_CONVERSIONS OFF)
target_link_libraries(CookieUtils PRIVATE nlohmann_json::nlohmann_json)

find_package(Boost REQUIRED)
target_link_libraries(CookieUtils PRIVATE Boost::boost)


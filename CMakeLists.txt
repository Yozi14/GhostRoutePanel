cmake_minimum_required(VERSION 3.20)
project(GhostRoutePanel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GR_BUILD_CLI "Build ghostroute CLI" ON)
option(GR_FETCH_DEPS "Fetch third-party deps via FetchContent" ON)

# --- Dependencies ---
include(FetchContent)

if(GR_FETCH_DEPS)
  FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(json)

  FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.16.3
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(httplib)

  FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(spdlog)
endif()

find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
  message(STATUS "SQLite3 not found — using bundled amalgamation")
  add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
  target_include_directories(sqlite3 PUBLIC third_party/sqlite)
  set(SQLITE3_LIBRARIES sqlite3)
  set(SQLITE3_INCLUDE_DIRS third_party/sqlite)
else()
  set(SQLITE3_LIBRARIES SQLite::SQLite3)
endif()

# --- Core library ---
file(GLOB_RECURSE GR_SOURCES
  src/core/*.cpp
  src/services/*.cpp
  src/tenant/*.cpp
  src/updater/*.cpp
  src/api/*.cpp)

add_library(ghostroute_core STATIC ${GR_SOURCES})
target_include_directories(ghostroute_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SQLITE3_INCLUDE_DIRS})
target_link_libraries(ghostroute_core PUBLIC
  nlohmann_json::nlohmann_json
  httplib::httplib
  spdlog::spdlog
  ${SQLITE3_LIBRARIES})

if(WIN32)
  target_link_libraries(ghostroute_core PUBLIC ws2_32 crypt32)
endif()

# --- Panel server ---
add_executable(ghostroute-panel src/main.cpp)
target_link_libraries(ghostroute-panel PRIVATE ghostroute_core)

# --- CLI ---
if(GR_BUILD_CLI)
  add_executable(ghostroute cli/ghostroute.cpp)
  target_link_libraries(ghostroute PRIVATE ghostroute_core)
endif()

install(TARGETS ghostroute-panel RUNTIME DESTINATION bin)
if(GR_BUILD_CLI)
  install(TARGETS ghostroute RUNTIME DESTINATION bin)
endif()
install(DIRECTORY web/ DESTINATION share/ghostroute/web)
install(DIRECTORY config/ DESTINATION etc/ghostroute)
install(FILES data/schema.sql DESTINATION share/ghostroute/data)

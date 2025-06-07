cmake_minimum_required(VERSION 3.16)
project(mcguire_step_cli LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Prefix paths for OpenCASCADE (macOS, Linux, Docker)
# ---------------------------------------------------------------------------
list(APPEND CMAKE_PREFIX_PATH
  "$ENV{HOME}/occt-install/lib/cmake/opencascade"  # Manually built OCCT
  "/opt/homebrew/opt/opencascade"                  # macOS / Homebrew
  "/usr/local"                                      # Linux or Docker
)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(OpenCASCADE REQUIRED)
find_package(nlohmann_json QUIET)

# ---------------------------------------------------------------------------
# Executable and sources
# ---------------------------------------------------------------------------
add_executable(mcguire_step_cli
  main.cpp
  src/step_to_json.cpp
  src/stl_to_json.cpp
  src/obj_to_json.cpp
)

# ---------------------------------------------------------------------------
# Includes and Linking
# ---------------------------------------------------------------------------
target_include_directories(mcguire_step_cli PRIVATE
  include
  ${OpenCASCADE_INCLUDE_DIRS}
)

target_link_libraries(mcguire_step_cli PRIVATE
  ${OpenCASCADE_LIBRARIES}
)

if(nlohmann_json_FOUND)
  target_link_libraries(mcguire_step_cli PRIVATE nlohmann_json::nlohmann_json)
  message(STATUS "Using system nlohmann_json")
else()
  message(WARNING "nlohmann_json not found; using vendored header in include/nlohmann/json.hpp")
  target_include_directories(mcguire_step_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

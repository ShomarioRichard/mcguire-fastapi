cmake_minimum_required(VERSION 3.16)
project(mcguire_step_cli LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global flags
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Search paths  (desktop + Docker)
# ---------------------------------------------------------------------------
list(APPEND CMAKE_PREFIX_PATH
     "/opt/homebrew/opt/opencascade"   # macOS / Homebrew
     "/usr/local")                     # Docker image install prefix

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
# Ask for the whole OpenCASCADE package – it will expose the libs that exist
find_package(OpenCASCADE REQUIRED)     # no COMPONENTS list

# nlohmann/json — optional
find_package(nlohmann_json QUIET)

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_executable(mcguire_step_cli
  main.cpp
  src/step_to_json.cpp
)

# ---------------------------------------------------------------------------
# Include paths & libraries
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
else()
  message(STATUS "nlohmann_json not found; using vendored header in include/")
  target_include_directories(mcguire_step_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

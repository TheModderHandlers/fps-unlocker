cmake_minimum_required(VERSION 3.24)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

project(unlocker LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Os -ffunction-sections -fdata-sections)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
add_link_options(-static -static-libgcc -static-libstdc++
                 -Wl,--gc-sections -Wl,--strip-all -Wl,--enable-stdcall-fixup)

include(FetchContent)

FetchContent_Declare(minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG        v1.3.4
  GIT_SHALLOW    TRUE)

FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip)

FetchContent_MakeAvailable(minhook nlohmann_json)

add_library(nlohmann_json_hdr INTERFACE)
target_include_directories(nlohmann_json_hdr INTERFACE
  ${nlohmann_json_SOURCE_DIR}/include)

add_library(unlocker SHARED src/main.cpp)
target_link_libraries(unlocker PRIVATE minhook nlohmann_json_hdr)
target_include_directories(unlocker PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(unlocker PROPERTIES
  PREFIX ""
  OUTPUT_NAME "unlocker")

cmake_minimum_required(VERSION 3.14)

# Pulled in as an ESP-IDF component (idf_component.yml + idf.py add-dependency).
if(ESP_PLATFORM)
  idf_component_register(
    SRCS "src/mist.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_websocket_client
  )
  target_compile_definitions(${COMPONENT_LIB} PUBLIC __ESP32__)
else()

option(ESP32 "Build ESP-IDF test firmware (test/esp-project) via idf.py" OFF)

if(ESP32)
  include("${CMAKE_CURRENT_LIST_DIR}/cmake/esp32-build.cmake")
else()

# AUTO: use host-toolchain only when ESP cross-tools are on PATH (see cmake/detect-idf-pollution.cmake)
set(MIST_FORCE_HOST_TOOLCHAIN "AUTO" CACHE STRING
    "Host toolchain for desktop builds: AUTO (detect ESP tools on PATH), ON, or OFF")

if(NOT CMAKE_TOOLCHAIN_FILE)
  set(_mist_use_host_toolchain FALSE)
  if(MIST_FORCE_HOST_TOOLCHAIN STREQUAL "ON")
    set(_mist_use_host_toolchain TRUE)
  elseif(NOT MIST_FORCE_HOST_TOOLCHAIN STREQUAL "OFF")
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/detect-idf-pollution.cmake")
    mist_detect_idf_toolchain_pollution(_mist_idf_polluted)
    if(_mist_idf_polluted)
      set(_mist_use_host_toolchain TRUE)
    endif()
  endif()
  if(_mist_use_host_toolchain)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/host-toolchain.cmake"
        CACHE FILEPATH "Host toolchain (ESP cross-tools detected on PATH)" FORCE)
  endif()
endif()

project(Mist++ VERSION 0.3.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT TARGET CURL::libcurl)
  find_package(CURL REQUIRED)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
  include(FetchContent)
  FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
  FetchContent_MakeAvailable(json)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries (OFF for static)")
if(BUILD_SHARED_LIBS)
  add_library(mist++ SHARED src/mist.cpp)
else()
  add_library(mist++ STATIC src/mist.cpp)
endif()

set_target_properties(mist++ PROPERTIES
  VERSION   0.3.5
  SOVERSION 0
)

target_include_directories(mist++ PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(mist++ PUBLIC CURL::libcurl)
if(TARGET nlohmann_json::nlohmann_json)
  target_link_libraries(mist++ PUBLIC nlohmann_json::nlohmann_json)
endif()

install(TARGETS mist++
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include
  FILES_MATCHING
  PATTERN "*.hpp"
  PATTERN "*.h"
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/mist++.pc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/mist++.pc"
  @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mist++.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

option(BUILD_TEST "Build the test project" ON)

if(BUILD_TEST)
  add_subdirectory(test)
endif()

endif() # ESP32 (host / idf.py wrapper)

endif() # ESP_PLATFORM
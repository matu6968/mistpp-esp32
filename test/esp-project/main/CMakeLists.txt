set(SRC_FILES "main.cpp") # Define source files for Mist++ example
set(INCLUDE_DIRS "." "${CMAKE_CURRENT_SOURCE_DIR}/../../../include") # Include both local and mist++ headers
set(EMBED_FILES "") # Initialize an empty list for files to embed

# Add mist.cpp source file to the component
list(APPEND SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/mist.cpp")


# Register the component with source files, include dirs, and any conditionally added embedded files
idf_component_register(SRCS "${SRC_FILES}"
                       INCLUDE_DIRS "${INCLUDE_DIRS}"
                       EMBED_TXTFILES "${EMBED_FILES}"
                       REQUIRES "esp_websocket_client protocol_examples_common nvs_flash"
                       PRIV_REQUIRES "esp_wifi")

# Define __ESP32__ macro for the component
target_compile_definitions(${COMPONENT_LIB} PUBLIC __ESP32__)

# Fetch nlohmann_json if not available
if(NOT TARGET nlohmann_json::nlohmann_json)
  include(FetchContent)
  FetchContent_Declare(json 
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  )
  FetchContent_MakeAvailable(json)
endif()

# Link nlohmann_json to the component
target_link_libraries(${COMPONENT_LIB} PUBLIC nlohmann_json::nlohmann_json)



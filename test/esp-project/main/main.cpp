#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <random>
#include <sstream>
#include <iomanip>

#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "protocol_examples_common.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "mist/mist.hpp"

static const char *TAG = "mist_example";

// Generate a random username for the cloud connection
static std::string generate_random_username()
{
    std::random_device rd;
    std::ostringstream usernameStream;
    usernameStream << "esp32_" << std::setw(5) << std::setfill('0') << (rd() % 100000);
    return usernameStream.str();
}

// Task to handle Mist++ example
static void mist_example_task(void *pvParameters)
{
    // Create a Mist++ connection
    std::string username = generate_random_username();
    ESP_LOGI(TAG, "Creating Mist connection with username: %s", username.c_str());
    
    MistConnection *client = new MistConnection(
        CONFIG_MIST_CLOUD_URL,
        CONFIG_MIST_PROJECT_ID,
        username,
        CONFIG_MIST_CONTACT_INFO);

    // Set up connection status callback
    client->onConnectionStatus([](bool connected, const std::string &message) {
        if (connected) {
            ESP_LOGI(TAG, "Connected: %s", message.c_str());
        } else {
            ESP_LOGI(TAG, "Disconnected: %s", message.c_str());
        }
    });

    // Set up variable update callback
    client->onVariableUpdate([](const std::string &name, const std::string &value) {
        ESP_LOGI(TAG, "Variable \"%s\" changed to: %s", name.c_str(), value.c_str());
    });

    // Attempt to connect
    ESP_LOGI(TAG, "Connecting to TurboWarp cloud...");
    if (client->connect()) {
        ESP_LOGI(TAG, "Client started. Waiting for connection and handshake...");

        // Wait for connection to establish
        vTaskDelay(pdMS_TO_TICKS(5000));

        // Set a variable multiple times to demonstrate functionality
        ESP_LOGI(TAG, "Starting variable updates...");
        for (int counter = 0; counter < 10; counter++) {
            std::string value = std::to_string(counter);
            if (client->set("☁ Test Variable", value)) {
                ESP_LOGI(TAG, "Set \"☁ Test Variable\" to: %s", value.c_str());
            } else {
                ESP_LOGW(TAG, "Failed to set variable (not connected?)");
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Read the variable back
        std::string currentValue = client->get("☁ Test Variable");
        ESP_LOGI(TAG, "Current value of \"☁ Test Variable\": %s", currentValue.c_str());

        ESP_LOGI(TAG, "Disconnecting...");
        client->disconnect();
    } else {
        ESP_LOGE(TAG, "Failed to connect to TurboWarp Cloud Variables.");
    }

    // Clean up
    delete client;

    ESP_LOGI(TAG, "Mist example completed. Deleting task...");
    vTaskDelete(NULL);
}

// This must be unmangled otherwaise it will fail to link against ESP's libmain
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network interfaces
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Connect to Wi-Fi using the protocol examples common helper
    // This will use the SSID and password from menuconfig
    ESP_ERROR_CHECK(example_connect());

    // Create a task to run the Mist++ example
    // Run on core 0 with priority 5 and 8KB stack
    xTaskCreatePinnedToCore(
        mist_example_task,
        "mist_example",
        8192,
        NULL,
        5,
        NULL,
        0
    );
}

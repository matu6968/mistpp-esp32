#include "mist/mist.hpp"
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#ifndef __ESP32__
#include <curl/curl.h>
#else
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mist";

// Forward declaration
class MistConnection;

// ESP32 event handler
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  MistConnection *conn = static_cast<MistConnection *>(handler_args);
  
  (void)base;

  esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

  switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
      conn->_on_ws_connected();
      break;
      
    case WEBSOCKET_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
      conn->_on_ws_disconnected();
      break;
      
    case WEBSOCKET_EVENT_DATA:
      ESP_LOGD(TAG, "WEBSOCKET_EVENT_DATA - opcode=%d, data_len=%d", data->op_code, data->data_len);
      if (data->op_code == 0x1) {  // Text frame
        conn->_handle_recv_message((const char *)data->data_ptr, data->data_len);
      }
      break;
      
    case WEBSOCKET_EVENT_ERROR:
      ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
      conn->_on_ws_error();
      break;
  }
}
#endif

// Define the static member for desktop builds
#ifndef __ESP32__
MistConnection::CurlGlobalIniter MistConnection::s_curl_initer;
#endif

MistConnection::MistConnection(const std::string &project_id, const std::string &username, const std::string &user_agent_contact_info) 
  : project_id_(project_id), username_(username), user_agent_contact_info_(user_agent_contact_info), 
    ws_url_("wss://clouddata.turbowarp.org/"), running_(false), connected_(false)
#ifndef __ESP32__
  , curl_easy_(nullptr), curl_multi_(nullptr)
#else
  , ws_client_(nullptr)
#endif
{
  user_agent_ = generateUserAgent();
}

MistConnection::MistConnection(const std::string &url, const std::string &project_id, const std::string &username, const std::string &user_agent_contact_info) 
  : project_id_(project_id), username_(username), user_agent_contact_info_(user_agent_contact_info), 
    ws_url_(url), running_(false), connected_(false)
#ifndef __ESP32__
  , curl_easy_(nullptr), curl_multi_(nullptr)
#else
  , ws_client_(nullptr)
#endif
{
  user_agent_ = generateUserAgent();
}

MistConnection::~MistConnection() {
  disconnect();
#ifndef __ESP32__
  std::lock_guard<std::mutex> lock(curl_mutex_);
  if (curl_easy_) {
    curl_easy_cleanup(curl_easy_);
    curl_easy_ = nullptr;
  }
  if (curl_multi_) {
    curl_multi_cleanup(curl_multi_);
    curl_multi_ = nullptr;
  }
#else
  // ESP32: cleanup handled in disconnect()
#endif
}

bool MistConnection::connect(bool secure) {
#ifndef __ESP32__
  // Desktop (libcurl) implementation
  std::lock_guard<std::mutex> lock(curl_mutex_);
  if (running_) {
    std::cerr << "[ERROR] Already connected or attempting to connect." << std::endl;
    return false;
  }

  curl_easy_ = curl_easy_init();
  if (!curl_easy_) {
    std::cerr << "[ERROR] Failed to initialize CURL easy handle." << std::endl;
    if (conn_status_callback_) conn_status_callback_(false, "Failed to initialize CURL.");
    return false;
  }

  curl_easy_setopt(curl_easy_, CURLOPT_URL, ws_url_.c_str());
  curl_easy_setopt(curl_easy_, CURLOPT_CONNECT_ONLY, 2L);
  curl_easy_setopt(curl_easy_, CURLOPT_USERAGENT, user_agent_.c_str());
  if (!secure) {
    curl_easy_setopt(curl_easy_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_easy_, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  curl_multi_ = curl_multi_init();
  if (!curl_multi_) {
    std::cerr << "[ERROR] Failed to initialize CURL multi handle." << std::endl;
    curl_easy_cleanup(curl_easy_);
    curl_easy_ = nullptr;
    if (conn_status_callback_) conn_status_callback_(false, "Failed to initialize CURL multi.");
    return false;
  }

  CURLMcode mc = curl_multi_add_handle(curl_multi_, curl_easy_);
  if (mc != CURLM_OK) {
    std::cerr << "[ERROR] curl_multi_add_handle failed: " << curl_multi_strerror(mc) << std::endl;
    curl_easy_cleanup(curl_easy_);
    curl_easy_ = nullptr;
    curl_multi_cleanup(curl_multi_);
    curl_multi_ = nullptr;
    if (conn_status_callback_) conn_status_callback_(false, "Failed to add easy handle to multi: " + std::string(curl_multi_strerror(mc)));
    return false;
  }

  running_ = true;
  curl_thread_ = std::thread([this]() {
    try {
      this->processMessages();
    } catch (const std::exception &e) {
      std::cerr << "[ERROR] Error in CURL processing thread: " << e.what() << std::endl;
      if (conn_status_callback_) conn_status_callback_(false, "Internal thread error: " + std::string(e.what()));
    }
  });

  return true;
#else
  // ESP32 (esp_websocket_client) implementation
  if (running_) {
    ESP_LOGE(TAG, "Already connected or attempting to connect.");
    return false;
  }

  // Build the WebSocket configuration
  esp_websocket_client_config_t websocket_cfg = {};
  websocket_cfg.uri = ws_url_.c_str();
  websocket_cfg.user_agent = user_agent_.c_str();
  websocket_cfg.crt_bundle_attach = esp_crt_bundle_attach;

#if CONFIG_WS_OVER_TLS_SKIP_COMMON_NAME_CHECK
  websocket_cfg.skip_cert_common_name_check = true;
#else
  websocket_cfg.skip_cert_common_name_check = !secure;
#endif

  ws_client_ = esp_websocket_client_init(&websocket_cfg);
  if (!ws_client_) {
    ESP_LOGE(TAG, "Failed to initialize WebSocket client");
    if (conn_status_callback_) conn_status_callback_(false, "Failed to initialize WebSocket client.");
    return false;
  }

  // Register event handler
  esp_websocket_register_events(ws_client_, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)this);

  // Start the WebSocket client
  esp_err_t err = esp_websocket_client_start(ws_client_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WebSocket client: %s", esp_err_to_name(err));
    esp_websocket_client_destroy(ws_client_);
    ws_client_ = nullptr;
    if (conn_status_callback_) conn_status_callback_(false, "Failed to start WebSocket client.");
    return false;
  }

  running_ = true;
  
  // Wait for connection to establish before returning
  // Note: In ESP32, the actual connection is async. This waits briefly for the event.
  int retry_count = 0;
  while (!connected_ && retry_count < 100 && running_) {  // max 10 seconds
    vTaskDelay(pdMS_TO_TICKS(100));
    retry_count++;
  }

  if (!connected_ && running_) {
    ESP_LOGW(TAG, "WebSocket did not connect within timeout");
  } else if (connected_) {
    send_handshake();
  }

  return true;
#endif
}

bool MistConnection::connect() {
  return connect(true);
}

void MistConnection::disconnect() {
  if (!running_) return;
  running_ = false;

#ifndef __ESP32__
  // Desktop (libcurl) implementation
  std::lock_guard<std::mutex> lock(curl_mutex_);
  if (connected_) {
    size_t sent;
    CURLcode res = curl_ws_send(curl_easy_, "", 0, &sent, 0, CURLWS_CLOSE);
    if (res != CURLE_OK) std::cerr << "[ERROR] Error sending WebSocket close frame: " << curl_easy_strerror(res) << std::endl;
  }

  if (curl_thread_.joinable()) curl_thread_.join();

  if (curl_easy_) {
    curl_multi_remove_handle(curl_multi_, curl_easy_);
    curl_easy_cleanup(curl_easy_);
    curl_easy_ = nullptr;
  }
  if (curl_multi_) {
    curl_multi_cleanup(curl_multi_);
    curl_multi_ = nullptr;
  }

  connected_ = false;
  if (conn_status_callback_) conn_status_callback_(false, "Disconnected by client request.");
#else
  // ESP32 (esp_websocket_client) implementation
  if (ws_client_) {
    esp_websocket_client_close(ws_client_, portMAX_DELAY);
    esp_websocket_client_destroy(ws_client_);
    ws_client_ = nullptr;
  }

  connected_ = false;
  if (conn_status_callback_) conn_status_callback_(false, "Disconnected by client request.");
#endif
}

bool MistConnection::set(const std::string &name, const std::string &value) {
#ifndef __ESP32__
  std::lock_guard<std::mutex> lock(curl_mutex_);
#endif
  if (!connected_) {
    std::cerr << "[ERROR] Not connected to send variables." << std::endl;
    return false;
  }

  nlohmann::json msg;
  msg["method"] = "set";
  msg["name"] = name;
  msg["value"] = value;

  send_json_message(msg);
  std::lock_guard<std::mutex> var_lock(variables_mutex_);
  cloud_variables_[name] = value;
  return true;
}

std::string MistConnection::get(const std::string &name) const {
  std::lock_guard<std::mutex> lock(variables_mutex_);
  auto it = cloud_variables_.find(name);
  if (it != cloud_variables_.end()) return it->second;
  return "";
}

void MistConnection::onVariableUpdate(VariableUpdateCallback callback) {
  var_update_callback_ = std::move(callback);
}

void MistConnection::onConnectionStatus(ConnectionStatusCallback callback) {
  conn_status_callback_ = std::move(callback);
}

#ifndef __ESP32__

size_t MistConnection::write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  return size * nmemb;
}

size_t MistConnection::header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
  return size * nitems;
}

void MistConnection::processMessages() {
  int still_running = 0;
  char buffer[4096];

  while (running_) {
    {
      std::lock_guard<std::mutex> lock(curl_mutex_);

      CURLMcode perform_code = CURLM_CALL_MULTI_PERFORM;
      while (perform_code == CURLM_CALL_MULTI_PERFORM) perform_code = curl_multi_perform(curl_multi_, &still_running);

      if (perform_code != CURLM_OK) {
        std::cerr << "[ERROR] curl_multi_perform failed: " << curl_multi_strerror(perform_code) << std::endl;
        if (conn_status_callback_) conn_status_callback_(false, "Multi perform error: " + std::string(curl_multi_strerror(perform_code)));
        running_ = false;
      }

      CURLMsg *msg;
      int msgs_left;
      while ((msg = curl_multi_info_read(curl_multi_, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE && msg->easy_handle == curl_easy_) {
          if (msg->data.result == CURLE_OK) {
            if (!connected_) {
              connected_ = true;
              if (conn_status_callback_) conn_status_callback_(true, "Connected successfully.");

              send_handshake();

              int still_running_after_send;
              CURLMcode perform_code_after_send = CURLM_CALL_MULTI_PERFORM;
              while (perform_code_after_send == CURLM_CALL_MULTI_PERFORM) perform_code_after_send = curl_multi_perform(curl_multi_, &still_running_after_send);
              if (perform_code_after_send != CURLM_OK) std::cerr << "[ERROR] curl_multi_perform after handshake send failed: " << curl_multi_strerror(perform_code_after_send) << std::endl; // TODO: Improve error handling
            }
          } else {
            std::cerr << "[ERROR] Connection closed unexpectedly: " << curl_easy_strerror(msg->data.result) << std::endl;
            running_ = false;
            connected_ = false;
            if (conn_status_callback_) conn_status_callback_(false, "Connection terminated: " + std::string(curl_easy_strerror(msg->data.result)));
          }
        }
      }

      if (!running_) break;

      if (connected_) {
        size_t rlen;
        const struct curl_ws_frame *ws_meta;
        CURLcode recv_res = curl_ws_recv(curl_easy_, buffer, sizeof(buffer), &rlen, &ws_meta);

        if (recv_res == CURLE_OK) {
          if (ws_meta->flags & CURLWS_TEXT) handle_recv_message(buffer, rlen);
          else if (ws_meta->flags & CURLWS_CLOSE) {
            running_ = false;
            connected_ = false;
            if (conn_status_callback_) conn_status_callback_(false, "Server closed connection.");
          }
        } else if (recv_res != CURLE_AGAIN) {
          std::cerr << "[ERROR] curl_ws_recv failed: " << curl_easy_strerror(recv_res) << std::endl;
          running_ = false;
          connected_ = false;
          if (conn_status_callback_) conn_status_callback_(false, "Receive error: " + std::string(curl_easy_strerror(recv_res)));
        }
      }
    }

    if (!running_) break;

    long curl_timeout_ms = -1;
    if (still_running > 0) {
      std::lock_guard<std::mutex> lock_timeout(curl_mutex_);
      curl_multi_timeout(curl_multi_, &curl_timeout_ms);
    }

    if (still_running == 0) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    else if (curl_timeout_ms < 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    else if (curl_timeout_ms != 0) std::this_thread::sleep_for(std::chrono::milliseconds(curl_timeout_ms));
  }
}

#endif // !__ESP32__

void MistConnection::send_json_message(const nlohmann::json &json_msg) {
#ifndef __ESP32__
  if (!curl_easy_ || !connected_) {
    std::cerr << "[ERROR] Cannot send message: Not connected or CURL handle not initialized." << std::endl;
    return;
  }

  std::string payload = json_msg.dump();
  size_t sent;
  CURLcode res = curl_ws_send(curl_easy_, payload.c_str(), payload.length(), &sent, 0, CURLWS_TEXT);

  if (res != CURLE_OK) std::cerr << "[ERROR] Error sending WebSocket message: " << curl_easy_strerror(res) << std::endl;
#else
  if (!ws_client_ || !connected_) {
    ESP_LOGE(TAG, "Cannot send message: Not connected or WebSocket client not initialized.");
    return;
  }

  std::string payload = json_msg.dump();
  int sent = esp_websocket_client_send_text(ws_client_, payload.c_str(), payload.length(), portMAX_DELAY);

  if (sent < 0) {
    ESP_LOGE(TAG, "Error sending WebSocket message (%d)", sent);
  }
#endif
}

void MistConnection::send_handshake() {
  nlohmann::json handshake_msg;
  handshake_msg["method"] = "handshake";
  handshake_msg["user"] = username_;
  handshake_msg["project_id"] = project_id_;
  send_json_message(handshake_msg);
}

std::string MistConnection::generateUserAgent() const {
  return "Mist++/0.3.5 :: " + user_agent_contact_info_;
}

void MistConnection::handle_recv_message(const char *data, size_t len) {
  std::istringstream stream(std::string(data, len));
  std::string line;

  while (std::getline(stream, line)) {
    if (line.empty()) {
      continue;
    }

    nlohmann::json json_msg = nlohmann::json::parse(line, nullptr, false);
    if (json_msg.is_discarded()) {
#ifdef __ESP32__
      ESP_LOGE(TAG, "Error processing message. Raw message: %s", line.c_str());
#else
      std::cerr << "[ERROR] Error processing message. Raw message: " << line << std::endl;
#endif
      continue;
    }

    if (json_msg.contains("method") && json_msg["method"] == "set") {
      std::string name = json_msg["name"].get<std::string>();

      std::string value;
      if (json_msg["value"].is_string()) value = json_msg["value"].get<std::string>();
      else value = std::to_string(json_msg["value"].get<double>());

      std::lock_guard<std::mutex> lock(variables_mutex_);
      cloud_variables_[name] = value;

      if (var_update_callback_) var_update_callback_(name, value);
    }
  }
}

#ifdef __ESP32__
void MistConnection::_on_ws_connected() {
  connected_ = true;
  if (conn_status_callback_) conn_status_callback_(true, "Connected successfully.");
}

void MistConnection::_on_ws_disconnected() {
  connected_ = false;
  if (conn_status_callback_) conn_status_callback_(false, "Disconnected from server.");
}

void MistConnection::_on_ws_error() {
  connected_ = false;
  if (conn_status_callback_) conn_status_callback_(false, "WebSocket error occurred.");
}
#endif

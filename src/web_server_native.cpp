#include "web_server_native.h"

#include <cstring>
#include <sys/stat.h>
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "config.h"

static const char *TAG = "web_server_native";

namespace web_server_native {

static httpd_handle_t g_server = nullptr;
static light_core::LightCore *g_core = nullptr;

static char *serializeStateJson(const light_core::LightState &st) {
  cJSON *doc = cJSON_CreateObject();
  cJSON_AddBoolToObject(doc, "power", st.power);
  cJSON_AddNumberToObject(doc, "r", st.r);
  cJSON_AddNumberToObject(doc, "g", st.g);
  cJSON_AddNumberToObject(doc, "b", st.b);
  cJSON_AddNumberToObject(doc, "brightness", st.brightness);
  cJSON_AddStringToObject(doc, "lightMode", st.mode.c_str());
  cJSON_AddNumberToObject(doc, "colorTemp", st.colorTemp);
  cJSON_AddNumberToObject(doc, "warmth", st.warmth);
  cJSON_AddStringToObject(doc, "effect", st.effect.c_str());
  cJSON_AddNumberToObject(doc, "speed", st.speed);
  cJSON_AddNumberToObject(doc, "musicSensitivity", st.musicSensitivity);
  
  char *out = cJSON_PrintUnformatted(doc);
  cJSON_Delete(doc);
  return out;
}

// GET /api/status
static esp_err_t status_handler(httpd_req_t *req) {
  if (!g_core) return ESP_FAIL;
  light_core::LightState st = g_core->getState();
  
  cJSON *doc = cJSON_CreateObject();
  cJSON_AddStringToObject(doc, "version", "2.0.0-idf");
  cJSON_AddBoolToObject(doc, "power", st.power);
  cJSON_AddNumberToObject(doc, "r", st.r);
  cJSON_AddNumberToObject(doc, "g", st.g);
  cJSON_AddNumberToObject(doc, "b", st.b);
  cJSON_AddNumberToObject(doc, "brightness", st.brightness);
  cJSON_AddStringToObject(doc, "lightMode", st.mode.c_str());
  cJSON_AddNumberToObject(doc, "colorTemp", st.colorTemp);
  cJSON_AddNumberToObject(doc, "warmth", st.warmth);
  cJSON_AddStringToObject(doc, "effect", st.effect.c_str());
  cJSON_AddNumberToObject(doc, "speed", st.speed);
  cJSON_AddNumberToObject(doc, "musicSensitivity", st.musicSensitivity);
  cJSON_AddStringToObject(doc, "ip", "rgb-node.local");
  cJSON_AddStringToObject(doc, "ssid", config::kWifiSsid);
  cJSON_AddStringToObject(doc, "mode", "STA");

  char *out = cJSON_PrintUnformatted(doc);
  cJSON_Delete(doc);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, out);
  free(out);
  return ESP_OK;
}

// POST /api/state
static esp_err_t state_post_handler(httpd_req_t *req) {
  if (!g_core) return ESP_FAIL;
  char buf[512];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  buf[ret] = '\0';

  cJSON *doc = cJSON_Parse(buf);
  if (!doc) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;
  }

  light_core::LightState st = g_core->getState();
  cJSON *item = nullptr;

  if ((item = cJSON_GetObjectItem(doc, "power")) && cJSON_IsBool(item)) st.power = cJSON_IsTrue(item);
  if ((item = cJSON_GetObjectItem(doc, "r")) && cJSON_IsNumber(item)) st.r = item->valueint;
  if ((item = cJSON_GetObjectItem(doc, "g")) && cJSON_IsNumber(item)) st.g = item->valueint;
  if ((item = cJSON_GetObjectItem(doc, "b")) && cJSON_IsNumber(item)) st.b = item->valueint;
  if ((item = cJSON_GetObjectItem(doc, "brightness")) && cJSON_IsNumber(item)) st.brightness = item->valueint;
  if ((item = cJSON_GetObjectItem(doc, "lightMode")) && cJSON_IsString(item)) st.mode = item->valuestring;
  if ((item = cJSON_GetObjectItem(doc, "colorTemp")) && cJSON_IsNumber(item)) st.colorTemp = item->valueint;
  if ((item = cJSON_GetObjectItem(doc, "warmth")) && cJSON_IsNumber(item)) st.warmth = item->valueint;
  if ((item = cJSON_GetObjectItem(doc, "effect")) && cJSON_IsString(item)) st.effect = item->valuestring;
  if ((item = cJSON_GetObjectItem(doc, "speed")) && cJSON_IsNumber(item)) st.speed = item->valueint;
  if ((item = cJSON_GetObjectItem(doc, "musicSensitivity")) && cJSON_IsNumber(item)) st.musicSensitivity = item->valueint;

  cJSON_Delete(doc);

  g_core->updateState(st);
  broadcastState(st);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
  return ESP_OK;
}

// WebSocket handler GET /ws
static esp_err_t ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "WebSocket handshake completed");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) return ret;

  if (ws_pkt.len) {
    buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret == ESP_OK && ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
      cJSON *doc = cJSON_Parse((char*)buf);
      if (doc && g_core) {
        light_core::LightState st = g_core->getState();
        cJSON *item = nullptr;

        if ((item = cJSON_GetObjectItem(doc, "power")) && cJSON_IsBool(item)) st.power = cJSON_IsTrue(item);
        if ((item = cJSON_GetObjectItem(doc, "r")) && cJSON_IsNumber(item)) st.r = item->valueint;
        if ((item = cJSON_GetObjectItem(doc, "g")) && cJSON_IsNumber(item)) st.g = item->valueint;
        if ((item = cJSON_GetObjectItem(doc, "b")) && cJSON_IsNumber(item)) st.b = item->valueint;
        if ((item = cJSON_GetObjectItem(doc, "brightness")) && cJSON_IsNumber(item)) st.brightness = item->valueint;
        if ((item = cJSON_GetObjectItem(doc, "lightMode")) && cJSON_IsString(item)) st.mode = item->valuestring;
        if ((item = cJSON_GetObjectItem(doc, "colorTemp")) && cJSON_IsNumber(item)) st.colorTemp = item->valueint;
        if ((item = cJSON_GetObjectItem(doc, "warmth")) && cJSON_IsNumber(item)) st.warmth = item->valueint;
        if ((item = cJSON_GetObjectItem(doc, "effect")) && cJSON_IsString(item)) st.effect = item->valuestring;
        if ((item = cJSON_GetObjectItem(doc, "speed")) && cJSON_IsNumber(item)) st.speed = item->valueint;
        if ((item = cJSON_GetObjectItem(doc, "musicSensitivity")) && cJSON_IsNumber(item)) st.musicSensitivity = item->valueint;

        cJSON_Delete(doc);
        g_core->updateState(st);
        broadcastState(st);
      }
    }
    free(buf);
  }
  return ESP_OK;
}

// Static File Server Handler from /spiffs
static esp_err_t static_file_handler(httpd_req_t *req) {
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "/spiffs%s", req->uri);

  struct stat st;
  if (stat(filepath, &st) != 0 || S_ISDIR(st.st_mode)) {
    snprintf(filepath, sizeof(filepath), "/spiffs/index.html");
    if (stat(filepath, &st) != 0) {
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
  }

  FILE *fd = fopen(filepath, "r");
  if (!fd) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  if (strstr(filepath, ".html")) httpd_resp_set_type(req, "text/html");
  else if (strstr(filepath, ".css")) httpd_resp_set_type(req, "text/css");
  else if (strstr(filepath, ".js")) httpd_resp_set_type(req, "application/javascript");
  else if (strstr(filepath, ".png")) httpd_resp_set_type(req, "image/png");

  char chunk[1024];
  size_t bytes_read;
  while ((bytes_read = fread(chunk, 1, sizeof(chunk), fd)) > 0) {
    if (httpd_resp_send_chunk(req, chunk, bytes_read) != ESP_OK) {
      fclose(fd);
      return ESP_FAIL;
    }
  }
  fclose(fd);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

void init(light_core::LightCore *core) {
  g_core = core;

  // Mount LittleFS filesystem to /spiffs
  esp_vfs_littlefs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "spiffs",
      .format_if_mount_failed = true,
      .dont_mount = false
  };
  esp_err_t ret = esp_vfs_littlefs_register(&conf);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount LittleFS: %s", esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "LittleFS mounted successfully at /spiffs");
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.max_open_sockets = 7;

  if (httpd_start(&g_server, &config) == ESP_OK) {
    httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL };
    httpd_uri_t state_uri = { .uri = "/api/state", .method = HTTP_POST, .handler = state_post_handler, .user_ctx = NULL };
    httpd_uri_t ws_uri = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .user_ctx = NULL, .is_websocket = true };
    httpd_uri_t static_uri = { .uri = "/*", .method = HTTP_GET, .handler = static_file_handler, .user_ctx = NULL };

    httpd_register_uri_handler(g_server, &status_uri);
    httpd_register_uri_handler(g_server, &state_uri);
    httpd_register_uri_handler(g_server, &ws_uri);
    httpd_register_uri_handler(g_server, &static_uri);

    ESP_LOGI(TAG, "Native ESP-IDF httpd web server and WebSockets started!");
  }
}

void broadcastState(const light_core::LightState &state) {
  if (!g_server) return;
  char *payload = serializeStateJson(state);
  size_t clients = 7;
  int fds[7];

  if (httpd_get_client_list(g_server, &clients, fds) == ESP_OK) {
    for (size_t i = 0; i < clients; i++) {
      if (httpd_ws_get_fd_info(g_server, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.payload = (uint8_t*)payload;
        ws_pkt.len = strlen(payload);
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        httpd_ws_send_frame_async(g_server, fds[i], &ws_pkt);
      }
    }
  }
  free(payload);
}

}  // namespace web_server_native

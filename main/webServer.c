#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include "stdbool.h"
#include "esp_vfs.h"
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>

#include "projectLog.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

typedef struct server_context {
    char           base_path[ESP_VFS_PATH_MAX + 1];
    char           scratch[SCRATCH_BUFSIZE];
} server_context_t;

httpd_handle_t server;

void sendToRemoteDebugger(const char *format, ...)
{
    int clientFds[7];
    size_t clientCount;

    char buffer[128];
    uint16_t len;

    va_list arg;

    if(server != NULL)
    {
        httpd_get_client_list(server, &clientCount, clientFds);
        LOGI("ClientList (Count: %d):", clientCount);
        for(uint i = 0; i < clientCount; i++)
        {
            LOGI("  Client Index %02d: %d", i, clientFds[i]);
        }

        va_start(arg, format);
        vsnprintf(buffer, 128, format, arg);
        va_end(arg);

        len = strlen(buffer);

        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        ws_pkt.len = len;
        ws_pkt.payload = (uint8_t*)buffer;

        for(uint i = 0; i < clientCount; i++)
        {
            LOGI("Sending Data to client ID: %d", clientFds[i]);
            httpd_ws_send_frame_async(server, clientFds[i], &ws_pkt);
            LOGI("Data Sent");
        }
    }
}

static esp_err_t wsHandler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(__func__, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(__func__, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(__func__, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(__func__, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(__func__, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(__func__, "Got packet with message: %s", ws_pkt.payload);
    }

    char sendMessage[] = "Connection Success";
    ws_pkt.len = strnlen(sendMessage, 50);
    ws_pkt.payload = (uint8_t*)sendMessage;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    ESP_LOGI(__func__, "Send Frame");
    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(__func__, "httpd_ws_send_frame failed with %d", ret);
    }
    free(buf);
    return ret;
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    ESP_LOGI(__func__, "get_handler: %s", req->uri);

    server_context_t *serverContext = (server_context_t *)req->user_ctx;
    strlcpy(filepath, serverContext->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(__func__, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = serverContext->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(__func__, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(__func__, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(__func__, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t start_web_server(const char *base_path)
{
    server_context_t *serverContext = calloc(1, sizeof(server_context_t));

    if(serverContext == NULL)
    {
        ESP_LOGE("__func__", "No Memory for Server Context");
        abort();
    }

    strlcpy(serverContext->base_path, base_path, sizeof(serverContext->base_path));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_LOGI("startServer", "Max Open Connections = %d", config.max_open_sockets);
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(__func__, "Starting HTTP Server");
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    /* URI handler for Websocket */
    httpd_uri_t ws = {
        .uri        = "/api/v1/ws/remoteDebugger",
        .method     = HTTP_GET,
        .handler    = wsHandler,
        .user_ctx   = serverContext,
        .is_websocket = true
    };
    httpd_register_uri_handler(server, &ws);

    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = common_get_handler,
        .user_ctx = serverContext
    };
    httpd_register_uri_handler(server, &common_get_uri);

    return ESP_OK;
}
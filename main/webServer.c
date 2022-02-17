/**
 * @file webServer.c
 * 
 * @brief 
 * Handles All Webserver functions.
 * 
 */

// ESP IDF Includes
#include "esp_http_server.h"
#include "esp_err.h"
#include "stdbool.h"
#include "esp_vfs.h"
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>

// Project Incudes
#include "projectLog.h"


#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

typedef struct server_context {
    char           base_path[ESP_VFS_PATH_MAX + 1];
    char           scratch[SCRATCH_BUFSIZE];
} server_context_t;

httpd_handle_t server;

/**
 * Enumeration for Websocket Types
 */
typedef enum
{
    WS_NONE = 0,
    WS_DEBUG,
    WS_DATA
} socketType_t;

/**
 * @brief 
 * Sends string out to remote debugger websockets. Parameters work like standard printf()
 */
void sendToRemoteDebugger(const char *format, ...)
{
    int clientFds[7];
    size_t clientCount;

    char buffer[128];   // Buffer for building string
    uint16_t len;

    va_list arg;

    // Make sure server is running
    if(server != NULL)
    {
        // Get Client list. Return immediately if there is an error with the list.
        if(httpd_get_client_list(server, &clientCount, clientFds) != ESP_OK)
            return;

        // Create string from variable arg list
        va_start(arg, format);
        vsnprintf(buffer, 128, format, arg);
        va_end(arg);

        len = strlen(buffer);

        // Create Websocket packet
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        ws_pkt.len = len;
        ws_pkt.payload = (uint8_t*)buffer;

        // Send to all open debugger Websockets
        for(uint i = 0; i < clientCount; i++)
        {
            // Check that this connection is a Debug Websocket
            if((uint32_t)httpd_sess_get_ctx(server, clientFds[i]) == WS_DEBUG)
            {
                httpd_ws_send_frame_async(server, clientFds[i], &ws_pkt);
            }
        }
    }
}

/**
 * @brief 
 * Sends Data out on all open Data Websockets
 * 
 * @param dataType Data Type Specifier to add to packet
 * @param data Data to be sent
 */
void sendData(wsDataType_t dataType, uint32_t data)
{
    int clientFds[7];
    size_t clientCount;

    uint32_t payload[2];
    payload[0] = dataType;
    payload[1] = data;

    // Make sure server is running
    if(server != NULL)
    {
        // Get Client list. Return immediately if there is an error with the list.
        if(httpd_get_client_list(server, &clientCount, clientFds) != ESP_OK)
            return;

        // Create Websocket packet
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.type = HTTPD_WS_TYPE_BINARY;
        ws_pkt.len = 8;
        ws_pkt.payload = (uint8_t*)payload;

        // Send to all open data Websockets
        for(uint i = 0; i < clientCount; i++)
        {
            // Check that socket is a data websocket
            if((uint32_t)httpd_sess_get_ctx(server, clientFds[i]) == WS_DATA)
            {
                LOGI("Sending Data to client ID: %d", clientFds[i]);
                httpd_ws_send_frame_async(server, clientFds[i], &ws_pkt);
            }
        }
    }
}

/**
 * @brief 
 * Blank Session Context Free Function. Session Context pointers are just used to save what type of connection
 * is intended to be used with the socket and don't actually point to anyting. This function is intended to 
 * override the Session Context Free function so the default function doesn't error when tryhing to free
 * a non existant pointer.
 * 
 * @param ctx Required for free function. Will contain the integer passed for the Session Context.
 */
static void wsSessContextFreeFunc(void *ctx)
{
    return;     // Do nothing
}

/**
 * @brief 
 * Websocket handler function. 
 * 
 * @param req 
 * @return esp_err_t 
 */
static esp_err_t wsHandler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        LOGI("Handshake done, Websocket connection was opened");

        switch((uint32_t)req->user_ctx)
        {
            case WS_DEBUG:
                LOGI("Handshake done, Debug Websocket opened");
                break;

            case WS_DATA:
            LOGI("Handshake done, Data Websocket opened");
                break;

            default:
                LOGI("Handshake done, Unknown Websocket");
                return ESP_FAIL;
                break;
        }

        /**
         * Assign session context to the requests user context to mark what type
         * of socket connection this is for use later. Assign free context function
         * to function that does noting to prevent the server from trying to free 
         * a non existent pointer stored in the session context field.
         */
        req->free_ctx = wsSessContextFreeFunc;
        req->sess_ctx = req->user_ctx;
        return ESP_OK;
    }

    // Start handling data received from websockets here. Not Implemented yet, place holder only.

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        LOGE("httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    LOGI("frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            LOGE("Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            LOGE("httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        LOGI("Got packet with message: %s", ws_pkt.payload);
    }

    char sendMessage[] = "Debugger Message Received";
    ws_pkt.len = strnlen(sendMessage, 50);
    ws_pkt.payload = (uint8_t*)sendMessage;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    LOGI("Send Frame");
    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        LOGE("httpd_ws_send_frame failed with %d", ret);
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

/**
 * @brief 
 * Handles common HTTP Requests for webpages.
 * 
 * @param req 
 * @return esp_err_t 
 */
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

/**
 * @brief 
 * Top Level Function to start the webserver and register URIs
 * 
 * @param base_path base path for webserver files
 * @return esp_err_t 
 */
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

    /* URI handler for Debugger Websocket */
    httpd_uri_t wsDebugger = {
        .uri        = "/api/v1/ws/remoteDebugger",
        .method     = HTTP_GET,
        .handler    = wsHandler,
        .user_ctx   = (void*)WS_DEBUG,  // Indicate this is a debugger websocket
        .is_websocket = true
    };
    httpd_register_uri_handler(server, &wsDebugger);

    /* URI handler for Data Websocket */
    httpd_uri_t wsData = {
        .uri        = "/api/v1/ws/data",
        .method     = HTTP_GET,
        .handler    = wsHandler,
        .user_ctx   = (void*)WS_DATA,   // Indicate this is a data websocket
        .is_websocket = true
    };
    httpd_register_uri_handler(server, &wsData);

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
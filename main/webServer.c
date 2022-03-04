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

#include "esp_log.h"

// Project Incudes
#include "projectLog.h"
#include "pumpControl.h"


#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

#define DATA_QUEUE_LEN (32)

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

typedef struct server_context {
    char           base_path[ESP_VFS_PATH_MAX + 1];
    char           scratch[SCRATCH_BUFSIZE];
} server_context_t;

/**
 * Stuct for queued websocket packets
 */
typedef struct
{
    httpd_ws_frame_t ws_pkt;    // Packet to send
    int fd;                     // Websocket file descriptor to send with
} queued_ws_frame_t;

/**
 * Server Handle
 */
httpd_handle_t server;

/**
 * Enumeration for Websocket Types
 */
typedef enum
{
    WS_NONE = 0,
    WS_DEBUG = 111,
    WS_DATA = 222
} socketType_t;

// Function Prototypes
void sendNewConnectionData(int clientFd);


void receiveSettingsData(data32_t* payload, uint32_t len)
{
    const uint32_t SettingsPacketLen = 5;

    if(len != SettingsPacketLen)
    {
        LOGW("Incorrect Settings Packet Length of %d", len);
        return;
    }

    if(payload[SETTINGS_TYPE].i != WS_DATA_NEW_SETTING_DATA)
    {
        LOGW("Incorrect Settings Packet Type of %d", payload[SETTINGS_TYPE].i);
    }

    // Length and type good. Apply settings.
    SetMinAmbientTemperature(payload[SETTINGS_MIN_AMB_TEMP].f);
    SetMinWaterTemperature(payload[SETTINGS_MIN_WATER_TEMP].f);
    SetAmbientTempHysteresis(payload[SETTINGS_AMB_HYSTERESIS].f);
    SetWaterTempHysteresis(payload[SETTINGS_WATER_HYSTERESIS].f);

    // Resend Settings Data to All Active Clients
    sendNewConnectionData(WS_ALL_CLIENTS);
}

/**
 * @brief 
 * Function meant to execute in httpd context due to calling httpd_queue_work() and send frame
 * 
 * @param arg pointer to httpd_ws_frame_t to send over websocket
 */
static void wsAsyncSend(void *arg)
{
    queued_ws_frame_t* queuedFrame = (queued_ws_frame_t*)arg;

    httpd_ws_send_frame_async(server, queuedFrame->fd, &queuedFrame->ws_pkt);

    // Free Data that has been sent
    free(queuedFrame->ws_pkt.payload);

    // Free the frame data now that it is not needed
    free(queuedFrame);
}

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

        // Send to all open debugger Websockets
        for(uint i = 0; i < clientCount; i++)
        {
            // Check that this connection is a Debug Websocket
            if((uint32_t)httpd_sess_get_ctx(server, clientFds[i]) == WS_DEBUG)
            {
                // Create Websocket packet
                queued_ws_frame_t * queuedData = calloc(1, sizeof(queued_ws_frame_t));

                // Copy Data to be sent so that is persists after this function ends
                char* dataCopy = malloc(len);
                memcpy(dataCopy, buffer, len); 

                queuedData->ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                queuedData->ws_pkt.len = len;
                queuedData->ws_pkt.payload = (uint8_t*)dataCopy;

                queuedData->fd = clientFds[i];

                //httpd_queue_work(server, wsAsyncSend, queuedData);
                wsAsyncSend(queuedData);
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
 * @param clientFd Specific Client to Send Data too, set to -1 to send to all clients
 */
void sendData(wsDataType_t dataType, uint32_t data, int clientFd)
{
    int allClientFds[7];
    size_t clientCount;

    // Make sure server is running
    if(server != NULL)
    {
        // Create Websocket packet
        queued_ws_frame_t wsFrame;

        wsFrame.ws_pkt.type = HTTPD_WS_TYPE_BINARY;
        wsFrame.ws_pkt.len = 8;
        wsFrame.fd = clientFd;

        // Check if we are sending to a specific client
        if(clientFd != WS_ALL_CLIENTS)
        {
            // Allocate persistent memory and copy packet data
            queued_ws_frame_t * queuedData = calloc(1, sizeof(queued_ws_frame_t));
            memcpy(queuedData, &wsFrame, sizeof(queued_ws_frame_t));

            // Copy Data to be sent so that is persists after this function ends
            uint32_t* payload = malloc(2*sizeof(uint32_t));
            payload[0] = dataType;
            payload[1] = data;

            queuedData->ws_pkt.payload = (uint8_t*)payload;

            //httpd_queue_work(server, wsAsyncSend, queuedData);
            wsAsyncSend(queuedData);

            return;     // Only sending to specific client. Skip the rest of the function.
        }

        // Get Client list. Return immediately if there is an error with the list.
        if(httpd_get_client_list(server, &clientCount, allClientFds) != ESP_OK)
            return;

        // Send to all open data Websockets
        for(uint i = 0; i < clientCount; i++)
        {
            // Check that socket is a data websocket
            if((uint32_t)httpd_sess_get_ctx(server, allClientFds[i]) == WS_DATA)
            {
                // Create Websocket packet
                queued_ws_frame_t * queuedData = calloc(1, sizeof(queued_ws_frame_t));
                memcpy(queuedData, &wsFrame, sizeof(queued_ws_frame_t));

                // Copy Data to be sent so that is persists after this function ends
                uint32_t* payload = malloc(2*sizeof(uint32_t));
                payload[0] = dataType;
                payload[1] = data;

                // Update payload pointer
                queuedData->ws_pkt.payload = (uint8_t*)payload;

                // Update client Fds
                queuedData->fd = allClientFds[i];

                LOGI("Sending Data to client ID: %d", allClientFds[i]);

                //httpd_queue_work(server, wsAsyncSend, queuedData);
                wsAsyncSend(queuedData);
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

void sendNewConnectionData(int clientFd)
{
    data32_t temp;

    LOGI("Sending new Connection data");
    temp.f = GetMinAmbientTemperature();
    sendData(WS_DATA_SETTING_MIN_AMB,    temp.i, clientFd);

    temp.f = GetAmbientTempHysteresis();
    sendData(WS_DATA_SETTING_AMB_HYST,   temp.i, clientFd);

    temp.f = GetMinWaterTemperature();
    sendData(WS_DATA_SETTING_MIN_WATER,  temp.i, clientFd);

    temp.f = GetWaterTempHysteresis();
    sendData(WS_DATA_SETTING_WATER_HYST, temp.i, clientFd);
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
                sendNewConnectionData(httpd_req_to_sockfd(req));
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
    data32_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        LOGE("httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    LOGI("frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        buf = calloc(1, ws_pkt.len);
        if (buf == NULL) {
            LOGE("Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = (uint8_t*)buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            LOGE("httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        
        // Show received data
        for(uint16_t i = 0; i < (ws_pkt.len/sizeof(uint32_t)); i++)
        {
            if(i == 0)
            {
                LOGI("Data %d: %d", i, buf[i].i);
            }
            else
            {
                LOGI("Data %d: %0.2f", i, buf[i].f);
            }
        }
    }

    receiveSettingsData(buf, ws_pkt.len/sizeof(data32_t));

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
#include "../include/rtmp_capi.h"
#include <stdio.h>
#include <unistd.h>

static void on_connect_cb(const char* ip, void* data)
{
    printf("Client connected: %s\n", ip);
}

static void on_publish_cb(const char* ip, const char* app, const char* key,
                          void* data)
{
    printf("Publish from %s: %s/%s\n", ip, app, key);
}

static void on_audio_cb(const char* app, const char* key, const uint8_t* data,
                        uint32_t len, uint32_t ts, void* ud)
{
    printf("Audio data for %s/%s, len=%u, ts=%u\n", app, key, len, ts);
}

int main()
{
    rtmp_logger_set_level(RTMP_LOG_INFO);
    RtmpServerHandle server = rtmp_server_create(1935);
    rtmp_server_set_on_connect(server, on_connect_cb, NULL);
    rtmp_server_set_on_publish(server, on_publish_cb, NULL);
    rtmp_server_set_on_audio_data(server, on_audio_cb, NULL);
    rtmp_server_enable_gop_cache(server, true);
    if (rtmp_server_start(server))
    {
        printf("RTMP Server started on port 1935. Press Ctrl+C to stop.\n");
        sleep(300); // 5 min
    }
    else
    {
        printf("Failed to start server\n");
    }
    rtmp_server_stop(server);
    rtmp_server_destroy(server);
    return 0;
}
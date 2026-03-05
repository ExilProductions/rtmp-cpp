#include "../include/rtmp_capi.hpp"
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/select.h>
#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

// Store connected players
#define MAX_PLAYERS 100
typedef struct {
    char app[64];
    char stream_key[64];
    void* session;
} PlayerInfo;

static PlayerInfo g_players[MAX_PLAYERS];
static int g_player_count = 0;

static int find_player(void* session) {
    for (int i = 0; i < g_player_count; i++) {
        if (g_players[i].session == session) return i;
    }
    return -1;
}

static void remove_player(int index) {
    if (index < 0 || index >= g_player_count) return;
    for (int i = index; i < g_player_count - 1; i++) {
        g_players[i] = g_players[i + 1];
    }
    g_player_count--;
}

static void broadcast_audio_to_players(const char* app, const char* stream_key,
                                      const uint8_t* data, uint32_t length,
                                      uint32_t timestamp) {
    // Note: In C, you would need to track sessions and use rtmp_server_broadcast_audio
    // This is a placeholder - the C API doesn't expose direct session access
    // For full C implementation, use the broadcast functions
    (void)app;
    (void)stream_key;
    (void)data;
    (void)length;
    (void)timestamp;
}

static void broadcast_video_to_players(const char* app, const char* stream_key,
                                      const uint8_t* data, uint32_t length,
                                      uint32_t timestamp) {
    (void)app;
    (void)stream_key;
    (void)data;
    (void)length;
    (void)timestamp;
}

// Callbacks
static void on_connect_cb(const char* ip, void* data) {
    printf("Client connected: %s\n", ip);
    (void)data;
}

static void on_publish_cb(const char* ip, const char* app, const char* key, void* data) {
    printf("Publish from %s: %s/%s\n", ip, app, key);
    (void)data;
}

static void on_play_cb(const char* ip, const char* app, const char* key, void* data) {
    printf("Player joined: %s/%s from %s\n", app, key, ip);
    (void)data;
}

static void on_audio_cb(const char* app, const char* stream_key,
                        const uint8_t* data, uint32_t length,
                        uint32_t timestamp, void* user_data) {
    printf("Audio from %s/%s: %u bytes, ts: %u\n", 
           app, stream_key, length, timestamp);
    
    // Example: Forward to players manually using broadcast function
    // rtmp_server_broadcast_audio(server, app, stream_key, data, length, timestamp);
    
    (void)user_data;
}

static void on_video_cb(const char* app, const char* stream_key,
                        const uint8_t* data, uint32_t length,
                        uint32_t timestamp, void* user_data) {
    printf("Video from %s/%s: %u bytes, ts: %u\n", 
           app, stream_key, length, timestamp);
    
    // Example: Forward to players manually using broadcast function
    // rtmp_server_broadcast_video(server, app, stream_key, data, length, timestamp);
    
    (void)user_data;
}

static void on_disconnect_cb(const char* ip, const char* app,
                             const char* stream_key,
                             bool was_publishing, bool was_playing,
                             void* user_data) {
    printf("Client disconnected: %s (publishing: %d, playing: %d)\n", 
           ip, was_publishing, was_playing);
    (void)app;
    (void)stream_key;
    (void)user_data;
}


static struct termios g_orig_termios;

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}

static int setup_nonblocking_stdin(void) {
    struct termios raw;
    int flags;

    if (tcgetattr(STDIN_FILENO, &g_orig_termios) != 0)
        return 0;

    raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
        return 0;

    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags < 0)
        return 0;

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0)
        return 0;

    atexit(restore_terminal);
    return 1;
}

int main(void) {
    rtmp_logger_set_level(RTMP_LOG_INFO);

    if (!setup_nonblocking_stdin()) {
        printf("Failed to configure terminal input\n");
        return 1;
    }

    RtmpServerHandle server = rtmp_server_create(1935);
    if (!server) {
        printf("Failed to create server\n");
        return 1;
    }

    // Set callbacks - data goes to callbacks, not automatically relayed
    rtmp_server_set_on_connect(server, on_connect_cb, NULL);
    rtmp_server_set_on_publish(server, on_publish_cb, NULL);
    rtmp_server_set_on_play(server, on_play_cb, NULL);
    rtmp_server_set_on_audio_data(server, on_audio_cb, NULL);
    rtmp_server_set_on_video_data(server, on_video_cb, NULL);
    rtmp_server_set_on_disconnect(server, on_disconnect_cb, NULL);

    // NOTE: relay is disabled by default (relay_enabled = false)
    // Data from publishers goes to on_audio_cb / on_video_data callbacks
    // You can manually broadcast to players using:
    //   rtmp_server_broadcast_audio(server, app, stream_key, data, length, timestamp);
    //   rtmp_server_broadcast_video(server, app, stream_key, data, length, timestamp);
    //
    // For traditional auto-relay behavior, use:
    //   rtmp_server_enable_relay(server, true);

    // Example: Enable relay for traditional behavior
    // rtmp_server_enable_relay(server, true);
    // rtmp_server_enable_gop_cache(server, true);

    bool isRunning = false;
    if (!rtmp_server_start(server, &isRunning)) {
        printf("Failed to start server\n");
        rtmp_server_destroy(server);
        return 1;
    }

    printf("RTMP Listen-Only Server running on port 1935\n");
    printf("- Data from publishers goes to callbacks\n");
    printf("- Use rtmp_server_enable_relay(server, true) for auto-broadcast\n");
    printf("Press 'q' to stop.\n");

    while (isRunning) {
        fd_set readfds;
        struct timeval tv;

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            char ch;
            ssize_t n = read(STDIN_FILENO, &ch, 1);
            if (n == 1 && (ch == 'q' || ch == 'Q')) {
                printf("Shutting down...\n");
                rtmp_server_stop(server);
                break;
            }
        }
    }

    rtmp_server_destroy(server);
    return 0;
}

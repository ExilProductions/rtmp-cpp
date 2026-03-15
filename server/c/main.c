#include "../../include/rtmp_capi.hpp"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
    #include <sys/select.h>
    #include <termios.h>
    #include <fcntl.h>
#endif

#ifndef _WIN32
static struct termios g_orig_termios;
#endif

static RtmpServerHandle g_server = NULL;

#ifndef _WIN32
static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}
#endif

static int setup_nonblocking_stdin(void) {
#ifdef _WIN32
    return 1;
#else
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
#endif
}

static void on_connect_cb(const char* ip, void* data) {
    printf("[+] Client connected: %s\n", ip);
    (void)data;
}

static void on_publish_cb(const char* ip, const char* app, const char* key, void* data) {
    printf("[+] Publisher started: %s/%s from %s\n", app, key, ip);
    (void)data;
}

static void on_play_cb(const char* ip, const char* app, const char* key, void* data) {
    printf("[+] Player joined: %s/%s from %s\n", app, key, ip);
    (void)data;
}

static void on_audio_cb(const char* app, const char* stream_key,
                        const uint8_t* data, uint32_t length,
                        uint32_t timestamp, void* user_data) {
    printf("[+] Audio: %s/%s (%u bytes, ts: %u)\n", 
           app, stream_key, length, timestamp);
    
    rtmp_server_broadcast_audio(g_server, app, stream_key, data, length, timestamp);
    (void)user_data;
}

static void on_video_cb(const char* app, const char* stream_key,
                        const uint8_t* data, uint32_t length,
                        uint32_t timestamp, void* user_data) {
    printf("[+] Video: %s/%s (%u bytes, ts: %u)\n", 
           app, stream_key, length, timestamp);
    
    rtmp_server_broadcast_video(g_server, app, stream_key, data, length, timestamp);
    (void)user_data;
}

static void on_disconnect_cb(const char* ip, const char* app,
                             const char* stream_key,
                             bool was_publishing, bool was_playing,
                             void* user_data) {
    printf("[-] Client disconnected: %s (was_publishing=%d, was_playing=%d)\n", 
           ip, was_publishing, was_playing);
    (void)app;
    (void)stream_key;
    (void)user_data;
}

static bool auth_cb(const char* app, const char* stream_key, const char* client_ip, void* user_data) {
    printf("[AUTH] %s trying to access %s/%s\n", client_ip, app, stream_key);
    (void)user_data;
    return true;
}

int main(void) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
#endif

    rtmp_logger_set_level(RTMP_LOG_INFO);

    if (!setup_nonblocking_stdin()) {
        printf("Failed to configure terminal input\n");
        return 1;
    }

    g_server = rtmp_server_create(1935);
    if (!g_server) {
        printf("Failed to create server\n");
        return 1;
    }

    rtmp_server_set_on_connect(g_server, on_connect_cb, NULL);
    rtmp_server_set_on_publish(g_server, on_publish_cb, NULL);
    rtmp_server_set_on_play(g_server, on_play_cb, NULL);
    rtmp_server_set_on_audio_data(g_server, on_audio_cb, NULL);
    rtmp_server_set_on_video_data(g_server, on_video_cb, NULL);
    rtmp_server_set_on_disconnect(g_server, on_disconnect_cb, NULL);
    rtmp_server_set_auth_callback(g_server, auth_cb, NULL);

    rtmp_server_enable_gop_cache(g_server, true);

    bool isRunning = false;
    if (!rtmp_server_start(g_server, &isRunning)) {
        printf("Failed to start server\n");
        rtmp_server_destroy(g_server);
        return 1;
    }

    printf("\n");
    printf("===========================================\n");
    printf("   RTMP Server running on port 1935\n");
    printf("===========================================\n");
    printf("- Waiting for publishers and players...\n");
    printf("- Press 'q' to stop.\n");
    printf("- Press 's' to show statistics.\n");
    printf("===========================================\n\n");

    while (isRunning) {
#ifdef _WIN32
        Sleep(100);
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                printf("\nShutting down...\n");
                rtmp_server_stop(g_server);
                break;
            } else if (ch == 's' || ch == 'S') {
                printf("\n--- Statistics ---\n");
                printf("Active publishers: %d\n", rtmp_server_get_active_publishers(g_server));
                printf("Active players: %d\n", rtmp_server_get_active_players(g_server));
                printf("Total connections: %d\n", rtmp_server_get_total_connections(g_server));
                printf("------------------\n\n");
            }
        }
#else
        fd_set readfds;
        struct timeval tv;

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            char ch;
            ssize_t n = read(STDIN_FILENO, &ch, 1);
            if (n == 1) {
                if (ch == 'q' || ch == 'Q') {
                    printf("\nShutting down...\n");
                    rtmp_server_stop(g_server);
                    break;
                } else if (ch == 's' || ch == 'S') {
                    printf("\n--- Statistics ---\n");
                    printf("Active publishers: %d\n", rtmp_server_get_active_publishers(g_server));
                    printf("Active players: %d\n", rtmp_server_get_active_players(g_server));
                    printf("Total connections: %d\n", rtmp_server_get_total_connections(g_server));
                    printf("------------------\n\n");
                }
            }
        }
#endif
        
        isRunning = rtmp_server_is_running(g_server);
    }

    rtmp_server_destroy(g_server);
    g_server = NULL;

#ifdef _WIN32
    WSACleanup();
#endif

    printf("Server stopped.\n");
    return 0;
}

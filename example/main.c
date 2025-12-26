#include "../include/rtmp_capi.hpp"
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/select.h>
#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>

// Callbacks
static void on_connect_cb(const char* ip, void* data)
{
    printf("Client connected: %s\n", ip);
}

static void on_publish_cb(const char* ip, const char* app, const char* key, void* data)
{
    printf("Publish from %s: %s/%s\n", ip, app, key);
}


static struct termios g_orig_termios;

static void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}

static int setup_nonblocking_stdin(void)
{
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

int main(void)
{
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

    rtmp_server_set_on_connect(server, on_connect_cb, NULL);
    rtmp_server_set_on_publish(server, on_publish_cb, NULL);
    rtmp_server_enable_gop_cache(server, true);

    bool isRunning = false;
    if (!rtmp_server_start(server, &isRunning)) {
        printf("Failed to start server\n");
        rtmp_server_destroy(server);
        return 1;
    }

    printf("RTMP server running. Press 'q' to stop.\n");

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

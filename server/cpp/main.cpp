#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #include <conio.h>
#else
    #include <unistd.h>
    #include <sys/select.h>
    #include <termios.h>
    #include <fcntl.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <algorithm>

#include "../../include/rtmp_server.hpp"

#ifndef _WIN32
static struct termios g_orig_termios;
#endif

using namespace rtmp;

class RTMPServerApp {
    RTMPServer server;

public:
    RTMPServerApp(int port) : server(port) {
        server.setOnConnect([](std::shared_ptr<RTMPSession> session) {
            std::cout << "[+] Client connected: " 
                      << session->getStreamInfo().client_ip << std::endl;
        });

        server.setOnPublish([](std::shared_ptr<RTMPSession> session,
                               const std::string& app,
                               const std::string& key) {
            std::cout << "[+] Publisher started: " << app << "/" << key 
                      << " from " << session->getStreamInfo().client_ip << std::endl;
        });

        server.setOnPlay([](std::shared_ptr<RTMPSession> session,
                            const std::string& app,
                            const std::string& key) {
            std::cout << "[+] Player joined: " << app << "/" << key 
                      << " from " << session->getStreamInfo().client_ip << std::endl;
        });

        server.setOnAudioData([](std::shared_ptr<RTMPSession> session,
                                 const std::vector<uint8_t>& data,
                                 uint32_t timestamp) {
            auto& info = session->getStreamInfo();
            std::cout << "[+] Audio: " << info.app << "/" << info.stream_key 
                      << " (" << data.size() << " bytes, ts: " << timestamp << ")" << std::endl;
        });

        server.setOnVideoData([](std::shared_ptr<RTMPSession> session,
                                 const std::vector<uint8_t>& data,
                                 uint32_t timestamp) {
            auto& info = session->getStreamInfo();
            std::cout << "[+] Video: " << info.app << "/" << info.stream_key 
                      << " (" << data.size() << " bytes, ts: " << timestamp << ")" << std::endl;
        });

        server.setOnDisconnect([](std::shared_ptr<RTMPSession> session) {
            auto& info = session->getStreamInfo();
            std::cout << "[-] Client disconnected: " << info.client_ip << std::endl;
        });

        server.enableRelay(true);
        server.enableGOPCache(true);
    }

    bool start(bool& isRunning) {
        return server.start(isRunning);
    }

    void stop() {
        server.stop();
    }

    int getActivePublishers() {
        return server.getActivePublishers();
    }

    int getActivePlayers() {
        return server.getActivePlayers();
    }

    int getTotalConnections() {
        return server.getTotalConnections();
    }

    bool isRunning() {
        return server.isRunning();
    }
};

#ifndef _WIN32
static void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}
#endif

static bool setup_nonblocking_stdin() {
#ifdef _WIN32
    return true;
#else
    struct termios raw;
    int flags;

    if (tcgetattr(STDIN_FILENO, &g_orig_termios) != 0)
        return false;

    raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
        return false;

    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags < 0)
        return false;

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0)
        return false;

    std::atexit(restore_terminal);
    return true;
#endif
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
#endif

    Logger::getInstance().setLevel(LogLevel::INFO);

    if (!setup_nonblocking_stdin()) {
        std::cerr << "Failed to configure terminal input" << std::endl;
        return 1;
    }

    RTMPServerApp app(1935);

    bool isRunning = false;
    if (!app.start(isRunning)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "\n===========================================" << std::endl;
    std::cout << "   RTMP Server running on port 1935" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << "- Auto-relay enabled (publishers -> players)" << std::endl;
    std::cout << "- GOP cache enabled for instant playback" << std::endl;
    std::cout << "- Press 'q' to stop." << std::endl;
    std::cout << "- Press 's' to show statistics." << std::endl;
    std::cout << "===========================================\n" << std::endl;

    while (app.isRunning()) {
#ifdef _WIN32
        Sleep(100);
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                std::cout << "\nShutting down..." << std::endl;
                app.stop();
                break;
            } else if (ch == 's' || ch == 'S') {
                std::cout << "\n--- Statistics ---" << std::endl;
                std::cout << "Active publishers: " << app.getActivePublishers() << std::endl;
                std::cout << "Active players: " << app.getActivePlayers() << std::endl;
                std::cout << "Total connections: " << app.getTotalConnections() << std::endl;
                std::cout << "------------------\n" << std::endl;
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
                    std::cout << "\nShutting down..." << std::endl;
                    app.stop();
                    break;
                } else if (ch == 's' || ch == 'S') {
                    std::cout << "\n--- Statistics ---" << std::endl;
                    std::cout << "Active publishers: " << app.getActivePublishers() << std::endl;
                    std::cout << "Active players: " << app.getActivePlayers() << std::endl;
                    std::cout << "Total connections: " << app.getTotalConnections() << std::endl;
                    std::cout << "------------------\n" << std::endl;
                }
            }
        }
#endif
    }

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "Server stopped." << std::endl;
    return 0;
}

#include <cstdio>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <fcntl.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <algorithm>

#include "../../include/rtmp_server.hpp"

using namespace rtmp;

class ListenOnlyServer {
    RTMPServer server;
    std::map<std::string, std::vector<std::shared_ptr<RTMPSession>>> players;

public:
    ListenOnlyServer(int port) : server(port) {
        server.setOnConnect([](std::shared_ptr<RTMPSession> session) {
            std::cout << "Client connected: " 
                      << session->getStreamInfo().client_ip << std::endl;
        });

        server.setOnPublish([](std::shared_ptr<RTMPSession> session,
                               const std::string& app,
                               const std::string& key) {
            std::cout << "Publish from " 
                      << session->getStreamInfo().client_ip 
                      << ": " << app << "/" << key << std::endl;
        });

        server.setOnPlay([this](std::shared_ptr<RTMPSession> session,
                                const std::string& app,
                                const std::string& key) {
            std::string fullKey = app + "/" + key;
            players[fullKey].push_back(session);
            std::cout << "Player joined: " << fullKey 
                      << " (total: " << players[fullKey].size() << ")" << std::endl;
        });

        server.setOnAudioData([this](std::shared_ptr<RTMPSession> session,
                                     const std::vector<uint8_t>& data,
                                     uint32_t timestamp) {
            auto& info = session->getStreamInfo();
            std::string key = info.app + "/" + info.stream_key;
            
            std::cout << "Audio from " << key << ": " 
                      << data.size() << " bytes, ts: " << timestamp << std::endl;
            
            broadcastAudio(info.app, info.stream_key, data, timestamp);
        });

        server.setOnVideoData([this](std::shared_ptr<RTMPSession> session,
                                     const std::vector<uint8_t>& data,
                                     uint32_t timestamp) {
            auto& info = session->getStreamInfo();
            std::string key = info.app + "/" + info.stream_key;
            
            std::cout << "Video from " << key << ": " 
                      << data.size() << " bytes, ts: " << timestamp << std::endl;
            
            // Example: Only broadcast every 30th frame (demo purposes)
            // In real use, you might process/transform frames here
            static int frame_count = 0;
            frame_count++;
            if (frame_count % 30 == 0) {
                broadcastVideo(info.app, info.stream_key, data, timestamp);
            }
        });

        server.setOnDisconnect([this](std::shared_ptr<RTMPSession> session) {
            auto& info = session->getStreamInfo();
            std::string key = info.app + "/" + info.stream_key;
            
            auto it = players.find(key);
            if (it != players.end()) {
                it->second.erase(std::remove(it->second.begin(), it->second.end(), session), it->second.end());
                if (it->second.empty()) {
                    players.erase(it);
                }
            }
            
            std::cout << "Client disconnected: " << info.client_ip << std::endl;
        });

        // NOTE: relay_enabled is false by default
        // Data goes to callbacks only - you handle broadcasting
        // Use enableRelay(true) for traditional auto-broadcast behavior
    }

    void broadcastAudio(const std::string& app, const std::string& key,
                       const std::vector<uint8_t>& data, uint32_t timestamp) {
        std::string fullKey = app + "/" + key;
        auto it = players.find(fullKey);
        if (it == players.end()) return;

        for (auto& player : it->second) {
            player->sendChunk(4, timestamp, (uint8_t)MessageType::AUDIO, 
                            1, data);
        }
    }

    void broadcastVideo(const std::string& app, const std::string& key,
                       const std::vector<uint8_t>& data, uint32_t timestamp) {
        std::string fullKey = app + "/" + key;
        auto it = players.find(fullKey);
        if (it == players.end()) return;

        for (auto& player : it->second) {
            player->sendChunk(6, timestamp, (uint8_t)MessageType::VIDEO, 
                            1, data);
        }
    }

    bool start(bool& isRunning) {
        return server.start(isRunning);
    }

    void stop() {
        server.stop();
    }
};

static struct termios g_orig_termios;

static void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}

static bool setup_nonblocking_stdin() {
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
}

int main() {
    Logger::getInstance().setLevel(LogLevel::INFO);

    if (!setup_nonblocking_stdin()) {
        std::cerr << "Failed to configure terminal input" << std::endl;
        return 1;
    }

    ListenOnlyServer listenServer(1935);

    bool isRunning = false;
    if (!listenServer.start(isRunning)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "RTMP Listen-Only Server running on port 1935" << std::endl;
    std::cout << "- Data from publishers goes to callbacks" << std::endl;
    std::cout << "- You decide what to do with the data" << std::endl;
    std::cout << "- Use enableRelay(true) for traditional auto-broadcast" << std::endl;
    std::cout << "Press 'q' to stop." << std::endl;

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
                std::cout << "Shutting down..." << std::endl;
                listenServer.stop();
                break;
            }
        }
    }

    return 0;
}

# rtmp-cpp

A lightweight C++ RTMP server library with C-compatible API.

## Features
- Full RTMP protocol implementation (handshake, chunking, AMF0)
- Supports publish and play streams
- Callbacks for connect, publish, play, audio/video data, disconnect
- GOP cache for low-latency playback
- FLV file recording
- Authentication callback
- Stream statistics (bitrate, frames, uptime)
- Connection limits, timeouts, ping/pong
- **Flexible data handling**: Choose between automatic relay or manual data processing

## Roadmap
You can View the Roadmap [Here](ROADMAP.md).

## Build
```bash
./build.sh
```

This builds `librtmp.so` and example binaries `rtmp_server_cpp` and `rtmp_server_c`.

## Windows Support

Windows Support hasn't been added yet, it will be implemented once the Linux Port is fully Working.

## Quick Start

### Basic Server (Auto-relay mode)
```bash
./build/rtmp_server_cpp
# or ./build/rtmp_server_c
```

Server listens on `rtmp://localhost:1935/live/stream`

Test with OBS:
- Server: `rtmp://127.0.0.1/live`
- Stream key: `stream`

Or FFmpeg:
```bash
ffmpeg -re -i input.mp4 -c copy -f flv rtmp://127.0.0.1/live/stream
```

---

## Usage Modes

### Mode 1: Listen Only (Process Data Yourself) - Recommended

By default, relay is disabled. Publishers send data to callbacks - you decide what to do with it. This is useful for:
- Processing/transforming video before forwarding
- Multi-destination streaming (forward to multiple RTMP endpoints)
- Recording to multiple locations
- Custom analytics/transcoding

**C++ Example:**
```cpp
#include "../../include/rtmp_server.hpp"

using namespace rtmp;

void onAudioData(std::shared_ptr<RTMPSession> session,
                 const std::vector<uint8_t>& data, uint32_t timestamp) {
    // Handle audio data yourself
    // Example: Process, analyze, or forward to another server
    printf("Received audio: %u bytes, ts: %u\n", data.size(), timestamp);
}

void onVideoData(std::shared_ptr<RTMPSession> session,
                 const std::vector<uint8_t>& data, uint32_t timestamp) {
    // Handle video data yourself
    printf("Received video: %u bytes, ts: %u\n", data.size(), timestamp);
    
    // Example: Forward to players manually
    const auto& info = session->getStreamInfo();
    // Note: Need server instance to broadcast - see below for complete example
}

int main() {
    RTMPServer server(1935);
    
    // Relay is disabled by default - data goes to callbacks only
    server.setOnAudioData(onAudioData);
    server.setOnVideoData(onVideoData);
    
    bool isRunning = false;
    server.start(isRunning);
    
    // ... handle shutdown
}
```

**Complete C++ Example with Manual Broadcasting:**
```cpp
#include "../../include/rtmp_server.hpp"
#include <map>
#include <memory>

using namespace rtmp;

class CustomRTMPServer {
    RTMPServer server;
    std::map<std::string, std::vector<std::shared_ptr<RTMPSession>>> players;
    
public:
    CustomRTMPServer(int port) : server(port) {
        server.setOnAudioData([this](auto session, auto& data, auto ts) {
            auto& info = session->getStreamInfo();
            std::string key = info.app + "/" + info.stream_key;
            
            printf("Audio from %s: %u bytes\n", key.c_str(), data.size());
            
            // Forward to players manually
            broadcastAudio(info.app, info.stream_key, data, ts);
        });
        
        server.setOnVideoData([this](auto session, auto& data, auto ts) {
            auto& info = session->getStreamInfo();
            std::string key = info.app + "/" + info.stream_key;
            
            printf("Video from %s: %u bytes\n", key.c_str(), data.size());
            
            // Forward to players manually
            broadcastVideo(info.app, info.stream_key, data, ts);
        });
        
        server.setOnPlay([this](auto session, auto& app, auto& key) {
            std::string fullKey = app + "/" + key;
            players[fullKey].push_back(session);
            printf("Player joined: %s (total: %zu)\n", 
                   fullKey.c_str(), players[fullKey].size());
        });
    }
    
    void broadcastAudio(const std::string& app, const std::string& key,
                       const std::vector<uint8_t>& data, uint32_t ts) {
        std::string fullKey = app + "/" + key;
        for (auto& player : players[fullKey]) {
            player->sendChunk(4, ts, (uint8_t)MessageType::AUDIO, 1, data);
        }
    }
    
    void broadcastVideo(const std::string& app, const std::string& key,
                       const std::vector<uint8_t>& data, uint32_t ts) {
        std::string fullKey = app + "/" + key;
        for (auto& player : players[fullKey]) {
            player->sendChunk(6, ts, (uint8_t)MessageType::VIDEO, 1, data);
        }
    }
    
    void start() {
        bool isRunning = false;
        server.start(isRunning);
    }
};
```

**C Example:**
```c
#include "../include/rtmp_capi.hpp"
#include <stdio.h>

static void on_audio_cb(const char* app, const char* stream_key,
                        const uint8_t* data, uint32_t length,
                        uint32_t timestamp, void* user_data) {
    printf("Audio: app=%s, key=%s, %u bytes\n", app, stream_key, length);
    // Handle audio data - process, forward, etc.
}

static void on_video_cb(const char* app, const char* stream_key,
                        const uint8_t* data, uint32_t length,
                        uint32_t timestamp, void* user_data) {
    printf("Video: app=%s, key=%s, %u bytes\n", app, stream_key, length);
    // Handle video data - process, forward, etc.
}

int main() {
    RtmpServerHandle server = rtmp_server_create(1935);
    
    // Relay disabled by default - use callbacks to handle data
    rtmp_server_set_on_audio_data(server, on_audio_cb, NULL);
    rtmp_server_set_on_video_data(server, on_video_cb, NULL);
    
    // Manually broadcast to players using:
    // rtmp_server_broadcast_audio(server, app, stream_key, data, length, timestamp);
    // rtmp_server_broadcast_video(server, app, stream_key, data, length, timestamp);
    
    bool isRunning = false;
    rtmp_server_start(server, &isRunning);
    
    // ... handle shutdown
    
    rtmp_server_destroy(server);
    return 0;
}
```

---

### Mode 2: Traditional Relay (Auto-broadcast)

If you want the old behavior where publishers' data is automatically broadcast to players:

**C++:**
```cpp
RTMPServer server(1935);
server.enableRelay(true);   // Enable automatic relay
server.enableGOPCache(true); // Enable GOP cache for instant playback
```

**C:**
```c
RtmpServerHandle server = rtmp_server_create(1935);
rtmp_server_enable_relay(server, true);
rtmp_server_enable_gop_cache(server, true);
```

---

## API Reference

### Callbacks
- `setOnConnect` / `rtmp_server_set_on_connect` - Client connects
- `setOnPublish` / `rtmp_server_set_on_publish` - Client starts publishing
- `setOnPlay` / `rtmp_server_set_on_play` - Client starts playing
- `setOnAudioData` / `rtmp_server_set_on_audio_data` - Audio data received
- `setOnVideoData` / `rtmp_server_set_on_video_data` - Video data received
- `setOnDisconnect` / `rtmp_server_set_on_disconnect` - Client disconnects

### Configuration
- `enableRelay(bool)` / `rtmp_server_enable_relay` - Enable/disable auto-relay (default: false)
- `enableGOPCache(bool)` / `rtmp_server_enable_gop_cache` - Enable GOP cache (default: false)
- `setAuthCallback` / `rtmp_server_set_auth_callback` - Authentication
- `setConnectionTimeout(int)` / `rtmp_server_set_connection_timeout`
- `setMaxPlayersPerStream(int)` / `rtmp_server_set_max_players_per_stream`

### Manual Broadcasting (use with relay disabled)
- `sendAudioToPlayers(app, stream_key, data, timestamp)` / `rtmp_server_broadcast_audio`
- `sendVideoToPlayers(app, stream_key, data, timestamp)` / `rtmp_server_broadcast_video`
- `sendMetadataToPlayers(app, stream_key, data)` / `rtmp_server_broadcast_metadata`

### Recording
- `startRecording(app, stream_key, filename)` / `rtmp_server_start_recording`
- `stopRecording(app, stream_key)` / `rtmp_server_stop_recording`

## License
MIT

![GitHubViewsCounter](https://openlabx.com/githubviewscounter/api/gitvcr.php?username=ExilProductions&repository=rtmp-cpp&theme=dark)

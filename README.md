# rtmp-cpp
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/user/rtmp-cpp)

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

## Quick Start

### Build
```bash
./build.sh
```

This builds `librtmp.so` and example binary `rtmp_example`.

### Run Example
```bash
./build/rtmp_example
```

Server listens on `rtmp://localhost:1935/live/stream`

Test with OBS:
- Server: `rtmp://127.0.0.1/live`
- Stream key: `stream`

Or FFmpeg:
```bash
ffmpeg -re -i input.mp4 -c copy -f flv rtmp://127.0.0.1/live/stream
```

## License
MIT
#ifndef MXOEMU_WEBGPU_TERMINAL_BRIDGE_H
#define MXOEMU_WEBGPU_TERMINAL_BRIDGE_H

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>
#include <memory>

struct GPSCoordinate
{
    double latitude{37.7749};   // San Francisco default reference
    double longitude{-122.4194};
    double altitudeMeters{15.0};
};

struct ARHardlineOverlay
{
    uint32 hardlineId{1};
    std::string name{"Bush & Montgomery Phone Booth"};
    GPSCoordinate realWorldCoords;
    float matrixX{0.0f};
    float matrixZ{0.0f};
    float compassHeadingDeg{45.0f};
    float distanceMeters{125.0f};
    bool isDecoded{false};
};

struct WebRTCAudioChannel
{
    uint32 channelId{1};
    std::string roomId{"Zion-Tactical-Alpha"};
    uint32 connectedPeers{2};
    bool isAmrNb1999CodecActive{true};
    float bandpassLowHz{300.0f};
    float bandpassHighHz{3400.0f};
    float sampleRateHz{8000.0f};
    float bitrateKbps{12.2f};
    float averageLatencyMs{14.5f};
};

struct WebGPUTileStream
{
    uint32 tileId{1};
    std::string gltfChunkUri{"/tiles/slums_sector_01.glb"};
    uint32 vertexCount{18500};
    uint32 triangleCount{24000};
    size_t byteSize{1450000};
    bool isCached{true};
};

class WebGPUTerminalBridge : public Singleton<WebGPUTerminalBridge>
{
public:
    WebGPUTerminalBridge();
    ~WebGPUTerminalBridge() = default;

    void Initialize();

    // GPS to Megacity Spatial Mapping
    void ConvertGPSToMatrixCoords(const GPSCoordinate& gps, float& outMatrixX, float& outMatrixZ) const;
    void ConvertMatrixCoordsToGPS(float matrixX, float matrixZ, GPSCoordinate& outGPS) const;

    // AR Operator Hardline Overlay
    uint32 RegisterARHardline(const std::string& name, const GPSCoordinate& gps, float matrixX, float matrixZ);
    std::vector<ARHardlineOverlay> QueryNearbyHardlines(const GPSCoordinate& userPos, float maxRadiusMeters);
    bool DecodeHardlineWithCamera(uint32 hardlineId, const std::string& cameraMatrixCode);

    // WebRTC Audio Bridge & 1999 Cell Phone Emulation
    uint32 CreateAudioChannel(const std::string& roomId);
    bool ConnectPeerToAudio(uint32 channelId);
    const WebRTCAudioChannel* GetAudioChannel(uint32 channelId) const;

    // WebGPU Tile Streaming
    uint32 RegisterTileStream(const std::string& gltfUri, uint32 vertices, uint32 triangles, size_t bytes);
    size_t GetTileStreamCount() const;

private:
    mutable std::recursive_mutex m_bridgeMutex;
    std::map<uint32, ARHardlineOverlay> m_hardlines;
    std::map<uint32, WebRTCAudioChannel> m_channels;
    std::vector<WebGPUTileStream> m_tiles;

    uint32 m_nextHardlineId{1};
    uint32 m_nextChannelId{1};
    uint32 m_nextTileId{1};

    // Reference datum: Downtown City Center
    double m_refLatitude{37.7749};
    double m_refLongitude{-122.4194};
};

#define sWebGPUTerminalBridge WebGPUTerminalBridge::getSingleton()

#endif // MXOEMU_WEBGPU_TERMINAL_BRIDGE_H

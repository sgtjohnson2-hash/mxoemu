#include "WebGPUTerminalBridge.h"
#include "Log.h"
#include <algorithm>

createFileSingleton(WebGPUTerminalBridge);

WebGPUTerminalBridge::WebGPUTerminalBridge()
{
    Initialize();
}

void WebGPUTerminalBridge::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_bridgeMutex);
    m_hardlines.clear();
    m_channels.clear();
    m_tiles.clear();

    // Default AR Hardlines
    GPSCoordinate c1{37.7752, -122.4188, 12.0};
    RegisterARHardline("Slums Pioneer Phone Booth", c1, 99640.0f, 8350.0f);

    GPSCoordinate c2{37.7765, -122.4170, 18.0};
    RegisterARHardline("Downtown Financial Booth", c2, 39216.0f, -21475.0f);

    GPSCoordinate c3{37.7730, -122.4210, 15.0};
    RegisterARHardline("International Gateway Booth", c3, -37444.0f, 23659.0f);

    // Default WebRTC Audio Channel
    CreateAudioChannel("Zion-Tactical-Alpha");

    // Default WebGPU glTF Tiles
    RegisterTileStream("/tiles/slums_district_mesh.glb", 45000, 68000, 4200000);
    RegisterTileStream("/tiles/downtown_towers_mesh.glb", 85000, 124000, 8900000);
    RegisterTileStream("/tiles/international_metro_mesh.glb", 52000, 78000, 5100000);
    RegisterTileStream("/tiles/richland_villas_mesh.glb", 38000, 56000, 3700000);
}

void WebGPUTerminalBridge::ConvertGPSToMatrixCoords(const GPSCoordinate& gps, float& outMatrixX, float& outMatrixZ) const
{
    const double degToRad = 0.017453292519943295;
    double latRad = m_refLatitude * degToRad;
    double metersPerDegreeLon = 111320.0 * std::cos(latRad);
    double metersPerDegreeLat = 110540.0;

    double dLon = gps.longitude - m_refLongitude;
    double dLat = gps.latitude - m_refLatitude;

    outMatrixX = (float)(dLon * metersPerDegreeLon);
    outMatrixZ = (float)(dLat * metersPerDegreeLat);
}

void WebGPUTerminalBridge::ConvertMatrixCoordsToGPS(float matrixX, float matrixZ, GPSCoordinate& outGPS) const
{
    const double degToRad = 0.017453292519943295;
    double latRad = m_refLatitude * degToRad;
    double metersPerDegreeLon = 111320.0 * std::cos(latRad);
    double metersPerDegreeLat = 110540.0;

    outGPS.longitude = m_refLongitude + ((double)matrixX / metersPerDegreeLon);
    outGPS.latitude = m_refLatitude + ((double)matrixZ / metersPerDegreeLat);
    outGPS.altitudeMeters = 15.0;
}

uint32 WebGPUTerminalBridge::RegisterARHardline(const std::string& name, const GPSCoordinate& gps, float matrixX, float matrixZ)
{
    std::lock_guard<std::recursive_mutex> lock(m_bridgeMutex);
    uint32 id = m_nextHardlineId++;
    ARHardlineOverlay h;
    h.hardlineId = id;
    h.name = name;
    h.realWorldCoords = gps;
    h.matrixX = matrixX;
    h.matrixZ = matrixZ;
    h.isDecoded = false;
    m_hardlines[id] = h;
    return id;
}

std::vector<ARHardlineOverlay> WebGPUTerminalBridge::QueryNearbyHardlines(const GPSCoordinate& userPos, float maxRadiusMeters)
{
    std::lock_guard<std::recursive_mutex> lock(m_bridgeMutex);
    std::vector<ARHardlineOverlay> results;

    const double R = 6371000.0; // Earth radius in meters
    const double degToRad = 0.017453292519943295;

    for (auto& kv : m_hardlines) {
        ARHardlineOverlay h = kv.second;
        double dLat = (h.realWorldCoords.latitude - userPos.latitude) * degToRad;
        double dLon = (h.realWorldCoords.longitude - userPos.longitude) * degToRad;

        double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
                   std::cos(userPos.latitude * degToRad) * std::cos(h.realWorldCoords.latitude * degToRad) *
                   std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
        double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
        double distance = R * c;

        if (distance <= (double)maxRadiusMeters) {
            h.distanceMeters = (float)distance;
            // Calculate compass bearing
            double y = std::sin(dLon) * std::cos(h.realWorldCoords.latitude * degToRad);
            double x = std::cos(userPos.latitude * degToRad) * std::sin(h.realWorldCoords.latitude * degToRad) -
                       std::sin(userPos.latitude * degToRad) * std::cos(h.realWorldCoords.latitude * degToRad) * std::cos(dLon);
            double bearing = std::atan2(y, x) / degToRad;
            if (bearing < 0.0) bearing += 360.0;
            h.compassHeadingDeg = (float)bearing;
            results.push_back(h);
        }
    }
    return results;
}

bool WebGPUTerminalBridge::DecodeHardlineWithCamera(uint32 hardlineId, const std::string& cameraMatrixCode)
{
    std::lock_guard<std::recursive_mutex> lock(m_bridgeMutex);
    auto it = m_hardlines.find(hardlineId);
    if (it == m_hardlines.end()) return false;

    if (!cameraMatrixCode.empty()) {
        it->second.isDecoded = true;
        return true;
    }
    return false;
}

uint32 WebGPUTerminalBridge::CreateAudioChannel(const std::string& roomId)
{
    std::lock_guard<std::recursive_mutex> lock(m_bridgeMutex);
    uint32 id = m_nextChannelId++;
    WebRTCAudioChannel ch;
    ch.channelId = id;
    ch.roomId = roomId;
    ch.connectedPeers = 2;
    ch.isAmrNb1999CodecActive = true;
    ch.bandpassLowHz = 300.0f;
    ch.bandpassHighHz = 3400.0f;
    ch.sampleRateHz = 8000.0f;
    ch.bitrateKbps = 12.2f;
    ch.averageLatencyMs = 14.5f;
    m_channels[id] = ch;
    return id;
}

bool WebGPUTerminalBridge::ConnectPeerToAudio(uint32 channelId)
{
    std::lock_guard<std::recursive_mutex> lock(m_bridgeMutex);
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) return false;
    it->second.connectedPeers++;
    return true;
}

const WebRTCAudioChannel* WebGPUTerminalBridge::GetAudioChannel(uint32 channelId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_bridgeMutex);
    auto it = m_channels.find(channelId);
    if (it != m_channels.end()) return &it->second;
    return nullptr;
}

uint32 WebGPUTerminalBridge::RegisterTileStream(const std::string& gltfUri, uint32 vertices, uint32 triangles, size_t bytes)
{
    std::lock_guard<std::recursive_mutex> lock(m_bridgeMutex);
    uint32 id = m_nextTileId++;
    WebGPUTileStream t;
    t.tileId = id;
    t.gltfChunkUri = gltfUri;
    t.vertexCount = vertices;
    t.triangleCount = triangles;
    t.byteSize = bytes;
    t.isCached = true;
    m_tiles.push_back(t);
    return id;
}

size_t WebGPUTerminalBridge::GetTileStreamCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_bridgeMutex);
    return m_tiles.size();
}

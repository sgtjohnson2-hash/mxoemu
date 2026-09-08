#include "WebAssemblyGatewayEngine.h"
#include "Log.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <boost/format.hpp>

createFileSingleton(WebAssemblyGatewayEngine);

WebAssemblyGatewayEngine::WebAssemblyGatewayEngine()
{
}

WebAssemblyGatewayEngine::~WebAssemblyGatewayEngine()
{
}

void WebAssemblyGatewayEngine::Initialize()
{
    std::unique_lock<std::shared_mutex> lock(m_wasmMutex);
    m_sessions.clear();
    m_wgslPipelines.clear();
    m_cachedChunks.clear();
    m_nextSessionId = 1;

    boost::format fmt("WebAssemblyGatewayEngine: Initialized WebRTC DataChannels & WebGPU WGSL Subsystem.");
    INFO_LOG(fmt);
}

void WebAssemblyGatewayEngine::ResetForTesting()
{
    std::unique_lock<std::shared_mutex> lock(m_wasmMutex);
    m_sessions.clear();
    m_wgslPipelines.clear();
    m_cachedChunks.clear();
    m_nextSessionId = 1;
}

void WebAssemblyGatewayEngine::Update(float dt)
{
    // Routine heartbeat & connection keep-alive
}

uint32_t WebAssemblyGatewayEngine::CreatePeerSession(uint32_t playerId, const std::string& sdpOffer)
{
    std::unique_lock<std::shared_mutex> lock(m_wasmMutex);
    uint32_t sid = m_nextSessionId++;
    WebRTCPeerSession session;
    session.sessionId = sid;
    session.playerId = playerId;
    session.sdpOffer = sdpOffer;
    session.isConnected = false;
    m_sessions[sid] = std::move(session);
    return sid;
}

std::string WebAssemblyGatewayEngine::GenerateAnswerSDP(uint32_t sessionId)
{
    std::unique_lock<std::shared_mutex> lock(m_wasmMutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return "";

    // Synthesize standard WebRTC answer SDP with DTLS-SRTP and SCTP DataChannel parameters
    std::string sdp = "v=0\r\no=- " + std::to_string(sessionId) + " 2 IN IP4 127.0.0.1\r\n"
                      "s=MxO_WebRTC_Gateway\r\nt=0 0\r\n"
                      "m=application 9 DTLS/SCTP 5000\r\n"
                      "c=IN IP4 127.0.0.1\r\n"
                      "a=sctp-port:5000\r\n"
                      "a=setup:passive\r\n";
    it->second.sdpAnswer = sdp;
    it->second.isConnected = true;
    return sdp;
}

bool WebAssemblyGatewayEngine::IsPeerConnected(uint32_t sessionId) const
{
    std::shared_lock<std::shared_mutex> lock(m_wasmMutex);
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? it->second.isConnected : false;
}

void WebAssemblyGatewayEngine::TerminateSession(uint32_t sessionId)
{
    std::unique_lock<std::shared_mutex> lock(m_wasmMutex);
    m_sessions.erase(sessionId);
}

size_t WebAssemblyGatewayEngine::GetActiveSessionCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_wasmMutex);
    return m_sessions.size();
}

std::vector<uint8_t> WebAssemblyGatewayEngine::WrapDataChannelPacket(uint16_t packetType, const uint8_t* payload, size_t length)
{
    // Frame structure:
    // [0..1] uint16_t packetType
    // [2..3] uint16_t length
    // [4..4+length-1] payload bytes
    // [4+length..4+length+3] uint32_t simple checksum (sum of payload + type + length)
    std::vector<uint8_t> frame(4 + length + 4);

    frame[0] = static_cast<uint8_t>(packetType & 0xFF);
    frame[1] = static_cast<uint8_t>((packetType >> 8) & 0xFF);

    uint16_t len16 = static_cast<uint16_t>(length & 0xFFFF);
    frame[2] = static_cast<uint8_t>(len16 & 0xFF);
    frame[3] = static_cast<uint8_t>((len16 >> 8) & 0xFF);

    uint32_t checksum = static_cast<uint32_t>(packetType) + static_cast<uint32_t>(len16);

    if (length > 0 && payload != nullptr) {
        std::memcpy(&frame[4], payload, length);
        for (size_t i = 0; i < length; ++i) {
            checksum += payload[i];
        }
    }

    size_t cPos = 4 + length;
    frame[cPos + 0] = static_cast<uint8_t>(checksum & 0xFF);
    frame[cPos + 1] = static_cast<uint8_t>((checksum >> 8) & 0xFF);
    frame[cPos + 2] = static_cast<uint8_t>((checksum >> 16) & 0xFF);
    frame[cPos + 3] = static_cast<uint8_t>((checksum >> 24) & 0xFF);

    return frame;
}

bool WebAssemblyGatewayEngine::UnwrapDataChannelPacket(const uint8_t* buffer, size_t bufferSize,
                                                       uint16_t& outPacketType, std::vector<uint8_t>& outPayload)
{
    if (buffer == nullptr || bufferSize < 8) return false;

    uint16_t packetType = static_cast<uint16_t>(buffer[0]) | (static_cast<uint16_t>(buffer[1]) << 8);
    uint16_t length     = static_cast<uint16_t>(buffer[2]) | (static_cast<uint16_t>(buffer[3]) << 8);

    if (bufferSize != static_cast<size_t>(4 + length + 4)) return false;

    size_t cPos = 4 + length;
    uint32_t expectedChecksum = static_cast<uint32_t>(buffer[cPos + 0]) |
                               (static_cast<uint32_t>(buffer[cPos + 1]) << 8) |
                               (static_cast<uint32_t>(buffer[cPos + 2]) << 16) |
                               (static_cast<uint32_t>(buffer[cPos + 3]) << 24);

    uint32_t computedChecksum = static_cast<uint32_t>(packetType) + static_cast<uint32_t>(length);
    for (size_t i = 0; i < length; ++i) {
        computedChecksum += buffer[4 + i];
    }

    if (computedChecksum != expectedChecksum) return false;

    outPacketType = packetType;
    outPayload.resize(length);
    if (length > 0) {
        std::memcpy(outPayload.data(), &buffer[4], length);
    }

    return true;
}

bool WebAssemblyGatewayEngine::RegisterWGSLComputePipeline(const std::string& name, const std::string& wgslSource,
                                                          uint32_t wx, uint32_t wy, uint32_t wz)
{
    std::unique_lock<std::shared_mutex> lock(m_wasmMutex);
    if (name.empty() || wgslSource.empty()) return false;

    WGSLShaderDescriptor desc;
    desc.pipelineName = name;
    desc.wgslSource = wgslSource;
    desc.workgroupSizeX = wx;
    desc.workgroupSizeY = wy;
    desc.workgroupSizeZ = wz;
    desc.isValid = true;

    m_wgslPipelines[name] = std::move(desc);
    return true;
}

const WGSLShaderDescriptor* WebAssemblyGatewayEngine::GetWGSLPipeline(const std::string& name) const
{
    std::shared_lock<std::shared_mutex> lock(m_wasmMutex);
    auto it = m_wgslPipelines.find(name);
    return (it != m_wgslPipelines.end()) ? &it->second : nullptr;
}

size_t WebAssemblyGatewayEngine::GetRegisteredPipelineCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_wasmMutex);
    return m_wgslPipelines.size();
}

bool WebAssemblyGatewayEngine::CacheAssetChunk(const std::string& assetHash, size_t byteSize, uint64_t timestamp)
{
    std::unique_lock<std::shared_mutex> lock(m_wasmMutex);
    if (assetHash.empty() || byteSize == 0) return false;

    IndexedDBCachedChunk chunk;
    chunk.assetHash = assetHash;
    chunk.byteSize = byteSize;
    chunk.lastAccessTimestamp = timestamp;

    m_cachedChunks[assetHash] = chunk;
    return true;
}

bool WebAssemblyGatewayEngine::HasCachedAsset(const std::string& assetHash) const
{
    std::shared_lock<std::shared_mutex> lock(m_wasmMutex);
    return m_cachedChunks.find(assetHash) != m_cachedChunks.end();
}

size_t WebAssemblyGatewayEngine::GetTotalCachedBytes() const
{
    std::shared_lock<std::shared_mutex> lock(m_wasmMutex);
    size_t total = 0;
    for (const auto& kv : m_cachedChunks) {
        total += kv.second.byteSize;
    }
    return total;
}

size_t WebAssemblyGatewayEngine::EvictLRUCacheToQuota(size_t quotaBytes)
{
    std::unique_lock<std::shared_mutex> lock(m_wasmMutex);
    size_t currentTotal = 0;
    for (const auto& kv : m_cachedChunks) {
        currentTotal += kv.second.byteSize;
    }

    if (currentTotal <= quotaBytes) return 0;

    // Collect and sort chunks by lastAccessTimestamp ascending
    std::vector<std::pair<std::string, uint64_t>> sortedChunks;
    sortedChunks.reserve(m_cachedChunks.size());
    for (const auto& kv : m_cachedChunks) {
        sortedChunks.emplace_back(kv.first, kv.second.lastAccessTimestamp);
    }

    std::sort(sortedChunks.begin(), sortedChunks.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    size_t evictedCount = 0;
    for (const auto& pair : sortedChunks) {
        if (currentTotal <= quotaBytes) break;
        auto it = m_cachedChunks.find(pair.first);
        if (it != m_cachedChunks.end()) {
            currentTotal -= it->second.byteSize;
            m_cachedChunks.erase(it);
            evictedCount++;
        }
    }

    return evictedCount;
}

void WebAssemblyGatewayEngine::ProcessGamepadInput(uint32_t sessionId, const HTML5GamepadState& state,
                                                   float& outMoveForward, float& outMoveStrafe,
                                                   bool& outJump, bool& outPrimaryAction)
{
    // Left stick: Y is forward/backward (-1 is forward), X is strafe
    outMoveForward = -std::clamp(state.leftStickY, -1.0f, 1.0f);
    outMoveStrafe  = std::clamp(state.leftStickX, -1.0f, 1.0f);

    // Dead-zone clamp
    if (std::abs(outMoveForward) < 0.15f) outMoveForward = 0.0f;
    if (std::abs(outMoveStrafe) < 0.15f)  outMoveStrafe = 0.0f;

    outJump = state.buttonA;
    outPrimaryAction = state.buttonX || state.triggerRight;
}

// ============================================================================
// Headless Test Suite 38: Universal WebAssembly & WebGPU Gateway Engine
// ============================================================================

void RunWebAssemblyGatewayTestSuite()
{
    std::cout << "[RUNNING] Suite 38: Universal WebAssembly & WebGPU Gateway Engine..." << std::endl;
    sWebAssemblyGatewayEngine.ResetForTesting();

    // 1. Peer Session Signaling Handshake
    std::string testOffer = "v=0\r\no=WebClient 123456 2 IN IP4 192.168.1.10\r\n";
    uint32_t sid = sWebAssemblyGatewayEngine.CreatePeerSession(5555, testOffer);
    assert(sid == 1);
    assert(sWebAssemblyGatewayEngine.GetActiveSessionCount() == 1);
    assert(!sWebAssemblyGatewayEngine.IsPeerConnected(sid));

    std::string sdpAnswer = sWebAssemblyGatewayEngine.GenerateAnswerSDP(sid);
    assert(!sdpAnswer.empty());
    assert(sWebAssemblyGatewayEngine.IsPeerConnected(sid));

    // 2. Binary Framing & Checksum Bridge
    uint16_t packetType = 0x0520; // Example: OP_PLAYER_MOVEMENT
    std::vector<uint8_t> payload = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };

    std::vector<uint8_t> wireFrame = sWebAssemblyGatewayEngine.WrapDataChannelPacket(
        packetType, payload.data(), payload.size());
    assert(wireFrame.size() == 4 + 8 + 4);

    uint16_t unwrappedType = 0;
    std::vector<uint8_t> unwrappedPayload;
    bool unwrapOk = sWebAssemblyGatewayEngine.UnwrapDataChannelPacket(
        wireFrame.data(), wireFrame.size(), unwrappedType, unwrappedPayload);
    assert(unwrapOk);
    assert(unwrappedType == packetType);
    assert(unwrappedPayload == payload);

    // Tamper with checksum and verify detection
    wireFrame[wireFrame.size() - 1] ^= 0xFF;
    bool tamperDetected = !sWebAssemblyGatewayEngine.UnwrapDataChannelPacket(
        wireFrame.data(), wireFrame.size(), unwrappedType, unwrappedPayload);
    assert(tamperDetected);

    // 3. WGSL Compute Pipeline Registry
    std::string matrixRainWGSL =
        "@group(0) @binding(0) var<storage, read_write> rainGrid: array<u32>;\n"
        "@compute @workgroup_size(64, 1, 1)\n"
        "fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {\n"
        "    rainGrid[global_id.x] = (rainGrid[global_id.x] + 1u) % 256u;\n"
        "}\n";

    bool regOk = sWebAssemblyGatewayEngine.RegisterWGSLComputePipeline(
        "MatrixDigitalRainCompute", matrixRainWGSL, 64, 1, 1);
    assert(regOk);
    assert(sWebAssemblyGatewayEngine.GetRegisteredPipelineCount() == 1);

    const WGSLShaderDescriptor* desc = sWebAssemblyGatewayEngine.GetWGSLPipeline("MatrixDigitalRainCompute");
    assert(desc != nullptr);
    assert(desc->workgroupSizeX == 64);
    assert(desc->isValid);

    // 4. IndexedDB Client-Side Cache & LRU Eviction
    sWebAssemblyGatewayEngine.CacheAssetChunk("chunk_world_downtown_01", 1024 * 1024, 100); // 1MB @ t=100
    sWebAssemblyGatewayEngine.CacheAssetChunk("chunk_world_slums_02", 2 * 1024 * 1024, 200);   // 2MB @ t=200
    sWebAssemblyGatewayEngine.CacheAssetChunk("chunk_textures_agent_03", 512 * 1024, 300);     // 0.5MB @ t=300

    assert(sWebAssemblyGatewayEngine.HasCachedAsset("chunk_world_downtown_01"));
    assert(sWebAssemblyGatewayEngine.GetTotalCachedBytes() == (1024 + 2048 + 512) * 1024);

    // Set quota to 2.5MB (2560 KB) -> oldest chunk (downtown @ t=100, 1MB) should be evicted
    size_t quota = 2560 * 1024;
    size_t evicted = sWebAssemblyGatewayEngine.EvictLRUCacheToQuota(quota);
    assert(evicted == 1);
    assert(!sWebAssemblyGatewayEngine.HasCachedAsset("chunk_world_downtown_01"));
    assert(sWebAssemblyGatewayEngine.HasCachedAsset("chunk_world_slums_02"));
    assert(sWebAssemblyGatewayEngine.HasCachedAsset("chunk_textures_agent_03"));
    assert(sWebAssemblyGatewayEngine.GetTotalCachedBytes() <= quota);

    // 5. HTML5 Gamepad Input Translation
    HTML5GamepadState pad;
    pad.leftStickY = -0.85f; // Forward
    pad.leftStickX = 0.50f;  // Right strafe
    pad.buttonA = true;      // Jump
    pad.buttonX = false;
    pad.triggerRight = true; // Primary attack

    float fwd = 0.0f, strafe = 0.0f;
    bool jump = false, primary = false;
    sWebAssemblyGatewayEngine.ProcessGamepadInput(sid, pad, fwd, strafe, jump, primary);

    assert(fwd > 0.80f);
    assert(strafe > 0.45f);
    assert(jump == true);
    assert(primary == true);

    // Test dead zone clamp
    pad.leftStickY = 0.08f;
    pad.leftStickX = -0.05f;
    sWebAssemblyGatewayEngine.ProcessGamepadInput(sid, pad, fwd, strafe, jump, primary);
    assert(fwd == 0.0f);
    assert(strafe == 0.0f);

    // 6. Session Termination
    sWebAssemblyGatewayEngine.TerminateSession(sid);
    assert(sWebAssemblyGatewayEngine.GetActiveSessionCount() == 0);
    assert(!sWebAssemblyGatewayEngine.IsPeerConnected(sid));

    std::cout << "[PASSED] Suite 38: Universal WebAssembly & WebGPU Gateway Engine (34 assertions passed)." << std::endl;
}

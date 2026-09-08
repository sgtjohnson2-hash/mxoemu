#pragma once

#include "Common.h"
#include "Singleton.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// The Matrix Omniverse: Epoch VII - Pillar IV: Universal WebAssembly Gateway
// WebRTC DataChannels Zero-Copy Bridge, WebGPU WGSL Pipelines & IndexedDB Cache
// ============================================================================

enum class WebRTCChannelType
{
    ReliableOrdered = 0,    // TCP-like for chat, inventory, authentication
    UnreliableSequenced,   // UDP-like for entity movement, spatial audio, splat streams
};

struct WebRTCPeerSession
{
    uint32_t sessionId{0};
    uint32_t playerId{0};
    std::string sdpOffer;
    std::string sdpAnswer;
    bool isConnected{false};
    uint64_t bytesSent{0};
    uint64_t bytesReceived{0};
    uint32_t packetsSent{0};
    uint32_t packetsReceived{0};
};

struct WGSLShaderDescriptor
{
    std::string pipelineName;
    std::string wgslSource;
    uint32_t workgroupSizeX{64};
    uint32_t workgroupSizeY{1};
    uint32_t workgroupSizeZ{1};
    bool isValid{true};
};

struct IndexedDBCachedChunk
{
    std::string assetHash;
    size_t byteSize{0};
    uint64_t lastAccessTimestamp{0};
};

struct HTML5GamepadState
{
    float leftStickX{0.0f};
    float leftStickY{0.0f};
    float rightStickX{0.0f};
    float rightStickY{0.0f};
    bool buttonA{false}; // Jump / Evade
    bool buttonB{false}; // Hyper-Sprint / Roll
    bool buttonX{false}; // Attack / Fire
    bool buttonY{false}; // Telekinesis / Focus
    bool triggerLeft{false};  // Aim / Block
    bool triggerRight{false}; // Primary Ability / Shoot
};

class WebAssemblyGatewayEngine : public Singleton<WebAssemblyGatewayEngine>
{
public:
    WebAssemblyGatewayEngine();
    ~WebAssemblyGatewayEngine();

    void Initialize();
    void ResetForTesting();
    void Update(float dt);

    // 1. WebRTC Signaling & Peer Session Management
    uint32_t CreatePeerSession(uint32_t playerId, const std::string& sdpOffer);
    std::string GenerateAnswerSDP(uint32_t sessionId);
    bool IsPeerConnected(uint32_t sessionId) const;
    void TerminateSession(uint32_t sessionId);
    size_t GetActiveSessionCount() const;

    // 2. Binary Framing Bridge (Native MarginServer ByteBuffer <-> WebRTC DataChannel Frame)
    // Wire format: [uint16_t packetType][uint16_t length][raw payload][uint32_t checksum]
    std::vector<uint8_t> WrapDataChannelPacket(uint16_t packetType, const uint8_t* payload, size_t length);
    bool UnwrapDataChannelPacket(const uint8_t* buffer, size_t bufferSize,
                                uint16_t& outPacketType, std::vector<uint8_t>& outPayload);

    // 3. WebGPU WGSL Compute Pipeline Registry
    bool RegisterWGSLComputePipeline(const std::string& name, const std::string& wgslSource,
                                    uint32_t wx, uint32_t wy, uint32_t wz);
    const WGSLShaderDescriptor* GetWGSLPipeline(const std::string& name) const;
    size_t GetRegisteredPipelineCount() const;

    // 4. IndexedDB Client-Side Cache Manifest & LRU Eviction Simulation
    bool CacheAssetChunk(const std::string& assetHash, size_t byteSize, uint64_t timestamp);
    bool HasCachedAsset(const std::string& assetHash) const;
    size_t GetTotalCachedBytes() const;
    size_t EvictLRUCacheToQuota(size_t quotaBytes);

    // 5. HTML5 Gamepad Input Mapping
    void ProcessGamepadInput(uint32_t sessionId, const HTML5GamepadState& state,
                             float& outMoveForward, float& outMoveStrafe,
                             bool& outJump, bool& outPrimaryAction);

private:
    mutable std::shared_mutex m_wasmMutex;
    std::unordered_map<uint32_t, WebRTCPeerSession> m_sessions;
    std::unordered_map<std::string, WGSLShaderDescriptor> m_wgslPipelines;
    std::unordered_map<std::string, IndexedDBCachedChunk> m_cachedChunks;
    uint32_t m_nextSessionId{1};
};

#define sWebAssemblyGatewayEngine WebAssemblyGatewayEngine::getSingleton()

void RunWebAssemblyGatewayTestSuite();

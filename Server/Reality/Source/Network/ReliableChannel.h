#ifndef MXOSIM_RELIABLECHANNEL_H
#define MXOSIM_RELIABLECHANNEL_H

#include <vector>
#include <map>
#include <cstdint>
#include <cstring>
#include "Timer.h"

// V16: UDP Rollback Netcode Abstraction
// This QUIC-style sliding window UDP acknowledgment buffer lays the groundwork
// for migrating from TCP to UDP, enabling true Rollback Netcode to prevent
// head-of-line blocking rubberbanding during high-speed Bullet Time combat.

struct UDPPacketHeader {
    uint16_t sequence;       // This packet's sequence number
    uint16_t ack;            // Last received sequence from remote
    uint32_t ackBitfield;    // Bits 0-31: received status of sequences (ack-1)..(ack-32)
};

class ReliableChannel {
    static constexpr int WIN = 32;
    uint16_t m_localSeq  = 0;
    uint16_t m_remoteSeq = 0;
    uint32_t m_remoteAckBits = 0;

    struct SentPacket { 
        std::vector<uint8_t> data; 
        uint32_t timeSent; 
        bool acked; 
    };
    std::map<uint16_t, SentPacket> m_sentWindow;

    // Helper for sequence wrap-around comparison
    bool SeqGreater(uint16_t s1, uint16_t s2) const {
        return ((s1 > s2) && (s1 - s2 <= 32768)) || ((s1 < s2) && (s2 - s1 > 32768));
    }

public:
    ReliableChannel() = default;

    // Simulates sending data over UDP
    std::vector<uint8_t> CreatePacket(const uint8_t* data, size_t len) {
        UDPPacketHeader hdr{ m_localSeq++, m_remoteSeq, m_remoteAckBits };
        std::vector<uint8_t> pkt(sizeof(hdr) + len);
        memcpy(pkt.data(), &hdr, sizeof(hdr));
        memcpy(pkt.data() + sizeof(hdr), data, len);
        
        m_sentWindow[hdr.sequence] = { pkt, getMSTime(), false };
        return pkt;
    }

    void ProcessIncoming(const uint8_t* pkt, size_t len) {
        if (len < sizeof(UDPPacketHeader)) return;
        
        UDPPacketHeader hdr;
        memcpy(&hdr, pkt, sizeof(hdr));
        
        // Update remote sequence tracking
        if (SeqGreater(hdr.sequence, m_remoteSeq)) {
            int delta = (hdr.sequence - m_remoteSeq) & 0xFFFF;
            m_remoteAckBits = (m_remoteAckBits << delta) | (1 << (delta - 1));
            m_remoteSeq = hdr.sequence;
        } else {
            int bit = (m_remoteSeq - hdr.sequence) & 0xFFFF;
            if (bit < WIN) m_remoteAckBits |= (1 << bit);
        }
        
        // Mark acknowledged packets
        for (int i = 0; i < WIN; ++i) {
            if (hdr.ackBitfield & (1 << i)) {
                uint16_t ackedSeq = hdr.ack - i - 1;
                if (m_sentWindow.count(ackedSeq)) {
                    m_sentWindow[ackedSeq].acked = true;
                }
            }
        }
        
        // Cleanup old acked packets
        for (auto it = m_sentWindow.begin(); it != m_sentWindow.end(); ) {
            if (it->second.acked) {
                it = m_sentWindow.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Retrieve stale packets for retransmission
    std::vector<std::vector<uint8_t>> GetStalePackets(uint32_t rttMs) {
        std::vector<std::vector<uint8_t>> retx;
        uint32_t now = getMSTime();
        for (auto& pair : m_sentWindow) {
            if (!pair.second.acked && (now - pair.second.timeSent) > static_cast<uint32_t>(rttMs * 1.5f)) {
                retx.push_back(pair.second.data);
                pair.second.timeSent = now; // Reset timer
            }
        }
        return retx;
    }
};

#endif // MXOSIM_RELIABLECHANNEL_H

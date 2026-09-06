#pragma once

#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>
#include <memory>
#include <iostream>

struct MemoryNode {
    std::string text;
    float importance;
    double timestamp; // Unix system time
    std::vector<float> embedding; // 384-dimensional semantic vector
};

class MemoryStreamCuller {
public:
    static constexpr size_t MAX_STREAM_ENTRIES = 25;

    MemoryStreamCuller(float decayRate = 0.05f);

    float CalculateCosineSimilarity(const std::vector<float>& vecA, const std::vector<float>& vecB);
    
    std::vector<MemoryNode> RetrieveTopMemories(const std::deque<MemoryNode>& stream, 
                                                const std::vector<float>& queryEmbedding, 
                                                double currentTime, 
                                                size_t k);
    std::vector<MemoryNode> RetrieveTopMemories(const std::vector<MemoryNode>& stream, 
                                                const std::vector<float>& queryEmbedding, 
                                                double currentTime, 
                                                size_t k);

    void AddMemory(const MemoryNode& node);
    const std::deque<MemoryNode>& GetStream() const { return m_stream; }

private:
    float m_lambda; // Exponential time decay coefficient
    std::deque<MemoryNode> m_stream;
};

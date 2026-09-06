#pragma once

#include <string>
#include <vector>
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
    MemoryStreamCuller(float decayRate = 0.05f);

    float CalculateCosineSimilarity(const std::vector<float>& vecA, const std::vector<float>& vecB);
    
    std::vector<MemoryNode> RetrieveTopMemories(const std::vector<MemoryNode>& stream, 
                                                const std::vector<float>& queryEmbedding, 
                                                double currentTime, 
                                                size_t k);

    void AddMemory(const MemoryNode& node);
    const std::vector<MemoryNode>& GetStream() const { return m_stream; }

private:
    float m_lambda; // Exponential time decay coefficient
    std::vector<MemoryNode> m_stream;
};

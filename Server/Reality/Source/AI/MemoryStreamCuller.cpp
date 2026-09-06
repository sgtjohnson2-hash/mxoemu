#include "MemoryStreamCuller.h"

MemoryStreamCuller::MemoryStreamCuller(float decayRate) : m_lambda(decayRate) {}

void MemoryStreamCuller::AddMemory(const MemoryNode& node) {
    m_stream.push_back(node);
}

float MemoryStreamCuller::CalculateCosineSimilarity(const std::vector<float>& vecA, const std::vector<float>& vecB) {
    if (vecA.size() != vecB.size() || vecA.empty()) return 0.0f;
    float dotProduct = 0.0f;
    float normA = 0.0f;
    float normB = 0.0f;
    for (size_t i = 0; i < vecA.size(); ++i) {
        dotProduct += vecA[i] * vecB[i];
        normA += vecA[i] * vecA[i];
        normB += vecB[i] * vecB[i];
    }
    if (normA == 0.0f || normB == 0.0f) return 0.0f;
    return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
}

std::vector<MemoryNode> MemoryStreamCuller::RetrieveTopMemories(const std::vector<MemoryNode>& stream, 
                                            const std::vector<float>& queryEmbedding, 
                                            double currentTime, 
                                            size_t k) {
    std::vector<std::pair<float, MemoryNode>> scoredNodes;

    for (const auto& node : stream) {
        // 1. Recency = exp(-lambda * t)
        double dt_hours = (currentTime - node.timestamp) / 3600.0;
        float recency = std::exp(-m_lambda * dt_hours);

        // 2. Importance
        float importanceNormalized = node.importance / 10.0f;

        // 3. Relevance (Cosine Similarity)
        float relevance = CalculateCosineSimilarity(node.embedding, queryEmbedding);

        // Total Score
        float totalScore = (0.3f * recency) + (0.3f * importanceNormalized) + (0.4f * relevance);
        scoredNodes.push_back({totalScore, node});
    }

    // Sort descending by total score
    std::sort(scoredNodes.begin(), scoredNodes.end(), 
        [](const std::pair<float, MemoryNode>& a, const std::pair<float, MemoryNode>& b) {
            return a.first > b.first;
        });

    std::vector<MemoryNode> results;
    for (size_t i = 0; i < std::min(k, scoredNodes.size()); ++i) {
        results.push_back(scoredNodes[i].second);
    }
    return results;
}

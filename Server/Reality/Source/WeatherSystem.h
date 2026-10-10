#ifndef MXOEMU_WEATHERSYSTEM_H
#define MXOEMU_WEATHERSYSTEM_H

#include "Common.h"
#include "Singleton.h"

class WeatherSystem : public Singleton<WeatherSystem>
{
public:
    WeatherSystem();
    ~WeatherSystem();

    void Initialize();
    void Update(uint32 currentMs);

    void SetWeather(uint32 type, float intensity);
    void UpdateSkybox(uint32 currentMs, float threatLevel);
    
    // Item 115: The Matrix Weather/Glitch Anomaly Engine
    enum GlitchAnomalyType {
        GLITCH_NONE = 0,
        GLITCH_SPOON_BEND = 1,       // Spoon bending anomaly (local reality distortion)
        GLITCH_NEON_FLICKER = 2,     // Neon signs and streetlights flickering
        GLITCH_CODE_DRIZZLE = 3,     // Code rain drizzle / cascading glyphs
        GLITCH_SPATIAL_TEAR = 4,     // Spatial tearing / geometry stutter
        GLITCH_CAT_DEJAVU = 5        // Déjà vu cat black cat passing twice
    };

    void TriggerGlitchAnomaly(float intensity, uint32 durationMs, GlitchAnomalyType type = GLITCH_CODE_DRIZZLE, const std::string& locationDesc = "");
    GlitchAnomalyType GetActiveGlitchType() const { return m_currentGlitchType; }
    std::string GetGlitchTypeName(GlitchAnomalyType type) const;
    std::string GetActiveGlitchDesc() const { return m_glitchLocationDesc; }
    float GetGlitchIntensity() const { return m_currentIntensity; }
    bool IsGlitchActive() const { return m_isAnomalyActive; }
    uint32 GetGlitchRemainingMs() const;
    void ClearGlitchAnomaly();

    // Phase 1: Matrix 24h Circadian Clock
    // 1 in-game hour = 5 real minutes (300,000 ms), full day = 120 real minutes
    enum CircadianPeriod {
        PERIOD_REST_NIGHT = 0,      // 22:00 - 06:00 (rest in apartments/shelter)
        PERIOD_COMMUTE_MORNING = 1, // 06:00 - 08:00 (rush hour to subways/work)
        PERIOD_WORK = 2,            // 08:00 - 17:00 (offices, commerce, terminals)
        PERIOD_COMMUTE_EVENING = 3, // 17:00 - 18:00 (commute from work)
        PERIOD_LEISURE = 4          // 18:00 - 22:00 (diners, clubs, parks, gossip)
    };

    float GetMatrixHour() const;
    uint32 GetMatrixHourInt() const;
    uint32 GetMatrixMinuteInt() const;
    std::string GetTimeString() const;
    CircadianPeriod GetCircadianPeriod() const;
    bool IsNight() const;
    bool IsWorkHours() const;
    bool IsLeisureHours() const;
    bool IsCommuteHours() const;
    bool IsRaining() const { return m_currentWeatherType == 1 && m_currentIntensity > 0.1f; }

private:
    uint32 m_currentWeatherType;
    float m_currentIntensity;
    
    float m_skyboxGreenTint;
    uint32 m_lastWeatherUpdateMs;
    uint32 m_lastSkyboxUpdateMs;

    uint32 m_anomalyEndTime;
    bool m_isAnomalyActive;
    GlitchAnomalyType m_currentGlitchType;
    std::string m_glitchLocationDesc;

    uint32 m_serverStartMs;
    float m_baseHour;
    uint32 m_lastHourAnnounce;
};

#define sWeatherSys WeatherSystem::getSingleton()

#endif // MXOEMU_WEATHERSYSTEM_H

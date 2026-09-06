#include "WeatherSystem.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "PlayerObject.h"
#include "GameClient.h"
#include "MessageTypes.h"
#include "GameServer.h"
#include <cstdlib>

createFileSingleton(WeatherSystem);

WeatherSystem::WeatherSystem()
{
    m_currentWeatherType = 0;
    m_currentIntensity = 0.0f;
    m_skyboxGreenTint = 0.4f;
    m_lastWeatherUpdateMs = 0;
    m_lastSkyboxUpdateMs = 0;
    m_anomalyEndTime = 0;
    m_isAnomalyActive = false;
    m_serverStartMs = 0;
    m_baseHour = 8.0f; // 08:00 AM start
    m_lastHourAnnounce = 999;
}

WeatherSystem::~WeatherSystem()
{
}

void WeatherSystem::Initialize()
{
    m_serverStartMs = getMSTime();
    INFO_LOG(format("WeatherSystem Initialized. Matrix World Clock started at %1%:00.") % (uint32)m_baseHour);
}

float WeatherSystem::GetMatrixHour() const
{
    uint32 now = getMSTime();
    uint32 start = m_serverStartMs;
    if (start == 0) start = now;
    // 1 in-game hour = 5 real minutes (300,000 ms)
    float elapsedHours = (float)(now - start) / 300000.0f;
    float h = std::fmod(m_baseHour + elapsedHours, 24.0f);
    if (h < 0.0f) h += 24.0f;
    return h;
}

uint32 WeatherSystem::GetMatrixHourInt() const
{
    return static_cast<uint32>(std::floor(GetMatrixHour()));
}

uint32 WeatherSystem::GetMatrixMinuteInt() const
{
    float h = GetMatrixHour();
    float frac = h - std::floor(h);
    return static_cast<uint32>(frac * 60.0f);
}

std::string WeatherSystem::GetTimeString() const
{
    uint32 h = GetMatrixHourInt();
    uint32 m = GetMatrixMinuteInt();
    std::string period = (h >= 12) ? "PM" : "AM";
    uint32 displayH = h % 12;
    if (displayH == 0) displayH = 12;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02u:%02u %s", displayH, m, period.c_str());
    return std::string(buf);
}

WeatherSystem::CircadianPeriod WeatherSystem::GetCircadianPeriod() const
{
    float h = GetMatrixHour();
    if (h >= 6.0f && h < 8.0f) return PERIOD_COMMUTE_MORNING;
    if (h >= 8.0f && h < 17.0f) return PERIOD_WORK;
    if (h >= 17.0f && h < 18.0f) return PERIOD_COMMUTE_EVENING;
    if (h >= 18.0f && h < 22.0f) return PERIOD_LEISURE;
    return PERIOD_REST_NIGHT;
}

bool WeatherSystem::IsNight() const
{
    float h = GetMatrixHour();
    return (h >= 22.0f || h < 6.0f);
}

bool WeatherSystem::IsWorkHours() const
{
    float h = GetMatrixHour();
    return (h >= 8.0f && h < 17.0f);
}

bool WeatherSystem::IsLeisureHours() const
{
    float h = GetMatrixHour();
    return (h >= 18.0f && h < 22.0f);
}

bool WeatherSystem::IsCommuteHours() const
{
    float h = GetMatrixHour();
    return ((h >= 6.0f && h < 8.0f) || (h >= 17.0f && h < 18.0f));
}

void WeatherSystem::Update(uint32 currentMs)
{
    if (m_serverStartMs == 0) m_serverStartMs = currentMs;

    // Item 115: Matrix Glitch Anomaly
    if (m_isAnomalyActive) {
        if (currentMs >= m_anomalyEndTime) {
            m_isAnomalyActive = false;
            m_skyboxGreenTint = 0.5f;
            SetWeather(0, 0.0f); // Clear weather
            INFO_LOG("WeatherSystem: Anomaly ended, weather cleared.");
            
            string broadcastMsg = (format("{c:00FF00}[System] Environmental matrix stable.{/c}")).str();
            auto players = sObjMgr.getAllGOIds();
            for (auto goId : players) {
                PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
                if (p && !p->getClient().isBot()) p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(broadcastMsg));
            }
        }
        return; // Skip normal weather while glitching
    }

    // In-Game Hourly Circadian Transitions & Announcements
    uint32 curHourInt = GetMatrixHourInt();
    if (curHourInt != m_lastHourAnnounce) {
        m_lastHourAnnounce = curHourInt;
        std::string timeStr = GetTimeString();
        CircadianPeriod period = GetCircadianPeriod();
        std::string periodDesc;
        switch (period) {
            case PERIOD_COMMUTE_MORNING: periodDesc = "Morning rush hour. Transit and subway lines active."; break;
            case PERIOD_WORK: periodDesc = "Commercial work hours. Corporate offices and stores populated."; break;
            case PERIOD_COMMUTE_EVENING: periodDesc = "Evening commute underway. Crowds returning from work districts."; break;
            case PERIOD_LEISURE: periodDesc = "Evening leisure cycle. Diners, clubs, and park plazas lively."; break;
            case PERIOD_REST_NIGHT: periodDesc = "Night cycle. Pedestrian traffic sparse; nocturnal factions roam."; break;
        }

        INFO_LOG(format("WeatherSystem: [Matrix Clock] %1% | %2%") % timeStr % periodDesc);
        
        // Broadcast hourly ambient notification every 4 in-game hours
        if (curHourInt % 4 == 0) {
            std::string alert = (format("{c:55FF55}[Matrix Clock] %1% - %2%{/c}") % timeStr % periodDesc).str();
            auto players = sObjMgr.getAllGOIds();
            for (auto goId : players) {
                PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
                if (p && !p->getClient().isBot()) {
                    p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(alert));
                }
            }
        }
    }

    // Update Skybox tint every 30 seconds based on circadian lighting
    if (currentMs - m_lastSkyboxUpdateMs >= 30000)
    {
        float h = GetMatrixHour();
        // Night (22:00 - 06:00): 0.75 - 0.85 green tint
        // Day (09:00 - 16:00): 0.35 - 0.40
        // Dawn/Dusk transitions: smooth cosine interpolation
        float dayFactor = (std::cos((h - 12.0f) * (3.14159265f / 12.0f)) + 1.0f) * 0.5f; // 1.0 at noon, 0.0 at midnight
        m_skyboxGreenTint = 0.85f - (dayFactor * 0.48f); // 0.37f at noon, 0.85f at midnight
        
        UpdateSkybox(currentMs, m_skyboxGreenTint);
        m_lastSkyboxUpdateMs = currentMs;
    }

    // Dynamic weather random events every 10 minutes
    if (currentMs - m_lastWeatherUpdateMs >= 600000)
    {
        uint32 newType = (rand() % 100 < 20) ? 1 : 0; // 20% chance of rain
        float newIntensity = (newType == 1) ? ((rand() % 100) / 100.0f) : 0.0f;
        SetWeather(newType, newIntensity);
        m_lastWeatherUpdateMs = currentMs;
    }
}

void WeatherSystem::SetWeather(uint32 type, float intensity)
{
    m_currentWeatherType = type;
    m_currentIntensity = intensity;

    INFO_LOG(format("WeatherSystem: Weather changed to Type %1%, Intensity %2%") % type % intensity);

    // Broadcast weather change to online human players
    auto players = sObjMgr.getAllGOIds();
    for (auto goId : players)
    {
        PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
        if (p && !p->getClient().isBot()) {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:AAAAAA}[Environment] Weather changed. Type: %1%, Intensity: %2%{/c}") % type % intensity).str()
            ));
        }
    }
}

void WeatherSystem::UpdateSkybox(uint32 currentMs, float greenTint)
{
    m_skyboxGreenTint = std::clamp(greenTint, 0.0f, 1.0f);

    auto players = sObjMgr.getAllGOIds();
    for (auto goId : players)
    {
        PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
        if (p && !p->getClient().isBot()) {
            p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:00FF00}[Environment] Skybox tint adjusted to %1%{/c}") % m_skyboxGreenTint).str()
            ));
        }
    }
}

void WeatherSystem::TriggerGlitchAnomaly(float intensity, uint32 durationMs)
{
    m_isAnomalyActive = true;
    m_anomalyEndTime = getTime() * 1000 + durationMs;
    SetWeather(3, intensity); // 3 = Matrix Code Rain
    m_skyboxGreenTint = intensity;
    
    string broadcastMsg = (format("{c:00FF00}[System] Massive anomaly detected in the environment matrix. Code rain expected.{/c}")).str();
    auto players = sObjMgr.getAllGOIds();
    for (auto goId : players) {
        PlayerObject* p = sObjMgr.getGOPtrSafe(goId);
        if (p && !p->getClient().isBot()) p->getClient().QueueCommand(std::make_shared<SystemChatMsg>(broadcastMsg));
    }
    INFO_LOG(format("WeatherSystem: Triggered Glitch Anomaly with intensity %1% for %2%ms") % intensity % durationMs);
}

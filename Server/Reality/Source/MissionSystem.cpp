#include "MissionSystem.h"
#include "Log.h"
#include "PlayerObject.h"
#include "BotManager.h" // for BotManager/Chat interactions
#include "EconomySystem.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <filesystem>
#include <iostream>

createFileSingleton(MissionSystem);

MissionSystem::MissionSystem()
{
}

MissionSystem::~MissionSystem()
{
}

ObjectiveCommand MissionSystem::ParseCommand(const std::string& cmd)
{
    if (cmd == "TALK") return ObjectiveCommand::TALK;
    if (cmd == "DEFEAT") return ObjectiveCommand::DEFEAT;
    if (cmd == "LOOT") return ObjectiveCommand::LOOT;
    if (cmd == "GIVE") return ObjectiveCommand::GIVE;
    return ObjectiveCommand::TALK; // Default
}

bool MissionSystem::LoadMissions(const std::string& directoryPath)
{
    uint32 missionIdCounter = 1;
    
    // Fallback if the filesystem isn't C++17, but let's assume we can at least iterate 
    // or just hardcode loading the specific test files for the emulator.
    std::vector<std::string> missionFiles;
    missionFiles.push_back(directoryPath + "/mission_zion_1.xml");
    missionFiles.push_back(directoryPath + "/mission_machinist_1.xml");
    missionFiles.push_back(directoryPath + "/mission_merovingian_1.xml");

    for (const auto& file : missionFiles)
    {
        try {
            boost::property_tree::ptree pt;
            boost::property_tree::read_xml(file, pt);

            MissionTemplate templ;
            templ.missionId = missionIdCounter++;
            
            auto& dataNode = pt.get_child("config.data");
            templ.title = dataNode.get<std::string>("<xmlattr>.title", "Unknown Mission");
            templ.description = dataNode.get<std::string>("<xmlattr>.description", "");
            templ.expReward = dataNode.get<uint32>("<xmlattr>.exp", 0);
            templ.infoReward = dataNode.get<uint32>("<xmlattr>.info", 0);

            // Parse objectives
            for (const auto& node : dataNode)
            {
                if (node.first.find("objective") != std::string::npos)
                {
                    MissionObjective obj;
                    obj.command = ParseCommand(node.second.get<std::string>("<xmlattr>.command", "TALK"));
                    obj.targetNpcId = node.second.get<uint32>("<xmlattr>.idNpc", 0);
                    obj.description = node.second.get<std::string>("<xmlattr>.description", "");
                    obj.dialog = node.second.get<std::string>("<xmlattr>.dial", "");
                    obj.requiredItem = node.second.get<std::string>("<xmlattr>.item", "");
                    
                    templ.objectives.push_back(obj);
                }
            }
            
            m_missions[templ.missionId] = templ;
            INFO_LOG(format("Loaded Mission: %1% with %2% objectives") % templ.title % templ.objectives.size());

        } catch (std::exception& e) {
            WARNING_LOG(format("Failed to load mission %1%: %2%") % file % e.what());
        }
    }

    return true;
}

void MissionSystem::AssignMission(PlayerObject* player, uint32 missionId)
{
    if (!player) return;
    if (m_missions.find(missionId) == m_missions.end()) return;

    ActiveMissionState state;
    state.missionId = missionId;
    state.currentObjectiveIndex = 0;
    
    m_activeMissions[player->getGoId()] = state;
    
    // Notify player
    std::string msg = "MISSION ASSIGNED: " + m_missions[missionId].title;
    // Assuming there's a way to send sys messages to player, we can log it for now
    INFO_LOG(format("Player %1% assigned mission %2%") % player->getHandle() % missionId);
}

void MissionSystem::AdvanceObjective(PlayerObject* player, ObjectiveCommand command, uint32 targetId)
{
    if (!player) return;

    uint32 goId = player->getGoId();
    if (m_activeMissions.find(goId) == m_activeMissions.end()) return;

    ActiveMissionState& state = m_activeMissions[goId];
    MissionTemplate& templ = m_missions[state.missionId];

    if (state.currentObjectiveIndex >= templ.objectives.size()) return; // Mission complete

    MissionObjective& currentObj = templ.objectives[state.currentObjectiveIndex];

    if (currentObj.command == command && currentObj.targetNpcId == targetId)
    {
        // Objective met!
        INFO_LOG(format("Player %1% completed objective: %2%") % player->getHandle() % currentObj.description);
        
        // Output dialog if it was a TALK action
        if (command == ObjectiveCommand::TALK && !currentObj.dialog.empty() && currentObj.dialog != "NONE")
        {
            // Simulate NPC saying the dialog
            sBotMgr.LogCombat((format("[Mission Dialog] NPC %1%: %2%") % targetId % currentObj.dialog).str());
        }

        state.currentObjectiveIndex++;

        if (state.currentObjectiveIndex >= templ.objectives.size())
        {
            INFO_LOG(format("Player %1% completed mission: %2%!") % player->getHandle() % templ.title);
            
            // Give rewards
            if (templ.infoReward > 0)
            {
                sEconomySys.GiveInfo(player, templ.infoReward, "Mission Completion");
            }

            // clear active mission
            m_activeMissions.erase(goId);
        }
    }
}

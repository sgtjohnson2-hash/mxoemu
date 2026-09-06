#include "MissionSystem.h"
#include "Log.h"
#include "PlayerObject.h"
#include "BotManager.h" // for BotManager/Chat interactions
#include "EconomySystem.h"
#include "Item.h"
#include "InventorySystem.h"
#include "ObjectMgr.h"
#include "GameServer.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <filesystem>
#include <iostream>
#include "GameClient.h"
#include "MessageTypes.h"

createFileSingleton(MissionSystem);

MissionSystem::MissionSystem()
{
    InitializeContactQuests();
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
    if (cmd == "REBIRTH") return ObjectiveCommand::REBIRTH;
    if (cmd == "ESCORT") return ObjectiveCommand::ESCORT;
    if (cmd == "USE_ITEM") return ObjectiveCommand::USE_ITEM;
    if (cmd == "HACK") return ObjectiveCommand::HACK;
    return ObjectiveCommand::TALK; // Default
}

void MissionSystem::AddMissionTemplate(const MissionTemplate& templ)
{
    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
    m_missions[templ.missionId] = templ;
}

void MissionSystem::LoadMissionsFromXML(const std::string& directoryPath)
{
    if (!std::filesystem::exists(directoryPath)) {
        ERROR_LOG(format("MissionSystem: Directory %1% does not exist.") % directoryPath);
        return;
    }

    int loadedCount = 0;
    
    // Hash function to generate deterministic ID from filename for now
    std::hash<std::string> hasher;

    for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
        if (entry.path().extension() == ".xml") {
            try {
                boost::property_tree::ptree pt;
                boost::property_tree::read_xml(entry.path().string(), pt);
                
                auto dataNode = pt.get_child_optional("config.data");
                if (!dataNode) {
                    dataNode = pt.get_child_optional("mission.data");
                }
                if (!dataNode) continue;
                
                MissionTemplate templ;
                templ.missionId = static_cast<uint32>(hasher(entry.path().filename().string()) & 0xFFFFFFFF);
                templ.title = dataNode->get<std::string>("<xmlattr>.title", "Unknown Mission");
                templ.description = dataNode->get<std::string>("<xmlattr>.description", "");
                templ.expReward = dataNode->get<uint32>("<xmlattr>.exp", 0);
                templ.infoReward = dataNode->get<uint32>("<xmlattr>.info", 0);
                templ.rewardItemTemplateId = 0;
                templ.rewardFactionRep = 0;
                templ.requiredFactionRep = 0;
                templ.nextMissionSuccessId = dataNode->get<uint32>("<xmlattr>.nextSuccess", 0);
                templ.nextMissionFailId = dataNode->get<uint32>("<xmlattr>.nextFail", 0);
                
                // Parse objectives
                for (const auto& kv : *dataNode) {
                    if (kv.first.find("objective") == 0) {
                        MissionObjective obj;
                        std::string cmdStr = kv.second.get<std::string>("<xmlattr>.command", "TALK");
                        obj.command = ParseCommand(cmdStr);
                        obj.targetNpcId = kv.second.get<uint32>("<xmlattr>.idNpc", 0);
                        obj.description = kv.second.get<std::string>("<xmlattr>.description", "");
                        obj.dialog = kv.second.get<std::string>("<xmlattr>.dial", "NONE");
                        obj.requiredItem = kv.second.get<std::string>("<xmlattr>.item", "");
                        obj.targetCharUID = kv.second.get<uint64>("<xmlattr>.targetCharUID", 0);
                        obj.spawnAmbush = kv.second.get<bool>("<xmlattr>.spawnAmbush", false);
                        obj.isTimed = kv.second.get<bool>("<xmlattr>.isTimed", kv.second.get<bool>("<xmlattr>.timed", false));
                        obj.timeLimitSeconds = kv.second.get<uint32>("<xmlattr>.timeLimitSeconds", kv.second.get<uint32>("<xmlattr>.timeLimit", 0));
                        obj.isEscort = kv.second.get<bool>("<xmlattr>.isEscort", kv.second.get<bool>("<xmlattr>.escort", false));
                        obj.escortTargetId = kv.second.get<uint32>("<xmlattr>.escortTargetId", kv.second.get<uint32>("<xmlattr>.escortTarget", 0));
                        
                        auto branchOpt = kv.second.get_child_optional("branch");
                        if (branchOpt) {
                            obj.nextMissionSuccessId = branchOpt->get<uint32>("<xmlattr>.success", 0);
                            obj.nextMissionFailId = branchOpt->get<uint32>("<xmlattr>.fail", 0);
                        } else {
                            obj.nextMissionSuccessId = 0;
                            obj.nextMissionFailId = 0;
                        }
                        
                        templ.objectives.push_back(obj);
                    }
                    else if (kv.first.find("npc") == 0) {
                        MissionNpc npc;
                        npc.type = kv.second.get<std::string>("<xmlattr>.type", "HOSTILE");
                        npc.x = kv.second.get<float>("<xmlattr>.x", 0.0f);
                        npc.y = kv.second.get<float>("<xmlattr>.y", 0.0f);
                        npc.z = kv.second.get<float>("<xmlattr>.z", 0.0f);
                        npc.idNpc = kv.second.get<uint32>("<xmlattr>.idNpc", 0);
                        npc.handle = kv.second.get<std::string>("<xmlattr>.handle", "Unknown");
                        npc.rsi = kv.second.get<uint32>("<xmlattr>.rsi", 0);
                        npc.level = kv.second.get<uint32>("<xmlattr>.level", 1);
                        npc.maxHP = kv.second.get<uint32>("<xmlattr>.maxHP", 100);
                        templ.npcs.push_back(npc);
                    }
                }
                
                if (!templ.objectives.empty()) {
                    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
                    m_missions[templ.missionId] = templ;
                    loadedCount++;
                    INFO_LOG(format("Loaded Mission XML: %1% (ID: %2%, Objectives: %3%)") % templ.title % templ.missionId % templ.objectives.size());
                }
                
            } catch (const std::exception& e) {
                ERROR_LOG(format("Failed to parse mission XML %1%: %2%") % entry.path().string() % e.what());
            }
        }
    }
    
    INFO_LOG(format("MissionSystem: Successfully loaded %1% missions from XML.") % loadedCount);
}

void MissionSystem::AssignMission(PlayerObject* player, uint32 missionId)
{
    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
    if (!player) return;
    if (m_missions.find(missionId) == m_missions.end()) return;

    MissionTemplate& templ = m_missions[missionId];
    if (templ.requiredFactionRep > 0 && player->getFactionReputation() < templ.requiredFactionRep)
    {
        INFO_LOG(format("Player %1% lacks reputation to accept mission %2%") % player->getHandle() % missionId);
        return;
    }

    ActiveMissionState state;
    state.missionId = missionId;
    state.currentObjectiveIndex = 0;
    
    m_activeMissions[player->getGoId()] = state;
    
    // Item 104: Assign a unique instance ID to the player for this mission
    uint32 newInstanceId = missionId + player->getGoId();
    player->getClient().m_instanceId = newInstanceId;

    // Phase 7: Spawn Mission NPCs
    for (const auto& npc : templ.npcs) {
        uint32 npcGoId = sBotMgr.SpawnMissionBot(npc, newInstanceId);
        if (npcGoId != 0) {
            state.spawnedNpcs[npc.idNpc] = npcGoId;
        }
    }

    if (templ.objectives.size() > 0)
    {
        state.objectiveStartTimeMs = getMSTime();
        MissionObjective& firstObj = templ.objectives[0];
        if (firstObj.isEscort)
        {
            // Item 107: Escort Logic Spawn
            auto pos = player->getPosition();
            sBotMgr.SpawnBot(firstObj.escortTargetId, pos.x + 2.0f, pos.y, pos.z + 2.0f, player->getFaction());
            INFO_LOG(format("MissionSystem: Spawning Escort Target %1% for Player %2%") % firstObj.escortTargetId % player->getHandle());
        }
    }

    
    // Notify player
    std::string msg = "MISSION ASSIGNED: " + m_missions[missionId].title;
    // Assuming there's a way to send sys messages to player, we can log it for now
    INFO_LOG(format("Player %1% assigned mission %2%, moved to instance %3%") % player->getHandle() % missionId % newInstanceId);
}

void MissionSystem::AdvanceObjective(PlayerObject* player, ObjectiveCommand command, uint32 targetId)
{
    std::unique_lock<std::recursive_mutex> lock(m_missionMutex);
    if (!player) return;

    uint32 goId = player->getGoId();
    if (m_activeMissions.find(goId) == m_activeMissions.end()) return;

    ActiveMissionState& state = m_activeMissions[goId];
    MissionTemplate& templ = m_missions[state.missionId];

    if (state.currentObjectiveIndex >= templ.objectives.size()) return; // Mission complete

    MissionObjective& currentObj = templ.objectives[state.currentObjectiveIndex];

    bool objectiveMet = false;

    // Resolve targetNpcId to GoId if it was spawned by this mission
    uint32 expectedTargetGoId = currentObj.targetNpcId; // default to raw ID (e.g. static world NPCs)
    if (state.spawnedNpcs.find(currentObj.targetNpcId) != state.spawnedNpcs.end()) {
        expectedTargetGoId = state.spawnedNpcs[currentObj.targetNpcId];
    }

    // Default check
    if (currentObj.command == command && expectedTargetGoId == targetId)
        objectiveMet = true;

    // Item 37: Assassination (targetCharUID check)
    if (command == ObjectiveCommand::DEFEAT && currentObj.targetCharUID > 0) {
        PlayerObject* targetObj = sObjMgr.getGOPtr(targetId);
        if (targetObj && targetObj->getCharacterUID() == currentObj.targetCharUID) {
            objectiveMet = true;
        }
    }

    // Item 36: Courier (GIVE command with item check)
    if (command == ObjectiveCommand::GIVE && expectedTargetGoId == targetId) {
        if (!currentObj.requiredItem.empty() && player->getInventory()) {
            uint32 reqItemId = 0;
            try { reqItemId = std::stoul(currentObj.requiredItem); } catch(...) {}
            
            if (player->getInventory()->consumeItemByTemplate(reqItemId)) {
                objectiveMet = true;
            } else {
                objectiveMet = false; // Missing item!
            }
        }
    }

    if (command == ObjectiveCommand::REBIRTH) {
        if (player->getLevel() >= 50) {
            objectiveMet = true;
            player->PerformRebirth();
        } else {
            player->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}You must be Level 50 to Rebirth.{/c}"));
            objectiveMet = false;
        }
    }

    // Phase 7: LOOT objective
    if (command == ObjectiveCommand::LOOT && expectedTargetGoId == targetId) {
        if (!currentObj.requiredItem.empty() && player->getInventory()) {
            uint32 reqItemId = 0;
            try { reqItemId = std::stoul(currentObj.requiredItem, nullptr, 16); } catch(...) {}
            if (reqItemId > 0) {
                auto item = std::make_shared<Item>(rand(), reqItemId);
                player->getInventory()->addItemAuto(item);
                INFO_LOG(format("MissionSystem: Player %1% looted item %2% from target.") % player->getHandle() % currentObj.requiredItem);
                player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    (format("{c:00FF00}You looted a mission item.{/c}")).str()
                ));
            }
        }
        objectiveMet = true;
    }

    if (objectiveMet)
    {
        // Objective met!
        INFO_LOG(format("Player %1% completed objective: %2%") % player->getHandle() % currentObj.description);
        
        // Output dialog if it was a TALK action
        if (command == ObjectiveCommand::TALK && !currentObj.dialog.empty() && currentObj.dialog != "NONE")
        {
            // Simulate NPC saying the dialog
            sBotMgr.LogCombat((format("[Mission Dialog] NPC %1%: %2%") % targetId % currentObj.dialog).str());
        }

        if (currentObj.spawnAmbush) {
            // Item 40: Dynamic Spawns
            INFO_LOG(format("AMBUSH! Spawning enemies for player %1%") % player->getHandle());
            auto pos = player->getPosition();
            sBotMgr.SpawnBot(2, pos.x + 2.0f, pos.y, pos.z + 2.0f, 0);
        }

        uint32 jumpMissionId = 0;
        
        if (currentObj.nextMissionSuccessId > 0) {
            // Objective-level branching: completes immediately and jumps
            jumpMissionId = currentObj.nextMissionSuccessId;
            INFO_LOG(format("Objective completed with branch! Jumping to mission %1%") % jumpMissionId);
            m_activeMissions.erase(goId);
        }
        else {
            state.currentObjectiveIndex++;

            if (state.currentObjectiveIndex < templ.objectives.size())
            {
                state.objectiveStartTimeMs = getMSTime();
                MissionObjective& nextObj = templ.objectives[state.currentObjectiveIndex];
                if (nextObj.isEscort)
                {
                    auto pos = player->getPosition();
                    sBotMgr.SpawnBot(nextObj.escortTargetId, pos.x + 2.0f, pos.y, pos.z + 2.0f, player->getFaction());
                    INFO_LOG(format("MissionSystem: Spawning Escort Target %1% for Player %2%") % nextObj.escortTargetId % player->getHandle());
                }
            }
            else
            {
                INFO_LOG(format("Player %1% completed mission: %2%!") % player->getHandle() % templ.title);
                
                // Item 38: Mission Rewards
                if (templ.infoReward > 0)
                    sEconomySys.GiveInfo(player, templ.infoReward, "Mission Completion");
                
                if (templ.expReward > 0)
                    player->awardCombatExperience(templ.expReward);

                if (templ.rewardFactionRep > 0)
                    player->addFactionReputation(templ.rewardFactionRep);

                // Item 38: physical reward item
                if (templ.rewardItemTemplateId > 0 && player->getInventory())
                {
                    auto item = std::make_shared<Item>(rand(), templ.rewardItemTemplateId);
                    player->getInventory()->addItemAuto(item);
                    INFO_LOG(format("Rewarded Item %1% to %2%") % templ.rewardItemTemplateId % player->getHandle());
                }

                if (templ.nextMissionSuccessId > 0) {
                    jumpMissionId = templ.nextMissionSuccessId;
                    INFO_LOG(format("Mission completed with branch! Jumping to mission %1%") % jumpMissionId);
                }

                // clear active mission
                m_activeMissions.erase(goId);
            }
        }
        
        if (jumpMissionId > 0) {
            // We must unlock the mutex before assigning a new mission to avoid deadlock
            lock.unlock();
            AssignMission(player, jumpMissionId);
        }
    }
}

// Item 37: Bounty Hunter Contracts
void MissionSystem::PlaceBounty(uint32 targetGoId, uint32 infoAmount, const std::string& placedBy)
{
    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
    BountyContract contract;
    contract.targetGoId = targetGoId;
    contract.infoReward = infoAmount;
    contract.placedBy = placedBy;
    m_activeBounties.push_back(contract);
    
    INFO_LOG(format("MissionSystem: Bounty placed on %1% for %2% Info by %3%") % targetGoId % infoAmount % placedBy);
}

void MissionSystem::GenerateAssassinationMission(PlayerObject* hunter, uint32 targetGoId)
{
    if (!hunter) return;
    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);

    // Find the bounty
    auto it = std::find_if(m_activeBounties.begin(), m_activeBounties.end(), [targetGoId](const BountyContract& b) {
        return b.targetGoId == targetGoId;
    });

    if (it != m_activeBounties.end())
    {
        // Dynamically create a mission template
        MissionTemplate huntTempl;
        huntTempl.missionId = rand() % 100000 + 50000;
        huntTempl.title = "Assassination Contract";
        huntTempl.description = "Eliminate the target and collect the bounty.";
        huntTempl.infoReward = it->infoReward;
        huntTempl.expReward = 5000;
        huntTempl.rewardItemTemplateId = 0;
        huntTempl.rewardFactionRep = 0;
        huntTempl.requiredFactionRep = 0;

        MissionObjective obj;
        obj.command = ObjectiveCommand::DEFEAT;
        obj.targetNpcId = targetGoId; // Can be a player goId too
        obj.description = "Defeat the marked target.";
        obj.targetCharUID = targetGoId;
        obj.spawnAmbush = false;
        huntTempl.objectives.push_back(obj);

        m_missions[huntTempl.missionId] = huntTempl;
        m_activeBounties.erase(it);

        AssignMission(hunter, huntTempl.missionId);
        INFO_LOG(format("MissionSystem: Generated Assassination Mission %1% for %2%") % huntTempl.missionId % hunter->getHandle());
    }
}

// Item 38: Information Brokering
void MissionSystem::SellMissionIntel(PlayerObject* player, int factionId)
{
    if (!player) return;
    
    // Example: Faction 1 = Zion, Faction 2 = Machines, Faction 3 = Merovingian
    if (player->getFaction() == 1 && factionId == 3)
    {
        // Selling Zion intel to Mervs
        player->addFactionReputation(-50); // Lose Zion Rep
        INFO_LOG(format("MissionSystem: %1% sold Zion Intel to Merovingians! Reputation penalized.") % player->getHandle());
    }
}

void MissionSystem::ProcessBounties(PlayerObject* killer, PlayerObject* victim)
{
    if (!killer || !victim) return;
    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
    
    uint32 targetId = victim->getGoId();
    for (auto it = m_activeBounties.begin(); it != m_activeBounties.end(); ++it)
    {
        if (it->targetGoId == targetId)
        {
            uint32 reward = it->infoReward;
            sEconomySys.GiveInfo(killer, reward, "Bounty Claimed");
            killer->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:00FF00}Bounty Claimed! You earned %1% Info for defeating %2%.{/c}") % reward % victim->getHandle()).str()
            ));
            INFO_LOG(format("MissionSystem: Player %1% claimed bounty on %2% for %3% Info") % killer->getHandle() % victim->getHandle() % reward);
            
            m_activeBounties.erase(it);
            break;
        }
    }
}

void MissionSystem::Update(uint32 deltaMs)
{
    std::vector<std::pair<PlayerObject*, uint32>> deferredAssignments;
    {
        std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
        uint64 currentMs = getMSTime();

        for (auto it = m_activeMissions.begin(); it != m_activeMissions.end(); )
        {
            PlayerObject* player = sObjMgr.getGOPtr(it->first);
            if (!player)
            {
                ++it;
                continue;
            }

            ActiveMissionState& state = it->second;
            MissionTemplate& templ = m_missions[state.missionId];

            if (state.currentObjectiveIndex < templ.objectives.size())
            {
                MissionObjective& currentObj = templ.objectives[state.currentObjectiveIndex];
                
                // Item 108: Timed Events
                if (currentObj.isTimed && currentObj.timeLimitSeconds > 0)
                {
                    uint64 elapsedMs = currentMs - state.objectiveStartTimeMs;
                    if (elapsedMs > (uint64)currentObj.timeLimitSeconds * 1000)
                    {
                        player->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FF0000}Mission Failed: Time ran out.{/c}"));
                        INFO_LOG(format("MissionSystem: Player %1% failed mission %2% due to time limit.") % player->getHandle() % templ.title);
                        
                        uint32 failJumpId = currentObj.nextMissionFailId;
                        if (failJumpId == 0) {
                            failJumpId = templ.nextMissionFailId;
                        }

                        if (failJumpId > 0) {
                            deferredAssignments.push_back({player, failJumpId});
                        }
                        
                        it = m_activeMissions.erase(it);
                        continue;
                    }
                }
            }
            ++it;
        }
    }
    
    // Process branches outside the lock to prevent deadlock
    for (auto& assignment : deferredAssignments) {
        AssignMission(assignment.first, assignment.second);
    }
}

void MissionSystem::InitializeContactQuests()
{
    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
    m_contactQuests.clear();

    // 1. Morpheus (Zion)
    ContactQuest q1;
    q1.questId = 1001;
    q1.contact = CONTACT_MORPHEUS;
    q1.contactName = "Morpheus";
    q1.title = "The Awakening Protocol";
    q1.dialogGreeting = "I can only show you the door. You're the one that has to walk through it.";
    q1.dialogCompletion = "Welcome to the real world, operative. The construct bends to your will.";
    q1.objectives = {
        "Locate the Slums Uplink Substation",
        "Hack the Corrupted Signal Repeater",
        "Escape through the Morrell Station Hardline"
    };
    q1.requiredFaction = 1; // Zion
    q1.rewardInfo = 2500;
    q1.rewardExp = 5000;
    q1.rewardItemTemplateId = 10103; // Dark Shades
    m_contactQuests.push_back(q1);

    // 2. Ghost (Zion)
    ContactQuest q2;
    q2.questId = 1002;
    q2.contact = CONTACT_GHOST;
    q2.contactName = "Ghost";
    q2.title = "Weapons Cache Insertion";
    q2.dialogGreeting = "Target verified. I need someone on the ground who can handle a firearm.";
    q2.dialogCompletion = "Cache secured. The resistance has enough munitions for the next push.";
    q2.objectives = {
        "Infiltrate Downtown Armory",
        "Neutralize Machine Tactical Guards",
        "Secure Munitions Cache"
    };
    q2.requiredFaction = 1; // Zion
    q2.rewardInfo = 3000;
    q2.rewardExp = 6000;
    q2.rewardItemTemplateId = 10101; // Dual Berettas
    m_contactQuests.push_back(q2);

    // 3. Trinity (Zion)
    ContactQuest q3;
    q3.questId = 1003;
    q3.contact = CONTACT_TRINITY;
    q3.contactName = "Trinity";
    q3.title = "Tower Infiltration";
    q3.dialogGreeting = "The answer is out there, operative. It's looking for you, and it will find you if you want it to.";
    q3.dialogCompletion = "Transmission routed. You move faster than most. Good work.";
    q3.objectives = {
        "Scale Mara Central High-Rise",
        "Disable Rooftop Security Node",
        "Extract Trapped Zion Operative"
    };
    q3.requiredFaction = 1; // Zion
    q3.rewardInfo = 4000;
    q3.rewardExp = 8000;
    q3.rewardItemTemplateId = 10102; // Onyx Trenchcoat
    m_contactQuests.push_back(q3);

    // 4. Niobe (Zion)
    ContactQuest q4;
    q4.questId = 1004;
    q4.contact = CONTACT_NIOBE;
    q4.contactName = "Niobe";
    q4.title = "Logistics Intercept";
    q4.dialogGreeting = "Keep your eyes open and your engine hot. We're intercepting a Machine convoy.";
    q4.dialogCompletion = "Telemetry captured. Logos navigation has new route coordinates.";
    q4.objectives = {
        "Scout the Barrens Highway",
        "Ambush Autonomous Machine Courier",
        "Retrieve Substation Code Core"
    };
    q4.requiredFaction = 1; // Zion
    q4.rewardInfo = 2800;
    q4.rewardExp = 5500;
    q4.rewardItemTemplateId = 10104; // Slums Trenchcoat
    m_contactQuests.push_back(q4);

    // 5. The Merovingian (Exiles)
    ContactQuest q5;
    q5.questId = 1005;
    q5.contact = CONTACT_MEROVINGIAN;
    q5.contactName = "The Merovingian";
    q5.title = "A Key For A Price";
    q5.dialogGreeting = "Nom de dieu de putain de bordel de merde... Cause and effect. You want something, you pay the price.";
    q5.dialogCompletion = "Magnifique. Our agreement is fulfilled. Do not disappoint me in the future.";
    q5.objectives = {
        "Acquire Exiled Memory Shard from Club Hel",
        "Corrupt Machine Transmission Relay",
        "Deliver Cryptographic Cipher to Château"
    };
    q5.requiredFaction = 2; // Merovingian
    q5.rewardInfo = 5000;
    q5.rewardExp = 10000;
    q5.rewardItemTemplateId = 10106; // Mirrored Shades
    m_contactQuests.push_back(q5);

    INFO_LOG(format("MissionSystem: Initialized %1% Contact Quests (Morpheus, Ghost, Trinity, Niobe, Merovingian).")
             % m_contactQuests.size());
}

std::vector<ContactQuest> MissionSystem::GetQuestsForContact(ContactId contact) const
{
    std::vector<ContactQuest> result;
    for (const auto& q : m_contactQuests)
    {
        if (q.contact == contact) result.push_back(q);
    }
    return result;
}

const ContactQuest* MissionSystem::GetContactQuest(uint32 questId) const
{
    for (const auto& q : m_contactQuests)
    {
        if (q.questId == questId) return &q;
    }
    return nullptr;
}

bool MissionSystem::AcceptContactQuest(PlayerObject* player, uint32 questId)
{
    if (!player) return false;
    const ContactQuest* q = GetContactQuest(questId);
    if (!q) return false;

    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
    uint64 charId = player->getCharId();
    m_playerContactProgress[charId][questId] = 0;

    INFO_LOG(format("Player %1% accepted Contact Quest '%2%' from %3%")
             % player->getHandle() % q->title % q->contactName);
    sBotMgr.LogCombat((format("[%1%] %2%: %3%") % q->contactName % q->title % q->dialogGreeting).str());
    return true;
}

bool MissionSystem::ProgressContactQuest(PlayerObject* player, uint32 questId, uint32 stepIndex)
{
    if (!player) return false;
    const ContactQuest* q = GetContactQuest(questId);
    if (!q) return false;

    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
    uint64 charId = player->getCharId();
    m_playerContactProgress[charId][questId] = stepIndex;

    if (stepIndex < q->objectives.size())
    {
        sBotMgr.LogCombat((format("Objective Updated: %1%") % q->objectives[stepIndex]).str());
    }

    return true;
}

bool MissionSystem::CompleteContactQuest(PlayerObject* player, uint32 questId)
{
    if (!player) return false;
    const ContactQuest* q = GetContactQuest(questId);
    if (!q) return false;

    std::lock_guard<std::recursive_mutex> lock(m_missionMutex);
    uint64 charId = player->getCharId();
    m_playerContactProgress[charId][questId] = static_cast<uint32>(q->objectives.size());

    // Distribute rewards
    sEconomySys.GiveInfo(player, q->rewardInfo, "Contact Quest Reward: " + q->title);
    player->addExp(q->rewardExp);
    if (q->rewardItemTemplateId > 0)
    {
        player->giveItem(q->rewardItemTemplateId);
    }

    INFO_LOG(format("Player %1% completed Contact Quest '%2%' from %3%")
             % player->getHandle() % q->title % q->contactName);
    sBotMgr.LogCombat((format("[%1%] %2%: %3%") % q->contactName % q->title % q->dialogCompletion).str());
    return true;
}

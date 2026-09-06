#include "MissionContext.h"
#include "ScriptEngine.h"
#include "PlayerObject.h"
#include "Log.h"
#include "BotManager.h"
#include "BotClient.h"
#include "BehaviorTree.h"
#include "Timer.h"
#include "GameClient.h"

// --- Lua Bindings ---

static int lua_MissionSetTimer(lua_State* L)
{
	MissionContext* ctx = (MissionContext*)lua_touserdata(L, lua_upvalueindex(1));
	if (!ctx) return 0;

	uint32 id = (uint32)luaL_checkinteger(L, 1);
	uint32 delayMs = (uint32)luaL_checkinteger(L, 2);
	std::string callback = luaL_checkstring(L, 3);

	ctx->SetTimer(id, delayMs, callback);
	return 0;
}

static int lua_MissionCompleteMission(lua_State* L)
{
	MissionContext* ctx = (MissionContext*)lua_touserdata(L, lua_upvalueindex(1));
	if (!ctx) return 0;

	uint32 xp = (uint32)luaL_optinteger(L, 1, 0);
	uint32 info = (uint32)luaL_optinteger(L, 2, 0);

	ctx->CompleteMission(xp, info);
	return 0;
}

// Lua C closure for Player:SetBotEscortTarget(botGoId, targetGoId)
static int lua_PlayerSetBotEscortTarget(lua_State* L)
{
	PlayerObject* player = (PlayerObject*)lua_touserdata(L, lua_upvalueindex(1));
	if (!player) return 0;

	uint32 botGoId = (uint32)luaL_checkinteger(L, 1);
	uint32 targetGoId = (uint32)luaL_checkinteger(L, 2);

	std::shared_ptr<BotClient> bot = BotManager::getSingletonPtr()->GetBotByGOID(botGoId);
	if (bot) {
		auto root = std::make_shared<SelectorNode>();
		auto sequence = std::make_shared<SequenceNode>();
		sequence->AddChild(std::make_shared<ActionFindTarget>());
		sequence->AddChild(std::make_shared<ActionEngageTarget>());
		root->AddChild(sequence);
		root->AddChild(std::make_shared<ActionEscort>(targetGoId, 5.0f));
		bot->SetBehaviorTree(root);
		INFO_LOG(format("Lua Mission: SetBotEscortTarget applied to %1% following %2%") % botGoId % targetGoId);
	}

	return 0;
}

// Lua C closure for Player:GiveItem(itemId)
static int lua_PlayerGiveItem(lua_State* L)
{
	PlayerObject* player = (PlayerObject*)lua_touserdata(L, lua_upvalueindex(1));
	if (!player) return 0;

	uint32 itemId = (uint32)luaL_checkinteger(L, 1);
	
	// Mock implementation for GiveItem
	INFO_LOG(format("Lua Mission: Giving item %1% to player %2%") % itemId % player->getFirstName());

	return 0;
}

// Lua C closure for Player:SpawnMob(mobId, x, y, z)
static int lua_PlayerSpawnMob(lua_State* L)
{
	PlayerObject* player = (PlayerObject*)lua_touserdata(L, lua_upvalueindex(1));
	if (!player) return 0;

	uint32 mobId = (uint32)luaL_checkinteger(L, 1);
	float x = (float)luaL_checknumber(L, 2);
	float y = (float)luaL_checknumber(L, 3);
	float z = (float)luaL_checknumber(L, 4);

	INFO_LOG(format("Lua Mission: Spawning mob %1% at (%2%, %3%, %4%) for player %5%") 
		% mobId % x % y % z % player->getFirstName());

	return 0;
}

// Lua C closure for Mission:CompleteObjective(objId)
static int lua_MissionCompleteObjective(lua_State* L)
{
	MissionContext* ctx = (MissionContext*)lua_touserdata(L, lua_upvalueindex(1));
	if (!ctx) return 0;

	uint32 objId = (uint32)luaL_checkinteger(L, 1);
	ctx->CompleteObjective(objId);

	return 0;
}

// Lua C closure for Player:SetWaypoint(x, y, z, name)
static int lua_PlayerSetWaypoint(lua_State* L)
{
	PlayerObject* player = (PlayerObject*)lua_touserdata(L, lua_upvalueindex(1));
	if (!player) return 0;

	float x = (float)luaL_checknumber(L, 1);
	float y = (float)luaL_checknumber(L, 2);
	float z = (float)luaL_checknumber(L, 3);
	std::string name = luaL_checkstring(L, 4);

	player->SendWaypoint(x, y, z, name);

	return 0;
}


// --- MissionContext ---

MissionContext::MissionContext(PlayerObject* player, const std::string& scriptName)
	: m_player(player), m_scriptName(scriptName), m_completed(false), m_failed(false)
{
}

MissionContext::~MissionContext()
{
}

bool MissionContext::Start()
{
	INFO_LOG(format("Starting Mission: %1% for player %2%") % m_scriptName % m_player->getFirstName());

	lua_State* L = sScriptEngine.GetState();
	if (!L) return false;

	BindPlayerToLua();

	// Load and run the script
	std::string scriptPath = "Data/Missions/" + m_scriptName + ".lua";
	
	// Actually execute the script so it registers its global variables
	if (!sScriptEngine.ExecuteFile(scriptPath))
	{
		return false;
	}

	// Call OnStart() if it exists in the script
	lua_getglobal(L, "OnStart");
	if (lua_isfunction(L, -1))
	{
		if (lua_pcall(L, 0, 0, 0) != LUA_OK)
		{
			CRITICAL_LOG(format("Lua error in OnStart: %1%") % lua_tostring(L, -1));
			lua_pop(L, 1); // pop error message
			return false;
		}
	}
	else
	{
		lua_pop(L, 1); // pop non-function
	}

	return true;
}

void MissionContext::Update()
{
	if (m_completed || m_failed) return;

	lua_State* L = sScriptEngine.GetState();
	if (!L) return;

	// Call OnUpdate() if it exists in the script
	lua_getglobal(L, "OnUpdate");
	if (lua_isfunction(L, -1))
	{
		if (lua_pcall(L, 0, 0, 0) != LUA_OK)
		{
			CRITICAL_LOG(format("Lua error in OnUpdate: %1%") % lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	else
	{
		lua_pop(L, 1);
	}

	// Item 108: Timed Events
	uint32 now = getMSTime();
	for (auto it = m_timers.begin(); it != m_timers.end();)
	{
		if (now >= it->endTimeMs)
		{
			lua_getglobal(L, it->callbackName.c_str());
			if (lua_isfunction(L, -1))
			{
				lua_pushinteger(L, it->id);
				if (lua_pcall(L, 1, 0, 0) != LUA_OK)
				{
					CRITICAL_LOG(format("Lua error in timer callback %1%: %2%") % it->callbackName % lua_tostring(L, -1));
					lua_pop(L, 1);
				}
			}
			else
			{
				lua_pop(L, 1);
			}
			it = m_timers.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void MissionContext::SetTimer(uint32 id, uint32 delayMs, const std::string& callbackName)
{
	m_timers.push_back({id, getMSTime() + delayMs, callbackName});
}

bool MissionContext::IsCompleted() const
{
	return m_completed;
}

void MissionContext::AddObjective(uint32 id, const std::string& description)
{
	ContextMissionObjective obj = { id, description, false };
	m_objectives.push_back(obj);
	INFO_LOG(format("Mission %1% Objective Added: [%2%] %3%") % m_scriptName % id % description);
}

void MissionContext::CompleteObjective(uint32 id)
{
	for (auto& obj : m_objectives)
	{
		if (obj.id == id && !obj.completed)
		{
			obj.completed = true;
			INFO_LOG(format("Mission %1% Objective Completed: [%2%] %3%") % m_scriptName % id % obj.description);
			break;
		}
	}
}

void MissionContext::CleanupInstancing()
{
	if (m_player) {
		GameClient& client = m_player->getClient();
		if (client.m_instanceId != 0) {
			client.m_instanceId = 0;
			m_player->setPosition(m_player->getSavedPos());
			INFO_LOG(format("Mission %1% teardown: Player %2% returned to public instance") % m_scriptName % m_player->getFirstName());
		}
	}
}

void MissionContext::FailMission(const std::string& reason)
{
	m_failed = true;
	INFO_LOG(format("Mission %1% Failed: %2%") % m_scriptName % reason);
	CleanupInstancing();
}

void MissionContext::CompleteMission(uint32 xp, uint32 info)
{
	m_completed = true;
	INFO_LOG(format("Mission %1% Completed Successfully! Reward: %2% XP, %3% Info") % m_scriptName % xp % info);

	if (m_player) {
		if (xp > 0) m_player->awardCombatExperience(xp);
		if (info > 0) m_player->addInfo(info);
		
		m_player->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
			(format("{c:00FF00}Mission Complete! You earned %1% XP and %2% Info.{/c}") % xp % info).str()
		));
	}
	CleanupInstancing();
}

void MissionContext::BindPlayerToLua()
{
	lua_State* L = sScriptEngine.GetState();

	// Create "Player" table
	lua_newtable(L);

	// Player.GiveItem
	lua_pushlightuserdata(L, m_player);
	lua_pushcclosure(L, lua_PlayerGiveItem, 1);
	lua_setfield(L, -2, "GiveItem");

	// Player.SpawnMob
	lua_pushlightuserdata(L, m_player);
	lua_pushcclosure(L, lua_PlayerSpawnMob, 1);
	lua_setfield(L, -2, "SpawnMob");

	lua_pushlightuserdata(L, m_player);
	lua_pushcclosure(L, lua_PlayerSetWaypoint, 1);
	lua_setfield(L, -2, "SetWaypoint");

	lua_pushlightuserdata(L, m_player);
	lua_pushcclosure(L, lua_PlayerSetBotEscortTarget, 1);
	lua_setfield(L, -2, "SetBotEscortTarget");

	lua_setglobal(L, "Player");

	// Create "Mission" table
	lua_newtable(L);

	// Mission.CompleteObjective
	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, lua_MissionCompleteObjective, 1);
	lua_setfield(L, -2, "CompleteObjective");

	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, lua_MissionSetTimer, 1);
	lua_setfield(L, -2, "SetTimer");

	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, lua_MissionCompleteMission, 1);
	lua_setfield(L, -2, "CompleteMission");

	lua_setglobal(L, "Mission");
}

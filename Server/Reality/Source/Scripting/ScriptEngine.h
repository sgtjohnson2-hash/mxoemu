#pragma once

#include "../Common.h"
#include "../Singleton.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

class ScriptEngine : public Singleton<ScriptEngine>
{
public:
	ScriptEngine();
	~ScriptEngine();

	void Init();
	void Shutdown();

	lua_State* GetState() { return m_luaState; }

	// Executes a lua script file
	bool ExecuteFile(const std::string& filename);

	// Executes a raw lua string
	bool ExecuteString(const std::string& script);

private:
	void SetupSandbox();

	lua_State* m_luaState;
};

#define sScriptEngine ScriptEngine::getSingleton()

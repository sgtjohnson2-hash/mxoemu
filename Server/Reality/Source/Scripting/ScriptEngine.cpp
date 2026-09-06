#include "ScriptEngine.h"
#include "../Log.h"

createFileSingleton(ScriptEngine);

ScriptEngine::ScriptEngine() : m_luaState(nullptr)
{
}

ScriptEngine::~ScriptEngine()
{
	Shutdown();
}

void ScriptEngine::Init()
{
	m_luaState = luaL_newstate();
	if (!m_luaState)
	{
		CRITICAL_LOG("ScriptEngine: Failed to initialize Lua state.");
		return;
	}

	// Load standard libraries
	luaL_openlibs(m_luaState);

	SetupSandbox();

	INFO_LOG("ScriptEngine Initialized.");
}

void ScriptEngine::Shutdown()
{
	if (m_luaState)
	{
		lua_close(m_luaState);
		m_luaState = nullptr;
		INFO_LOG("ScriptEngine Shutdown.");
	}
}

void ScriptEngine::SetupSandbox()
{
	// Disable dangerous functions like os.execute, io.popen, etc.
	// In Lua 5.4, we can set them to nil in the global table
	const char* dangerousFuncs[] = {
		"os.execute", "os.remove", "os.rename", "os.exit", "os.getenv", "os.setlocale",
		"io.popen", "io.lines", "io.open", "io.input", "io.output",
		"dofile", "loadfile", "package.loadlib",
		nullptr
	};

	for (int i = 0; dangerousFuncs[i] != nullptr; ++i)
	{
		std::string funcPath = dangerousFuncs[i];
		size_t dotPos = funcPath.find('.');
		if (dotPos != std::string::npos)
		{
			std::string lib = funcPath.substr(0, dotPos);
			std::string func = funcPath.substr(dotPos + 1);
			lua_getglobal(m_luaState, lib.c_str());
			if (lua_istable(m_luaState, -1))
			{
				lua_pushnil(m_luaState);
				lua_setfield(m_luaState, -2, func.c_str());
			}
			lua_pop(m_luaState, 1);
		}
		else
		{
			lua_pushnil(m_luaState);
			lua_setglobal(m_luaState, funcPath.c_str());
		}
	}
}

bool ScriptEngine::ExecuteFile(const std::string& filename)
{
	if (!m_luaState) return false;

	if (luaL_dofile(m_luaState, filename.c_str()) != LUA_OK)
	{
		CRITICAL_LOG(format("ScriptEngine: Error executing file %1%: %2%") % filename % lua_tostring(m_luaState, -1));
		lua_pop(m_luaState, 1);
		return false;
	}
	return true;
}

bool ScriptEngine::ExecuteString(const std::string& script)
{
	if (!m_luaState) return false;

	if (luaL_dostring(m_luaState, script.c_str()) != LUA_OK)
	{
		CRITICAL_LOG(format("ScriptEngine: Error executing script: %1%") % lua_tostring(m_luaState, -1));
		lua_pop(m_luaState, 1);
		return false;
	}
	return true;
}

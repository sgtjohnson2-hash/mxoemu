// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
// Copyright (C) 2006-2010 Rajko Stojadinovic
// http://mxoemu.info
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ---------------------------------------------------------------------------
//
// ***************************************************************************

#include "Common.h"
#include "Config.h"
#include <map>
#include "DotConfPP/dotconfpp.h"

createFileSingleton(Config);

Config::Config() : mConf(0)
{
}


Config::~Config()
{
    if (mConf)
        delete mConf;
}


bool Config::SetSource(const char *file, bool ignorecase)
{
    mConf = new DOTCONFDocument(ignorecase ?
        DOTCONFDocument::CASEINSENSETIVE :
    DOTCONFDocument::CASESENSETIVE);

    if (mConf->setContent(file) == -1)
    {
        delete mConf;
        mConf = 0;
        return false;
    }

    return true;
}


// Runtime overrides (test suites, console). They win over the file.
static std::map<std::string, std::string> s_configOverrides;

void Config::SetOverride(const char* name, const char* value)
{
    if (value) s_configOverrides[name] = value;
    else s_configOverrides.erase(name);
}

const char* Config::RawValue(const char* name)
{
    auto it = s_configOverrides.find(name);
    if (it != s_configOverrides.end())
        return it->second.c_str();
    if(!mConf)
        return nullptr;
    DOTCONFDocumentNode *node = (DOTCONFDocumentNode *)mConf->findNode(name);
    if(!node || !node->getValue())
        return nullptr;
    return node->getValue();
}

bool Config::GetString(const char* name, std::string *value)
{
    const char* raw = RawValue(name);
    if(!raw)
        return false;

    *value = raw;

    return true;
}


std::string Config::GetStringDefault(const char* name, const char* def)
{
    const char* raw = RawValue(name);
    return raw ? std::string(raw) : std::string(def);
}


bool Config::GetBool(const char* name, bool *value)
{
    const char* raw = RawValue(name);
    if(!raw)
        return false;

    const char* str = raw;
    if(strcmp(str, "true") == 0 || strcmp(str, "TRUE") == 0 ||
        strcmp(str, "yes") == 0 || strcmp(str, "YES") == 0 ||
        strcmp(str, "1") == 0)
    {
        *value = true;
    }
    else
        *value = false;

    return true;
}


bool Config::GetBoolDefault(const char* name, const bool def)
{
    bool val;
    return GetBool(name, &val) ? val : def;
}


bool Config::GetInt(const char* name, int *value)
{
    const char* raw = RawValue(name);
    if(!raw)
        return false;

    *value = (int)strtoul(raw, NULL, 0);

    return true;
}


bool Config::GetFloat(const char* name, float *value)
{
    const char* raw = RawValue(name);
    if(!raw)
        return false;

    *value = (float)atof(raw);

    return true;
}


int Config::GetIntDefault(const char* name, const int def)
{
    int val;
    return GetInt(name, &val) ? val : def;
}


float Config::GetFloatDefault(const char* name, const float def)
{
    float val;
    return (GetFloat(name, &val) ? val : def);
}

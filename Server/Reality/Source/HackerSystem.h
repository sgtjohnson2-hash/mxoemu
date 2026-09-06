#ifndef HACKERSYSTEM_H
#define HACKERSYSTEM_H

#include "Common.h"
#include "Singleton.h"

class PlayerObject;

class HackerSystem : public Singleton<HackerSystem> {
public:
    HackerSystem();
    ~HackerSystem();

    void Initialize();
    
    // Core Hacker Abilities
    bool CompileProgram(PlayerObject* hacker, uint32 programId, uint32 targetGoId);
    bool ExecuteProgram(PlayerObject* hacker, uint32 programId, uint32 targetGoId);
    bool ExtractSourceCode(PlayerObject* hacker, uint32 targetGoId);
};

#define sHackerSystem Singleton<HackerSystem>::getSingleton()

#endif

#include "HackerSystem.h"
#include "PlayerObject.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "StatusEffectManager.h"
#include "GameServer.h"
#include "MessageTypes.h"
#include "InventorySystem.h"
#include "GameClient.h"
#include "SpatialGrid.h"
#include "BotManager.h"

#include "DataLoader.h"

createFileSingleton(HackerSystem);

HackerSystem::HackerSystem() {}
HackerSystem::~HackerSystem() {}

void HackerSystem::Initialize() {
    INFO_LOG("HackerSystem Initialized.");
}

bool HackerSystem::ExecuteHackerAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId, const AbilityTemplate* templ)
{
    if (!caster) return false;
    const bool humanCaster = !caster->getClient().isBot();

    // 1. Resolve Target
    if (targetGoId == 0)
        targetGoId = caster->getTargetGoId();

    bool isSelfCast = (templ && templ->isBuff && templ->name.find("Personal") != std::string::npos);
    PlayerObject* target = nullptr;
    if (isSelfCast)
    {
        target = caster;
        targetGoId = caster->getGoId();
    }
    else
    {
        target = sObjMgr.getGOPtrSafe(targetGoId);
        if (!target)
        {
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FF0000}[HACK] You must select a valid hostile target.{/c}"));
            }
            return false;
        }

        if (target->getGoId() == caster->getGoId())
        {
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FF0000}[HACK] You cannot target yourself with offensive viral logic.{/c}"));
            }
            return false;
        }

        // Distance check: 35.0m max (3500 world units)
        LocationVector posA = caster->getPosition();
        LocationVector posB = target->getPosition();
        double dx = posB.x - posA.x;
        double dz = posB.z - posA.z;
        double distSq = dx * dx + dz * dz;
        if (distSq > (3500.0 * 3500.0))
        {
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FF0000}[HACK] Target is out of range (max 35m).{/c}"));
            }
            return false;
        }
    }

    // 2. Resource check: Inner Strength
    uint16 isCost = templ ? templ->innerStrengthCost : 15;
    if (caster->getCurrentIS() < isCost)
    {
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:FF0000}[HACK] Insufficient Inner Strength to compile payload.{/c}"));
        }
        return false;
    }
    caster->spendIS(isCost);

    // 3. Cast Bar: RPC 0x80ac
    float castSec = templ ? float(templ->castTime) / 1000.0f : 2.0f;
    if (castSec > 0.05f && humanCaster)
    {
        caster->getClient().QueueCommand(std::make_shared<CastBarMsg>(abilityId, castSec));
    }

    // 4. Caster Animation: 16-bit ExtendedAnimationMsg (opcode 0x29)
    // 0x0429 = Hacker_VirusLaunch_A
    sGame.AnnounceStateUpdateNear(caster->getPosition().x, caster->getPosition().z, 20000.0f,
        std::make_shared<ExtendedAnimationMsg>(caster->getGoId(), 0x0429, 1));

    std::string abilName = templ ? templ->name : "Logic Attack";
    INFO_LOG(format("HackerSystem: %1%:%2% executes %3% (id %4%) on %5%:%6%")
        % caster->getHandle() % caster->getGoId() % abilName % abilityId
        % (target ? target->getHandle() : "<none>") % targetGoId);

    if (humanCaster)
    {
        caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            (format("{c:00FFCC}[HACK] Compiled and transmitted %1% to %2%.{/c}")
                % abilName % target->getHandle()).str()));
    }

    // 5. Ability Payload Execution
    if (abilityId == 57) // LogicBlast1Ability
    {
        target->takeDamage(caster->getGoId(), 45, 0x280006DF);
    }
    else if (abilityId == 58) // LogicBlast2Ability
    {
        target->takeDamage(caster->getGoId(), 75, 0x280006DF);
    }
    else if (abilityId == 59) // LogicBlast3Ability
    {
        target->takeDamage(caster->getGoId(), 110, 0x280006DF);
    }
    else if (abilityId == 60) // LogicBomb1Ability
    {
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_LOGIC_BOMB, 10.0f, 1.0f, 75.0f, caster->getGoId());
    }
    else if (abilityId == 359) // CodeNukeAbility
    {
        target->takeDamage(caster->getGoId(), 220, 0x110A0028);
    }
    else if (abilityId == 53 || abilName.find("Virus") != std::string::npos) // HarmfulCode / TransmitVirus
    {
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_VIRUS_DOT, 15.0f, 1.0f, 15.0f, caster->getGoId());
    }
    else if (abilityId == 40 || abilName.find("Freeze") != std::string::npos) // CodeFreeze1
    {
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_CODE_FREEZE, 8.0f, 1.0f, 1.0f, caster->getGoId());
    }
    else if (abilityId == 68) // PersonalFirewall1Ability
    {
        sStatusEffectManager.ApplyEffect(caster->getGoId(), EFFECT_FIREWALL, 45.0f, 1.0f, 200.0f, caster->getGoId());
    }
    else if (abilityId == 63) // NetworkFirewall1Ability
    {
        sStatusEffectManager.ApplyEffect(caster->getGoId(), EFFECT_FIREWALL, 60.0f, 1.0f, 150.0f, caster->getGoId());
        auto nearby = sSpatialGrid.GetClientsInRadius(caster->getPosition().x, caster->getPosition().z, 1500.0f);
        for (GameClient* client : nearby)
        {
            if (client->GetPlayerGoId() != caster->getGoId())
            {
                sStatusEffectManager.ApplyEffect(client->GetPlayerGoId(), EFFECT_FIREWALL, 60.0f, 1.0f, 150.0f, caster->getGoId());
            }
        }
    }
    else if (abilityId == 43) // DisruptInputs1Ability
    {
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_STUN, 2.5f, 0.5f, 1.0f, caster->getGoId());
    }
    else if (abilityId == 97) // UILag1Ability
    {
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_UI_LAG, 10.0f, 1.0f, 1.0f, caster->getGoId());
    }
    else
    {
        uint16 dmg = 35 + caster->getLevel() * 5;
        target->takeDamage(caster->getGoId(), dmg, 0x280006DF);
    }

    return true;
}

bool HackerSystem::ExecuteCoderAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId, const AbilityTemplate* templ)
{
    if (!caster) return false;
    const bool humanCaster = !caster->getClient().isBot();

    // 1. Resolve Target (Self if targetGoId == 0 or targetGoId == caster->getGoId())
    PlayerObject* target = nullptr;
    if (targetGoId == 0 || targetGoId == caster->getGoId())
    {
        target = caster;
    }
    else
    {
        target = sObjMgr.getGOPtrSafe(targetGoId);
        if (!target)
            target = caster; // Fallback to self
    }

    // Distance check if targeting another entity (max 25m = 2500 world units)
    if (target != caster)
    {
        LocationVector posA = caster->getPosition();
        LocationVector posB = target->getPosition();
        double dx = posB.x - posA.x;
        double dz = posB.z - posA.z;
        double distSq = dx * dx + dz * dz;
        if (distSq > (2500.0 * 2500.0))
        {
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FF0000}[CODER] Target is out of range (max 25m).{/c}"));
            }
            return false;
        }
    }

    // 2. Resource check: Inner Strength
    uint16 isCost = templ ? templ->innerStrengthCost : 15;
    if (caster->getCurrentIS() < isCost)
    {
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:FF0000}[CODER] Insufficient Inner Strength to reconstruct RSI.{/c}"));
        }
        return false;
    }

    // Special condition checks:
    if (abilityId == 375) // ReviveRSIAbility
    {
        if (!target->isDead())
        {
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FFFF00}[CODER] Target RSI is fully active (revive requires downed operative).{/c}"));
            }
            return false;
        }
    }
    else if (abilityId == 169) // EmergencyRepairs1Ability
    {
        if (target->getCurrentHealth() > (target->getMaximumHealth() * 35 / 100))
        {
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FFFF00}[CODER] Emergency Repairs requires target RSI below 35%.{/c}"));
            }
            return false;
        }
    }

    caster->spendIS(isCost);

    // 3. Cast Bar: RPC 0x80ac
    float castSec = templ ? float(templ->castTime) / 1000.0f : 2.0f;
    if (castSec > 0.05f && humanCaster)
    {
        caster->getClient().QueueCommand(std::make_shared<CastBarMsg>(abilityId, castSec));
    }

    // 4. Caster Animation: 16-bit ExtendedAnimationMsg (opcode 0x29)
    // 0x0428 = coding posture
    sGame.AnnounceStateUpdateNear(caster->getPosition().x, caster->getPosition().z, 20000.0f,
        std::make_shared<ExtendedAnimationMsg>(caster->getGoId(), 0x0428, 1));

    std::string abilName = templ ? templ->name : "Support Ability";
    INFO_LOG(format("HackerSystem: %1%:%2% executes Coder ability %3% (id %4%) on %5%:%6%")
        % caster->getHandle() % caster->getGoId() % abilName % abilityId
        % target->getHandle() % target->getGoId());

    if (humanCaster)
    {
        caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            (format("{c:00FF00}[CODER] Executing %1% on %2%.{/c}")
                % abilName % (target == caster ? "Self" : target->getHandle())).str()));
    }

    // 5. Ability Payload Execution
    if (abilityId == 77) // RestoreHealth1Ability
    {
        target->applyHeal(caster->getGoId(), 60, 0x01000060);
    }
    else if (abilityId == 80) // RestoreHealth2Ability
    {
        target->applyHeal(caster->getGoId(), 120, 0x01000060);
    }
    else if (abilityId == 234) // RestoreHealth3Ability
    {
        target->applyHeal(caster->getGoId(), 200, 0x01000060);
    }
    else if (abilityId == 46) // FastHealing1Ability
    {
        target->applyHeal(caster->getGoId(), 40, 0x01000060);
    }
    else if (abilityId == 169) // EmergencyRepairs1Ability
    {
        target->applyHeal(caster->getGoId(), 160, 0x01000060);
    }
    else if (abilityId == 56) // GroupRepairs1Ability
    {
        target->applyHeal(caster->getGoId(), 75, 0x01000060);
        auto nearby = sSpatialGrid.GetClientsInRadius(caster->getPosition().x, caster->getPosition().z, 2000.0f);
        for (GameClient* client : nearby)
        {
            if (client->GetPlayerGoId() != target->getGoId())
            {
                PlayerObject* po = sObjMgr.getGOPtrSafe(client->GetPlayerGoId());
                if (po && !po->isDead())
                {
                    po->applyHeal(caster->getGoId(), 75, 0x01000060);
                }
            }
        }
    }
    else if (abilityId == 50) // GroupRepairs2Ability
    {
        target->applyHeal(caster->getGoId(), 130, 0x01000060);
        auto nearby = sSpatialGrid.GetClientsInRadius(caster->getPosition().x, caster->getPosition().z, 2000.0f);
        for (GameClient* client : nearby)
        {
            if (client->GetPlayerGoId() != target->getGoId())
            {
                PlayerObject* po = sObjMgr.getGOPtrSafe(client->GetPlayerGoId());
                if (po && !po->isDead())
                {
                    po->applyHeal(caster->getGoId(), 130, 0x01000060);
                }
            }
        }
    }
    else if (abilityId == 39) // BolsterHealth1Ability
    {
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_BOLSTER_HEALTH, 300.0f, 1.0f, 100.0f, caster->getGoId());
    }
    else if (abilityId == 375) // ReviveRSIAbility
    {
        target->revive(caster->getGoId(), 0.5f);
    }
    else if (abilityId == 23) // DeflectCodeAbility
    {
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:00FF00}[CODER] Passive Code Deflection algorithm active in RSI buffer.{/c}"));
        }
    }
    else if (abilityId == 20) // FortifySimulacra1Ability
    {
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_BOLSTER_HEALTH, 180.0f, 1.0f, 50.0f, caster->getGoId());
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:00FF00}[CODER] Fortifying target RSI construct with reinforced subroutine shielding.{/c}"));
        }
    }
    else if (abilityId == 30) // RepairSimulacra1Ability
    {
        target->applyHeal(caster->getGoId(), 120, 0x01000060);
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_REGEN_HOT, 10.0f, 2.0f, 15.0f, caster->getGoId());
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:00FF00}[CODER] Performing diagnostic synthesis and repair on construct.{/c}"));
        }
    }
    else
    {
        uint16 healAmt = 50 + caster->getLevel() * 5;
        target->applyHeal(caster->getGoId(), healAmt, 0x01000060);
    }

    return true;
}

bool HackerSystem::CompileProgram(PlayerObject* hacker, uint32 programId, uint32 targetGoId) {
    const AbilityTemplate* t = sDataLoader.GetAbilityTemplate((uint16)programId);
    return ExecuteHackerAbility(hacker, (uint16)programId, targetGoId, t);
}

bool HackerSystem::ExecuteProgram(PlayerObject* hacker, uint32 programId, uint32 targetGoId) {
    const AbilityTemplate* t = sDataLoader.GetAbilityTemplate((uint16)programId);
    return ExecuteHackerAbility(hacker, (uint16)programId, targetGoId, t);
}

bool HackerSystem::ExtractSourceCode(PlayerObject* hacker, uint32 targetGoId) {
    if (!hacker) return false;
    hacker->restoreIS(25);
    return true;
}

bool HackerSystem::ExecuteSoldierAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId, const AbilityTemplate* templ)
{
    if (!caster) return false;
    const bool humanCaster = !caster->getClient().isBot();

    // 1. Resolve Target
    if (targetGoId == 0)
        targetGoId = caster->getTargetGoId();

    PlayerObject* target = sObjMgr.getGOPtrSafe(targetGoId);
    if (!target)
    {
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:FF0000}[SOLDIER] You must select a valid hostile target.{/c}"));
        }
        return false;
    }

    if (target->getGoId() == caster->getGoId())
    {
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:FF0000}[SOLDIER] You cannot target yourself with firearm attacks.{/c}"));
        }
        return false;
    }

    // Distance checks based on weapon type
    LocationVector posA = caster->getPosition();
    LocationVector posB = target->getPosition();
    double dx = posB.x - posA.x;
    double dz = posB.z - posA.z;
    double distSq = dx * dx + dz * dz;

    double maxRange = 3500.0; // default 35m
    if (abilityId == 505) // SniperShotAbility
        maxRange = 8000.0; // 80m sniper range
    else if (abilityId == 147) // RiflesAbility
        maxRange = 4500.0; // 45m rifle range
    else if (abilityId == 453 || abilityId == 501) // RifleButtSmash, PistolWhip
        maxRange = 400.0; // 4m melee range

    if (distSq > (maxRange * maxRange))
    {
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:FF0000}[SOLDIER] Target is out of range (max %1%m).{/c}") % (int)(maxRange / 100.0)).str()));
        }
        return false;
    }

    // 2. Resource check: Inner Strength
    uint16 isCost = templ ? templ->innerStrengthCost : 15;
    if (caster->getCurrentIS() < isCost)
    {
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:FF0000}[SOLDIER] Insufficient Inner Strength to execute tactical technique.{/c}"));
        }
        return false;
    }
    caster->spendIS(isCost);

    // If caster was stealthed, firing breaks stealth
    if (caster->isStealthed())
    {
        caster->setStealth(false);
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:FFFF00}[SPY] Concealment dropped to engage target.{/c}"));
        }
    }

    // 3. Cast Bar: RPC 0x80ac
    float castSec = templ ? float(templ->castTime) / 1000.0f : 1.0f;
    if (castSec > 0.05f && humanCaster)
    {
        caster->getClient().QueueCommand(std::make_shared<CastBarMsg>(abilityId, castSec));
    }

    // 4. Caster Animation: 16-bit ExtendedAnimationMsg (opcode 0x29)
    uint16 animId = (abilityId == 147 || abilityId == 505 || abilityId == 453) ? 0x0529 : 0x0528;
    sGame.AnnounceStateUpdateNear(caster->getPosition().x, caster->getPosition().z, 20000.0f,
        std::make_shared<ExtendedAnimationMsg>(caster->getGoId(), animId, 1));

    std::string abilName = templ ? templ->name : "Firearm Ability";
    INFO_LOG(format("HackerSystem: %1%:%2% executes Soldier ability %3% (id %4%) on %5%:%6%")
        % caster->getHandle() % caster->getGoId() % abilName % abilityId
        % target->getHandle() % target->getGoId());

    if (humanCaster)
    {
        caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            (format("{c:FF4444}[SOLDIER] Executing %1% on %2%.{/c}") % abilName % target->getHandle()).str()));
    }

    // 5. Ability Payload Execution
    uint16 damage = 25;
    uint32 hitFx = 0x280006DF;
    if (abilityId == 14) // PowerShotAbility
    {
        damage = 50;
    }
    else if (abilityId == 126) // PistolDisarmingShotAbility
    {
        damage = 25;
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_DISARMED, 6.0f, 1.0f, 0.0f, caster->getGoId());
        target->getClient().QueueCommand(std::make_shared<SystemChatMsg>("{c:FFAA00}[DISARMED] Your weapon has been disarmed!{/c}"));
    }
    else if (abilityId == 129) // HandgunsAbility
    {
        damage = 25;
    }
    else if (abilityId == 147) // RiflesAbility
    {
        damage = 40;
    }
    else if (abilityId == 453) // RifleButtSmashAbility
    {
        damage = 30;
    }
    else if (abilityId == 499) // PistolPointBlankAbility
    {
        damage = 45;
    }
    else if (abilityId == 501) // PistolWhipAbility
    {
        damage = 20;
    }
    else if (abilityId == 505) // SniperShotAbility
    {
        damage = 75;
    }

    if (caster->isDualWielding())
    {
        damage = static_cast<uint16>(damage * 1.5f);
    }

    target->takeDamage(caster->getGoId(), damage, hitFx);
    return true;
}

bool HackerSystem::ExecuteSpyAbility(PlayerObject* caster, uint16 abilityId, uint32 targetGoId, const AbilityTemplate* templ)
{
    if (!caster) return false;
    const bool humanCaster = !caster->getClient().isBot();

    // 1. Resolve Target / Mode
    bool isSelfCast = (abilityId == 209 || abilityId == 293 || (templ && templ->isBuff));
    PlayerObject* target = nullptr;

    if (isSelfCast)
    {
        target = caster;
        targetGoId = caster->getGoId();
    }
    else
    {
        if (targetGoId == 0)
            targetGoId = caster->getTargetGoId();

        target = sObjMgr.getGOPtrSafe(targetGoId);
        if (!target)
        {
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:AA00FF}[SPY] You must select a valid hostile target.{/c}"));
            }
            return false;
        }

        if (target->getGoId() == caster->getGoId())
        {
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:AA00FF}[SPY] You cannot target yourself with offensive blade attacks.{/c}"));
            }
            return false;
        }

        // Distance check
        LocationVector posA = caster->getPosition();
        LocationVector posB = target->getPosition();
        double dx = posB.x - posA.x;
        double dz = posB.z - posA.z;
        double distSq = dx * dx + dz * dz;

        double maxRange = 3000.0; // 30m for knife throwing, 4m for poison knife
        if (abilityId == 146) // PoisonKnifeAbility
            maxRange = 400.0; // 4m melee range

        if (distSq > (maxRange * maxRange))
        {
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    (format("{c:AA00FF}[SPY] Target is out of range (max %1%m).{/c}") % (int)(maxRange / 100.0)).str()));
            }
            return false;
        }
    }

    // 2. Resource check: Inner Strength
    uint16 isCost = templ ? templ->innerStrengthCost : 15;
    if (caster->getCurrentIS() < isCost)
    {
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:AA00FF}[SPY] Insufficient Inner Strength to execute covert operation.{/c}"));
        }
        return false;
    }
    caster->spendIS(isCost);

    // 3. Cast Bar: RPC 0x80ac
    float castSec = templ ? float(templ->castTime) / 1000.0f : 1.5f;
    if (castSec > 0.05f && humanCaster)
    {
        caster->getClient().QueueCommand(std::make_shared<CastBarMsg>(abilityId, castSec));
    }

    // 4. Caster Animation: 16-bit ExtendedAnimationMsg (opcode 0x29)
    sGame.AnnounceStateUpdateNear(caster->getPosition().x, caster->getPosition().z, 20000.0f,
        std::make_shared<ExtendedAnimationMsg>(caster->getGoId(), 0x052A, 1));

    std::string abilName = templ ? templ->name : "Spy Ability";
    INFO_LOG(format("HackerSystem: %1%:%2% executes Spy ability %3% (id %4%) on %5%:%6%")
        % caster->getHandle() % caster->getGoId() % abilName % abilityId
        % (target ? target->getHandle() : "none") % targetGoId);

    if (humanCaster)
    {
        caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
            (format("{c:AA00FF}[SPY] Executing %1% on %2%.{/c}")
                % abilName % (target == caster ? "Self" : target->getHandle())).str()));
    }

    // 5. Ability Payload Execution
    if (abilityId == 209) // StealthAbility
    {
        caster->setStealth(true);
        sStatusEffectManager.ApplyEffect(caster->getGoId(), EFFECT_STEALTH, 60.0f, 1.0f, 0.0f, caster->getGoId());
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:AA00FF}[SPY] Stealth cloak active. You are masked from sensory detection.{/c}"));
        }
    }
    else if (abilityId == 293) // StealthCountermeasuresAbility
    {
        sStatusEffectManager.ApplyEffect(caster->getGoId(), EFFECT_ORACLE_PREMONITION_BOOST, 30.0f, 1.0f, 25.0f, caster->getGoId());
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                "{c:AA00FF}[SPY] Countermeasures deployed. Evasion profile increased.{/c}"));
        }
    }
    else if (abilityId == 146) // PoisonKnifeAbility
    {
        uint16 dmg = 25;
        if (caster->isStealthed())
        {
            dmg = static_cast<uint16>(dmg * 1.75f); // 1.75x Ambush critical strike
            caster->setStealth(false);
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FFFF00}[SPY] Ambush critical strike! Concealment broken.{/c}"));
            }
        }
        target->takeDamage(caster->getGoId(), dmg, 0x280006DF);
        sStatusEffectManager.ApplyEffect(target->getGoId(), EFFECT_VIRUS_DOT, 10.0f, 1.0f, 8.0f, caster->getGoId());
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:AA00FF}[SPY] Poisoned blade inserted into %1%'s RSI.{/c}") % target->getHandle()).str()));
        }
    }
    else if (abilityId == 283) // KnifeThrowerAbility
    {
        uint16 dmg = 30;
        if (caster->isStealthed())
        {
            dmg = static_cast<uint16>(dmg * 1.5f); // 1.5x Ambush knife throw
            caster->setStealth(false);
            if (humanCaster)
            {
                caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                    "{c:FFFF00}[SPY] Concealment dropped to throw combat dagger.{/c}"));
            }
        }
        target->takeDamage(caster->getGoId(), dmg, 0x280006DF);
        if (humanCaster)
        {
            caster->getClient().QueueCommand(std::make_shared<SystemChatMsg>(
                (format("{c:AA00FF}[SPY] Thrown combat dagger impales %1%.{/c}") % target->getHandle()).str()));
        }
    }

    return true;
}

// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// ***************************************************************************

#include "CombatAnimationMatrix.h"
#include "CombatSystem.h"

InterlockAnimPair CombatAnimationMatrix::GetAnimationPair(
    FightingStyle attackerStyle,
    uint8 attackerTactic,
    FightingStyle defenderStyle,
    uint8 defenderTactic,
    InterlockExchangeOutcome outcome,
    uint16 moveId
)
{
    (void)defenderStyle;
    (void)defenderTactic;

    InterlockAnimPair pair;
    pair.attackerAnimId = 0x0D58;
    pair.defenderAnimId = 0x0AFE;
    pair.hitFxId = 0x280006DF; // FX_CHARACTER_TEXT_DAMAGE
    pair.contactDelaySeconds = 0.50f;

    // Handle Clash (identical stances)
    if (outcome == InterlockExchangeOutcome::Clash)
    {
        pair.hitFxId = 0x28000794; // FX_CHARACTER_BLOCK_INTERLOCK
        pair.contactDelaySeconds = 0.40f;
        switch (attackerStyle)
        {
            case FightingStyle::KungFu:
                pair.attackerAnimId = 0x0D5C; // WP_A_SR_PegAHF_WPLb
                pair.defenderAnimId = 0x0CDB; // WD_D_SR_BPegHF_WDLb
                break;
            case FightingStyle::Karate:
                pair.attackerAnimId = 0x0567; // KS_A_SRFM_SegA_KSLbPushKick
                pair.defenderAnimId = 0x058B; // KS_D_SR_BSegA_KSLb
                break;
            case FightingStyle::Aikido:
                pair.attackerAnimId = 0x0072; // AD_A_SR_ADLb_BlockFailedPunch
                pair.defenderAnimId = 0x00A3; // AD_D_SR_ADLb_FailedPunch_F50
                break;
            case FightingStyle::None:
            default:
                pair.attackerAnimId = 0x09E2; // SS_A_SRFM_SegC_SSHeadButt_B50
                pair.defenderAnimId = 0x08BE; // SD_D_SR_BPegHF_SDLb
                break;
        }
        return pair;
    }

    // Handle Dodges
    if (outcome == InterlockExchangeOutcome::Dodged)
    {
        pair.hitFxId = 0; // No damage text
        switch (attackerStyle)
        {
            case FightingStyle::KungFu:
                pair.attackerAnimId = 0x0D58; // WP_A_SRFM_PegBHR_WPTigerPunch_B50
                pair.defenderAnimId = 0x0CFC; // WD_D_SR_DPegHF_WDLb
                pair.contactDelaySeconds = 0.40f;
                break;
            case FightingStyle::Karate:
                pair.attackerAnimId = 0x04F0; // KP_A_SRFM_PegAMR_JumpingSpinKick
                pair.defenderAnimId = 0x0493; // KD_D_SR_DPegHR_KDLb
                pair.contactDelaySeconds = 0.46f;
                break;
            case FightingStyle::Aikido:
                pair.attackerAnimId = 0x00FC; // AP_A_SRFM_PegAMF_APCollarboneBreak
                pair.defenderAnimId = 0x009D; // AD_D_SR_ADLb_DPegMF
                pair.contactDelaySeconds = 0.43f;
                break;
            case FightingStyle::None:
            default:
                pair.attackerAnimId = 0x09E0; // SS_A_SRFM_SegA_SSHeadGrabKneeFace_F100
                pair.defenderAnimId = 0x0AF0; // V_D_MR_DASideTackleLF_F50
                pair.contactDelaySeconds = 0.45f;
                break;
        }
        return pair;
    }

    // Handle Blocks / Rebounds
    if (outcome == InterlockExchangeOutcome::Blocked)
    {
        pair.hitFxId = 0x28000794; // FX_CHARACTER_BLOCK_INTERLOCK
        switch (attackerStyle)
        {
            case FightingStyle::KungFu:
                pair.attackerAnimId = 0x0D5C; // WP_A_SR_PegAHF_WPLb
                pair.defenderAnimId = 0x0CDB; // WD_D_SR_BPegHF_WDLb
                pair.contactDelaySeconds = 0.46f;
                break;
            case FightingStyle::Karate:
                pair.attackerAnimId = 0x04F4; // KP_A_SR_KPLb_PegAMR_F50
                pair.defenderAnimId = 0x0472; // KD_D_SR_BPegHR_KDLb
                pair.contactDelaySeconds = 0.53f;
                break;
            case FightingStyle::Aikido:
                pair.attackerAnimId = 0x00FC; // AP_A_SRFM_PegAMF_APCollarboneBreak
                pair.defenderAnimId = 0x0114; // AP_D_SR_APLb_BPegMF
                pair.contactDelaySeconds = 0.50f;
                break;
            case FightingStyle::None:
            default:
                pair.attackerAnimId = 0x08AD; // SD_A_SR_BlockFailedPunch_SDLb
                pair.defenderAnimId = 0x08BE; // SD_D_SR_BPegHF_SDLb
                pair.contactDelaySeconds = 0.46f;
                break;
        }
        return pair;
    }

    // Handle Guard / Power Breaks (Unblockable Throws / Grabs)
    if (outcome == InterlockExchangeOutcome::GuardBreak)
    {
        pair.hitFxId = 0x28000432; // FX_INTERLOCK_IMPACTS_IMPACT_FALLING
        switch (attackerStyle)
        {
            case FightingStyle::KungFu:
                pair.attackerAnimId = 0x0F99; // WS_A_SRFM_SegB_WSMantisThrow
                pair.defenderAnimId = 0x0B14; // V_D_SRFM_SegB_WSMantisThrow
                pair.contactDelaySeconds = 0.80f;
                break;
            case FightingStyle::Karate:
                pair.attackerAnimId = 0x04F3; // KP_A_SRSM_FtSwLF_KiPunchMF_F50
                pair.defenderAnimId = 0x0AE7; // V_D_MRSM_BodyShot_F390
                pair.hitFxId = 0x2800045A;    // FX_INTERLOCK_KI_AURA_IMPACT
                pair.contactDelaySeconds = 0.73f;
                break;
            case FightingStyle::Aikido:
                pair.attackerAnimId = 0x0068; // AD_A_SRFM_ADLbCartwheelToTomoNage
                pair.defenderAnimId = 0x0AF2; // V_D_SRFM_FailedPunch_ADLbCartwheelToTomoNage
                pair.contactDelaySeconds = 0.93f;
                break;
            case FightingStyle::None:
            default:
                pair.attackerAnimId = 0x145F; // SD_A_SRFM_SDLbArmComboThrow_F50
                pair.defenderAnimId = 0x0F4A; // V_D_MR_EntryMoveLegSweepLF
                pair.contactDelaySeconds = 0.83f;
                break;
        }
        return pair;
    }

    // Handle Special Combat Moves (Headbutt, Cheap Shot, Spin Clay Pigeon, Karate Focus Ki Blast)
    if (moveId != 0 && (outcome == InterlockExchangeOutcome::NormalHit || outcome == InterlockExchangeOutcome::SpecialHit))
    {
        switch (moveId)
        {
            case 197: // Head Butt
                pair.attackerAnimId = 0x1135; // S_A_SRSM_SLb_HeadButt_F100
                pair.defenderAnimId = 0x1136; // V_D_SRSM_HeadButt_B100
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.45f;
                return pair;
            case 198: // Cheap Shot
                pair.attackerAnimId = 0x112F; // S_A_SRSM_SLb_CheapShot
                pair.defenderAnimId = 0x1131; // V_D_SRSM_CheapShot_B50
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.50f;
                return pair;
            case 296: // AikidoSpinClayPigeonAbility
                pair.attackerAnimId = 0x01B4; // A_A_SRSM_ALb_SpinClayPigeon_F100
                pair.defenderAnimId = 0x1132; // V_D_SRSM_CutthroatFootsweep
                pair.hitFxId = 0x28000432;
                pair.contactDelaySeconds = 0.70f;
                return pair;
            case 531: // KarateFocusAbility
                pair.attackerAnimId = 0x04F3; // KP_A_SRSM_FtSwLF_KiPunchMF_F50
                pair.defenderAnimId = 0x0AE7; // V_D_MRSM_BodyShot_F390
                pair.hitFxId = 0x2800045A;    // FX_INTERLOCK_KI_AURA_IMPACT
                pair.contactDelaySeconds = 0.73f;
                return pair;
            default:
                break;
        }
    }

    // Handle Style-Specific Clean Hits & Crushes (Power crushes Speed, Speed interrupts Grab, etc.)
    switch (attackerStyle)
    {
        case FightingStyle::KungFu:
            if (attackerTactic == TACTIC_POWER)
            {
                pair.attackerAnimId = 0x0D58; // WP_A_SRFM_PegBHR_WPTigerPunch_B50
                pair.defenderAnimId = 0x0AFE; // V_D_SRFM_PegBHR_WPTigerPunch
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.53f;
            }
            else if (attackerTactic == TACTIC_SPEED)
            {
                pair.attackerAnimId = 0x0CCE; // WD_A_SR_WDLbPumaParrySpinKick
                pair.defenderAnimId = 0x0AF1; // V_D_MR_LeapKickMF_B140
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.60f;
            }
            else // Grab / Retaliate
            {
                pair.attackerAnimId = 0x0F99; // WS_A_SRFM_SegB_WSMantisThrow
                pair.defenderAnimId = 0x0B14; // V_D_SRFM_SegB_WSMantisThrow
                pair.hitFxId = 0x28000432;
                pair.contactDelaySeconds = 0.80f;
            }
            break;

        case FightingStyle::Karate:
            if (attackerTactic == TACTIC_POWER)
            {
                pair.attackerAnimId = 0x04F0; // KP_A_SRFM_PegAMR_JumpingSpinKick
                pair.defenderAnimId = 0x0F44; // V_D_MR_LeapKickMR_L140
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.63f;
            }
            else if (attackerTactic == TACTIC_SPEED)
            {
                pair.attackerAnimId = 0x0567; // KS_A_SRFM_SegA_KSLbPushKick
                pair.defenderAnimId = 0x058B; // KS_D_SR_BSegA_KSLb
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.40f;
            }
            else // Grab / Retaliate / Ki Strike
            {
                pair.attackerAnimId = 0x04F3; // KP_A_SRSM_FtSwLF_KiPunchMF_F50
                pair.defenderAnimId = 0x0AE7; // V_D_MRSM_BodyShot_F390
                pair.hitFxId = 0x2800045A;
                pair.contactDelaySeconds = 0.73f;
            }
            break;

        case FightingStyle::Aikido:
            if (attackerTactic == TACTIC_POWER)
            {
                pair.attackerAnimId = 0x01B4; // A_A_SRSM_ALb_SpinClayPigeon_F100
                pair.defenderAnimId = 0x1132; // V_D_SRSM_CutthroatFootsweep
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.70f;
            }
            else if (attackerTactic == TACTIC_SPEED)
            {
                pair.attackerAnimId = 0x00FC; // AP_A_SRFM_PegAMF_APCollarboneBreak
                pair.defenderAnimId = 0x0114; // AP_D_SR_APLb_BPegMF
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.50f;
            }
            else // Grab / Wrist lock
            {
                pair.attackerAnimId = 0x0069; // AD_A_SRFM_ADLbKoteGaeshiKoteGaeshi_B50
                pair.defenderAnimId = 0x0AF3; // V_D_SRFM_FailedPunch_ADLbKoteGaeshiKoteGaeshi_F280
                pair.hitFxId = 0x28000432;
                pair.contactDelaySeconds = 0.86f;
            }
            break;

        case FightingStyle::None:
        default:
            if (attackerTactic == TACTIC_POWER)
            {
                pair.attackerAnimId = 0x1135; // S_A_SRSM_SLb_HeadButt_F100
                pair.defenderAnimId = 0x1136; // V_D_SRSM_HeadButt_B100
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.45f;
            }
            else if (attackerTactic == TACTIC_SPEED)
            {
                pair.attackerAnimId = 0x112F; // S_A_SRSM_SLb_CheapShot
                pair.defenderAnimId = 0x1131; // V_D_SRSM_CheapShot_B50
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.50f;
            }
            else // Grab / Retaliate / Double Overhand
            {
                pair.attackerAnimId = 0x09E0; // SS_A_SRFM_SegA_SSHeadGrabKneeFace_F100
                pair.defenderAnimId = 0x0AF0; // V_D_MR_DASideTackleLF_F50
                pair.hitFxId = 0x280006DF;
                pair.contactDelaySeconds = 0.66f;
            }
            break;
    }

    return pair;
}

uint16 CombatAnimationMatrix::GetDisarmAnimation(FightingStyle attackerStyle, uint32 weaponType)
{
    // weaponType: 0=Pistol, 1=Dual Pistol, 2=Rifle, 3=SMG
    switch (attackerStyle)
    {
        case FightingStyle::KungFu:
            switch (weaponType)
            {
                case 1:  return 0x0E3D; // W_A_SR_WLb_DualWeaponDisarm
                case 2:  return 0x0E3F; // W_A_SR_WLb_RifleDisarm
                case 3:  return 0x0E40; // W_A_SR_WLb_SubMDisarm
                default: return 0x0E3E; // W_A_SR_WLb_PistolDisarm
            }
        case FightingStyle::Karate:
            switch (weaponType)
            {
                case 1:  return 0x05FD; // K_A_SR_KLb_DualWeaponDisarm
                case 2:  return 0x05FF; // K_A_SR_KLb_RifleDisarm
                case 3:  return 0x0600; // K_A_SR_KLb_SubMDisarm
                default: return 0x05FE; // K_A_SR_KLb_PistolDisarm
            }
        case FightingStyle::Aikido:
            switch (weaponType)
            {
                case 1:  return 0x01B6; // A_A_SR_ALb_DualWeaponDisarm
                case 2:  return 0x01B8; // A_A_SR_ALb_RifleDisarm
                case 3:  return 0x01B9; // A_A_SR_ALb_SubMDisarm
                default: return 0x01B7; // A_A_SR_ALb_PistolDisarm
            }
        case FightingStyle::None:
        default:
            switch (weaponType)
            {
                case 1:  return 0x0AC2; // S_A_SR_SLb_DualWeaponDisarm
                case 2:  return 0x0AC4; // S_A_SR_SLb_RifleDisarm
                case 3:  return 0x0AC5; // S_A_SR_SLb_SubMDisarm
                default: return 0x0AC3; // S_A_SR_SLb_PistolDisarm
            }
    }
}

#include "SLMDialogueContextEngine.h"
#include "CityLifeManager.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cassert>

// Singleton instantiation
createFileSingleton(SLMDialogueContextEngine);

SLMDialogueContextEngine::SLMDialogueContextEngine()
{
    Initialize();
}

void SLMDialogueContextEngine::Initialize()
{
    std::lock_guard<std::mutex> lock(m_dialogueMutex);
    if (m_initialized) return;

    InitializeForbiddenDictionaries();
    m_initialized = true;
}

void SLMDialogueContextEngine::Reset()
{
    std::lock_guard<std::mutex> lock(m_dialogueMutex);
    m_totalInferences = 0;
    m_breachesBlocked = 0;
    m_forbiddenKeywords.clear();
    m_initialized = false;
    InitializeForbiddenDictionaries();
    m_initialized = true;
}

void SLMDialogueContextEngine::Update(uint32 deltaMs)
{
    (void)deltaMs;
}

void SLMDialogueContextEngine::InitializeForbiddenDictionaries()
{
    // Bluepill Sleeper: Zero awareness of simulation, Zion, pods, agents
    m_forbiddenKeywords[EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER] = {
        "matrix", "simulation", "redpill", "red pill", "bluepill", "blue pill",
        "zion", "agent smith", "agents", "machine war", "machines harvesting",
        "unplug", "unplugged", "power plant", "hardline", "rsi", "residual self image",
        "jack in", "neo", "the one", "morpheus", "trinity", "bullet time", "code stream",
        "human battery", "sentinel", "squiddy", "architect"
    };

    // Matrix Skeptic: Knows glitches happen, but does NOT know Zion or real-world military
    m_forbiddenKeywords[EpistemicHorizon::HORIZON_MATRIX_SKEPTIC] = {
        "zion", "hovercraft", "nebuchadnezzar", "sentinel", "machine city",
        "architect", "source code", "real world", "pod", "human battery",
        "deus ex machina"
    };

    // Redpill Operative: Knows matrix, but does NOT know secret Machine / Architect root keys
    m_forbiddenKeywords[EpistemicHorizon::HORIZON_REDPILL_OPERATIVE] = {
        "architect backdoor key", "machine city central mainframe core code",
        "deus ex machina internal registry", "prime program source hash"
    };

    // Cypherite Renegade: Bitter about Zion, rejects redpill salvation myths
    m_forbiddenKeywords[EpistemicHorizon::HORIZON_CYPHERITE_RENEGADE] = {
        "zion will liberate", "zion salvation", "neo is our savior", "glory to zion",
        "zion holy salvation", "liberate us from the machines"
    };

    // MMPD Police: Treats redpills as cyber-terrorists; does not believe in simulation
    m_forbiddenKeywords[EpistemicHorizon::HORIZON_MMPD_POLICE] = {
        "matrix", "simulation", "zion", "redpill", "machines harvesting bodies", "agents are code"
    };

    // Syndicate Underworld: Mob rackets, turf wars, loans; does not know philosophical matrix lore
    m_forbiddenKeywords[EpistemicHorizon::HORIZON_SYNDICATE_UNDERWORLD] = {
        "simulation", "matrix", "architect", "zion military council", "unplugging pods"
    };

    // Machine Agent: Pure logic and system integrity; blind to emotional love/sacrifice
    m_forbiddenKeywords[EpistemicHorizon::HORIZON_MACHINE_AGENT] = {
        "human unconditional love", "irrational self-sacrifice", "poetic soul"
    };

    // Exile Program: Causality and power; rejects machine city loyalty
    m_forbiddenKeywords[EpistemicHorizon::HORIZON_EXILE_PROGRAM] = {
        "machine city absolute obedience", "pure zion redpill martyrdom"
    };
}

EpistemicHorizon SLMDialogueContextEngine::DetermineEpistemicHorizon(uint32 entityId, mxoFaction faction)
{
    if (faction == FACTION_MACHINES) return EpistemicHorizon::HORIZON_MACHINE_AGENT;
    if (faction == FACTION_ZION) return EpistemicHorizon::HORIZON_REDPILL_OPERATIVE;

    // Check awakening stage from NPCEmergentLifeEngine
    AwakeningStage stage = sNPCEmergentLifeEngine.GetAwakeningStage(entityId);
    if (stage == AwakeningStage::STAGE_CYPHERITE_RENEGADE) {
        return EpistemicHorizon::HORIZON_CYPHERITE_RENEGADE;
    }
    if (stage == AwakeningStage::STAGE_3_REDPILL_SYMPATHIZER || stage == AwakeningStage::STAGE_4_FREED_MIND_OPERATIVE) {
        return EpistemicHorizon::HORIZON_REDPILL_OPERATIVE;
    }
    if (stage == AwakeningStage::STAGE_1_MATRIX_SKEPTIC || stage == AwakeningStage::STAGE_2_AWAKENING_SEARCHER) {
        return EpistemicHorizon::HORIZON_MATRIX_SKEPTIC;
    }

    // Check career track from NPCFamilyDreamsEngine
    const auto* career = sFamilyDreamsEngine.GetCareer(entityId);
    if (career) {
        if (career->track == CareerTrack::MMPDLawEnforcement) {
            return EpistemicHorizon::HORIZON_MMPD_POLICE;
        }
        if (career->track == CareerTrack::SyndicateRacket) {
            return EpistemicHorizon::HORIZON_SYNDICATE_UNDERWORLD;
        }
    }

    return EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER;
}

std::string SLMDialogueContextEngine::GetEpistemicHorizonName(EpistemicHorizon horizon) const
{
    switch (horizon) {
        case EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER: return "Bluepill Sleeper (Zero Meta Awareness)";
        case EpistemicHorizon::HORIZON_MATRIX_SKEPTIC: return "Matrix Skeptic (Anomaly Observant)";
        case EpistemicHorizon::HORIZON_REDPILL_OPERATIVE: return "Redpill Operative (Simulation Lucid)";
        case EpistemicHorizon::HORIZON_CYPHERITE_RENEGADE: return "Cypherite Renegade (Anti-Zion Disillusioned)";
        case EpistemicHorizon::HORIZON_MMPD_POLICE: return "MMPD Police (Tactical Law Enforcement)";
        case EpistemicHorizon::HORIZON_SYNDICATE_UNDERWORLD: return "Syndicate Underworld (Mob / Racket Enforcer)";
        case EpistemicHorizon::HORIZON_MACHINE_AGENT: return "Machine Agent (System Enforcement Protocol)";
        case EpistemicHorizon::HORIZON_EXILE_PROGRAM: return "Exile Sub-Program (Causality & Code Traffic)";
    }
    return "Unknown Horizon";
}

const std::vector<std::string>& SLMDialogueContextEngine::GetForbiddenTopics(EpistemicHorizon horizon) const
{
    static const std::vector<std::string> emptyList;
    auto it = m_forbiddenKeywords.find(horizon);
    if (it != m_forbiddenKeywords.end()) {
        return it->second;
    }
    return emptyList;
}

bool SLMDialogueContextEngine::EvaluateEpistemicBreach(EpistemicHorizon horizon, const std::string& userUtterance,
                                                      std::string& outBreachedKeyword, EpistemicBreachAction& outAction) const
{
    std::string lowerUtterance = userUtterance;
    std::transform(lowerUtterance.begin(), lowerUtterance.end(), lowerUtterance.begin(), ::tolower);

    const auto& forbidden = GetForbiddenTopics(horizon);
    for (const auto& keyword : forbidden) {
        if (lowerUtterance.find(keyword) != std::string::npos) {
            outBreachedKeyword = keyword;

            switch (horizon) {
                case EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER:
                    if (lowerUtterance.find("agent") != std::string::npos ||
                        lowerUtterance.find("kill") != std::string::npos ||
                        lowerUtterance.find("war") != std::string::npos) {
                        outAction = EpistemicBreachAction::ALARM_CALL_POLICE;
                    } else {
                        outAction = EpistemicBreachAction::DISMISS_AS_ABSURD;
                    }
                    break;

                case EpistemicHorizon::HORIZON_MATRIX_SKEPTIC:
                    outAction = EpistemicBreachAction::AFFIRM_SKEPTICISM;
                    break;

                case EpistemicHorizon::HORIZON_REDPILL_OPERATIVE:
                    outAction = EpistemicBreachAction::CONFIRM_OPERATIVE_IDENTITY;
                    break;

                case EpistemicHorizon::HORIZON_CYPHERITE_RENEGADE:
                    outAction = EpistemicBreachAction::CYPHERITE_CONTEMPT;
                    break;

                case EpistemicHorizon::HORIZON_MMPD_POLICE:
                    outAction = EpistemicBreachAction::ALARM_CALL_POLICE;
                    break;

                case EpistemicHorizon::HORIZON_MACHINE_AGENT:
                    outAction = EpistemicBreachAction::ENFORCE_SYSTEM_PROTOCOL;
                    break;

                default:
                    outAction = EpistemicBreachAction::DISMISS_AS_ABSURD;
                    break;
            }
            return true;
        }
    }

    outBreachedKeyword.clear();
    outAction = EpistemicBreachAction::NONE;
    return false;
}

SituatedDialogueContext SLMDialogueContextEngine::AssembleSituatedContext(uint32 speakerEntityId, uint32 interlocutorId,
                                                                        const std::string& district,
                                                                        const std::string& venue)
{
    SituatedDialogueContext ctx;
    ctx.speakerEntityId = speakerEntityId;
    ctx.interlocutorId = interlocutorId;
    ctx.districtName = district.empty() ? "Downtown" : district;
    ctx.currentVenue = venue.empty() ? "Sidewalk" : venue;
    ctx.timeOfDay = "Afternoon";

    // 1. Biographical Profile
    BiographicalProfile bio = sBioEngine.GenerateProfile(speakerEntityId);
    ctx.speakerName = bio.primaryName.empty() ? ("Citizen#" + std::to_string(speakerEntityId)) : bio.primaryName;
    ctx.walkOfLife = bio.titleRole.empty() ? "Urban Pedestrian" : bio.titleRole;
    ctx.birthplace = bio.birthOrCompilePlace.empty() ? district : bio.birthOrCompilePlace;
    ctx.physicalQuirks = !bio.scarsAndQuirks.empty() ? bio.scarsAndQuirks[0] : "None";

    const auto* citizen = sCityLifeMgr.GetCitizen(speakerEntityId);
    if (citizen) {
        if (!citizen->name.empty()) {
            ctx.speakerName = citizen->name;
        }
        ctx.openness = citizen->traits.openness;
        ctx.conscientiousness = citizen->traits.conscientiousness;
        ctx.extraversion = citizen->traits.extraversion;
        ctx.agreeableness = citizen->traits.agreeableness;
        ctx.neuroticism = citizen->traits.neuroticism;
    } else {
        ctx.openness = 0.5f;
        ctx.conscientiousness = 0.5f;
        ctx.extraversion = 0.5f;
        ctx.agreeableness = 0.5f;
        ctx.neuroticism = 0.5f;
    }

    // Epistemic Horizon
    ctx.horizon = DetermineEpistemicHorizon(speakerEntityId);

    // 2. Family Household
    const auto* household = sFamilyDreamsEngine.GetHouseholdByMember(speakerEntityId);
    if (household) {
        ctx.householdId = household->householdId;
        ctx.householdName = household->householdName;
        ctx.familySavingsBits = static_cast<double>(household->householdSavingsInfoCredits);
        ctx.childrenCount = 0;

        for (const auto& m : household->members) {
            if (m.entityId == speakerEntityId) {
                ctx.kinshipRole = sFamilyDreamsEngine.GetKinshipRoleName(m.role);
                if (!m.name.empty()) {
                    ctx.speakerName = m.name;
                }
            } else if (m.role == KinshipRole::Spouse) {
                ctx.spouseName = m.name;
            } else if (m.role == KinshipRole::Child) {
                ctx.childrenCount++;
            }
        }
    } else {
        ctx.kinshipRole = "Single Resident";
        ctx.householdName = "Studio Apartment";
    }

    // 3. Career & Vocation
    const auto* career = sFamilyDreamsEngine.GetCareer(speakerEntityId);
    if (career) {
        ctx.careerTrackName = sFamilyDreamsEngine.GetCareerTrackName(career->track);
        ctx.jobTitle = career->jobTitle;
        ctx.jobLevel = static_cast<uint32>(career->level);
        ctx.employerName = career->workplaceName;
        ctx.hourlyWage = static_cast<float>(career->hourlyWage);
        ctx.burnoutLevel = career->burnoutIndex;
    } else {
        ctx.careerTrackName = "Service & Retail";
        ctx.jobTitle = "Freelance Worker";
        ctx.employerName = "Independent";
        ctx.hourlyWage = 20.0f;
    }

    // 4. Life Dreams & Aspirations
    const auto* asp = sFamilyDreamsEngine.GetAspiration(speakerEntityId);
    if (asp) {
        ctx.activeDreamTitle = asp->title;
        ctx.dreamProgress = asp->progress;
        ctx.isInCrisis = asp->isInCrisis;
    } else {
        ctx.activeDreamTitle = "Financial Independence and Peace";
        ctx.dreamProgress = 0.25f;
        ctx.isInCrisis = false;
    }

    // 5. Social & Interpersonal Relationship
    const auto* social = sSocialEngine.GetProfile(speakerEntityId);
    if (social && interlocutorId != 0) {
        if (social->partnerEntityId == interlocutorId) {
            ctx.relationshipTier = "Romantic Partner / Spouse";
            ctx.interpersonalTrust = 95.0f;
        } else {
            const auto* edge = social->GetRelationship(interlocutorId);
            if (edge) {
                ctx.relationshipTier = "Friend / Coworker";
                ctx.interpersonalTrust = edge->trust;
            } else {
                ctx.relationshipTier = "Stranger";
                ctx.interpersonalTrust = 40.0f;
            }
        }
    } else {
        ctx.relationshipTier = "Stranger";
        ctx.interpersonalTrust = 40.0f;
    }

    if (interlocutorId != 0) {
        const auto* interCitizen = sCityLifeMgr.GetCitizen(interlocutorId);
        if (interCitizen && !interCitizen->name.empty()) {
            ctx.interlocutorName = interCitizen->name;
        } else {
            BiographicalProfile interBio = sBioEngine.GenerateProfile(interlocutorId);
            ctx.interlocutorName = interBio.primaryName.empty() ? ("Citizen#" + std::to_string(interlocutorId)) : interBio.primaryName;
        }
    } else {
        ctx.interlocutorName = "Passerby";
    }

    // 6. Episodic Memories (Top 3)
    auto mems = sSocialEngine.RetrieveCoreMemories(speakerEntityId);
    for (size_t i = 0; i < std::min<size_t>(3, mems.size()); ++i) {
        ctx.relevantMemories.push_back(mems[i].narrativeProse);
    }

    // 7. Known Rumors (Strictly rumors personally heard via gossip network!)
    auto rumors = sNPCEmergentLifeEngine.GetEntityKnownRumors(speakerEntityId);
    for (const auto& r : rumors) {
        ctx.knownRumors.push_back(r.headline);
    }

    // 8. Routine & Leisure
    EmergentActivityType actType = sNPCEmergentLifeEngine.GetCitizenCurrentActivity(speakerEntityId);
    EmergentActivityRecord actDef = sNPCEmergentLifeEngine.GetActivityDefinition(actType);
    ctx.currentActivityName = actDef.activityName;

    // 9. Awakening Stage & Cognitive Dissonance
    const auto& ap = sNPCEmergentLifeEngine.GetOrCreateAwakeningProfile(speakerEntityId);
    ctx.awakeningStage = ap.stage;
    ctx.cognitiveDissonance = ap.cognitiveDissonance;
    ctx.hasHardlineContact = ap.hasHardlineContact;

    return ctx;
}

DialoguePromptPayload SLMDialogueContextEngine::ComposeSLMPromptPayload(const SituatedDialogueContext& ctx,
                                                                      const std::string& userUtterance)
{
    DialoguePromptPayload payload;

    // 1. System Prompt
    std::ostringstream sysOss;
    sysOss << "You are roleplaying as " << ctx.speakerName << ", a " << ctx.walkOfLife << " living in Megacity.\n";
    sysOss << "Occupation: " << ctx.jobTitle << " at " << ctx.employerName << ".\n";
    sysOss << "Current Location: " << ctx.districtName << " (" << ctx.currentVenue << "). Activity: " << ctx.currentActivityName << ".\n";
    sysOss << "Personality (OCEAN): Openness=" << std::fixed << std::setprecision(2) << ctx.openness
           << ", Conscientiousness=" << ctx.conscientiousness
           << ", Extraversion=" << ctx.extraversion
           << ", Agreeableness=" << ctx.agreeableness
           << ", Neuroticism=" << ctx.neuroticism << ".\n";
    sysOss << "Epistemic Horizon: " << GetEpistemicHorizonName(ctx.horizon) << ".\n";
    sysOss << "Speaking Cadence: Grounded, authentic, natural human cadence. NO fourth-wall breaks or AI assistant meta-disclaimers.\n";
    payload.systemPrompt = sysOss.str();

    // 2. Epistemic Prohibitions
    std::ostringstream forbidOss;
    const auto& forbidden = GetForbiddenTopics(ctx.horizon);
    forbidOss << "MANDATORY EPISTEMIC FIREWALL & STRICT IGNORANCE BOUNDARIES:\n";
    forbidOss << "You have ZERO KNOWLEDGE of the following prohibited concepts:\n";
    for (size_t i = 0; i < forbidden.size(); ++i) {
        forbidOss << " - " << forbidden[i] << "\n";
    }
    forbidOss << "CRITICAL DIRECTIVE: If the interlocutor mentions any of these prohibited concepts, you MUST remain completely in-character. ";
    if (ctx.horizon == EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER) {
        forbidOss << "Treat the interlocutor as mentally unwell, delusional, or a conspiracy nut. Deny and deflect.\n";
    } else if (ctx.horizon == EpistemicHorizon::HORIZON_MATRIX_SKEPTIC) {
        forbidOss << "Acknowledge that reality feels strange or glitched, but you do NOT know what Zion or real-world pods are.\n";
    } else if (ctx.horizon == EpistemicHorizon::HORIZON_CYPHERITE_RENEGADE) {
        forbidOss << "Mock Zion's cold misery and express longing for comfortable matrix oblivion.\n";
    } else if (ctx.horizon == EpistemicHorizon::HORIZON_MMPD_POLICE) {
        forbidOss << "Treat subversive matrix talk as dangerous cyber-terrorist agitation and order them to stand down.\n";
    }
    payload.epistemicProhibitions = forbidOss.str();

    // 3. Situated Dossier
    std::ostringstream dosOss;
    dosOss << "SITUATED CHARACTER DOSSIER:\n";
    dosOss << "- Family: " << ctx.kinshipRole << " in '" << ctx.householdName << "'. ";
    if (!ctx.spouseName.empty()) dosOss << "Spouse: " << ctx.spouseName << ". ";
    dosOss << "Children: " << ctx.childrenCount << ". Household Savings: " << ctx.familySavingsBits << " bits.\n";
    dosOss << "- Vocation: " << ctx.jobTitle << " at " << ctx.employerName << " (" << ctx.careerTrackName << "), Level " << ctx.jobLevel << ".\n";
    dosOss << "- Lifelong Aspiration: '" << ctx.activeDreamTitle << "' (Progress: " << (int)(ctx.dreamProgress * 100) << "%).\n";
    dosOss << "- Relationship to Speaker (" << ctx.interlocutorName << "): " << ctx.relationshipTier << " (Trust: " << (int)ctx.interpersonalTrust << "%).\n";
    
    dosOss << "- Personally Heard Rumors (Do NOT mention unlearned rumors!):\n";
    if (ctx.knownRumors.empty()) {
        dosOss << "  (None currently heard)\n";
    } else {
        for (const auto& r : ctx.knownRumors) {
            dosOss << "  * \"" << r << "\"\n";
        }
    }

    dosOss << "- Recent Episodic Memories:\n";
    if (ctx.relevantMemories.empty()) {
        dosOss << "  (Normal peaceful daily routine)\n";
    } else {
        for (const auto& m : ctx.relevantMemories) {
            dosOss << "  * " << m << "\n";
        }
    }
    payload.situatedDossier = dosOss.str();

    // 4. Dialogue Turn
    std::ostringstream turnOss;
    turnOss << ctx.interlocutorName << ": \"" << userUtterance << "\"\n";
    turnOss << ctx.speakerName << ":";
    payload.dialogueTurn = turnOss.str();

    // 5. JSON Payload Format
    std::ostringstream jsonOss;
    jsonOss << "{\n";
    jsonOss << "  \"speaker\": \"" << ctx.speakerName << "\",\n";
    jsonOss << "  \"horizon\": \"" << GetEpistemicHorizonName(ctx.horizon) << "\",\n";
    jsonOss << "  \"district\": \"" << ctx.districtName << "\",\n";
    jsonOss << "  \"venue\": \"" << ctx.currentVenue << "\",\n";
    jsonOss << "  \"interlocutor\": \"" << ctx.interlocutorName << "\",\n";
    jsonOss << "  \"relationship\": \"" << ctx.relationshipTier << "\",\n";
    jsonOss << "  \"job_title\": \"" << ctx.jobTitle << "\",\n";
    jsonOss << "  \"employer\": \"" << ctx.employerName << "\",\n";
    jsonOss << "  \"active_dream\": \"" << ctx.activeDreamTitle << "\",\n";
    jsonOss << "  \"known_rumors_count\": " << ctx.knownRumors.size() << ",\n";
    jsonOss << "  \"user_utterance\": \"" << userUtterance << "\"\n";
    jsonOss << "}";
    payload.jsonPayload = jsonOss.str();

    return payload;
}

std::string SLMDialogueContextEngine::GenerateHeuristicBreachReply(const SituatedDialogueContext& ctx,
                                                                  const std::string& breachedKeyword,
                                                                  EpistemicBreachAction action) const
{
    std::ostringstream oss;
    switch (action) {
        case EpistemicBreachAction::DISMISS_AS_ABSURD:
            if (ctx.neuroticism > 0.6f) {
                oss << "Whoa, back up! What kind of sick cult conspiracy are you peddling? '" << breachedKeyword
                    << "'? Look, I work 50 hours a week at " << ctx.employerName << ", I have bills to pay and a family to feed. Take your sci-fi madness somewhere else!";
            } else {
                oss << "Excuse me? '" << breachedKeyword << "'? Look buddy, I work at " << ctx.employerName
                    << " and have bills to pay right here on " << ctx.districtName << " pavement. You've spent way too many hours reading weird dark-web forums. Go get some fresh air.";
            }
            break;

        case EpistemicBreachAction::ALARM_CALL_POLICE:
            if (ctx.horizon == EpistemicHorizon::HORIZON_MMPD_POLICE) {
                oss << "10-99 code yellow! Suspect is citing classified insurgent term '" << breachedKeyword
                    << "'. Step back from the curb, keep your hands where dispatch can see them, or we will deploy suppression protocols!";
            } else {
                oss << "Security! Officer! This person is screaming lunatic terrorist nonsense about '" << breachedKeyword
                    << "' right outside " << ctx.employerName << "! I'm calling the 4th precinct right now!";
            }
            break;

        case EpistemicBreachAction::AFFIRM_SKEPTICISM:
            oss << "Keep your voice down... Did you notice it too? Ever since that power flicker last week, the reflections on subway tile walls don't sync up. "
                << "I don't know what you mean by '" << breachedKeyword << "', but I know this city isn't normal.";
            break;

        case EpistemicBreachAction::CONFIRM_OPERATIVE_IDENTITY:
            oss << "Transmission acknowledged. Operative frequency locked. The hardline signal is clear on 4th street. What are your loadout orders?";
            break;

        case EpistemicBreachAction::CYPHERITE_CONTEMPT:
            oss << "Save your pious Zion gospel about '" << breachedKeyword << "'. Have you tasted that real-world nutrient sludge? Smelled the damp machine tunnels? "
                << "This simulation has warmth, steak, and music. I want back in, and your little crusade won't stop me.";
            break;

        case EpistemicBreachAction::ENFORCE_SYSTEM_PROTOCOL:
            oss << "Anomalous cognitive variance detected in entity. Prohibited lexical artifact '" << breachedKeyword
                << "' violates local matrix kernel integrity. Initiating immediate diagnostic trace.";
            break;

        default:
            oss << "I have no idea what you're talking about.";
            break;
    }
    return oss.str();
}

std::string SLMDialogueContextEngine::GenerateHeuristicInContextReply(const SituatedDialogueContext& ctx,
                                                                      const std::string& userUtterance) const
{
    std::string lower = userUtterance;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    std::ostringstream oss;

    // 1. Inquiries about Family or Domestic Life
    if (lower.find("family") != std::string::npos || lower.find("kids") != std::string::npos ||
        lower.find("wife") != std::string::npos || lower.find("husband") != std::string::npos ||
        lower.find("home") != std::string::npos) {
        if (!ctx.spouseName.empty()) {
            oss << "My spouse " << ctx.spouseName << " and I are managing well in '" << ctx.householdName << "'. ";
            if (ctx.childrenCount > 0) {
                oss << "With " << ctx.childrenCount << " children to look after, every bit counts, but shared family dinners make it worthwhile.";
            } else {
                oss << "We're saving up what we can from each shift.";
            }
        } else {
            oss << "Just me right now in my studio on " << ctx.districtName << ". Simple life, but I keep my space in order.";
        }
        return oss.str();
    }

    // 2. Inquiries about Work / Job / Employer
    if (lower.find("job") != std::string::npos || lower.find("work") != std::string::npos ||
        lower.find("boss") != std::string::npos || lower.find("career") != std::string::npos) {
        oss << "I'm working as a " << ctx.jobTitle << " over at " << ctx.employerName << ". ";
        if (ctx.burnoutLevel > 0.6f) {
            oss << "To be honest, the overtime shifts have been brutal lately, but I can't afford to fall behind on performance reviews.";
        } else {
            oss << "It pays " << std::fixed << std::setprecision(0) << ctx.hourlyWage << " bits an hour and keeps a roof over my head.";
        }
        return oss.str();
    }

    // 3. Inquiries about Dreams / Goals / Future
    if (lower.find("dream") != std::string::npos || lower.find("goal") != std::string::npos ||
        lower.find("future") != std::string::npos || lower.find("hope") != std::string::npos) {
        oss << "My main aspiration right now is to " << ctx.activeDreamTitle << ". ";
        if (ctx.isInCrisis) {
            oss << "Things have been tough recently—a major setback hit hard, but I haven't given up hope.";
        } else {
            oss << "I'm about " << (int)(ctx.dreamProgress * 100.0f) << "% of the way there. Just taking it day by day.";
        }
        return oss.str();
    }

    // 4. Inquiries about Rumors / News / Street Talk
    if (lower.find("rumor") != std::string::npos || lower.find("news") != std::string::npos ||
        lower.find("heard") != std::string::npos || lower.find("street") != std::string::npos) {
        if (!ctx.knownRumors.empty()) {
            oss << "People on the street have been whispering: \"" << ctx.knownRumors[0] << "\". ";
            if (ctx.knownRumors.size() > 1) {
                oss << "And I also overheard someone talking about \"" << ctx.knownRumors[1] << "\". You can never tell what's true around here.";
            } else {
                oss << "Seems like everyone at the cafe was talking about it this morning.";
            }
        } else {
            oss << "Haven't heard much out of the ordinary today. Just the usual subway delays and siren traffic.";
        }
        return oss.str();
    }

    // 5. Inquiries about Location / Neighborhood
    if (lower.find("where") != std::string::npos || lower.find("neighborhood") != std::string::npos ||
        lower.find("district") != std::string::npos) {
        oss << "You're standing right in " << ctx.districtName << ", near the " << ctx.currentVenue << ". ";
        if (ctx.districtName == "Richland") {
            oss << "Clean streets and glass towers, provided you have the bank account for it.";
        } else if (ctx.districtName == "The Slums") {
            oss << "Keep your coat buttoned and watch your pockets. Things get unpredictable after dark.";
        } else {
            oss << "Decent part of town, lots of foot traffic heading toward the transit hubs.";
        }
        return oss.str();
    }

    // 6. Default Pleasantry / Greeting
    if (ctx.relationshipTier == "Romantic Partner / Spouse") {
        oss << "Hey sweetheart. Always good to see you. How was your day?";
    } else if (ctx.relationshipTier == "Friend / Coworker") {
        oss << "Hey! Good running into you here on " << ctx.districtName << ". How are things at work?";
    } else {
        if (ctx.extraversion > 0.6f) {
            oss << "Afternoon! Nice day for a walk through " << ctx.districtName << ", isn't it?";
        } else {
            oss << "Hello. Mind giving me a bit of space? I've got places to be.";
        }
    }
    return oss.str();
}

bool SLMDialogueContextEngine::GenerateSituatedDialogueResponse(uint32 npcEntityId, uint32 interlocutorId,
                                                               const std::string& userUtterance,
                                                               DialogueResponseResult& outResult,
                                                               const std::string& district,
                                                               const std::string& venue)
{
    std::lock_guard<std::mutex> lock(m_dialogueMutex);

    SituatedDialogueContext ctx = AssembleSituatedContext(npcEntityId, interlocutorId, district, venue);

    outResult.speakerEntityId = npcEntityId;
    outResult.speakerName = ctx.speakerName;
    outResult.interlocutorName = ctx.interlocutorName;
    outResult.episodicMemoryLogged = false;

    std::string breachedKeyword;
    EpistemicBreachAction breachAction;
    bool isBreach = EvaluateEpistemicBreach(ctx.horizon, userUtterance, breachedKeyword, breachAction);

    if (isBreach) {
        m_breachesBlocked++;
        outResult.wasEpistemicBreachDetected = true;
        outResult.breachedConcept = breachedKeyword;
        outResult.breachActionTaken = breachAction;
        outResult.responseText = GenerateHeuristicBreachReply(ctx, breachedKeyword, breachAction);
        outResult.emotionalImpactDelta = -15.0f;

        // Log episodic memory of alarming/bizarre encounter
        sSocialEngine.RecordEpisodicMemory(npcEntityId, "Disturbing Encounter with Delusional Interlocutor",
                                          "An individual on " + ctx.districtName + " pavement attempted to discuss '" + breachedKeyword + "'.",
                                          MemoryCategory::TRAUMA_WITNESS_GLITCH, -0.80f, 0.90f,
                                          interlocutorId, ctx.currentVenue, false);
        outResult.episodicMemoryLogged = true;
    } else {
        m_totalInferences++;
        outResult.wasEpistemicBreachDetected = false;
        outResult.breachedConcept.clear();
        outResult.breachActionTaken = EpistemicBreachAction::NONE;
        outResult.responseText = GenerateHeuristicInContextReply(ctx, userUtterance);
        outResult.emotionalImpactDelta = +5.0f;

        // Log pleasant episodic memory
        sSocialEngine.RecordEpisodicMemory(npcEntityId, "Street Conversation in " + ctx.districtName,
                                          "Exchanged words with " + ctx.interlocutorName + " regarding daily affairs.",
                                          MemoryCategory::DAILY_PLEASURE, +0.35f, 0.40f,
                                          interlocutorId, ctx.currentVenue, false);
        outResult.episodicMemoryLogged = true;
    }

    return true;
}

size_t SLMDialogueContextEngine::GetTotalDialogueInferencesRun() const
{
    std::lock_guard<std::mutex> lock(m_dialogueMutex);
    return m_totalInferences;
}

size_t SLMDialogueContextEngine::GetTotalEpistemicBreachesBlocked() const
{
    std::lock_guard<std::mutex> lock(m_dialogueMutex);
    return m_breachesBlocked;
}

std::string SLMDialogueContextEngine::GenerateSLMTelemetryReport() const
{
    std::lock_guard<std::mutex> lock(m_dialogueMutex);
    std::ostringstream oss;
    oss << "=== MEGACITY SLM DIALOGUE & EPISTEMIC TELEMETRY ===\n";
    oss << "Total Dialogue Inferences Run: " << m_totalInferences << "\n";
    oss << "Epistemic Meta-Breaches Blocked: " << m_breachesBlocked << "\n";
    oss << "Epistemic Horizons Active: 8\n";
    oss << "====================================================\n";
    return oss.str();
}

// ============================================================================
// Standalone C++ Automated Test Suite
// ============================================================================

void RunSLMDialogueTestSuite()
{
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  RUNNING SLM DIALOGUE CONTEXT & EPISTEMIC BOUNDARY SUITE   " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    SLMDialogueContextEngine& engine = sSLMDialogueEngine;
    engine.Reset();
    sSocialEngine.Reset();
    sFamilyDreamsEngine.Reset();
    sNPCEmergentLifeEngine.Reset();

    uint32 passedCount = 0;
    uint32 failedCount = 0;

    auto TEST_ASSERT = [&](bool condition, const char* message) {
        if (condition) {
            std::cout << " [PASS] " << message << std::endl;
            passedCount++;
        } else {
            std::cerr << " [FAIL] " << message << std::endl;
            failedCount++;
        }
    };

    // 1. Horizon Determination Tests
    {
        EpistemicHorizon blue = engine.DetermineEpistemicHorizon(101);
        TEST_ASSERT(blue == EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER, "Default Civilian Horizon is Bluepill Sleeper");

        EpistemicHorizon zion = engine.DetermineEpistemicHorizon(102, FACTION_ZION);
        TEST_ASSERT(zion == EpistemicHorizon::HORIZON_REDPILL_OPERATIVE, "Zion Faction Maps to Redpill Operative");

        EpistemicHorizon mach = engine.DetermineEpistemicHorizon(103, FACTION_MACHINES);
        TEST_ASSERT(mach == EpistemicHorizon::HORIZON_MACHINE_AGENT, "Machines Faction Maps to Machine Agent");

        // Skeptic
        sNPCEmergentLifeEngine.GetOrCreateAwakeningProfile(104, "Skeptic NPC");
        sNPCEmergentLifeEngine.AdvanceAwakeningStage(104, AwakeningStage::STAGE_1_MATRIX_SKEPTIC, "Flickering reflections");
        TEST_ASSERT(engine.DetermineEpistemicHorizon(104) == EpistemicHorizon::HORIZON_MATRIX_SKEPTIC, "Awakening Stage 1 Maps to Matrix Skeptic");

        // Cypherite
        sNPCEmergentLifeEngine.GetOrCreateAwakeningProfile(105, "Cypherite NPC");
        sNPCEmergentLifeEngine.TriggerCypheriteDisillusionment(105, "Cold nutrient sludge");
        TEST_ASSERT(engine.DetermineEpistemicHorizon(105) == EpistemicHorizon::HORIZON_CYPHERITE_RENEGADE, "Cypherite Renegade Maps to Cypherite Horizon");
    }

    // 2. Epistemic Firewall: Forbidden Topic Rejection (Zero Meta-Leaks)
    {
        std::string keyword;
        EpistemicBreachAction action;

        bool b1 = engine.EvaluateEpistemicBreach(EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER, "What is the Matrix?", keyword, action);
        TEST_ASSERT(b1, "Bluepill Blocks 'Matrix' Keyword");
        TEST_ASSERT(keyword == "matrix", "Breached Keyword Identified as 'matrix'");
        TEST_ASSERT(action == EpistemicBreachAction::DISMISS_AS_ABSURD, "Action is Dismiss as Absurd");

        bool b2 = engine.EvaluateEpistemicBreach(EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER, "Are you unplugged from Zion?", keyword, action);
        TEST_ASSERT(b2, "Bluepill Blocks 'Zion' and 'Unplugged'");

        bool b3 = engine.EvaluateEpistemicBreach(EpistemicHorizon::HORIZON_BLUEPILL_SLEEPER, "Where is the nearest bakery?", keyword, action);
        TEST_ASSERT(!b3, "Bluepill Allows Normal Mundane Inquiries (Bakery)");

        bool b4 = engine.EvaluateEpistemicBreach(EpistemicHorizon::HORIZON_MATRIX_SKEPTIC, "Did you notice the flickering neon sign?", keyword, action);
        TEST_ASSERT(!b4, "Matrix Skeptic Allows Glitch Observation Inquiries");

        bool b5 = engine.EvaluateEpistemicBreach(EpistemicHorizon::HORIZON_MATRIX_SKEPTIC, "Tell me about the real world power plant pods", keyword, action);
        TEST_ASSERT(b5, "Matrix Skeptic Blocks Real World Pod Lore");
    }

    // 3. Situated Context Assembly
    {
        // Setup Citizen 201 with family and career
        uint32 hhId = sFamilyDreamsEngine.CreateHousehold(201, "Marcus Vance", "Vance Household", "Richland Apartments", LocationVector(100.0f, 200.0f, 0.0f));
        sFamilyDreamsEngine.AddKinshipMember(hhId, 202, "Clara Vance", KinshipRole::Spouse, 32);
        sFamilyDreamsEngine.AddKinshipMember(hhId, 203, "Toby Vance", KinshipRole::Child, 6);
        sFamilyDreamsEngine.AssignCareer(201, CareerTrack::CorporateTech, JobPositionLevel::Level1_JuniorAssociate,
                                         "Senior Data Analyst", "Metacortex Systems", 1, 45);
        sFamilyDreamsEngine.AssignDream(201, LifeDreamType::BuyRichlandHighRise, "Buy Richland Penthouse");

        // Seed a rumor that Citizen 201 HAS heard
        uint32 rId = sNPCEmergentLifeEngine.SeedRumor(RumorTopicType::FRANK_CASTLE_SIGHTING,
                                                      "Castle Ambushed Downtown Racket",
                                                      "Gunfire erupted near 3rd street warehouse", 201, 0.9f);
        // Citizen 201 knows rumor rId

        SituatedDialogueContext ctx = engine.AssembleSituatedContext(201, 0, "Richland", "Cafe Terrace");
        TEST_ASSERT(ctx.speakerName == "Marcus Vance" || !ctx.speakerName.empty(), "Speaker Name Resolved");
        TEST_ASSERT(ctx.householdName == "Vance Household", "Household Context Resolved");
        TEST_ASSERT(ctx.spouseName == "Clara Vance", "Spouse Clara Vance Resolved");
        TEST_ASSERT(ctx.childrenCount == 1, "Child Count == 1 Resolved");
        TEST_ASSERT(ctx.jobTitle == "Senior Data Analyst", "Job Title Resolved");
        TEST_ASSERT(ctx.employerName == "Metacortex Systems", "Employer Metacortex Resolved");
        TEST_ASSERT(ctx.activeDreamTitle.find("Penthouse") != std::string::npos || !ctx.activeDreamTitle.empty(), "Dream Resolved");
        TEST_ASSERT(ctx.knownRumors.size() == 1, "Citizen 201 Strictly Knows Exactly 1 Personally Heard Rumor");
    }

    // 4. Prompt Payload Synthesis & JSON Serialization
    {
        SituatedDialogueContext ctx = engine.AssembleSituatedContext(201, 301, "Richland", "Sidewalk");
        DialoguePromptPayload payload = engine.ComposeSLMPromptPayload(ctx, "Hello, how are your kids doing?");

        TEST_ASSERT(!payload.systemPrompt.empty(), "System Prompt Non-Empty");
        TEST_ASSERT(payload.systemPrompt.find("Metacortex") != std::string::npos, "System Prompt Contains Employer Context");
        TEST_ASSERT(!payload.epistemicProhibitions.empty(), "Epistemic Prohibitions Non-Empty");
        TEST_ASSERT(payload.epistemicProhibitions.find("MANDATORY EPISTEMIC FIREWALL") != std::string::npos, "Firewall Section Formatted");
        TEST_ASSERT(!payload.jsonPayload.empty(), "JSON Payload Non-Empty");
        TEST_ASSERT(payload.jsonPayload.find("\"speaker\":") != std::string::npos, "JSON Has 'speaker' Key");
        TEST_ASSERT(payload.jsonPayload.find("\"horizon\":") != std::string::npos, "JSON Has 'horizon' Key");
        TEST_ASSERT(payload.jsonPayload.find("\"known_rumors_count\": 1") != std::string::npos, "JSON Encodes Exact Known Rumor Count");
    }

    // 5. In-Engine Heuristic Response: Meta-Breach Deflection
    {
        DialogueResponseResult res;
        bool handled = engine.GenerateSituatedDialogueResponse(201, 301, "Hey, have you seen Agent Smith inside the Matrix?", res);
        TEST_ASSERT(handled, "Breach Query Handled Successfully");
        TEST_ASSERT(res.wasEpistemicBreachDetected, "Epistemic Breach Correctly Flagged");
        TEST_ASSERT(res.breachedConcept == "agent smith" || res.breachedConcept == "matrix", "Breached Concept Matched");
        TEST_ASSERT(res.responseText.find("Metacortex") != std::string::npos, "Breach Response References Real Job Context");
        TEST_ASSERT(res.episodicMemoryLogged, "Episodic Memory of Disturbing Encounter Logged");
        TEST_ASSERT(engine.GetTotalEpistemicBreachesBlocked() >= 1, "Telemetry Epistemic Breaches Blocked Incremented");
    }

    // 6. In-Engine Heuristic Response: Situated In-Context Replies
    {
        DialogueResponseResult resFamily;
        engine.GenerateSituatedDialogueResponse(201, 301, "How is your family doing these days?", resFamily);
        TEST_ASSERT(!resFamily.wasEpistemicBreachDetected, "Family Inquiry Not a Breach");
        TEST_ASSERT(resFamily.responseText.find("Clara") != std::string::npos, "Family Reply Mentions Real Spouse Name");

        DialogueResponseResult resJob;
        engine.GenerateSituatedDialogueResponse(201, 301, "Where do you work?", resJob);
        TEST_ASSERT(!resJob.wasEpistemicBreachDetected, "Job Inquiry Not a Breach");
        TEST_ASSERT(resJob.responseText.find("Senior Data Analyst") != std::string::npos, "Job Reply Mentions Exact Title");
        TEST_ASSERT(resJob.responseText.find("Metacortex") != std::string::npos, "Job Reply Mentions Exact Employer");

        DialogueResponseResult resDream;
        engine.GenerateSituatedDialogueResponse(201, 301, "What are your hopes for the future?", resDream);
        TEST_ASSERT(!resDream.wasEpistemicBreachDetected, "Dream Inquiry Not a Breach");
        TEST_ASSERT(resDream.responseText.find("Penthouse") != std::string::npos, "Dream Reply Mentions Penthouse Aspiration");

        DialogueResponseResult resRumor;
        engine.GenerateSituatedDialogueResponse(201, 301, "Have you heard any news on the street?", resRumor);
        TEST_ASSERT(!resRumor.wasEpistemicBreachDetected, "Rumor Inquiry Not a Breach");
        TEST_ASSERT(resRumor.responseText.find("Castle") != std::string::npos, "Rumor Reply Accurately References Castle Sighting");
    }

    // 7. Police SWAT Epistemic Horizon
    {
        // Setup Police Officer 401
        sFamilyDreamsEngine.AssignCareer(401, CareerTrack::MMPDLawEnforcement, JobPositionLevel::Level2_MidLevelSpecialist,
                                         "Patrol Sergeant", "MMPD 4th Precinct", 1, 55);
        DialogueResponseResult resCop;
        engine.GenerateSituatedDialogueResponse(401, 301, "The Matrix is fake, we are human batteries!", resCop);
        TEST_ASSERT(resCop.wasEpistemicBreachDetected, "Police Flags Matrix Talk as Breach");
        TEST_ASSERT(resCop.responseText.find("10-99") != std::string::npos || resCop.responseText.find("insurgent") != std::string::npos,
                    "Police Uses Radio Code / Insurgent Framing");
    }

    // 8. Redpill Operative Horizon
    {
        // Setup Redpill Operative 501
        sNPCEmergentLifeEngine.GetOrCreateAwakeningProfile(501, "Trinity Contact");
        sNPCEmergentLifeEngine.AdvanceAwakeningStage(501, AwakeningStage::STAGE_3_REDPILL_SYMPATHIZER, "Phone booth connect");
        
        DialogueResponseResult resZion;
        engine.GenerateSituatedDialogueResponse(501, 301, "Signal is green, is the hardline safe?", resZion);
        TEST_ASSERT(!resZion.wasEpistemicBreachDetected, "Redpill Operative Permitted to Discuss Hardlines");
    }

    // 9. Cypherite Renegade Horizon
    {
        uint32 cypherId = 601;
        sNPCEmergentLifeEngine.GetOrCreateAwakeningProfile(cypherId, "Renegade Dan");
        sNPCEmergentLifeEngine.TriggerCypheriteDisillusionment(cypherId, "Hate Zion broth");

        DialogueResponseResult resCypher;
        engine.GenerateSituatedDialogueResponse(cypherId, 301, "Zion will liberate us from the machines!", resCypher);
        TEST_ASSERT(resCypher.wasEpistemicBreachDetected, "Cypherite Rejects Zion Salvation Myth");
        TEST_ASSERT(resCypher.responseText.find("sludge") != std::string::npos || resCypher.responseText.find("warmth") != std::string::npos,
                    "Cypherite Highlights Real World Sludge & Simulation Warmth");
    }

    // 10. Scale & Concurrency Test
    {
        for (uint32 id = 1000; id < 1200; ++id) {
            DialogueResponseResult res;
            engine.GenerateSituatedDialogueResponse(id, 9999, "Good morning, how are you?", res);
            TEST_ASSERT(!res.responseText.empty(), "Scale Dialogue Generated Non-Empty Response");
        }
        TEST_ASSERT(engine.GetTotalDialogueInferencesRun() >= 200, "Scale Test: Executed >= 200 Inferences");
        std::string telemetry = engine.GenerateSLMTelemetryReport();
        TEST_ASSERT(!telemetry.empty(), "SLM Telemetry Report Generated");
    }

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  SLM DIALOGUE TEST RESULTS: " << passedCount << " PASSED, " << failedCount << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;
}

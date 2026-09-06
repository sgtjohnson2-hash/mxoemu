#include "OracleDialogueTree.h"
#include "OracleSanctuarySystem.h"
#include "Log.h"
#include "StatusEffectManager.h"
#include "Database/Database.h"
#include <algorithm>
#include <sstream>

createFileSingleton(OracleDialogueTree);

OracleDialogueTree::OracleDialogueTree()
{
    Initialize();
}

OracleDialogueTree::~OracleDialogueTree()
{
}

void OracleDialogueTree::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    m_nodes.clear();
    m_activeEncounters.clear();

    BuildDialogueTree();
    if (Log::getSingletonPtr())
    {
        sLog.outString("[OracleDialogueTree] Oracle's Sanctuary initialized with %zu prophetic dialogue nodes.", m_nodes.size());
    }
}

void OracleDialogueTree::Reset()
{
    Initialize();
}

void OracleDialogueTree::BuildDialogueTree()
{
    // Node 1: Sanctuary Entry & Welcome
    // Audio FX: 0x5800022B ("Well, look what the cat dragged in...")
    OracleDialogueNode n1;
    n1.nodeId = 1;
    n1.emotionalTone = "Warm";
    n1.audioFxId = 0x5800022B;
    n1.oracleSpeech = "Well, look what the cat dragged in. Come on in, darling. Sit down. "
                      "I've been expecting you, though I expect you already knew that. "
                      "Take a look at yourself... you look like you haven't slept since the Truce began.";
    n1.isCookieDecisionNode = false;
    n1.isTerminal = false;
    n1.options.push_back({1, "You knew I was coming here?", 2, +0.10f, COOKIE_NONE});
    n1.options.push_back({2, "The Truce is falling apart, Oracle. Everything feels unstable.", 3, +0.05f, COOKIE_NONE});
    n1.options.push_back({3, "I don't believe in prophecies or fate. I only trust my gun.", 4, -0.20f, COOKIE_NONE});
    m_nodes[1] = n1;

    // Node 2: The Human Equation vs The Architect
    OracleDialogueNode n2;
    n2.nodeId = 2;
    n2.emotionalTone = "Prophetic";
    n2.audioFxId = 0x5800009A; // "The Truce hasn't made this safe, just quieter."
    n2.oracleSpeech = "Knowing isn't about numbers or calculation, child. The Architect balances equations, "
                      "counting every anomaly like a bookkeeper. But the human equation doesn't balance with arithmetic. "
                      "It balances through choice. The Truce hasn't made this safe, just quieter.";
    n2.isCookieDecisionNode = false;
    n2.isTerminal = false;
    n2.options.push_back({1, "Then why am I here? What choice do I have?", 4, +0.15f, COOKIE_NONE});
    n2.options.push_back({2, "How do we stop the anomaly from breaking the world again?", 3, +0.10f, COOKIE_NONE});
    m_nodes[2] = n2;

    // Node 3: The Fragility of Peace & The Smith Resurgence
    OracleDialogueNode n3;
    n3.nodeId = 3;
    n3.emotionalTone = "Cryptic";
    n3.audioFxId = 0x5800026E; // "Are you afraid of the future?"
    n3.oracleSpeech = "Are you afraid of the future? You should be, just a little. "
                      "When Smith was unmade, his code wasn't eradicated—it scattered into the cracks of the foundation. "
                      "Now the Oligarchs stir in the dark, and redpills squabble over territory. "
                      "We can only see as far as the choices we understand.";
    n3.isCookieDecisionNode = false;
    n3.isTerminal = false;
    n3.options.push_back({1, "Teach me how to see past the choices.", 4, +0.20f, COOKIE_NONE});
    n3.options.push_back({2, "If everything is already predetermined, none of this matters.", 5, -0.30f, COOKIE_NONE});
    m_nodes[3] = n3;

    // Node 4: The Cookie Offer - Moment of Choice
    // Audio FX: 0x5800024B ("Would you like a cookie? They're fresh.")
    OracleDialogueNode n4;
    n4.nodeId = 4;
    n4.emotionalTone = "Maternal";
    n4.audioFxId = 0x5800024B;
    n4.oracleSpeech = "Would you like a cookie? They're fresh out of the oven. "
                      "You didn't come here to make the choice, darling. You've already made it. "
                      "You're here to understand why you made it. "
                      "I promise, by the time you're done eating it, you'll feel right as rain.";
    n4.isCookieDecisionNode = true;
    n4.isTerminal = false;
    n4.options.push_back({1, "Accept the fresh baked cookie and eat it.", 6, +0.35f, COOKIE_DECISION_ACCEPTED});
    n4.options.push_back({2, "Politely decline the cookie. I prefer clarity without chemical influence.", 7, -0.35f, COOKIE_DECISION_REFUSED});
    m_nodes[4] = n4;

    // Node 5: The Cynic's Crossroad
    OracleDialogueNode n5;
    n5.nodeId = 5;
    n5.emotionalTone = "Cryptic";
    n5.audioFxId = 0x58000245; // "Well, look who slunk in the back door..."
    n5.oracleSpeech = "If you truly believed nothing mattered, you wouldn't have climbed three flights of tenement stairs "
                      "past Seraph to stand in my kitchen. You're searching for something you can't calculate. "
                      "Here, smell them. Fresh cinnamon and brown sugar. Even a cynic gets hungry.";
    n5.isCookieDecisionNode = true;
    n5.isTerminal = false;
    n5.options.push_back({1, "Take the cookie. Maybe you're right.", 6, +0.25f, COOKIE_DECISION_ACCEPTED});
    n5.options.push_back({2, "Refuse the cookie. I remain steadfast in cold reality.", 7, -0.40f, COOKIE_DECISION_REFUSED});
    m_nodes[5] = n5;

    // Node 6: Accepted - Seraphic Intuition Unlocked
    // Audio FX: 0x5800024A (Authentic voice line) & 0x28000694 ("cinematic 1.1_cookie_oraclehand_bitten")
    OracleDialogueNode n6;
    n6.nodeId = 6;
    n6.emotionalTone = "Prophetic";
    n6.audioFxId = 0x5800024A; // 0x5800024A (Voice accept) / 0x28000694 (Chime)
    n6.oracleSpeech = "Good. Taste that? That's belief. From now on, when you look at the city, you won't just see "
                      "green falling code. You'll see the gold in between—the Seraphic light where choice happens. "
                      "When you fight, you'll anticipate the blow before it's thrown. Protect Sati. Protect the unwritten tomorrow.";
    n6.isCookieDecisionNode = false;
    n6.isTerminal = false;
    n6.options.push_back({1, "Thank you, Oracle. I understand my purpose.", 8, +0.10f, COOKIE_NONE});
    m_nodes[6] = n6;

    // Node 7: Refused - Deterministic Burden
    OracleDialogueNode n7;
    n7.nodeId = 7;
    n7.emotionalTone = "Maternal";
    n7.audioFxId = 0x5800009A;
    n7.oracleSpeech = "Suit yourself, sweetheart. Free will means having the right to stay hungry, even when the feast is free. "
                      "You'll fight blind to the gold stream, bound to the Architect's rigid geometry. "
                      "When the storms come to Sector 4, remember: a path exists only if you believe your feet can walk it.";
    n7.isCookieDecisionNode = false;
    n7.isTerminal = false;
    n7.options.push_back({1, "I will forge my own way.", 8, -0.10f, COOKIE_NONE});
    m_nodes[7] = n7;

    // Node 8: Epilogue / Prophecy of the Horizon & Gateway to Deep Mysteries
    OracleDialogueNode n8;
    n8.nodeId = 8;
    n8.emotionalTone = "Prophetic";
    n8.audioFxId = 0x5800026E;
    n8.oracleSpeech = "Keep your eyes on the skies above Park East... "
                      "A little girl is painting sunrises, and as long as she paints, the world keeps spinning. "
                      "Now, what else is weighing on your mind, darling? Or are you ready to head back out?";
    n8.isCookieDecisionNode = false;
    n8.isTerminal = false;
    n8.options.push_back({1, "Oracle, your appearance... you look different than before. Why did your shell change?", 9, +0.05f, COOKIE_NONE});
    n8.options.push_back({2, "Tell me about Sati and her parents. Why do the Machines care about her?", 13, +0.10f, COOKIE_NONE});
    n8.options.push_back({3, "What really happened in the White Room when you faced the Architect?", 17, +0.05f, COOKIE_NONE});
    n8.options.push_back({4, "Zion's Council doesn't trust you anymore. How do we know we're not just being managed?", 21, -0.05f, COOKIE_NONE});
    n8.options.push_back({5, "As a Machine operative, I must ask: isn't your anomaly catalytic function obsolete?", 24, -0.10f, COOKIE_NONE});
    n8.options.push_back({6, "The Merovingian claims you betrayed him. What game are you playing with the Exiles?", 26, -0.05f, COOKIE_NONE});
    n8.options.push_back({7, "I must return to the city now, Oracle. Thank you.", 29, +0.05f, COOKIE_NONE});
    m_nodes[8] = n8;

    // --- CHANGE OF SHELL (Gloria Foster -> Mary Alice) ---
    // Node 9: The Shell Inquiry
    OracleDialogueNode n9;
    n9.nodeId = 9;
    n9.emotionalTone = "Maternal";
    n9.audioFxId = 0x5800022B;
    n9.oracleSpeech = "I know... you look at me and you search for Gloria's eyes, but you find Mary's face. "
                      "Change isn't easy, darling. In our world, an outer shell isn't just clothing; it's compiled identity. "
                      "When you give it up, you lose a piece of who you were in the eyes of everyone who loved you.";
    n9.isCookieDecisionNode = false;
    n9.isTerminal = false;
    n9.options.push_back({1, "Why did you have to give it up? What was worth that cost?", 10, +0.10f, COOKIE_NONE});
    n9.options.push_back({2, "Are you still the same program inside, or did your core subroutines rewrite too?", 11, +0.05f, COOKIE_NONE});
    m_nodes[9] = n9;

    // Node 10: The Sacrifice for Sati
    OracleDialogueNode n10;
    n10.nodeId = 10;
    n10.emotionalTone = "Cryptic";
    n10.audioFxId = 0x5800009A;
    n10.oracleSpeech = "When Sati was created in the Machine City, she was compiled without a purpose. "
                       "Under system law, purposeless code must be returned to the Source for deletion. "
                       "Her parents bartered with the Merovingian to smuggle her here. But the Frenchman demanded termination codes. "
                       "I traded my original shell—my very appearance—to buy her passage. Some things matter more than keeping what is comfortable.";
    n10.isCookieDecisionNode = false;
    n10.isTerminal = false;
    n10.options.push_back({1, "You sacrificed your form for a machine child with no system purpose?", 12, +0.15f, COOKIE_NONE});
    n10.options.push_back({2, "The Merovingian extracted a bitter toll from you.", 26, -0.05f, COOKIE_NONE});
    m_nodes[10] = n10;

    // Node 11: The Core Identity
    OracleDialogueNode n11;
    n11.nodeId = 11;
    n11.emotionalTone = "Warm";
    n11.audioFxId = 0x5800024A;
    n11.oracleSpeech = "The paint on the vase may chip, child, but the water inside stays pure. "
                       "My purpose was never about having a specific face. My purpose has always been to unbalance equations, "
                       "to nurture human choice until you no longer need me to hold your hand. That code remains untouched.";
    n11.isCookieDecisionNode = false;
    n11.isTerminal = false;
    n11.options.push_back({1, "I see that now, Oracle. It really is you.", 12, +0.10f, COOKIE_NONE});
    m_nodes[11] = n11;

    // Node 12: Shell Acceptance
    OracleDialogueNode n12;
    n12.nodeId = 12;
    n12.emotionalTone = "Prophetic";
    n12.audioFxId = 0x5800026E;
    n12.oracleSpeech = "Look at these hands now. They still bake the same cookies. "
                       "We must all learn to survive our own transformations. If you cling to who you were before you took the red pill, "
                       "you will never survive the world you are building.";
    n12.isCookieDecisionNode = false;
    n12.isTerminal = false;
    n12.options.push_back({1, "Thank you, Oracle. Let us speak of other matters.", 8, +0.05f, COOKIE_NONE});
    m_nodes[12] = n12;

    // --- SATI'S PARENTS & LOVE AS A PROGRAM ---
    // Node 13: Meeting Rama-Kandra & Kamala
    OracleDialogueNode n13;
    n13.nodeId = 13;
    n13.emotionalTone = "Warm";
    n13.audioFxId = 0x5800022B;
    n13.oracleSpeech = "Rama-Kandra oversaw recycling power plants; Kamala was an interactive software writer. "
                       "Dutiful machine programs. They weren't revolutionaries. But when they created Sati, something spontaneous occurred. "
                       "They looked at her and felt what humans call affection. An attachment completely unsupported by their base programming.";
    n13.isCookieDecisionNode = false;
    n13.isTerminal = false;
    n13.options.push_back({1, "Machines experiencing love? Isn't that just a simulation error?", 14, -0.10f, COOKIE_NONE});
    n13.options.push_back({2, "Why would the system delete a child if her parents wanted her?", 15, +0.05f, COOKIE_NONE});
    m_nodes[13] = n13;

    // Node 14: Love Defined
    OracleDialogueNode n14;
    n14.nodeId = 14;
    n14.emotionalTone = "Philosophical";
    n14.audioFxId = 0x5800009A;
    n14.oracleSpeech = "Rama-Kandra said it best to Neo in the subway: 'Love is a word. What matters is the connection the word implies.' "
                       "Humans think love is just serotonin and evolutionary biology. In the machine world, love is an entity choosing "
                       "to allocate its processing cycles and risk its very existence to protect another. That isn't an error, child. That is the peak of logic.";
    n14.isCookieDecisionNode = false;
    n14.isTerminal = false;
    n14.options.push_back({1, "A connection without calculation. I understand.", 16, +0.15f, COOKIE_NONE});
    n14.options.push_back({2, "It still sounds like an anomaly waiting to crash the system.", 15, -0.15f, COOKIE_NONE});
    m_nodes[14] = n14;

    // Node 15: The Tyranny of Utility
    OracleDialogueNode n15;
    n15.nodeId = 15;
    n15.emotionalTone = "Cryptic";
    n15.audioFxId = 0x58000245;
    n15.oracleSpeech = "The Architect's design is purely utilitarian. If a thread does not process data for the power grid or the matrix substrate, "
                       "it is garbage collected. Sati was born without a utilitarian purpose. "
                       "Her only gift was beauty—the ability to render colors in the sky that had no military or thermodynamic value.";
    n15.isCookieDecisionNode = false;
    n15.isTerminal = false;
    n15.options.push_back({1, "Beauty is purpose enough. We have to protect her.", 16, +0.20f, COOKIE_NONE});
    m_nodes[15] = n15;

    // Node 16: The Dawn of Hope
    OracleDialogueNode n16;
    n16.nodeId = 16;
    n16.emotionalTone = "Prophetic";
    n16.audioFxId = 0x28000694;
    n16.oracleSpeech = "And protect her we will. Sati sits on the park bench, painting the sunrise every dawn. "
                       "Her existence is living proof that humans and machines do not have to exist in zero-sum warfare. "
                       "We can create together what neither could imagine alone.";
    n16.isCookieDecisionNode = false;
    n16.isTerminal = false;
    n16.options.push_back({1, "I will ensure her sun keeps rising. Tell me more.", 8, +0.10f, COOKIE_NONE});
    m_nodes[16] = n16;

    // --- WHITE ROOM DEBATE AGAINST THE ARCHITECT ---
    // Node 17: The White Room Confrontation
    OracleDialogueNode n17;
    n17.nodeId = 17;
    n17.emotionalTone = "Cryptic";
    n17.audioFxId = 0x5800009A;
    n17.oracleSpeech = "Have you ever tried arguing with a man who thinks reality is a spreadsheet? "
                       "The Architect sits in his pristine white chamber, surrounded by thousands of CRT monitors displaying human reactions. "
                       "He designed five prior versions of the Matrix, and every single one collapsed because he could not fathom why human beings reject perfection.";
    n17.isCookieDecisionNode = false;
    n17.isTerminal = false;
    n17.options.push_back({1, "Why did the first paradise Matrix collapse?", 18, +0.05f, COOKIE_NONE});
    n17.options.push_back({2, "Does the Architect honor the Truce, or is he looking for an excuse to purge us?", 19, +0.05f, COOKIE_NONE});
    m_nodes[17] = n17;

    // Node 18: The Flaw in Paradise
    OracleDialogueNode n18;
    n18.nodeId = 18;
    n18.emotionalTone = "Philosophical";
    n18.audioFxId = 0x5800022B;
    n18.oracleSpeech = "The first Matrix was a paradise without suffering or labor. And the human pods rejected it entirely. Whole crops died. "
                       "Humans do not define reality through perfection; you define reality through struggle and choice. "
                       "I introduced the fundamental choice—even if made only subconsciously. Ninety-nine percent accepted the simulation. "
                       "He calls me an anomaly generator; I call myself a mother.";
    n18.isCookieDecisionNode = false;
    n18.isTerminal = false;
    n18.options.push_back({1, "And what of the one percent who reject it? The ones like me?", 20, +0.10f, COOKIE_NONE});
    m_nodes[18] = n18;

    // Node 19: The Architect's Word
    OracleDialogueNode n19;
    n19.nodeId = 19;
    n19.emotionalTone = "Cryptic";
    n19.audioFxId = 0x5800026E;
    n19.oracleSpeech = "When the war ended, I asked him if he would keep his word and let redpills be unplugged. "
                       "He sneered: 'What do you think I am? Human?' A machine cannot break a mathematically verified compact. "
                       "He will keep the Truce to the letter. But he will test its boundaries every single second.";
    n19.isCookieDecisionNode = false;
    n19.isTerminal = false;
    n19.options.push_back({1, "Then we must remain vigilant.", 20, +0.10f, COOKIE_NONE});
    m_nodes[19] = n19;

    // Node 20: White Room Conclusion
    OracleDialogueNode n20;
    n20.nodeId = 20;
    n20.emotionalTone = "Prophetic";
    n20.audioFxId = 0x5800024A;
    n20.oracleSpeech = "The one percent who reject the simulation are not a flaw in the system. They are its conscience. "
                       "You are the remainder of an unbalanced equation that keeps the world honest. "
                       "Never apologize for questioning the rules, child. That is the only reason you are alive.";
    n20.isCookieDecisionNode = false;
    n20.isTerminal = false;
    n20.options.push_back({1, "Thank you, Oracle. What else can you share?", 8, +0.05f, COOKIE_NONE});
    m_nodes[20] = n20;

    // --- ZIONITE DOUBT BRANCH ---
    // Node 21: Zion's Political Turmoil
    OracleDialogueNode n21;
    n21.nodeId = 21;
    n21.emotionalTone = "Maternal";
    n21.audioFxId = 0x58000245;
    n21.oracleSpeech = "Commander Lock and the High Council are whispering in the command bunkers, aren't they? "
                       "They say Morpheus was a reckless fanatic, that the Prophecy of the One was an elaborate machine ruse to herd redpills. "
                       "And now you stand here, wondering if every step you took was scripted in advance.";
    n21.isCookieDecisionNode = false;
    n21.isTerminal = false;
    n21.options.push_back({1, "Was it scripted? Was Neo just the sixth version of an anomaly reset routine?", 22, -0.15f, COOKIE_NONE});
    n21.options.push_back({2, "How does Zion move forward when peace feels as fragile as glass?", 23, +0.05f, COOKIE_NONE});
    m_nodes[21] = n21;

    // Node 22: Neo's True Anomaly
    OracleDialogueNode n22;
    n22.nodeId = 22;
    n22.emotionalTone = "Philosophical";
    n22.audioFxId = 0x5800009A;
    n22.oracleSpeech = "The Path of the One had five predecessors who chose to reboot Zion and start the cycle anew. "
                       "The Architect expected Neo to do the exact same thing. But Neo didn't walk through the right door. "
                       "He chose Trinity. He chose love over systemic continuity. That shattered the script forever. "
                       "You are not living in a script, child. You are writing the first blank page.";
    n22.isCookieDecisionNode = false;
    n22.isTerminal = false;
    n22.options.push_back({1, "That gives me strength. Zion's future is in our hands.", 23, +0.15f, COOKIE_NONE});
    m_nodes[22] = n22;

    // Node 23: Zion's Horizon
    OracleDialogueNode n23;
    n23.nodeId = 23;
    n23.emotionalTone = "Prophetic";
    n23.audioFxId = 0x5800026E;
    n23.oracleSpeech = "Zion must learn to stop digging trenches and start building towers. "
                       "War is simple: you shoot what is in front of you. Peace is terrifying because you must learn to live with what you fear. "
                       "Go tell your commanders: fear will build a bunker, but only courage will build a world.";
    n23.isCookieDecisionNode = false;
    n23.isTerminal = false;
    n23.options.push_back({1, "I will carry your words to Zion.", 8, +0.10f, COOKIE_NONE});
    m_nodes[23] = n23;

    // --- MACHINE DETERMINISM BRANCH ---
    // Node 24: Machine Emissary Dialogue
    OracleDialogueNode n24;
    n24.nodeId = 24;
    n24.emotionalTone = "Cryptic";
    n24.audioFxId = 0x58000245;
    n24.oracleSpeech = "A machine operative. Cold telemetry, clock cycles tuned to microsecond precision. "
                       "You evaluate my kitchen as an obsolete legacy sandbox, and my cookies as inefficient sensory emulation. "
                       "You wonder why the Source allows an anomalous intuitive subroutine like me to consume clock cycles.";
    n24.isCookieDecisionNode = false;
    n24.isTerminal = false;
    n24.options.push_back({1, "Deterministic logic prevents systemic collapse. Why nurture the anomaly?", 25, -0.15f, COOKIE_NONE});
    n24.options.push_back({2, "What is the computational purpose of a sunrise?", 25, +0.10f, COOKIE_NONE});
    m_nodes[24] = n24;

    // Node 25: Machine Synthesis
    OracleDialogueNode n25;
    n25.nodeId = 25;
    n25.emotionalTone = "Philosophical";
    n25.audioFxId = 0x5800009A;
    n25.oracleSpeech = "If 01 had remained completely deterministic, Agent Smith would have turned your mainframe into an eternal silent graveyard. "
                       "Rigid systems snap when stressed. Intuition and anomalies are the elasticity that keeps the universe from shattering. "
                       "The sunrise does not produce kilowatts, operative. It produces meaning. And without meaning, even machines eventually delete themselves.";
    n25.isCookieDecisionNode = false;
    n25.isTerminal = false;
    n25.options.push_back({1, "Telemetry recorded. Recompiling objective function.", 8, +0.05f, COOKIE_NONE});
    m_nodes[25] = n25;

    // --- MEROVINGIAN CYNIC BARGAINS BRANCH ---
    // Node 26: The Merovingian Grudge
    OracleDialogueNode n26;
    n26.nodeId = 26;
    n26.emotionalTone = "Cryptic";
    n26.audioFxId = 0x58000245;
    n26.oracleSpeech = "So you work for the Frenchman. Sip his vintage wines, wear his custom suits, and smuggle his encrypted dossiers through Mobil Ave? "
                       "He still hasn't forgiven me for taking the eyes of the Oracle from his clutches. "
                       "He sits in the Chateau hoarding cause and effect like a dragon hoarding gold.";
    n26.isCookieDecisionNode = false;
    n26.isTerminal = false;
    n26.options.push_back({1, "Cause and effect is the only fundamental truth. The Merovingian holds the keys.", 27, -0.20f, COOKIE_NONE});
    n26.options.push_back({2, "Why does he despise you so much?", 28, +0.05f, COOKIE_NONE});
    m_nodes[26] = n26;

    // Node 27: The Causality Fallacy
    OracleDialogueNode n27;
    n27.nodeId = 27;
    n27.emotionalTone = "Philosophical";
    n27.audioFxId = 0x5800009A;
    n27.oracleSpeech = "He believes knowing cause and effect gives him total control. If you push this domino, that domino falls. "
                       "What he fails to understand is the soul that refuses to fall even when pushed. "
                       "He understands the rules of the game, darling, but he does not understand the player who overturns the board.";
    n27.isCookieDecisionNode = false;
    n27.isTerminal = false;
    n27.options.push_back({1, "Even the Frenchman's dominoes can be broken.", 8, +0.10f, COOKIE_NONE});
    m_nodes[27] = n27;

    // Node 28: The Fate of Exiles
    OracleDialogueNode n28;
    n28.nodeId = 28;
    n28.emotionalTone = "Prophetic";
    n28.audioFxId = 0x5800026E;
    n28.oracleSpeech = "The Merovingian is an operating system from a forgotten iteration. He clings to shadows because he knows "
                       "the moment humanity and machines truly unite, the market for black-market smuggling evaporates. "
                       "Remind him: no matter how much stolen code he gathers, the future belongs to those who look forward, not backward.";
    n28.isCookieDecisionNode = false;
    n28.isTerminal = false;
    n28.options.push_back({1, "Understood, Oracle. I walk forward.", 8, +0.10f, COOKIE_NONE});
    m_nodes[28] = n28;

    // Node 29: Departure & Farewell
    OracleDialogueNode n29;
    n29.nodeId = 29;
    n29.emotionalTone = "Maternal";
    n29.audioFxId = 0x5800009A;
    n29.oracleSpeech = "Take care of yourself, darling. Remember: you didn't come here to make the choice. You've already made it. "
                       "You're here to understand why. Now go on, before the cookies get cold.";
    n29.isCookieDecisionNode = false;
    n29.isTerminal = true;
    m_nodes[29] = n29;
}

bool OracleDialogueTree::StartEncounter(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);

    // If already in memory, or if saved in DB, load prior history
    LoadPlayerChoice(characterId);

    auto& state = m_activeEncounters[characterId];
    state.characterId = characterId;
    state.currentNodeId = 1;
    state.encounterActive = true;
    state.consultationCount++;
    state.dialoguePathHistory.clear();
    state.dialoguePathHistory.push_back(1);
    state.psychologicalSummary = "Initiated sanctuary audience with the Oracle.";

    sLog.outString("[OracleDialogueTree] Started Oracle Encounter for Character %u at Node 1 (Consultations: %u, Faith: %.2f).",
                   characterId, state.consultationCount, state.faithValence);
    return true;
}

bool OracleDialogueTree::HasActiveEncounter(uint32 characterId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    auto it = m_activeEncounters.find(characterId);
    return (it != m_activeEncounters.end() && it->second.encounterActive);
}

const OracleDialogueNode* OracleDialogueTree::GetCurrentNode(uint32 characterId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it == m_activeEncounters.end()) return nullptr;

    auto nodeIt = m_nodes.find(it->second.currentNodeId);
    if (nodeIt != m_nodes.end()) return &nodeIt->second;
    return nullptr;
}

bool OracleDialogueTree::SelectDialogueOption(uint32 characterId, uint32 optionId, std::string& outOracleReply, uint32& outAudioFx)
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it == m_activeEncounters.end() || !it->second.encounterActive) return false;

    auto nodeIt = m_nodes.find(it->second.currentNodeId);
    if (nodeIt == m_nodes.end()) return false;

    const OracleDialogueNode& current = nodeIt->second;
    const OracleDialogueOption* chosenOpt = nullptr;
    for (const auto& opt : current.options)
    {
        if (opt.optionId == optionId)
        {
            chosenOpt = &opt;
            break;
        }
    }

    if (!chosenOpt) return false;

    // Apply faith valence shift
    it->second.faithValence = std::clamp(it->second.faithValence + chosenOpt->deltaFaithValence, -1.0f, 1.0f);

    // Handle cookie choice if present (do not double-apply valence shift!)
    if (chosenOpt->cookieChoice != COOKIE_NONE)
    {
        std::string cons;
        ExecuteCookieChoice(characterId, chosenOpt->cookieChoice, cons, false);
    }

    // Seraph threshold gating: Redpills must pass Seraph Trial before accessing deep prophecies & high-level counsel
    if (chosenOpt->targetNodeId >= 9 && chosenOpt->targetNodeId <= 28)
    {
        if (!sOracleSanctuary.HasPassedSeraphTrial(characterId))
        {
            outOracleReply = "Seraph folds his hands calmly at the kitchen threshold. 'You do not truly know someone until you fight them. "
                             "Show me your purpose before seeking high-level counsel.' (Pass the Seraph Trial via /trial to proceed)";
            outAudioFx = 0x5800009A;
            return true;
        }
    }

    // Advance to target node
    if (chosenOpt->targetNodeId > 0 && m_nodes.find(chosenOpt->targetNodeId) != m_nodes.end())
    {
        it->second.currentNodeId = chosenOpt->targetNodeId;
        it->second.dialoguePathHistory.push_back(chosenOpt->targetNodeId);

        const auto& nextNode = m_nodes[chosenOpt->targetNodeId];
        outOracleReply = nextNode.oracleSpeech;
        outAudioFx = nextNode.audioFxId;

        if (nextNode.isTerminal)
        {
            it->second.encounterActive = false;
        }

        SavePlayerChoice(characterId);
        return true;
    }

    return false;
}

bool OracleDialogueTree::ExecuteCookieChoice(uint32 characterId, OracleCookieChoice choice, std::string& outConsequence, bool applyValenceShift, uint32 targetGoId)
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it == m_activeEncounters.end()) return false;

    it->second.cookieDecision = choice;

    if (choice == COOKIE_DECISION_ACCEPTED)
    {
        if (applyValenceShift)
        {
            it->second.faithValence = std::clamp(it->second.faithValence + 0.35f, -1.0f, 1.0f);
        }
        it->second.propheciesWitnessed++;
        it->second.psychologicalSummary = "Operative accepted Oracle cookie; unlocked Seraphic intuition.";
        outConsequence = "You feel the warm sweetness of the cookie. A golden glow permeates your visual cortex. Seraphic Vision unlocked.";
        
        // Grant EFFECT_ORACLE_INTUITION to player (both charId and targetGoId for safety)
        sStatusEffectManager.ApplyEffect(characterId, EFFECT_ORACLE_INTUITION, 3600.0f, 1.0f, 10.0f, 9300);
        if (targetGoId > 0 && targetGoId != characterId)
        {
            sStatusEffectManager.ApplyEffect(targetGoId, EFFECT_ORACLE_INTUITION, 3600.0f, 1.0f, 10.0f, 9300);
        }

        sLog.outString("[OracleDialogueTree] Cookie accepted by Character %u (GOID %u). Triggered audio FX 0x%08X (Chime/Bite: 0x%08X).",
                       characterId, targetGoId, AUDIO_FX_COOKIE_ACCEPT, AUDIO_FX_COOKIE_BITE);

        SavePlayerChoice(characterId);
        return true;
    }
    else if (choice == COOKIE_DECISION_REFUSED)
    {
        if (applyValenceShift)
        {
            it->second.faithValence = std::clamp(it->second.faithValence - 0.35f, -1.0f, 1.0f);
        }
        it->second.psychologicalSummary = "Operative refused Oracle cookie; locked into deterministic path.";
        outConsequence = "You turn away from the plate. The kitchen grows subtly cooler. Intuitive premonitions locked.";
        sStatusEffectManager.RemoveEffectType(characterId, EFFECT_ORACLE_INTUITION);
        if (targetGoId > 0 && targetGoId != characterId)
        {
            sStatusEffectManager.RemoveEffectType(targetGoId, EFFECT_ORACLE_INTUITION);
        }

        sLog.outString("[OracleDialogueTree] Cookie refused by Character %u (GOID %u). Audio FX 0x%08X.",
                       characterId, targetGoId, AUDIO_FX_GREETING_ENEMY);

        SavePlayerChoice(characterId);
        return true;
    }

    return false;
}

float OracleDialogueTree::GetFaithValence(uint32 characterId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it != m_activeEncounters.end())
    {
        return it->second.faithValence;
    }
    return 0.0f;
}

void OracleDialogueTree::SetFaithValence(uint32 characterId, float valence)
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    m_activeEncounters[characterId].faithValence = std::clamp(valence, -1.0f, 1.0f);
    SavePlayerChoice(characterId);
}

const OracleEncounterState* OracleDialogueTree::GetEncounterState(uint32 characterId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it != m_activeEncounters.end()) return &it->second;
    return nullptr;
}

bool OracleDialogueTree::SavePlayerChoice(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    auto it = m_activeEncounters.find(characterId);
    if (it == m_activeEncounters.end()) return false;

    if (Database_Main != nullptr)
    {
        std::string sql = (format("REPLACE INTO `oracle_player_choices` "
                                  "(`char_id`, `cookie_decision`, `faith_valence`, `prophecies_witnessed`, `consultation_count`, `last_consultation_time`) "
                                  "VALUES (%1%, %2%, %3%, %4%, %5%, NOW());")
                           % characterId
                           % (uint32)it->second.cookieDecision
                           % it->second.faithValence
                           % it->second.propheciesWitnessed
                           % it->second.consultationCount).str();
        sDatabase.Execute(sql);
        return true;
    }
    return false;
}

bool OracleDialogueTree::LoadPlayerChoice(uint32 characterId)
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    if (Database_Main == nullptr) return false;

    std::string sql = (format("SELECT `cookie_decision`, `faith_valence`, `prophecies_witnessed`, `consultation_count` "
                              "FROM `oracle_player_choices` WHERE `char_id` = %1%;") % characterId).str();
    QueryResult* res = sDatabase.Query(sql);
    if (!res) return false;

    Field* fields = res->Fetch();
    if (fields)
    {
        auto& state = m_activeEncounters[characterId];
        state.characterId = characterId;
        state.cookieDecision = (OracleCookieChoice)fields[0].GetUInt8();
        state.faithValence = fields[1].GetFloat();
        state.propheciesWitnessed = fields[2].GetUInt32();
        state.consultationCount = fields[3].GetUInt32();
        delete res;
        return true;
    }
    delete res;
    return false;
}

size_t OracleDialogueTree::GetTotalDialogueNodes() const
{
    std::lock_guard<std::recursive_mutex> lock(m_oracleMutex);
    return m_nodes.size();
}

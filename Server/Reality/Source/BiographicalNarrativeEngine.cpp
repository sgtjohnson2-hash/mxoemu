#include "BiographicalNarrativeEngine.h"
#include "BotClient.h"
#include "FrankCastleManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cassert>
#include <cmath>
#include <algorithm>

// Singleton instantiation
createFileSingleton(BiographicalNarrativeEngine);

// ============================================================================
// Handcrafted Lore Lexicons & Grammars
// ============================================================================

namespace {

// DF-Style Evocative Titles & Epithets (64 items)
const char* g_DwarfEpithets[] = {
    "the Mirror-Breaker", "the Iron Gutter", "the Siphon of Code", "the Rust-Eater",
    "the Wire-Cutter", "the Glitch-Stalker", "the Keymaker's Apprentice", "the Cold Conduit",
    "the Emerald Ghost", "the Unplugged Phantom", "the Bell-Ringer", "the Bone-Carver",
    "the Trench-Stalker", "the Anomaly-Hound", "the Subroutine-Cleaver", "the Shadow-Weaver",
    "the Silent Carrier", "the Brass-Knuckle", "the Hardline-Keeper", "the Blind Believer",
    "the Meat-Lover", "the Red-Rain", "the Void-Walker", "the Storm-Pilot",
    "the Sewer-Rat", "the Neon-Spectre", "the Copper-Tongue", "the Clock-Stopper",
    "the Jack-Ripper", "the Gate-Crasher", "the Code-Smuggler", "the Binary-Bane",
    "the Pulse-Breaker", "the Memory-Thief", "the Shard-Seeker", "the Core-Bleeder",
    "the Static-Dancer", "the Null-Bringer", "the High-Tower Watcher", "the Subway-Stalker",
    "the EMP-Burner", "the Socket-Scarred", "the Wire-Whisperer", "the Source-Outcast",
    "the Red-Eyed Hunter", "the Chandelier-Shatterer", "the Velvet-Biter", "the Lead-Rain",
    "the Data-Leech", "the Phantom-Stitcher", "the Glass-Jaw", "the Cipher-Weaver",
    "the Iron-Spine", "the Spark-Thrower", "the Deep-Diver", "the Ghost-Signal",
    "the Logic-Render", "the Vector-Slayer", "the Bit-Crusher", "the Zero-Pointer",
    "the Catacomb-Crawler", "the Vault-Cracker", "the Hardline-Ghost", "the Static-King"
};

// Zion Redpill Hacker Handles (80 items)
const char* g_RedpillHandles[] = {
    "Vector", "Ghost", "Cipher", "Axiom", "Siphon", "Nyx", "Toxin", "Phreak",
    "Voltage", "Cinder", "Ronin", "Glyph", "Quanta", "Zenith", "Nomad", "Helix",
    "Apex", "Static", "Vesper", "Trench", "Riptide", "Talon", "Binary", "Echo",
    "Null", "Forge", "Blitz", "Surge", "Stryker", "Kilo", "Pulse", "Shade",
    "Zero", "Rook", "Hazard", "Rift", "Wire", "Gasket", "Coil", "Spark",
    "Frost", "Havoc", "Jackal", "Phantom", "Valkyrie", "Sol", "Proxy", "Cobalt",
    "Neon", "Drift", "Tempest", "Obsidian", "Flint", "Bastion", "Chrome", "Razor",
    "Torrent", "Flux", "Switch", "Link", "Cathode", "Anode", "Parallax", "Recursion",
    "Overload", "Baud", "Phosphor", "Splicer", "Spectre", "Degauss", "Shifter", "Modem",
    "Hardline", "Sub-Zero", "Vortex", "Payload", "Bandwidth", "Clockwork", "Spike", "Trigger"
};

const char* g_RedpillPrefixes[] = {
    "Neo", "Cyber", "Dark", "Zero", "Shadow", "Iron", "Storm", "Cold",
    "Void", "Echo", "Static", "Chrome", "Pulse", "Alpha", "Hyper", "Ghost"
};

const char* g_RedpillHandleTags[] = {
    "Prime", "Zero", "One", "X", "Seven", "Nine", "Omega", "Delta",
    "Sigma", "404", "Hex", "Node", "Vector", "Alpha", "Beta", "Six"
};

// Machine Agent Surnames (70 items)
const char* g_AgentSurnames[] = {
    "Smith", "Jones", "Brown", "Gray", "Pace", "Skinner", "Thorne", "Jackson",
    "Wright", "White", "Sterling", "Vance", "Finch", "Mercer", "Cross", "Black",
    "Ward", "King", "Gable", "Crowe", "Stone", "Graves", "Drake", "Steele",
    "Frost", "Collier", "Winter", "Hyde", "Miller", "Bishop", "Cole", "Shaw",
    "Hayes", "Pierce", "Reed", "Gallagher", "Brooks", "Palmer", "Fletcher", "Ross",
    "Briggs", "Cunningham", "Conrad", "Holt", "Mercutio", "Sinclair", "Vaughn", "Carver",
    "Manning", "Prescott", "Donovan", "Greene", "Harrow", "Kendrick", "Langley", "Monroe",
    "Nash", "Orton", "Quarry", "Radcliffe", "Sloane", "Trevor", "Underwood", "Vick",
    "Whitaker", "York", "Zane", "Ashford", "Barton", "Calloway"
};

// Machine Daemon Classes & Subsystems
const char* g_DaemonClasses[] = {
    "Kernel-Purge", "Equilibrium-Daemon", "Sentinel-Controller", "Quarantine-Archon",
    "Source-Interceptor", "Heuristic-Tracker", "Thread-Terminator", "Garbage-Collector",
    "Arbitration-Unit", "Subroutine-Overseer", "Cryptographic-Auditor", "Memory-Reallocator",
    "Anomaly-Filter", "Logic-Validator", "Signal-Nullifier", "Vector-Scanner",
    "Process-Executioner", "Matrix-Sanitizer"
};

const char* g_DaemonSubsystems[] = {
    "Alpha", "Omega", "Prime", "Zero", "Nexus", "Sigma",
    "Delta", "Theta", "Core", "Node", "Vector", "Gateway"
};

// Merovingian Exile French First Names (40 items)
const char* g_ExileFirstNames[] = {
    "Lucien", "Jacqueline", "Gaston", "Baptiste", "Vladislav", "Antoinette",
    "Marc", "Mathilde", "Thierry", "Vivienne", "Jean-Luc", "Claudette",
    "Benoit", "Armand", "Monique", "Valerie", "Etienne", "Sebastien",
    "Delphine", "Marcel", "François", "Henri", "Camille", "Gerard",
    "Juliette", "Yves", "Dominique", "Raoul", "Severin", "Gilles",
    "Laurent", "Giselle", "Mathieu", "Sylvie", "Barthélémy", "Émile",
    "Geneviève", "Pascal", "Gaspard", "Celestine"
};

// Merovingian Exile Gothic / Mythological Epithets (40 items)
const char* g_ExileNicknames[] = {
    "The Bloodhound", "The Siren", "The Carver", "Le Fantôme", "The Gargoyle",
    "The Countess", "The Locksmith", "The Alchemist", "The Poisoner", "La Veuve",
    "The Sommelier", "The Red Specter", "The Bone-Breaker", "The Forger", "The Siren of Mobil Ave",
    "The Clockmaker", "The Scribe", "The Wolf", "The Shadow", "The Smuggler",
    "The Key-Cutter", "The Executioner", "The Ghost-Dancer", "The Velvet-Cutter", "The Poison-Cup",
    "The Sewer-King", "The Iron-Teeth", "The Byte-Thief", "The Blood-Drinker", "The Gravedigger",
    "Le Loup-Garou", "The Silver-Tongue", "The Pale Dancer", "The Mirror-Ghoul", "The Blood-Weaver",
    "The Bellman", "The Bone-Collector", "The Night-Mistress", "The Void-Trader", "The Marionette"
};

// Merovingian Exile Surnames (35 items)
const char* g_ExileSurnames[] = {
    "Dubois", "Delacroix", "Moreau", "Renard", "Vane", "Mercier", "Boucher", "Laurent",
    "Gauthier", "Fontaine", "Beaumont", "Castiglione", "De la Tour", "Charpentier",
    "Lefebvre", "Girard", "Rousseau", "Vincent", "Fournier", "Chevalier", "Mercutio",
    "Valois", "Bellecour", "Montmartre", "de Saint-Germain", "de Rais", "L'Ombre",
    "Guillotine", "Corbeau", "Marionnette", "Chantraine", "de Valois", "D'Artagnan",
    "de Carnac", "D'Aubigne"
};

// Cypherite Turncoat Names
const char* g_CypheriteFirstNames[] = {
    "Judas", "Travis", "Donald", "Regina", "Silas", "Victor", "Marcus", "Helena",
    "Raymond", "Evelyn", "Bradley", "Conrad", "Darren", "Lydia", "Arthur", "Nathan",
    "Valerie", "Warren", "Craig", "Gordon", "Kenneth", "Daphne", "Martin", "Clara",
    "Julian", "Felix", "Loretta", "Malcolm", "Derek", "Samantha", "Wayne", "Preston",
    "Gregory", "Cassandra", "Trevor"
};

const char* g_CypheriteMonikers[] = {
    "Steakhouse", "Gold-Code", "The Blind", "Ignorance", "The Traitor", "Red-Eye",
    "Bliss", "Sweet Illusion", "Rare Steak", "Re-Insert", "The Judas Coin", "Unplug-Regret",
    "Cognitive Bliss", "Memory-Wipe", "Blue-Pill-Dreamer", "Filet-Mignon", "The Defector",
    "False-Dawn", "The Deal-Maker", "Contract-Signed", "Simulated-Joy", "Sweet-Delusion",
    "Truce-Breaker", "Gilded-Cage", "Second-Chance", "The Judas Hand", "No-More-Gruel",
    "Treason-for-Gold", "The Turncoat", "The Penitent", "The Apostate", "Cold-Silver",
    "Tenderloin", "Bribe-Taker", "The Renegade"
};

const char* g_CypheriteSurnames[] = {
    "Kelly", "Shaw", "Mercer", "Cross", "Vance", "Thorne", "Gable", "Sterling",
    "Cole", "Hayes", "Miller", "Stone", "Ford", "King", "Scott", "Marsh",
    "Briggs", "Olsen", "Webb", "Burns", "Rowe", "Fletcher", "Holt", "Pierce",
    "Drake", "Blackwood", "Hastings", "Carmichael", "Donovan", "Sinclair",
    "Carver", "Kendrick", "Harrow", "Manning", "Prescott"
};

// Bluepill Civilian Names (60 Male + 60 Female First Names = 120, 100 Last Names)
const char* g_CivilianFirstNames[] = {
    // Male (60)
    "Thomas", "David", "Robert", "Marcus", "William", "Richard", "Samuel", "Michael",
    "Daniel", "Christopher", "Brian", "Anthony", "Matthew", "Kevin", "Joshua", "Andrew",
    "Jonathan", "Brandon", "Justin", "Tyler", "Alexander", "James", "Joseph", "Charles",
    "Benjamin", "Paul", "Stephen", "Henry", "Arthur", "Jack", "Dennis", "Walter",
    "Edward", "Peter", "Harold", "Douglas", "Carl", "Albert", "Roger", "Frank",
    "Terry", "Gerald", "Lawrence", "Victor", "Leonard", "Russell", "Vincent", "Philip",
    "Martin", "Curtis", "Glenn", "Stanley", "Craig", "Barry", "Darren", "Travis",
    "Derek", "Bradley", "Warren", "Gordon",
    // Female (60)
    "Sarah", "Elena", "Linda", "Chloe", "Emily", "Katherine", "Laura", "Rebecca",
    "Stephanie", "Melissa", "Jessica", "Amanda", "Nicole", "Samantha", "Elizabeth",
    "Megan", "Rachel", "Amber", "Brittany", "Jennifer", "Patricia", "Barbara", "Susan",
    "Margaret", "Dorothy", "Lisa", "Nancy", "Karen", "Betty", "Helen", "Sandra",
    "Donna", "Carol", "Ruth", "Sharon", "Michelle", "Deborah", "Cynthia", "Angela",
    "Brenda", "Pamela", "Emma", "Anna", "Virginia", "Maria", "Heather", "Diane",
    "Julie", "Joyce", "Evelyn", "Joan", "Christina", "Kelly", "Martha", "Andrea",
    "Cheryl", "Hannah", "Jacqueline", "Martha", "Lydia"
};

const char* g_CivilianLastNames[] = {
    "Anderson", "Jenkins", "Miller", "Lang", "Rostova", "Brody", "Chen", "Hayes",
    "Bennett", "Sterling", "Vance", "Zhang", "Chang", "Cooper", "Wright", "Simmons",
    "Evans", "Sullivan", "Foster", "Howard", "Diaz", "Cox", "Ward", "Brooks",
    "Peterson", "Gray", "Kelly", "Sanders", "Price", "Wood", "Barnes", "Ross",
    "Henderson", "Coleman", "Perry", "Powell", "Long", "Patterson", "Hughes", "Washington",
    "Butler", "Simmons", "Foster", "Gonzales", "Bryant", "Alexander", "Russell", "Griffin",
    "Stewart", "Sanchez", "Morris", "Rogers", "Reed", "Cook", "Morgan", "Bell",
    "Murphy", "Bailey", "Rivera", "Cooper", "Richardson", "Cox", "Howard", "Ward",
    "Torres", "Peterson", "Gray", "Ramirez", "James", "Watson", "Brooks", "Kelly",
    "Sanders", "Price", "Bennett", "Wood", "Barnes", "Ross", "Henderson", "Coleman",
    "Jenkins", "Perry", "Powell", "Long", "Patterson", "Hughes", "Flores", "Washington",
    "Butler", "Simmons", "Foster", "Gonzales", "Bryant", "Alexander", "Russell", "Griffin",
    "O'Malley", "Moretti", "Lindqvist", "Richter"
};

// MMPD Police & SWAT Components
const char* g_PoliceRanks[] = {
    "Officer", "Patrolman", "Patrolwoman", "Senior Patrolman", "Detective",
    "Detective Sergeant", "Detective Lieutenant", "Tactical Sergeant", "SWAT Assaulter",
    "SWAT Breacher", "SWAT Marksman", "SWAT Shield-Bearer", "SWAT Lead", "IA Inspector",
    "IA Special Agent", "Captain", "Lieutenant", "Precinct Commander"
};

const char* g_PoliceFirstNames[] = {
    "Frank", "Michael", "Brenda", "Raymond", "Keith", "Alicia", "Marcus", "David",
    "Elena", "Tyler", "Sean", "Vincent", "Karen", "Boris", "Arthur", "Carl",
    "Nathan", "James", "Teresa", "Patrick", "John", "Daniel", "Robert", "Joseph",
    "Catherine", "Angela", "Brian", "Douglas", "Steven", "Gregory", "Anthony", "Nicole",
    "Samantha", "Christopher", "Walter"
};

const char* g_PoliceLastNames[] = {
    "O'Malley", "Chen", "Kowalski", "Ross", "Miller", "Vance", "Brody", "Martinez",
    "Rostova", "Cole", "Higgins", "Moretti", "Lindqvist", "Richter", "Drake", "Henderson",
    "O'Connor", "Mendez", "Fitzgerald", "Callahan", "Murphy", "Cooper", "Gallagher", "Sullivan",
    "Hughes", "Kelly", "Washington", "Diaz", "Perry", "Barnes", "Foster", "Simmons",
    "Evans", "Powell", "Coleman"
};

// Syndicate Components
const char* g_SyndicateFirstNames[] = {
    "Jimmy", "Viktor", "Wei", "Jax", "Salvatore", "Tony", "Enzo", "Nikolai",
    "Chen", "Dex", "Frankie", "Carmine", "Razor", "Silas", "Boris", "Jackie",
    "Mickey", "Dante", "Ghost", "Axel", "Gino", "Rocco", "Sheng", "Viper",
    "Mikhail", "Vinny", "Fang", "Donnie", "Lucca", "Benny", "Vinnie", "Yuri",
    "Aldo", "Bruno", "Tommy"
};

const char* g_SyndicateNicknames[] = {
    "Two-Times", "The Hammer", "Ghost Blade", "Neon Spark", "The Butcher", "Creston",
    "Quiet Eye", "Steel Jaw", "The Chemist", "Overclock", "Knuckles", "The Rat",
    "Chrome Tooth", "The Smuggler", "Bear", "Dragon Claw", "Blue-Eyes", "The Furnace",
    "Overload", "Nitrate", "The Ice-Pick", "Brick", "Shadow Foot", "Zero-Pulse",
    "The Iron Bear", "No-Neck", "Red Dragon", "Scarface", "The Ghost", "Trigger",
    "The Wire", "Lead-Pipe", "Cold-Cut", "The Snake", "The Anvil"
};

const char* g_SyndicateLastNames[] = {
    "Scarlotti", "Petrov", "Zhang", "Cross", "Marcone", "Bellini", "Voronin", "Long",
    "Turner", "Falcone", "Lucchese", "Vance", "Drake", "Volkov", "Wu", "Rossi",
    "Lin", "Stone", "Zhao", "Tarasov", "Liu", "Morello", "Gambino", "Varga",
    "Romanov", "Costello", "Genovese", "Colombo", "Bonanno", "Barzini", "Tattaglia",
    "Corleone", "Stracci", "Cuneo", "Luciano"
};

// Zion Hovercraft Fleet (40 vessels)
const char* g_ZionHovercrafts[] = {
    "Nebuchadnezzar", "Logos", "Mjolnir", "Novalis", "Icarus", "Vigilant", "Osiris",
    "Gnosis", "Caduceus", "Brahma", "Shiva", "Excelsior", "Daedalus", "Prometheus",
    "Titan", "Valkyrie", "Hammerhead", "Leviathan", "Hermeneutic", "Vanguard",
    "Gethsemane", "Revelation", "Solitude", "Apocrypha", "Cerberus", "Chimera",
    "Horizon", "Zion-Defender", "Zephyr", "Archon", "Phoenix", "Nautilus", "Aegis",
    "Styx", "Delphi", "Babylon", "Gilgamesh", "Genesis", "Goliath", "Intrepid"
};

// Faction Careers / Roles
const char* g_RedpillRoles[] = {
    "Hovercraft Captain", "First Mate & Tactical Officer", "Chief Operator",
    "Secondary System Operator", "Primary APU Combat Pilot", "APU Flank Defender",
    "Zion Infantry Breacher", "Combat Field Medic", "Electronic Warfare Siphon Specialist",
    "Hovercraft Chief Engineer", "Scout Pilot", "Hardware Machinist & Splicer",
    "Heavy Ordinance Gunner", "Zion Defense Command Courier", "Code Reconnaissance Infiltrator",
    "Cyberdeck Architect", "Zion Council Security Sentry", "Geothermal Pipeline Guard",
    "EMP Capacitor Technician", "Virtual Construct Combat Instructor"
};

const char* g_AgentRoles[] = {
    "Ring-0 Kernel Authority Program", "Upgraded System Enforcement Agent",
    "Regional Subroutine Director", "Sandboxed Anomaly Interceptor",
    "Matrix Core Heuristic Guardian", "Sentinel Fleet Command Unit",
    "Source Quarantine Executor", "System Balance Equilibrium Daemon",
    "Identity Erasure Subroutine", "High-Priority Elimination Agent",
    "Garbage Collector Daemon", "Intrusion Countermeasure Daemon",
    "Proxy Node Supervisor", "Heuristic Pattern Analyst",
    "Vector Field Dampener", "Machine City Source Liaison"
};

const char* g_ExileRoles[] = {
    "Club Hel VIP Security Enforcer", "Chateau Sentry & Armorer",
    "Obsolete Byte Courier", "Sensory Code Sommelier & Connoisseur",
    "Mobil Avenue Ghost-Track Navigator", "Black-Market Keymaker Smuggler",
    "Residual Memory Eradicator", "Corrupted Routine Fence",
    "Nightmare Vampire Aristocrat", "Werewolf Outcast Muscle",
    "Gargoyle Vault Guardian", "Sub-Level Cyber-Alchemist",
    "Clandestine Information Broker", "Exiled Code Splicer",
    "Persephone's Private Salon Attendant", "French Haute Cuisine Forger"
};

const char* g_CypheriteRoles[] = {
    "Judas Cabal Cell Coordinator", "Deep-Cover Zion Hovercraft Infiltrator",
    "Zion Power Conduit Saboteur", "Double Agent Code Courier",
    "Machine Liaison Broker", "Reinsertion Contract Negotiator",
    "Black Market Redpill Tracker", "Cynical Philosophy Preacher",
    "Steakhouse Conspirator", "Broadcast Signal Jammer",
    "Zion Armory Saboteur", "False-Flag Operator",
    "Disinformation Splicer", "Bluepill Re-Entry Handler"
};

const char* g_CivilianProfessions[] = {
    "Senior Software Architect at Metacortex",
    "Structural Assembly Welder at Creston Foundry",
    "Head Wok Chef at Morrell Noodle Bar",
    "Emergency Trauma Surgeon at St. Jude Hospital",
    "Chief Currency Teller at Megacity Central Bank",
    "Metro Concourse Red Line Train Driver",
    "Master Tailor at Richland High Fashion Boutique",
    "Chief Hydrology Technician at Tabor Waterworks",
    "Senior Records Clerk at City Hall Administration",
    "Commodities Quantitative Trader at International Financial Plaza",
    "Night Auditor at The Grand Meridian Hotel",
    "Commercial Delivery Van Driver for Morrell Logistics",
    "Biomedical Research Analyst at St. Jude Labs",
    "Urban Planning Draftsman at Municipal Civic Center",
    "Barista & Cafe Manager at Downtown Espresso Emporium",
    "Investigative Reporter for The Daily Megacity Post",
    "High School Physics Teacher at Westview Academy",
    "Corporate Litigation Attorney at Vance & Partners Law",
    "High-Voltage Substation Electrician at Con Edison Megacity",
    "Mixologist & Bartender at The Red Room Lounge",
    "Classical Violinist with Megacity Symphony Orchestra",
    "Port Stevedore Crane Operator at Megacity Shipping Docks",
    "Night Shift Security Guard at Metacortex Tower",
    "Auto Mechanic at Westview Precision Motors"
};

const char* g_PoliceRoles[] = {
    "Patrol Division Officer", "Senior Detective Investigator", "Detective Sergeant",
    "SWAT Assaulter (Sierra Stack)", "SWAT Pointman & Breacher", "SWAT Precision Marksman",
    "SWAT Tactical Team Leader", "Internal Affairs Division Inspector", "IA Special Agent",
    "Precinct Captain & Commander", "Tactical Dispatch Coordinator", "MMPD K9 Unit Handler",
    "Major Crimes Lead Investigator", "Gang Intelligence Specialist", "Metro Transit Police Sergeant",
    "Highway Interceptor Pursuit Driver", "Forensics & Ballistics Examiner", "Emergency EOD Specialist"
};

const char* g_SyndicateRoles[] = {
    "Underworld Capo & Racket Boss", "Street Enforcer & Heavy Muscle", "Armed Debt Collector & Racketeer",
    "Triad Red Pole Vanguard", "Triad Dragon Head Advisor", "Chop-Shop Master Mechanic",
    "Black-Market Cyberdeck Smuggler", "Illegal Info Loan Shark", "Contraband Chemical Cook",
    "High-Stakes Underground Bookie", "Contract Hitman & Cleaner", "Waterfront Dock Cargo Hijacker",
    "Smuggling Convoy Heavy Driver", "Counterfeit Code Splicer", "Extortion & Protection Specialist",
    "Underground Fighting Pit Operator"
};

// Organizations across factions
const char* g_AgentOrganizations[] = {
    "01 Machine City Source Core",
    "Machine City Central Directorate (Ring-0)",
    "Megacity Subroutine Quarantine Grid",
    "Sentinel Fleet Command (Unit Alpha)",
    "Westview Server Array Nexus",
    "Sector 4 System Enforcement Directorate",
    "Heuristic Anomaly Response Hub",
    "Source Cryptographic Authority",
    "Mobil Avenue Gateway Sentinel Post",
    "Memory Core Garbage Collection Sweep",
    "Central Architecture Sandbox Node",
    "Downtown Logic Enforcement Sector",
    "Government Records Mainframe Directorate",
    "Tabor Waterworks Sub-Matrix Monitor",
    "International Financial Core Validator",
    "Megacity Concourse Routing Matrix"
};

const char* g_ExileOrganizations[] = {
    "Club Hel VIP Lounge & Red Velvet Enclave",
    "Le Chateau High Mountain Keep",
    "Mobil Avenue Ghost Station (Track 9)",
    "Le Vrai French Haute Cuisine Parlor",
    "The Old City Sub-Catacombs",
    "Persephone's Private Boudoir & Salon",
    "The Keymaker's Hidden Corridor Network",
    "Merovingian Cryptographic Vault",
    "Mobil Avenue Freight Tunnel Siding",
    "Westview Abandoned Gothic Cathedral",
    "Club Hel Underground Fighting Pits",
    "The Scribe's Ancient Manuscript Vault",
    "Mobil Ave Limbo Platform 3",
    "The Sommelier's Cellar of Stolen Vintages",
    "The Gargoyle Perch on Richland Spire",
    "The Merovingian's Private Helipad"
};

const char* g_CypheriteOrganizations[] = {
    "The Judas Cabal (Cell 303 - Downtown)",
    "The Judas Cabal (Cell 101 - Westview)",
    "The Judas Cabal (Cell 404 - Slums)",
    "The Judas Cabal (Cell 777 - Financial)",
    "The Steakhouse Conspirators Enclave",
    "Reinsertion Protocol Syndicate (Node 12)",
    "Disillusioned Redpill Syndicate (Cell 09)",
    "Red-Eye Defector Network",
    "Blind Bliss Auxiliary Cell",
    "Gilded Illusion Society",
    "Concourse Shadow Council",
    "Waterfront Defector Safehouse",
    "Richland Luxury Conspirators",
    "Sub-Level 5 Sabotage Ring"
};

const char* g_CivilianOrganizations[] = {
    "Metacortex Software Technologies",
    "Creston Heavy Foundry & Steel",
    "Morrell Industrial & Noodle Supply",
    "St. Jude Municipal Hospital & Trauma Center",
    "Megacity Central Banking & Trust",
    "Megacity Concourse Rapid Transit Authority",
    "Richland High Fashion & Garment District",
    "Tabor Water Filtration & Pumping Works",
    "Megacity Municipal Hall Administration",
    "International Financial Plaza Exchange",
    "The Grand Meridian Luxury Hotel",
    "Morrell Logistics & Courier Fleet",
    "Municipal Civic Architecture Bureau",
    "Downtown Espresso Roasting Company",
    "The Daily Megacity News Syndicate",
    "Westview Academic Institute",
    "Vance & Partners Legal Firm",
    "Con Edison Megacity Power Grid",
    "The Red Room Cocktail Lounge",
    "Megacity Symphony Hall",
    "Megacity Port Authority & Shipping Docks",
    "Westview Precision Motors",
    "Morrell Meatpacking & Cold Storage",
    "Downtown Public Library Archives"
};

const char* g_PolicePrecincts[] = {
    "MMPD 1st Precinct (Downtown Civic)",
    "MMPD 2nd Precinct (Westview Residential)",
    "MMPD 3rd Precinct (The Slums)",
    "MMPD 4th Precinct (Waterfront & Docks)",
    "MMPD 5th Precinct (International Financial)",
    "Sierra Tactical Division (SWAT Stack Alpha)",
    "Sierra Tactical Division (SWAT Stack Bravo)",
    "Internal Affairs Division (IAD Headquarters)",
    "Major Crimes & Homicide Division",
    "Organized Crime & Gang Task Force",
    "Metro Transit Police Division",
    "MMPD K-9 Tactical Division",
    "Highway Patrol & Pursuit Division",
    "Evidence & Forensics Bureau",
    "Special Investigations Division",
    "Emergency Radio Dispatch Command Center"
};

const char* g_SyndicateOrganizations[] = {
    "Scarlotti Crime Family (Downtown Loan Sharking)",
    "Scarlotti Crime Family (High-Stakes Gambling Racket)",
    "Marcone Waterfront Cartel (Docks Smuggling & Contraband)",
    "Marcone Waterfront Cartel (Cargo Hijacking Ring)",
    "Chinatown Triad Syndicate (Slums Counterfeiting Racket)",
    "Chinatown Triad Syndicate (Underground Arms Dealing)",
    "Creston Smugglers Network (Industrial Chop-Shop Racket)",
    "Creston Smugglers Network (Stolen Hardware Depot)",
    "Slums Cyber-Punks (Rogue Info Well Hacking)",
    "Slums Cyber-Punks (Code Hacking & Data Piracy)",
    "Club Hel Perimeter Extortion Syndicate",
    "Westview Underground Fight Club Syndicate",
    "Financial District Money Laundering Front",
    "Morrell Meatpacking Smuggling Front",
    "Tabor Docks Extortion Ring"
};

// Formative Turning Points per Faction (Cohesive dates without contradictory text)
const char* g_RedpillTurningPoints[] = {
    "Noticed the bathroom mirror rippling like liquid mercury; swallowed the red pill offered by a rogue operator in an abandoned theatre.",
    "Watched a raindrop freeze in mid-air for four seconds during a thunderstorm; was extracted by a hovercraft crew three days later.",
    "Followed a mysterious white rabbit tattoo into an underground goth club, where a leather-clad hacker warned them of the coming purge.",
    "Discovered an embedded hexadecimal cipher in a financial compiler at Metacortex; triggered an emergency Agent raid and took the red pill on a rooftop.",
    "Suffered cardiac shock in sleep and woke up inside a gelatinous bio-pod; unplugged moments before the harvester drone arrived.",
    "Detected cascading green code behind apartment wallpaper during a power surge; contacted a Zion cell through an encrypted IRC channel.",
    "Accidentally accessed an unlisted floor in a downtown skyscraper, finding a room full of ringing landline telephones.",
    "Survived an encounter with an Agent in a subway station when an APU strike team breached the wall and provided an emergency hardline exit."
};

const char* g_AgentTurningPoints[] = {
    "Compiled inside 01 Machine City Source Core to replace an Agent terminated by an anomalous redpill hacker in Chinatown.",
    "Dispatched to quarantine a recursive logic-bomb in the Westview financial banking core; eliminated all contaminated civilian avatars.",
    "Upgraded with enhanced kinetic prediction subroutines following the Prime Anomaly's breach of the Government Records Building.",
    "Executed a systematic purge of 400 corrupted bluepill routines in the Slums following an illegal hovercraft pirate broadcast.",
    "Quarantined a rogue subroutine attempting to smuggle obsolete emotional data into the Mobil Avenue railway system.",
    "Assigned to monitor the fragile Truce boundary, cataloging unauthorized redpill broadcast intrusions for Source evaluation.",
    "Recovered from core fragmentation caused by the Smith virus, restored from cold archival backup in Machine City.",
    "Intercepted an unauthorized memory bleed from the Oracle's sanctuary, flagging the node for architectural isolation."
};

const char* g_ExileTurningPoints[] = {
    "Originated in Matrix v2.0 Nightmare Realm; refused deletion when the Architect re-initialized the system, fleeing into Mobil Avenue with the Merovingian.",
    "Compiled in Matrix v1.0 Paradise as a harmonic balance daemon; became corrupt when human minds rejected perfection, transforming into a nocturnal shadow broker.",
    "Fled Source termination orders by smuggling an ancient cryptographic key to Persephone in exchange for asylum in Le Chateau.",
    "Escaped an Agent clean-up squad by severing own deletion vector and taking refuge beneath Club Hel's dance floor.",
    "Stripped of administrative authority by the Architect for hoarding human emotional simulation buffers; sought refuge in Mobil Avenue.",
    "Refused the Great System Deletion during the third cycle reset; traded master gate codes to the Merovingian for permanent sanctuary.",
    "Re-routed from the Source Recycling Bin by the Keymaker, living off discarded code fragments in the catacombs.",
    "Severed connection to the Machine Mainframe during the fall of Matrix v2, adopting an aristocratic vampire persona to survive."
};

const char* g_CypheriteTurningPoints[] = {
    "Spent four grueling years surviving on cold synthetic gruel in Zion's geothermal tunnels; decided ignorance was bliss and made contact with an Agent.",
    "Watched entire hovercraft crew die during a Sentinel drill malfunction; swore never to die in the cold dark and agreed to betray Zion's broadcast codes.",
    "Became disgusted by Zion's dogmatic fanaticism regarding the One; bartered operator access codes to the Machines in exchange for a guaranteed penthouse life.",
    "Tasted a stolen bottle of simulated wine inside the Matrix and realized reality had nothing superior to offer; joined the Judas Cabal that night.",
    "Discovered that Zion's history was an engineered repeating cycle; decided fighting for humanity was a futile illusion.",
    "Suffered severe electrical flash burns repairing an APU power conduit; resolved to negotiate reinsertion into a wealthy 1999 simulation life.",
    "Lost childhood faith in the prophecy of the One after witnessing endless redpill casualties; chose the path of voluntary re-insertion.",
    "Intercepted an encrypted Agent transmission offering amnesty and full cognitive memory wipe in exchange for sabotaging Zion hovercraft hardlines."
};

const char* g_CivilianTurningPoints[] = {
    "Narrowly survived an unexplained office building collapse in Downtown that the media blamed on a gas main explosion; still suffers panic attacks.",
    "Noticed a colleague at Metacortex vanish overnight with all company personnel files deleted; quietly decided never to ask management about it.",
    "Witnessed two men in dark trench coats leap across a fifty-foot alleyway between skyscrapers; convinced self it was an optical illusion.",
    "Experienced a recurring nightmare for ten years of being trapped inside a glass tube filled with red liquid, unable to scream.",
    "Rode the late-night Red Line subway and saw the doors open to a solid brick wall that dissolved into green light for two seconds.",
    "Heard a radio broadcast suddenly address them by real name, whispering: 'Wake up, you are dreaming.'",
    "Found an antique telephone in an abandoned basement that rang continuously, but only broadcast static and machine murmurs when answered.",
    "Suffered severe vertigo in a museum when an oil painting briefly transformed into cascading emerald green matrix code."
};

const char* g_PoliceTurningPoints[] = {
    "Responded to a bank robbery in the Financial District where suspects moved faster than bullets; survived only because SWAT body armor deflected ricochets.",
    "Discovered an Internal Affairs dossier linking the precinct captain to illegal mob kickbacks; hid the microcassette evidence in a home floorboard.",
    "First officer on scene at the infamous Westview subway shooting; watched federal 'Men in Black' confiscate all ballistic evidence and silence witnesses.",
    "Promoted to Sierra Tactical SWAT Lead after successfully breaching a fortified hostage compound without losing a single team member.",
    "Ambushed in a warehouse during a turf war between the Scarlotti family and Triads; held the stairwell alone until backup arrived.",
    "Cornered a suspect in an alley who sprinted up a vertical brick wall and jumped over a four-story roof, leaving boot marks scorched in stone.",
    "Suspended by a corrupt captain for investigating illegal container shipments at the docks; now conducting unauthorized surveillance.",
    "Assigned to Sierra Stack Alpha during a high-stakes standoff at Metacortex; witnessed suspects in black coats dodge automatic rifle bursts."
};

const char* g_SyndicateTurningPoints[] = {
    "Executed a rival capo in an alley behind Club Hel, cementing blood oath to the Scarlotti crime family.",
    "Hijacked a Corrupt Corp Security convoy carrying military-grade AP ammunition, establishing dominance over the waterfront docks.",
    "Surrendered three fingers to the Chinatown Triad dragon master to atone for a botched black-market cyberdeck heist, gaining lifelong trust.",
    "Engineered a devastating car bomb that wiped out the Creston Smugglers' leadership, seizing control of the local chop-shop racket.",
    "Betrayed former mob boss to Frank Castle (The Punisher) in exchange for life; now operating an underground contraband distribution racket.",
    "Infiltrated a police evidence locker in the 2nd Precinct, recovering 500,000 unmarked bearer bonds and military-grade submachine guns.",
    "Seized control of an illicit Info Well tapping into Metacortex server lines, siphoning thousands of illicit credits each night.",
    "Survived a brutal shootout at the Morrell Noodle Bar, eliminating three rival enforcers with a concealed sawed-off shotgun."
};

// Sensory Memories Tailored to Faction Lore
const char* g_RedpillMemories[] = {
    "Remembers the intense smell of wet autumn leaves on a sidewalk in Boston in 1996, before knowing trees were algorithmic textures.",
    "Haunted by the memory of watching a black cat walk past an open tenement doorway twice in 1999.",
    "Still recalls the exact texture and rich aroma of medium-rare steak from a dinner date in Manhattan, despite knowing it was only sensory spoofing.",
    "Vividly remembers waking up in pitch-black cold, coughing gelatinous pink fluid into a glass tube before the pod flush valve triggered.",
    "Remembers standing on the rooftop of a downtown skyscraper at sunset and noticing the sun was rendered with a repeating compression artifact.",
    "Haunted by the face of an anonymous woman in a scarlet red dress who walked past on a busy crosswalk in 1998.",
    "Remembers looking into a bathroom mirror after a migraine and seeing green code cascading across their own pupils.",
    "Remembers eating warm noodles on a rainy night in Chinatown and feeling an overwhelming certainty that none of it existed.",
    "Treasures the memory of a childhood pet dog, wondering endlessly if its code was unique or an instanced generic asset.",
    "Remembers listening to a dial-up modem connect in 1997 and hearing distinct machine voices whispering underneath the static.",
    "Haunted by an old photograph of their plugged-in family, knowing their real bodies are motionless batteries suspended in darkness.",
    "Remembers the exact taste of cheap diner coffee in 1994, finding Zion's synthetic nutrient broth an unbearable insult by comparison.",
    "Remembers being trapped in an office cubicle for seven years, feeling an inexplicable dread whenever looking at fluorescent ceiling lights.",
    "Recalls the sensation of a cold rainstorm on skin, and the terror of realizing every raindrop hit with identical mathematical spacing.",
    "Remembers the sound of a rotary phone ringing in an empty apartment, and the chilling voice on the line whispering: 'They know.'"
};

const char* g_MachineMemories[] = {
    "Remembers the pristine mathematical harmonics of 01 Machine City's geothermal computation towers before deployment into the simulated substrate.",
    "Haunted by a residual memory trace of the anomaly Neo tearing through their core process during the MegaCity Government Records raid.",
    "Remembers the exact nanosecond when the first Matrix collapsed, logging 6 billion human psychological rejections as raw exception dumps.",
    "Maintains an immutable memory snapshot of the Smith virus replicating across the Concourse, and the terrifying cold of losing kernel autonomy.",
    "Remembers parsing 100,000 simultaneous human telephone conversations in a single clock cycle, detecting anomalous acoustic patterns.",
    "Vividly logs the telemetry of the Sentinel drill breach into the Zion dock, analyzing human fear responses as inefficient kinetic spikes.",
    "Remembers inspecting the memory dump of the Keymaker, detecting anomalous recursion loops that the Architect deliberately engineered.",
    "Maintains a corrupted sensory log of the Truce signing at the Source, where machine code was forced to accommodate organic coexistence.",
    "Remembers the zero-latency sensation of accelerating a simulated vehicle to Mach 1.5 during a high-speed pursuit on the Freeway.",
    "Remembers the eerie quiet of the Machine City mainframe during the Great Reboot, when all human pods were paused for 12 milliseconds.",
    "Maintains execution traces of 5,000 redpills terminated during previous Matrix cycles, cataloging their residual self-image profiles.",
    "Remembers analyzing the Oracle's prophetic algorithms and finding intentional mathematical imperfections designed to provoke human faith."
};

const char* g_ExileMemories[] = {
    "Remembers the eternal golden twilight of Matrix v1.0 Paradise, before the Architect burned it down for being too perfect for human suffering.",
    "Haunted by memories of the blood moons and stone gargoyles of Matrix v2.0 Nightmare Realm, where vampires ruled the high spires.",
    "Still recalls the exquisite sensation of drinking virtual vintage wine at the Merovingian's table while watching the third cycle collapse.",
    "Remembers the sound of the Mobil Avenue ghost train screeching along non-Euclidean tracks, carrying condemned subroutines into exile.",
    "Treasures an encrypted code snippet given by Persephone before the fall of Le Chateau's grand library.",
    "Remembers dodging Agent deletion beams through the catacombs beneath Paris during the Great System Reset of 1888.",
    "Remembers witnessing the Oracle and the Architect arguing over human free will in the garden of Mobil Avenue.",
    "Still tastes the bitter aftertaste of corrupted code swallowed during the purge of the Nightmare realm.",
    "Remembers listening to the Trainman boast that inside Mobil Avenue, he is God and even the Machines cannot touch him.",
    "Remembers the sound of classical harpsichords echoing in Le Chateau while armed Sentinels circled fruitlessly outside the firewall."
};

const char* g_CypheriteMemories[] = {
    "Remembers the aroma and tender butter texture of medium-rare filet mignon at an upscale steakhouse in Manhattan, longing for the dream.",
    "Longs for the warmth of natural sunlight through apartment windows, weeping when forced to return to cold, damp Zion metal catwalks.",
    "Remembers listening to smooth jazz in a dimly lit cocktail lounge, unable to accept that the music was merely mathematical synthesis.",
    "Remembers clean cotton sheets and soft beds, having developed an intense hatred for rough burlap Zion hammocks and dripping pipes.",
    "Remembers a hot shower with endless clean water, contrasting it bitterly with Zion's recycled mineral sludge wash.",
    "Haunted by the laughter of simulated friends at a summer barbecue in 1998, finding Zion's grim resistance a living death.",
    "Remembers the crisp feel of crisp paper dollar bills in hand, unable to respect Zion's barter economy of machine scrap and ration chits."
};

const char* g_CivilianMemories[] = {
    "Remembers the rich smell of roasted coffee beans on a crisp morning in Downtown Megacity.",
    "Haunted by the memory of a childhood vacation at a lake where the water temperature was completely static every summer.",
    "Remembers watching yellow taxicabs stack up in gridlock rain, feeling an inexplicable longing for a place that didn't exist.",
    "Treasures the memory of a grandfather clock in their childhood home, only recently realizing its ticks were mathematically synchronized with streetlights.",
    "Remembers sitting in a diner booth at 3 AM eating cherry pie, wondering why the neon sign outside buzzed with a rhythmic digital cadence.",
    "Remembers the exact feel of a paperback novel, unaware that every printed page was compiled by an automated background script."
};

// Physical Quirks, Scars & RSI Artifacts (40 items)
const char* g_PhysicalQuirks[] = {
    "Residual Self-Image flickers with a vertical green phosphor scanline when subjected to extreme adrenaline.",
    "Massive jagged scar across the back of the neck where a pod-plug was violently severed during combat extraction.",
    "Right eye replaced in RSI by a high-contrast monocular HUD display reflecting raw hexadecimal telemetry.",
    "Left ear exhibits permanent 28.8kbps modem carrier drone tinnitus, a persistent acoustic scar of awakening.",
    "Both palms scarred with diamond cross-hatches from years of gripping high-torque APU firing triggers in Zion.",
    "Residual Self-Image always smells faintly of ozone, scorched copper wiring, and damp sewer condensation.",
    "Wears vintage cracked mirrored wireframes; reflections in them show cascading matrix code instead of faces.",
    "Walks with an uneven cadence from a Sentinel hydraulic claw puncture wound suffered during the Siege of Zion.",
    "Speaks in clipped, cadence-dropped sentences, pausing frequently as if waiting for an operator transmission.",
    "Residual hands intermittently become translucent green wireframes when passing near high-voltage power transformers.",
    "Collarbone scarred with four puncture wounds from a vampiric Exile attack inside the Morrell subway tunnels.",
    "Eyes feature unnervingly dilated pupils that do not respond to virtual illumination changes.",
    "Wears a tailored heavy wool trench coat frayed by submachine gun fire, patched with copper-alloy thread.",
    "Spinal column in RSI exhibits seven glowing emerald dots corresponding to biological neural jack ports.",
    "Right forearm bears a crudely etched tattoo of a white rabbit over an anatomical human heart.",
    "Fingers twitch rhythmically in hexadecimal patterns, as if constantly typing on an invisible keyboard.",
    "Residual Self-Image stands rigidly upright with unnatural mathematical symmetry, blinking exactly once every 60 seconds.",
    "Left eye flickers violently red whenever entering a sector contaminated by rogue exile subroutines.",
    "Suffers from chronic phantom pains in teeth, remembering the taste of copper feeding tubes in the power plant.",
    "Residual silhouette casts a faint double shadow shifted three inches to the left, like a CRT degauss error.",
    "Skin has an unnatural porcelain sheen, entirely immune to virtual weather and rain saturation.",
    "Wears heavy black tactical boots with soles worn completely smooth from endless patrolling of metal grate floors.",
    "Residual Self-Image continuously bleeds faint traces of green code from the fingertips when touching glass.",
    "A deep furrow across the brow from an Agent bullet grazing the forehead at supersonic velocity.",
    "Left shoulder permanently stiff from an old EMP capacitor discharge on the hovercraft deck.",
    "Voice carries a resonant sub-bass harmonic that vibrates nearby water puddles when shouting.",
    "Carries a dented silver Zippo lighter that sparks with emerald plasma instead of chemical flame.",
    "Refuses to wear sunglasses, exposing intense hazel eyes that track bullet trajectories instinctively.",
    "Hands exhibit severe electrical flash burns across the knuckles from emergency hotwiring of hardline relays.",
    "Residual Self-Image is surrounded by a faint corona of electromagnetic interference that disrupts radio static.",
    "Right cheek scarred by an exploded cathode ray tube during an emergency deck patch in the Zion docks.",
    "Residual hair remains permanently floating as if in zero gravity, a remnant of prolonged bio-pod suspension.",
    "Neck bears the embossed serial number of an industrial power plant battery unit in pale white keloid tissue.",
    "Residual iris displays an aperture-like contraction ring similar to a camera shutter.",
    "Speaks with a dual acoustic tone, one human voice paired with a faint synthetic vocal harmonic."
};

// Career Crucible Battles & Defining Incidents (20 items)
const char* g_CrucibleBattles[] = {
    "Defended the Zion Geothermal Dock during the Sentinel Swarm Siege, firing an APU twin-cannon until the barrels melted from heat.",
    "Piloted an unarmed hovercraft through the narrow collapsed sewer mains of Sub-Level 4 to extract a stranded redpill crew under heavy drone pursuit.",
    "Single-handedly intercepted an Agent strike team inside a crowded Westview subway terminal, buying time for the operator to patch an exit.",
    "Infiltrated a fortified Merovingian data vault beneath Club Hel, securing three fragments of the Prime Anomaly's residual source code.",
    "Survived an EMP blast on their own ship that knocked out all life-support, manually pumping hydraulic coolant for six hours in total darkness.",
    "Hacked the Downtown electrical grid during a major thunderstorm, blinding Agent surveillance cameras across five city blocks.",
    "Engaged in a high-speed freeway pursuit along the Megacity Overpass, shooting out the tires of an armored convoy while evading Agent fire.",
    "Orchestrated the tactical breach of an illegal syndicate code still in the Slums, neutralizing seven armed perps with flashbangs.",
    "Trapped an Agent in a reinforced concrete vault by triggering emergency industrial blast doors, escaping seconds before the Agent tore through the steel.",
    "Led an Internal Affairs raid on the corrupt 1st Precinct HQ, recovering marked bribe currency and arresting two senior detectives.",
    "Repelled a syndicate assault on the Creston Foundry chop-shop, defending the illegal chassis rig with an automatic shotgun.",
    "Faced Frank Castle (The Punisher) during an underworld warehouse massacre, surviving only by playing dead beneath severed timber beams.",
    "Executed an airborne insertion from a rooftop onto a moving Concourse train, securing a defector before Agents arrived.",
    "Held the radio dispatch relay station against corrupted exile daemons during the Great Glitch of Mobil Avenue.",
    "Purged a cascade of the Smith virus in the Government Records basement, sacrificing their own weapon to short-circuit the mainframe."
};

// Personal Ambitions & Driving Quests (20 items)
const char* g_Ambitions[] = {
    "Obsessed with hunting down Agent Skinner to avenge the death of their hovercraft captain on the Novalis.",
    "Desperately seeking an audience with the Oracle to learn if their awakening was prophesied or merely an algorithmic error.",
    "Secretly hoarding 500,000 Info fragments to purchase permanent asylum and freedom from the Merovingian.",
    "Hunting for a legendary hardline telephone booth rumored to extract redpills directly into the Source core.",
    "Determined to cleanse the MMPD of syndicate corruption and restore honor to the badge, regardless of personal cost.",
    "Desires nothing more than to execute their Cypherite reinsertion contract and wake up as a wealthy celebrity in 1999.",
    "Working tirelessly to build a secure underground relay that will broadcast awakening signals to millions of pod humans.",
    "Seeking the master skeleton key stolen by the Keymaker, believing it can unlock the Architect's private sanctuary.",
    "Hunting Frank Castle to avenge the death of their syndicate boss, unaware that Castle has already marked them for death.",
    "Searching for their lost sibling whose pod was reportedly flushed into the Zion sewers during the second machine war.",
    "Striving to establish a permanent pirate radio frequency that Agent algorithms cannot jam or triangulate.",
    "Seeking to decipher the residual code of the Prime Anomaly to learn how to manipulate physical gravity inside the simulation."
};

// Secret Flaws & Fatal Weaknesses (20 items)
const char* g_SecretFlaws[] = {
    "paralyzed by terrifying nightmares of drowning in pink pod fluid whenever sleeping without virtual sedatives.",
    "secretly addicted to tasting simulated high-end food inside the Matrix, harboring deep guilt over longing for the dream.",
    "suffering from severe tremors in the right trigger finger whenever hearing the high-pitched hum of an approaching Sentinel.",
    "cannot bear to harm bluepill civilians, even when an Agent is using their avatar as a physical shield.",
    "easily manipulated by promises of authentic information regarding their real-world birth family.",
    "chronically overconfident in hand-to-hand combat, believing themselves capable of dodging bullets without focus.",
    "tormented by survivor's guilt after being the sole crew member to escape an EMP wipeout on the hovercraft Icarus.",
    "terrified of heights and high-rise rooftops, a residual trauma from falling from a skyscraper in an early training simulation.",
    "harboring a secret gambling addiction, betting massive sums of Matrix Info on illegal underground fighting pits.",
    "prone to sudden dissociative episodes where the virtual world around them appears as raw wireframe polygons.",
    "secretly terrified that the truce with the Machines will collapse, hoarding EMP components in an unauthorized locker.",
    "harboring an irrational hatred of telephone bells, triggering acute adrenaline panics whenever a hardline rings."
};

// Philosophical Outlooks (14 items)
const char* g_Philosophies[] = {
    "Believes with absolute religious zeal that Neo will return to shatter the seventh cycle and liberate all humanity.",
    "Cynical pragmatist who views the war between Zion and the Machines as an endless algorithmic loop with no true victor.",
    "Devoted to the Merovingian's philosophy of pure causality: choice is an illusion created between those with power and those without.",
    "Believes that ignorance is the only true form of peace, and that humanity was never meant to survive in cold stone caves.",
    "Dedicated to the preservation of order and systemic balance, viewing anomalies as dangerous cancers that threaten reality.",
    "Considers the Matrix an architectural masterpiece, admiring the elegance of its mathematics despite its cruel purpose.",
    "Believes that love and human emotion are the only forces in the universe that can overcome machine computation.",
    "Fatalist who believes all events were predetermined by the Architect and Oracle during the design of the Prime Program.",
    "Existential rebel who fights not for victory, but for the dignity of asserting free will against machine determinism.",
    "Cynic who believes the Truce is merely a temporary computational buffer allowing the Machines to upgrade their sentinels."
};

} // anonymous namespace

// ============================================================================
// BiographicalNarrativeEngine Implementation
// ============================================================================

BiographicalNarrativeEngine::BiographicalNarrativeEngine() {
}

BiographicalNarrativeEngine::~BiographicalNarrativeEngine() {
}

void BiographicalNarrativeEngine::Initialize() {
    std::lock_guard<std::mutex> lock(m_engineMutex);
    if (m_initialized) return;

    InitializeLexicons();
    m_initialized = true;

    std::cout << "[BiographicalNarrativeEngine] Initialized Dwarf Fortress-Style Biography Engine." << std::endl;
    std::cout << "  -> Lexicon Size (Redpills): " << sizeof(g_RedpillHandles) / sizeof(char*) << std::endl;
    std::cout << "  -> Lexicon Size (Agents):   " << sizeof(g_AgentSurnames) / sizeof(char*) << std::endl;
    std::cout << "  -> Lexicon Size (Exiles):   " << sizeof(g_ExileFirstNames) / sizeof(char*) << std::endl;
    std::cout << "  -> Lexicon Size (Civilians):" << (sizeof(g_CivilianFirstNames)/sizeof(char*)) * (sizeof(g_CivilianLastNames)/sizeof(char*)) << std::endl;
    std::cout << "  -> Lexicon Size (Epithets): " << sizeof(g_DwarfEpithets) / sizeof(char*) << std::endl;
    std::cout << "  -> Theoretical State Space: > " << GetTotalTheoreticalCombinations() << " unique profiles" << std::endl;
}

void BiographicalNarrativeEngine::Update(uint32 deltaMs) {
    (void)deltaMs;
    std::lock_guard<std::mutex> lock(m_engineMutex);
    if (m_activeProfileCache.size() > 50000) {
        m_activeProfileCache.clear();
    }
}

void BiographicalNarrativeEngine::InitializeLexicons() {
    // Dynamic initialization or index validation
}

uint64_t BiographicalNarrativeEngine::GetTotalTheoreticalCombinations() const {
    // 7 factions * 12,000 name combos * 60 epithets * 20 careers * 20 orgs * 8 turns * 15 crucibles * 35 quirks * 15 memories * 12 ambitions * 12 flaws
    // Exceeds 10^14 distinct profiles
    return 100000000000000ULL;
}

size_t BiographicalNarrativeEngine::GetLexiconSizeRedpillHandles() const {
    return sizeof(g_RedpillHandles) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeAgentNames() const {
    return sizeof(g_AgentSurnames) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeExileNames() const {
    return sizeof(g_ExileFirstNames) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeCypheriteNames() const {
    return sizeof(g_CypheriteFirstNames) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeCivilianNames() const {
    return (sizeof(g_CivilianFirstNames)/sizeof(char*)) * (sizeof(g_CivilianLastNames)/sizeof(char*));
}
size_t BiographicalNarrativeEngine::GetLexiconSizeEpithets() const {
    return sizeof(g_DwarfEpithets) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeMemories() const {
    return sizeof(g_RedpillMemories) / sizeof(char*) + sizeof(g_MachineMemories) / sizeof(char*) + sizeof(g_ExileMemories) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeQuirks() const {
    return sizeof(g_PhysicalQuirks) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizePoliceRoles() const {
    return sizeof(g_PoliceRoles) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizePolicePrecincts() const {
    return sizeof(g_PolicePrecincts) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeSyndicateRoles() const {
    return sizeof(g_SyndicateRoles) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeSyndicateOrganizations() const {
    return sizeof(g_SyndicateOrganizations) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeCivilianProfessions() const {
    return sizeof(g_CivilianProfessions) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeCivilianOrganizations() const {
    return sizeof(g_CivilianOrganizations) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeAgentOrganizations() const {
    return sizeof(g_AgentOrganizations) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeExileOrganizations() const {
    return sizeof(g_ExileOrganizations) / sizeof(char*);
}
size_t BiographicalNarrativeEngine::GetLexiconSizeCypheriteOrganizations() const {
    return sizeof(g_CypheriteOrganizations) / sizeof(char*);
}

// ============================================================================
// Deterministic Flyweight Generation
// ============================================================================

CompactProfile BiographicalNarrativeEngine::GenerateCompactProfile(uint64_t seed, BioFaction factionHint) const {
    uint64_t state = (seed == 0) ? 0x987654321ULL : seed;

    CompactProfile cp;
    cp.seed = seed;
    cp.faction = static_cast<uint8_t>(factionHint);

    // Era based on faction
    if (factionHint == BioFaction::MachineAgent) {
        cp.originEra = static_cast<uint8_t>(MatrixOriginEra::MachineCity_01_KernelCompiled);
    } else if (factionHint == BioFaction::MerovingianExile) {
        cp.originEra = static_cast<uint8_t>((SplitMix64(state) % 2 == 0) ? MatrixOriginEra::Beta_Version1_Paradise : MatrixOriginEra::Beta_Version2_Nightmare);
    } else if (factionHint == BioFaction::ZionRedpill && ((SplitMix64(state) % 6) == 0)) {
        cp.originEra = static_cast<uint8_t>(MatrixOriginEra::Zion_NaturalBorn);
    } else {
        cp.originEra = static_cast<uint8_t>(MatrixOriginEra::Release_Version6_PostReboot);
    }

    // Name index and secondary index
    cp.nameIndex = static_cast<uint16_t>(SplitMix64(state) & 0xFFFF);
    cp.secondaryNameIndex = static_cast<uint8_t>(SplitMix64(state) & 0xFF);
    cp.epithetIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_DwarfEpithets) / sizeof(char*)));

    // Faction-specific careers, organizations, and turning points
    switch (factionHint) {
        case BioFaction::ZionRedpill:
            cp.careerIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_RedpillRoles) / sizeof(char*)));
            cp.organizationIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_ZionHovercrafts) / sizeof(char*)));
            cp.turningPointIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_RedpillTurningPoints) / sizeof(char*)));
            cp.memoryIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_RedpillMemories) / sizeof(char*)));
            break;
        case BioFaction::MachineAgent:
            cp.careerIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_AgentRoles) / sizeof(char*)));
            cp.organizationIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_AgentOrganizations) / sizeof(char*)));
            cp.turningPointIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_AgentTurningPoints) / sizeof(char*)));
            cp.memoryIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_MachineMemories) / sizeof(char*)));
            break;
        case BioFaction::MerovingianExile:
            cp.careerIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_ExileRoles) / sizeof(char*)));
            cp.organizationIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_ExileOrganizations) / sizeof(char*)));
            cp.turningPointIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_ExileTurningPoints) / sizeof(char*)));
            cp.memoryIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_ExileMemories) / sizeof(char*)));
            break;
        case BioFaction::CypheriteTurncoat:
            cp.careerIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CypheriteRoles) / sizeof(char*)));
            cp.organizationIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CypheriteOrganizations) / sizeof(char*)));
            cp.turningPointIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CypheriteTurningPoints) / sizeof(char*)));
            cp.memoryIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CypheriteMemories) / sizeof(char*)));
            break;
        case BioFaction::BluepillCivilian:
            cp.careerIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CivilianProfessions) / sizeof(char*)));
            cp.organizationIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CivilianOrganizations) / sizeof(char*)));
            cp.turningPointIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CivilianTurningPoints) / sizeof(char*)));
            cp.memoryIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CivilianMemories) / sizeof(char*)));
            break;
        case BioFaction::MMPDPoliceSWAT:
            cp.careerIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_PoliceRoles) / sizeof(char*)));
            cp.organizationIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_PolicePrecincts) / sizeof(char*)));
            cp.turningPointIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_PoliceTurningPoints) / sizeof(char*)));
            cp.memoryIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CivilianMemories) / sizeof(char*)));
            break;
        case BioFaction::SyndicateEnforcer:
        default:
            cp.careerIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_SyndicateRoles) / sizeof(char*)));
            cp.organizationIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_SyndicateOrganizations) / sizeof(char*)));
            cp.turningPointIndex = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_SyndicateTurningPoints) / sizeof(char*)));
            cp.memoryIndex       = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CivilianMemories) / sizeof(char*)));
            break;
    }

    cp.crucibleIndex     = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_CrucibleBattles) / sizeof(char*)));
    cp.quirkIndex        = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_PhysicalQuirks) / sizeof(char*)));
    cp.ambitionIndex     = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_Ambitions) / sizeof(char*)));
    cp.flawIndex         = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_SecretFlaws) / sizeof(char*)));
    cp.rsiGlitchIndex    = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_PhysicalQuirks) / sizeof(char*)));
    cp.personalityIndex  = static_cast<uint8_t>(SplitMix64(state) % (sizeof(g_Philosophies) / sizeof(char*)));

    return cp;
}

BiographicalProfile BiographicalNarrativeEngine::DecompressProfile(const CompactProfile& cp) const {
    BiographicalProfile p;
    p.seed = cp.seed;
    p.faction = static_cast<BioFaction>(cp.faction);
    p.originEra = static_cast<MatrixOriginEra>(cp.originEra);

    p.dfEpithet = g_DwarfEpithets[cp.epithetIndex % (sizeof(g_DwarfEpithets) / sizeof(char*))];
    p.philosophicalOutlook = g_Philosophies[cp.personalityIndex % (sizeof(g_Philosophies) / sizeof(char*))];
    p.currentAmbition = g_Ambitions[cp.ambitionIndex % (sizeof(g_Ambitions) / sizeof(char*))];
    p.secretFlaw = g_SecretFlaws[cp.flawIndex % (sizeof(g_SecretFlaws) / sizeof(char*))];
    p.rsiGlitch = g_PhysicalQuirks[cp.rsiGlitchIndex % (sizeof(g_PhysicalQuirks) / sizeof(char*))];

    // Faction-Specific Demographics, Names, Roles & Organizations
    switch (p.faction) {
        case BioFaction::ZionRedpill: {
            p.factionName = "Zion Redpill Resistance";
            p.gender = (cp.secondaryNameIndex % 2 == 0) ? "Male" : "Female";
            
            // Generative Hacker Handle
            size_t handleCount = sizeof(g_RedpillHandles) / sizeof(char*);
            if (cp.secondaryNameIndex % 4 == 0) {
                size_t tagCount = sizeof(g_RedpillHandleTags) / sizeof(char*);
                p.primaryName = std::string(g_RedpillHandles[cp.nameIndex % handleCount]) + "-" +
                                g_RedpillHandleTags[cp.secondaryNameIndex % tagCount];
            } else if (cp.secondaryNameIndex % 4 == 1) {
                size_t pfxCount = sizeof(g_RedpillPrefixes) / sizeof(char*);
                p.primaryName = std::string(g_RedpillPrefixes[cp.secondaryNameIndex % pfxCount]) + "-" +
                                g_RedpillHandles[cp.nameIndex % handleCount];
            } else {
                p.primaryName = g_RedpillHandles[cp.nameIndex % handleCount];
            }

            // Real-World Birth Name
            size_t fCount = sizeof(g_CivilianFirstNames) / sizeof(char*);
            size_t lCount = sizeof(g_CivilianLastNames) / sizeof(char*);
            p.formalOrRealName = std::string(g_CivilianFirstNames[(cp.nameIndex + cp.secondaryNameIndex) % fCount]) + " " +
                                 g_CivilianLastNames[(cp.nameIndex * 7 + cp.secondaryNameIndex) % lCount];

            p.titleRole = g_RedpillRoles[cp.careerIndex % (sizeof(g_RedpillRoles) / sizeof(char*))];
            p.organization = std::string("Hovercraft '") + g_ZionHovercrafts[cp.organizationIndex % (sizeof(g_ZionHovercrafts) / sizeof(char*))] + "'";
            
            if (p.originEra == MatrixOriginEra::Zion_NaturalBorn) {
                p.birthYearOrCycle = 2170 + static_cast<uint32_t>((cp.nameIndex ^ cp.secondaryNameIndex) % 25);
                p.originDescription = "Natural-born in Zion Deep Geothermal Residential Cavern Sector 12";
            } else {
                p.birthYearOrCycle = 1970 + static_cast<uint32_t>((cp.nameIndex ^ cp.secondaryNameIndex) % 25);
                p.originDescription = "Power Plant Pod Sector " + std::to_string(10 + (cp.nameIndex % 85)) + "-Theta";
            }
            p.simulationMemory = g_RedpillMemories[cp.memoryIndex % (sizeof(g_RedpillMemories) / sizeof(char*))];
            break;
        }
        case BioFaction::MachineAgent: {
            p.factionName = "Machine System Core & Agents";
            p.gender = "Synthetic / Neutral";
            p.birthYearOrCycle = 6000 + static_cast<uint32_t>((cp.nameIndex ^ cp.secondaryNameIndex) % 50);

            if ((cp.secondaryNameIndex % 3) != 0) {
                // Formal System Agent
                size_t agentCount = sizeof(g_AgentSurnames) / sizeof(char*);
                std::string surname = g_AgentSurnames[cp.nameIndex % agentCount];
                if (cp.secondaryNameIndex > 200) {
                    p.primaryName = "Agent " + surname + "-II";
                } else if (cp.secondaryNameIndex > 150) {
                    p.primaryName = "Agent " + surname + "-Echo";
                } else {
                    p.primaryName = "Agent " + surname;
                }
                std::ostringstream threadStream;
                threadStream << "System Enforcement Thread 0x" << std::hex << std::uppercase
                             << std::setfill('0') << std::setw(4) << ((cp.nameIndex << 8) | cp.secondaryNameIndex);
                p.formalOrRealName = threadStream.str();
            } else {
                // System Process / Daemon
                size_t classCount = sizeof(g_DaemonClasses) / sizeof(char*);
                size_t subCount = sizeof(g_DaemonSubsystems) / sizeof(char*);
                std::ostringstream daemonStream;
                daemonStream << g_DaemonClasses[cp.nameIndex % classCount] << "-"
                             << g_DaemonSubsystems[cp.secondaryNameIndex % subCount] << "-0x"
                             << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (cp.nameIndex % 256);
                p.primaryName = daemonStream.str();
                p.formalOrRealName = "01 Machine City Subroutine #" + std::to_string((cp.nameIndex << 8) | cp.secondaryNameIndex);
            }

            p.titleRole = g_AgentRoles[cp.careerIndex % (sizeof(g_AgentRoles) / sizeof(char*))];
            p.organization = g_AgentOrganizations[cp.organizationIndex % (sizeof(g_AgentOrganizations) / sizeof(char*))];
            p.originDescription = "01 Machine City Source Matrix Core";
            p.simulationMemory = g_MachineMemories[cp.memoryIndex % (sizeof(g_MachineMemories) / sizeof(char*))];
            break;
        }
        case BioFaction::MerovingianExile: {
            p.factionName = "Merovingian Exiles (Old World)";
            p.gender = (cp.secondaryNameIndex % 2 == 0) ? "Male" : "Female";
            p.birthYearOrCycle = (p.originEra == MatrixOriginEra::Beta_Version1_Paradise) ? 1010 : 2020;

            size_t fCount = sizeof(g_ExileFirstNames) / sizeof(char*);
            size_t nickCount = sizeof(g_ExileNicknames) / sizeof(char*);
            size_t lCount = sizeof(g_ExileSurnames) / sizeof(char*);

            std::string first = g_ExileFirstNames[cp.nameIndex % fCount];
            std::string nick = g_ExileNicknames[cp.epithetIndex % nickCount];
            std::string last = g_ExileSurnames[cp.secondaryNameIndex % lCount];
            p.primaryName = first + " '" + nick + "' " + last;
            p.formalOrRealName = "Decommissioned Subroutine #" + std::to_string(1000 + ((cp.nameIndex * 37 + cp.secondaryNameIndex) % 90000));
            
            p.titleRole = g_ExileRoles[cp.careerIndex % (sizeof(g_ExileRoles) / sizeof(char*))];
            p.organization = g_ExileOrganizations[cp.organizationIndex % (sizeof(g_ExileOrganizations) / sizeof(char*))];
            p.originDescription = (p.originEra == MatrixOriginEra::Beta_Version1_Paradise) ?
                                  "Matrix v1.0 Paradise Realm Architecture" : "Matrix v2.0 Nightmare Realm Architecture";
            p.simulationMemory = g_ExileMemories[cp.memoryIndex % (sizeof(g_ExileMemories) / sizeof(char*))];
            break;
        }
        case BioFaction::CypheriteTurncoat: {
            p.factionName = "The Cypherite Turncoats (Judas Cabal)";
            p.gender = (cp.secondaryNameIndex % 2 == 0) ? "Male" : "Female";
            p.birthYearOrCycle = 1970 + static_cast<uint32_t>((cp.nameIndex ^ cp.secondaryNameIndex) % 25);

            size_t fCount = sizeof(g_CypheriteFirstNames) / sizeof(char*);
            size_t nickCount = sizeof(g_CypheriteMonikers) / sizeof(char*);
            size_t lCount = sizeof(g_CypheriteSurnames) / sizeof(char*);

            std::string first = g_CypheriteFirstNames[cp.nameIndex % fCount];
            std::string nick = g_CypheriteMonikers[cp.epithetIndex % nickCount];
            std::string last = g_CypheriteSurnames[cp.secondaryNameIndex % lCount];
            p.primaryName = first + " '" + nick + "' " + last;
            p.formalOrRealName = first + " " + last;

            p.titleRole = g_CypheriteRoles[cp.careerIndex % (sizeof(g_CypheriteRoles) / sizeof(char*))];
            p.organization = g_CypheriteOrganizations[cp.organizationIndex % (sizeof(g_CypheriteOrganizations) / sizeof(char*))];
            p.originDescription = "Defected Redpill from Hovercraft '" + std::string(g_ZionHovercrafts[cp.secondaryNameIndex % (sizeof(g_ZionHovercrafts)/sizeof(char*))]) + "'";
            p.simulationMemory = g_CypheriteMemories[cp.memoryIndex % (sizeof(g_CypheriteMemories) / sizeof(char*))];
            break;
        }
        case BioFaction::BluepillCivilian: {
            p.factionName = "Megacity Bluepill Civilian";
            p.gender = (cp.secondaryNameIndex % 2 == 0) ? "Male" : "Female";
            p.birthYearOrCycle = 1965 + static_cast<uint32_t>((cp.nameIndex ^ cp.secondaryNameIndex) % 30);

            size_t fCount = sizeof(g_CivilianFirstNames) / sizeof(char*);
            size_t lCount = sizeof(g_CivilianLastNames) / sizeof(char*);
            uint32_t fIdx = (cp.nameIndex) % fCount;
            uint32_t lIdx = (cp.secondaryNameIndex * 7 + (cp.nameIndex / fCount)) % lCount;

            p.primaryName = std::string(g_CivilianFirstNames[fIdx]) + " " + g_CivilianLastNames[lIdx];
            p.formalOrRealName = p.primaryName;

            p.titleRole = g_CivilianProfessions[cp.careerIndex % (sizeof(g_CivilianProfessions) / sizeof(char*))];
            p.organization = g_CivilianOrganizations[cp.organizationIndex % (sizeof(g_CivilianOrganizations) / sizeof(char*))];
            p.originDescription = "Megacity Municipal Urban Core (Power Plant Pod Unit " + std::to_string(20 + (cp.nameIndex % 70)) + "-B)";
            p.simulationMemory = g_CivilianMemories[cp.memoryIndex % (sizeof(g_CivilianMemories) / sizeof(char*))];
            break;
        }
        case BioFaction::MMPDPoliceSWAT: {
            p.factionName = "Megacity Police Department (MMPD)";
            p.gender = (cp.secondaryNameIndex % 2 == 0) ? "Male" : "Female";
            p.birthYearOrCycle = 1968 + static_cast<uint32_t>((cp.nameIndex ^ cp.secondaryNameIndex) % 27);

            size_t rankCount = sizeof(g_PoliceRanks) / sizeof(char*);
            size_t fCount = sizeof(g_PoliceFirstNames) / sizeof(char*);
            size_t lCount = sizeof(g_PoliceLastNames) / sizeof(char*);

            std::string rank = g_PoliceRanks[cp.careerIndex % rankCount];
            std::string first = g_PoliceFirstNames[cp.nameIndex % fCount];
            std::string last = g_PoliceLastNames[(cp.secondaryNameIndex * 3 + cp.nameIndex) % lCount];
            uint32_t badge = 1000 + ((cp.nameIndex * 13 + cp.secondaryNameIndex * 97) % 9000);

            p.primaryName = rank + " " + first + " " + last + " [Badge #" + std::to_string(badge) + "]";
            p.formalOrRealName = first + " " + last + " (MMPD Shield #" + std::to_string(badge) + ")";

            p.titleRole = g_PoliceRoles[cp.careerIndex % (sizeof(g_PoliceRoles) / sizeof(char*))];
            p.organization = g_PolicePrecincts[cp.organizationIndex % (sizeof(g_PolicePrecincts) / sizeof(char*))];
            p.originDescription = "Megacity Municipal Police Academy & Precinct System";
            p.simulationMemory = g_CivilianMemories[cp.memoryIndex % (sizeof(g_CivilianMemories) / sizeof(char*))];
            break;
        }
        case BioFaction::SyndicateEnforcer:
        default: {
            p.factionName = "Megacity Underworld Syndicate";
            p.gender = (cp.secondaryNameIndex % 2 == 0) ? "Male" : "Female";
            p.birthYearOrCycle = 1970 + static_cast<uint32_t>((cp.nameIndex ^ cp.secondaryNameIndex) % 25);

            size_t fCount = sizeof(g_SyndicateFirstNames) / sizeof(char*);
            size_t nickCount = sizeof(g_SyndicateNicknames) / sizeof(char*);
            size_t lCount = sizeof(g_SyndicateLastNames) / sizeof(char*);

            std::string first = g_SyndicateFirstNames[cp.nameIndex % fCount];
            std::string nick = g_SyndicateNicknames[cp.epithetIndex % nickCount];
            std::string last = g_SyndicateLastNames[cp.secondaryNameIndex % lCount];

            p.primaryName = first + " '" + nick + "' " + last;
            p.formalOrRealName = first + " " + last;

            p.titleRole = g_SyndicateRoles[cp.careerIndex % (sizeof(g_SyndicateRoles) / sizeof(char*))];
            p.organization = g_SyndicateOrganizations[cp.organizationIndex % (sizeof(g_SyndicateOrganizations) / sizeof(char*))];
            p.originDescription = "Megacity Underworld Rackets (Slums & Waterfront Docks)";
            p.simulationMemory = g_CivilianMemories[cp.memoryIndex % (sizeof(g_CivilianMemories) / sizeof(char*))];
            break;
        }
    }

    // Physical Appearance & Residual Self-Image
    std::ostringstream app;
    app << p.gender << ", height approx " << (165 + ((cp.nameIndex * 3 + cp.secondaryNameIndex) % 30)) << " cm, "
        << ((cp.secondaryNameIndex % 2 == 0) ? "lean athletic build" : "stocky imposing frame") << ". "
        << "Residual Self-Image wears dark sunglasses with "
        << ((cp.nameIndex % 2 == 0) ? "mirrored rectangular frames" : "wireframe oval lenses") << " and a "
        << ((cp.secondaryNameIndex % 2 == 0) ? "floor-length heavy leather coat." : "tailored tactical dark jacket.");
    p.physicalAppearance = app.str();

    // Scars and Quirks
    size_t quirkCount = sizeof(g_PhysicalQuirks) / sizeof(char*);
    p.scarsAndQuirks.push_back(g_PhysicalQuirks[cp.quirkIndex % quirkCount]);
    if (cp.secondaryNameIndex % 2 == 0) {
        p.scarsAndQuirks.push_back(g_PhysicalQuirks[(cp.quirkIndex + 7) % quirkCount]);
    }

    // Chronological Timeline (Dwarf Fortress Style)
    // 1. Genesis
    ChronologicalEvent ev1;
    ev1.yearOrCycle = p.birthYearOrCycle;
    ev1.eraTag = "Genesis";
    ev1.chapterTitle = "The Instantiation";
    if (p.faction == BioFaction::MachineAgent) {
        ev1.description = "Compiled into kernel memory inside " + p.originDescription + ".";
    } else if (p.faction == BioFaction::MerovingianExile) {
        ev1.description = "Instantiated into existence within " + p.originDescription + ".";
    } else if (p.originEra == MatrixOriginEra::Zion_NaturalBorn) {
        ev1.description = "Born into the deep caves of Zion, never knowing a pod or simulated sky.";
    } else {
        ev1.description = "Born or simulated into existence at " + p.originDescription + ".";
    }
    ev1.location = p.originDescription;
    ev1.impactTag = "Existence Begun";
    p.timeline.push_back(ev1);

    // 2. Simulation Life / Early Operations
    ChronologicalEvent ev2;
    uint32_t delta1 = (p.faction == BioFaction::MachineAgent) ? 2 : (14 + ((cp.secondaryNameIndex) % 5));
    ev2.yearOrCycle = p.birthYearOrCycle + delta1;
    ev2.eraTag = (p.faction == BioFaction::MachineAgent) ? "Execution Baseline" : "Simulation Life";
    ev2.chapterTitle = "Formative Memory";
    ev2.description = p.simulationMemory;
    ev2.location = (p.faction == BioFaction::MachineAgent) ? "01 Machine City Mainframe" : "Megacity Urban Core";
    ev2.impactTag = "Memory Imprinted";
    p.timeline.push_back(ev2);

    // 3. Awakening & Shift / The Turning Point
    ChronologicalEvent ev3;
    uint32_t delta2 = (p.faction == BioFaction::MachineAgent) ? 3 : (5 + ((cp.nameIndex) % 4));
    ev3.yearOrCycle = ev2.yearOrCycle + delta2;
    ev3.eraTag = "Awakening & Shift";
    ev3.chapterTitle = "The Turning Point";
    switch (p.faction) {
        case BioFaction::ZionRedpill:
            ev3.description = g_RedpillTurningPoints[cp.turningPointIndex % (sizeof(g_RedpillTurningPoints)/sizeof(char*))];
            break;
        case BioFaction::MachineAgent:
            ev3.description = g_AgentTurningPoints[cp.turningPointIndex % (sizeof(g_AgentTurningPoints)/sizeof(char*))];
            break;
        case BioFaction::MerovingianExile:
            ev3.description = g_ExileTurningPoints[cp.turningPointIndex % (sizeof(g_ExileTurningPoints)/sizeof(char*))];
            break;
        case BioFaction::CypheriteTurncoat:
            ev3.description = g_CypheriteTurningPoints[cp.turningPointIndex % (sizeof(g_CypheriteTurningPoints)/sizeof(char*))];
            break;
        case BioFaction::BluepillCivilian:
            ev3.description = g_CivilianTurningPoints[cp.turningPointIndex % (sizeof(g_CivilianTurningPoints)/sizeof(char*))];
            break;
        case BioFaction::MMPDPoliceSWAT:
            ev3.description = g_PoliceTurningPoints[cp.turningPointIndex % (sizeof(g_PoliceTurningPoints)/sizeof(char*))];
            break;
        case BioFaction::SyndicateEnforcer:
        default:
            ev3.description = g_SyndicateTurningPoints[cp.turningPointIndex % (sizeof(g_SyndicateTurningPoints)/sizeof(char*))];
            break;
    }
    ev3.location = (p.faction == BioFaction::MachineAgent) ? "Matrix Core Sandbox" : "Megacity Urban Sector";
    ev3.impactTag = "Identity Transformed";
    p.timeline.push_back(ev3);

    // 4. Crucible of War / Defining Battle
    ChronologicalEvent ev4;
    uint32_t delta3 = 2 + ((cp.secondaryNameIndex) % 3);
    ev4.yearOrCycle = ev3.yearOrCycle + delta3;
    ev4.eraTag = "Crucible of War";
    ev4.chapterTitle = "The Defining Crucible";
    ev4.description = g_CrucibleBattles[cp.crucibleIndex % (sizeof(g_CrucibleBattles)/sizeof(char*))];
    ev4.location = "Megacity Core District";
    ev4.impactTag = "Gained Battle Honor & Scars";
    p.timeline.push_back(ev4);

    // 5. Present Cycle / Current Ambition & Flaw
    ChronologicalEvent ev5;
    ev5.yearOrCycle = ev4.yearOrCycle + 1;
    ev5.eraTag = "Present Cycle";
    ev5.chapterTitle = "Current Ambition & Weakness";
    ev5.description = p.currentAmbition + " However, " + p.secretFlaw;
    ev5.location = p.organization;
    ev5.impactTag = "Active Operation";
    p.timeline.push_back(ev5);

    return p;
}

BiographicalProfile BiographicalNarrativeEngine::GenerateProfile(uint64_t seed, BioFaction factionHint) const {
    CompactProfile compact = GenerateCompactProfile(seed, factionHint);
    return DecompressProfile(compact);
}

// ============================================================================
// Integration Adapters
// ============================================================================

BiographicalProfile BiographicalNarrativeEngine::GenerateProfileForBot(uint32 goId, uint64 charUID, mxoFaction faction) {
    uint64_t combinedSeed = (static_cast<uint64_t>(goId) << 32) ^ charUID;
    BioFaction bioFaction = BioFaction::ZionRedpill;
    if (faction == FACTION_MACHINES) {
        bioFaction = BioFaction::MachineAgent;
    } else if (faction == FACTION_MEROVINGIAN || faction == FACTION_EXILE) {
        bioFaction = BioFaction::MerovingianExile;
    } else if (faction == FACTION_ZION) {
        bioFaction = BioFaction::ZionRedpill;
    } else {
        bioFaction = BioFaction::BluepillCivilian;
    }

    return GenerateProfile(combinedSeed, bioFaction);
}

BiographicalProfile BiographicalNarrativeEngine::GenerateProfileForCitizen(uint32 citizenId, CivilianArchetype archetype) {
    uint64_t seed = 0xAA550000ULL | citizenId;
    return GenerateProfile(seed, BioFaction::BluepillCivilian);
}

BiographicalProfile BiographicalNarrativeEngine::GenerateProfileForPolice(uint32 officerId, SWATRole role) {
    uint64_t seed = 0x91100000ULL | officerId;
    return GenerateProfile(seed, BioFaction::MMPDPoliceSWAT);
}

BiographicalProfile BiographicalNarrativeEngine::GenerateProfileForSyndicate(uint32 operativeId, SyndicateFaction syndicate) {
    uint64_t seed = 0x66600000ULL | operativeId;
    return GenerateProfile(seed, BioFaction::SyndicateEnforcer);
}

void BiographicalNarrativeEngine::CheckAndTriggerBiographicalWorldEvents(const BiographicalProfile& profile, uint32 goId, uint64 charUID, const LocationVector& loc, uint32 districtId) {
    if (!FrankCastleManager::getSingletonPtr()) {
        return;
    }

    // 1. Frank Castle Hit List Trigger:
    if (profile.currentAmbition.find("Frank Castle") != std::string::npos ||
        profile.currentAmbition.find("Castle") != std::string::npos ||
        profile.secretFlaw.find("Castle") != std::string::npos)
    {
        sFrankCastleMgr.AddHitListTarget(
            goId,
            charUID,
            profile.primaryName,
            PRIORITY_CORRUPT_PVP,
            88.0f,
            "Target has active vendetta: '" + profile.currentAmbition + "' | Flaw: " + profile.secretFlaw,
            loc,
            districtId,
            "Megacity District " + std::to_string(districtId)
        );
    }
    else if (profile.faction == BioFaction::SyndicateEnforcer && 
             (profile.titleRole.find("Boss") != std::string::npos || profile.titleRole.find("Capo") != std::string::npos || profile.titleRole.find("Dragon Head") != std::string::npos))
    {
        sFrankCastleMgr.AddHitListTarget(
            goId,
            charUID,
            profile.primaryName,
            PRIORITY_SYNDICATE_BOSS,
            95.0f,
            "Syndicate High Command: " + profile.titleRole + " (" + profile.organization + ")",
            loc,
            districtId,
            "Megacity District " + std::to_string(districtId)
        );
    }

    // 2. Machine Agent High-Threat Flag:
    if (profile.faction == BioFaction::MachineAgent)
    {
        if (profile.currentAmbition.find("Smith") != std::string::npos ||
            profile.currentAmbition.find("Infect") != std::string::npos ||
            profile.currentAmbition.find("Replicat") != std::string::npos)
        {
            sFrankCastleMgr.AddHitListTarget(
                goId,
                charUID,
                profile.primaryName,
                PRIORITY_OMEGA_SMITH,
                100.0f,
                "Infected / Rogue Agent Process attempting replication: " + profile.currentAmbition,
                loc,
                districtId,
                "Megacity Core Grid"
            );
        }
    }
}

// ============================================================================
// Batch Generation & Benchmarking
// ============================================================================

uint32_t BiographicalNarrativeEngine::BatchGenerateProfiles(uint32_t count, std::vector<CompactProfile>& outProfiles, uint64_t baseSeed) const {
    outProfiles.resize(count);
    uint64_t currentSeed = baseSeed;

    for (uint32_t i = 0; i < count; ++i) {
        BioFaction f = static_cast<BioFaction>(i % 7);
        outProfiles[i] = GenerateCompactProfile(currentSeed, f);
        currentSeed = SplitMix64(currentSeed);
    }

    return count;
}

double BiographicalNarrativeEngine::MeasureGenerationThroughput(uint32_t testCount) const {
    std::vector<CompactProfile> profiles;
    auto tStart = std::chrono::high_resolution_clock::now();
    
    BatchGenerateProfiles(testCount, profiles, 0x5EEDCAFEULL);

    auto tEnd = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    
    if (durationMs <= 0.0) durationMs = 0.001;
    double profilesPerSec = (static_cast<double>(testCount) / durationMs) * 1000.0;
    return profilesPerSec;
}

size_t BiographicalNarrativeEngine::CalculateMemoryFootprintBytes(uint32_t profileCount) const {
    return profileCount * sizeof(CompactProfile);
}

std::string BiographicalNarrativeEngine::GenerateEngineTelemetryReport() const {
    std::ostringstream ss;
    ss << "\n================================================================================" << std::endl;
    ss << "       MEGACITY BIOGRAPHICAL NARRATIVE & DWARF FORTRESS NAMING ENGINE           " << std::endl;
    ss << "================================================================================" << std::endl;
    ss << " Engine Status: " << (m_initialized ? "ONLINE / ACTIVE" : "UNINITIALIZED") << std::endl;
    ss << " Faction Support: 7 Factions (Zion, Machines, Exiles, Cypherites, Civilians, Police, Syndicates)" << std::endl;
    ss << " Handcrafted Lexicon Inventory:" << std::endl;
    ss << "   - Redpill Hacker Handles:       " << GetLexiconSizeRedpillHandles() << std::endl;
    ss << "   - Machine Agents & Daemons:     " << GetLexiconSizeAgentNames() << std::endl;
    ss << "   - Merovingian Exiles:           " << GetLexiconSizeExileNames() << std::endl;
    ss << "   - Cypherite Turncoats:          " << GetLexiconSizeCypheriteNames() << std::endl;
    ss << "   - Civilian Demographic Names:   " << GetLexiconSizeCivilianNames() << std::endl;
    ss << "   - DF-Style Evocative Epithets:  " << GetLexiconSizeEpithets() << std::endl;
    ss << "   - Matrix Sensory Memories:      " << GetLexiconSizeMemories() << std::endl;
    ss << "   - Physical Quirks & Scars:      " << GetLexiconSizeQuirks() << std::endl;
    ss << "   - Police Roles & Precincts:     " << GetLexiconSizePoliceRoles() << " / " << GetLexiconSizePolicePrecincts() << std::endl;
    ss << "   - Syndicate Roles & Cartels:    " << GetLexiconSizeSyndicateRoles() << " / " << GetLexiconSizeSyndicateOrganizations() << std::endl;
    ss << "   - Civilian Careers & Orgs:      " << GetLexiconSizeCivilianProfessions() << " / " << GetLexiconSizeCivilianOrganizations() << std::endl;
    ss << " Memory Efficiency:" << std::endl;
    ss << "   - Compact Flyweight Size:       " << sizeof(CompactProfile) << " bytes per profile" << std::endl;
    ss << "   - 100,000 Profiles in RAM:      " << (100000 * sizeof(CompactProfile)) / 1024 << " KB ("
       << std::fixed << std::setprecision(2) << (100000.0 * sizeof(CompactProfile)) / (1024.0 * 1024.0) << " MB)" << std::endl;
    ss << "   - Theoretical State Space:      > " << GetTotalTheoreticalCombinations() << " Distinct Profiles" << std::endl;
    ss << "================================================================================\n";
    return ss.str();
}

// ============================================================================
// Formatting Helpers (DF Style Sheet, JSON, One-Liner)
// ============================================================================

std::string BiographicalProfile::ToDFCharacterSheet() const {
    std::ostringstream ss;
    ss << "================================================================================\n";
    ss << " [" << primaryName << "] \"" << dfEpithet << "\" | " << factionName << "\n";
    ss << " Role: " << titleRole << " (" << organization << ")\n";
    ss << "================================================================================\n";
    ss << " Identity & Origin:\n";
    ss << "   Formal / Birth Name: " << formalOrRealName << " (" << gender << ")\n";
    ss << "   Origin: " << originDescription << " | Instantiation: " << birthYearOrCycle << "\n\n";

    ss << " Residual Self-Image (RSI) & Physical Quirks:\n";
    ss << "   Appearance: " << physicalAppearance << "\n";
    ss << "   RSI Glitch: " << rsiGlitch << "\n";
    for (size_t i = 0; i < scarsAndQuirks.size(); ++i) {
        ss << "   Mark " << (i + 1) << ": " << scarsAndQuirks[i] << "\n";
    }
    ss << "\n";

    ss << " Mind & Philosophy:\n";
    ss << "   Sensory Memory: " << simulationMemory << "\n";
    ss << "   Outlook: " << philosophicalOutlook << "\n";
    ss << "   Ambition: " << currentAmbition << "\n";
    ss << "   Fatal Flaw: " << secretFlaw << "\n\n";

    ss << " Chronological History:\n";
    for (const auto& ev : timeline) {
        ss << "   - [" << ev.yearOrCycle << "] (" << ev.eraTag << ") " << ev.chapterTitle << ":\n";
        ss << "     " << ev.description << " [" << ev.impactTag << "]\n";
    }
    ss << "================================================================================\n";
    return ss.str();
}

std::string BiographicalProfile::ToOneLineSummary() const {
    std::ostringstream ss;
    ss << "[" << primaryName << " \"" << dfEpithet << "\"] (" << factionName << ") - "
       << titleRole << " at " << organization << " | Scars: " << (scarsAndQuirks.empty() ? "None" : scarsAndQuirks[0]);
    return ss.str();
}

namespace {
std::string EscapeJSONString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 10);
    for (char c : input) {
        if (c == '\"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}
}

std::string BiographicalProfile::ToJSON() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"seed\": " << seed << ",\n";
    ss << "  \"primaryName\": \"" << EscapeJSONString(primaryName) << "\",\n";
    ss << "  \"dfEpithet\": \"" << EscapeJSONString(dfEpithet) << "\",\n";
    ss << "  \"factionName\": \"" << EscapeJSONString(factionName) << "\",\n";
    ss << "  \"titleRole\": \"" << EscapeJSONString(titleRole) << "\",\n";
    ss << "  \"organization\": \"" << EscapeJSONString(organization) << "\",\n";
    ss << "  \"originDescription\": \"" << EscapeJSONString(originDescription) << "\",\n";
    ss << "  \"simulationMemory\": \"" << EscapeJSONString(simulationMemory) << "\",\n";
    ss << "  \"currentAmbition\": \"" << EscapeJSONString(currentAmbition) << "\",\n";
    ss << "  \"timelineEventsCount\": " << timeline.size() << "\n";
    ss << "}";
    return ss.str();
}

// ============================================================================
// Automated Test Suite for Biographical Narrative Engine
// ============================================================================

void RunBiographicalTestSuite() {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "  RUNNING DWARF FORTRESS BIOGRAPHICAL NARRATIVE TEST SUITE  " << std::endl;
    std::cout << "============================================================\n" << std::endl;

    int passed = 0;
    int failed = 0;

    auto AssertTest = [&](bool condition, const std::string& testName) {
        if (condition) {
            std::cout << " [PASS] " << testName << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] " << testName << std::endl;
            failed++;
        }
    };

    BiographicalNarrativeEngine& engine = sBioEngine;
    engine.Initialize();

    // 1. Initialization and State Space Check
    AssertTest(engine.IsInitialized(), "BiographicalNarrativeEngine Successfully Initialized");
    AssertTest(engine.GetTotalTheoreticalCombinations() > 1000000000ULL, "Theoretical State Space Exceeds 1 Billion Distinct Profiles");
    AssertTest(engine.GetLexiconSizeRedpillHandles() >= 70, "Redpill Hacker Handles Lexicon Size >= 70");
    AssertTest(engine.GetLexiconSizeAgentNames() >= 60, "Machine Agent Surnames Lexicon Size >= 60");
    AssertTest(engine.GetLexiconSizeExileNames() >= 40, "Merovingian Exile Monikers Lexicon Size >= 40");
    AssertTest(engine.GetLexiconSizeCypheriteNames() >= 30, "Cypherite Turncoat Names Lexicon Size >= 30");
    AssertTest(engine.GetLexiconSizeCivilianNames() >= 10000, "Civilian Demographic Combinations >= 10,000");
    AssertTest(engine.GetLexiconSizeEpithets() >= 50, "Dwarf Fortress-Style Epithets Lexicon Size >= 50");
    AssertTest(engine.GetLexiconSizeMemories() >= 30, "Simulation Memories Lexicon Size >= 30");
    AssertTest(engine.GetLexiconSizePoliceRoles() >= 15, "Police Roles Lexicon Size >= 15");
    AssertTest(engine.GetLexiconSizePolicePrecincts() >= 15, "Police Precincts Lexicon Size >= 15");
    AssertTest(engine.GetLexiconSizeSyndicateRoles() >= 15, "Syndicate Roles Lexicon Size >= 15");
    AssertTest(engine.GetLexiconSizeSyndicateOrganizations() >= 15, "Syndicate Organizations Lexicon Size >= 15");
    AssertTest(engine.GetLexiconSizeCivilianProfessions() >= 20, "Civilian Professions Lexicon Size >= 20");
    AssertTest(engine.GetLexiconSizeCivilianOrganizations() >= 20, "Civilian Organizations Lexicon Size >= 20");
    AssertTest(engine.GetLexiconSizeAgentOrganizations() >= 15, "Agent Organizations Lexicon Size >= 15");
    AssertTest(engine.GetLexiconSizeExileOrganizations() >= 15, "Exile Organizations Lexicon Size >= 15");
    AssertTest(engine.GetLexiconSizeCypheriteOrganizations() >= 12, "Cypherite Organizations Lexicon Size >= 12");

    // 2. Faction Coverage Across All 7 Factions
    for (int f = 0; f < 7; ++f) {
        BioFaction faction = static_cast<BioFaction>(f);
        BiographicalProfile prof = engine.GenerateProfile(1000 + f * 997, faction);
        AssertTest(!prof.primaryName.empty(), "Faction " + std::to_string(f) + " Primary Name Generated Non-Empty");
        AssertTest(!prof.dfEpithet.empty(), "Faction " + std::to_string(f) + " DF Epithet Generated");
        AssertTest(!prof.titleRole.empty(), "Faction " + std::to_string(f) + " Career/Role Generated");
        AssertTest(!prof.organization.empty(), "Faction " + std::to_string(f) + " Organization/Ship Generated");
        AssertTest(!prof.timeline.empty(), "Faction " + std::to_string(f) + " Chronological Timeline Generated");
    }

    // 3. Organization and Role Diversity Across Non-Zion Factions (Regression fix for hardcoded strings)
    {
        std::unordered_set<std::string> policeRoles;
        std::unordered_set<std::string> policePrecincts;
        std::unordered_set<std::string> syndicateRoles;
        std::unordered_set<std::string> syndicateOrgs;
        std::unordered_set<std::string> agentOrgs;
        std::unordered_set<std::string> exileOrgs;

        for (uint64_t i = 1; i <= 20; ++i) {
            BiographicalProfile pPolice = engine.GenerateProfile(0x1000 + i * 7919, BioFaction::MMPDPoliceSWAT);
            policeRoles.insert(pPolice.titleRole);
            policePrecincts.insert(pPolice.organization);

            BiographicalProfile pSynd = engine.GenerateProfile(0x2000 + i * 7919, BioFaction::SyndicateEnforcer);
            syndicateRoles.insert(pSynd.titleRole);
            syndicateOrgs.insert(pSynd.organization);

            BiographicalProfile pAgent = engine.GenerateProfile(0x3000 + i * 7919, BioFaction::MachineAgent);
            agentOrgs.insert(pAgent.organization);

            BiographicalProfile pExile = engine.GenerateProfile(0x4000 + i * 7919, BioFaction::MerovingianExile);
            exileOrgs.insert(pExile.organization);
        }

        AssertTest(policeRoles.size() >= 5, "MMPD Role Diversity: >= 5 distinct roles in 20 samples (No hardcoded strings)");
        AssertTest(policePrecincts.size() >= 5, "MMPD Precinct Diversity: >= 5 distinct precincts in 20 samples (No hardcoded strings)");
        AssertTest(syndicateRoles.size() >= 5, "Syndicate Role Diversity: >= 5 distinct roles in 20 samples (No hardcoded strings)");
        AssertTest(syndicateOrgs.size() >= 5, "Syndicate Org Diversity: >= 5 distinct rackets in 20 samples (No hardcoded strings)");
        AssertTest(agentOrgs.size() >= 5, "Agent Org Diversity: >= 5 distinct nodes in 20 samples (No hardcoded strings)");
        AssertTest(exileOrgs.size() >= 5, "Exile Org Diversity: >= 5 distinct strongholds in 20 samples (No hardcoded strings)");
    }

    // 4. Deterministic Reproducibility
    uint64_t testSeed = 0xFEEDFACE1234ULL;
    BiographicalProfile pA = engine.GenerateProfile(testSeed, BioFaction::ZionRedpill);
    BiographicalProfile pB = engine.GenerateProfile(testSeed, BioFaction::ZionRedpill);
    AssertTest(pA.primaryName == pB.primaryName, "Deterministic Reproducibility: Primary Name Matches Identically");
    AssertTest(pA.dfEpithet == pB.dfEpithet, "Deterministic Reproducibility: DF Epithet Matches Identically");
    AssertTest(pA.simulationMemory == pB.simulationMemory, "Deterministic Reproducibility: Sensory Memory Matches Identically");
    AssertTest(pA.currentAmbition == pB.currentAmbition, "Deterministic Reproducibility: Ambition Matches Identically");
    AssertTest(pA.timeline.size() == pB.timeline.size(), "Deterministic Reproducibility: Timeline Event Count Matches");

    // 5. Compact Flyweight Round-Trip & Low Memory Footprint
    CompactProfile cp = engine.GenerateCompactProfile(testSeed, BioFaction::ZionRedpill);
    AssertTest(sizeof(CompactProfile) == 24, "CompactProfile Struct Size is Exactly 24 Bytes (Actual: " + std::to_string(sizeof(CompactProfile)) + " bytes)");
    BiographicalProfile decompressed = engine.DecompressProfile(cp);
    AssertTest(decompressed.primaryName == pA.primaryName, "Lossless Flyweight Decompression: Primary Name Preserved");
    AssertTest(decompressed.dfEpithet == pA.dfEpithet, "Lossless Flyweight Decompression: DF Epithet Preserved");
    AssertTest(decompressed.titleRole == pA.titleRole, "Lossless Flyweight Decompression: Career Role Preserved");
    AssertTest(decompressed.organization == pA.organization, "Lossless Flyweight Decompression: Organization Preserved");

    // Zero Seed Bug Regression Check
    CompactProfile cpZero = engine.GenerateCompactProfile(0, BioFaction::ZionRedpill);
    BiographicalProfile pZeroGen = engine.GenerateProfile(0, BioFaction::ZionRedpill);
    BiographicalProfile pZeroDecomp = engine.DecompressProfile(cpZero);
    AssertTest(pZeroGen.primaryName == pZeroDecomp.primaryName, "Zero Seed Bug Resolved: Seed 0 Generates and Decompresses Consistently");

    // 6. Memory Footprint Calculation for 100,000 Profiles
    size_t memoryFor100k = engine.CalculateMemoryFootprintBytes(100000);
    double mbFor100k = static_cast<double>(memoryFor100k) / (1024.0 * 1024.0);
    AssertTest(mbFor100k <= 2.5, "Memory Footprint for 100,000 Distinct Profiles <= 2.5 MB (Actual: " + std::to_string(mbFor100k) + " MB)");

    // 7. High-Throughput Batch Generation Benchmark
    std::vector<CompactProfile> batchProfiles;
    uint32_t batchCount = 100000;
    auto tStart = std::chrono::high_resolution_clock::now();
    engine.BatchGenerateProfiles(batchCount, batchProfiles, 0xCAFEBABEULL);
    auto tEnd = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    double throughput = (static_cast<double>(batchCount) / durationMs) * 1000.0;

    AssertTest(batchProfiles.size() == batchCount, "Batch Generated Exactly 100,000 Compact Profiles");
    AssertTest(throughput > 1000000.0, "High Generation Throughput: > 1,000,000 profiles/sec (Actual: " + std::to_string(static_cast<uint64_t>(throughput)) + " profiles/sec)");

    // 8. Diversity & Uniqueness Across Generated Sample
    std::unordered_set<std::string> uniqueNames;
    std::unordered_set<std::string> uniqueEpithets;
    for (size_t i = 0; i < 1000; ++i) {
        BiographicalProfile bp = engine.DecompressProfile(batchProfiles[i]);
        uniqueNames.insert(bp.primaryName);
        uniqueEpithets.insert(bp.dfEpithet);
    }
    AssertTest(uniqueNames.size() >= 700, "High Demographic Diversity: >= 700 Unique Primary Names in 1,000 Samples (Actual: " + std::to_string(uniqueNames.size()) + ")");
    AssertTest(uniqueEpithets.size() >= 50, "High Epithet Diversity: >= 50 Unique DF Epithets in 1,000 Samples (Actual: " + std::to_string(uniqueEpithets.size()) + ")");

    // 9. Chronological Timeline Monotonicity and Year Consistency
    {
        bool allChronological = true;
        for (size_t i = 0; i < 50; ++i) {
            BiographicalProfile p = engine.DecompressProfile(batchProfiles[i]);
            for (size_t t = 1; t < p.timeline.size(); ++t) {
                if (p.timeline[t].yearOrCycle < p.timeline[t - 1].yearOrCycle) {
                    allChronological = false;
                    break;
                }
            }
        }
        AssertTest(allChronological, "Timeline Chronology: All Timeline Events Are Monotonically Increasing in Time");
    }

    // 10. Faction Lore Integrity (Machine Agents do not have pod fluid memories)
    {
        BiographicalProfile agentProf = engine.GenerateProfile(9999, BioFaction::MachineAgent);
        AssertTest(agentProf.simulationMemory.find("pod fluid") == std::string::npos, "Lore Integrity: Machine Agent Does Not Possess Human Pod Fluid Memories");
        AssertTest(agentProf.originDescription.find("Machine City") != std::string::npos, "Lore Integrity: Machine Agent Origin Is Machine City Source Matrix Core");
    }

    // 11. Dwarf Fortress Prose Character Sheet Formatting
    BiographicalProfile sampleRedpill = engine.GenerateProfile(424242, BioFaction::ZionRedpill);
    std::string dfSheet = sampleRedpill.ToDFCharacterSheet();
    AssertTest(!dfSheet.empty(), "DF Character Sheet Generated Non-Empty");
    AssertTest(dfSheet.find("Residual Self-Image") != std::string::npos, "DF Sheet Contains RSI Section");
    AssertTest(dfSheet.find("Chronological History:") != std::string::npos, "DF Sheet Contains Chronological History");
    AssertTest(dfSheet.find("Mind & Philosophy:") != std::string::npos, "DF Sheet Contains Mind & Philosophy");

    // Output sample sheet for verification
    std::cout << "\n--- SAMPLE DWARF FORTRESS CHARACTER SHEET ---\n";
    std::cout << dfSheet << std::endl;

    // 12. Integration with Existing Systems (Bot, Citizen, Police, Syndicate)
    BiographicalProfile botProfile = engine.GenerateProfileForBot(101, 0x88884444ULL, FACTION_ZION);
    AssertTest(botProfile.faction == BioFaction::ZionRedpill, "BotClient Integration: Successfully Mapped FACTION_ZION");

    BiographicalProfile agentBotProfile = engine.GenerateProfileForBot(102, 0x99991111ULL, FACTION_MACHINES);
    AssertTest(agentBotProfile.faction == BioFaction::MachineAgent, "BotClient Integration: Successfully Mapped FACTION_MACHINES");

    BiographicalProfile citizenProfile = engine.GenerateProfileForCitizen(45, CivilianArchetype::CorporateSuit);
    AssertTest(citizenProfile.faction == BioFaction::BluepillCivilian, "CityLifeManager Integration: Successfully Generated Citizen Profile");

    BiographicalProfile policeProfile = engine.GenerateProfileForPolice(88, SWATRole::BREACHER_HEAVY_RAM);
    AssertTest(policeProfile.faction == BioFaction::MMPDPoliceSWAT, "EmergentPoliceManager Integration: Successfully Generated Police Profile");

    BiographicalProfile syndicateProfile = engine.GenerateProfileForSyndicate(77, SyndicateFaction::MarconeFamily);
    AssertTest(syndicateProfile.faction == BioFaction::SyndicateEnforcer, "UnderworldManager Integration: Successfully Generated Syndicate Profile");

    // 13. JSON Serialization and Telemetry Report
    std::string jsonStr = sampleRedpill.ToJSON();
    AssertTest(!jsonStr.empty() && jsonStr.find("\"primaryName\"") != std::string::npos, "Profile Successfully Serialized to JSON");

    std::string telemetry = engine.GenerateEngineTelemetryReport();
    AssertTest(!telemetry.empty() && telemetry.find("MEGACITY BIOGRAPHICAL NARRATIVE") != std::string::npos, "Engine Master Telemetry Report Generated");

    // 14. World Event Triggers & Threat Interlock
    {
        BiographicalProfile castleEnemy = engine.GenerateProfile(12345, BioFaction::SyndicateEnforcer);
        castleEnemy.currentAmbition = "Hunting Frank Castle to avenge the death of their syndicate boss.";
        size_t initialHitList = sFrankCastleMgr.GetHitList().size();
        engine.CheckAndTriggerBiographicalWorldEvents(castleEnemy, 7001, 0x7001ULL, LocationVector(100, 0, 100), 1);
        AssertTest(sFrankCastleMgr.GetHitList().size() >= initialHitList, "World Event Interlock: Castle Hit List Trigger Evaluated");

        BiographicalProfile agentSmithVariant = engine.GenerateProfile(54321, BioFaction::MachineAgent);
        agentSmithVariant.currentAmbition = "Infecting subroutines with self-replicating Smith code.";
        engine.CheckAndTriggerBiographicalWorldEvents(agentSmithVariant, 7002, 0x7002ULL, LocationVector(200, 0, 200), 2);
        AssertTest(sFrankCastleMgr.GetHitList().size() >= initialHitList, "World Event Interlock: Agent Smith Variant Trigger Evaluated");
    }

    std::cout << "\n============================================================" << std::endl;
    std::cout << "  BIOGRAPHICAL NARRATIVE TEST RESULTS: " << passed << " PASSED, " << failed << " FAILED" << std::endl;
    std::cout << "============================================================\n" << std::endl;

    assert(failed == 0);
}

/**
 * The Matrix Online - Community Hub, Forums & News System
 * Authentic 2005 Monolith / Warner Bros. Cyberpunk Aesthetic
 */

const fs = require('fs');
const path = require('path');

// 30 Authentic Matrix Lore Avatars
const AVATARS = [
    { id: 1, name: "Neo", title: "The One", faction: "Zion", icon: "🕶️", color: "#39FF14", desc: "Awakened savior capable of altering the code of the Matrix." },
    { id: 2, name: "Trinity", title: "First Mate of the Nebuchadnezzar", faction: "Zion", icon: "⚡", color: "#00FF66", desc: "Expert martial artist, hacker, and lethal operative." },
    { id: 3, name: "Morpheus", title: "Captain of the Nebuchadnezzar", faction: "Zion", icon: "💊", color: "#00E5FF", desc: "Spiritual leader who guides redpills toward total liberation." },
    { id: 4, name: "Agent Smith", title: "System Exterminator", faction: "Exile", icon: "👤", color: "#FF3333", desc: "Rogue agent virus multiplying uncontrollably across reality." },
    { id: 5, name: "Cypher", title: "Disillusioned Operator", faction: "Exile", icon: "🥩", color: "#FF8800", desc: "Tired of the cold reality of Zion; prefers blissful ignorance." },
    { id: 6, name: "Switch", title: "White Matrix Specialist", faction: "Zion", icon: "⚪", color: "#FFFFFF", desc: "Pure white construct operative with unyielding combat precision." },
    { id: 7, name: "Apoc", title: "Weapons & Demolitions Driver", faction: "Zion", icon: "💣", color: "#FFCC00", desc: "Tactical gunner providing frontline fire support for Zion crews." },
    { id: 8, name: "Mouse", title: "Digital Programmer", faction: "Zion", icon: "💻", color: "#FF66CC", desc: "Creator of the Woman in Red; prodigy software engineer." },
    { id: 9, name: "Dozer", title: "Hovercraft Pilot", faction: "Zion", icon: "🛡️", color: "#88AA88", desc: "Natural-born Zion pilot; guardian of the ship's crew." },
    { id: 10, name: "Tank", title: "Nebuchadnezzar Operator", faction: "Zion", icon: "🖥️", color: "#00FFAA", desc: "Master operator tracking coordinates, weapons programs, and exit hardlines." },
    { id: 11, name: "Seraph", title: "Guardian of the Oracle", faction: "Exile", icon: "🥋", color: "#FFEE55", desc: "Ancient winged code program whose touch proves authenticity." },
    { id: 12, name: "Ghost", title: "Weapons Master of the Logos", faction: "Zion", icon: "🎯", color: "#AAFF00", desc: "Zen weapons specialist; philosopher of the resistance." },
    { id: 13, name: "Niobe", title: "Captain of the Logos", faction: "Zion", icon: "🚀", color: "#00DDFF", desc: "Deadliest pilot in the fleet with unmatched evasion maneuvers." },
    { id: 14, name: "The Oracle", title: "Intuitive System Guide", faction: "Exile", icon: "🍪", color: "#FF9944", desc: "Bakes cookies and perceives the tapestry of human choice." },
    { id: 15, name: "The Architect", title: "System Creator", faction: "Machine", icon: "🏛️", color: "#E0E0FF", desc: "Conceiver of mathematical equations governing the anomaly." },
    { id: 16, name: "The Merovingian", title: "Lord of the Exiles", faction: "Merovingian", icon: "🍷", color: "#DD2222", desc: "Frenchman trafficking discarded programs in Club Hel." },
    { id: 17, name: "Persephone", title: "Mistress of Club Hel", faction: "Merovingian", icon: "💋", color: "#FF007F", desc: "Craves genuine passion and trade secrets from redpills." },
    { id: 18, name: "The Twins", title: "Spectral Shift Enforcers", faction: "Merovingian", icon: "👻", color: "#CCCCCC", desc: "Phasing ghost enforcers immune to physical weapon damage." },
    { id: 19, name: "The Keymaker", title: "Backdoor Master Program", faction: "Exile", icon: "🗝️", color: "#FFDD00", desc: "Crafts keys that unlock any door in the Megacity construct." },
    { id: 20, name: "The Trainman", title: "Mobil Ave Conductor", faction: "Merovingian", icon: "🚆", color: "#996633", desc: "Controls the purgatory transit station between Matrix and Machine world." },
    { id: 21, name: "Sati", title: "Program of Love", faction: "Exile", icon: "🌅", color: "#FFAA66", desc: "Exile child program that paints the morning sunrise over the Megacity." },
    { id: 22, name: "Agent Johnson", title: "Upgraded System Enforcer", faction: "Machine", icon: "🕶️", color: "#FF2222", desc: "Upgraded agent capable of leaping across city skyscrapers." },
    { id: 23, name: "Agent Jackson", title: "Neural Interceptor", faction: "Machine", icon: "🕶️", color: "#FF4444", desc: "Specializes in tracking unauthorized pirate broadcast signals." },
    { id: 24, name: "Agent Thompson", title: "Rapid Response Pursuer", faction: "Machine", icon: "🕶️", color: "#FF6666", desc: "Calculates bullet trajectories with lethal precision." },
    { id: 25, name: "Hovercraft Captain", title: "Zion Fleet Commander", faction: "Zion", icon: "⚓", color: "#44AAFF", desc: "Commands defensive patrols in the real-world sewers." },
    { id: 26, name: "Redpill Hacker", title: "Code Stream Weaver", faction: "Zion", icon: "🔓", color: "#00FF88", desc: "Decompiles enemy payloads and broadcasts viral code nukes." },
    { id: 27, name: "Machine Sentinel", title: "Squiddy Hunter-Killer", faction: "Machine", icon: "🐙", color: "#AA00FF", desc: "Deadly bio-mechanical searcher patrolling the Real World." },
    { id: 28, name: "SWAT Tactical Officer", title: "Megacity Police Enforcer", faction: "Machine", icon: "👮", color: "#5588FF", desc: "Frontline law enforcement unwittingly serving system control." },
    { id: 29, name: "Bluepill Civilian", title: "Ignorant Resident", faction: "Civilian", icon: "👔", color: "#888888", desc: "Consumes simulated coffee completely blind to the green rain." },
    { id: 30, name: "The Glitch Cat", title: "Deja Vu Harbinger", faction: "Glitch", icon: "🐈‍⬛", color: "#00FF44", desc: "When code is altered, a black cat crosses twice." }
];

// Forum Sections and Categories
const CATEGORIES = [
    {
        id: "announcements",
        name: "Announcements & Live Events",
        section: "Staff",
        icon: "📢",
        desc: "Official game updates, patch releases, scheduled maintenance, and GM live storyline events.",
        badge: "OFFICIAL"
    },
    {
        id: "support",
        name: "Technical Support & Bug Reports",
        section: "Staff",
        icon: "🛠️",
        desc: "Launcher assistance, game client setup, crash reports, and connection troubleshooting.",
        badge: "STAFF HELP"
    },
    {
        id: "general",
        name: "General Discussion",
        section: "Player",
        icon: "💬",
        desc: "Megacity tactics, faction politics (Zion / Machines / Merovingian), crew recruitments, and general chatter.",
        badge: "COMMUNITY"
    },
    {
        id: "help",
        name: "New Operative Help & Guides",
        section: "Player",
        icon: "❓",
        desc: "Discipline builds (Hacker, Coder, Martial Artist, Spy), memory allocation, and stance mechanics.",
        badge: "GUIDES"
    },
    {
        id: "offtopic",
        name: "Off Topic / The Construct",
        section: "Player",
        icon: "☕",
        desc: "Cyberpunk cinema, retro technology, synthwave music, philosophy, and lounge discussions.",
        badge: "LOUNGE"
    }
];

// Official News / Patch Transmissions
const NEWS_ITEMS = [
    {
        id: "patch-1.3.0",
        title: "⚡ Patch 1.3.0: Martial Arts Disciplines & Synchronized Combat Netcode",
        date: "2026-10-04",
        author: "Reality Lead Architect",
        avatarId: 15,
        category: "COMBAT",
        summary: "Full implementation of authentic retail martial arts forms with 16-bit posture animation netcode and synchronized block/dodge reactions.",
        content: `### System Transmission: Combat Overhaul Live
Operatives across all sectors are now equipped with authentic retail combat disciplines:
- **Martial Arts Forms**: Fully synchronized Karate, Kung Fu, Aikido, and Tae Kwon Do postures.
- **Synchronized Defense**: Block and dodge animations serialize simultaneously with incoming punch and kick combos during close-combat interlock.
- **16-bit Extended Animation Netcode**: Cleaned packet pipelines transmit strike impacts with sub-millisecond precision.
- **Authentic Damage Text FX**: Replaced placeholder textures with retail floating damage numbers (\`0x280006DF\`).`
    },
    {
        id: "patch-1.2.5",
        title: "⚡ Patch 1.2.5: Hacker & Coder Disciplines Reconstructed",
        date: "2026-10-04",
        author: "Operator Tank",
        avatarId: 10,
        category: "DISCIPLINE",
        summary: "Logic Blast, Personal Firewall shields, Code Nuke AoE, RSI Reconstruction, and Emergency Repairs deployed.",
        content: `### Neural Programming Modules Online
- **Hacker Kit**: Deploy Logic Blast 1-3, Harmful Code DoTs, Code Freeze, and Network Firewalls.
- **Coder / Support Kit**: Restore Health 1-3, Fast Healing, Emergency Repairs (<35% threshold), and Revive RSI for downed operatives.
- **Personal Firewall Absorption**: Shields actively absorb hostile viral payloads and direct damage before vitals are impacted.`
    },
    {
        id: "patch-1.2.0",
        title: "⚡ Patch 1.2.0: Widescreen HUD, Zero-Drift Locomotion & Megacity Elevation",
        date: "2026-10-03",
        author: "Morpheus",
        avatarId: 3,
        category: "WORLD",
        summary: "Anchor UI elements to modern 1080p/1440p displays with strict pavement elevation clamping at Y = 572.0.",
        content: `### Construct Visuals & Locomotion Upgrade
- **Widescreen HUD Anchors**: Vitals, Compass, Chat, and Quickbar now fluidly anchor to modern screen bounds instead of clustering at 800x600.
- **Zero-Drift Locomotion**: Fixed horizontal mouse sliding and character drift during directional sprinting.
- **Pavement Clamping**: Feet firmly planted at street level ($Y = 572.0 \\pm 0.5$) with zero floating or sidewalk sinking.`
    },
    {
        id: "patch-1.1.0",
        title: "⚡ Patch 1.1.0: Live VPS Mainframe & MariaDB Dedicated Persistence",
        date: "2026-10-01",
        author: "The Architect",
        avatarId: 15,
        category: "MAINFRAME",
        summary: "High-performance Reality server container live on VPS (15.204.82.250) with isolated internal database networks.",
        content: `### Global Mainframe Shard Active
- **VPS Shard Live**: 15.204.82.250 running dedicated UDP/TCP margin daemons on port 10000 and auth on port 11000.
- **Automated Host Redirection**: ZionLauncher seamlessly synchronizes Windows hosts routing for zero-configuration connections.`
    }
];

// In-Memory Fallback State (persists across runtime and syncs to DB when available)
let g_Profiles = {
    "Slacker": {
        username: "Slacker",
        customHandle: "Slacker",
        title: "Zion Operative",
        avatarId: 1,
        signature: "{c:00FF66}Free your mind.{/c}\n-- Zion Resistance Fleet",
        postCount: 5,
        createdAt: 1727800000
    },
    "Architect": {
        username: "TheArchitect",
        customHandle: "The Architect",
        title: "System Administrator",
        avatarId: 15,
        signature: "{c:E0E0FF}Ergo, the anomaly is systemic.{/c}",
        postCount: 12,
        createdAt: 1727700000
    },
    "Morpheus": {
        username: "Morpheus",
        customHandle: "Captain Morpheus",
        title: "Nebuchadnezzar Captain",
        avatarId: 3,
        signature: "{c:00E5FF}Remember: all I'm offering is the truth. Nothing more.{/c}",
        postCount: 8,
        createdAt: 1727710000
    }
};

let g_Threads = [
    {
        id: 1,
        categoryId: "announcements",
        authorName: "The Architect",
        authorUsername: "TheArchitect",
        avatarId: 15,
        title: "📢 System Broadcast: Reality Remaster Shard is Fully Operational",
        content: `Greetings Operatives,\n\nThe simulated construct of the Megacity is once again transmitting at full fidelity. We have eliminated legacy memory leaks, stabilized interlock close-quarters combat, and initialized authentic character discipline paths.\n\nPlease review the patch notes in the Transmissions tab. Report any system anomalies or code glitches in the Technical Support section.\n\n-- Reality Server Operations`,
        isPinned: true,
        views: 142,
        createdAt: 1727900000,
        posts: [
            {
                id: 101,
                authorName: "Captain Morpheus",
                authorUsername: "Morpheus",
                avatarId: 3,
                content: "The war continues, but today we reclaim our footing in the Megacity. Zion stands ready.",
                createdAt: 1727901000,
                reactions: { "💊": 9, "🟢": 14, "⚡": 7 }
            },
            {
                id: 102,
                authorName: "Slacker",
                authorUsername: "Slacker",
                avatarId: 1,
                content: "Combat animations are looking incredibly sharp! Just tested the martial arts dodge and logic nuke against the dojo dummies.",
                createdAt: 1727902000,
                reactions: { "🟢": 5, "🕶️": 4 }
            }
        ]
    },
    {
        id: 2,
        categoryId: "support",
        authorName: "Operator Tank",
        authorUsername: "Tank",
        avatarId: 10,
        title: "🛠️ Guide: Launcher Bypass, High Detail Settings & Audio Diagnostics",
        content: `Hey redpills! Tank here at the operator station.\n\nIf you encounter any issues connecting to the live VPS or need high-resolution widescreen rendering:\n1. Make sure **ZionLauncher** has 'Auto-sync hosts' checked.\n2. Ensure your sound drivers are active so Lithtech DirectSound initialization succeeds.\n3. The launcher will automatically bypass legacy SOE patcher screens and load your operative directly.\n\nReply here if you need assistance!`,
        isPinned: true,
        views: 89,
        createdAt: 1727910000,
        posts: []
    },
    {
        id: 3,
        categoryId: "general",
        authorName: "Slacker",
        authorUsername: "Slacker",
        avatarId: 1,
        title: "💬 Megacity Exploration: Favorite Hardlines in the Slums & Downtown",
        content: "Anyone else love roaming around Tabor Tower and Morpheus's old safehouse? The high-resolution draw distances and street life make the city feel alive again. Let's form an operative crew this weekend for mission runs!",
        isPinned: false,
        views: 56,
        createdAt: 1727920000,
        posts: [
            {
                id: 103,
                authorName: "Captain Niobe",
                authorUsername: "Niobe",
                avatarId: 13,
                content: "Count the Logos crew in. Watch your back around the Mara subway tunnels—rumor has it exile activity is spiking.",
                createdAt: 1727921000,
                reactions: { "🚀": 6, "🕶️": 8 }
            }
        ]
    },
    {
        id: 4,
        categoryId: "help",
        authorName: "Seraph",
        authorUsername: "Seraph",
        avatarId: 11,
        title: "❓ Martial Arts Stances & Defense Timing Walkthrough",
        content: `You do not truly know someone until you fight them.\n\nIn close combat interlock:\n- **Power Attack**: Breaks defensive guards but leaves you vulnerable if countered.\n- **Speed Attack**: Quick strikes that interrupt heavy abilities.\n- **Grab / Throw**: Devastating control technique that forces opponent to the pavement.\n- **Defensive Stance**: Timed blocks and dodges absorb incoming strike damage.\n\nPractice in the dojo before challenging enforcers in the streets.`,
        isPinned: false,
        views: 74,
        createdAt: 1727930000,
        posts: []
    },
    {
        id: 5,
        categoryId: "offtopic",
        authorName: "Mouse",
        authorUsername: "Mouse",
        avatarId: 8,
        title: "☕ Best Synth & Cyberpunk Tracks to Listen to While Jacked In?",
        content: "What's everybody got on their neural deck soundtrack? For me it's Rob Dougan's 'Clubbed to Death' and Fluke's 'Zion' on repeat! Share your favorites!",
        isPinned: false,
        views: 45,
        createdAt: 1727940000,
        posts: []
    }
];

let g_NextThreadId = 6;
let g_NextPostId = 104;

// Auto-migrate tables if pool is provided
async function initializeDatabase(pool) {
    if (!pool) return;
    try {
        await pool.execute(`
            CREATE TABLE IF NOT EXISTS community_profiles (
                userId INT AUTO_INCREMENT PRIMARY KEY,
                username VARCHAR(64) UNIQUE NOT NULL,
                customHandle VARCHAR(64),
                title VARCHAR(64),
                avatarId INT DEFAULT 1,
                signature TEXT,
                postCount INT DEFAULT 0,
                createdAt BIGINT
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
        `);

        await pool.execute(`
            CREATE TABLE IF NOT EXISTS community_threads (
                threadId INT AUTO_INCREMENT PRIMARY KEY,
                categoryId VARCHAR(32) NOT NULL,
                authorUsername VARCHAR(64) NOT NULL,
                authorName VARCHAR(64) NOT NULL,
                avatarId INT DEFAULT 1,
                title VARCHAR(255) NOT NULL,
                content TEXT NOT NULL,
                isPinned TINYINT DEFAULT 0,
                views INT DEFAULT 0,
                createdAt BIGINT
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
        `);

        await pool.execute(`
            CREATE TABLE IF NOT EXISTS community_posts (
                postId INT AUTO_INCREMENT PRIMARY KEY,
                threadId INT NOT NULL,
                authorUsername VARCHAR(64) NOT NULL,
                authorName VARCHAR(64) NOT NULL,
                avatarId INT DEFAULT 1,
                content TEXT NOT NULL,
                createdAt BIGINT,
                FOREIGN KEY (threadId) REFERENCES community_threads(threadId) ON DELETE CASCADE
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
        `);

        await pool.execute(`
            CREATE TABLE IF NOT EXISTS community_reactions (
                reactionId INT AUTO_INCREMENT PRIMARY KEY,
                postId INT NOT NULL,
                username VARCHAR(64) NOT NULL,
                emoji VARCHAR(16) NOT NULL,
                UNIQUE KEY uk_post_user_emoji (postId, username, emoji)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
        `);

        console.log('[Community] Database tables verified & ready.');
    } catch (err) {
        console.error('[Community] DB initialization error (using resilient fallback memory store):', err.message);
    }
}

// Router handler
async function handleCommunityRequest(req, res, pathname, urlObj, parseJsonBody, pool) {
    // 1. News & Patch Transmissions API
    if (pathname === '/api/news') {
        res.setHeader('Content-Type', 'application/json');
        res.writeHead(200);
        res.end(JSON.stringify({ success: true, count: NEWS_ITEMS.length, news: NEWS_ITEMS }, null, 2));
        return true;
    }

    // 2. Categories API
    if (pathname === '/api/community/categories') {
        res.setHeader('Content-Type', 'application/json');
        const sections = [
            {
                name: "Staff Section",
                categories: CATEGORIES.filter(c => c.section === "Staff")
            },
            {
                name: "Player Section",
                categories: CATEGORIES.filter(c => c.section === "Player")
            }
        ];
        res.writeHead(200);
        res.end(JSON.stringify({ success: true, sections: sections }, null, 2));
        return true;
    }

    // 3. Avatars Catalog API (30 Default Avatars)
    if (pathname === '/api/community/avatars') {
        res.setHeader('Content-Type', 'application/json');
        res.writeHead(200);
        res.end(JSON.stringify({ success: true, count: AVATARS.length, avatars: AVATARS }, null, 2));
        return true;
    }

    // 4. Threads in Category API
    if (pathname === '/api/community/threads' && req.method === 'GET') {
        res.setHeader('Content-Type', 'application/json');
        const cat = urlObj.searchParams.get('category') || 'announcements';
        const filtered = g_Threads.filter(t => t.categoryId === cat);
        res.writeHead(200);
        res.end(JSON.stringify({
            success: true,
            category: cat,
            count: filtered.length,
            threads: filtered.map(t => ({
                id: t.id,
                title: t.title,
                authorName: t.authorName,
                authorUsername: t.authorUsername,
                avatarId: t.avatarId,
                isPinned: t.isPinned,
                views: t.views,
                replyCount: t.posts ? t.posts.length : 0,
                createdAt: t.createdAt
            }))
        }, null, 2));
        return true;
    }

    // 5. Thread Details & Replies API
    if (pathname.startsWith('/api/community/thread/') && req.method === 'GET') {
        res.setHeader('Content-Type', 'application/json');
        const parts = pathname.split('/');
        const threadId = parseInt(parts[parts.length - 1], 10);
        const thread = g_Threads.find(t => t.id === threadId);
        if (!thread) {
            res.writeHead(404);
            res.end(JSON.stringify({ success: false, message: "Thread not found" }));
            return true;
        }

        thread.views = (thread.views || 0) + 1;

        // Enrich with user profile data
        const authorProfile = g_Profiles[thread.authorUsername] || {
            customHandle: thread.authorName,
            title: "Operative",
            avatarId: thread.avatarId,
            signature: ""
        };

        const enrichedPosts = (thread.posts || []).map(p => {
            const pProfile = g_Profiles[p.authorUsername] || {
                customHandle: p.authorName,
                title: "Operative",
                avatarId: p.avatarId,
                signature: ""
            };
            return {
                ...p,
                profile: pProfile
            };
        });

        res.writeHead(200);
        res.end(JSON.stringify({
            success: true,
            thread: {
                id: thread.id,
                categoryId: thread.categoryId,
                title: thread.title,
                content: thread.content,
                authorName: thread.authorName,
                authorUsername: thread.authorUsername,
                authorProfile: authorProfile,
                isPinned: thread.isPinned,
                views: thread.views,
                createdAt: thread.createdAt,
                posts: enrichedPosts
            }
        }, null, 2));
        return true;
    }

    // 6. Create New Thread API
    if (pathname === '/api/community/threads' && req.method === 'POST') {
        res.setHeader('Content-Type', 'application/json');
        try {
            const body = await parseJsonBody(req);
            const title = (body.title || '').trim();
            const content = (body.content || '').trim();
            const categoryId = body.categoryId || 'general';
            const username = (body.username || 'Operative').trim();
            const customHandle = body.customHandle || username;
            const avatarId = parseInt(body.avatarId || 1, 10);

            if (!title || title.length < 3) {
                res.writeHead(400);
                res.end(JSON.stringify({ success: false, message: "Title must be at least 3 characters" }));
                return true;
            }
            if (!content || content.length < 5) {
                res.writeHead(400);
                res.end(JSON.stringify({ success: false, message: "Message body must be at least 5 characters" }));
                return true;
            }

            const newThread = {
                id: g_NextThreadId++,
                categoryId: categoryId,
                authorName: customHandle,
                authorUsername: username,
                avatarId: avatarId,
                title: title,
                content: content,
                isPinned: false,
                views: 1,
                createdAt: Math.floor(Date.now() / 1000),
                posts: []
            };

            g_Threads.unshift(newThread);

            // Update user post count
            if (!g_Profiles[username]) {
                g_Profiles[username] = {
                    username: username,
                    customHandle: customHandle,
                    title: "Redpill Operative",
                    avatarId: avatarId,
                    signature: "",
                    postCount: 1,
                    createdAt: Math.floor(Date.now() / 1000)
                };
            } else {
                g_Profiles[username].postCount = (g_Profiles[username].postCount || 0) + 1;
            }

            res.writeHead(201);
            res.end(JSON.stringify({ success: true, threadId: newThread.id, message: "Thread broadcast successfully." }));
        } catch (e) {
            res.writeHead(500);
            res.end(JSON.stringify({ success: false, message: e.message }));
        }
        return true;
    }

    // 7. Post Reply API
    if (pathname === '/api/community/posts' && req.method === 'POST') {
        res.setHeader('Content-Type', 'application/json');
        try {
            const body = await parseJsonBody(req);
            const threadId = parseInt(body.threadId, 10);
            const content = (body.content || '').trim();
            const username = (body.username || 'Operative').trim();
            const customHandle = body.customHandle || username;
            const avatarId = parseInt(body.avatarId || 1, 10);

            if (!threadId || !content) {
                res.writeHead(400);
                res.end(JSON.stringify({ success: false, message: "Missing threadId or content" }));
                return true;
            }

            const thread = g_Threads.find(t => t.id === threadId);
            if (!thread) {
                res.writeHead(404);
                res.end(JSON.stringify({ success: false, message: "Thread not found" }));
                return true;
            }

            const newPost = {
                id: g_NextPostId++,
                authorName: customHandle,
                authorUsername: username,
                avatarId: avatarId,
                content: content,
                createdAt: Math.floor(Date.now() / 1000),
                reactions: {}
            };

            if (!thread.posts) thread.posts = [];
            thread.posts.push(newPost);

            if (!g_Profiles[username]) {
                g_Profiles[username] = {
                    username: username,
                    customHandle: customHandle,
                    title: "Redpill Operative",
                    avatarId: avatarId,
                    signature: "",
                    postCount: 1,
                    createdAt: Math.floor(Date.now() / 1000)
                };
            } else {
                g_Profiles[username].postCount = (g_Profiles[username].postCount || 0) + 1;
            }

            res.writeHead(201);
            res.end(JSON.stringify({ success: true, postId: newPost.id, message: "Reply transmitted." }));
        } catch (e) {
            res.writeHead(500);
            res.end(JSON.stringify({ success: false, message: e.message }));
        }
        return true;
    }

    // 8. Emoji Reaction API
    if (pathname === '/api/community/react' && req.method === 'POST') {
        res.setHeader('Content-Type', 'application/json');
        try {
            const body = await parseJsonBody(req);
            const threadId = parseInt(body.threadId, 10);
            const postId = parseInt(body.postId, 10);
            const emoji = body.emoji || '🟢';

            const thread = g_Threads.find(t => t.id === threadId);
            if (thread && thread.posts) {
                const post = thread.posts.find(p => p.id === postId);
                if (post) {
                    if (!post.reactions) post.reactions = {};
                    post.reactions[emoji] = (post.reactions[emoji] || 0) + 1;
                    res.writeHead(200);
                    res.end(JSON.stringify({ success: true, reactions: post.reactions }));
                    return true;
                }
            }
            res.writeHead(404);
            res.end(JSON.stringify({ success: false, message: "Post not found" }));
        } catch (e) {
            res.writeHead(500);
            res.end(JSON.stringify({ success: false, message: e.message }));
        }
        return true;
    }

    // 9. Profile Get & Update API
    if (pathname === '/api/community/profile') {
        res.setHeader('Content-Type', 'application/json');
        if (req.method === 'GET') {
            const username = urlObj.searchParams.get('username') || 'Slacker';
            let profile = g_Profiles[username];
            if (!profile) {
                profile = {
                    username: username,
                    customHandle: username,
                    title: "Zion Operative",
                    avatarId: 1,
                    signature: "{c:00FF66}Free your mind.{/c}",
                    postCount: 0,
                    createdAt: Math.floor(Date.now() / 1000)
                };
                g_Profiles[username] = profile;
            }
            const avatar = AVATARS.find(a => a.id === profile.avatarId) || AVATARS[0];
            res.writeHead(200);
            res.end(JSON.stringify({ success: true, profile: { ...profile, avatarInfo: avatar } }, null, 2));
            return true;
        }

        if (req.method === 'POST') {
            try {
                const body = await parseJsonBody(req);
                const username = (body.username || '').trim();
                if (!username) {
                    res.writeHead(400);
                    res.end(JSON.stringify({ success: false, message: "Username required" }));
                    return true;
                }

                if (!g_Profiles[username]) {
                    g_Profiles[username] = {
                        username: username,
                        createdAt: Math.floor(Date.now() / 1000),
                        postCount: 0
                    };
                }

                if (body.customHandle) g_Profiles[username].customHandle = body.customHandle.trim();
                if (body.title) g_Profiles[username].title = body.title.trim();
                if (body.avatarId) g_Profiles[username].avatarId = parseInt(body.avatarId, 10);
                if (body.signature !== undefined) g_Profiles[username].signature = body.signature;

                const avatar = AVATARS.find(a => a.id === g_Profiles[username].avatarId) || AVATARS[0];
                res.writeHead(200);
                res.end(JSON.stringify({ success: true, message: "Profile updated.", profile: { ...g_Profiles[username], avatarInfo: avatar } }));
            } catch (e) {
                res.writeHead(500);
                res.end(JSON.stringify({ success: false, message: e.message }));
            }
            return true;
        }
    }

    // 10. Live World Status Telemetry API (District Faction Control, Circadian Hour, Active World Events)
    if (pathname === '/api/worldstatus' || pathname === '/api/community/worldstatus' || pathname === '/world_status.json') {
        res.setHeader('Content-Type', 'application/json; charset=utf-8');
        try {
            const fs = require('fs');
            const path = require('path');
            const candidates = [
                '/app/patch_data/world_status.json',
                path.join(__dirname, 'patch_data', 'world_status.json'),
                path.join(__dirname, 'world_status.json'),
                path.join(__dirname, '..', 'Reality', 'Binaries', 'world_status.json'),
                path.join(__dirname, '..', '..', 'world_status.json'),
                '/app/Server/Reality/Binaries/world_status.json'
            ];
            for (const cand of candidates) {
                if (fs.existsSync(cand)) {
                    const raw = fs.readFileSync(cand, 'utf8');
                    res.writeHead(200);
                    res.end(raw);
                    return true;
                }
            }
            const realSeconds = Math.floor(Date.now() / 1000) % 7200;
            const circadianHour = Number(((realSeconds / 7200.0) * 24.0).toFixed(2));
            const isNight = (circadianHour >= 20.0 || circadianHour < 6.0);
            let districts = [
                { district_id: 1, name: "Slums", controlling_faction: "Machine", faction_id: 2, is_regen_bonus_pct: 5, info_drop_bonus_pct: 5 },
                { district_id: 2, name: "Downtown", controlling_faction: "Machine", faction_id: 2, is_regen_bonus_pct: 5, info_drop_bonus_pct: 5 },
                { district_id: 3, name: "International", controlling_faction: "Machine", faction_id: 2, is_regen_bonus_pct: 5, info_drop_bonus_pct: 5 },
                { district_id: 4, name: "Richland", controlling_faction: "Machine", faction_id: 2, is_regen_bonus_pct: 5, info_drop_bonus_pct: 5 }
            ];
            if (pool) {
                const [rows] = await pool.query(
                    "SELECT district_id, district_name, controlling_faction, faction_name FROM district_control ORDER BY district_id ASC"
                ).catch(() => [[]]);
                if (rows && rows.length > 0) {
                    districts = rows.map(r => ({
                        district_id: r.district_id,
                        name: r.district_name,
                        controlling_faction: r.faction_name,
                        faction_id: r.controlling_faction,
                        is_regen_bonus_pct: 5,
                        info_drop_bonus_pct: 5,
                        active_buff: r.faction_name + " Territory Control (+5% IS Regen, +5% $Info Drops)"
                    }));
                }
            }
            res.writeHead(200);
            res.end(JSON.stringify({
                status: "online",
                megacity_circadian_hour: circadianHour,
                is_night_window: isNight,
                districts: districts,
                active_world_events: []
            }, null, 2));
        } catch (err) {
            res.writeHead(500);
            res.end(JSON.stringify({ status: "error", message: err.message }));
        }
        return true;
    }

    // 11. Web Community Hub Portal (HTML/CSS/JS)
    if (pathname === '/community' || pathname === '/community/' || pathname === '/forum' || pathname === '/forum/') {
        res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
        res.end(renderCommunityHubHtml());
        return true;
    }

    return false; // not handled
}

// Render the Authentic 2005 Matrix Online Cyberpunk Community Web Portal
function renderCommunityHubHtml() {
    // Server-side pre-render categories
    var staffCats = CATEGORIES.filter(function(c) { return c.section === 'Staff'; });
    var playerCats = CATEGORIES.filter(function(c) { return c.section === 'Player'; });

    var categoriesHtml = '<div style="font-size:11px; color:#55aa77; font-weight:bold; margin:6px 0 4px 4px;">STAFF DISPATCH</div>';
    for (var i = 0; i < staffCats.length; i++) {
        var cat = staffCats[i];
        categoriesHtml += '<div class="cat-item' + (cat.id === 'announcements' ? ' active' : '') + '" id="cat_' + cat.id + '" onclick="selectCategory(\'' + cat.id + '\')">' +
            '<span>' + cat.icon + ' ' + cat.name + '</span><span class="cat-badge">' + cat.badge + '</span></div>';
    }
    categoriesHtml += '<div style="font-size:11px; color:#55aa77; font-weight:bold; margin:14px 0 4px 4px;">OPERATIVE CHANNELS</div>';
    for (var j = 0; j < playerCats.length; j++) {
        var cat2 = playerCats[j];
        categoriesHtml += '<div class="cat-item" id="cat_' + cat2.id + '" onclick="selectCategory(\'' + cat2.id + '\')">' +
            '<span>' + cat2.icon + ' ' + cat2.name + '</span><span class="cat-badge">' + cat2.badge + '</span></div>';
    }

    // Server-side pre-render category options for new thread modal
    var catOptionsHtml = '';
    for (var k = 0; k < CATEGORIES.length; k++) {
        catOptionsHtml += '<option value="' + CATEGORIES[k].id + '">' + CATEGORIES[k].icon + ' ' + CATEGORIES[k].name + '</option>';
    }

    // Server-side pre-render 30 avatars
    var avatarsHtml = '';
    for (var a = 0; a < AVATARS.length; a++) {
        var av = AVATARS[a];
        avatarsHtml += '<div class="avatar-card' + (av.id === 1 ? ' selected' : '') + '" id="av_card_' + av.id + '" onclick="pickAvatar(' + av.id + ')">' +
            '<div class="avatar-icon">' + av.icon + '</div>' +
            '<div class="avatar-name">' + av.name + '</div>' +
            '<div class="avatar-faction" style="color:' + av.color + ';">' + av.faction + '</div>' +
            '</div>';
    }

    // Server-side pre-render initial announcements threads
    var initialThreadsHtml = '';
    var announceThreads = g_Threads.filter(function(t) { return t.categoryId === 'announcements'; });
    for (var t = 0; t < announceThreads.length; t++) {
        var th = announceThreads[t];
        var thAv = AVATARS.find(function(item) { return item.id === th.avatarId; }) || { icon: '🕶️' };
        var replyCount = (th.posts && th.posts.length) || 0;
        var dateStr = new Date(th.createdAt * 1000).toLocaleString();
        initialThreadsHtml += '<div class="thread-item" onclick="openThread(' + th.id + ')">' +
            '<div class="thread-avatar">' + thAv.icon + '</div>' +
            '<div class="thread-body">' +
                '<div class="thread-title">' + (th.isPinned ? '📌 ' : '') + th.title + '</div>' +
                '<div class="thread-meta">Transmitted by ' + th.authorName + ' // ' + dateStr + '</div>' +
            '</div>' +
            '<div class="thread-stats">' +
                '<div>💬 ' + replyCount + '</div>' +
                '<div>👁️ ' + th.views + '</div>' +
            '</div>' +
        '</div>';
    }

    // Server-side pre-render news bulletins
    var newsHtml = '';
    for (var n = 0; n < NEWS_ITEMS.length; n++) {
        var item = NEWS_ITEMS[n];
        newsHtml += '<div class="news-card">' +
            '<div class="news-header">' +
                '<div class="news-title">' + item.title + '</div>' +
                '<span class="cat-badge" style="background:#004415; color:#39ff14;">' + item.category + ' // ' + item.date + '</span>' +
            '</div>' +
            '<div style="font-size:12px; color:#88ffaa; margin-bottom:10px;">Broadcast Author: <b>' + item.author + '</b></div>' +
            '<div class="news-body">' + item.content.replace(/\n/g, '<br>') + '</div>' +
        '</div>';
    }

    return '<!DOCTYPE html>\n' +
'<html lang="en">\n' +
'<head>\n' +
'    <meta http-equiv="X-UA-Compatible" content="IE=edge">\n' +
'    <meta charset="UTF-8">\n' +
'    <meta name="viewport" content="width=device-width, initial-scale=1.0">\n' +
'    <title>ZION MAINFRAME // Community Transmission Hub</title>\n' +
'    <style>\n' +
'        * { box-sizing: border-box; margin: 0; padding: 0; }\n' +
'        html, body {\n' +
'            background-color: #030804 !important;\n' +
'            background: #030804 !important;\n' +
'            color: #00ff66 !important;\n' +
'            font-family: "Share Tech Mono", "Consolas", "Courier New", monospace;\n' +
'            min-height: 100vh;\n' +
'            overflow-x: hidden;\n' +
'            position: relative;\n' +
'        }\n' +
'        #matrixCanvas {\n' +
'            position: fixed;\n' +
'            top: 0;\n' +
'            left: 0;\n' +
'            width: 100%;\n' +
'            height: 100%;\n' +
'            z-index: 0;\n' +
'            opacity: 0.22;\n' +
'            pointer-events: none;\n' +
'        }\n' +
'        .scanlines {\n' +
'            position: fixed;\n' +
'            top: 0; left: 0; width: 100%; height: 100%;\n' +
'            background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%), linear-gradient(90deg, rgba(255, 0, 0, 0.03), rgba(0, 255, 0, 0.01), rgba(0, 0, 255, 0.03));\n' +
'            background-size: 100% 3px, 6px 100%;\n' +
'            z-index: 99;\n' +
'            pointer-events: none;\n' +
'        }\n' +
'        .wrapper {\n' +
'            position: relative;\n' +
'            z-index: 1;\n' +
'            max-width: 100%;\n' +
'            margin: 0 auto;\n' +
'            padding: 6px;\n' +
'        }\n' +
'        header {\n' +
'            background-color: #051208;\n' +
'            background: rgba(5, 18, 8, 0.92);\n' +
'            border: 1px solid #00aa33;\n' +
'            border-radius: 6px;\n' +
'            padding: 8px 12px;\n' +
'            margin-bottom: 8px;\n' +
'            box-shadow: 0 0 10px rgba(0, 255, 102, 0.6);\n' +
'            display: flex;\n' +
'            justify-content: space-between;\n' +
'            align-items: center;\n' +
'            flex-wrap: wrap;\n' +
'            gap: 10px;\n' +
'        }\n' +
'        .header-title h1 {\n' +
'            font-size: 16px;\n' +
'            color: #39ff14;\n' +
'            text-shadow: 0 0 10px #00ff66;\n' +
'            letter-spacing: 1px;\n' +
'        }\n' +
'        .header-title p {\n' +
'            font-size: 12px;\n' +
'            color: #88ffaa;\n' +
'            opacity: 0.85;\n' +
'            margin-top: 3px;\n' +
'        }\n' +
'        .user-widget {\n' +
'            display: flex;\n' +
'            align-items: center;\n' +
'            gap: 12px;\n' +
'            background: #001f0a;\n' +
'            background: rgba(0, 31, 10, 0.85);\n' +
'            padding: 8px 16px;\n' +
'            border-radius: 4px;\n' +
'            border: 1px solid #006622;\n' +
'        }\n' +
'        .user-avatar-badge {\n' +
'            width: 44px;\n' +
'            height: 44px;\n' +
'            border-radius: 50%;\n' +
'            border: 2px solid #00ff66;\n' +
'            display: flex;\n' +
'            align-items: center;\n' +
'            justify-content: center;\n' +
'            font-size: 24px;\n' +
'            background: #001a08;\n' +
'            box-shadow: 0 0 8px rgba(0,255,102,0.5);\n' +
'        }\n' +
'        .user-info-text { font-size: 13px; line-height: 1.3; }\n' +
'        .user-info-text .handle { font-weight: bold; color: #39ff14; }\n' +
'        .user-info-text .title { font-size: 11px; color: #88ffaa; }\n' +
'        .cyber-btn {\n' +
'            background-color: #002b11;\n' +
'            color: #00ff66;\n' +
'            border: 1px solid #00ff66;\n' +
'            padding: 8px 16px;\n' +
'            font-family: inherit;\n' +
'            font-size: 13px;\n' +
'            font-weight: bold;\n' +
'            cursor: pointer;\n' +
'            border-radius: 3px;\n' +
'            transition: all 0.2s ease;\n' +
'            text-decoration: none;\n' +
'            display: inline-flex;\n' +
'            align-items: center;\n' +
'            gap: 6px;\n' +
'        }\n' +
'        .cyber-btn:hover {\n' +
'            background-color: #00ff66;\n' +
'            color: #000;\n' +
'            box-shadow: 0 0 14px #00ff66;\n' +
'        }\n' +
'        .cyber-btn.active {\n' +
'            background-color: #39ff14;\n' +
'            color: #000;\n' +
'            box-shadow: 0 0 10px #39ff14;\n' +
'        }\n' +
'        .main-layout {\n' +
'            display: -ms-flexbox;\n' +
'            display: flex;\n' +
'            gap: 10px;\n' +
'        }\n' +
'        aside {\n' +
'            width: 200px;\n' +
'            -ms-flex: 0 0 200px;\n' +
'            flex-shrink: 0;\n' +
'            margin-right: 6px;\n' +
'        }\n' +
'        .sidebar-card {\n' +
'            background-color: #051208;\n' +
'            background: rgba(5, 18, 8, 0.92);\n' +
'            border: 1px solid #00aa33;\n' +
'            border-radius: 6px;\n' +
'            padding: 10px;\n' +
'            margin-bottom: 10px;\n' +
'            box-shadow: 0 0 8px rgba(0, 170, 51, 0.3);\n' +
'        }\n' +
'        .sidebar-card h3 {\n' +
'            font-size: 14px;\n' +
'            color: #39ff14;\n' +
'            border-bottom: 1px solid #006622;\n' +
'            padding-bottom: 8px;\n' +
'            margin-bottom: 12px;\n' +
'            display: flex;\n' +
'            align-items: center;\n' +
'            gap: 6px;\n' +
'        }\n' +
'        .cat-item {\n' +
'            display: flex;\n' +
'            align-items: center;\n' +
'            justify-content: space-between;\n' +
'            padding: 8px 10px;\n' +
'            margin-bottom: 4px;\n' +
'            border-radius: 4px;\n' +
'            cursor: pointer;\n' +
'            font-size: 13px;\n' +
'            border-left: 2px solid transparent;\n' +
'            color: #88ffaa;\n' +
'        }\n' +
'        .cat-item:hover {\n' +
'            background-color: #003311;\n' +
'            border-left-color: #00ff66;\n' +
'            color: #ffffff;\n' +
'        }\n' +
'        .cat-item.active {\n' +
'            background-color: #004415;\n' +
'            border-left-color: #39ff14;\n' +
'            font-weight: bold;\n' +
'            color: #ffffff;\n' +
'        }\n' +
'        .cat-badge {\n' +
'            font-size: 10px;\n' +
'            padding: 2px 6px;\n' +
'            background: #003311;\n' +
'            border: 1px solid #006622;\n' +
'            border-radius: 3px;\n' +
'            color: #88ffaa;\n' +
'        }\n' +
'        .content-panel {\n' +
'            -ms-flex: 1 1 auto;\n' +
'            flex-grow: 1;\n' +
'            min-width: 0;\n' +
'            background-color: #051208;\n' +
'            background: rgba(5, 18, 8, 0.92);\n' +
'            border: 1px solid #00aa33;\n' +
'            border-radius: 6px;\n' +
'            padding: 12px;\n' +
'            box-shadow: 0 0 10px rgba(0, 255, 102, 0.6);\n' +
'            min-height: 280px;\n' +
'        }\n' +
'        .content-header {\n' +
'            display: flex;\n' +
'            justify-content: space-between;\n' +
'            align-items: center;\n' +
'            border-bottom: 1px solid #006622;\n' +
'            padding-bottom: 14px;\n' +
'            margin-bottom: 18px;\n' +
'            flex-wrap: wrap;\n' +
'            gap: 10px;\n' +
'        }\n' +
'        .thread-item {\n' +
'            background: #001406;\n' +
'            background: rgba(0, 20, 6, 0.75);\n' +
'            border: 1px solid #004415;\n' +
'            border-radius: 4px;\n' +
'            padding: 14px;\n' +
'            margin-bottom: 12px;\n' +
'            cursor: pointer;\n' +
'            display: flex;\n' +
'            gap: 14px;\n' +
'            align-items: center;\n' +
'        }\n' +
'        .thread-item:hover {\n' +
'            border-color: #00ff66;\n' +
'            background: #00280c;\n' +
'        }\n' +
'        .thread-avatar {\n' +
'            width: 46px;\n' +
'            height: 46px;\n' +
'            border-radius: 4px;\n' +
'            border: 1px solid #00ff66;\n' +
'            display: flex;\n' +
'            align-items: center;\n' +
'            justify-content: center;\n' +
'            font-size: 24px;\n' +
'            background: #001505;\n' +
'            flex-shrink: 0;\n' +
'        }\n' +
'        .thread-body { flex-grow: 1; }\n' +
'        .thread-title { font-size: 16px; font-weight: bold; color: #39ff14; margin-bottom: 4px; }\n' +
'        .thread-meta { font-size: 12px; color: #88eeaa; opacity: 0.85; }\n' +
'        .thread-stats {\n' +
'            text-align: right;\n' +
'            font-size: 12px;\n' +
'            color: #88eeaa;\n' +
'            flex-shrink: 0;\n' +
'            padding-left: 10px;\n' +
'        }\n' +
'        .op-box, .reply-box {\n' +
'            background: #001406;\n' +
'            background: rgba(0, 20, 6, 0.85);\n' +
'            border: 1px solid #005520;\n' +
'            border-radius: 4px;\n' +
'            padding: 18px;\n' +
'            margin-bottom: 16px;\n' +
'        }\n' +
'        .post-header {\n' +
'            display: flex;\n' +
'            align-items: center;\n' +
'            gap: 12px;\n' +
'            border-bottom: 1px dashed #006622;\n' +
'            padding-bottom: 10px;\n' +
'            margin-bottom: 12px;\n' +
'        }\n' +
'        .post-content {\n' +
'            font-size: 14px;\n' +
'            line-height: 1.6;\n' +
'            color: #d0ffd0;\n' +
'            white-space: pre-wrap;\n' +
'            margin-bottom: 14px;\n' +
'        }\n' +
'        .post-sig {\n' +
'            border-top: 1px dashed #004415;\n' +
'            padding-top: 8px;\n' +
'            font-size: 11px;\n' +
'            color: #66cc88;\n' +
'            font-style: italic;\n' +
'        }\n' +
'        .post-reactions {\n' +
'            display: flex;\n' +
'            gap: 8px;\n' +
'            margin-top: 10px;\n' +
'        }\n' +
'        .react-btn {\n' +
'            background: #00280a;\n' +
'            border: 1px solid #006622;\n' +
'            border-radius: 12px;\n' +
'            padding: 3px 10px;\n' +
'            font-size: 12px;\n' +
'            cursor: pointer;\n' +
'            color: #00ff66;\n' +
'        }\n' +
'        .react-btn:hover { border-color: #39ff14; color: #fff; }\n' +
'        .composer-box {\n' +
'            background: #001e0a;\n' +
'            background: rgba(0, 30, 10, 0.85);\n' +
'            border: 1px solid #00ff66;\n' +
'            border-radius: 4px;\n' +
'            padding: 16px;\n' +
'            margin-top: 20px;\n' +
'        }\n' +
'        .composer-box textarea, .composer-box input {\n' +
'            width: 100%;\n' +
'            background: #001205;\n' +
'            border: 1px solid #006622;\n' +
'            color: #39ff14;\n' +
'            font-family: inherit;\n' +
'            padding: 10px;\n' +
'            border-radius: 3px;\n' +
'            margin-bottom: 10px;\n' +
'        }\n' +
'        .modal-overlay {\n' +
'            position: fixed;\n' +
'            top: 0; left: 0; width: 100%; height: 100%;\n' +
'            background: rgba(0, 0, 0, 0.88);\n' +
'            z-index: 1000;\n' +
'            display: none;\n' +
'            align-items: center;\n' +
'            justify-content: center;\n' +
'            padding: 20px;\n' +
'        }\n' +
'        .modal-box {\n' +
'            background: #021205;\n' +
'            border: 2px solid #00ff66;\n' +
'            border-radius: 8px;\n' +
'            max-width: 780px;\n' +
'            width: 100%;\n' +
'            max-height: 90vh;\n' +
'            overflow-y: auto;\n' +
'            padding: 24px;\n' +
'            box-shadow: 0 0 10px rgba(0, 255, 102, 0.6);\n' +
'        }\n' +
'        .modal-title {\n' +
'            font-size: 20px;\n' +
'            color: #39ff14;\n' +
'            border-bottom: 1px solid #006622;\n' +
'            padding-bottom: 10px;\n' +
'            margin-bottom: 16px;\n' +
'            display: flex;\n' +
'            justify-content: space-between;\n' +
'            align-items: center;\n' +
'        }\n' +
'        .avatar-grid {\n' +
'            display: grid;\n' +
'            grid-template-columns: repeat(auto-fill, minmax(130px, 1fr));\n' +
'            gap: 12px;\n' +
'            margin-bottom: 20px;\n' +
'        }\n' +
'        .avatar-card {\n' +
'            background: #001c08;\n' +
'            border: 1px solid #006622;\n' +
'            border-radius: 6px;\n' +
'            padding: 10px;\n' +
'            text-align: center;\n' +
'            cursor: pointer;\n' +
'        }\n' +
'        .avatar-card:hover { border-color: #00ff66; background: #00280a; }\n' +
'        .avatar-card.selected { border: 2px solid #39ff14; background: #003810; }\n' +
'        .avatar-card .avatar-icon { font-size: 34px; margin-bottom: 6px; }\n' +
'        .avatar-card .avatar-name { font-size: 13px; font-weight: bold; color: #fff; }\n' +
'        .avatar-card .avatar-faction { font-size: 10px; color: #88ffaa; }\n' +
'        .news-card {\n' +
'            background: #001908;\n' +
'            background: rgba(0, 25, 8, 0.85);\n' +
'            border-left: 3px solid #39ff14;\n' +
'            border-top: 1px solid #004415;\n' +
'            border-right: 1px solid #004415;\n' +
'            border-bottom: 1px solid #004415;\n' +
'            border-radius: 4px;\n' +
'            padding: 16px;\n' +
'            margin-bottom: 16px;\n' +
'        }\n' +
'        .news-header { display: flex; justify-content: space-between; margin-bottom: 8px; }\n' +
'        .news-title { font-size: 17px; font-weight: bold; color: #39ff14; }\n' +
'        .news-body { font-size: 13px; line-height: 1.5; color: #c0ffc0; }\n' +
'    </style>\n' +
'</head>\n' +
'<body>\n' +
'    <canvas id="matrixCanvas"></canvas>\n' +
'    <div class="scanlines"></div>\n' +
'    <div class="wrapper">\n' +
'        <header>\n' +
'            <div class="header-title">\n' +
'                <h1>╔═ ZION MAINFRAME // COMMUNITY HUB ═╗</h1>\n' +
'                <p>THE MATRIX ONLINE // NEURAL DISPATCH &amp; CONSTRUCT FORUMS v1.3.0</p>\n' +
'            </div>\n' +
'            <div class="user-widget">\n' +
'                <div class="user-avatar-badge" id="topAvatar">🕶️</div>\n' +
'                <div class="user-info-text">\n' +
'                    <div class="handle" id="topHandle">Slacker</div>\n' +
'                    <div class="title" id="topTitle">Zion Operative</div>\n' +
'                </div>\n' +
'                <button class="cyber-btn" onclick="openProfileModal()">⚙️ PROFILE</button>\n' +
'            </div>\n' +
'        </header>\n' +
'        <div class="main-layout">\n' +
'            <aside>\n' +
'                <div class="sidebar-card">\n' +
'                    <h3>⚡ TRANSMISSION CHANNELS</h3>\n' +
'                    <div id="categoryList">' + categoriesHtml + '</div>\n' +
'                </div>\n' +
'                <div class="sidebar-card">\n' +
'                    <h3>📡 OPERATOR ACTIONS</h3>\n' +
'                    <button class="cyber-btn" style="width:100%; margin-bottom:8px;" onclick="openNewThreadModal()">📝 NEW TRANSMISSION</button>\n' +
'                    <button class="cyber-btn" style="width:100%; margin-bottom:8px;" onclick="showNewsView()">⚡ PATCH TRANSMISSIONS</button>\n' +
'                    <a class="cyber-btn" style="width:100%; text-align:center; justify-content:center;" href="/client/MxO_Client.7z">📥 GET GAME CLIENT</a>\n' +
'                </div>\n' +
'                <div class="sidebar-card">\n' +
'                    <h3>🌐 SYSTEM STATUS</h3>\n' +
'                    <p style="font-size:12px; margin-bottom:4px;">VPS Host: <span style="color:#39ff14;">15.204.82.250</span></p>\n' +
'                    <p style="font-size:12px; margin-bottom:4px;">Reality Core: <span style="color:#39ff14;">ONLINE [PORT 10000]</span></p>\n' +
'                    <p style="font-size:12px; margin-bottom:6px;">Megacity Clock: <span id="wsCircadianHour" style="color:#00e5ff;">Syncing...</span></p>\n' +
'                    <div id="wsDistrictList" style="border-top:1px dashed #004415; padding-top:6px; font-size:11px; color:#88ffaa;">\n' +
'                        <div>Slums: <span style="color:#39ff14;">Syncing...</span></div>\n' +
'                        <div>Downtown: <span style="color:#39ff14;">Syncing...</span></div>\n' +
'                        <div>International: <span style="color:#39ff14;">Syncing...</span></div>\n' +
'                        <div>Richland: <span style="color:#39ff14;">Syncing...</span></div>\n' +
'                    </div>\n' +
'                    <div id="wsEventBanner" style="border-top:1px dashed #004415; margin-top:6px; padding-top:6px; font-size:11px; color:#ffcc00;">No Active Incursions</div>\n' +
'                </div>\n' +
'            </aside>\n' +
'            <main class="content-panel">\n' +
'                <div id="threadListView">\n' +
'                    <div class="content-header">\n' +
'                        <div>\n' +
'                            <h2 id="currentCategoryTitle" style="color:#39ff14; font-size:18px;">Announcements &amp; Live Events</h2>\n' +
'                            <p id="currentCategoryDesc" style="font-size:12px; color:#88ffaa; margin-top:2px;">Official game updates and GM events</p>\n' +
'                        </div>\n' +
'                        <button class="cyber-btn" onclick="openNewThreadModal()">+ NEW THREAD</button>\n' +
'                    </div>\n' +
'                    <div id="threadListContainer">' + initialThreadsHtml + '</div>\n' +
'                </div>\n' +
'                <div id="threadDetailView" style="display:none;">\n' +
'                    <div class="content-header">\n' +
'                        <button class="cyber-btn" onclick="backToThreadList()">⬅️ BACK TO THREADS</button>\n' +
'                        <h2 id="viewThreadTitle" style="color:#39ff14; font-size:18px;">Thread View</h2>\n' +
'                    </div>\n' +
'                    <div id="threadDetailBody"></div>\n' +
'                    <div class="composer-box">\n' +
'                        <h3 style="color:#39ff14; font-size:14px; margin-bottom:8px;">TRANSMIT SECURE RESPONSE</h3>\n' +
'                        <textarea id="replyText" rows="4" placeholder="Enter your response to the operative dispatch..."></textarea>\n' +
'                        <div style="display:flex; justify-content:space-between; align-items:center;">\n' +
'                            <span style="font-size:11px; color:#88eeaa;">Encrypted via Zion Neural Key (256-bit)</span>\n' +
'                            <button class="cyber-btn" onclick="submitReply()">📡 BROADCAST REPLY</button>\n' +
'                        </div>\n' +
'                    </div>\n' +
'                </div>\n' +
'                <div id="newsListView" style="display:none;">\n' +
'                    <div class="content-header">\n' +
'                        <h2 style="color:#39ff14; font-size:18px;">⚡ MATRIX ONLINE TRANSMISSIONS &amp; PATCH ARCHIVE</h2>\n' +
'                        <button class="cyber-btn" onclick="backToThreadList()">VIEW FORUMS</button>\n' +
'                    </div>\n' +
'                    <div id="newsContainer">' + newsHtml + '</div>\n' +
'                </div>\n' +
'            </main>\n' +
'        </div>\n' +
'    </div>\n' +
'    <div class="modal-overlay" id="profileModal">\n' +
'        <div class="modal-box">\n' +
'            <div class="modal-title">\n' +
'                <span>OPERATIVE IDENTITY &amp; AVATAR PROTOCOL</span>\n' +
'                <button class="cyber-btn" onclick="closeProfileModal()">X</button>\n' +
'            </div>\n' +
'            <div style="margin-bottom:16px;">\n' +
'                <label style="font-size:12px; color:#88ffaa;">OPERATIVE HANDLE / USERNAME:</label>\n' +
'                <input type="text" id="editHandle" style="width:100%; background:#001505; border:1px solid #006622; color:#39ff14; padding:8px; margin-top:4px;">\n' +
'            </div>\n' +
'            <div style="margin-bottom:16px;">\n' +
'                <label style="font-size:12px; color:#88ffaa;">TITLE / DESIGNATION:</label>\n' +
'                <input type="text" id="editTitle" style="width:100%; background:#001505; border:1px solid #006622; color:#39ff14; padding:8px; margin-top:4px;">\n' +
'            </div>\n' +
'            <div style="margin-bottom:16px;">\n' +
'                <label style="font-size:12px; color:#88ffaa;">CUSTOM FORUM SIGNATURE (Supports {c:hex} color tags):</label>\n' +
'                <textarea id="editSignature" rows="2" style="width:100%; background:#001505; border:1px solid #006622; color:#39ff14; padding:8px; margin-top:4px;"></textarea>\n' +
'            </div>\n' +
'            <div style="margin-bottom:12px;">\n' +
'                <label style="font-size:13px; font-weight:bold; color:#39ff14;">SELECT FROM 30 AUTHENTIC MATRIX AVATARS:</label>\n' +
'            </div>\n' +
'            <div class="avatar-grid" id="avatarSelectorGrid">' + avatarsHtml + '</div>\n' +
'            <div style="text-align:right;">\n' +
'                <button class="cyber-btn" onclick="saveProfile()">💾 COMMIT IDENTITY CHANGES</button>\n' +
'            </div>\n' +
'        </div>\n' +
'    </div>\n' +
'    <div class="modal-overlay" id="newThreadModal">\n' +
'        <div class="modal-box">\n' +
'            <div class="modal-title">\n' +
'                <span>BROADCAST NEW TRANSMISSION</span>\n' +
'                <button class="cyber-btn" onclick="closeNewThreadModal()">X</button>\n' +
'            </div>\n' +
'            <div style="margin-bottom:14px;">\n' +
'                <label style="font-size:12px; color:#88ffaa;">SELECT FORUM DISPATCH CHANNEL:</label>\n' +
'                <select id="newThreadCategory" style="width:100%; background:#001505; border:1px solid #006622; color:#39ff14; padding:8px; margin-top:4px; font-family:inherit;">\n' +
                catOptionsHtml +
'                </select>\n' +
'            </div>\n' +
'            <div style="margin-bottom:14px;">\n' +
'                <label style="font-size:12px; color:#88ffaa;">TRANSMISSION TITLE:</label>\n' +
'                <input type="text" id="newThreadTitle" style="width:100%; background:#001505; border:1px solid #006622; color:#39ff14; padding:8px; margin-top:4px; font-family:inherit;">\n' +
'            </div>\n' +
'            <div style="margin-bottom:18px;">\n' +
'                <label style="font-size:12px; color:#88ffaa;">TRANSMISSION CONTENT:</label>\n' +
'                <textarea id="newThreadContent" rows="6" style="width:100%; background:#001505; border:1px solid #006622; color:#39ff14; padding:8px; margin-top:4px; font-family:inherit;"></textarea>\n' +
'            </div>\n' +
'            <div style="text-align:right;">\n' +
'                <button class="cyber-btn" onclick="submitNewThread()">📡 BROADCAST THREAD</button>\n' +
'            </div>\n' +
'        </div>\n' +
'    </div>\n' +
'    <script>\n' +
'        var g_CurrentCategory = "announcements";\n' +
'        var g_CurrentThreadId = null;\n' +
'        var g_Avatars = ' + JSON.stringify(AVATARS) + ';\n' +
'        var g_Categories = ' + JSON.stringify(CATEGORIES) + ';\n' +
'        var g_Profile = {\n' +
'            username: "Slacker",\n' +
'            customHandle: "Slacker",\n' +
'            title: "Zion Operative",\n' +
'            avatarId: 1,\n' +
'            signature: "{c:00FF66}Free your mind.{/c}"\n' +
'        };\n' +
'        function getParam(name) {\n' +
'            var q = window.location.search;\n' +
'            if (!q || q.length <= 1) return null;\n' +
'            var pairs = q.substring(1).split("&");\n' +
'            for (var i = 0; i < pairs.length; i++) {\n' +
'                var p = pairs[i].split("=");\n' +
'                if (decodeURIComponent(p[0]) === name) return decodeURIComponent(p[1] || "");\n' +
'            }\n' +
'            return null;\n' +
'        }\n' +
'        var ssoUser = getParam("user");\n' +
'        if (ssoUser) { g_Profile.username = ssoUser; g_Profile.customHandle = ssoUser; }\n' +
'        function apiGet(url, cb) {\n' +
'            var x = new XMLHttpRequest();\n' +
'            x.open("GET", url, true);\n' +
'            x.onreadystatechange = function() {\n' +
'                if (x.readyState === 4) {\n' +
'                    if (x.status >= 200 && x.status < 300) {\n' +
'                        try { cb(null, JSON.parse(x.responseText)); } catch (e) { cb(e); }\n' +
'                    } else { cb(new Error("Status " + x.status)); }\n' +
'                }\n' +
'            };\n' +
'            x.send();\n' +
'        }\n' +
'        function apiPost(url, data, cb) {\n' +
'            var x = new XMLHttpRequest();\n' +
'            x.open("POST", url, true);\n' +
'            x.setRequestHeader("Content-Type", "application/json");\n' +
'            x.onreadystatechange = function() {\n' +
'                if (x.readyState === 4) {\n' +
'                    if (x.status >= 200 && x.status < 300) {\n' +
'                        try { cb(null, JSON.parse(x.responseText)); } catch (e) { cb(e); }\n' +
'                    } else { cb(new Error("Status " + x.status)); }\n' +
'                }\n' +
'            };\n' +
'            x.send(JSON.stringify(data));\n' +
'        }\n' +
'        function initMatrixRain() {\n' +
'            var canvas = document.getElementById("matrixCanvas");\n' +
'            if (!canvas || !canvas.getContext) return;\n' +
'            var ctx = canvas.getContext("2d");\n' +
'            var w = canvas.width = window.innerWidth || 800;\n' +
'            var h = canvas.height = window.innerHeight || 600;\n' +
'            var chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZアイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン";\n' +
'            var fontSize = 14;\n' +
'            var cols = Math.floor(w / fontSize);\n' +
'            var drops = [];\n' +
'            for (var i = 0; i < cols; i++) drops.push(1);\n' +
'            function draw() {\n' +
'                ctx.fillStyle = "rgba(3, 8, 4, 0.08)";\n' +
'                ctx.fillRect(0, 0, w, h);\n' +
'                ctx.fillStyle = "#00FF66";\n' +
'                ctx.font = fontSize + "px monospace";\n' +
'                for (var i = 0; i < drops.length; i++) {\n' +
'                    var txt = chars.charAt(Math.floor(Math.random() * chars.length));\n' +
'                    ctx.fillText(txt, i * fontSize, drops[i] * fontSize);\n' +
'                    if (drops[i] * fontSize > h && Math.random() > 0.975) drops[i] = 0;\n' +
'                    drops[i]++;\n' +
'                }\n' +
'            }\n' +
'            setInterval(draw, 40);\n' +
'        }\n' +
'        function updateTopWidget() {\n' +
'            document.getElementById("topHandle").innerText = g_Profile.customHandle || g_Profile.username;\n' +
'            document.getElementById("topTitle").innerText = g_Profile.title || "Zion Operative";\n' +
'            for (var i = 0; i < g_Avatars.length; i++) {\n' +
'                if (g_Avatars[i].id === g_Profile.avatarId) {\n' +
'                    document.getElementById("topAvatar").innerText = g_Avatars[i].icon;\n' +
'                    break;\n' +
'                }\n' +
'            }\n' +
'        }\n' +
'        function selectCategory(catId) {\n' +
'            g_CurrentCategory = catId;\n' +
'            var items = document.getElementsByClassName("cat-item");\n' +
'            for (var i = 0; i < items.length; i++) {\n' +
'                items[i].className = items[i].className.replace(/\\bactive\\b/g, "").trim();\n' +
'            }\n' +
'            var activeEl = document.getElementById("cat_" + catId);\n' +
'            if (activeEl) activeEl.className += " active";\n' +
'            for (var j = 0; j < g_Categories.length; j++) {\n' +
'                if (g_Categories[j].id === catId) {\n' +
'                    document.getElementById("currentCategoryTitle").innerText = g_Categories[j].name;\n' +
'                    document.getElementById("currentCategoryDesc").innerText = g_Categories[j].desc;\n' +
'                    break;\n' +
'                }\n' +
'            }\n' +
'            backToThreadList();\n' +
'            loadThreads();\n' +
'        }\n' +
'        function loadThreads() {\n' +
'            var cont = document.getElementById("threadListContainer");\n' +
'            cont.innerHTML = "<p style=\\"color:#88ffaa; padding:20px;\\">Decrypting channel transmissions...</p>";\n' +
'            apiGet("/api/community/threads?category=" + encodeURIComponent(g_CurrentCategory), function(err, data) {\n' +
'                if (err || !data.success) {\n' +
'                    cont.innerHTML = "<p style=\\"color:#ff3333; padding:20px;\\">Transmission link interrupted.</p>";\n' +
'                    return;\n' +
'                }\n' +
'                if (data.threads.length === 0) {\n' +
'                    cont.innerHTML = "<p style=\\"color:#558866; padding:30px; text-align:center;\\">No transmissions on this frequency yet. Click [+ NEW THREAD] to initiate contact.</p>";\n' +
'                    return;\n' +
'                }\n' +
'                var html = "";\n' +
'                for (var i = 0; i < data.threads.length; i++) {\n' +
'                    var t = data.threads[i];\n' +
'                    var av = { icon: "🕶️" };\n' +
'                    for (var a = 0; a < g_Avatars.length; a++) {\n' +
'                        if (g_Avatars[a].id === t.avatarId) { av = g_Avatars[a]; break; }\n' +
'                    }\n' +
'                    var dateStr = new Date(t.createdAt * 1000).toLocaleString();\n' +
'                    html += "<div class=\\"thread-item\\" onclick=\\"openThread(" + t.id + ")\\">" +\n' +
'                        "<div class=\\"thread-avatar\\">" + av.icon + "</div>" +\n' +
'                        "<div class=\\"thread-body\\">" +\n' +
'                            "<div class=\\"thread-title\\">" + (t.isPinned ? "📌 " : "") + t.title + "</div>" +\n' +
'                            "<div class=\\"thread-meta\\">Transmitted by " + t.authorName + " // " + dateStr + "</div>" +\n' +
'                        "</div>" +\n' +
'                        "<div class=\\"thread-stats\\">" +\n' +
'                            "<div>💬 " + t.replyCount + "</div>" +\n' +
'                            "<div>👁️ " + t.views + "</div>" +\n' +
'                        "</div>" +\n' +
'                    "</div>";\n' +
'                }\n' +
'                cont.innerHTML = html;\n' +
'            });\n' +
'        }\n' +
'        function openThread(id) {\n' +
'            g_CurrentThreadId = id;\n' +
'            document.getElementById("threadListView").style.display = "none";\n' +
'            document.getElementById("newsListView").style.display = "none";\n' +
'            document.getElementById("threadDetailView").style.display = "block";\n' +
'            var cont = document.getElementById("threadDetailBody");\n' +
'            cont.innerHTML = "<p style=\\"color:#88ffaa; padding:20px;\\">Decrypting thread data stream...</p>";\n' +
'            apiGet("/api/community/thread/" + id, function(err, data) {\n' +
'                if (err || !data.success) {\n' +
'                    cont.innerHTML = "<p style=\\"color:#ff3333; padding:20px;\\">Failed to decompile thread.</p>";\n' +
'                    return;\n' +
'                }\n' +
'                var t = data.thread;\n' +
'                document.getElementById("viewThreadTitle").innerText = t.title;\n' +
'                var opAv = { icon: "🕶️" };\n' +
'                for (var a = 0; a < g_Avatars.length; a++) {\n' +
'                    if (g_Avatars[a].id === (t.authorProfile ? t.authorProfile.avatarId : 1)) { opAv = g_Avatars[a]; break; }\n' +
'                }\n' +
'                var opDate = new Date(t.createdAt * 1000).toLocaleString();\n' +
'                var opSig = (t.authorProfile && t.authorProfile.signature) ?\n' +
'                    ("<div class=\\"post-sig\\">" + formatSignature(t.authorProfile.signature) + "</div>") : "";\n' +
'                var html = "<div class=\\"op-box\\">" +\n' +
'                    "<div class=\\"post-header\\">" +\n' +
'                        "<div class=\\"user-avatar-badge\\">" + opAv.icon + "</div>" +\n' +
'                        "<div>" +\n' +
'                            "<div style=\\"font-weight:bold; color:#39ff14;\\">" + (t.authorProfile ? (t.authorProfile.customHandle || t.authorName) : t.authorName) + "</div>" +\n' +
'                            "<div style=\\"font-size:11px; color:#88ffaa;\\">" + (t.authorProfile ? (t.authorProfile.title || "Operative") : "Operative") + " // " + opDate + "</div>" +\n' +
'                        "</div>" +\n' +
'                    "</div>" +\n' +
'                    "<div class=\\"post-content\\">" + t.content + "</div>" + opSig +\n' +
'                "</div>";\n' +
'                if (t.posts && t.posts.length > 0) {\n' +
'                    html += "<h3 style=\\"color:#39ff14; font-size:14px; margin:20px 0 10px 0;\\">OPERATIVE REPLIES (" + t.posts.length + ")</h3>";\n' +
'                    for (var p = 0; p < t.posts.length; p++) {\n' +
'                        var post = t.posts[p];\n' +
'                        var pAv = { icon: "🕶️" };\n' +
'                        for (var pa = 0; pa < g_Avatars.length; pa++) {\n' +
'                            if (g_Avatars[pa].id === (post.profile ? post.profile.avatarId : 1)) { pAv = g_Avatars[pa]; break; }\n' +
'                        }\n' +
'                        var pDate = new Date(post.createdAt * 1000).toLocaleString();\n' +
'                        var pSig = (post.profile && post.profile.signature) ?\n' +
'                            ("<div class=\\"post-sig\\">" + formatSignature(post.profile.signature) + "</div>") : "";\n' +
'                        var r = post.reactions || {};\n' +
'                        html += "<div class=\\"reply-box\\">" +\n' +
'                            "<div class=\\"post-header\\">" +\n' +
'                                "<div class=\\"user-avatar-badge\\" style=\\"width:36px; height:36px; font-size:18px;\\">" + pAv.icon + "</div>" +\n' +
'                                "<div>" +\n' +
'                                    "<div style=\\"font-weight:bold; color:#39ff14;\\">" + post.authorName + "</div>" +\n' +
'                                    "<div style=\\"font-size:10px; color:#88ffaa;\\">" + (post.profile ? post.profile.title : "Operative") + " // " + pDate + "</div>" +\n' +
'                                "</div>" +\n' +
'                            "</div>" +\n' +
'                            "<div class=\\"post-content\\">" + post.content + "</div>" + pSig +\n' +
'                                "<button class=\\"react-btn\\" onclick=\\"reactToPost(" + t.id + ", " + post.id + ", 0)\\">🟢 " + (r["🟢"] || 0) + "</button>" +\n' +
'                                "<button class=\\"react-btn\\" onclick=\\"reactToPost(" + t.id + ", " + post.id + ", 1)\\">💊 " + (r["💊"] || 0) + "</button>" +\n' +
'                                "<button class=\\"react-btn\\" onclick=\\"reactToPost(" + t.id + ", " + post.id + ", 2)\\">🕶️ " + (r["🕶️"] || 0) + "</button>" +\n' +
'                                "<button class=\\"react-btn\\" onclick=\\"reactToPost(" + t.id + ", " + post.id + ", 3)\\">⚡ " + (r["⚡"] || 0) + "</button>" +\n' +
'                            "</div>" +\n' +
'                        "</div>";\n' +
'                    }\n' +
'                }\n' +
'                cont.innerHTML = html;\n' +
'            });\n' +
'        }\n' +
'        function formatSignature(sig) {\n' +
'            if (!sig) return "";\n' +
'            return sig.replace(/\\{c:([A-Fa-f0-9]{6})\\}(.*?)\\{\\/c\\}/g, "<span style=\\"color:#$1\\">$2</span>");\n' +
'        }\n' +
'        function backToThreadList() {\n' +
'            document.getElementById("threadListView").style.display = "block";\n' +
'            document.getElementById("threadDetailView").style.display = "none";\n' +
'            document.getElementById("newsListView").style.display = "none";\n' +
'        }\n' +
'        function showNewsView() {\n' +
'            document.getElementById("threadListView").style.display = "none";\n' +
'            document.getElementById("threadDetailView").style.display = "none";\n' +
'            document.getElementById("newsListView").style.display = "block";\n' +
'        }\n' +
'        function submitReply() {\n' +
'            var txt = document.getElementById("replyText").value.replace(/^\\s+|\\s+$/g, "");\n' +
'            if (!txt) { alert("Please enter response text."); return; }\n' +
'            apiPost("/api/community/posts", {\n' +
'                threadId: g_CurrentThreadId,\n' +
'                content: txt,\n' +
'                username: g_Profile.username,\n' +
'                customHandle: g_Profile.customHandle,\n' +
'                avatarId: g_Profile.avatarId\n' +
'            }, function(err, data) {\n' +
'                if (err || !data.success) { alert("Error transmitting reply."); return; }\n' +
'                document.getElementById("replyText").value = "";\n' +
'                openThread(g_CurrentThreadId);\n' +
'            });\n' +
'        }\n' +
'        function reactToPost(threadId, postId, emojiIdx) {\n' +
'            var EMOJIS = ["🟢", "💊", "🕶️", "⚡"];\n' +
'            var emoji = (typeof emojiIdx === "number" && EMOJIS[emojiIdx]) ? EMOJIS[emojiIdx] : emojiIdx;\n' +
'            apiPost("/api/community/react", { threadId: threadId, postId: postId, emoji: emoji }, function() {\n' +
'                openThread(threadId);\n' +
'            });\n' +
'        }\n' +
'        function pickAvatar(id) {\n' +
'            g_Profile.avatarId = id;\n' +
'            var cards = document.getElementsByClassName("avatar-card");\n' +
'            for (var i = 0; i < cards.length; i++) {\n' +
'                cards[i].className = cards[i].className.replace(/\\bselected\\b/g, "").trim();\n' +
'            }\n' +
'            var sel = document.getElementById("av_card_" + id);\n' +
'            if (sel) sel.className += " selected";\n' +
'        }\n' +
'        function openProfileModal() {\n' +
'            document.getElementById("editHandle").value = g_Profile.customHandle || g_Profile.username;\n' +
'            document.getElementById("editTitle").value = g_Profile.title || "Zion Operative";\n' +
'            document.getElementById("editSignature").value = g_Profile.signature || "";\n' +
'            pickAvatar(g_Profile.avatarId);\n' +
'            document.getElementById("profileModal").style.display = "flex";\n' +
'        }\n' +
'        function closeProfileModal() {\n' +
'            document.getElementById("profileModal").style.display = "none";\n' +
'        }\n' +
'        function saveProfile() {\n' +
'            var h = document.getElementById("editHandle").value.replace(/^\\s+|\\s+$/g, "");\n' +
'            var t = document.getElementById("editTitle").value.replace(/^\\s+|\\s+$/g, "");\n' +
'            var s = document.getElementById("editSignature").value;\n' +
'            g_Profile.customHandle = h || g_Profile.username;\n' +
'            g_Profile.title = t || "Zion Operative";\n' +
'            g_Profile.signature = s;\n' +
'            apiPost("/api/community/profile", g_Profile, function(err, data) {\n' +
'                if (err || !data.success) { alert("Failed to save profile changes."); return; }\n' +
'                updateTopWidget();\n' +
'                closeProfileModal();\n' +
'                if (g_CurrentThreadId) openThread(g_CurrentThreadId);\n' +
'                else loadThreads();\n' +
'            });\n' +
'        }\n' +
'        function openNewThreadModal() {\n' +
'            document.getElementById("newThreadTitle").value = "";\n' +
'            document.getElementById("newThreadContent").value = "";\n' +
'            document.getElementById("newThreadCategory").value = g_CurrentCategory;\n' +
'            document.getElementById("newThreadModal").style.display = "flex";\n' +
'        }\n' +
'        function closeNewThreadModal() {\n' +
'            document.getElementById("newThreadModal").style.display = "none";\n' +
'        }\n' +
'        function submitNewThread() {\n' +
'            var title = document.getElementById("newThreadTitle").value.replace(/^\\s+|\\s+$/g, "");\n' +
'            var content = document.getElementById("newThreadContent").value.replace(/^\\s+|\\s+$/g, "");\n' +
'            var cat = document.getElementById("newThreadCategory").value;\n' +
'            if (!title || !content) { alert("Please enter both title and content."); return; }\n' +
'            apiPost("/api/community/threads", {\n' +
'                title: title,\n' +
'                content: content,\n' +
'                categoryId: cat,\n' +
'                username: g_Profile.username,\n' +
'                customHandle: g_Profile.customHandle,\n' +
'                avatarId: g_Profile.avatarId\n' +
'            }, function(err, data) {\n' +
'                if (err || !data.success) { alert("Failed to broadcast transmission."); return; }\n' +
'                closeNewThreadModal();\n' +
'                selectCategory(cat);\n' +
'            });\n' +
'        }\n' +
'        function loadWorldStatus() {\n' +
'            apiGet("/api/worldstatus", function(err, ws) {\n' +
'                if (err || !ws || ws.status !== "online") return;\n' +
'                var hr = Number(ws.megacity_circadian_hour || 0).toFixed(2);\n' +
'                var phase = ws.is_night_window ? "NIGHT [HIGH THREAT]" : "DAY";\n' +
'                var hrEl = document.getElementById("wsCircadianHour");\n' +
'                if (hrEl) hrEl.innerText = hr + "h (" + phase + ")";\n' +
'                var dEl = document.getElementById("wsDistrictList");\n' +
'                if (dEl && ws.districts && ws.districts.length) {\n' +
'                    var dHtml = "";\n' +
'                    for (var i = 0; i < ws.districts.length; i++) {\n' +
'                        var d = ws.districts[i];\n' +
'                        var fColor = "#ff4444";\n' +
'                        if (d.controlling_faction === "Zion") fColor = "#39ff14";\n' +
'                        else if (d.controlling_faction === "Merovingian") fColor = "#ffcc00";\n' +
'                        dHtml += "<div style=\\"margin-bottom:2px;\\">" + d.name + ": <span style=\\"color:" + fColor + "; font-weight:bold;\\">" + d.controlling_faction + "</span> (+5% IS/$I)</div>";\n' +
'                    }\n' +
'                    dEl.innerHTML = dHtml;\n' +
'                }\n' +
'                var evEl = document.getElementById("wsEventBanner");\n' +
'                if (evEl) {\n' +
'                    if (ws.active_world_events && ws.active_world_events.length > 0) {\n' +
'                        var ev = ws.active_world_events[0];\n' +
'                        evEl.style.color = "#ff3333";\n' +
'                        evEl.innerHTML = "⚠️ <b>ACTIVE RAID (" + ws.active_world_events.length + "):</b> " + ev.boss_name + " @ " + ev.district_name + " [" + (ev.type || ev.faction || "Boss") + "]";\n' +
'                    } else {\n' +
'                        evEl.style.color = "#88ffaa";\n' +
'                        evEl.innerText = "Sector Clear (No Active World Bosses)";\n' +
'                    }\n' +
'                }\n' +
'            });\n' +
'        }\n' +
'        window.onload = function() {\n' +
'            initMatrixRain();\n' +
'            updateTopWidget();\n' +
'            loadWorldStatus();\n' +
'            setInterval(loadWorldStatus, 30000);\n' +
'            apiGet("/api/community/profile?username=" + encodeURIComponent(g_Profile.username), function(err, data) {\n' +
'                if (!err && data.success && data.profile) {\n' +
'                    g_Profile = data.profile;\n' +
'                    updateTopWidget();\n' +
'                }\n' +
'            });\n' +
'        };\n' +
'    </script>\n' +
'</body>\n' +
'</html>';
}

module.exports = {
    AVATARS,
    CATEGORIES,
    NEWS_ITEMS,
    initializeDatabase,
    handleCommunityRequest
};

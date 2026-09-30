const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

let mysql;
try {
    mysql = require('mysql2/promise');
} catch (e) {
    console.log('[PatchServer] mysql2 not yet installed in local environment, will be available in container.');
}

const PORT = parseInt(process.env.PORT || '80', 10);
const PATCH_DIR = path.join(__dirname, 'patch_data');
const CRASHDUMPS_DIR = path.join(PATCH_DIR, 'crashdumps');

if (!fs.existsSync(CRASHDUMPS_DIR)) {
    fs.mkdirSync(CRASHDUMPS_DIR, { recursive: true });
}

if (!fs.existsSync(PATCH_DIR)) {
    fs.mkdirSync(PATCH_DIR, { recursive: true });
}

// Database Connection Pool (communicates across Docker internal network)
const DB_HOST = process.env.DB_HOST || 'database';
const DB_PORT = parseInt(process.env.DB_PORT || '3306', 10);
const DB_USER = process.env.DB_USER || 'reality';
const DB_PASSWORD = process.env.DB_PASSWORD || 'reality';
const DB_NAME = process.env.DB_NAME || 'reality';

let pool = null;
if (mysql) {
    pool = mysql.createPool({
        host: DB_HOST,
        port: DB_PORT,
        user: DB_USER,
        password: DB_PASSWORD,
        database: DB_NAME,
        waitForConnections: true,
        connectionLimit: 10,
        queueLimit: 0
    });
}

// Password Hashing Functions matching AuthServer C++
function sha1(str) {
    return crypto.createHash('sha1').update(str).digest('hex').toLowerCase();
}

function hashPassword(salt, password) {
    const thingToHash = sha1(salt) + sha1(password);
    return sha1(thingToHash);
}

function generateSalt(length = 8) {
    const chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=';
    let salt = '';
    for (let i = 0; i < length; i++) {
        salt += chars.charAt(Math.floor(Math.random() * chars.length));
    }
    return salt;
}

// Helper to read raw request body (up to maxBytes)
function readRawBody(req, maxBytes = 50 * 1024 * 1024) {
    return new Promise((resolve, reject) => {
        const chunks = [];
        let total = 0;
        req.on('data', chunk => {
            total += chunk.length;
            if (total > maxBytes) {
                req.destroy();
                reject(new Error('Payload exceeds maximum allowed size'));
                return;
            }
            chunks.push(chunk);
        });
        req.on('end', () => resolve(Buffer.concat(chunks)));
        req.on('error', reject);
    });
}

// Multipart/form-data parser for retail crashreporter.exe uploads
function parseMultipartBuffer(buffer, boundary) {
    const fields = {};
    const files = [];
    if (!boundary || !buffer || buffer.length === 0) return { fields, files };

    boundary = boundary.replace(/^"|"$/g, '');
    let boundaryMarker = '--' + boundary;
    let bBuf = Buffer.from(boundaryMarker);
    let startIdx = buffer.indexOf(bBuf);
    if (startIdx === -1 && buffer.indexOf(Buffer.from(boundary)) !== -1) {
        boundaryMarker = boundary;
        bBuf = Buffer.from(boundaryMarker);
        startIdx = buffer.indexOf(bBuf);
    }

    while (startIdx !== -1 && startIdx < buffer.length) {
        startIdx += bBuf.length;
        if (startIdx + 2 <= buffer.length && buffer[startIdx] === 0x2D && buffer[startIdx + 1] === 0x2D) {
            break;
        }
        if (startIdx + 2 <= buffer.length && buffer[startIdx] === 0x0D && buffer[startIdx + 1] === 0x0A) {
            startIdx += 2;
        }

        const headerEnd = buffer.indexOf(Buffer.from('\r\n\r\n'), startIdx);
        if (headerEnd === -1) break;

        const headerStr = buffer.slice(startIdx, headerEnd).toString('latin1');
        const contentStart = headerEnd + 4;

        const nextBIdx = buffer.indexOf(bBuf, contentStart);
        const contentEnd = (nextBIdx !== -1) ? (nextBIdx >= 2 && buffer[nextBIdx - 2] === 0x0D && buffer[nextBIdx - 1] === 0x0A ? nextBIdx - 2 : nextBIdx) : buffer.length;

        const bodySlice = buffer.slice(contentStart, contentEnd);
        const nameMatch = headerStr.match(/name="([^"]+)"/i);
        const fnMatch = headerStr.match(/filename="([^"]+)"/i);

        if (nameMatch) {
            const fieldName = nameMatch[1];
            if (fnMatch) {
                files.push({
                    fieldName: fieldName,
                    filename: fnMatch[1],
                    data: bodySlice
                });
            } else {
                fields[fieldName] = bodySlice.toString('utf8').trim();
            }
        }

        startIdx = nextBIdx;
    }

    return { fields, files };
}

// Helper to parse JSON body
function parseJsonBody(req) {
    return new Promise((resolve, reject) => {
        let body = '';
        req.on('data', chunk => {
            body += chunk.toString();
            if (body.length > 1e6) { // 1MB limit
                req.destroy();
                reject(new Error('Body too large'));
            }
        });
        req.on('end', () => {
            if (!body) return resolve({});
            try {
                resolve(JSON.parse(body));
            } catch (err) {
                reject(err);
            }
        });
        req.on('error', reject);
    });
}

// Generate default patch manifest if not present
const manifestJsonPath = path.join(PATCH_DIR, 'patch_manifest.json');
const defaultManifest = {
    version: "7.6005",
    serverName: "Reality",
    authServer: "15.204.82.250:11000",
    gameServer: "15.204.82.250:10000",
    patchServer: "http://15.204.82.250",
    launcher: {
        version: "1.2.0",
        minVersion: "1.0.0",
        url: "/launcher/ZionLauncher.exe",
        filename: "ZionLauncher.exe",
        notes: "Matrix Digital Rain loader & auto-updater"
    },
    clientPackage: {
        filename: "MxO_Client.7z",
        url: "/client/MxO_Client.7z",
        sizeBytes: 1327331186,
        compressed: true,
        compressionType: "7z"
    },
    patchZip: {
        filename: "MxO_7.6004p.zip",
        url: "/client/MxO_7.6004p.zip"
    }
};

try {
    if (!fs.existsSync(manifestJsonPath)) {
        fs.writeFileSync(manifestJsonPath, JSON.stringify(defaultManifest, null, 2));
    }
} catch (err) {
    console.log('[PatchServer] Notice: manifest writing skipped:', err.message);
}

const server = http.createServer(async (req, res) => {
    const urlObj = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
    const pathname = decodeURIComponent(urlObj.pathname);

    console.log(`[PatchServer] ${req.method} ${pathname}`);

    // Enable CORS for web/launcher clients
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, HEAD, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Range, Content-Type, Authorization');

    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        res.end();
        return;
    }

    if (pathname === '/favicon.ico') {
        res.writeHead(204);
        res.end();
        return;
    }

    // 1. Landing Page (Root /)
    if (pathname === '/' || pathname === '/index.html') {
        res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
        res.end(`<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Zion Mainframe // The Matrix Online</title>
    <style>
        body { background-color: #0d1117; color: #00ff66; font-family: 'Consolas', monospace; padding: 40px; }
        h1 { color: #39ff14; text-shadow: 0 0 10px #00ff66; }
        .card { background: #161b22; border: 1px solid #30363d; border-radius: 8px; padding: 20px; max-width: 650px; margin-bottom: 20px; }
        .btn { display: inline-block; background: #005a20; color: #39ff14; border: 1px solid #00ff66; padding: 10px 20px; text-decoration: none; border-radius: 4px; font-weight: bold; margin-top: 10px; }
        .btn:hover { background: #008f39; color: #fff; }
        .stat { color: #58a6ff; font-weight: bold; }
    </style>
</head>
<body>
    <h1>[ZION] THE MATRIX ONLINE // REALITY SERVER</h1>
    <div class="card">
        <h3>SERVER TELEMETRY</h3>
        <p>Host: <span class="stat">15.204.82.250</span></p>
        <p>Auth Daemon: <span class="stat">Port 11000 TCP [ONLINE]</span></p>
        <p>Game World: <span class="stat">Port 10000 TCP/UDP [ONLINE]</span></p>
        <p>REST API: <span class="stat">Port 80 HTTP [SECURED]</span></p>
        <p>Database: <span class="stat">Isolated Internal Network [ENFORCED]</span></p>
        <p>Crash Telemetry: <span class="stat">crashdumps.lith.thematrixonline.net/upload/ [ONLINE]</span></p>
    </div>
    <div class="card">
        <h3>GAME CLIENT DOWNLOAD</h3>
        <p>Download the authentic Matrix Online client archive directly from this server:</p>
        <a class="btn" href="/client/MxO_Client.7z">DOWNLOAD CLIENT (MxO_Client.7z - 1.26 GB)</a>
        <br><br>
        <a class="btn" href="/patch/patch_manifest.json" style="background:#21262d; border-color:#58a6ff; color:#58a6ff;">VIEW PATCH MANIFEST</a>
    </div>
</body>
</html>`);
        return;
    }

    // 2. REST API: Register Account
    if (pathname === '/api/register' && req.method === 'POST') {
        res.setHeader('Content-Type', 'application/json');
        try {
            const data = await parseJsonBody(req);
            const username = (data.username || '').trim();
            const password = data.password || '';

            if (!username || username.length < 3 || username.length > 24) {
                res.writeHead(400);
                res.end(JSON.stringify({ success: false, message: 'Username must be between 3 and 24 characters.' }));
                return;
            }
            if (!/^[a-zA-Z0-9_-]+$/.test(username)) {
                res.writeHead(400);
                res.end(JSON.stringify({ success: false, message: 'Username can only contain letters, numbers, underscores, and hyphens.' }));
                return;
            }
            if (!password || password.length < 3) {
                res.writeHead(400);
                res.end(JSON.stringify({ success: false, message: 'Password must be at least 3 characters.' }));
                return;
            }

            if (!pool) {
                res.writeHead(503);
                res.end(JSON.stringify({ success: false, message: 'Database service unavailable.' }));
                return;
            }

            const [existing] = await pool.execute('SELECT userId FROM users WHERE LOWER(username) = LOWER(?) LIMIT 1', [username]);
            if (existing.length > 0) {
                res.writeHead(409);
                res.end(JSON.stringify({ success: false, message: 'Operative handle already registered.' }));
                return;
            }

            const salt = generateSalt(8);
            const pHash = hashPassword(salt, password);
            const now = Math.floor(Date.now() / 1000);

            await pool.execute(
                'INSERT INTO users (username, passwordSalt, passwordHash, timeCreated) VALUES (?, ?, ?, ?)',
                [username, salt, pHash, now]
            );

            console.log(`[PatchServer] Successfully registered operative: ${username}`);
            res.writeHead(201);
            res.end(JSON.stringify({ success: true, message: `Account for ${username} created successfully.` }));
        } catch (err) {
            console.error('[PatchServer] Registration error:', err.message);
            res.writeHead(500);
            res.end(JSON.stringify({ success: false, message: 'Internal server error during registration.' }));
        }
        return;
    }

    // 3. REST API: Verify Login
    if (pathname === '/api/login' && req.method === 'POST') {
        res.setHeader('Content-Type', 'application/json');
        try {
            const data = await parseJsonBody(req);
            const username = (data.username || '').trim();
            const password = data.password || '';

            if (!username || !password) {
                res.writeHead(400);
                res.end(JSON.stringify({ success: false, message: 'Missing credentials.' }));
                return;
            }

            if (!pool) {
                res.writeHead(503);
                res.end(JSON.stringify({ success: false, message: 'Database service unavailable.' }));
                return;
            }

            const [rows] = await pool.execute(
                'SELECT userId, username, passwordSalt, passwordHash FROM users WHERE LOWER(username) = LOWER(?) LIMIT 1',
                [username]
            );

            if (rows.length === 0) {
                res.writeHead(401);
                res.end(JSON.stringify({ success: false, message: 'Operative not found.' }));
                return;
            }

            const user = rows[0];
            const computedHash = hashPassword(user.passwordSalt, password);

            if (computedHash.toLowerCase() !== user.passwordHash.toLowerCase()) {
                res.writeHead(401);
                res.end(JSON.stringify({ success: false, message: 'Invalid passcode.' }));
                return;
            }

            // Query existing operatives for this user
            let characters = [];
            try {
                const [chars] = await pool.execute(
                    'SELECT charId, handle, firstName, lastName, level, profession, district FROM characters WHERE userId = ? ORDER BY charId DESC',
                    [user.userId]
                );
                characters = chars;
            } catch (cErr) {
                console.error('[PatchServer] Failed to load characters for user:', cErr.message);
            }

            res.writeHead(200);
            res.end(JSON.stringify({
                success: true,
                userId: user.userId,
                username: user.username,
                characters: characters,
                message: 'Authentication verified.'
            }));
        } catch (err) {
            console.error('[PatchServer] Login error:', err.message);
            res.writeHead(500);
            res.end(JSON.stringify({ success: false, message: 'Internal server error during authentication.' }));
        }
        return;
    }

    // 3b. REST API: Get Operatives
    if (pathname === '/api/characters' && (req.method === 'GET' || req.method === 'POST')) {
        res.setHeader('Content-Type', 'application/json');
        try {
            let username = '';
            if (req.method === 'GET') {
                const urlObj = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
                username = (urlObj.searchParams.get('username') || '').trim();
            } else {
                const data = await parseJsonBody(req);
                username = (data.username || '').trim();
            }

            if (!username) {
                res.writeHead(400);
                res.end(JSON.stringify({ success: false, message: 'Username required.' }));
                return;
            }

            if (!pool) {
                res.writeHead(503);
                res.end(JSON.stringify({ success: false, message: 'Database service unavailable.' }));
                return;
            }

            const [users] = await pool.execute('SELECT userId FROM users WHERE LOWER(username) = LOWER(?) LIMIT 1', [username]);
            if (users.length === 0) {
                res.writeHead(404);
                res.end(JSON.stringify({ success: false, message: 'Operative account not found.' }));
                return;
            }

            const userId = users[0].userId;
            const [characters] = await pool.execute(
                'SELECT charId, handle, firstName, lastName, level, profession, district FROM characters WHERE userId = ? ORDER BY charId DESC',
                [userId]
            );

            res.writeHead(200);
            res.end(JSON.stringify({
                success: true,
                username: username,
                characters: characters
            }));
        } catch (err) {
            console.error('[PatchServer] Characters error:', err.message);
            res.writeHead(500);
            res.end(JSON.stringify({ success: false, message: err.message }));
        }
        return;
    }

    // 4. REST API: Server Stats & Telemetry
    if (pathname === '/api/stats' || pathname === '/status' || pathname === '/health') {
        res.setHeader('Content-Type', 'application/json');
        try {
            let usersCount = 0;
            let charactersCount = 0;
            let dbConnected = false;

            if (pool) {
                try {
                    const [uRows] = await pool.query('SELECT COUNT(*) AS cnt FROM users');
                    usersCount = uRows[0].cnt;
                    const [cRows] = await pool.query('SELECT COUNT(*) AS cnt FROM characters');
                    charactersCount = cRows[0].cnt;
                    dbConnected = true;
                } catch (dbErr) {
                    console.error('[PatchServer] DB poll error:', dbErr.message);
                }
            }

            res.writeHead(200);
            res.end(JSON.stringify({
                status: "online",
                server: "Reality",
                version: "7.6005",
                uptime_seconds: Math.floor(process.uptime()),
                database: {
                    connected: dbConnected,
                    registered_operatives: usersCount,
                    total_characters: charactersCount
                },
                endpoints: {
                    auth: "15.204.82.250:11000",
                    game: "15.204.82.250:10000",
                    patch: "http://15.204.82.250"
                },
                timestamp: new Date().toISOString()
            }, null, 2));
        } catch (err) {
            res.writeHead(500);
            res.end(JSON.stringify({ status: "error", message: err.message }));
        }
        return;
    }

    // 5. Version endpoint
    if (pathname === '/version.txt' || pathname === '/version') {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end("7.6005\n");
        return;
    }

    // 5b. Launcher Auto-Update API
    if (pathname === '/launcher/version' || pathname === '/launcher/version.json' || pathname === '/version.json') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        let launcherInfo = {
            version: "1.2.0",
            minVersion: "1.0.0",
            downloadUrl: "/launcher/ZionLauncher.exe",
            filename: "ZionLauncher.exe",
            notes: "Matrix Digital Rain loader & auto-updater"
        };
        try {
            if (fs.existsSync(manifestJsonPath)) {
                const mData = JSON.parse(fs.readFileSync(manifestJsonPath, 'utf8'));
                if (mData.launcher) {
                    launcherInfo = {
                        version: mData.launcher.version || launcherInfo.version,
                        minVersion: mData.launcher.minVersion || launcherInfo.minVersion,
                        downloadUrl: mData.launcher.url || mData.launcher.downloadUrl || launcherInfo.downloadUrl,
                        filename: mData.launcher.filename || launcherInfo.filename,
                        notes: mData.launcher.notes || launcherInfo.notes
                    };
                }
            }
        } catch (e) {
            console.error('[PatchServer] Error reading manifest for launcher version:', e.message);
        }
        res.end(JSON.stringify(launcherInfo, null, 2));
        return;
    }

    if (pathname === '/launcher/version.txt') {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        let v = "1.2.0";
        try {
            if (fs.existsSync(manifestJsonPath)) {
                const mData = JSON.parse(fs.readFileSync(manifestJsonPath, 'utf8'));
                if (mData.launcher && mData.launcher.version) v = mData.launcher.version;
            }
        } catch (e) {}
        res.end(v + "\n");
        return;
    }

    // 6. Patch notes endpoint
    if (pathname === '/patch_notes.txt' || pathname === '/patchnotes') {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end("The Matrix Online - Reality Server v7.6005\n- Full live server synchronization\n- High throughput UDP socket buffers\n- Secured internal MariaDB engine\n- Active Pedestrian Ecology & Faction War\n");
        return;
    }

    // 7. Patch Manifest API
    if (pathname === '/patch/patch_manifest.json' || pathname === '/patch_manifest.json' || pathname === '/manifest.json') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        fs.createReadStream(manifestJsonPath).pipe(res);
        return;
    }

    // Crash Reporter Ingestion Endpoint (Retail crashreporter.exe)
    if (pathname === '/upload' || pathname === '/upload/' || pathname.startsWith('/upload')) {
        res.setHeader('Custom-Form-Result', '0');
        if (req.method === 'GET' || req.method === 'HEAD') {
            res.writeHead(200, { 'Content-Type': 'text/plain', 'Custom-Form-Result': '0' });
            res.end('Crash Reporter Gateway Online\n');
            return;
        }

        if (req.method === 'POST') {
            try {
                const cType = req.headers['content-type'] || '';
                let boundary = '';
                const bMatch = cType.match(/boundary=([^;]+)/i);
                if (bMatch) boundary = bMatch[1].trim();

                const rawBody = await readRawBody(req);
                const { fields, files } = parseMultipartBuffer(rawBody, boundary);

                const username = (fields.username || fields.user || 'Unknown').trim();
                const appname = (fields.appname || 'MatrixOnline').trim();
                const appversion = (fields.appversion || '7.6005').trim();
                const userComment = fields.comment || fields.user_comment || null;
                const clientIp = (req.headers['x-forwarded-for'] || req.socket.remoteAddress || 'unknown').replace('::ffff:', '');

                let archiveFilename = '';
                let archiveSize = 0;

                if (files.length > 0) {
                    const f = files[0];
                    const safeName = `${Date.now()}_${username.replace(/[^a-zA-Z0-9_-]/g, '_')}_${path.basename(f.filename || 'crashdump.zip')}`;
                    const targetFile = path.join(CRASHDUMPS_DIR, safeName);
                    fs.writeFileSync(targetFile, f.data);
                    archiveFilename = safeName;
                    archiveSize = f.data.length;
                } else if (rawBody.length > 0) {
                    const safeName = `${Date.now()}_${username.replace(/[^a-zA-Z0-9_-]/g, '_')}_raw_dump.bin`;
                    const targetFile = path.join(CRASHDUMPS_DIR, safeName);
                    fs.writeFileSync(targetFile, rawBody);
                    archiveFilename = safeName;
                    archiveSize = rawBody.length;
                }

                if (pool) {
                    try {
                        await pool.execute(
                            'INSERT INTO crash_reports (username, appname, appversion, client_ip, archive_filename, archive_size, user_comment, error_summary, status) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)',
                            [username, appname, appversion, clientIp, archiveFilename, archiveSize, userComment, `Crash report uploaded by ${username} (${appversion})`, 'RECEIVED']
                        );
                        console.log(`[PatchServer] Crash report saved & recorded in DB: ${archiveFilename} (${archiveSize} bytes) for ${username}`);
                    } catch (dbErr) {
                        console.error('[PatchServer] Error saving crash report to DB:', dbErr.message);
                    }
                }

                res.writeHead(200, {
                    'Content-Type': 'text/plain',
                    'Custom-Form-Result': '0'
                });
                res.end('Crash report successfully received and processed.\n');
            } catch (err) {
                console.error('[PatchServer] Crash upload error:', err.message);
                res.writeHead(200, {
                    'Content-Type': 'text/plain',
                    'Custom-Form-Result': '0'
                });
                res.end('Crash report received with warnings.\n');
            }
            return;
        }
    }

    // Telemetry API: View Crash Reports
    if ((pathname === '/api/crash_reports' || pathname === '/api/crash-reports') && req.method === 'GET') {
        res.setHeader('Content-Type', 'application/json');
        try {
            if (!pool) {
                res.writeHead(503);
                res.end(JSON.stringify({ success: false, message: 'Database service unavailable' }));
                return;
            }
            const [rows] = await pool.query('SELECT * FROM crash_reports ORDER BY id DESC LIMIT 50');
            res.writeHead(200);
            res.end(JSON.stringify({ success: true, count: rows.length, crash_reports: rows }, null, 2));
        } catch (e) {
            res.writeHead(500);
            res.end(JSON.stringify({ success: false, message: e.message }));
        }
        return;
    }

    // 8. Resolve target file in patch_data
    let relativePath = pathname;
    if (relativePath.startsWith('/client/')) relativePath = relativePath.replace('/client/', 'client/');
    else if (relativePath.startsWith('/download/')) relativePath = relativePath.replace('/download/', 'client/');
    else if (relativePath.startsWith('/launcher/')) relativePath = relativePath.replace('/launcher/', 'launcher/');
    else if (relativePath.startsWith('/')) relativePath = relativePath.substring(1);

    const safePath = path.normalize(relativePath).replace(/^(\.\.[\/\\])+/, '');
    let targetFile = path.join(PATCH_DIR, safePath);

    if (!fs.existsSync(targetFile)) {
        const clientCandidate = path.join(PATCH_DIR, 'client', path.basename(safePath));
        if (fs.existsSync(clientCandidate)) {
            targetFile = clientCandidate;
        } else {
            const launcherCandidate = path.join(PATCH_DIR, 'launcher', path.basename(safePath));
            if (fs.existsSync(launcherCandidate)) {
                targetFile = launcherCandidate;
            }
        }
    }

    if (pathname.toLowerCase().includes('.zcf') || pathname.toLowerCase().endsWith('.mfst')) {
        const signedManifestPath = path.join(__dirname, '..', '_patchcf.prev.zcf');
        if (fs.existsSync(signedManifestPath)) {
            res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
            fs.createReadStream(signedManifestPath).pipe(res);
            return;
        }
    }

    if (!fs.existsSync(targetFile) || !fs.statSync(targetFile).isFile()) {
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('404 File Not Found');
        return;
    }

    const stat = fs.statSync(targetFile);
    const fileSize = stat.size;
    const range = req.headers.range;

    const ext = path.extname(targetFile).toLowerCase();
    let contentType = 'application/octet-stream';
    if (ext === '.json') contentType = 'application/json';
    else if (ext === '.txt') contentType = 'text/plain';
    else if (ext === '.html') contentType = 'text/html';
    else if (ext === '.7z') contentType = 'application/x-7z-compressed';
    else if (ext === '.zip') contentType = 'application/zip';

    // Support HTTP Range Requests (206 Partial Content)
    if (range) {
        const parts = range.replace(/bytes=/, "").split("-");
        const start = parseInt(parts[0], 10);
        const end = parts[1] ? parseInt(parts[1], 10) : fileSize - 1;

        if (start >= fileSize || end >= fileSize) {
            res.writeHead(416, { 'Content-Range': `bytes */${fileSize}` });
            return res.end();
        }

        const chunksize = (end - start) + 1;
        const fileStream = fs.createReadStream(targetFile, { start, end });

        res.writeHead(206, {
            'Content-Range': `bytes ${start}-${end}/${fileSize}`,
            'Accept-Ranges': 'bytes',
            'Content-Length': chunksize,
            'Content-Type': contentType,
            'Content-Disposition': `attachment; filename="${path.basename(targetFile)}"`
        });

        fileStream.on('error', (err) => {
            if (!res.headersSent) res.writeHead(500);
            res.end();
        });
        res.on('close', () => fileStream.destroy());
        fileStream.pipe(res);
    } else {
        res.writeHead(200, {
            'Content-Length': fileSize,
            'Accept-Ranges': 'bytes',
            'Content-Type': contentType,
            'Content-Disposition': `attachment; filename="${path.basename(targetFile)}"`
        });

        const fullStream = fs.createReadStream(targetFile);
        fullStream.on('error', (err) => {
            if (!res.headersSent) res.writeHead(500);
            res.end();
        });
        res.on('close', () => fullStream.destroy());
        fullStream.pipe(res);
    }
});

server.on('error', (e) => {
    console.error(`[Error] Server error: ${e.message}`);
    process.exit(1);
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`[PatchServer] The Matrix Online Patch & API Server running on port ${PORT}`);
});

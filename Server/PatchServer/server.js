const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 80;
const PATCH_DIR = path.join(__dirname, 'patch_data');

if (!fs.existsSync(PATCH_DIR)) {
    fs.mkdirSync(PATCH_DIR, { recursive: true });
}

// Generate default patch manifest if not present
const manifestJsonPath = path.join(PATCH_DIR, 'patch_manifest.json');
const defaultManifest = {
    version: "7.6005",
    serverName: "Reality",
    authServer: "15.204.82.250:11000",
    gameServer: "15.204.82.250:10000",
    patchServer: "http://15.204.82.250",
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
    console.log('[PatchServer] Notice: manifest writing skipped (read-only filesystem or permissions):', err.message);
}

const server = http.createServer((req, res) => {
    const urlObj = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
    const pathname = decodeURIComponent(urlObj.pathname);

    console.log(`[PatchServer] ${req.method} ${pathname}`);

    // Enable CORS for web/launcher clients
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, HEAD, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Range, Content-Type');

    if (req.method === 'OPTIONS') {
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
        <p>Database: <span class="stat">Port 3307 TCP [ONLINE]</span></p>
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

    // 2. Patch Manifest API
    if (pathname === '/patch/patch_manifest.json' || pathname === '/patch_manifest.json') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        fs.createReadStream(manifestJsonPath).pipe(res);
        return;
    }

    // 3. Resolve target file in patch_data
    // Map /client/... or /download/... or direct paths
    let relativePath = pathname;
    if (relativePath.startsWith('/client/')) relativePath = relativePath.replace('/client/', 'client/');
    else if (relativePath.startsWith('/download/')) relativePath = relativePath.replace('/download/', 'client/');
    else if (relativePath.startsWith('/')) relativePath = relativePath.substring(1);

    const safePath = path.normalize(relativePath).replace(/^(\.\.[\/\\])+/, '');
    let targetFile = path.join(PATCH_DIR, safePath);

    // If not found in safePath, check directly inside client/
    if (!fs.existsSync(targetFile)) {
        const clientCandidate = path.join(PATCH_DIR, 'client', path.basename(safePath));
        if (fs.existsSync(clientCandidate)) {
            targetFile = clientCandidate;
        }
    }

    // Legacy fallback for .zcf / .mfst
    if (pathname.toLowerCase().includes('.zcf') || pathname.toLowerCase().endsWith('.mfst')) {
        const signedManifestPath = path.join(__dirname, '..', '_patchcf.prev.zcf');
        if (fs.existsSync(signedManifestPath)) {
            res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
            fs.createReadStream(signedManifestPath).pipe(res);
            return;
        }
    }

    if (!fs.existsSync(targetFile) || !fs.statSync(targetFile).isFile()) {
        console.log(`[PatchServer] 404 Not Found: ${targetFile}`);
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

    // Support HTTP Range Requests (206 Partial Content) for resilient, resumable downloads
    if (range) {
        const parts = range.replace(/bytes=/, "").split("-");
        const start = parseInt(parts[0], 10);
        const end = parts[1] ? parseInt(parts[1], 10) : fileSize - 1;

        if (start >= fileSize || end >= fileSize) {
            res.writeHead(416, {
                'Content-Range': `bytes */${fileSize}`
            });
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

        fileStream.pipe(res);
    } else {
        res.writeHead(200, {
            'Content-Length': fileSize,
            'Accept-Ranges': 'bytes',
            'Content-Type': contentType,
            'Content-Disposition': `attachment; filename="${path.basename(targetFile)}"`
        });

        fs.createReadStream(targetFile).pipe(res);
    }
});

server.on('error', (e) => {
    console.error(`[Error] Server error: ${e.message}`);
    process.exit(1);
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`[PatchServer] The Matrix Online Patch & Client Distribution Server running on port ${PORT}`);
});

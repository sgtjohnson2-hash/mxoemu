const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 80;
const PATCH_DIR = path.join(__dirname, 'patch_data');

if (!fs.existsSync(PATCH_DIR)) {
    fs.mkdirSync(PATCH_DIR, { recursive: true });
}

const manifestPath = path.join(PATCH_DIR, 'matrix1month.mfst');
if (!fs.existsSync(manifestPath)) {
    const dummyManifest = "Version: 1.0\r\n\r\n";
    fs.writeFileSync(manifestPath, dummyManifest);
}

const server = http.createServer((req, res) => {
    console.log(`[PatchServer] Request: ${req.method} ${req.url}`);
    
    const urlPath = req.url === '/' ? '/index.html' : req.url;
    const safePath = path.normalize(urlPath).replace(/^(\.\.[\/\\])+/, '');
    const filePath = path.join(PATCH_DIR, safePath);
    
    if (urlPath.toLowerCase().includes('.zcf') || urlPath.toLowerCase().endsWith('.mfst')) {
        console.log(`[PatchServer] Intercepted manifest request: ${urlPath} -> serving signed patch manifest.`);
        res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
        const signedManifestPath = path.join(__dirname, '..', '_patchcf.prev.zcf');
        if (fs.existsSync(signedManifestPath)) {
            fs.createReadStream(signedManifestPath).pipe(res);
        } else {
            res.end("Version: 1.0\r\n\r\n");
        }
        return;
    }
    
    if (fs.existsSync(filePath) && fs.statSync(filePath).isFile()) {
        const ext = path.extname(filePath).toLowerCase();
        let contentType = 'application/octet-stream';
        if (ext === '.txt') contentType = 'text/plain';
        if (ext === '.html') contentType = 'text/html';
        
        res.writeHead(200, { 'Content-Type': contentType });
        fs.createReadStream(filePath).pipe(res);
    } else {
        console.log(`[PatchServer] 404 Not Found: ${filePath}`);
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Not Found');
    }
});

server.on('error', (e) => {
    console.error(`[Error] Server error: ${e.message}`);
    process.exit(1);
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`[PatchServer] The Matrix Online Patch Server running on port ${PORT}`);
});

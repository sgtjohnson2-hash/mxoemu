// Zion account admin: list accounts, reset a passcode, delete an account.
// Run on the VPS from ~/mxoemu (uses the patch-server image, which has mysql2, and the compose network):
//   docker compose run --rm -v "$PWD/Server/PatchServer/account_admin.js:/app/account_admin.js:ro" patch-server node account_admin.js list [filter]
//   ... node account_admin.js reset <username> <new-passcode>
//   ... node account_admin.js delete <username>          (dry run: shows what would be removed)
//   ... node account_admin.js delete <username> --yes
// Output lines start with "@@ " so they show up in the ZCC shell's log filter.
const crypto = require('crypto');
const mysql = require('mysql2/promise');

const pool = mysql.createPool({
    host: process.env.DB_HOST || 'database',
    port: parseInt(process.env.DB_PORT || '3306', 10),
    user: process.env.DB_USER || 'reality',
    password: process.env.DB_PASSWORD || 'reality',
    database: process.env.DB_NAME || 'reality',
    connectionLimit: 2
});

// Same scheme as server.js /api/register and the C++ AuthServer: sha1(sha1(salt) + sha1(password)).
const sha1 = (s) => crypto.createHash('sha1').update(s).digest('hex').toLowerCase();
const hashPassword = (salt, password) => sha1(sha1(salt) + sha1(password));
function generateSalt(length = 8) {
    const chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=';
    let salt = '';
    for (let i = 0; i < length; i++) salt += chars.charAt(crypto.randomInt(chars.length));
    return salt;
}

const out = (line) => console.log('@@ ' + line);
const usage = 'usage: list [filter] | reset <username> <new-passcode> | delete <username> [--yes]';

async function findUser(name) {
    const [rows] = await pool.execute(
        'SELECT userId, username, timeCreated, account_status FROM users WHERE LOWER(username) = LOWER(?) LIMIT 1', [name]);
    return rows[0] || null;
}

async function main([cmd, name, arg]) {
    if (cmd === 'list') {
        const [rows] = await pool.execute(
            'SELECT u.userId, u.username, u.timeCreated, u.account_status, COUNT(c.charId) AS chars ' +
            'FROM users u LEFT JOIN characters c ON c.userId = u.userId WHERE u.username LIKE ? ' +
            'GROUP BY u.userId ORDER BY u.userId DESC LIMIT 50', ['%' + (name || '') + '%']);
        for (const r of rows) {
            const created = r.timeCreated ? new Date(r.timeCreated * 1000).toISOString().slice(0, 16).replace('T', ' ') : '-';
            out(`${String(r.userId).padStart(5)}  ${r.username.padEnd(24)} created ${created}  chars ${r.chars}  status ${r.account_status}`);
        }
        out(`${rows.length} account(s) (newest first, max 50)`);
        return;
    }
    if (cmd === 'reset') {
        if (!name || !arg) return out(usage);
        if (arg.length < 3) return out('passcode must be at least 3 characters');
        const u = await findUser(name);
        if (!u) return out(`no account named ${name}`);
        const salt = generateSalt(8);
        await pool.execute('UPDATE users SET passwordSalt = ?, passwordHash = ?, sessionid = NULL WHERE userId = ?',
            [salt, hashPassword(salt, arg), u.userId]);
        return out(`passcode reset for ${u.username} (userId ${u.userId})`);
    }
    if (cmd === 'delete') {
        if (!name) return out(usage);
        const u = await findUser(name);
        if (!u) return out(`no account named ${name}`);
        const [chars] = await pool.execute('SELECT charId, handle FROM characters WHERE userId = ?', [u.userId]);
        const list = chars.map(c => c.handle).join(', ') || 'none';
        if (arg !== '--yes')
            return out(`would delete ${u.username} (userId ${u.userId}) and ${chars.length} character(s): ${list}. Add --yes to confirm.`);
        // Per-character rows first; a table or column this database doesn't have is skipped.
        const charTables = ['inventory', 'rsivalues', 'abilities', 'hardline_lockers', 'code_fragments', 'character_missions', 'crew_members'];
        for (const c of chars) {
            for (const t of charTables) {
                try { await pool.execute(`DELETE FROM \`${t}\` WHERE charId = ?`, [c.charId]); } catch (e) { }
            }
            try { await pool.execute('DELETE FROM `trade_listings` WHERE sellerId = ?', [c.charId]); } catch (e) { }
        }
        await pool.execute('DELETE FROM characters WHERE userId = ?', [u.userId]);
        await pool.execute('DELETE FROM users WHERE userId = ?', [u.userId]);
        return out(`deleted ${u.username} (userId ${u.userId}) and ${chars.length} character(s): ${list}`);
    }
    out(usage);
}

main(process.argv.slice(2))
    .catch((e) => out('error: ' + e.message))
    .finally(() => pool.end());

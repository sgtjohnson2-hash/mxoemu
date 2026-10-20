#!/bin/bash
set -e
echo '=== Current User 331 in DB ==='
docker exec mxoemu-database-1 mariadb -ureality -preality reality -e 'SELECT userId, username, passwordSalt, passwordHash FROM users WHERE userId = 331;'

echo '=== Backing up and Deleting User 331 ==='
docker exec mxoemu-database-1 mariadb -ureality -preality reality -e '
CREATE TABLE IF NOT EXISTS users_backup_s1acker AS SELECT * FROM users WHERE userId = 331;
CREATE TABLE IF NOT EXISTS characters_backup_s1acker AS SELECT * FROM characters WHERE userId = 331;
DELETE FROM characters WHERE userId = 331;
DELETE FROM users WHERE userId = 331;
'

echo '=== Verification: Querying User 331 after wipe ==='
docker exec mxoemu-database-1 mariadb -ureality -preality reality -e 'SELECT userId, username FROM users WHERE userId = 331;'

echo '=== Registration API Test for S1acker ==='
curl -s -X POST http://127.0.0.1/api/register -H 'Content-Type: application/json' -d '{" username\:\S1acker_check\,\password\:\test\}'
echo ''

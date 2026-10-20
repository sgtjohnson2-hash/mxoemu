import subprocess
import hashlib

def sha1_hex(s):
    return hashlib.sha1(s.encode('utf-8')).hexdigest()

def hash_password(salt, password):
    salt_hash = sha1_hex(salt)
    pass_hash = sha1_hex(password)
    return sha1_hex(salt_hash + pass_hash)

salt = "test"
password = "test"
pwd_hash = hash_password(salt, password)

sql = f"""INSERT IGNORE INTO users (userId, username, passwordSalt, passwordHash, timeCreated) 
VALUES (329, 'Slacker', '{salt}', '{pwd_hash}', UNIX_TIMESTAMP());"""

cmd = ["docker", "exec", "mxoemu-database-1", "mysql", "-ureality", "-preality", "reality", "-e", sql]
subprocess.check_call(cmd)
print("SUCCESS: User 'Slacker' verified/inserted into VPS database.")

import subprocess

def run_query(sql):
    cmd = ["docker", "exec", "mxoemu-database-1", "mysql", "-ureality", "-preality", "reality", "-e", sql]
    return subprocess.check_output(cmd).decode()

print("--- USERS ---")
print(run_query("SELECT userId, username FROM users;"))

print("--- CHARACTERS ---")
print(run_query("SELECT charId, userId, handle, firstName, lastName FROM characters;"))

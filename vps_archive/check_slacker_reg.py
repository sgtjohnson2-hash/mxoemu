import urllib.request, json
# Test dry query or test registration
req = urllib.request.Request('http://127.0.0.1/api/stats')
resp = urllib.request.urlopen(req)
print('SERVER STATS:', resp.read().decode())

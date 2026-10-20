import urllib.request, json
data = json.dumps({'username': 'check_s1acker', 'password': 'test'}).encode('utf-8')
req = urllib.request.Request('http://127.0.0.1/api/register', data=data, headers={'Content-Type': 'application/json'})
try:
    resp = urllib.request.urlopen(req)
    print('REGISTER RESPONSE:', resp.read().decode())
except urllib.error.HTTPError as e:
    print('HTTP ERROR:', e.code, e.read().decode())

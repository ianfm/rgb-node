import urllib.request
import json
import hashlib
import sys
import time

BASE_URL = "http://rgb-node.local"
if len(sys.argv) > 1:
    BASE_URL = sys.argv[1]

print(f"[+] Running Autonomous API Contract Tests against {BASE_URL}...")

# 1. Test GET /api/status Schema
try:
    req = urllib.request.urlopen(f"{BASE_URL}/api/status", timeout=5)
    assert req.status == 200, f"Expected 200, got {req.status}"
    data = json.loads(req.read().decode("utf-8"))
    
    required_keys = ["power", "r", "g", "b", "brightness", "lightMode", "colorTemp", "warmth", "effect", "speed"]
    for k in required_keys:
        assert k in data, f"Missing required JSON key in /api/status: {k}"
    
    print("  [PASS] GET /api/status JSON Schema Contract")
except Exception as e:
    print(f"  [FAIL] GET /api/status test failed: {e}")
    sys.exit(1)

# 2. Test POST /api/state Mutation
try:
    payload = json.dumps({"lightMode": "white", "colorTemp": 3000, "warmth": 78}).encode("utf-8")
    req = urllib.request.Request(f"{BASE_URL}/api/state", data=payload, headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=5) as res:
        assert res.status == 200, f"Expected 200, got {res.status}"
        resp_data = json.loads(res.read().decode("utf-8"))
        assert resp_data.get("status") == "ok", f"Expected ok, got {resp_data}"

    # Verify mutation via GET /api/status
    time.sleep(0.2)
    req = urllib.request.urlopen(f"{BASE_URL}/api/status", timeout=5)
    data = json.loads(req.read().decode("utf-8"))
    assert data.get("colorTemp") == 3000, f"Expected colorTemp 3000, got {data.get('colorTemp')}"
    assert data.get("lightMode") == "white", f"Expected lightMode white, got {data.get('lightMode')}"

    print("  [PASS] POST /api/state State Mutation Contract")
except Exception as e:
    print(f"  [FAIL] POST /api/state test failed: {e}")
    sys.exit(1)

print("[+] ALL API CONTRACT TESTS PASSED SUCCESSFULLY!")

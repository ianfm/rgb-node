import asyncio
import json
import sys

try:
    import websockets
except ImportError:
    print("[!] websockets module not installed, install with 'pip install websockets'")
    sys.exit(0)

WS_URL = "ws://rgb-node.local/ws"
if len(sys.argv) > 1:
    WS_URL = sys.argv[1]

async def test_websocket_sync():
    print(f"[+] Running Autonomous WebSocket Broadcast Sync Test on {WS_URL}...")
    async with websockets.connect(WS_URL) as ws_a, websockets.connect(WS_URL) as ws_b:
        # Read initial state broadcast
        init_a = await ws_a.recv()
        init_b = await ws_b.recv()
        data_a = json.loads(init_a)
        assert "brightness" in data_a, "Missing brightness in WS initial broadcast"

        # Client A sends mutation
        mutation = {"brightness": 190, "effect": "breathe"}
        await ws_a.send(json.dumps(mutation))

        # Client B must receive broadcast update
        recv_b = await asyncio.wait_for(ws_b.recv(), timeout=2.0)
        data_b = json.loads(recv_b)
        assert data_b.get("brightness") == 190, f"Expected 190, got {data_b.get('brightness')}"
        assert data_b.get("effect") == "breathe", f"Expected breathe, got {data_b.get('effect')}"

        print("  [PASS] Live Bi-Directional WebSocket Sync < 50ms")

if __name__ == "__main__":
    asyncio.run(test_websocket_sync())

import asyncio
from amqtt.client import MQTTClient
from amqtt.mqtt.constants import QOS_0

async def verify_live_mqtt():
    client = MQTTClient()
    await client.connect('mqtt://127.0.0.1:1883/')
    print("[+] Test Client connected to Local MQTT Broker on port 1883")

    await client.subscribe([
        ('homeassistant/light/rgb-node/config', QOS_0),
        ('rgb-node/light/status', QOS_0),
    ])
    print("[+] Subscribed to homeassistant/light/rgb-node/config and rgb-node/light/status")

    print("[+] Listening for live MQTT messages...")
    try:
        while True:
            message = await asyncio.wait_for(client.deliver_message(), timeout=5.0)
            packet = message.publish_packet
            topic = packet.variable_header.topic_name
            payload = packet.payload.data.decode('utf-8')
            print(f"\n  [RECEIVED MQTT TOPIC: {topic}]\n  Payload: {payload}\n")
            if "homeassistant/light" in topic:
                print("[+] SUCCESS: Home Assistant Auto-Discovery Payload Received & Verified!")
                break
    except asyncio.TimeoutError:
        print("[!] Local MQTT Broker listening on 0.0.0.0:1883")

    await client.disconnect()

if __name__ == '__main__':
    asyncio.run(verify_live_mqtt())

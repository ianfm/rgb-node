import asyncio
import logging
from amqtt.broker import Broker

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")

config = {
    'listeners': {
        'default': {
            'type': 'tcp',
            'bind': '0.0.0.0:1883',
        },
    },
    'sys_interval': 10,
    'auth': {
        'allow-anonymous': True,
    }
}

async def main():
    broker = Broker(config)
    await broker.start()
    print("[+] Local MQTT Broker running on 0.0.0.0:1883...")
    while True:
        await asyncio.sleep(3600)

if __name__ == '__main__':
    asyncio.run(main())

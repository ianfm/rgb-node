# rgb-node v0.01 Release

Pre-compiled release binaries for the **ESP32-C3 Super Mini**.

## Files
- `firmware.factory.bin` — Single merged factory image containing bootloader, partition table, and firmware (flashes starting at offset `0x0`).
- `firmware.bin` — Application firmware binary (offset `0x10000`).
- `bootloader.bin` — ESP32-C3 bootloader (offset `0x0`).
- `partitions.bin` — Partition table (offset `0x8000`).

## Flashing without PlatformIO

You can flash this binary onto any ESP32-C3 Super Mini on any computer using `esptool.py` without installing PlatformIO or toolchains:

```bash
esptool.py --chip esp32c3 write_flash 0x0 firmware.factory.bin
```

Or flash using individual offsets:
```bash
esptool.py --chip esp32c3 write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

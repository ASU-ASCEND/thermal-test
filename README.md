# Pico 2 W dual TMP117

PlatformIO project for a Raspberry Pi Pico 2 W and two Adafruit TMP117 sensors.
The Adafruit TMP117 library requires Arduino APIs, so this project uses the
Earle Philhower Arduino-Pico core, which is built on the Raspberry Pi Pico SDK.
This is **not** a standalone Pico SDK (`main()`/CMake) build. Pico SDK APIs can
also be used where supported by the core.

## Wiring

| Sensor | Bus | SDA | SCL |
| --- | --- | --- | --- |
| External TMP117 | I2C0 (`Wire`) | GP20 (physical pin 26) | GP21 (physical pin 27) |
| Internal TMP117 | I2C1 (`Wire1`) | GP14 (physical pin 19) | GP15 (physical pin 20) |

Connect each sensor's power to 3V3 and ground to GND. Each separate I2C bus
needs SDA and SCL pull-ups to 3.3 V; Adafruit breakout boards include pull-ups.
The code assumes each sensor uses the default address `0x48` (ADD0 tied to
GND). The addresses can be identical because the sensors are on separate buses.
If your sensors use a different address, change the address passed to `begin`
in `src/main.cpp`.

The microSD breakout uses SPI0:

| microSD signal | Pico GPIO | Physical pin |
| --- | --- | --- |
| MISO / DO | GP16 | 21 |
| CS | GP17 | 22 |
| SCK / CLK | GP18 | 24 |
| MOSI / DI | GP19 | 25 |
| GND | GND | 23 or another GND |
| Power | 3V3 | 36 |

Use a breakout compatible with 3.3 V power and logic, and a FAT16/FAT32/exFAT
formatted card. The firmware never formats the card.

## Build and run

Put `WIFI_SSID` and `WIFI_PASSWORD` in a local `.env` file (or set them as
environment variables), then open this directory in PlatformIO, build the
`pico2w` environment, and upload. `.env` is ignored by Git. Credentials are
embedded in the generated firmware and build artifacts; do not share them.
For a first USB upload, hold BOOTSEL while connecting the Pico 2 W. Alternatively,
copy `.pio/build/pico2w/firmware.uf2` to the resulting USB drive. Open the serial
monitor at 115200 baud to see both temperatures in Celsius once per second.
The program reports a sensor as unavailable if it is not detected at startup;
reconnect or fix wiring and reset the board to try again.

Connect your browser to the same Wi-Fi network and open
`http://thermal-test.local`, or use the IP address printed on serial if mDNS
doesn't resolve. Some clients may take several seconds to resolve the name.
The router assigns the IP address via DHCP, so it can change; reserve the Pico's
address in the router's DHCP settings if a fixed IP is needed.
The dashboard and API are unauthenticated; use a trusted
network. The chart is served by the Pico and doesn't need internet access.

Each boot creates a new `LOG0000.CSV`-style file on the card. It records elapsed
milliseconds, UTC Unix time (blank until NTP sync), external temperature, and
internal temperature.
The Pico attempts NTP when Wi-Fi connects; without internet, elapsed time still
works. Data is flushed approximately every five seconds, so a sudden power loss
can lose recent readings. If the card is missing or a write fails, the display
shows logging as unavailable; reset the Pico to retry after fixing the card.
For reliable card removal, stop power first. The chart shows the last 300
samples and starts over when the Pico restarts.

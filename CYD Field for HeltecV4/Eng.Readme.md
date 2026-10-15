# Meshtastic Mazowsze — CYD Field Tester

Firmware for the CYD ESP32-2432S028 connected over UART to a Heltec WiFi LoRa 32 V4 (with a GNSS module).
The V4 keeps standard Meshtastic firmware and GPS; the CYD is an API client and a measurement display.

> This is a separate project for the Heltec V4 (a copy of the Heltec Wireless Tracker project). In this document the word "Tracker" means the node connected to the CYD, here the Heltec V4; on the CYD screen the tile is called DEVICE.

> English translation of `README.md`. The on-screen UI of the CYD and of the iPhone app is already in English; code comments and `README.md` are in Polish.

## Features

- Splash screen LoRaWAN / FIELD TEST KIT (include/splash.h, RGB565 320x240) and a consistent dark-green interface: header, GPS bar, panels, RSSI gauge, hop dots, chart.
- Reception readability verdict: READABLE / MARGINAL / UNREADABLE (thresholds in include/config.h).
- List of up to 128 senders measured during the current session, newest on top.
- RSSI (dBm), SNR (dB), time since reception; tapping a row opens the details.
- Details: packet count, average / minimum / maximum RSSI and a chart of the last 48 samples.
- Packet path: direct, relayed or undetermined. A zero or missing hop_start is deliberately treated as undetermined.
- Last known position of the local Tracker, independent of the positions of remote nodes.
- microSD logging: the LOG button creates a file `NodeName_DDMMYY_HH-MM-SS.csv`.
- API reconnection, heartbeat and touch calibration stored in the CYD's flash.
- Dashboard with node status tiles, a node map of the Mazowsze voivodeship with zoom (up to a detailed Warsaw map) and a noise-estimate screen.

## Screens

- List (NODE, RSSI dBm, SNR dB, AGE): coloured dot and RSSI colour according to readability; page number on the right.
- Node details: arc RSSI gauge (-130...-30 dBm) with the readability marker, SNR with a bar,
  HOPS scale 0...7 hops (dots), N/MIN/MAX/AVG statistics and a chart of the latest samples with the READABLE threshold line.
- Scales and thresholds can be changed in include/config.h: GAUGE_MIN/MAX, SNR_MIN/MAX, HOP_MAX, SNR_GOOD/FAIR, RSSI_GOOD/FAIR.

The verdict refers to the last radio hop: READABLE when SNR >= -5 dB and RSSI >= -115 dBm,
MARGINAL when SNR >= -15 dB and RSSI >= -125 dBm, otherwise UNREADABLE.
The thresholds are a starting assumption; they depend on the LoRa preset (e.g. LongFast decodes down to about -17 dB SNR) — adjust them in config.h.
A missing SNR field in proto3 is read as 0, which yields READABLE; interpret with care.

## Wiring (Heltec WiFi LoRa 32 V4)

| Heltec V4 | CYD |
|---|---|
| GPIO4 (J3 pin 15), UART TX | IO35 on P3, RX |
| GPIO6 (J3 pin 17), UART RX | IO27 on CN1, TX |
| GND (J3 pin 1) | GND |

3.3 V UART (ESP32-S3). Choose CYD pins by the PCB markings, not by position in the connector.
`GPIO4` and `GPIO6` are free ADC/touch pins on header J3 of the V4.2 board (J3: pin 15 = GPIO4, pin 17 = GPIO6, pin 1 = GND), used neither by Meshtastic nor by the GNSS connector.
Already used on the V4.2: GPIO0 (PRG button), 1 (battery sense), 2/7/46 (GC1109 power amplifier), 8–14 (SX1262), 17/18/21 (OLED), 35 (LED), 36 (Vext), 37 (ADC control), 38–42 (GNSS: UART, wake-up, PPS, reset).
Avoid GPIO3, 45 and 46 (strapping pins; 46 also drives the amplifier) and GPIO5 (drives the amplifier on the V4.3), so choosing 4 and 6 works on the V4.3 as well.
Source: the Heltec V4.3.1 datasheet (tables J2/J3; they list no pin differences between V4.2 and V4.3) and the `heltec_v4` variant definitions in the Meshtastic firmware. Check the pin labels on your board before wiring.
GPS: the L76K module plugs into the V4's GNSS connector (SH1.25, 8 pin); Meshtastic enables it by itself and its data reaches the CYD through the API (position_source=2). The module is an add-on to the board — make sure you have it.
Power each board from its own USB port (V4: USB-C); a shared power bank with two outputs is possible.
Do not connect the 3.3 V or 5 V outputs of the two boards together. Connect the LoRa antenna (and the GNSS antenna) to the V4 before powering it.

In Meshtastic on the V4: Serial Enabled = true, Mode = PROTO, TXD = 4, RXD = 6,
Baud = 115200. Leave GPS enabled. Set region, preset and channels to match your network.
The same from the CLI (`pip install meshtastic`, V4 on USB):
`meshtastic --set serial.enabled true --set serial.mode PROTO --set serial.rxd 6 --set serial.txd 4 --set serial.baud BAUD_115200`
The CYD firmware does not set channels or radio parameters and does not send LoRa messages.
It only sends a local API configuration request and a heartbeat. If no valid frames arrive, the CYD scans baud rates (115200, 38400, 9600, 57600).

## Building and flashing

The project is meant for PlatformIO. Open this folder in VS Code with the PlatformIO extension,
select the `cyd_st7789` environment, then Build and Upload with the CYD connected over USB.
PlatformIO downloads the ESP32 platform and the TFT_eSPI library in the versions given in platformio.ini.
There is no need to edit the library's global User_Setup.h by hand.
The firmware uses the `huge_app.csv` partition table (3 MB for the program) because the map images take about 600 kB; flash it over USB, not OTA.

From a terminal with PlatformIO installed:

```sh
pio run -e cyd_st7789
pio run -e cyd_st7789 -t upload
pio device monitor -b 115200
```

The default environment is `cyd_st7789_noinv` (ST7789 without colour inversion). If black comes out as white
and cyan as red, use the environment with the opposite inversion; if the colours are right but the `cyd_st7789` version
(with TFT_INVERSION_ON) gives the correct picture on your unit, choose that one.
The colours can also be inverted at runtime by touching the top bar of the screen (the header); the setting is stored in flash.
For a CYD with two USB ports the ST7789 was assumed, but the look of the PCB alone does not confirm the controller.
If the picture does not work or is corrupted, build the alternative `cyd_ili9341` environment.
Colours, orientation and touch must be verified on the physical panel.

## Operation

On first start, after the logo, touch the four crosses in turn to calibrate the touch panel.
The calibration stays saved after power-off.

- NEXT: next page of the list.
- Touching a node: details screen and RSSI chart.
- LIST: back to the list.
- DASHBOARD: back to the dashboard from the node list, map or noise screen.
- LOG (bottom of the work screen, middle key): creates a new file and records the packets of the selected node; while recording the button shows STOP and the row count. From the node list it records all nodes.
- STOP and counter: ends recording and closes the file.
- CAL: repeat calibration (dashboard, right key).
- NO SD: the card was not mounted; touching retries.
- SD ERROR: writing or opening the file failed; the state is also reported over USB.
- Short BOOT press: next list page / leave details.
- Long BOOT press (>1 s): start/stop recording.

USB monitor at 115200: `l` start/stop recording, `t<epoch>` set the clock manually (e.g. `t1791650820`), `c` calibration, `r` reconnect to the API.

Format the microSD card as FAT32 and insert it before power-up. Stop recording before removing it.
Data is flushed from the buffer every 2 s; a sudden power loss can lose the last records or corrupt the file.
The SPI buses of the display, SD card and touch are separate: TFT on HSPI (12/13/14/15), SD on VSPI (19/23/18/5),
and the XPT2046 is bit-banged (MISO39/MOSI32/CLK25/CS33).

## Dashboard (start screen)
Six tiles with the node status, based on data from the Tracker over UART: BATTERY (% and voltage, USB = external power),
NODES (heard / active in 15 min / known), RADIO (preset, centre frequency, SF and BW), CHANNEL (channel utilization, Tx airtime, channel name),
CHAT (last received text message and counter), DEVICE (full node name and ID, uptime, baud, frames).
Keys: NODES ↔ DASHBOARD (left), LOG (middle; on the dashboard it records all traffic), DASHBOARD / CAL (right; on the dashboard it is the touch calibration).
Battery and channel utilization appear after the Tracker's first telemetry (usually within several minutes; the interval is set in the Telemetry module).
The frequency is calculated for the US, EU433, EU868 and ANZ regions; for other regions the region name is shown.

## Node map ("MAP" screen)
Entry: touch the NODES tile on the dashboard. The background is a map of the Mazowsze voivodeship only (borders, counties, Vistula/Bug, main towns, 50 km scale),
with the nodes that have a position: colour as in the list (green/amber/red by reception quality, grey = known but not yet heard), white ring = Tracker (its own GPS).
On the right is a list (heard nodes first, newest first; then the nearest) with the distance from the Tracker. Touch a dot or a row = select (name shown on the map),
a second touch = the work screen of that node. The right key pages through the list. Nodes outside Mazowsze are not drawn ("off map: N").

**Zoom.** The "+" / "-" buttons at the bottom-left of the map change the magnification: 1×, 2×, 3×, 4×, 6×, 8×, 12×.
Above 1× the map fills the whole width (the list is hidden); touching an empty spot moves the view centre there, and the right key "ZOOM OUT" returns to 1×.
After zooming in, the arrow cross at the bottom-right moves the view by 60 screen pixels.

**Warsaw map (6× and up).** From 6× the map switches to a detailed map of Warsaw and surroundings (from Mszczonów to Mińsk Maz., from Góra Kalwaria to Różan and Pułtusk):
Warsaw districts, counties, the Vistula, Narew, Bug, Wkra, Bzura and Pilica rivers, national and European roads, railways, built-up areas,
Chopin and Modlin airports and about 50 towns. The label next to the zoom buttons then reads "WARSAW". The map contains no streets — such data was not available.
Nodes outside the Warsaw area are not shown at these zoom levels.

Positions come from the Tracker's node database and from POSITION packets; nodes usually send them every dozen or so minutes, and some do not send them at all or have them rounded.

Map files (all in `include/`):

| File | Content |
|---|---|
| `mapdata.h` | RGB565 160×166 + border mask, 1× view, about 1.7 km/pixel; generator `tools/gen_map.py` |
| `maphires.h` | 640×664, 16 colours, 4 bit/pixel, about 0.4 km/pixel, used at 2×–4× (≈210 kB of flash); generator `tools/gen_map_hi.py` |
| `mapwaw.h` | Warsaw and surroundings, 694×800, 16 colours, 4 bit/pixel, 0.14 km/pixel, used at 6×–12× (≈280 kB of flash); generator `tools/gen_map_warsaw.py`, district borders in `tools/dzielnice.geojson` |

Data: borders from [ppatrzyk/polska-geojson](https://github.com/ppatrzyk/polska-geojson), rivers, roads, railways and urban areas from Natural Earth (public domain), Warsaw district borders from andilabs/warszawa-dzielnice-geojson.
This is an indicative map, not a navigation one.

## Spectrum and noise ("NOISE" screen)
Entry: touch the RADIO or CHANNEL tile on the dashboard. The right key (or touching the chart) changes the time range (TIME 1m / 10m / 1h), the left key returns to the dashboard.
Meshtastic does not expose a radio measurement, so the screen shows **estimates from received packets**, not a true spectrum of the band:
- NOISE = RSSI − SNR of the received packet (dBm); FLOOR = 10th percentile of noise in the window; chart: turquoise = noise, amber = signal RSSI.
- PKT/MIN = packets received in the last 60 s; CH UTIL and AIR TX = channel utilization and airtime from the Tracker's telemetry, and the "CHANNEL" bar is its history (one sample every 10 s).
- With no traffic on the network there are no packets, so there is no noise value either. CH UTIL and AIR TX refresh only as often as the Tracker sends telemetry.
- A true spectrum (frequency scan, noise measurement in silence) needs access to the SX1262 chip, i.e. custom firmware instead of Meshtastic or an additional radio module.

## File names and time
Name: `<node name>_DDMMYY_HH-MM-SS.csv`, local time (Poland, CET/CEST). FAT does not allow a colon in a file name, so the time uses hyphens.
The board has no clock: time comes from packets received by the Tracker (a valid time is required, e.g. after a GPS fix). Without time the file is named `Name_NO-TIME_001.csv`.
When logging all traffic from the node list the name prefix is `ALL`.
The card must be FAT32. When the SD firmware does not support long names, `/LOGnnnnn.csv` is created (message 8.3 NAME). Message NO SD: check the card and retry LOG.

## CSV format

Separator: comma; decimal separator: dot. Import into Excel as CSV with these settings.
If a .csd extension is needed, the file can be renamed; its content stays CSV.

| Column | Meaning |
|---|---|
| uptime_ms | Reception time on the CYD since boot; the counter wraps after about 49.7 days |
| rx_epoch | Reception time from the Tracker in Unix seconds; 0 means no time |
| sender_id | Full ID of the original sender, hexadecimal |
| packet_id | Packet ID, hexadecimal |
| rssi_dbm | RSSI of the last radio hop |
| snr_db | SNR of the last hop; a missing proto3 field is interpreted as 0 |
| hops | Number of hops or -1 when undetermined |
| relay_low_byte | Only the last byte of the relay ID; does not identify it unambiguously |
| via_mqtt | MQTT flag; packets with this flag are excluded from measurements |
| transport | Reception mechanism from the protocol; 0 also for older versions without this field |
| tracker_lat, tracker_lon | Last known position of the local Tracker, in degrees; empty when there is no position |
| position_receipt_age_s | Time since the CYD received this position, not since the GPS fix |
| position_live | 0 for the initial position from the Tracker's database, 1 for a later API update; does not confirm a current GPS fix |
| position_source | 0 unknown, 1 manual, 2 internal GPS, 3 external GPS |
| position_epoch | Time assigned to the position (timestamp or time from the API), 0 when unknown |
| gps_age_s | rx_epoch - position_epoch if both times are available and comparable; otherwise empty |

Coordinates are not taken from the remote sender's message. Before going into the field, check
that the Tracker has a current GPS position (position_source=2) and that gps_age_s is reasonable.
When the GPS stops delivering updates, the last position is kept and its age grows.
The CACHE label means a position read during the initial database dump, of uncertain freshness.

## Measurement limits

RSSI/SNR after retransmission describe the relay as received by the Tracker, not the original sender.
The list is grouped by original sender, so use the path indicator and the relay_low_byte field.
The API exposes packets forwarded to the client; it is not a raw sniffer of all transmissions nor a
continuous measurement. The firmware does not compute packet loss without an external, numbered test source.
Packets without RSSI, local packets and packets coming from MQTT/UDP/API are skipped.
A missing measurement is not replaced by a fictitious RSSI from the NodeInfo database.
History and statistics are reset when the CYD restarts. Recording does not start automatically.

## Verification status

The protocol parser was checked in native C++ with AddressSanitizer and UndefinedBehaviorSanitizer:
negative RSSI, zero RSSI vs missing field, negative and zero coordinates, corrupted data,
framing and synchronisation, unknown fields and 10,000 random inputs.
The application and the screens (including the dashboard, node map with all zoom levels, noise screen, probe/ACK logic and file naming) were run on a host
against simplified library stubs with the same sanitizers — this does not replace a build for the ESP32.
No PlatformIO/Arduino tooling or dependency downloads were available in that environment.
No target ESP32 build or physical CYD/Tracker test was performed; no .bin file is provided.
Old version of the interface: main.cpp.bak (a single .cpp replacing src/main.cpp).

Parser test, if a C++ compiler is available:

```sh
c++ -std=c++11 -I include test/protocol_test.cpp -o protocol-test
./protocol-test
```

## Source documentation

- https://meshtastic.org/docs/configuration/module/serial/
- https://meshtastic.org/docs/development/device/client-api/
- https://github.com/meshtastic/protobufs/blob/master/meshtastic/mesh.proto
- https://resource.heltec.cn/download/Wireless_Tracker/Wireless_Tracker%20_1.1.pdf
- https://resource.heltec.cn/download/Wireless_Tracker_V2/Wireless_Tracker_v2_Datasheet/Wireless%20Tracker%20v2.pdf
- https://github.com/Bodmer/TFT_eSPI

## Interface language
The UI of the CYD and of the iPhone app is in English (READABLE / MARGINAL / UNREADABLE, HOPS, NOISE, CH UTIL, AIR TX, etc.). Code comments and `README.md` are in Polish; this file is the English version of the README.

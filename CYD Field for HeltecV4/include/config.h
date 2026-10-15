#pragma once
constexpr int RADIO_RX=35, RADIO_TX=27;
constexpr unsigned RADIO_BAUD=115200;
constexpr int BACKLIGHT=21, SD_CS=5;
constexpr int TOUCH_CS=33, TOUCH_CLK=25, TOUCH_MOSI=32, TOUCH_MISO=39;
constexpr unsigned MAX_NODES=128, HISTORY=48;
// Skale wskaznikow i progi oceny czytelnosci odbioru (dla ostatniego odcinka radiowego).
constexpr int GAUGE_MIN=-130, GAUGE_MAX=-30;   // zakres wskaznika RSSI [dBm]
constexpr int SNR_MIN=-20, SNR_MAX=15;         // zakres paska SNR [dB]
constexpr int HOP_MAX=7;                       // skala "Przekazany" (max hop_limit Meshtastic)
constexpr float SNR_GOOD=-5.0f, SNR_FAIR=-15.0f;   // CZYTELNY / GRANICZNY
constexpr int RSSI_GOOD=-115, RSSI_FAIR=-125;
// Aktywny test lacza (wlaczany po wybraniu wezla): pakiet bezposredni na port prywatny z zadaniem ACK.
// Kazdy test to transmisja LoRa - nie skracaj interwalu bez potrzeby (limit wypelnienia i czas antenowy sieci).
constexpr uint32_t PROBE_INTERVAL_MS=15000, PROBE_TIMEOUT_MS=25000;
constexpr unsigned PROBE_PORT=256;       // PRIVATE_APP: bez wiadomosci widocznych dla uzytkownikow
constexpr unsigned PROBE_CHANNEL=0;      // indeks kanalu (0 = podstawowy)
constexpr unsigned PROBE_HOP_LIMIT=0;    // 0 = domyslny z konfiguracji Trackera

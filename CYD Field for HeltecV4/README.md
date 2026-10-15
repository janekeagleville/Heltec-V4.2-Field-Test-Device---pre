# Meshtastic Mazowsze — CYD Field Tester

Firmware dla CYD ESP32-2432S028 połączonego przez UART z Heltec WiFi LoRa 32 V4 (z modułem GNSS).
V4 zachowuje standardowy Meshtastic oraz GPS; CYD jest klientem API i ekranem pomiarowym.

> To osobny projekt dla Heltec V4 (kopia projektu dla Heltec Wireless Tracker). W dokumencie słowo „Tracker” oznacza węzeł podłączony do CYD, tutaj Heltec V4; na ekranie CYD kafelek nazywa się DEVICE.

## Funkcje

- Ekran startowy MESHTASTIC MAZOWSZE FIELD TESTER (include/splash.h, RGB565 320x240) i spójny ciemnozielony interfejs: nagłówek, pasek GPS, panele, wskaźnik RSSI, kropki przekazań, wykres.
- Znacznik czytelności odbioru: READABLE / MARGINAL / UNREADABLE (progi w include/config.h).
- Lista do 64 nadawców z pomiarami odebranymi podczas bieżącej sesji, najnowsze na górze.
- RSSI (dBm), SNR (dB), czas od odbioru; dotknięcie wiersza otwiera szczegóły.
- W szczegółach: licznik, średnia/minimum/maksimum RSSI i wykres 48 ostatnich próbek.
- Droga pakietu: bezpośrednia, przekazana lub nieustalona. Zerowy/nieobecny hop_start jest celowo traktowany jako nieustalony.
- Ostatnia znana pozycja lokalnego Trackera, niezależnie od pozycji odległych węzłów.
- Zapis na microSD: przycisk LOG na ekranie pracy tworzy plik `Nazwa-noda_DDMMYY_HH-MM-SS.csv`.
- Ponowienie połączenia API, heartbeat i kalibracja dotyku przechowywana w pamięci CYD.

## Ekrany

- Lista (NODE, RSSI, SNR, WIEK): kolorowa kropka i kolor RSSI według czytelności; numer strony po prawej.
- Szczegóły węzła: wskaźnik łukowy RSSI (-130...-30 dBm) ze znacznikiem czytelności, SNR z paskiem,
  skala „Przekazany" 0...7 hop (kropki), statystyki N/MIN/MAX/SR. i wykres ostatnich próbek z linią progu CZYTELNY.
- Zmiana skal i progów: GAUGE_MIN/MAX, SNR_MIN/MAX, HOP_MAX, SNR_GOOD/FAIR, RSSI_GOOD/FAIR w include/config.h.

Ocena dotyczy ostatniego odcinka radiowego: CZYTELNY gdy SNR >= -5 dB i RSSI >= -115 dBm,
GRANICZNY gdy SNR >= -15 dB i RSSI >= -125 dBm, w pozostałych przypadkach NIECZYTELNY.
Progi są założeniem startowym; zależą od presetu LoRa (np. LongFast dekoduje do ok. -17 dB SNR) — dostosuj je w config.h.
Brak pola SNR w proto3 jest odczytywany jako 0, co daje ocenę CZYTELNY; interpretuj ostrożnie.

## Połączenie (Heltec WiFi LoRa 32 V4)

| Heltec V4 | CYD |
|---|---|
| GPIO4 (J3 pin 15), UART TX | IO35 na P3, RX |
| GPIO6 (J3 pin 17), UART RX | IO27 na CN1, TX |
| GND (J3 pin 1) | GND |

UART 3,3 V (ESP32-S3). Piny CYD wybieraj po oznaczeniach na PCB, nie numerach pozycji w złączu.
`GPIO4` i `GPIO6` to wolne piny ADC/dotyk na złączu J3 płytki V4.2 (J3: pin 15 = GPIO4, pin 17 = GPIO6, pin 1 = GND), nieużywane przez Meshtastic ani przez złącze GNSS.
Zajęte w V4.2 są m.in. GPIO0 (przycisk PRG), 1 (pomiar baterii), 2/7/46 (wzmacniacz mocy GC1109), 8–14 (SX1262), 17/18/21 (OLED), 35 (LED), 36 (Vext), 37 (ADC), 38–42 (GNSS: UART, wake-up, PPS, reset).
Unikaj GPIO3, 45 i 46 (piny rozruchowe; 46 steruje też wzmacniaczem) oraz GPIO5 (w V4.3 steruje wzmacniaczem), więc wybór 4 i 6 działa też na V4.3.
Źródło: arkusz danych Heltec V4.3.1 (tabele J2/J3; w tabelach nie ma różnic pinów V4.2/V4.3) i definicje wariantu `heltec_v4` w firmware Meshtastic. Przed podłączeniem sprawdź opisy pinów na swojej płytce.
GPS: moduł L76K podłącza się do złącza GNSS (SH1.25, 8 pin) V4; Meshtastic włącza go sam, a dane z niego trafiają do CYD przez API (position_source=2). Moduł jest dodatkiem do płytki — upewnij się, że go masz.
Zasilaj każdą płytkę przez jej USB (V4: USB-C); wspólny powerbank z dwoma wyjściami jest możliwy.
Nie łącz wyjść 3,3 V lub 5 V obu płytek. V4 musi mieć podłączoną antenę LoRa (i GNSS), zanim ją włączysz.

W Meshtastic na V4: Serial Enabled = true, Mode = PROTO, TXD = 4, RXD = 6,
Baud = 115200. GPS pozostaje włączony. Region, preset i kanały ustaw odpowiednio do swojej sieci.
To samo z CLI (`pip install meshtastic`, V4 na USB):
`meshtastic --set serial.enabled true --set serial.mode PROTO --set serial.rxd 6 --set serial.txd 4 --set serial.baud BAUD_115200`
Firmware CYD nie ustawia kanałów ani parametrów radiowych i nie wysyła wiadomości LoRa.
Wysyła tylko lokalne żądanie konfiguracji API i heartbeat. Gdy nie nadchodzą ramki, CYD skanuje prędkości portu (115200, 38400, 9600, 57600).

## Budowanie i wgrywanie

Projekt jest przeznaczony dla PlatformIO. Otwórz ten folder w VS Code z rozszerzeniem PlatformIO,
wybierz środowisko `cyd_st7789`, następnie Build i Upload, gdy CYD jest podłączony do USB.
PlatformIO pobierze platformę ESP32 i bibliotekę TFT_eSPI w wersjach wskazanych w platformio.ini.
Nie trzeba ręcznie edytować globalnego User_Setup.h biblioteki.

Z terminala z zainstalowanym PlatformIO:

```sh
pio run -e cyd_st7789
pio run -e cyd_st7789 -t upload
pio device monitor -b 115200
```

Domyślne środowisko to `cyd_st7789_noinv` (ST7789 bez inwersji kolorów). Jeśli czerń wychodzi jako biel,
a cyjan jako czerwień, użyj środowiska z odwrotną inwersją; gdy kolory są poprawne, ale wersja `cyd_st7789`
(z TFT_INVERSION_ON) daje właściwy obraz na Twoim egzemplarzu, wybierz ją.
Dla CYD z dwoma portami USB przyjęto ST7789, ale sam wygląd PCB nie potwierdza kontrolera.
Jeśli obraz nie działa lub jest uszkodzony, skompiluj alternatywne środowisko `cyd_ili9341`.
Kolory, orientacja i dotyk wymagają sprawdzenia na fizycznym panelu.

## Obsługa

Przy pierwszym uruchomieniu po logo dotknij kolejno czterech krzyżyków, aby skalibrować dotyk.
Kalibracja pozostaje zapisana po wyłączeniu.

- DALEJ: następna strona listy.
- Dotknięcie węzła: ekran szczegółów i wykres RSSI.
- LISTA: powrót do listy.
- LOG (dół ekranu pracy, środkowy klawisz): tworzy nowy plik i zapisuje pakiety wybranego noda; podczas zapisu przycisk pokazuje STOP i liczbę wierszy. Z listy węzłów zapisuje wszystkie.
- STOP i licznik: zakończenie zapisu i zamknięcie pliku.
- KAL: powtórzenie kalibracji.
- NO SD: karta nie została zamontowana; dotknięcie ponawia próbę.
- BLAD SD: zapis lub otwarcie pliku nie powiodły się; stan jest też raportowany przez USB.
- Krótki przycisk BOOT: następna strona listy / wyjście ze szczegółów.
- Długi BOOT (>1 s): start/stop zapisu.

Monitor USB przy 115200: `l` start/stop zapisu, `t<epoch>` ręczne ustawienie zegara (np. `t1791650820`), `c` kalibracja, `r` ponowne połączenie API.

Kartę microSD sformatuj jako FAT32 i włóż przed uruchomieniem. Zatrzymaj zapis przed jej wyjęciem.
Dane są opróżniane z bufora co 2 s; nagłe odłączenie zasilania może utracić ostatnie rekordy lub uszkodzić plik.
SPI ekranu, SD i dotyku są rozdzielone: TFT HSPI (12/13/14/15), SD VSPI (19/23/18/5),
a XPT2046 obsługiwany programowo (MISO39/MOSI32/CLK25/CS33).

## Pulpit (ekran startowy)
Sześć kafelków ze statusem węzła, na podstawie danych z Trackera przez UART: BATTERY (% i napięcie, USB = zasilanie zewnętrzne),
NODES (słyszane / aktywne w 15 min / znane), RADIO (preset, częstotliwość środkowa, SF i BW), CHANNEL (channel utilization, Tx airtime, nazwa kanału),
CHAT (ostatnia odebrana wiadomość tekstowa i licznik), DEVICE (ID, czas pracy, baud, ramki).
Klawisze: NODES ↔ dashboard (lewy), LOG (środek, na pulpicie zapisuje cały ruch), DASHBOARD / CAL (prawy; na pulpicie to kalibracja dotyku).
Bateria i zajętość kanału pojawiają się po pierwszej telemetrii Trackera (zwykle do kilkunastu minut; interwał ustawia się w module Telemetry).
Częstotliwość wyliczana jest dla regionów US, EU433, EU868 i ANZ; dla innych pokazana jest nazwa regionu.

## Mapa nodów (ekran „MAP”)
Wejście: dotknij kafelka NODES na pulpicie. Tło to mapa samego województwa mazowieckiego (granice, powiaty, Wisła/Bug, główne miasta, skala 50 km),
na niej węzły z pozycją: kolor jak w liście (zielony/bursztyn/czerwony wg jakości odbioru, szary = znany, jeszcze niesłyszany), biały pierścień = Tracker (własny GPS).
Po prawej lista (najpierw słyszane, najświeższe; potem najbliższe) z odległością od Trackera. Dotknij punkt lub wiersz = zaznaczenie (nazwa na mapie),
drugie dotknięcie = ekran pracy tego noda. Prawy klawisz przewija strony listy. Przyciski „+” / „-” w lewym dolnym rogu mapy zmieniają powiększenie: 1×, 2×, 3×, 4×, 6×, 8×, 12× (widok na pełną szerokość, bez listy; dotknięcie pustego miejsca przesuwa środek widoku, prawy klawisz „ZOOM OUT” wraca do 1×). Od 6× mapa zmienia się na szczegółową mapę Warszawy i okolic (od Mszczonowa po Mińsk Maz., od Góry Kalwarii po Różan i Pułtusk): dzielnice Warszawy, powiaty, Wisła, Narew, Bug, Wkra, Bzura, drogi krajowe i europejskie, linie kolejowe, obszary zabudowane, lotniska Chopina i Modlin oraz ok. 50 miejscowości (`include/mapwaw.h`, 0,14 km/piksel, generator `tools/gen_map_warsaw.py`, ok. 280 kB flasha; dane Natural Earth i granice dzielnic). Mapa nie zawiera ulic — takie dane nie były dostępne. Po powiększeniu krzyżak ze strzałkami w prawym dolnym rogu przesuwa widok o 60 pikseli (widok na pełną szerokość, bez listy; dotknięcie pustego miejsca przesuwa środek widoku, prawy klawisz „ZOOM OUT” wraca do 1×). Węzły poza Mazowszem nie są rysowane („poza mapa: N”).
Pozycje pochodzą z bazy węzłów Trackera i pakietów POSITION; węzły zwykle wysyłają je co kilkanaście minut, a niektóre nie wysyłają wcale lub mają ją zaokrągloną.
Tło: `include/mapdata.h` (RGB565 160×166 + maska granicy, widok 1×, generator `tools/gen_map.py`) oraz `include/maphires.h` (640×664, 16 kolorów, 4 bit/piksel, ok. 0,4 km/piksel, używana przy powiększeniu 2×–6×, generator `tools/gen_map_hi.py`; zajmuje ok. 210 kB flasha). Dane: granice [ppatrzyk/polska-geojson](https://github.com/ppatrzyk/polska-geojson), rzeki Natural Earth (domena publiczna).
Skala ok. 1,7 km/piksel — to mapa poglądowa, nie nawigacyjna.

## Widmo i szum (ekran „NOISE”)
Wejście: dotknij kafelka RADIO lub CHANNEL na pulpicie. Prawy klawisz (lub dotknięcie wykresu) zmienia przedział czasu (TIME 1m / 10m / 1h), lewy wraca na pulpit.
Meshtastic nie udostępnia pomiaru radia, więc ekran pokazuje **estymaty z odebranych pakietów**, a nie prawdziwe widmo pasma:
- NOISE = RSSI − SNR odebranego pakietu (dBm); FLOOR = 10. percentyl szumu w oknie; wykres: turkus = szum, bursztyn = RSSI sygnału.
- PKT/MIN = odebrane pakiety z ostatnich 60 s; CH UTIL i AIR TX = zajętość kanału i airtime z telemetrii Trackera, pasek „CHANNEL” to jej historia (próbka co 10 s).
- Bez ruchu w sieci nie ma pakietów, więc nie ma też szumu. CH UTIL i AIR TX odświeżają się tak rzadko, jak Tracker wysyła telemetrię.
- Prawdziwe widmo (skan częstotliwości, pomiar szumu w ciszy) wymaga dostępu do układu SX1262, czyli własnego firmware zamiast Meshtastic albo dodatkowego modułu radiowego.

## Nazwy plików i czas
Nazwa: `<nazwa noda>_DDMMYY_HH-MM-SS.csv`, czas lokalny (Polska, CET/CEST). System FAT nie dopuszcza dwukropka w nazwie, więc godzina ma myślniki.
Płytka nie ma zegara: czas pochodzi z pakietów odebranych przez Tracker (wymaga ważnego czasu, np. po złapaniu GPS). Bez czasu plik nazywa się `Name_NO-TIME_001.csv`.
Karta musi być FAT32. Gdy firmware SD nie obsługuje długich nazw, powstaje `/LOGnnnnn.csv` (komunikat 8.3 NAME). Komunikat NO SD: sprawdź kartę i ponów LOG.

## Format CSV

Separator: przecinek; separator dziesiętny: kropka. Importuj w Excelu jako CSV z tymi ustawieniami.
Jeżeli potrzebne jest rozszerzenie .csd, plik można przemianować; jego zawartość pozostaje CSV.

| Kolumna | Znaczenie |
|---|---|
| uptime_ms | Czas odbioru przez CYD od uruchomienia; licznik zawija się po ok. 49,7 dnia |
| rx_epoch | Czas odbioru z Trackera w sekundach Unix; 0 oznacza brak czasu |
| sender_id | Pełne ID pierwotnego nadawcy, szesnastkowo |
| packet_id | ID pakietu, szesnastkowo |
| rssi_dbm | RSSI ostatniego odcinka radiowego |
| snr_db | SNR ostatniego odcinka; brak pola w proto3 jest interpretowany jako 0 |
| hops | Liczba przeskoków lub -1 gdy nieustalona |
| relay_low_byte | Tylko ostatni bajt ID przekaźnika; nie identyfikuje go jednoznacznie |
| via_mqtt | Flaga MQTT; pakiety z tą flagą są wykluczane z pomiarów |
| transport | Mechanizm odbioru z protokołu; 0 także dla starszych wersji bez tego pola |
| tracker_lat, tracker_lon | Ostatnia znana pozycja lokalnego Trackera, w stopniach; puste przy braku pozycji |
| position_receipt_age_s | Czas od otrzymania tej pozycji przez CYD, nie od ustalenia GPS |
| position_live | 0 dla początkowej pozycji z bazy Trackera, 1 dla późniejszej aktualizacji API; nie potwierdza aktualnego fix GPS |
| position_source | 0 nieustalona, 1 ręczna, 2 wewnętrzny GPS, 3 zewnętrzny GPS |
| position_epoch | Czas przypisany pozycji (timestamp lub time z API), 0 gdy nieznany |
| gps_age_s | rx_epoch - position_epoch, jeśli oba czasy są dostępne i porównywalne; inaczej puste |

Współrzędne nie są pobierane z wiadomości odległego nadawcy. Przed wyjściem w teren sprawdź,
że Tracker ma aktualną pozycję z GPS (position_source=2), a gps_age_s jest rozsądne.
Gdy GPS przestanie dostarczać aktualizacji, zachowywana jest ostatnia pozycja i rośnie jej wiek.
Napis CACHE oznacza pozycję pobraną podczas początkowego odczytu bazy, o niepewnej aktualności.

## Granice pomiarów

RSSI/SNR po retransmisji opisują przekaźnik odbierany przez Tracker, a nie pierwotnego nadawcę.
Lista jest grupowana według pierwotnego nadawcy, dlatego używaj oznaczenia drogi i pola relay_low_byte.
API udostępnia pakiety przekazane do klienta; nie jest to surowy sniffer wszystkich transmisji ani pomiar
ciągły. Firmware nie oblicza utraty pakietów bez zewnętrznego, numerowanego źródła testowego.
Pakiety bez RSSI, pakiety lokalne i pakiety pochodzące z MQTT/UDP/API są pomijane.
Brak pomiaru nie jest zastępowany fikcyjnym RSSI z bazy NodeInfo.
Historia i statystyki są resetowane przy ponownym uruchomieniu CYD. Zapis nie startuje automatycznie.

## Stan weryfikacji

Sprawdzono parser protokołu w natywnym C++ z AddressSanitizer i UndefinedBehaviorSanitizer:
ujemny RSSI, zero RSSI vs brak pola, ujemne i zerowe współrzędne, uszkodzone dane,
ramkowanie i synchronizacja, nieznane pola oraz 10 000 losowych wejść.
Sprawdzono składnię aplikacji z uproszczonymi interfejsami bibliotek — to nie zastępuje kompilacji ESP32.
W tym środowisku nie było narzędzi PlatformIO/Arduino ani dostępu do pobrania zależności.
Warstwa ekranu (splash.h, ekrany, znacznik czytelności) przeszła sprawdzenie składni z uproszczonymi interfejsami bibliotek.
Nie wykonano kompilacji docelowej ESP32 ani testu fizycznego CYD/Trackera; pliku .bin nie dostarczono.
Stara wersja interfejsu: main.cpp.bak (jeden plik .cpp zastępujący src/main.cpp).

Test parsera, jeśli dostępny jest kompilator C++:

```sh
c++ -std=c++11 -I include test/protocol_test.cpp -o protocol-test
./protocol-test
```

## Dokumentacja źródłowa

- https://meshtastic.org/docs/configuration/module/serial/
- https://meshtastic.org/docs/development/device/client-api/
- https://github.com/meshtastic/protobufs/blob/master/meshtastic/mesh.proto
- https://resource.heltec.cn/download/Wireless_Tracker/Wireless_Tracker%20_1.1.pdf
- https://resource.heltec.cn/download/Wireless_Tracker_V2/Wireless_Tracker_v2_Datasheet/Wireless%20Tracker%20v2.pdf
- https://github.com/Bodmer/TFT_eSPI

## Język interfejsu
Interfejs CYD i aplikacji iPhone jest po angielsku (READABLE / MARGINAL / UNREADABLE, HOPS, NOISE, CH UTIL, AIR TX itd.). Komentarze w kodzie i ten README są po polsku.

# GUIwire-Server: Waveshare ESP32-S3-Touch-AMOLED-2.41

Dieser ESP-IDF-Port verwendet die gemeinsame GUIwire-C-Library für Protokoll,
Sitzungswechsel, Hot-Plug und Objektmodell. Der Hardware-Port übernimmt
UART1 und das 600 × 450 große RM690B0-QSPI-AMOLED. Touch ist derzeit **kein**
GUIwire-Eingabekanal; die Touch-Hardware bleibt unbenutzt.

Waveshare verkauft zwei nicht austauschbare PCB-Revisionen. Ohne `Rev2.0`
auf der Platine ist es V1, mit `Rev2.0` V2. Die passende Variante muss in
`idf.py menuconfig` unter „GUIwire ESP32-S3 AMOLED server“ gewählt werden.
Ohne Einstellung wird **V1** gebaut. V1 aktiviert Display-Power über EXIO1
und setzt das Display über GPIO21 zurück; V2 setzt es über EXIO0 zurück.
Beide verwenden QSPI GPIO9–14 und den I²C-Expander an GPIO47/48.

## GUIwire-Anschluss

Der Port verwendet **GPIO7 = UART1 TX** und **GPIO8 = UART1 RX**, beide an
den freien GPIO-Anschlüssen des Waveshare-Boards. Diese Pins gehen an die
TX- und RX-Eingänge einer GUIwire-Transceiver-Stufe; sie werden **nicht**
direkt miteinander oder mit der Patchbuchse verbunden. Tip führt DATA,
Sleeve GND. Das Patchkabel führt keine Versorgungsspannung. Die ESP32-S3-
GPIOs sind 3,3-V-Pins; weder 5 V noch Eurorack-CV direkt anlegen.

GPIO43/44 bleiben unbenutzt, damit UART0-/Boot-Ausgaben nicht auf dem
GUIwire-Link landen. Debug-Logs gehen über den nativen USB-Port.

## Bauen und flashen

ESP-IDF **5.5.x** installieren und aktivieren. Im Repository-Stammverzeichnis:

```sh
cd firmware/guiwire_server_esp32s3
idf.py set-target esp32s3
idf.py menuconfig             # V1/V2 wählen; optional adaptive Baudrate abschalten
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Den tatsächlichen macOS-Port zuvor mit `ls /dev/cu.*` ermitteln. Der
Komponentenmanager lädt `espressif/esp_lcd_sh8601` beim ersten Build.
`sdkconfig.defaults` aktiviert die 8-MB-Octal-PSRAM des Boards; der RGB565-
Szenenpuffer benötigt etwa 540 kB davon. Eine kleine interne DMA-Zeilenfläche
überträgt ausschließlich geänderte Bereiche an das Display. Der unabhängige
UART-Empfangstask verhindert, dass QSPI-Transfers die seriellen Bytes verlieren.

Der Server beginnt mit **9600 Baud** und kann bei passendem GUIwire-Client
9600/19200/38400/57600 aushandeln. Ohne Client zeigt er „Waiting for
client...“. Nach Abziehen oder Clientwechsel verwirft die gemeinsame
Host-Library die alte Sitzung und Szene; das neue Device liefert seine Szene
erneut. Bei deaktivierter Baud-Automatik bleiben beide Seiten bei 9600.

Die Revision und der physische Displaybetrieb konnten hier mangels Board und
installierter ESP-IDF nicht auf Hardware verifiziert werden. Vor dem ersten
Flash insbesondere den PCB-Aufdruck prüfen.

## Lokaler Renderer-Test ohne ESP-IDF

Aus dem Repository-Stammverzeichnis:

```sh
cc -std=c11 -Wall -Wextra -Werror \
  -Ifirmware/common/include -Ifirmware/ports/esp32/include \
  tests/guiwire_framebuffer.c firmware/ports/esp32/guiwire_framebuffer.c \
  -o /tmp/guiwire_framebuffer_test
/tmp/guiwire_framebuffer_test
```

Hardwarequellen: [Waveshare-Revisionen und Pinbelegung](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.41),
[V2-Displaybeispiel](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.41-V2/blob/main/02_Example/ESP-IDF/10_FactoryProgram/main/example_qspi_with_ram.c),
[Espressif-QSPI-Paneltreiber](https://components.espressif.com/components/espressif/esp_lcd_sh8601/versions/2.0.1/readme).

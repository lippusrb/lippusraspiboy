# Schema Elettrico di Sviluppo (Breadboard Sicura)

Versione minimale per prototipazione software. Include il diodo di sicurezza per consentire l'uso contemporaneo di Batteria e USB collegata al PC.

## ⚡ 1. Alimentazione Protetta

| Origine | Destinazione / Collegamento |
| :--- | :--- |
| **Batteria Rasoio (+)** | Schedina Rasoio (+) **E** Interruttore a scorrimento (Pin 1) |
| **Batteria Rasoio (-)** | Schedina Rasoio (-) **E** Rete GND generale |
| **Interruttore (Pin 2)** | Diodo Schottky (Anodo) **E** Pin `VIN` del PAM8302 |
| **Diodo Schottky (Catodo, striscia)**| Pin **VSYS (Pin 39)** del Pico |
| **Pico 3V3 OUT (Pin 36)** | `VCC` Display, `LED` Display, Resistenza (220Ω) -> LED Verde |
| **Rete GND generale** | Pin 38 Pico, GND Display, GND PAM8302, LED Verde (Catodo) |

---

## 🎮 2. Pulsanti di Gioco (Diretti a Massa)

| Pulsante | Pin Raspberry Pi Pico |
| :--- | :--- |
| **D-Pad** | Su: GP2, Giù: GP3, Sinistra: GP4, Destra: GP5 |
| **Azione** | A: GP6, B: GP7, X: GP8, Y: GP9 |
| **Sistema** | Start: GP21, Select: GP22 |

*(Tutti i secondi piedini dei pulsanti vanno a GND)*

---

## 📺 3. Display ILI9341 + Lettore SD (SPI Separati)

| Pin Modulo Display/SD | Pin Raspberry Pi Pico | Funzione |
| :--- | :--- | :--- |
| **SCK / CLK** | GP18 (Pin 24) | SPI0 Clock (Video) |
| **MOSI / SDI** | GP19 (Pin 25) | SPI0 TX (Video) |
| **CS / CE** | GP17 (Pin 22) | SPI0 Chip Select (Video) |
| **DC / RS** | GP15 (Pin 20) | Data / Command (Video) |
| **RESET / RST** | GP14 (Pin 19) | Reset Hardware (Video) |
| **SD_SCK** | GP10 (Pin 14) | SPI1 Clock (Lettore SD) |
| **SD_MOSI** | GP11 (Pin 15) | SPI1 TX (Lettore SD) |
| **SD_MISO** | GP12 (Pin 16) | SPI1 RX (Lettore SD) |
| **SD_CS** | GP13 (Pin 17) | SPI1 Chip Select (Lettore SD) |

---

## 🎵 4. Audio (Catena Lineare)

| Componente | Collegamento |
| :--- | :--- |
| **Pico GP20 (Pin 26)** | Inizio Resistenza da 220Ω |
| **Uscita Resistenza** | Condensatore 100nF (L'altro piedino a GND) **E** Pin 1 Rotella Volume 10k |
| **Rotella Volume (Pin 3)**| Rete GND generale |
| **Rotella Volume (Pin 2)**| Ingresso **A+** del modulo PAM8302 |
| **Modulo PAM8302 (A-)** | Rete GND generale |
| **Modulo PAM8302 (OUT)** | Poli + e - dello Speaker 1W |
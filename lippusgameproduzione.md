# Schema Elettrico Definitivo (Livello Produzione) - Console Custom Raspberry Pi Pico

Questo documento contiene l'architettura hardware completa e ottimizzata per la produzione. Include tutte le reti di disaccoppiamento e filtraggio necessarie per garantire stabilità assoluta dei segnali, assenza di riavvii termici/elettrici e un audio privo di interferenze di fondo.

---

## ⚡ 1. Alimentazione, Protezione e Livellamento (Power Block)

Questa sezione gestisce la batteria, la ricarica e distribuisce l'energia filtrata a tutto il sistema per prevenire *brownout* durante i picchi di assorbimento del display o dell'amplificatore.

| Componente (Origine) | Destinazione / Collegamento | Funzione / Note |
| :--- | :--- | :--- |
| **Batteria 18650 (+)** | Schedina Rasoio (+) **E** Interruttore a scorrimento (Pin 1) | Nodo principale tensione positiva. |
| **Batteria 18650 (-)** | Schedina Rasoio (-) **E** Rete GND generale | Nodo principale massa. |
| **Interruttore (Pin 2)** | Diodo Schottky (Anodo, senza striscia) **E** Pin `VIN` PAM8302 | L'amplificatore prende corrente pura senza passare dal diodo. |
| **Diodo Schottky (Catodo)** | Raspberry Pi Pico **VSYS (Pin 39)** | Protegge la batteria dai 5V quando colleghi l'USB al Pico. |
| **Cond. Elettr. 10µF - 47µF (+)**| Pin **VSYS (Pin 39)** del Pico | Stabilizza l'ingresso al microcontrollore. |
| **Cond. Elettr. 10µF - 47µF (-)**| Rete GND generale | Massa del condensatore di livellamento. |
| **Cond. Elettr. 100µF - 470µF (+)**| Pin **VIN** del PAM8302 | Fornisce i picchi di corrente per i bassi audio. |
| **Cond. Elettr. 100µF - 470µF (-)**| Rete GND generale | Massa del condensatore audio. |
| **Pico 3V3 OUT (Pin 36)** | `VCC` Display, `LED` Display, `VCC` SD | Linea logica 3.3V per le periferiche. |
| **Cond. Elettr. 10µF (+)** | Rete **3V3 OUT** (il più vicino possibile al display) | Stabilizza i picchi di assorbimento del pannello TFT. |
| **Cond. Ceramico 100nF (104)** | In parallelo al condensatore da 10µF su **3V3 OUT** | Filtra il "rumore" ad alta frequenza generato dal bus SPI. |
| **Tutti i poli negativi/GND** | Pin 38 del Pico, GND Display, GND SD, GND PAM8302 | **Rete GND generale:** Tutte le masse devono essere unite. |

---

## 🟢 2. Indicatore di Stato (Power LED)

Si attiva solo a valle dell'interruttore e quando il Pico genera correttamente i 3.3V logici.

| Componente | Destinazione / Collegamento |
| :--- | :--- |
| **Pico 3V3 OUT (Pin 36)** | Resistenza (220Ω - 330Ω) |
| **Resistenza (Uscita)** | **LED Verde (Anodo / gamba lunga)** |
| **LED Verde (Catodo / gamba corta)** | Rete GND generale |

---

## 🎮 3. Input: Pulsanti di Gioco (Diretti a GND)

Nessuna resistenza esterna necessaria. Il software attiverà il pull-up interno (`gpio_pull_up`). Il debounce va gestito via software nel micro-kernel.

| Pulsante | Pin Raspberry Pi Pico | Collegamento 2 |
| :--- | :--- | :--- |
| **Su** | GP2 (Pin 4) | Rete GND |
| **Giù** | GP3 (Pin 5) | Rete GND |
| **Sinistra** | GP4 (Pin 6) | Rete GND |
| **Destra** | GP5 (Pin 7) | Rete GND |
| **A (Azione 1)** | GP6 (Pin 9) | Rete GND |
| **B (Azione 2)** | GP7 (Pin 10) | Rete GND |
| **X (Azione 3)** | GP8 (Pin 11) | Rete GND |
| **Y (Azione 4)** | GP9 (Pin 12) | Rete GND |
| **Start** | GP21 (Pin 27) | Rete GND |
| **Select** | GP22 (Pin 29) | Rete GND |

---

## 📺 4. Display ILI9341 (Bus SPI0 - 60MHz+)

Cablaggio ottimizzato per tracce corte. Mantenere i cavi SPI sotto i 5 cm per evitare corruzione grafica alle alte frequenze.

| Pin Display | Pin Raspberry Pi Pico | Funzione |
| :--- | :--- | :--- |
| **SCK / CLK** | GP18 (Pin 24) | SPI0 Clock |
| **MOSI / SDI** | GP19 (Pin 25) | SPI0 TX (Dati dal Pico al Display) |
| **MISO / SDO** | *Non collegato* | Il display è in sola scrittura. |
| **CS / CE** | GP17 (Pin 22) | SPI0 Chip Select |
| **DC / RS** | GP15 (Pin 20) | Data / Command |
| **RESET / RST** | GP14 (Pin 19) | Hardware Reset |

---

## 💾 5. Lettore MicroSD (Bus SPI1 - 10-25MHz)

Bus hardware fisicamente separato dalla grafica per permettere stream audio o caricamento dati asincrono senza stuttering a schermo.

| Pin Lettore SD | Pin Raspberry Pi Pico | Funzione |
| :--- | :--- | :--- |
| **SCK** | GP10 (Pin 14) | SPI1 Clock |
| **MOSI** | GP11 (Pin 15) | SPI1 TX (Comandi verso la SD) |
| **MISO** | GP12 (Pin 16) | SPI1 RX (Dati dalla SD al Pico) |
| **CS** | GP13 (Pin 17) | SPI1 Chip Select |

---

## 🎵 6. Audio Subsystem (Filtro, Controllo e Amplificazione)

Converte il PWM digitale in un'onda analogica pulita, ne regola l'ampiezza in hardware per mantenere intatta la risoluzione del segnale, e la amplifica.

1. **Sorgente PWM:** Collega il pin **GP20 (Pin 26)** a una **Resistenza (220Ω)**.
2. **Filtro RC Passa-Basso:** All'uscita della resistenza, collega un **Condensatore Ceramico 100nF (104)**; collega l'altro piedino del condensatore a **GND**.
3. **Ingresso Volume:** Dal nodo tra resistenza e condensatore, entra nel **Pin 1** (esterno) del potenziometro rotativo "dial" da 10k (B10K).
4. **Massa Volume:** Collega il **Pin 3** (l'altro pin esterno della rotellina) a **GND**.
5. **Uscita Volume:** Collega il **Pin 2** (il cursore centrale della rotellina) all'ingresso **A+** del modulo PAM8302.
6. **Massa Audio:** Collega l'ingresso **A-** del PAM8302 a **GND**.
7. **Speaker:** Collega i pin **OUT+** e **OUT-** del PAM8302 ai poli del mini altoparlante da 1W.

# Lippus Boy

Firmware di avvio e libreria grafica per la console su Raspberry Pi Pico descritta nello schema elettrico. All'accensione il Pico legge la scheda SD. Se trova un solo gioco lo avvia, se ne trova di più mostra un menu. La libreria (chiamate grafiche, pad e audio) resta nel firmware: i giochi la usano come una DirectX piccola, senza linkare il driver del display.

## Come è organizzato

Il Pico ha 2 MB di flash e 264 KB di RAM. Il firmware occupa i primi 256 KB di flash e ci resta sempre. Ogni gioco è un file `.lbg` sulla SD: all'avvio viene copiato nello slot di flash che parte da `0x10040000` ed eseguito da lì. La RAM in alto (circa 74 KB) è dei dati del gioco. Il framebuffer 320×240 a 16 bit sta nella RAM del firmware.

Lo stack è di 4 KB. Gli array grossi di un gioco vanno in variabili globali o in `sys_alloc()`, non come variabili locali.

Tenere premuto **SELECT** durante l'accensione apre il menu anche se sulla scheda c'è un solo gioco. Dentro un gioco, **START + SELECT** per circa un secondo torna al menu. Il gioco può chiamare anche `sys_exit()`.

## Hardware

| Segnale | GPIO |
| --- | --- |
| Joystick su, giù, sinistra, destra, click | GP2, GP3, GP4, GP5, GP0 |
| A, B, X, Y | GP6, GP7, GP8, GP9 |
| Start, Select | GP21, GP22 |
| Display ILI9341: SCK, MOSI, CS, DC, RST | GP18, GP19, GP17, GP15, GP14 |
| MicroSD: SCK, MOSI, MISO, CS | GP10, GP11, GP12, GP13 |
| Audio PWM | GP20 |

I pulsanti chiudono verso massa. Il display è in sola scrittura, landscape 320×240. Se l'immagine esce specchiata, cambia `LIPPUS_LCD_MADCTL` in `src/board.h` (`0x28`, `0xE8`, `0x48`, `0x88`). Se i pixel tremano sul cablaggio di prova, abbassa `LIPPUS_LCD_BAUD` da 62500000 a 31250000.

## Compilare e flashare

Serve il [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.1 o successivo, CMake, Ninja e la [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (`arm-none-eabi-gcc` con newlib). Se quel compilatore non è il primo nel `PATH`:

```bash
export PICO_TOOLCHAIN_PATH=/percorso/della/toolchain/bin
```

```bash
export PICO_SDK_PATH=/percorso/del/pico-sdk
cmake -B build -G Ninja
ninja -C build
```

Per provare un gioco sul Mac, senza copiarlo sul Pico:

```
./play garden_defender
```

Al posto di `garden_defender` va il nome della cartella in `games/`. Le frecce sono il joystick, `A` `S` `D` `F` i pulsanti A B X Y, `1` è Select, `2` è Start, la barra spaziatrice è il click dello stick. `1` e `2` insieme chiudono la finestra, come Start+Select sulla console.

Tieni premuto BOOTSEL, collega il Pico in USB e copia `build/lippus_os.uf2`.

Formatta la microSD in **FAT32** (non exFAT). Crea la cartella `GAMES` e copiaci `build/palla.lbg` e `build/tasti.lbg`. Con due file parte il menu. Con uno solo parte quel gioco.

`Palla` è un rimbalzo con racchetta. `Tasti` accende ogni pulsante, muove uno sprite e fa suonare A, B, X e Y: serve anche a provare il cablaggio. `Pacman` è il labirinto in verticale: gira il vetro prima di avviarlo, la console resta orizzontale e lo stick non cambia. `Maze Mouse` è un altro labirinto in verticale, con un topo, dei gatti, formaggio e ciotole di latte: anche lì il vetro si gira prima, e lo stick non cambia. Il file da copiare è `build/maze_mouse.lbg`. `Garden Defender` è in orizzontale: un giardiniere con la pompa tiene lontani gli insetti, i vasi si sgretolano e ogni tanto passa una farfalla. Il file è `build/garden_defender.lbg`.

Per i log di bring-up, metti `LIPPUS_DEBUG_UART` a 1 in `src/board.h`. L'uscita è UART0 a 115200 sul pin GP16.

## Scrivere un gioco

Un gioco è un programma C freestanding. Il punto di ingresso è `lippus_main()`, non `main()`.

```c
#include "lippus.h"

void lippus_main(void) {
    int x = 40;
    for (;;) {
        lippus_pad_t pad = pad_poll();
        if (pad_held(&pad, LIPPUS_RIGHT)) x += 2;
        if (pad_held(&pad, LIPPUS_LEFT)) x -= 2;

        gfx_clear(LIPPUS_RGB(12, 10, 28));
        gfx_fill_circle(x, 120, 8, LIPPUS_AMBER);
        gfx_text(8, 8, "CIAO", LIPPUS_CREAM, 2);
        gfx_present();
        sys_frame(33);
    }
}
```

Nel `CMakeLists.txt` di questo repo:

```cmake
lippus_add_game(mio DISPLAY_NAME "Mio gioco" SOURCES games/mio/main.c)
```

La build produce `mio.lbg`. Il nome nel menu è quello di `DISPLAY_NAME`.

Senza altre indicazioni il gioco è orizzontale, 320×240. Per uno schermo verticale, 240×320, nel file del gioco:

```c
const uint32_t lippus_orientation = LIPPUS_PORTRAIT;
```

Il vetro si ruota a mano prima di far partire il gioco. La console resta orizzontale. Il firmware non legge la posizione del pannello: l'orientamento è quello scritto nel gioco, orizzontale se la riga manca. All'ingresso in `lippus_main()` gira solo l'immagine, e al menu la riporta orizzontale. Dimensioni e disegno passano da `gfx_width()` e `gfx_height()`.

Lo stick non segue il vetro. Su, giù, sinistra e destra sono sempre i pin della console, anche con il gioco in verticale. Se l'immagine verticale esce capovolta, in `src/board.h` scambia `LIPPUS_LCD_MADCTL_PORTRAIT` con `0x88`.

La cartella `sdk/` è quella da dare a chi sviluppa: include, startup, linker e `sdk/tools/pack_lbg.py`. Un gioco non va linkato con il Pico SDK. Chiama solo `gfx_*`, `pad_*`, `snd_*` e `sys_*`.

| Chiamata | Effetto |
| --- | --- |
| `gfx_clear`, `gfx_pixel`, `gfx_fill_rect`, `gfx_rect`, `gfx_line` | disegno sul framebuffer |
| `gfx_circle`, `gfx_fill_circle` | cerchi |
| `gfx_blit` | sprite RGB565, con colore chiave opzionale (`LIPPUS_MAGENTA`) |
| `gfx_text`, `gfx_text_width` | font 8×8, `scale` intero, `\n` va a capo |
| `gfx_clip` | rettangolo di clip; larghezza 0 lo azzera |
| `gfx_present` | avvia l'invio del frame e torna subito |
| `pad_poll`, `pad_held`, `pad_pressed` | joystick, A B X Y, Start, Select, click |
| `snd_tone`, `snd_stop` | nota quadra sulla voce 0, finché non la fermi |
| `snd_voice` | voce 0–3, onda quadra, triangolare o rumore, volume 0–255, durata in ms (0 = tieni) |
| `snd_tune` | musichetta in sottofondo: array di `{hz, ms}`, `hz` 0 è una pausa |
| `sys_ticks`, `sys_sleep`, `sys_frame` | tempo e ritmo del frame |
| `sys_alloc`, `sys_alloc_reset` | memoria a bump, senza free singolo |
| `sys_exit` | torna al menu |

Il suono non è un buzzer. Il PWM del GP20 passa dal filtro 220 Ω + 100 nF e dal potenziometro: il firmware lo usa come un piccolo convertitore, a 20 kHz, con quattro voci. Il potenziometro è il volume generale. Una musichetta e un effetto stanno su voci diverse e vanno avanti mentre il gioco disegna.

```c
static const lippus_note_t motivo[] = {
    {523, 120}, {659, 120}, {784, 180}, {0, 60}, {523, 240},
};
snd_tune(0, SND_TRIANGLE, 180, motivo, 5);
snd_voice(1, SND_NOISE, 3000, 160, 80);
```

I colori sono RGB565. `LIPPUS_RGB(r, g, b)` accetta canali da 0 a 255.

`gfx_present` manda tutto il quadro. Il secondo core del Pico fa l'invio, circa 20 ms a 62.5 MHz, e `gfx_present` torna subito: nel frattempo il gioco aggiorna logica e legge i tasti. Il disegno successivo aspetta la fine dell'invio, perché il framebuffer è uno solo. I tasti si campionano ogni 16 ms, anche durante l'invio. Una pressione fra un frame e l'altro resta in `pressed` o `released` fino a `pad_read`.

Il gioco non registra interrupt e non tocca SPI, flash o GPIO. Display, SD e PWM restano al firmware.

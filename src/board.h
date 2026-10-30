#pragma once

/* Piedinatura presa dallo schema di produzione della console.
   I pulsanti chiudono verso GND: il firmware attiva il pull-up. */

#define LIPPUS_PIN_CLICK 0
#define LIPPUS_PIN_UP 2
#define LIPPUS_PIN_DOWN 3
#define LIPPUS_PIN_LEFT 4
#define LIPPUS_PIN_RIGHT 5
#define LIPPUS_PIN_A 6
#define LIPPUS_PIN_B 7
#define LIPPUS_PIN_X 8
#define LIPPUS_PIN_Y 9

#define LIPPUS_PIN_SD_SCK 10
#define LIPPUS_PIN_SD_MOSI 11
#define LIPPUS_PIN_SD_MISO 12
#define LIPPUS_PIN_SD_CS 13

#define LIPPUS_PIN_LCD_RST 14
#define LIPPUS_PIN_LCD_DC 15
#define LIPPUS_PIN_LCD_CS 17
#define LIPPUS_PIN_LCD_SCK 18
#define LIPPUS_PIN_LCD_MOSI 19

#define LIPPUS_PIN_AUDIO 20
#define LIPPUS_PIN_START 21
#define LIPPUS_PIN_SELECT 22

/* 62.5 MHz e' il massimo utile del SPI del RP2040. Se l'immagine trema
   sulla breadboard, abbassa questo valore a 31250000 o 15625000. */
#define LIPPUS_LCD_BAUD 62500000u
#define LIPPUS_SD_BAUD_INIT 400000u
#define LIPPUS_SD_BAUD_RUN 12500000u

/* MADCTL del ILI9341. La coppia e' la rotazione di 90 gradi del controller:
   orizzontale 320x240, verticale 240x320. Se il quadro intero esce
   specchiato (si legge namcap al posto di pacman), prova 0xE8 e 0x88.
   Se solo le lettere dentro la parola sono a specchio, e' il font. */
#define LIPPUS_LCD_MADCTL_LANDSCAPE 0x28
#define LIPPUS_LCD_MADCTL_PORTRAIT 0x48
#define LIPPUS_LCD_MADCTL LIPPUS_LCD_MADCTL_LANDSCAPE
#define LIPPUS_LCD_INVERT 0
#define LIPPUS_LCD_X_OFFSET 0
#define LIPPUS_LCD_Y_OFFSET 0

/* 1 = log su UART0 TX, piedino GP16, 115200 8N1. */
#define LIPPUS_DEBUG_UART 0

# -*- coding: utf-8 -*-
import zlib
import struct

def save_png(width, height, rgba_pixels, filename):
    """Salva un PNG a pixel reali (RGBA) senza usare librerie esterne."""
    img_data = b""
    for y in range(height):
        img_data += b"\x00"  # Filtro None per riga
        for x in range(width):
            img_data += bytes(rgba_pixels[y * width + x])
    
    png_signature = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0) # 6 = RGBA
    
    def make_chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    
    chunks = [
        make_chunk(b"IHDR", ihdr),
        make_chunk(b"IDAT", zlib.compress(img_data)),
        make_chunk(b"IEND", b"")
    ]
    
    with open(filename, "wb") as f:
        f.write(png_signature + b"".join(chunks))

# ==========================================
# DEFINIZIONE COLORI (R, G, B, A)
# ==========================================
_ = (0, 0, 0, 0)          # Trasparente puro
N = (0, 0, 0, 255)        # Nero (Contorni interni e maschera)
R = (255, 182, 193, 255)  # Rosa per naso/interno orecchie del topo

# Topo
P_TOPO = (176, 132, 86, 255)   # Pelliccia
O_TOPO = (120, 78, 46, 255)    # Orecchie

# Gatti (B=Corpo, O=Interno Orecchio/Dettaglio)
G_COLORI = {
    "arancio":      {"B": (255, 140, 32, 255),  "O": (190, 80, 10, 255)},
    "grigio":       {"B": (170, 174, 184, 255), "O": (120, 124, 134, 255)},
    "marrone_scuro":{"B": (96, 72, 58, 255),    "O": (60, 45, 35, 255)},
    "crema":        {"B": (214, 186, 140, 255), "O": (160, 130, 90, 255)}
}
G_SONNO = (96, 116, 168, 255)   # Gatto addormentato
W       = (255, 255, 255, 255) # Bianco (occhi, latte, frutti)

# Muri ed Elementi
M = (150, 78, 52, 255)     # Mattone
S = (86, 46, 34, 255)      # Malta scura
T = (132, 140, 152, 255)   # Tubo grigio
F = (206, 212, 220, 255)   # Filo chiaro del tubo
Y = (255, 220, 0, 255)     # Giallo formaggio/frutta
Y_O = (196, 140, 24, 255)   # Ombra formaggio
L_A = (198, 226, 255, 255) # Azzurro interno ciotola
PORTA = (92, 58, 36, 255)  # Legno porta

# ==========================================
# 1. COSTRUZIONE MATRICI TOPO (16x16)
# ==========================================
# Definiamo la sagoma base e la ruotiamo/adattiamo per i 4 versi
def get_topo_sprite(direzione):
    # Corpo base centrato su (8,8)
    grid = [[_]*16 for _ in range(16)]
    
    if direzione == "sinistra":
        # Coda a destra (x=12..14), Corpo centrale (x=5..11), Naso a sinistra (x=3)
        for y in range(6, 11):
            for x in range(5, 12): grid[y][x] = P_TOPO
        # Orecchie
        grid[4][6] = O_TOPO; grid[4][9] = O_TOPO; grid[5][6] = N; grid[5][9] = N
        # Occhio
        grid[6][5] = N
        # Naso
        grid[8][3] = R; grid[8][4] = N
        # Coda
        grid[9][12] = P_TOPO; grid[8][13] = P_TOPO; grid[7][14] = P_TOPO
    elif direzione == "destra":
        for y in range(6, 11):
            for x in range(4, 11): grid[y][x] = P_TOPO
        grid[4][6] = O_TOPO; grid[4][9] = O_TOPO; grid[5][6] = N; grid[5][9] = N
        grid[6][10] = N
        grid[8][12] = R; grid[8][11] = N
        grid[9][3] = P_TOPO; grid[8][2] = P_TOPO; grid[7][1] = P_TOPO
    elif direzione == "su":
        for y in range(5, 12):
            for x in range(5, 11): grid[y][x] = P_TOPO
        # Orecchie in alto aperte ai lati
        grid[4][4] = O_TOPO; grid[4][11] = O_TOPO
        # Naso in alto al centro
        grid[3][7] = R; grid[3][8] = R
        # Occhi
        grid[5][6] = N; grid[5][9] = N
        # Coda in basso
        grid[12][7] = P_TOPO; grid[13][7] = P_TOPO; grid[14][8] = P_TOPO
    elif direzione == "giu":
        for y in range(4, 11):
            for x in range(5, 11): grid[y][x] = P_TOPO
        grid[3][5] = O_TOPO; grid[3][10] = O_TOPO
        grid[11][7] = R; grid[11][8] = R
        grid[9][6] = N; grid[9][9] = N
        grid[3][8] = P_TOPO; grid[2][8] = P_TOPO
        
    # Applica bordi neri interni/esterni protettivi dove il pixel tocca il trasparente
    flat = []
    for y in range(16):
        for x in range(16):
            flat.append(grid[y][x])
    return flat

# Icona vita 8x8 (Topo miniaturizzato verso sinistra)
vita_pixels = [
    _,_,_,_,_,_,_,_,
    _,_,O_TOPO,_,_,_,_,_,
    _,P_TOPO,P_TOPO,P_TOPO,P_TOPO,_,_,_,
    R,N,P_TOPO,P_TOPO,P_TOPO,_,_,_,
    _,P_TOPO,P_TOPO,P_TOPO,P_TOPO,P_TOPO,_,_,
    _,_,P_TOPO,P_TOPO,_,P_TOPO,_,_,
    _,_,_,_,_,_,_,_,
    _,_,_,_,_,_,_,_
]

# ==========================================
# 2. COSTRUZIONE MATRICI GATTI (16x16)
# ==========================================
def get_gatto_sprite(col_name, stato):
    grid = [[_]*16 for _ in range(16)]
    if col_name == "sonno":
        B, O = G_SONNO, (70, 85, 130, 255)
    else:
        B, O = G_COLORI[col_name]["B"], G_COLORI[col_name]["O"]
        
    # Sagoma Base Corpo Tondo 14x12 agganciato al centro
    if stato in ["su", "giu", "sinistra", "destra", "sonno"]:
        for y in range(4, 13):
            for x in range(4, 12): grid[y][x] = B
        # Orecchie a punta in alto
        grid[3][4] = O; grid[3][11] = O
        grid[4][5] = N; grid[4][10] = N

    if stato == "sinistra":
        grid[6][4] = W; grid[6][5] = N # Occhio
        grid[8][3] = N # Musetto sporgente
        grid[10][12] = B; grid[9][13] = B; grid[8][13] = B # Coda alzata
    elif stato == "destra":
        grid[6][11] = W; grid[6][10] = N
        grid[8][12] = N
        grid[10][3] = B; grid[9][2] = B; grid[8][2] = B
    elif stato == "su":
        grid[4][6] = B; grid[4][9] = B
        grid[12][7] = B; grid[13][8] = B # Coda vista da dietro
    elif stato == "giu":
        grid[6][5] = W; grid[6][6] = N  # Occhio sin
        grid[6][10] = W; grid[6][9] = N # Occhio des
        grid[8][8] = N                  # Nasino
    elif stato == "sonno":
        grid[6][5] = N; grid[6][6] = N  # Occhi chiusi a fessura
        grid[6][9] = N; grid[6][10] = N
    elif stato == "occhi":
        # Posa "Solo Occhi" per il rientro in cuccia (tutto trasparente tranne le pupille)
        grid[7][5] = W; grid[7][6] = (0,0,255,255)  # Sclera bianca e pupilla blu
        grid[7][9] = W; grid[7][10] = (0,0,255,255)

    flat = []
    for y in range(16):
        for x in range(16): flat.append(grid[y][x])
    return flat

# ==========================================
# 3. MATRICI ELEMENTI FISSI (8x8)
# ==========================================
# Formaggio 8x8 (Spicchio triangolare con ombra)
formaggio_pixels = [
    _, _, _, _, _, _, _, _,
    _, _, Y, Y, Y, Y, _, _,
    _, Y, Y, Y, Y, Y, Y, _,
    _, Y, Y, Y, Y, Y, Y_O, _,
    _, _, Y, Y, Y, Y_O, _, _,
    _, _, _, Y, Y_O, _, _, _,
    _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _
]

# Ciotola di latte 8x8
ciotola_pixels = [
    _, _, _, _, _, _, _, _,
    _, _, W, W, W, W, _, _,
    _, W, L_A, L_A, L_A, L_A, W, _,
    _, W, L_A, L_A, L_A, L_A, W, _,
    _, _, W, W, W, W, _, _,
    _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _
]

# Porta 8x8 (Sbarra di legno orizzontale centrale)
porta_pixels = [
    _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _,
    PORTA, PORTA, PORTA, PORTA, PORTA, PORTA, PORTA, PORTA,
    PORTA, PORTA, PORTA, PORTA, PORTA, PORTA, PORTA, PORTA,
    _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _
]

# Muro Normale 8x8
muro_pixels = [
    N, M, M, M, N, M, M, M,
    M, M, M, M, M, M, M, M,
    M, M, M, M, M, M, M, M,
    N, N, N, N, N, N, N, N,
    M, M, N, M, M, M, N, M,
    M, M, M, M, M, M, M, M,
    M, M, M, M, M, M, M, M,
    S, S, S, S, S, S, S, S
]

# Muro con Tubo Orizzontale 8x8
muro_tubo_pixels = [
    N, M, M, M, N, M, M, M,
    M, M, M, M, M, M, M, M,
    N, N, N, N, N, N, N, N,
    T, T, T, T, T, T, T, T, # Tubo Grigio
    F, F, F, F, F, F, F, F, # Linea di riflesso chiara
    M, M, M, M, M, M, M, M,
    M, M, M, M, M, M, M, M,
    S, S, S, S, S, S, S, S
]

# ==========================================
# 4. SETTE FRUTTI VELOCI (8x8)
# ==========================================
def get_frutto_pixels(col_frutto):
    # Crea una pallina colorata centrata di diametro 4-5 pixel per non coprire i muri
    grid = [[_]*8 for _ in range(8)]
    for y in range(2, 6):
        for x in range(2, 6):
            grid[y][x] = col_frutto
    # Piccola fogliolina verde fissa in alto a destra per dargli l'aspetto di frutto
    grid[1][5] = (34, 177, 76, 255)
    
    flat = []
    for y in range(8):
        for x in range(8): flat.append(grid[y][x])
    return flat

frutti_colori = {
    "rosso":      (255, 0, 0, 255),
    "arancio":    (255, 128, 0, 255),
    "rosso_scuro":(180, 0, 48, 255),
    "verde":      (0, 255, 0, 255),
    "ciano":      (0, 255, 255, 255),
    "giallo":     (255, 255, 0, 255),
    "bianco":     (255, 255, 255, 255)
}

# ==========================================
# ESECUZIONE DELLA GENERAZIONE MASSIVA
# ==========================================
if __name__ == "__main__":
    print("⏳ Generazione degli asset di gioco in corso...")

    # 1. Salva i file del Topo
    for v in ["su", "sinistra", "giu", "destra"]:
        save_png(16, 16, get_topo_sprite(v), f"topo_{v}.png")
    save_png(8, 8, vita_pixels, "vita.png")

    # 2. Salva i file dei Gatti
    for colore in G_COLORI.keys():
        for v in ["su", "sinistra", "giu", "destra"]:
            save_png(16, 16, get_gatto_sprite(colore, v), f"gatto_{colore}_{v}.png")
            
    # Pose speciali gatti (uniche per tutti i tipi)
    save_png(16, 16, get_gatto_sprite("sonno", "sonno"), "gatto_sonno.png")
    save_png(16, 16, get_gatto_sprite("arancio", "occhi"), "gatto_occhi.png") # gli occhi non dipendono dal colore del corpo

    # 3. Salva elementi fissi e muri
    save_png(8, 8, formaggio_pixels, "formaggio.png")
    save_png(8, 8, ciotola_pixels, "ciotola.png")
    save_png(8, 8, porta_pixels, "porta.png")


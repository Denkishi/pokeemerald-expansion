#!/usr/bin/env python3
"""
GBA Sprite Palette Reducer (15 Colori + Trasparenza)
---------------------------------------------------
Converte sprite con artefatti o gradienti indesiderati (es. 100-250 colori)
in uno sprite indicizzato GBA 4bpp (15 colori utili + 1 trasparenza all'indice 0).

Raggruppa i colori simili calcolando la media ponderata (Weighted K-Means con K-Means++),
eliminando artefatti da compressione PNG e preservando la fedeltà dello sprite.
"""

import os
import sys
import argparse
import numpy as np
from PIL import Image, ImageFilter

# Colore di default per la trasparenza (lo standard di pokeemerald è 152, 208, 160)
DEFAULT_TRANSPARENT_COLOR = (152, 208, 160)

COLOR_PRESETS = {
    "pokeemerald": (152, 208, 160),
    "magenta": (255, 0, 255),
    "black": (0, 0, 0),
    "white": (255, 255, 255),
    "green": (0, 255, 0),
}


def parse_hex_color(color_str):
    """Interpreta una stringa di colore esadecimale o nome preset."""
    if not color_str:
        return None
    c = color_str.strip().lower()
    if c in COLOR_PRESETS:
        return COLOR_PRESETS[c]
    if c.startswith("#"):
        c = c[1:]
    if len(c) == 6:
        return (int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16))
    elif len(c) == 3:
        return (int(c[0]*2, 16), int(c[1]*2, 16), int(c[2]*2, 16))
    raise ValueError(f"Formato colore non valido: {color_str}")


def snap_to_gba_5bit(rgb_color):
    """
    Arrotonda un colore 8-bit ai 5-bit del GBA (0..31 per canale)
    e lo riscala a 8-bit, garantendo fedeltà hardware 1:1.
    """
    r, g, b = rgb_color[:3]
    r5 = int(round(r * 31.0 / 255.0))
    g5 = int(round(g * 31.0 / 255.0))
    b5 = int(round(b * 31.0 / 255.0))
    return (
        int(round(r5 * 255.0 / 31.0)),
        int(round(g5 * 255.0 / 31.0)),
        int(round(b5 * 255.0 / 31.0)),
    )


def rgb_to_gba_hex(r, g, b):
    """Converte RGB888 nel formato esadecimale GBA BGR555 (es. 0x7FFF)."""
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    val = (b5 << 10) | (g5 << 5) | r5
    return f"0x{val:04X}"


def rgb_to_luminance(rgb):
    """Calcola la luminanza percepita di un colore RGB."""
    return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]


def weighted_kmeans(colors, weights, k, n_init=10, max_iter=40):
    """
    Esegue K-Means pesato per frequenza dei pixel.
    I centroidi calcolati corrispondono esattamente al colore medio ponderato
    dei pixel assegnati a ciascun cluster, unificando le sfumature simili.
    """
    n_samples = len(colors)
    if n_samples <= k:
        return colors.copy()

    weights_sum = weights.sum()
    normalized_weights = weights / weights_sum

    best_centroids = None
    best_inertia = float("inf")

    for _ in range(n_init):
        # Inizializzazione K-Means++
        first_idx = np.random.choice(n_samples, p=normalized_weights)
        centroids = [colors[first_idx].astype(float)]

        for _ in range(1, k):
            # Minima distanza quadratica da ciascun punto ai centroidi esistenti
            dists = np.min(
                [np.sum((colors - c) ** 2, axis=1) for c in centroids],
                axis=0,
            )
            probs = dists * weights
            total_prob = probs.sum()
            if total_prob > 0:
                probs /= total_prob
            else:
                probs = normalized_weights
            next_idx = np.random.choice(n_samples, p=probs)
            centroids.append(colors[next_idx].astype(float))

        centroids = np.array(centroids, dtype=float)

        # Iterazioni EM
        for _ in range(max_iter):
            # Calcola distanze Euclidee a tutti i centroidi
            diffs = colors[:, np.newaxis, :] - centroids[np.newaxis, :, :]
            dists_sq = np.sum(diffs ** 2, axis=2)
            labels = np.argmin(dists_sq, axis=1)

            new_centroids = np.copy(centroids)
            for i in range(k):
                mask = (labels == i)
                if np.any(mask):
                    new_centroids[i] = np.average(
                        colors[mask],
                        axis=0,
                        weights=weights[mask],
                    )

            if np.allclose(centroids, new_centroids, atol=0.25):
                break
            centroids = new_centroids

        # Calcolo inerzia totale per selezionare il run migliore
        diffs = colors[:, np.newaxis, :] - centroids[np.newaxis, :, :]
        dists_sq = np.sum(diffs ** 2, axis=2)
        min_dists = np.min(dists_sq, axis=1)
        inertia = np.sum(min_dists * weights)

        if inertia < best_inertia:
            best_inertia = inertia
            best_centroids = centroids

    return best_centroids


def parse_size(size_str):
    """Interpreta stringhe come '64x64', '64', '64,64' in una tupla (width, height)."""
    if not size_str:
        return None
    s = size_str.lower().replace(",", "x").replace(" ", "")
    if "x" in s:
        parts = s.split("x")
        return (int(parts[0]), int(parts[1]))
    val = int(s)
    return (val, val)


def resize_pixel_art(img, target_size=(64, 64), mode="auto", keep_aspect=True):
    """
    Ridimensiona uno sprite pixel art con l'algoritmo migliore:
    - 'auto': rileva se le dimensioni sono multipli interi (es. 128->64, 256->64).
      Se sì, usa Nearest Neighbor per purezza 1:1. Altrimenti usa 'smart'.
    - 'smart': usa Lanczos per mantenere la massa e le linee sottili, poi applica
      Unsharp Mask per accentuare i contorni scuri prima della quantizzazione.
    - 'nearest': Nearest Neighbor classico.
    - 'lanczos': Lanczos puro.
    - 'box': Area averaging puro.
    """
    orig_w, orig_h = img.size
    target_w, target_h = target_size

    if (orig_w, orig_h) == (target_w, target_h):
        return img

    if keep_aspect:
        scale = min(target_w / orig_w, target_h / orig_h)
        new_w = max(1, int(round(orig_w * scale)))
        new_h = max(1, int(round(orig_h * scale)))
    else:
        new_w, new_h = target_w, target_h

    # Scelta dell'algoritmo
    chosen_method = mode
    if mode == "auto":
        is_integer_w = (orig_w % new_w == 0) and (orig_w >= new_w)
        is_integer_h = (orig_h % new_h == 0) and (orig_h >= new_h)
        if is_integer_w and is_integer_h and (orig_w // new_w == orig_h // new_h):
            chosen_method = "nearest"
            print(f"Rilevato scaling intero esatto ({orig_w // new_w}x) -> uso Nearest Neighbor puro.")
        else:
            chosen_method = "smart"
            print("Rilevato scaling non intero -> uso Smart Pixel Downscale (Lanczos + Outline Enhancement).")

    # Esegui il ridimensionamento
    img_rgba = img.convert("RGBA")

    if chosen_method == "nearest":
        resized_sprite = img_rgba.resize((new_w, new_h), Image.Resampling.NEAREST)
    elif chosen_method == "smart":
        # Lanczos/Box preserva la silhouette e le linee sottili
        resample_filter = Image.Resampling.LANCZOS if (orig_w / new_w <= 3) else Image.Resampling.BOX
        resized_sprite = img_rgba.resize((new_w, new_h), resample_filter)

        # Unsharp Mask separa i canali RGB per rafforzare il contrasto dei contorni scuri
        r, g, b, a = resized_sprite.split()
        rgb = Image.merge("RGB", (r, g, b))
        # Raggio 1.0, percent 130 per ri-accentuare i bordi neri
        sharpened_rgb = rgb.filter(ImageFilter.UnsharpMask(radius=1.0, percent=130, threshold=2))
        sr, sg, sb = sharpened_rgb.split()
        resized_sprite = Image.merge("RGBA", (sr, sg, sb, a))
    elif chosen_method == "lanczos":
        resized_sprite = img_rgba.resize((new_w, new_h), Image.Resampling.LANCZOS)
    elif chosen_method == "box":
        resized_sprite = img_rgba.resize((new_w, new_h), Image.Resampling.BOX)
    else:
        resized_sprite = img_rgba.resize((new_w, new_h), Image.Resampling.NEAREST)

    # Incolla al centro di un canvas target_w x target_h
    canvas = Image.new("RGBA", (target_w, target_h), (0, 0, 0, 0))
    offset_x = (target_w - new_w) // 2
    offset_y = (target_h - new_h) // 2
    canvas.paste(resized_sprite, (offset_x, offset_y), resized_sprite)

    return canvas


def process_sprite(
    input_path,
    output_path=None,
    num_fg_colors=15,
    bg_mode="auto",
    bg_custom_color=None,
    alpha_threshold=128,
    bg_tolerance=12,
    snap_gba=True,
    sort_palette="luminance",
    save_pal=False,
    resize_target=None,
    resize_mode="auto",
    keep_aspect=True,
):
    """Elabora una singola immagine e salva la versione indicizzata a 16 colori."""
    if not os.path.exists(input_path):
        raise FileNotFoundError(f"File non trovato: {input_path}")

    img = Image.open(input_path)
    orig_w, orig_h = img.size

    if resize_target:
        print(f"Ridimensionamento da {orig_w}x{orig_h} a {resize_target[0]}x{resize_target[1]} (mode: {resize_mode})...")
        img = resize_pixel_art(
            img,
            target_size=resize_target,
            mode=resize_mode,
            keep_aspect=keep_aspect,
        )

    width, height = img.size
    has_alpha = ("A" in img.getbands())

    if has_alpha:
        img_rgba = img.convert("RGBA")
        arr = np.array(img_rgba)
        rgb_data = arr[:, :, :3]
        alpha_data = arr[:, :, 3]
    else:
        img_rgb = img.convert("RGB")
        arr = np.array(img_rgb)
        rgb_data = arr[:, :, :3]
        alpha_data = np.full((height, width), 255, dtype=np.uint8)

    # Identificazione sfondo / trasparenza
    bg_mask = np.zeros((height, width), dtype=bool)
    bg_color = None

    if has_alpha:
        # Se c'è canale alfa, i pixel sotto soglia sono sfondo trasparente
        bg_mask = (alpha_data < alpha_threshold)
        if bg_custom_color:
            bg_color = bg_custom_color
        else:
            bg_color = DEFAULT_TRANSPARENT_COLOR
    else:
        # Immagine senza canale alfa: usa colore specificato o auto-detect
        if bg_custom_color:
            target_bg = np.array(bg_custom_color, dtype=int)
            diff = np.sqrt(np.sum((rgb_data.astype(int) - target_bg) ** 2, axis=2))
            bg_mask = (diff <= bg_tolerance)
            bg_color = bg_custom_color
        elif bg_mode == "auto":
            # Controlla i 4 angoli per rilevare il colore di sfondo
            corners = [
                tuple(rgb_data[0, 0]),
                tuple(rgb_data[0, width - 1]),
                tuple(rgb_data[height - 1, 0]),
                tuple(rgb_data[height - 1, width - 1]),
            ]
            from collections import Counter
            corner_counts = Counter(corners)
            most_common_corner, count = corner_counts.most_common(1)[0]
            if count >= 2:
                target_bg = np.array(most_common_corner, dtype=int)
                diff = np.sqrt(np.sum((rgb_data.astype(int) - target_bg) ** 2, axis=2))
                bg_mask = (diff <= bg_tolerance)
                bg_color = most_common_corner
            else:
                bg_color = DEFAULT_TRANSPARENT_COLOR
        else:
            bg_color = DEFAULT_TRANSPARENT_COLOR

    fg_mask = ~bg_mask
    fg_pixels = rgb_data[fg_mask]

    # Statistiche iniziali
    total_unique_colors = len(np.unique(rgb_data.reshape(-1, 3), axis=0))
    fg_unique_colors, fg_counts = np.unique(fg_pixels, axis=0, return_counts=True)
    num_unique_fg = len(fg_unique_colors)

    print(f"\n========================================================")
    print(f"File: {os.path.basename(input_path)} ({width}x{height})")
    print(f"Colori unici originali: {total_unique_colors} (Foreground: {num_unique_fg})")
    print(f"Pixel sfondo/trasparenza rilevati: {np.sum(bg_mask)} ({np.sum(bg_mask)*100/(width*height):.1f}%)")

    # Quantizzazione K-Means per i colori di primo piano
    k = min(num_fg_colors, max(1, num_unique_fg))

    if num_unique_fg <= k:
        fg_centroids = fg_unique_colors.astype(float)
    else:
        print(f"Raggruppamento K-Means di {num_unique_fg} sfumature in {k} colori medi...")
        fg_centroids = weighted_kmeans(fg_unique_colors, fg_counts, k)

    # Arrotonda e opzionalmente applica GBA 5-bit snap
    fg_centroids_rounded = np.clip(np.round(fg_centroids), 0, 255).astype(int)
    if snap_gba:
        fg_centroids_rounded = np.array([snap_to_gba_5bit(c) for c in fg_centroids_rounded])

    # Rimuovi eventuali duplicati nati dall'arrotondamento
    unique_centroids = []
    for c in fg_centroids_rounded:
        c_tuple = tuple(c)
        if c_tuple not in unique_centroids:
            unique_centroids.append(c_tuple)
    fg_palette_colors = [np.array(c) for c in unique_centroids]

    # Ordinamento palette foreground
    if sort_palette == "luminance":
        fg_palette_colors.sort(key=lambda c: rgb_to_luminance(c))
    elif sort_palette == "frequency":
        # Conta frequenza assegnata a ciascun centroide
        diffs = fg_pixels[:, np.newaxis, :] - np.array(fg_palette_colors)[np.newaxis, :, :]
        dists = np.sum(diffs ** 2, axis=2)
        labels = np.argmin(dists, axis=1)
        counts = [np.sum(labels == i) for i in range(len(fg_palette_colors))]
        fg_palette_colors = [c for _, c in sorted(zip(counts, fg_palette_colors), reverse=True)]

    # Costruzione palette finale da 16 colori (48 bytes)
    # INDICE 0 = Sfondo trasparente!
    if snap_gba and bg_color:
        final_bg_color = snap_to_gba_5bit(bg_color)
    else:
        final_bg_color = bg_color if bg_color else DEFAULT_TRANSPARENT_COLOR

    final_palette = [list(final_bg_color)] + [list(c) for c in fg_palette_colors]

    # Riempi fino a 16 colori se il foreground ha meno di 15 colori
    while len(final_palette) < 16:
        final_palette.append([0, 0, 0])

    final_palette = final_palette[:16]

    # Mappatura dei pixel all'indice della palette
    indexed_data = np.zeros((height, width), dtype=np.uint8)

    if len(fg_palette_colors) > 0:
        fg_palette_arr = np.array(fg_palette_colors)
        # Assegna ogni pixel di foreground al centroide più vicino
        diffs = fg_pixels[:, np.newaxis, :] - fg_palette_arr[np.newaxis, :, :]
        dists = np.sum(diffs ** 2, axis=2)
        fg_labels = np.argmin(dists, axis=1)
        # +1 perché l'indice 0 è lo sfondo!
        indexed_data[fg_mask] = (fg_labels + 1).astype(np.uint8)

    # I pixel in bg_mask rimangono a 0 (trasparenza)
    indexed_data[bg_mask] = 0

    # Creazione immagine PIL indicizzata
    out_img = Image.fromarray(indexed_data, mode="P")
    palette_flat = [val for rgb in final_palette for val in rgb]
    out_img.putpalette(palette_flat)

    # Imposta trasparenza all'indice 0
    if output_path is None:
        base, ext = os.path.splitext(input_path)
        output_path = f"{base}_16c.png"

    out_img.save(output_path, transparency=0)
    print(f"-> Sprite salvato con successo: {output_path}")

    # Salva file .pal (JASC-PAL) se richiesto
    if save_pal:
        pal_path = os.path.splitext(output_path)[0] + ".pal"
        with open(pal_path, "w", encoding="utf-8") as f:
            f.write("JASC-PAL\n0100\n16\n")
            for c in final_palette:
                f.write(f"{c[0]} {c[1]} {c[2]}\n")
        print(f"-> Palette JASC-PAL salvata: {pal_path}")

    # Stampa riassunto palette
    print("\nPalette GBA generata (16 colori):")
    print(f"{'Idx':<4} {'RGB':<18} {'HEX':<8} {'GBA BGR555':<12} {'Pixel':<8} {'Tipo'}")
    print("-" * 62)
    for idx, c in enumerate(final_palette):
        px_count = int(np.sum(indexed_data == idx))
        c_ints = tuple(int(v) for v in c)
        hex_str = f"#{c_ints[0]:02X}{c_ints[1]:02X}{c_ints[2]:02X}"
        gba_hex = rgb_to_gba_hex(*c_ints)
        col_type = "Trasparente (Index 0)" if idx == 0 else f"Colore {idx}"
        print(f"{idx:<4} {str(c_ints):<18} {hex_str:<8} {gba_hex:<12} {px_count:<8} {col_type}")
    print("========================================================\n")

    return output_path


def open_file_dialog():
    """Apre un file picker grafico Tkinter se disponibile."""
    try:
        import tkinter as tk
        from tkinter import filedialog
        root = tk.Tk()
        root.withdraw()
        root.attributes("-topmost", True)
        file_path = filedialog.askopenfilename(
            title="Seleziona lo sprite PNG da convertire (GBA 15+1 colori)",
            filetypes=[("File PNG", "*.png"), ("Tutti i file", "*.*")],
        )
        root.destroy()
        return file_path
    except Exception:
        return None


def main():
    parser = argparse.ArgumentParser(
        description="GBA Sprite Quantizer: Converte sprite PNG con molti colori in 15 colori + trasparenza calcolando il colore medio con K-Means.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Esempi:
  python tools/reduce_sprite.py sprite.png
  python tools/reduce_sprite.py sprite.png -o sprite_clean.png --save-pal
  python tools/reduce_sprite.py sprite.png --bg magenta --colors 15
  python tools/reduce_sprite.py graphics/pokemon/mio_sprite/*.png
        """
    )
    parser.add_argument("inputs", nargs="*", help="File PNG di input (uno o più file / wildcard)")
    parser.add_argument("-o", "--output", help="Percorso del file di output (valido per singolo file)")
    parser.add_argument("-c", "--colors", type=int, default=15, help="Numero di colori utili di primo piano (default: 15)")
    parser.add_argument("--bg", default="auto", help="Colore di sfondo se non c'è trasparenza: 'auto', 'magenta', 'pokeemerald', 'black', 'white', o hex '#RRGGBB'")
    parser.add_argument("--bg-color", help="Colore da assegnare all'indice 0 trasparente (es. '#98D0A0', 'magenta', 'black')")
    parser.add_argument("--alpha-threshold", type=int, default=128, help="Soglia canale alfa per considerare un pixel trasparente (0-255, default: 128)")
    parser.add_argument("--bg-tolerance", type=int, default=12, help="Tolleranza distanza colore per identificare lo sfondo solido (default: 12)")
    parser.add_argument("--no-gba-snap", action="store_true", help="Non arrotondare i colori allo spazio colore GBA 15-bit (BGR555)")
    parser.add_argument("--sort", choices=["luminance", "frequency", "none"], default="luminance", help="Ordinamento palette dei colori utili (default: luminance)")
    parser.add_argument("--save-pal", action="store_true", help="Salva anche il file .pal (JASC-PAL compatibile pokeemerald/Aseprite)")
    parser.add_argument("-r", "--resize", help="Ridimensiona a dimensioni specifiche (es. '64x64' o '64')")
    parser.add_argument("--resize-mode", choices=["auto", "smart", "nearest", "lanczos", "box"], default="auto", help="Algoritmo di ridimensionamento pixel art (default: auto)")
    parser.add_argument("--stretch", action="store_true", help="Disabilita il mantenimento delle proporzioni durante il resize")

    args = parser.parse_args()

    # Risolvi lista di file
    input_files = []
    if args.inputs:
        for pattern in args.inputs:
            import glob
            matched = glob.glob(pattern)
            if matched:
                input_files.extend(matched)
            else:
                input_files.append(pattern)
    else:
        # Se non è stato passato alcun file, prova ad aprire una finestra di dialogo
        print("Nessun file specificato. Apertura selettore file...")
        selected = open_file_dialog()
        if selected:
            input_files.append(selected)
        else:
            print("Nessun file selezionato. Uscita.")
            sys.exit(0)

    # Interpreta colore di sfondo
    bg_custom = None
    if args.bg_color:
        bg_custom = parse_hex_color(args.bg_color)
    elif args.bg not in ["auto", "none"]:
        bg_custom = parse_hex_color(args.bg)

    resize_dim = parse_size(args.resize) if args.resize else None

    for in_file in input_files:
        out_file = args.output if (len(input_files) == 1 and args.output) else None
        try:
            process_sprite(
                input_path=in_file,
                output_path=out_file,
                num_fg_colors=args.colors,
                bg_mode=args.bg,
                bg_custom_color=bg_custom,
                alpha_threshold=args.alpha_threshold,
                bg_tolerance=args.bg_tolerance,
                snap_gba=not args.no_gba_snap,
                sort_palette=args.sort,
                save_pal=args.save_pal,
                resize_target=resize_dim,
                resize_mode=args.resize_mode,
                keep_aspect=not args.stretch,
            )
        except Exception as e:
            print(f"Errore durante l'elaborazione di {in_file}: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()

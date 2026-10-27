#!/usr/bin/env python3
"""Libro dei piani (M9): dalle partite del self-play (tools/vgc_training/work/results/*.json) le coppie di lead
che ogni allenatore VGC ha usato, con vittorie e partite, scritte in src/data/vgc_plan_book.h.

Il ROM (team preview, AI_VgcPlanBookLeadBonus) usa il libro come strategia mista: una lead che ha vinto
di piu prende un bonus, e un piccolo rumore casuale lascia spazio all'esplorazione.

Uso:  python tools/vgc_training/gen_plan_book.py   (dopo una o piu campagne)
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RESULTS = ROOT / 'tools' / 'vgc_training' / 'work' / 'results'
TRAINERS = ROOT / 'tools' / 'vgc_training' / 'work' / 'allenatori.json'
OUT = ROOT / 'src' / 'data' / 'vgc_plan_book.h'
MAX_ENTRIES = 255


def side_won(game, side):
    return (game.get('esito') == 'VITTORIA_G' and side == 'G') or (game.get('esito') == 'VITTORIA_A' and side == 'A')


def lead_slots(game, side):
    tp = game.get('tp', {}).get(side) or {}
    names = tp.get('lead_scelta') or []
    sets = (game.get('set') or {}).get(side) or {}
    slots = []
    for name in names:
        for slot, s in sets.items():
            if s.get('specie') == name and int(slot) not in slots:
                slots.append(int(slot))
                break
    return tuple(sorted(slots)) if len(slots) == 2 else None


def vgc_trainer_constants():
    """Costanti TRAINER_* degli allenatori con 'Vgc Doubles' nella riga AI, nell'ordine di trainers.party
    (lo stesso ordine dell'elenco del self-play)."""
    consts, cur = [], None
    for line in (ROOT / 'src' / 'data' / 'trainers.party').read_text(encoding='utf-8').splitlines():
        if line.startswith('=== ') and line.endswith(' ==='):
            cur = line[4:-4].strip()
        elif line.startswith('AI:') and 'Vgc Doubles' in line and cur:
            consts.append(cur)
    return consts


def main():
    consts = vgc_trainer_constants()
    trainer_id = {i: c for i, c in enumerate(consts)}
    stats = {}
    for f in sorted(RESULTS.glob('*.json')):
        g = json.loads(f.read_text(encoding='utf-8'))
        if g.get('esito') not in ('VITTORIA_G', 'VITTORIA_A'):
            continue
        for side, idx in (('G', g.get('giocatore')), ('A', g.get('avversario'))):
            slots = lead_slots(g, side)
            tid = trainer_id.get(idx)
            if slots is None or tid is None:
                continue
            key = (tid, slots[0], slots[1])
            w, n = stats.get(key, (0, 0))
            stats[key] = (w + (1 if side_won(g, side) else 0), n + 1)
    rows = sorted(stats.items())[:MAX_ENTRIES]
    out = ['// Generato da tools/vgc_training/gen_plan_book.py dalle partite del self-play: non modificare a mano.',
           '// Per allenatore (id della tabella), coppia di lead (indici di squadra), vittorie e partite.',
           '#ifndef GUARD_DATA_VGC_PLAN_BOOK_H', '#define GUARD_DATA_VGC_PLAN_BOOK_H', '',
           'static const struct VgcPlanBookLead sVgcPlanBook[] =', '{']
    for (tid, a, b), (w, n) in rows:
        out.append(f'    {{ .trainerId = {tid}, .slotA = {a}, .slotB = {b}, .wins = {w}, .games = {n} }},')
    out += ['};', '', '#endif // GUARD_DATA_VGC_PLAN_BOOK_H', '']
    OUT.write_text('\n'.join(out), encoding='utf-8')
    print(f'{OUT}: {len(rows)} coppie di lead da {sum(n for _, n in stats.values())} lead osservate')


if __name__ == '__main__':
    main()

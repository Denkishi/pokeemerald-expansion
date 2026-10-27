#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Libro dei piani (M9): dalle partite del self-play (di solito l'esplorazione, work/dati/esplorazione/*.json) i piani
di gioco che hanno vinto di più, scritti in src/data/vgc_plan_book.h (const, in ROM).

Chiave generica, calcolata dal ROM e scritta nel log (riga "VSP CHIAVE"): hash della composizione della NOSTRA
squadra (specie) e classe della squadra avversaria vista in team preview (velocità base delle specie). Nessun nome
di allenatore. Per ogni chiave e coppia di lead: vittorie e partite, la retro migliore con quelle lead e le azioni
congiunte dei turni 1 e 2 che hanno vinto di più. Il ROM (src/battle_ai_vgc_book.c) sceglie a caso tra i piani buoni
(strategia mista) e dà un bonus alle loro scelte in team preview e ai turni 1/2.

Uso:  python tools/vgc_training/gen_plan_book.py [--dirs esplorazione,altro] [--min-games 3]
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / 'tools' / 'vgc_training' / 'work' / 'dati'
OUT = ROOT / 'src' / 'data' / 'vgc_plan_book.h'
SUMMARY = ROOT / 'tools' / 'vgc_training' / 'work' / 'libro.json'
ANY = 255
MAX_ENTRIES = 600
MAX_PER_KEY = 6


def side_score(game, side):
    e = game.get('esito')
    if e in ('PAREGGIO', 'LIMITE_TURNI'):
        return 0.5
    if e == 'VITTORIA_G':
        return 1.0 if side == 'G' else 0.0
    if e == 'VITTORIA_A':
        return 1.0 if side == 'A' else 0.0
    return None


def rate(w, n):
    return (w + 1) / (n + 2)


class Stat:
    __slots__ = ('w', 'n')

    def __init__(self):
        self.w = 0.0
        self.n = 0

    def add(self, s):
        self.w += s
        self.n += 1


def observe(game, side):
    """(squadra, classe, lead, retro, mosse turno 1, mosse turno 2, punteggio) per un lato, o None."""
    sc = side_score(game, side)
    key = (game.get('chiave') or {}).get(side)
    tp = (game.get('tp') or {}).get(side) or {}
    porta = tp.get('porta')
    if sc is None or not key or not porta:
        return None
    species = key['specie']
    slots = [s for s in porta if 0 <= s < len(species)]
    if len(slots) < 2:
        return None
    lead = tuple(sorted(species[s] for s in slots[:2]))
    back = tuple(sorted(species[s] for s in slots[2:4]))
    battlers = (0, 2) if side == 'G' else (1, 3)
    moves = {1: {}, 2: {}}
    for turn, b, pidx, move in game.get('scelte') or []:
        if b in battlers and turn in moves and pidx < len(slots):
            moves[turn][species[slots[pidx]]] = move
    t1 = tuple(moves[1].get(s, 0) for s in lead)
    t2 = tuple(moves[2].get(s, 0) for s in lead)
    return key['squadra'], key['classe'], lead, back, t1, t2, sc


def best_joint(table, base_rate, min_games):
    """Azione congiunta con il tasso stimato migliore (almeno min_games partite e non peggiore del piano)."""
    best, best_r = (0, 0), None
    for act, st in table.items():
        if st.n < min_games:
            continue
        r = rate(st.w, st.n)
        if r + 1e-9 >= base_rate - 0.02 and (best_r is None or r > best_r):
            best, best_r = act, r
    return best


def main():
    ap = argparse.ArgumentParser(description='Genera src/data/vgc_plan_book.h dal self-play')
    ap.add_argument('--dirs', default='esplorazione', help='cartelle di work/dati da leggere (separate da virgola)')
    ap.add_argument('--min-games', type=int, default=3, help='partite minime per un piano o un\'azione congiunta')
    args = ap.parse_args()

    plans = {}
    names, move_names = {}, {}
    files = 0
    for d in args.dirs.split(','):
        for f in sorted((DATA / d.strip()).glob('*.json')):
            g = json.loads(f.read_text(encoding='utf-8'))
            files += 1
            for mid, mname in (g.get('mosse_id') or {}).items():
                move_names[int(mid)] = mname
            for side in ('G', 'A'):
                key = (g.get('chiave') or {}).get(side)
                if key:
                    for slot, s in (g.get('set') or {}).get(side, {}).items():
                        if int(slot) < len(key['specie']):
                            names[key['specie'][int(slot)]] = s.get('specie')
                obs = observe(g, side)
                if obs is None:
                    continue
                team, cls, lead, back, t1, t2, sc = obs
                for c in (cls, ANY):
                    p = plans.setdefault((team, c, lead), {'st': Stat(), 'back': {}, 't1': {}, 't2': {}})
                    p['st'].add(sc)
                    p['back'].setdefault(back, Stat()).add(sc)
                    if 0 not in t1:
                        p['t1'].setdefault(t1, Stat()).add(sc)
                    if 0 not in t2:
                        p['t2'].setdefault(t2, Stat()).add(sc)

    by_key = {}
    for (team, cls, lead), p in plans.items():
        if p['st'].n < args.min_games:
            continue
        by_key.setdefault((team, cls), []).append((lead, p))
    rows = []
    for (team, cls), lst in sorted(by_key.items()):
        lst.sort(key=lambda lp: -rate(lp[1]['st'].w, lp[1]['st'].n))
        for lead, p in lst[:MAX_PER_KEY]:
            st = p['st']
            r = rate(st.w, st.n)
            backs = sorted(p['back'].items(), key=lambda kv: (-(rate(kv[1].w, kv[1].n) if kv[1].n >= 2 else 0), -kv[1].n))
            back = backs[0][0] if backs else ()
            back = tuple(back) + (0,) * (2 - len(back))
            t1 = best_joint(p['t1'], r, args.min_games)
            t2 = best_joint(p['t2'], r, args.min_games)
            rows.append({'team': team, 'cls': cls, 'lead': lead, 'back': back, 't1': t1, 't2': t2,
                         'wins': int(round(st.w)), 'games': st.n, 'rate': round(r, 3)})
    rows = rows[:MAX_ENTRIES]

    def sp(s):
        return names.get(s, str(s))

    out = ['// Generato da tools/vgc_training/gen_plan_book.py dalle partite del self-play: non modificare a mano.',
           f'// {len(rows)} piani da {files} partite (cartelle: {args.dirs}). Chiave: hash della nostra squadra e classe',
           '// dell\'avversario (VgcBook_TeamHash / VgcBook_OppClass, 255 = qualsiasi); specie e mosse come ID numerici.',
           '#ifndef GUARD_DATA_VGC_PLAN_BOOK_H', '#define GUARD_DATA_VGC_PLAN_BOOK_H', '',
           'static const struct VgcBookPlan sVgcPlanBook[] =', '{']
    for r in rows:
        out.append(f"    {{ .teamHash = {r['team']}, .oppClass = {r['cls']}, .lead = {{ {r['lead'][0]}, {r['lead'][1]} }}, "
                   f".back = {{ {r['back'][0]}, {r['back'][1]} }}, .t1Move = {{ {r['t1'][0]}, {r['t1'][1]} }}, "
                   f".t2Move = {{ {r['t2'][0]}, {r['t2'][1]} }}, .wins = {r['wins']}, .games = {r['games']} }}, "
                   f"// {sp(r['lead'][0])} + {sp(r['lead'][1])} / {sp(r['back'][0])}, {sp(r['back'][1])}; "
                   f"t1 {move_names.get(r['t1'][0], '-')} + {move_names.get(r['t1'][1], '-')}; "
                   f"t2 {move_names.get(r['t2'][0], '-')} + {move_names.get(r['t2'][1], '-')}")
    if not rows:
        out.append('    { .teamHash = 0, .oppClass = 255, .games = 0 }, // riga vuota (mai giocabile)')
    out += ['};', '', '#endif // GUARD_DATA_VGC_PLAN_BOOK_H', '']
    OUT.write_text('\n'.join(out), encoding='utf-8', newline='\n')
    SUMMARY.write_text(json.dumps({'partite': files, 'piani': len(rows), 'righe': rows}, ensure_ascii=False, indent=1),
                       encoding='utf-8')
    keys = {(r['team'], r['cls']) for r in rows}
    print(f'{OUT}: {len(rows)} piani per {len(keys)} chiavi (squadra, classe) da {files} partite')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Genera src/data/vgc_meta.h (base di conoscenza VGC in ROM per l'IA, milestone M1).

Fonte: statistiche d'uso pubbliche di Smogon / Pokemon Showdown (file "chaos" JSON,
https://www.smogon.com/stats/<mese>/chaos/<formato>-<cutoff>.json).

Per ogni specie (forma esatta: Rotom-Wash != Rotom-Mow; le Mega confluiscono nella forma base):
  - 2-3 spread più usate (natura + EV, raggruppate per "forma" dell'investimento) con frequenza;
  - mosse più usate con frequenza (fino a 10);
  - strumenti più usati con frequenza (fino a 4; la Megapietra è a parte, in megaP);
  - abilità con frequenza (della forma base, prima della Mega);
  - Mega possibili (fino a 2) con frequenza;
  - uso della specie nel formato (% di squadre).

Specie incluse: tutte quelle delle squadre VGC di src/data/trainers.party, le più usate del formato
principale e le più usate dei formati di riserva (per i Pokémon che il giocatore porta più spesso).
Per ogni specie si prende il primo formato della catena che ha abbastanza dati.

Uso (Windows o Linux, Python 3.8+):
    python tools/vgc_meta/gen_vgc_meta.py                 # ultimo mese disponibile
    python tools/vgc_meta/gen_vgc_meta.py --month 2026-09 # mese fisso
    python tools/vgc_meta/gen_vgc_meta.py --offline       # solo file già in cache
I download restano in tools/vgc_meta/cache/ (ignorata da git).
"""
import argparse
import json
import os
import re
import sys
import unicodedata
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'cache')
OUT = os.path.join(ROOT, 'src', 'data', 'vgc_meta.h')
BASE_URL = 'https://www.smogon.com/stats/'

# Catena dei formati: (formato, cutoff). Il primo con abbastanza dati vince.
# Champions (Reg M-A/B/C) ha le Mega come questo gioco; poi Reg G/doppie per chi in Champions non c'è.
FORMAT_CHAIN = [
    ('gen9championsvgc2026regmc', 1630),
    ('gen9championsvgc2026regmc', 0),
    ('gen9championsvgc2026regmb', 1630),
    ('gen9championsvgc2026regmb', 0),
    ('gen9vgc2024regg', 0),
    ('gen9nationaldexdoubles', 0),
    ('gen9doublesou', 0),
]
# Quante specie "probabili per il giocatore" prendere da ciascun formato (oltre a quelle delle squadre VGC).
TOP_PER_FORMAT = {
    'gen9championsvgc2026regmc': 130,
    'gen9championsvgc2026regmb': 40,
    'gen9vgc2024regg': 30,
    'gen9nationaldexdoubles': 20,
    'gen9doublesou': 20,
}
# Champions limita i movepool (es. niente Fake Out per Incineroar), questo gioco no: le mosse si
# mescolano con quelle del formato VGC di Scarlatto/Violetto, a peso ridotto.
MOVE_BLEND_FORMAT = ('gen9vgc2024regg', 0)
MOVE_BLEND_WEIGHT = 0.5
MIN_RAW = 150        # battaglie minime perché i dati di una specie in un formato valgano
MAX_MOVES = 10
MAX_ITEMS = 4
MAX_ABILITIES = 3
MAX_SPREADS = 3
MAX_MEGAS = 2


def log(*a):
    print(*a, file=sys.stderr)


# ---------------------------------------------------------------- costanti del gioco
def parse_enum(path, prefix):
    """NOME -> valore numerico per le costanti PREFIX_* di un file di enum/define.
    Segue la semantica degli enum C: valore esplicito, alias di un'altra costante, oppure precedente + 1."""
    syms = {}
    last = -1
    text = open(path, encoding='utf-8').read()
    for m in re.finditer(r'^\s*#define\s+([A-Z][A-Z0-9_]*)\s+(\d+)', text, re.M):
        syms[m.group(1)] = int(m.group(2))
    for m in re.finditer(r'^\s*(?:#define\s+)?([A-Z][A-Z0-9_]*)\s*(?:=\s*([A-Za-z0-9_]+)\s*)?(,|$|//)', text, re.M):
        name, v = m.group(1), m.group(2)
        if v is None:
            if m.group(0).lstrip().startswith('#define'):
                continue
            val = last + 1
        elif v.isdigit():
            val = int(v)
        elif v.startswith('0x'):
            val = int(v, 16)
        elif v in syms:
            val = syms[v]
        else:
            continue
        syms[name] = val
        last = val
    return {k: v for k, v in syms.items() if k.startswith(prefix)}


def norm_id(s):
    s = unicodedata.normalize('NFKD', s).encode('ascii', 'ignore').decode()
    return re.sub(r'[^a-z0-9]', '', s.lower())


def const_name(showdown_name):
    s = unicodedata.normalize('NFKD', showdown_name).encode('ascii', 'ignore').decode()
    s = s.replace('%', '').replace("'", '').replace('.', '').replace(':', '')
    s = re.sub(r'[\s\-]+', '_', s.strip()).upper()
    return s


class Consts:
    def __init__(self):
        inc = os.path.join(ROOT, 'include', 'constants')
        self.species = parse_enum(os.path.join(inc, 'species.h'), 'SPECIES_')
        self.moves = parse_enum(os.path.join(inc, 'moves.h'), 'MOVE_')
        self.items = parse_enum(os.path.join(inc, 'items.h'), 'ITEM_')
        self.abilities = parse_enum(os.path.join(inc, 'abilities.h'), 'ABILITY_')
        self.natures = parse_enum(os.path.join(inc, 'pokemon.h'), 'NATURE_')
        self.move_by_id = self._by_id(self.moves, 'MOVE_')
        self.item_by_id = self._by_id(self.items, 'ITEM_')
        self.ability_by_id = self._by_id(self.abilities, 'ABILITY_')

    @staticmethod
    def _by_id(table, prefix):
        out = {}
        # i nomi "veri" prima degli alias: a parità di valore vince il primo nome visto
        for name in table:
            key = norm_id(name[len(prefix):])
            out.setdefault(key, name)
        return out

    def species_const(self, name):
        if name.startswith('Floette-Mega'):
            return 'SPECIES_FLOETTE_MEGA'
        base = 'SPECIES_' + const_name(name)
        for cand in (base, base + '_M', base + '_INCARNATE', base + '_RIDER', base + '_SINGLE_STRIKE',
                     base + '_MIDDAY', base + '_NORMAL', base + '_ORDINARY', base + '_ARIA', base + '_50'):
            if cand in self.species:
                return cand
        return None


MEGA_BASE_FIX = {'Floette': 'Floette-Eternal'}


def mega_base(name):
    """'Charizard-Mega-Y' -> 'Charizard'; None se non è una Mega."""
    m = re.match(r'^(.*?)-Mega(?:-[XYZ])?$', name)
    if not m:
        return None
    return MEGA_BASE_FIX.get(m.group(1), m.group(1))


# ---------------------------------------------------------------- download
def fetch(url, dest, offline):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return dest
    if offline:
        return None
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    log('scarico', url)
    try:
        with urllib.request.urlopen(url, timeout=120) as r:
            data = r.read()
    except Exception as e:  # noqa: BLE001
        log('  impossibile:', e)
        return None
    with open(dest, 'wb') as f:
        f.write(data)
    return dest


def latest_month(offline):
    if offline:
        months = sorted(d for d in os.listdir(CACHE) if re.match(r'\d{4}-\d{2}$', d)) if os.path.isdir(CACHE) else []
        return months[-1] if months else None
    html = urllib.request.urlopen(BASE_URL, timeout=60).read().decode('utf-8', 'replace')
    months = sorted(set(re.findall(r'href="(\d{4}-\d{2})/"', html)))
    return months[-1] if months else None


def load_chaos(month, fmt, cutoff, offline):
    dest = os.path.join(CACHE, month, f'{fmt}-{cutoff}.json')
    path = fetch(f'{BASE_URL}{month}/chaos/{fmt}-{cutoff}.json', dest, offline)
    if path is None:
        return None
    with open(path, encoding='utf-8') as f:
        return json.load(f)


# ---------------------------------------------------------------- squadre VGC del gioco
def trainer_vgc_species(consts):
    lines = open(os.path.join(ROOT, 'src', 'data', 'trainers.party'), encoding='utf-8').read().split('\n')
    blocks, cur = [], []
    for l in lines:
        if l.startswith('=== '):
            if cur:
                blocks.append(cur)
            cur = [l]
        else:
            cur.append(l)
    blocks.append(cur)
    out = []
    for b in blocks:
        if not any(l.startswith('AI:') and 'Vgc Doubles' in l for l in b):
            continue
        for i, l in enumerate(b):
            if i == 0 or b[i - 1].strip() != '' or not l.strip() or re.match(r'^[A-Z][A-Za-z ]*:', l):
                continue
            name = l.split('@')[0].strip()
            name = re.sub(r'\s*\((M|F)\)\s*$', '', name)
            m = re.match(r'^(.*?)\s*\((.*)\)$', name)  # "Soprannome (Specie)"
            if m:
                name = m.group(2)
            c = consts.species_const(name)
            if c and c not in out:
                out.append(c)
            elif not c:
                log('specie dei trainer non trovata:', name)
    return out


# ---------------------------------------------------------------- aggregazione
def sp_to_ev(v, champions):
    if not champions:
        return min(252, v)
    return 0 if v <= 0 else min(252, v * 8 - 4)   # Champions: punti statistica 0..32 (1 punto ~ 8 EV)


# Nature che abbassano / alzano la Velocità.
NATURE_SPEED = {n: (n in ('brave', 'relaxed', 'quiet', 'sassy'), n in ('timid', 'hasty', 'jolly', 'naive'))
                for n in ('hardy', 'lonely', 'brave', 'adamant', 'naughty', 'bold', 'docile', 'relaxed', 'impish', 'lax',
                          'timid', 'hasty', 'serious', 'jolly', 'naive', 'modest', 'mild', 'quiet', 'bashful', 'rash',
                          'calm', 'gentle', 'sassy', 'careful', 'quirky')}


def ev_bucket(ev):
    return 0 if ev < 64 else (1 if ev < 188 else 2)


def speed_setters(fmt_data):
    """Per formato: specie (forma base, Mega comprese) -> quota dei set con Trick Room / Tailwind."""
    cache = fmt_data.get('_setters')
    if cache is not None:
        return cache
    tr, tw = {}, {}
    for n, x in fmt_data['data'].items():
        w = sum(x['Abilities'].values())
        if w <= 0:
            continue
        base = mega_base(n) or n
        mv = x['Moves']
        tr[base] = max(tr.get(base, 0), mv.get('trickroom', 0) / w)
        tw[base] = max(tw.get(base, 0), mv.get('tailwind', 0) / w)
    fmt_data['_setters'] = (tr, tw)
    return fmt_data['_setters']


def build_entry(fmt_data, champions, base_name, consts):
    """Unisce la forma base e le sue Mega di un formato. None se non ci sono dati sufficienti."""
    data = fmt_data['data']
    forms = [base_name] if base_name in data else []
    for n in data:
        if mega_base(n) == base_name:
            forms.append(n)
    if not forms:
        return None
    raw = sum(data[f].get('Raw count', 0) for f in forms)
    if raw < MIN_RAW:
        return None

    total = 0.0
    moves, items, abil, spreads, megas = {}, {}, {}, {}, {}
    base_w = 0.0
    usage = 0.0
    for f in forms:
        x = data[f]
        w = sum(x['Abilities'].values())
        if w <= 0:
            continue
        total += w
        usage += x.get('usage', 0)
        for k, v in x['Moves'].items():
            moves[k] = moves.get(k, 0) + v
        for k, v in x['Spreads'].items():
            spreads[k] = spreads.get(k, 0) + v
        if mega_base(f):
            megas[f] = megas.get(f, 0) + w
        else:
            base_w += w
            for k, v in x['Items'].items():
                items[k] = items.get(k, 0) + v
            for k, v in x['Abilities'].items():
                abil[k] = abil.get(k, 0) + v
    if total <= 0:
        return None

    e = {'usage': min(100, round(usage * 100)), 'moves': [], 'items': [], 'abilities': [], 'megas': [], 'spreads': []}
    for k, v in sorted(moves.items(), key=lambda t: -t[1]):
        c = consts.move_by_id.get(norm_id(k))
        if not k or c is None or c == 'MOVE_NONE':
            continue
        p = round(100 * v / total)
        if p < 2:
            break
        e['moves'].append((c, min(100, p)))
        if len(e['moves']) >= MAX_MOVES:
            break
    for k, v in sorted(items.items(), key=lambda t: -t[1]):
        c = consts.item_by_id.get(norm_id(k))
        if not k or k == 'nothing' or c is None:
            continue
        p = round(100 * v / total)
        if p < 2:
            break
        e['items'].append((c, p))
        if len(e['items']) >= MAX_ITEMS:
            break
    # Abilità della forma base (prima della Mega): servono dati sulla forma base.
    abil_total = sum(abil.values())
    if abil_total > 0 and base_w >= max(1.0, 0.01 * total):
        for k, v in sorted(abil.items(), key=lambda t: -t[1]):
            c = consts.ability_by_id.get(norm_id(k))
            if c is None:
                continue
            p = round(100 * v / abil_total)
            if p < 1:
                break
            e['abilities'].append((c, p))
            if len(e['abilities']) >= MAX_ABILITIES:
                break
    for k, v in sorted(megas.items(), key=lambda t: -t[1])[:MAX_MEGAS]:
        c = consts.species_const(k)
        if c:
            e['megas'].append((c, max(1, round(100 * v / total))))
    # Spread raggruppate: stessa natura e stesso "profilo" d'investimento (0 / parziale / massimo per stat).
    clusters = {}
    for k, v in spreads.items():
        try:
            nature, evs = k.split(':')
            evs = [sp_to_ev(int(x), champions) for x in evs.split('/')]
        except ValueError:
            continue
        nc = 'NATURE_' + nature.upper()
        if nc not in consts.natures or len(evs) != 6:
            continue
        key = (nc, tuple(ev_bucket(x) for x in evs))
        cl = clusters.setdefault(key, {'w': 0.0, 'best': None, 'bestw': -1})
        cl['w'] += v
        if v > cl['bestw']:
            cl['best'], cl['bestw'] = (nc, evs), v
    for key, cl in sorted(clusters.items(), key=lambda t: -t[1]['w'])[:MAX_SPREADS]:
        p = round(100 * cl['w'] / total)
        if p < 1:
            break
        e['spreads'].append((cl['best'][0], cl['best'][1], p))
    # Contesto di squadra (team preview): spread "da Trick Room" (natura -Vel, 0 EV in Velocità) e "veloce" (natura +Vel,
    # Velocità al massimo) più usate, con la loro quota sul totale dei set; dai compagni di squadra ("Teammates"),
    # quanto spesso la specie gioca con un setter di Trick Room / Tailwind (somma di P(compagno) x P(compagno ha la mossa)).
    trs, fast = {}, {}
    for k, v in spreads.items():
        try:
            nature, evs = k.split(':')
            evs = [sp_to_ev(int(x), champions) for x in evs.split('/')]
        except ValueError:
            continue
        nc = 'NATURE_' + nature.upper()
        if nc not in consts.natures or len(evs) != 6:
            continue
        down, up = NATURE_SPEED.get(nature.lower(), (False, False))
        if down and evs[5] < 64:
            trs[(nc, tuple(evs))] = trs.get((nc, tuple(evs)), 0) + v
        if up and evs[5] >= 188:
            fast[(nc, tuple(evs))] = fast.get((nc, tuple(evs)), 0) + v
    for name, table in (('trSpread', trs), ('fastSpread', fast)):
        if table:
            best = max(table.items(), key=lambda t: t[1])
            share = round(100 * sum(table.values()) / total)
            if share >= 1:
                e[name] = (best[0][0], list(best[0][1]), min(100, share))
    tr_set, tw_set = speed_setters(fmt_data)
    mates = {}
    for f in forms:
        for k, v in data[f].get('Teammates', {}).items():
            b = mega_base(k) or k
            mates[b] = mates.get(b, 0) + v
    trm = sum(min(1.0, v / total) * tr_set.get(k, 0) for k, v in mates.items() if k != base_name)
    twm = sum(min(1.0, v / total) * tw_set.get(k, 0) for k, v in mates.items() if k != base_name)
    e['trMate'] = min(100, round(100 * trm))
    e['twMate'] = min(100, round(100 * twm))
    e['raw'] = raw
    return e


def blend_moves(e, c, chain, consts):
    for fmt, cutoff, d, champions in chain:
        if (fmt, cutoff) != MOVE_BLEND_FORMAT:
            continue
        for n in d['data']:
            if mega_base(n) or consts.species_const(n) != c:
                continue
            e2 = build_entry(d, champions, n, consts)
            if e2 is None:
                continue
            moves = dict(e['moves'])
            for m, p in e2['moves']:
                moves[m] = max(moves.get(m, 0), round(p * MOVE_BLEND_WEIGHT))
            e['moves'] = sorted(moves.items(), key=lambda t: -t[1])[:MAX_MOVES]
            return


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--month')
    ap.add_argument('--offline', action='store_true')
    ap.add_argument('--out', default=OUT)
    args = ap.parse_args()

    consts = Consts()
    month = args.month or latest_month(args.offline)
    if not month:
        sys.exit('nessun mese di statistiche disponibile')
    log('mese:', month)

    chain = []
    for fmt, cutoff in FORMAT_CHAIN:
        d = load_chaos(month, fmt, cutoff, args.offline)
        if d is None:
            log('formato saltato:', fmt, cutoff)
            continue
        champions = 'champions' in fmt
        chain.append((fmt, cutoff, d, champions))
    if not chain:
        sys.exit('nessun dato scaricato: impossibile generare la tabella')

    # Specie: squadre VGC del gioco + le più usate di ogni formato.
    wanted = trainer_vgc_species(consts)
    n_trainer = len(wanted)
    seen_fmt = set()
    for fmt, cutoff, d, _ in chain:
        if fmt in seen_fmt:
            continue
        seen_fmt.add(fmt)
        top = sorted(d['data'].items(), key=lambda t: -t[1].get('usage', 0))
        added = 0
        for name, _x in top:
            if added >= TOP_PER_FORMAT.get(fmt, 0):
                break
            base = mega_base(name) or name
            c = consts.species_const(base)
            if c is None:
                log('specie non trovata nel gioco:', name)
                continue
            if c not in wanted:
                wanted.append(c)
                added += 1

    # Nome Showdown della forma base per ogni costante.
    entries = []
    missing = []
    for c in wanted:
        found = None
        for fmt, cutoff, d, champions in chain:
            names = [n for n in d['data'] if not mega_base(n) and consts.species_const(n) == c]
            names += sorted({mega_base(n) for n in d['data'] if mega_base(n) and consts.species_const(mega_base(n)) == c})
            for n in names:
                e = build_entry(d, champions, n, consts)
                if e is not None:
                    found = (fmt, cutoff, e)
                    break
            if found:
                break
        if found and 'champions' in found[0]:
            blend_moves(found[2], c, chain, consts)
        if found:
            fmt, cutoff, e = found
            e['species'] = c
            e['source'] = f'{fmt}-{cutoff}'
            entries.append(e)
        else:
            missing.append(c)

    entries.sort(key=lambda e: consts.species[e['species']])
    # Doppioni di valore (alias): tieni il primo.
    uniq, vals = [], set()
    for e in entries:
        v = consts.species[e['species']]
        if v not in vals:
            vals.add(v)
            uniq.append(e)
    entries = uniq

    write_header(args.out, month, chain, entries, missing, n_trainer)
    log(f'scritte {len(entries)} specie ({n_trainer} dalle squadre VGC); senza dati: {len(missing)}')
    if missing:
        log('  senza dati:', ' '.join(m[len("SPECIES_"):] for m in missing))


def write_header(path, month, chain, entries, missing, n_trainer):
    L = []
    w = L.append
    w('// GENERATO da tools/vgc_meta/gen_vgc_meta.py: non modificare a mano (rigenerare, vedi')
    w('// docs/ia_vgc/piano_implementazione.md, M1).')
    w(f'// Fonte: statistiche d\'uso Smogon/Showdown, mese {month}, formati (in ordine di preferenza):')
    for fmt, cutoff, _d, _c in chain:
        w(f'//   {fmt}-{cutoff}')
    w(f'// Specie: {len(entries)} (squadre VGC del gioco: {n_trainer}, più le più usate dei formati).')
    if missing:
        w('// Senza dati sufficienti (restano alla stima da learnset): ' + ', '.join(m[len("SPECIES_"):] for m in missing))
    w('// Probabilità in percentuale (0..100). EV in unità di 4 (0..63). Ordine EV: PS/Att/Dif/AttSp/DifSp/Vel.')
    w('// Tabella ordinata per numero di specie (ricerca binaria in src/battle_ai_scout.c).')
    w('')
    w('#define VGC_META_MOVES     %d' % MAX_MOVES)
    w('#define VGC_META_ITEMS     %d' % MAX_ITEMS)
    w('#define VGC_META_ABILITIES %d' % MAX_ABILITIES)
    w('#define VGC_META_SPREADS   %d' % MAX_SPREADS)
    w('#define VGC_META_MEGAS     %d' % MAX_MEGAS)
    w('')
    w('struct VgcMetaSpread')
    w('{')
    w('    u8 nature;')
    w('    u8 p;')
    w('    u8 ev[6];   // EV / 4, ordine Showdown: PS, Att, Dif, AttSp, DifSp, Vel')
    w('};')
    w('')
    w('struct VgcMetaEntry')
    w('{')
    w('    u16 species;')
    w('    u8 usage;                               // % di squadre del formato che la portano')
    w('    u8 source;                              // indice in sVgcMetaSources')
    w('    u16 moves[VGC_META_MOVES];')
    w('    u8 moveP[VGC_META_MOVES];')
    w('    u16 items[VGC_META_ITEMS];              // senza Megapietre (vedi megaP)')
    w('    u8 itemP[VGC_META_ITEMS];')
    w('    u16 abilities[VGC_META_ABILITIES];      // della forma base (prima della Mega)')
    w('    u8 abilityP[VGC_META_ABILITIES];')
    w('    u16 megas[VGC_META_MEGAS];')
    w('    u8 megaP[VGC_META_MEGAS];')
    w('    struct VgcMetaSpread spreads[VGC_META_SPREADS];')
    w('    struct VgcMetaSpread trSpread;          // spread da Trick Room più usata (p = quota di tutte le spread -Vel/0 EV)')
    w('    struct VgcMetaSpread fastSpread;        // spread veloce più usata (p = quota di tutte le spread +Vel/max Vel)')
    w('    u8 trMate;                              // % compagni con Trick Room (dai "Teammates": P(compagno) x P(ha TR))')
    w('    u8 twMate;                              // idem con Tailwind')
    w('};')
    w('')
    sources = []
    for e in entries:
        if e['source'] not in sources:
            sources.append(e['source'])
    w('static const char *const sVgcMetaSources[] UNUSED =')
    w('{')
    for s in sources:
        w(f'    "{s}",')
    w('};')
    w('')
    w('static const struct VgcMetaEntry sVgcMeta[] =')
    w('{')
    for e in entries:
        w('    {')
        w(f'        .species = {e["species"]}, .usage = {e["usage"]}, .source = {sources.index(e["source"])}, // {e["source"]}, {e["raw"]} lotte')
        if e['moves']:
            w('        .moves = { ' + ', '.join(m for m, _ in e['moves']) + ' },')
            w('        .moveP = { ' + ', '.join(str(p) for _, p in e['moves']) + ' },')
        if e['items']:
            w('        .items = { ' + ', '.join(m for m, _ in e['items']) + ' },')
            w('        .itemP = { ' + ', '.join(str(p) for _, p in e['items']) + ' },')
        if e['abilities']:
            w('        .abilities = { ' + ', '.join(m for m, _ in e['abilities']) + ' },')
            w('        .abilityP = { ' + ', '.join(str(p) for _, p in e['abilities']) + ' },')
        if e['megas']:
            w('        .megas = { ' + ', '.join(m for m, _ in e['megas']) + ' },')
            w('        .megaP = { ' + ', '.join(str(p) for _, p in e['megas']) + ' },')
        if e['spreads']:
            w('        .spreads = {')
            for nat, evs, p in e['spreads']:
                w(f'            {{ {nat}, {p}, {{ ' + ', '.join(str(x // 4) for x in evs) + ' } },')
            w('        },')
        for name in ('trSpread', 'fastSpread'):
            if name in e:
                nat, evs, p = e[name]
                w(f'        .{name} = {{ {nat}, {p}, {{ ' + ', '.join(str(x // 4) for x in evs) + ' } },')
        if e.get('trMate') or e.get('twMate'):
            w(f'        .trMate = {e["trMate"]}, .twMate = {e["twMate"]},')
        w('    },')
    w('};')
    w('')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(L))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Analisi dei log delle lotte VGC giocate a mano in mGBA (logger CUSTOM_VGC_BATTLE_LOG, src/vgc_battle_log.c).

Uso:
  python tools/vgc_training/analizza_log.py LOG [LOG ...] [--out CARTELLA]

LOG è il file di log salvato da mGBA (Impostazioni > Logging > "Log to file", oppure il testo copiato dalla
finestra Tools > View Logs). Vale qualunque file che contenga righe "VGCLOG ...": il prefisso di mGBA
("[WARN] GBA Debug: ") viene ignorato. Funziona anche sull'uscita del self-play (test/vgc_selfplay).

Per ogni lotta scrive un report Markdown turno per turno in docs/ia_vgc/report/partite_umane/ e, alla fine, il
riepilogo di tutte le lotte analizzate (riepilogo.md). I controlli dei misplay sono quelli del self-play
(test/vgc_selfplay/vgc_selfplay.c, CheckBattlerChoice) rifatti sui dati del log, solo per il lato dell'IA (A):
"certo" = errore senza attenuanti note, "dubbio" = caso che dipende da una lettura o da dati incompleti.
Le stime di danno sono quelle dell'IA a inizio turno (righe DMG): per i Pokémon del giocatore contano solo le
mosse che l'IA conosceva.

Guida: docs/ia_vgc/logger.md
"""
import argparse
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT_DEFAULT = ROOT / 'docs' / 'ia_vgc' / 'report' / 'partite_umane'

PROTECT_MOVES = {'Protect', 'Detect', 'Spiky Shield', "King's Shield", 'Baneful Bunker', 'Obstruct', 'Silk Trap',
                 'Burning Bulwark', 'Max Guard'}
SACRIFICE_MOVES = {'Final Gambit', 'Explosion', 'Self-Destruct', 'Selfdestruct', 'Misty Explosion', 'Memento'}
COMBO_ABILITIES = {'Telepathy', 'Justified', 'Stamina', 'Weak Armor', 'Anger Point', 'Berserk', 'Rattled',
                   'Volt Absorb', 'Lightning Rod', 'Lightningrod', 'Motor Drive', 'Water Absorb', 'Storm Drain',
                   'Dry Skin', 'Flash Fire', 'Well-Baked Body', 'Sap Sipper', 'Steam Engine', 'Water Compaction',
                   'Thermal Exchange', 'Levitate', 'Earth Eater'}
STATUS_NAMES = {'SLP': 'sonno', 'PSN': 'avvelenato', 'TOX': 'iperavvelenato', 'BRN': 'scottato', 'FRZ': 'congelato',
                'PAR': 'paralizzato', 'FRB': 'congelamento', '-': ''}

RE_ACTION_MOVE = re.compile(r'^AZIONE b(\d) ([GA]) (.+?) MOSSA (.+?) -> b(\d+) (\S*) ?cat=(\S+) fx=(\S+) idx=(\d+)(.*)$')
RE_ACTION_SWITCH = re.compile(r'^AZIONE b(\d) ([GA]) (.+?) CAMBIO -> (.+?) \(slot(\d+)\)$')
RE_ACTION_OTHER = re.compile(r'^AZIONE b(\d) ([GA]) ?(.*?) ?(NESSUNA|STRUMENTO|FUGA)$')
RE_MON = re.compile(r'^MON b(\d) ([GA]) (.+?) slot(\d+) ps=(\d+)/(\d+) stato=(\S+) stadi=(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+) '
                    r'vel=(\d+) strum=(.+?) abil=(.+?) primoturno=(\d) wincon=(\d) panchina=(-?\d+) encore=(.+)$')
RE_MON_EMPTY = re.compile(r'^MON b(\d) ([GA]) vuoto panchina=(-?\d+)$')
RE_DMG_HEAD = re.compile(r'^DMG b(\d)>b(\d)(.*)$')
RE_DMG_ENTRY = re.compile(r'^(.+) (-?\d+)-(-?\d+) a(\d+) e(\d+)$')
RE_SET = re.compile(r'^SET ([GA]) (\d) (.+?) @ (.+?) \| (.+?) \| (.+?) / (.+?) / (.+?) / (.+?) \| liv (\d+) vel (\d+)(?: ps (\d+))?$')
RE_PS = re.compile(r'^PS b(\d) ([GA]) (.+?) (-?\d+) -> (-?\d+) / (\d+)$')
RE_KO = re.compile(r'^KO b(\d) ([GA]) (.+?) t(\d+)$')
RE_FIELD = re.compile(r'^CAMPO meteo=(\S+) terreno=(\S+) trickroom=(\d+) tailwindG=(\d+) tailwindA=(\d+) schermiG=(\d+) schermiA=(\d+) lentaA=(\d+)$')
RE_PLAN_HEAD = re.compile(r'^PIANO b(\d) (.+?) T(\d+) coppie=(\d+) scelta=(\d+) regole=(-?\d+) fotogrammi=(\d+) simulazioni=(\d+)$')
RE_PAIR = re.compile(r'^COPPIA b(\d) #(\d+): b(\d) (.+?) \+ b(\d) (.+?) = (-?\d+)(.*)$')
RE_FOE = re.compile(r'^PREVISTA b(\d) ([AB])(\d+): b(\d) (.+?) peso=(-?\d+)$')
RE_END = re.compile(r'^FINE esito=(\S+) turni=(\d+) restantiG=(\d+) restantiA=(\d+)$')


def foes_of(b):
    return [0, 2] if b % 2 == 1 else [1, 3]


def partner_of(b):
    return b ^ 2


class Turn:
    def __init__(self, num):
        self.num = num
        self.field = {}
        self.mons = {}
        self.dmg = {}
        self.plans = []
        self.actions = {}
        self.order = []
        self.events = []      # (tipo, testo)
        self.kos = []         # (battler, specie)
        self.misplays = []    # (certo, categoria, battler, specie, dettaglio)


class Battle:
    def __init__(self):
        self.trainer = ''
        self.trainer_id = -1
        self.game = None
        self.best_of = None
        self.sets = {}
        self.tp_groups = []
        self.tp_leads = []
        self.tp_pred = []
        self.porta = {}
        self.started = False
        self.turns = []
        self.pre_events = []
        self.end = None

    @property
    def cur(self):
        return self.turns[-1] if self.turns else None

    def title(self):
        g = f' - gara {self.game} di Bo{self.best_of}' if self.game else ''
        return f'{self.trainer}{g}'


# ---------------------------------------------------------------------
# Lettura
# ---------------------------------------------------------------------
def parse_action_target(text):
    """'Earthquake>b0', 'Protect', 'CAMBIO>Garchomp', '-' -> (tipo, mossa, bersaglio)."""
    if text == '-':
        return ('nessuna', None, None)
    if text.startswith('CAMBIO>'):
        return ('cambio', text[7:], None)
    m = re.match(r'^(.+)>b(\d)$', text)
    if m:
        return ('mossa', m.group(1), int(m.group(2)))
    return ('protect', text, None)


def parse_lines(lines):
    battles = []
    b = None
    plan = None
    for raw in lines:
        i = raw.find('VGCLOG ')
        if i < 0:
            continue
        line = raw[i + 7:].rstrip('\r\n').rstrip()
        key = line.split(' ', 1)[0]
        rest = line[len(key) + 1:]
        if key == 'INIZIO':
            m = re.match(r'allenatore=(\d+) (.*)$', rest)
            if b is None or b.started or b.end is not None:
                b = Battle()
                battles.append(b)
            if m:
                b.trainer_id = int(m.group(1))
                b.trainer = m.group(2)
            continue
        if b is None:
            b = Battle()
            battles.append(b)
        if key == 'SET':
            m = RE_SET.match(line)
            if m:
                b.sets[(m.group(1), int(m.group(2)))] = {
                    'species': m.group(3), 'item': m.group(4), 'ability': m.group(5),
                    'moves': [x for x in m.group(6, 7, 8, 9)], 'level': int(m.group(10)), 'speed': int(m.group(11)),
                    'hp': int(m.group(12)) if m.group(12) else None}
        elif key == 'TP':
            kind, _, data = rest.partition(' ')
            if kind == 'GRUPPO':
                n, score, mons = data.split(' ', 2)
                b.tp_groups.append((int(n), int(score), mons))
            elif kind == 'LEAD':
                m = re.match(r'(\d+) (-?\d+) peggiore=(-?\d+) (.+)$', data)
                if m:
                    b.tp_leads.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)))
            elif kind == 'PREVISTE':
                m = re.match(r'(\d+) peso=(-?\d+) (.+)$', data)
                if m:
                    b.tp_pred.append((int(m.group(1)), int(m.group(2)), m.group(3)))
        elif key == 'PORTA':
            side, _, data = rest.partition(' ')
            b.porta[side] = data
        elif key == 'GARA':
            m = re.match(r'(\d+) di Bo(\d+) allenatore=(\d+) (.*)$', rest)
            if m:
                b.game, b.best_of = int(m.group(1)), int(m.group(2))
                b.trainer_id, b.trainer = int(m.group(3)), m.group(4)
        elif key == 'LOTTA':
            b.started = True
        elif key == 'TURNO':
            b.started = True
            b.turns.append(Turn(int(rest)))
            plan = None
        elif b.cur is None:
            if key in ('MSG', 'PS', 'ABILITA', 'KO'):
                b.pre_events.append((key, rest))
            elif key == 'FINE':
                m = RE_END.match(line)
                b.end = m.groups() if m else (rest,)
            continue
        elif key == 'CAMPO':
            m = RE_FIELD.match(line)
            if m:
                t = b.cur
                t.field = {'meteo': m.group(1), 'terreno': m.group(2), 'tr': int(m.group(3)), 'twG': int(m.group(4)),
                           'twA': int(m.group(5)), 'schermiG': int(m.group(6)), 'schermiA': int(m.group(7)),
                           'lentaA': int(m.group(8))}
        elif key == 'MON':
            m = RE_MON.match(line)
            if m:
                bt = int(m.group(1))
                b.cur.mons[bt] = {
                    'side': m.group(2), 'species': m.group(3), 'slot': int(m.group(4)), 'hp': int(m.group(5)),
                    'maxhp': int(m.group(6)), 'status': m.group(7), 'stages': [int(x) for x in m.group(8, 9, 10, 11, 12)],
                    'speed': int(m.group(13)), 'item': m.group(14), 'ability': m.group(15), 'first': m.group(16) == '1',
                    'wincon': m.group(17) == '1', 'bench': int(m.group(18)), 'encore': m.group(19), 'alive': True}
            else:
                m = RE_MON_EMPTY.match(line)
                if m:
                    b.cur.mons[int(m.group(1))] = {'side': m.group(2), 'alive': False, 'bench': int(m.group(3)),
                                                   'species': '-', 'hp': 0, 'maxhp': 1}
        elif key == 'DMG':
            m = RE_DMG_HEAD.match(line)
            if m:
                entries = []
                for part in m.group(3).split(' | ')[1:] if m.group(3).startswith(' | ') else m.group(3).split(' | '):
                    part = part.strip().lstrip('|').strip()
                    e = RE_DMG_ENTRY.match(part)
                    if e:
                        entries.append({'move': e.group(1), 'min': int(e.group(2)), 'max': int(e.group(3)),
                                        'acc': int(e.group(4)), 'eff': int(e.group(5))})
                b.cur.dmg[(int(m.group(1)), int(m.group(2)))] = entries
        elif key == 'PIANO':
            m = RE_PLAN_HEAD.match(line)
            if m:
                plan = {'me': int(m.group(1)), 'species': m.group(2), 'pairs': int(m.group(4)), 'best': int(m.group(5)),
                        'rules': int(m.group(6)), 'frames': int(m.group(7)), 'sims': int(m.group(8)),
                        'pair_list': [], 'foes': []}
                b.cur.plans.append(plan)
        elif key == 'COPPIA':
            m = RE_PAIR.match(line)
            if m and plan is not None:
                plan['pair_list'].append({'idx': int(m.group(2)), 'ua': int(m.group(3)), 'a': m.group(4),
                                          'ub': int(m.group(5)), 'b': m.group(6), 'v': int(m.group(7)),
                                          'mark': m.group(8).strip()})
        elif key == 'PREVISTA':
            m = RE_FOE.match(line)
            if m and plan is not None:
                plan['foes'].append({'slot': m.group(2), 'i': int(m.group(3)), 'user': int(m.group(4)),
                                     'act': m.group(5), 'w': int(m.group(6))})
        elif key == 'AZIONE':
            t = b.cur
            m = RE_ACTION_MOVE.match(line)
            if m:
                extra = m.group(10)
                t.actions[int(m.group(1))] = {'kind': 'mossa', 'side': m.group(2), 'species': m.group(3),
                                              'move': m.group(4), 'target': int(m.group(5)), 'rel': m.group(6),
                                              'cat': m.group(7), 'fx': m.group(8), 'idx': int(m.group(9)),
                                              'mega': '+MEGA' in extra, 'locked': 'bloccata' in extra}
                continue
            m = RE_ACTION_SWITCH.match(line)
            if m:
                t.actions[int(m.group(1))] = {'kind': 'cambio', 'side': m.group(2), 'species': m.group(3),
                                              'to': m.group(4), 'toSlot': int(m.group(5)), 'fx': '', 'cat': ''}
                continue
            m = RE_ACTION_OTHER.match(line)
            if m:
                t.actions[int(m.group(1))] = {'kind': m.group(4).lower(), 'side': m.group(2), 'species': m.group(3),
                                              'fx': '', 'cat': ''}
        elif key == 'ORDINE':
            b.cur.order = [int(x[1:]) for x in rest.split()]
        elif key in ('MSG', 'PS', 'ABILITA'):
            b.cur.events.append((key, rest))
        elif key == 'KO':
            m = RE_KO.match(line)
            b.cur.events.append((key, rest))
            if m:
                b.cur.kos.append((int(m.group(1)), m.group(3)))
        elif key == 'FINE':
            m = RE_END.match(line)
            b.end = m.groups() if m else (rest,)
    return [x for x in battles if x.turns or x.sets]


# ---------------------------------------------------------------------
# Controlli dei misplay (come CheckBattlerChoice del self-play, sui dati del log)
# ---------------------------------------------------------------------
class Checker:
    def __init__(self, battle):
        self.b = battle
        self.prev = {}         # battler -> (turno, slot/specie, mossa)
        self.prev_switch = {}  # battler -> (turno, specie entrata, specie uscita)

    # --- dati del turno ---
    def alive(self, t, x):
        m = t.mons.get(x)
        return m is not None and m.get('alive') and m['hp'] > 0

    def hp(self, t, x):
        return t.mons[x]['hp'] if x in t.mons else 0

    def entry(self, t, atk, dfn, move):
        for e in t.dmg.get((atk, dfn), []):
            if e['move'] == move:
                return e
        return None

    def choice_min_on(self, t, x, dfn):
        c = t.actions.get(x)
        if not c or c['kind'] != 'mossa' or c['cat'] == 'sta' or not self.alive(t, x) or not self.alive(t, dfn):
            return 0
        spread = 'A' in c['fx'] or 'X' in c['fx']
        if c['target'] != dfn and not spread:
            return 0
        if spread and 'A' in c['fx'] and (dfn % 2) == (x % 2):
            return 0
        e = self.entry(t, x, dfn, c['move'])
        if not e or e['acc'] < 100:
            return 0
        return e['min']

    def choice_kos(self, t, x, dfn):
        d = self.choice_min_on(t, x, dfn)
        return d > 0 and d >= self.hp(t, dfn)

    def pair_kos(self, t, x, p, dfn):
        if not self.alive(t, dfn):
            return False
        dx, dp = self.choice_min_on(t, x, dfn), self.choice_min_on(t, p, dfn)
        cp, cx = t.actions.get(p, {}), t.actions.get(x, {})
        if 'H' in cp.get('fx', ''):
            return dx > 0 and dx * 3 // 2 >= self.hp(t, dfn)
        if 'H' in cx.get('fx', ''):
            return dp > 0 and dp * 3 // 2 >= self.hp(t, dfn)
        return dx > 0 and dp > 0 and dx + dp >= self.hp(t, dfn)

    def clearly_faster(self, t, a, d):
        sa, sd = t.mons[a].get('speed', 0), t.mons[d].get('speed', 0)
        if t.field.get('tr', 0) > 0:
            return sa * 11 < sd * 10
        return sa * 10 > sd * 11

    def knows(self, t, x, names):
        mon = t.mons.get(x, {})
        side = mon.get('side')
        for (s, _), st in self.b.sets.items():
            if s == side and st['species'] == mon.get('species'):
                if any(mv in names for mv in st['moves']):
                    return True
        for (atk, _), entries in t.dmg.items():
            if atk == x and any(e['move'] in names for e in entries):
                return True
        return False

    def any_attack_hits_foe(self, t, x):
        for f in foes_of(x):
            if self.alive(t, f) and any(e['max'] > 0 and e['eff'] > 0 for e in t.dmg.get((x, f), [])):
                return True
        return False

    def report(self, t, certain, cat, x, detail):
        t.misplays.append((certain, cat, x, t.mons.get(x, {}).get('species', '?'), detail))

    def order_pos(self, t, x):
        return t.order.index(x) if x in t.order else 9

    # --- controlli ---
    def check_turn(self, t):
        for x in (1, 3):
            if self.alive(t, x) and x in t.actions:
                self.check_battler(t, x)
        # Turno di controllo velocità sprecato: tutto il lato A in Protect/cambio durante il proprio TW/TR.
        side = [x for x in (1, 3) if self.alive(t, x)]
        if side and all(t.actions.get(x, {}).get('kind') == 'cambio' or 'P' in t.actions.get(x, {}).get('fx', '') for x in side):
            if t.field.get('twA', 0) > 1 or (t.field.get('tr', 0) > 1 and t.field.get('lentaA')):
                self.report(t, False, 'VELOCITA_SPRECATA', side[0], 'tutto il lato in Protect/cambio durante il proprio Tailwind/Trick Room')
        self.check_after_turn(t)

    def check_battler(self, t, x):
        c = t.actions[x]
        mon = t.mons[x]
        p = partner_of(x)
        p_alive = self.alive(t, p)
        fs = foes_of(x)
        is_move = c['kind'] == 'mossa'
        fx = c.get('fx', '')
        damaging = is_move and c['cat'] != 'sta'
        spread = 'A' in fx or 'X' in fx
        tg = c.get('target', 9)
        prev = self.prev.get(x)

        # Minaccia di KO su x (mosse degli avversari note all'IA).
        threat_min = threat_max = False
        threat_from = {}
        threat_foe = fs[0]
        for f in fs:
            if not self.alive(t, f):
                continue
            for e in t.dmg.get((f, x), []):
                if e['max'] <= 0:
                    continue
                if e['min'] >= mon['hp'] and e['acc'] >= 100:
                    threat_min = True
                    threat_from[f] = True
                    threat_foe = f
                if e['max'] >= mon['hp']:
                    threat_max = True
        if mon['hp'] == mon['maxhp'] and 'Focus Sash' in mon['item']:
            threat_min = threat_max = False
        if threat_min:
            stopped = all(
                (self.choice_kos(t, x, f) and self.clearly_faster(t, x, f))
                or (p_alive and self.choice_kos(t, p, f) and self.clearly_faster(t, p, f))
                or (p_alive and self.pair_kos(t, x, p, f) and self.clearly_faster(t, x, f) and self.clearly_faster(t, p, f))
                for f in threat_from)
            if stopped:
                threat_min = False
        t.mons[x]['_threat'] = threat_min

        forced = is_move and (mon['encore'] != '-' or mon['status'] == 'SLP' or c.get('locked'))
        if is_move and not forced:
            # 1. Protect consecutivo.
            if 'P' in fx and prev and prev[0] == t.num - 1 and prev[1] == mon['species'] and prev[3]:
                self.report(t, self.any_attack_hits_foe(t, x), 'PROTECT_CONSECUTIVO', x, f"{c['move']} dopo {prev[2]} al turno prima")
            # 2. Fake Out fuori dal primo turno in campo.
            if 'F' in fx and not mon['first']:
                self.report(t, True, 'FAKE_OUT_TARDIVO', x, f"{c['move']} non al primo turno in campo (fallisce)")
            # 3. Mossa su un immune noto.
            if not spread and tg in fs and self.alive(t, tg):
                e = self.entry(t, x, tg, c['move'])
                if e and e['eff'] == 0:
                    if damaging:
                        o = fs[0] if tg == fs[1] else fs[1]
                        oe = self.entry(t, x, o, c['move'])
                        redirect = p_alive and self.choice_kos(t, p, tg) and self.alive(t, o) and oe is not None and oe['eff'] > 0
                        any_hit = self.any_attack_hits_foe(t, x)
                        self.report(t, not redirect and any_hit, 'IMMUNE', x, f"{c['move']} su {t.mons[tg]['species']} (immune"
                                    + (", ma il compagno lo manda KO prima e la mossa passa sull'altro" if redirect else '') + ')')
                    else:
                        self.report(t, False, 'IMMUNE', x, f"{c['move']} su {t.mons[tg]['species']} (efficacia 0 stimata: immunita' probabile)")
            # 4. Bersaglio che il compagno manda già KO prima.
            if damaging and not spread and p_alive and tg in fs and self.choice_kos(t, p, tg) \
                    and t.actions.get(p, {}).get('kind') == 'mossa' and self.order_pos(t, p) < self.order_pos(t, x):
                o = fs[0] if tg == fs[1] else fs[1]
                last_foe = not self.alive(t, o) and t.mons.get(tg, {}).get('bench', 0) == 0
                oe = self.entry(t, x, o, c['move'])
                if not last_foe and (not self.alive(t, o) or (oe is not None and oe['eff'] == 0)):
                    self.report(t, True, 'BERSAGLIO_GIA_KO', x, f"{c['move']} su {t.mons[tg]['species']} che il compagno manda KO prima; nessun redirect utile")
            # 5. Attacco sul compagno senza combo.
            if damaging and p_alive:
                pm = t.mons[p]
                e = self.entry(t, x, p, c['move'])
                combo = pm['ability'] in COMBO_ABILITIES or (e is not None and e['eff'] == 0) \
                    or (pm['item'] == 'Weakness Policy' and e is not None and e['eff'] >= 200) or c['move'] == 'Beat Up'
                if not spread and tg == p and not combo:
                    self.report(t, True, 'ATTACCO_COMPAGNO', x, f"{c['move']} sul compagno {pm['species']} senza combo")
                elif 'X' in fx and not combo and e is not None:
                    pc = t.actions.get(p, {})
                    if pc.get('kind') != 'cambio' and 'P' not in pc.get('fx', ''):
                        if e['min'] >= pm['hp']:
                            self.report(t, True, 'ATTACCO_COMPAGNO', x, f"{c['move']} manda KO il compagno {pm['species']}")
                        elif (e['min'] + e['max']) >= pm['hp']:
                            self.report(t, False, 'ATTACCO_COMPAGNO', x, f"{c['move']} toglie meta' dei PS rimasti al compagno {pm['species']}")
            # 6. Setup con un KO in arrivo.
            if 'S' in fx and threat_min:
                pc = t.actions.get(p, {}) if p_alive else {}
                covered = 'M' in pc.get('fx', '') or ('F' in pc.get('fx', '') and pc.get('target') == threat_foe and t.mons[p]['first'])
                if not covered:
                    self.report(t, True, 'SETUP_SOTTO_KO', x, f"{c['move']} mentre {t.mons[threat_foe]['species']} lo manda KO (danno minimo stimato)")
            elif 'S' in fx and threat_max:
                self.report(t, False, 'SETUP_SOTTO_KO', x, f"{c['move']} con un KO possibile (danno massimo)")
            # 7. Tailwind / Trick Room sprecati.
            if 'W' in fx and t.field.get('twA', 0) > 1:
                self.report(t, True, 'VELOCITA_SPRECATA', x, f"Tailwind con il proprio Tailwind ancora attivo ({t.field['twA']} turni)")
            if 'R' in fx and t.field.get('tr', 0) > 1 and t.field.get('lentaA'):
                self.report(t, True, 'VELOCITA_SPRECATA', x, f"Trick Room con Trick Room attivo ({t.field['tr']} turni): lo chiude")

        # 8. KO garantito disponibile e non preso.
        if c['kind'] in ('mossa', 'cambio') and not forced:
            for f in fs:
                if not self.alive(t, f):
                    continue
                fm = t.mons[f]
                if fm['hp'] == fm['maxhp'] and ('Focus Sash' in fm['item'] or fm['ability'] == 'Sturdy'):
                    continue
                pf = t.actions.get(partner_of(f), {})
                ko = None
                for e in t.dmg.get((x, f), []):
                    if e['move'] in SACRIFICE_MOVES or e['max'] <= 0:
                        continue
                    if e['min'] >= fm['hp'] and e['acc'] >= 100:
                        ko = e
                        break
                if ko is None:
                    continue
                if 'M' in pf.get('fx', '') and self.alive(t, partner_of(f)):
                    continue
                if threat_min and self.clearly_faster(t, threat_foe, x):
                    continue
                o = fs[0] if f == fs[1] else fs[1]
                covered = self.choice_kos(t, x, f) or (p_alive and self.choice_kos(t, p, f)) or self.choice_kos(t, x, o) \
                    or (p_alive and (self.pair_kos(t, x, p, f) or self.pair_kos(t, x, p, o)))
                if covered:
                    continue
                chosen = c.get('move') if c['kind'] == 'mossa' else 'cambio -> ' + c.get('to', '?')
                if damaging and self.knows(t, f, PROTECT_MOVES):
                    self.report(t, False, 'KO_NON_PRESO_LETTURA', x, f"{ko['move']} avrebbe mandato KO {fm['species']}, scelto {chosen} (ha Protect: possibile lettura)")
                elif damaging and spread:
                    self.report(t, False, 'KO_NON_PRESO_AREA', x, f"{ko['move']} avrebbe mandato KO {fm['species']}, scelta la mossa ad area {chosen}")
                elif damaging:
                    self.report(t, True, 'KO_NON_PRESO', x, f"{ko['move']} avrebbe mandato KO {fm['species']}, scelto {chosen}")
                else:
                    self.report(t, False, 'KO_NON_PRESO_SUPPORTO', x, f"{ko['move']} avrebbe mandato KO {fm['species']}, scelto {chosen}")
                break

        # 9. Cambi a ripetizione.
        if c['kind'] == 'cambio':
            ps = self.prev_switch.get(x)
            if ps and t.num - ps[0] <= 2 and ps[1] == mon['species'] and ps[2] == c['to']:
                self.report(t, t.num - ps[0] <= 1, 'CAMBI_RIPETUTI', x, f"rientra {c['to']} uscito {t.num - ps[0]} turni fa")
            self.prev_switch[x] = (t.num, c['to'], mon['species'])

        t.mons[x]['_prev_protect'] = bool(prev and prev[0] == t.num - 1 and prev[1] == mon['species'] and prev[3])
        self.prev[x] = (t.num, mon['species'], c.get('move'), 'P' in fx)

    def check_after_turn(self, t):
        """Controlli che si chiudono con l'esito del turno: cambio in un KO, win condition persa."""
        crit_before = False
        for kind, text in t.events:
            if kind == 'MSG' and 'critical hit' in text.lower():
                crit_before = True
            if kind == 'MSG' and ' used ' in text:
                crit_before = False
            if kind != 'KO':
                continue
            m = RE_KO.match('KO ' + text)
            if not m:
                continue
            x, species = int(m.group(1)), m.group(3)
            if x not in (1, 3):
                continue
            c = t.actions.get(x, {})
            mon = t.mons.get(x, {})
            if c.get('kind') == 'cambio' and c.get('to') == species:
                self.report(t, not crit_before, 'CAMBIO_IN_KO', x, 'entrato con un cambio e KO nello stesso turno' + (' (colpo critico)' if crit_before else ''))
            elif mon.get('alive') and mon.get('species') == species and mon.get('wincon') and c.get('kind') == 'mossa' \
                    and 'P' not in c.get('fx', ''):
                could_protect = self.knows(t, x, PROTECT_MOVES) and not mon.get('_prev_protect', False)
                could_switch = mon.get('bench', 0) > 0
                if could_protect or could_switch:
                    what = ', '.join(w for w, ok in (('Protect disponibile', could_protect), ('cambio possibile', could_switch)) if ok)
                    known = mon.get('_threat', False)
                    self.report(t, known, 'WIN_CONDITION_PERSA', x, f"KO senza Protect/cambio ({what})"
                                + (': il KO era prevedibile' if known else ': KO non previsto dalle stime'))


# ---------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------
def esc(s):
    return str(s).replace('|', '\\|')


def outcome_text(b):
    if not b.end:
        return 'non finita nel log'
    e = b.end[0]
    return {'VITTORIA_G': 'vince il giocatore', 'VITTORIA_A': "vince l'IA", 'PAREGGIO': 'pareggio', 'FUGA': 'fuga'}.get(e, e)


def mon_cell(m):
    if not m or not m.get('alive'):
        return '(vuoto)'
    st = [f"{m['species']} {m['hp']}/{m['maxhp']} ({100 * m['hp'] // max(m['maxhp'], 1)}%)"]
    if m['status'] != '-':
        st.append(STATUS_NAMES.get(m['status'], m['status']))
    stages = [f"{n}{v:+d}" for n, v in zip(('Atk', 'Def', 'Spe', 'SpA', 'SpD'), m['stages']) if v]
    if stages:
        st.append(' '.join(stages))
    if m['wincon']:
        st.append('win condition')
    if m['encore'] != '-':
        st.append('Encore: ' + m['encore'])
    return ', '.join(st)


def action_text(t, x):
    c = t.actions.get(x)
    if not c:
        return '-'
    if c['kind'] == 'mossa':
        tgt = t.mons.get(c['target'], {}).get('species', f"b{c['target']}") if c['target'] < 4 else '-'
        s = f"{c['move']} -> {tgt} (b{c['target']})"
        if c['mega']:
            s += ' +MEGA'
        if c['locked']:
            s += ' (bloccata da Choice)'
        return s
    if c['kind'] == 'cambio':
        return f"cambio -> {c['to']}"
    return c['kind']


def write_battle(b, path, idx):
    out = []
    w = out.append
    w(f"# Partita {idx}: {esc(b.title())}\n")
    nc = sum(1 for t in b.turns for m in t.misplays if m[0])
    nd = sum(1 for t in b.turns for m in t.misplays if not m[0])
    w(f"- Esito: **{outcome_text(b)}**" + (f" (turni {b.end[1]}, rimasti G {b.end[2]} / A {b.end[3]})" if b.end and len(b.end) == 4 else ''))
    w(f"- Misplay dell'IA: **{nc} certi**, {nd} dubbi\n")

    w("## Squadre\n")
    for side, name in (('G', 'Giocatore'), ('A', 'IA')):
        rows = sorted((k[1], v) for k, v in b.sets.items() if k[0] == side)
        if not rows:
            continue
        w(f"### {name}\n")
        w("| slot | Pokémon | strumento | abilità | mosse | vel |")
        w("|---|---|---|---|---|---|")
        for slot, s in rows:
            w(f"| {slot} | {esc(s['species'])} | {esc(s['item'])} | {esc(s['ability'])} | {esc(' / '.join(s['moves']))} | {s['speed']} |")
        w('')

    w("## Team preview\n")
    if b.tp_groups:
        w("Gruppi di 4 migliori dell'IA (punteggio):\n")
        for n, sc, mons in b.tp_groups:
            w(f"{n}. {esc(mons)} ({sc})")
        w('')
    if b.tp_leads:
        w("Lead migliori dell'IA (punteggio, caso peggiore contro le lead previste; la scelta è mista tra quelle vicine):\n")
        for n, sc, worst, mons in b.tp_leads:
            w(f"{n}. {esc(mons)} ({sc}, peggiore {worst})")
        w('')
    if b.tp_pred:
        w("Lead del giocatore previste dall'IA (peso):\n")
        for n, wt, mons in b.tp_pred:
            w(f"{n}. {esc(mons)} ({wt})")
        w('')
    for side, name in (('A', 'IA'), ('G', 'Giocatore')):
        if side in b.porta:
            w(f"- Porta {name}: {esc(b.porta[side])}")
    w('')

    if b.pre_events:
        w("## Prima del turno 1\n")
        for kind, text in b.pre_events:
            w(f"- {kind}: {esc(text)}")
        w('')

    for t in b.turns:
        w(f"## Turno {t.num}\n")
        f = t.field
        if f:
            parts = []
            if f['meteo'] != 'nessuno':
                parts.append('meteo ' + f['meteo'])
            if f['terreno'] != 'nessuno':
                parts.append('terreno ' + f['terreno'])
            if f['tr']:
                parts.append(f"Trick Room {f['tr']}")
            if f['twG']:
                parts.append(f"Tailwind G {f['twG']}")
            if f['twA']:
                parts.append(f"Tailwind A {f['twA']}")
            if f['schermiG']:
                parts.append(f"schermi G {f['schermiG']}")
            if f['schermiA']:
                parts.append(f"schermi A {f['schermiA']}")
            w('Campo: ' + (', '.join(parts) if parts else 'nulla') + '\n')
        w("| | sinistra | destra |")
        w("|---|---|---|")
        w(f"| IA | {esc(mon_cell(t.mons.get(1)))} | {esc(mon_cell(t.mons.get(3)))} |")
        w(f"| Giocatore | {esc(mon_cell(t.mons.get(0)))} | {esc(mon_cell(t.mons.get(2)))} |")
        w('')
        for plan in t.plans:
            w(f"**Pianificatore di b{plan['me']} {esc(plan['species'])}** ({plan['pairs']} coppie, {plan['frames']} fotogrammi, {plan['sims']} simulazioni)\n")
            pairs = sorted(plan['pair_list'], key=lambda p: -p['v'])
            for p in pairs[:6]:
                mark = ''
                if p['idx'] == plan['best']:
                    mark = ' **<== scelta**'
                if p['idx'] == plan['rules']:
                    mark += ' (regole)'
                w(f"- {p['v']}: b{p['ua']} {esc(p['a'])} + b{p['ub']} {esc(p['b'])}{mark}")
            if plan['foes']:
                w('- risposte previste del giocatore: ' + '; '.join(f"{x['slot']}{x['i']} b{x['user']} {esc(x['act'])} (peso {x['w']})" for x in plan['foes']))
            w('')
        w("Scelte:\n")
        for x, who in ((1, 'IA sx'), (3, 'IA dx'), (0, 'Giocatore sx'), (2, 'Giocatore dx')):
            if x in t.actions:
                w(f"- {who} ({esc(t.mons.get(x, {}).get('species', '?'))}): {esc(action_text(t, x))}")
        if t.order:
            w('- ordine: ' + ', '.join(f"b{x}" for x in t.order))
        w('')
        if t.events:
            w("Svolgimento:\n")
            for kind, text in t.events:
                if kind == 'MSG':
                    w(f"- {esc(text)}")
                elif kind == 'PS':
                    m = RE_PS.match('PS ' + text)
                    if m:
                        old, new, mx = int(m.group(4)), int(m.group(5)), int(m.group(6))
                        delta = new - old
                        w(f"  - PS {esc(m.group(3))} ({m.group(2)}): {old} -> {new} / {mx} ({'+' if delta > 0 else ''}{delta * 100 // max(mx, 1)}%)")
                elif kind == 'KO':
                    w(f"  - **KO** {esc(text)}")
                elif kind == 'ABILITA':
                    w(f"  - abilità: {esc(text)}")
            w('')
        if t.misplays:
            w("Misplay:\n")
            for certain, cat, x, sp, detail in t.misplays:
                w(f"- {'**CERTO**' if certain else 'dubbio'} {cat} b{x} {esc(sp)}: {esc(detail)}")
            w('')
    w("## Fine\n")
    w(f"{outcome_text(b)}" + (f": turni {b.end[1]}, rimasti G {b.end[2]} / A {b.end[3]}" if b.end and len(b.end) == 4 else ''))
    path.write_text('\n'.join(out) + '\n', encoding='utf-8')


def safe(s):
    return re.sub(r'[^A-Za-z0-9]+', '_', s).strip('_') or 'lotta'


def write_summary(results, path):
    out = ['# Partite umane contro l\'IA VGC: riepilogo\n']
    out.append(f"Lotte analizzate: {len(results)}\n")
    out.append('| report | allenatore | gara | esito | turni | misplay certi | dubbi |')
    out.append('|---|---|---|---|---|---|---|')
    cats_c, cats_d = Counter(), Counter()
    per_trainer = defaultdict(lambda: [0, 0, 0])
    for name, b in results:
        nc = sum(1 for t in b.turns for m in t.misplays if m[0])
        nd = sum(1 for t in b.turns for m in t.misplays if not m[0])
        for t in b.turns:
            for m in t.misplays:
                (cats_c if m[0] else cats_d)[m[1]] += 1
        pt = per_trainer[b.trainer]
        if b.end and b.end[0] == 'VITTORIA_A':
            pt[0] += 1
        elif b.end and b.end[0] == 'VITTORIA_G':
            pt[1] += 1
        pt[2] += nc
        gara = f"{b.game}/Bo{b.best_of}" if b.game else '-'
        turns = b.end[1] if b.end and len(b.end) == 4 else str(len(b.turns))
        out.append(f"| [{name}]({name}) | {esc(b.trainer)} | {gara} | {outcome_text(b)} | {turns} | {nc} | {nd} |")
    out.append('\n## Per allenatore\n')
    out.append('| allenatore | vittorie IA | vittorie giocatore | misplay certi |')
    out.append('|---|---|---|---|')
    for tr, (wa, wg, nc) in sorted(per_trainer.items()):
        out.append(f"| {esc(tr)} | {wa} | {wg} | {nc} |")
    out.append('\n## Misplay per categoria\n')
    out.append('| categoria | certi | dubbi |')
    out.append('|---|---|---|')
    for cat in sorted(set(cats_c) | set(cats_d), key=lambda c: (-cats_c[c], c)):
        out.append(f"| {cat} | {cats_c[cat]} | {cats_d[cat]} |")
    path.write_text('\n'.join(out) + '\n', encoding='utf-8')


def main():
    ap = argparse.ArgumentParser(description='Report delle lotte VGC dai log di mGBA (righe VGCLOG)')
    ap.add_argument('logs', nargs='+', help='file di log di mGBA')
    ap.add_argument('--out', default=str(OUT_DEFAULT), help='cartella dei report (default: docs/ia_vgc/report/partite_umane)')
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    results = []
    for log in args.logs:
        p = Path(log)
        text = p.read_text(encoding='utf-8', errors='replace').splitlines()
        battles = parse_lines(text)
        if not battles:
            print(f"{p}: nessuna riga VGCLOG (logger spento o livello di log sbagliato?)", file=sys.stderr)
            continue
        for i, b in enumerate(battles, 1):
            ck = Checker(b)
            for t in b.turns:
                ck.check_turn(t)
            name = f"{safe(p.stem)}_{i:02d}_{safe(b.trainer)}" + (f"_gara{b.game}" if b.game else '') + '.md'
            write_battle(b, out / name, i)
            results.append((name, b))
            nc = sum(1 for t in b.turns for m in t.misplays if m[0])
            print(f"{name}: {b.title()} - {outcome_text(b)}, {len(b.turns)} turni, {nc} misplay certi")
    if results:
        # Il riepilogo comprende anche i report già presenti nella cartella? No: solo i log passati ora, così
        # rilanciare lo script su tutti i log rigenera tutto in modo coerente.
        write_summary(results, out / 'riepilogo.md')
        print(f"riepilogo: {out / 'riepilogo.md'}")
    return 0 if results else 1


if __name__ == '__main__':
    sys.exit(main())

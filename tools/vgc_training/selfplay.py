#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Harness di self-play VGC: orchestratore delle partite IA contro IA (Windows).

Comandi (di solito tramite training.ps1 o i .bat):
  build                 compila il ROM del self-play in WSL (test/vgc_selfplay) e lo copia in work/
  start [opzioni]       prepara (o riprende) la coda delle partite e avvia il demone in sottofondo
  pause                 il demone finisce le partite in corso e non ne avvia altre
  resume                toglie la pausa (e riavvia il demone se non gira)
  stop                  ferma subito: le partite in corso tornano in coda
  status                avanzamento
  report                rigenera i report da work/results
  run-one P A [SEME]    una partita in primo piano (debug)
  list                  elenco degli allenatori VGC nel ROM

Guida: docs/ia_vgc/training.md
"""
import argparse
import ctypes
import datetime
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'tools' / 'vgc_training'
WORK = TOOL / 'work'
STATE = WORK / 'state.json'
PAUSE_FLAG = WORK / 'PAUSA'
STOP_FLAG = WORK / 'STOP'
PIDFILE = WORK / 'demone.pid'
DAEMON_LOG = WORK / 'demone.log'
ROM = WORK / 'selfplay.gba'
TRAINERS_JSON = WORK / 'allenatori.json'
LOGS = WORK / 'logs'
RESULTS = WORK / 'results'
TMP = WORK / 'tmp'
REPORT = ROOT / 'docs' / 'ia_vgc' / 'report'
MGBA = ROOT / 'tools' / 'mgba' / 'mgba-rom-test.exe'
MSYS_BIN = r'C:\msys64\mingw64\bin'
WSL_DISTRO = 'Ubuntu'
TEST_FILE = 'test/vgc_selfplay/vgc_selfplay.c'

# Layout di struct VgcSelfPlayArgs (include/test/vgc_selfplay.h): tenere in sincronia.
MAGIC = b'VGCSELFPLAYARG1\x00'
ARGS_LAYOUT = '<16sBBBBI4B4BB2BB4B20bB7B'
ARGS_SIZE = struct.calcsize(ARGS_LAYOUT)
assert ARGS_SIZE == 68
MODE_LIST, MODE_PREVIEW, MODE_FIXED = 0, 1, 2

DEFAULT_PARALLEL = 24
GAME_TIMEOUT_S = 45 * 60

CATEGORIES = {
    'IMMUNE': 'Mossa su un immune noto',
    'BERSAGLIO_GIA_KO': 'Bersaglio gia KO senza redirect utile',
    'KO_NON_PRESO': 'KO garantito non preso',
    'ATTACCO_COMPAGNO': 'Attacco sul compagno senza combo',
    'PROTECT_CONSECUTIVO': 'Protect consecutivo',
    'FAKE_OUT_TARDIVO': 'Fake Out dopo il primo turno',
    'CAMBIO_IN_KO': 'Cambio su chi va KO all\'entrata',
    'CAMBI_RIPETUTI': 'Cambi a ripetizione',
    'VELOCITA_SPRECATA': 'Turni di Tailwind/Trick Room sprecati',
    'SETUP_SOTTO_KO': 'Setup con KO in arrivo',
    'WIN_CONDITION_PERSA': 'Win condition persa evitabilmente',
    'BATON_SENZA_RICEVENTE': 'Team preview: Baton Passer senza ricevente',
    'ABILITA_SENZA_FIX': 'Team preview: abilita penalizzante senza chi la toglie',
    'LEAD_AUTOLESIONISTA': 'Team preview: lead che si danneggia da sola',
}

# ---------------------------------------------------------------------------------------------
# Utilita
# ---------------------------------------------------------------------------------------------

def now():
    return datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')


def load_json(path, default=None):
    try:
        with open(path, encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return default


def save_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + '.tmp')
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
    os.replace(tmp, path)


def wsl_path(p):
    p = str(Path(p).resolve()).replace('\\', '/')
    return '/mnt/' + p[0].lower() + p[2:]


def pid_alive(pid):
    if not pid:
        return False
    if os.name != 'nt':
        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    STILL_ACTIVE = 259
    h = ctypes.windll.kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not h:
        return False
    code = ctypes.c_ulong()
    ok = ctypes.windll.kernel32.GetExitCodeProcess(h, ctypes.byref(code))
    ctypes.windll.kernel32.CloseHandle(h)
    return bool(ok) and code.value == STILL_ACTIVE


def daemon_pid():
    try:
        pid = int(PIDFILE.read_text().strip())
    except (OSError, ValueError):
        return None
    return pid if pid_alive(pid) else None


def mgba_env():
    env = dict(os.environ)
    env['PATH'] = MSYS_BIN + os.pathsep + env.get('PATH', '')
    return env


def find_args_offset(rom_bytes):
    off = rom_bytes.find(MAGIC)
    if off < 0 or rom_bytes.find(MAGIC, off + 1) >= 0:
        raise RuntimeError('parametri del self-play non trovati (o trovati due volte) nel ROM: rifare "build"')
    return off


def pack_args(mode, player, enemy, seed, max_turns=0, player_order=None, enemy_order=None, scenario=None, verbose=0, variant=0):
    po = list(player_order or [0xFF] * 4)
    eo = list(enemy_order or [0xFF] * 4)
    sc = scenario or {}
    tw = sc.get('tailwind', [0, 0])
    hp = sc.get('hp', [0, 0, 0, 0])
    stages = sc.get('stadi', [[0] * 5 for _ in range(4)])
    flat = [int(v) for row in stages for v in row]
    reserved = [variant & 0xFF] + [0] * 6  # reserved[0]: variante dei pesi (M9, non ancora letta dal ROM)
    return struct.pack(ARGS_LAYOUT, MAGIC, mode, player, enemy, max_turns & 0xFF, seed & 0xFFFFFFFF,
                       *po, *eo, sc.get('turno', 0), tw[0], tw[1], sc.get('trickroom', 0), *hp, *flat, verbose, *reserved)


# ---------------------------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------------------------

def cmd_build(args):
    WORK.mkdir(parents=True, exist_ok=True)
    root = wsl_path(ROOT)
    script = (
        f"set -e; cd '{root}'; "
        f"make -j$(nproc) pokeemerald-test.elf TESTS='{TEST_FILE}'; "
        "mkdir -p build/vgc_selfplay; cp pokeemerald-test.elf build/vgc_selfplay/selfplay.elf; "
        f"tools/patchelf/patchelf build/vgc_selfplay/selfplay.elf gTestRunnerHeadless '\\x01' gTestRunnerSkipIsFail '\\x00' gTestRunnerArgv '{TEST_FILE}\\0'; "
        "arm-none-eabi-objcopy -O binary build/vgc_selfplay/selfplay.elf build/vgc_selfplay/selfplay.gba"
    )
    print(f'[{now()}] compilazione in WSL ({WSL_DISTRO})...')
    r = subprocess.run(['wsl', '-d', WSL_DISTRO, '--', 'bash', '-lc', script])
    if r.returncode != 0:
        print('Compilazione fallita.')
        return 1
    shutil.copyfile(ROOT / 'build' / 'vgc_selfplay' / 'selfplay.gba', ROM)
    find_args_offset(ROM.read_bytes())
    print(f'ROM pronto: {ROM}')
    return cmd_list(args)


# ---------------------------------------------------------------------------------------------
# Esecuzione di una partita
# ---------------------------------------------------------------------------------------------

def game_id(job):
    base = f"{job['p']:02d}_{job['e']:02d}_s{job['seed']}"
    if job.get('mode', MODE_PREVIEW) == MODE_FIXED:
        base += '_fisse'
    if job.get('variant'):
        base += f"_v{job['variant']}"
    return base


def start_game(job, rom_bytes, args_offset):
    TMP.mkdir(parents=True, exist_ok=True)
    LOGS.mkdir(parents=True, exist_ok=True)
    gid = game_id(job)
    data = bytearray(rom_bytes)
    packed = pack_args(job.get('mode', MODE_PREVIEW), job['p'], job['e'], job['seed'], job.get('max_turns', 0),
                       job.get('player_order'), job.get('enemy_order'), job.get('scenario'), job.get('verbose', 0),
                       job.get('variant', 0))
    data[args_offset:args_offset + ARGS_SIZE] = packed
    rom_path = TMP / f'{gid}.gba'
    rom_path.write_bytes(bytes(data))
    raw = open(LOGS / f'{gid}.raw.txt', 'wb')
    flags = 0x08000000 if os.name == 'nt' else 0  # CREATE_NO_WINDOW
    proc = subprocess.Popen([str(MGBA), '-l15', '-ClogLevel.gba.dma=16', '-Rr0', str(rom_path)],
                            stdout=raw, stderr=subprocess.STDOUT, env=mgba_env(), creationflags=flags)
    return {'proc': proc, 'raw': raw, 'rom': rom_path, 'gid': gid, 'start': time.time(), 'job': job}


def finish_game(run, killed=False):
    run['raw'].close()
    try:
        run['rom'].unlink()
    except OSError:
        pass
    gid = run['gid']
    raw_path = LOGS / f'{gid}.raw.txt'
    lines = []
    leaf = 0
    with open(raw_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if line.startswith('GBA Debug: '):
                line = line[len('GBA Debug: '):]
            if line.startswith('VGCLEAF'):
                leaf += 1
                continue
            lines.append(line)
    (LOGS / f'{gid}.log').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    raw_path.unlink()
    game = parse_game(lines, run['job'])
    game['vgcleaf_righe'] = leaf
    game['secondi'] = round(time.time() - run['start'], 1)
    if killed and game['esito'] in ('INCOMPLETA', None):
        game['esito'] = 'INTERROTTA'
    save_json(RESULTS / f'{gid}.json', game)
    write_game_report(game, lines)
    return game


def run_one(job):
    rom_bytes = ROM.read_bytes()
    off = find_args_offset(rom_bytes)
    run = start_game(job, rom_bytes, off)
    try:
        run['proc'].wait(timeout=GAME_TIMEOUT_S)
    except subprocess.TimeoutExpired:
        run['proc'].kill()
        run['proc'].wait()
    return finish_game(run)


# ---------------------------------------------------------------------------------------------
# Lettura del log
# ---------------------------------------------------------------------------------------------
RE_SET = re.compile(r'^VSP SET ([GA]) (\d+) (.*?) @ (.*?) \| (.*?) \| (.*?) \| liv (\d+) vel (\d+) \| id ([\d,]+)$')
RE_MIS = re.compile(r'^VSP (MISPLAY|DUBBIO) (\w+) t(\d+) b(\d+) (.*?): (.*)$')
RE_TPMIS = re.compile(r'^VSP (TPMISPLAY|TPDUBBIO) (\w+) ([GA]) (.*)$')
RE_PLAN = re.compile(r'^VGCPLAN b(\d+) pair(\d+): k(\d+) m(\d+) t(\d+) \+ k(\d+) m(\d+) t(\d+) = (-?\d+)(.*)$')
RE_FINE = re.compile(r'^VSP FINE esito=(\w+) turni=(\d+) restantiG=(\d+) restantiA=(\d+) misplay=(\d+) dubbi=(\d+)')
RE_INIZIO = re.compile(r'^VSP INIZIO giocatore=(\d+):(.*?) avversario=(\d+):(.*?) seme=(-?\d+)')
RE_PORTA = re.compile(r'^VSP PORTA ([GA]) (\d+) (\d+) (\d+) (\d+) \| lead (.*?) \+ (.*?) \| dietro (.*)$')
RE_TPG = re.compile(r'^VGCTP gruppo (\d+) (-?\d+)')
RE_TPL = re.compile(r'^VGCTP lead (\d+) (\d+) (-?\d+) (-?\d+)')


def side_of(b):
    return 'G' if int(b) % 2 == 0 else 'A'


def parse_game(lines, job):
    g = {'id': game_id(job), 'giocatore': job['p'], 'avversario': job['e'], 'seme': job['seed'],
         'modo': job.get('mode', MODE_PREVIEW), 'nomi': {}, 'esito': 'INCOMPLETA', 'turni': None,
         'restanti': None, 'misplay': [], 'dubbi': [], 'tp': {'G': {}, 'A': {}}, 'set': {'G': {}, 'A': {}},
         'mosse_id': {}, 'ultime_righe': lines[-25:]}
    tp_side = None
    turn = 0
    for line in lines:
        m = RE_INIZIO.match(line)
        if m:
            g['nomi'] = {'G': m.group(2), 'A': m.group(4)}
            continue
        m = RE_SET.match(line)
        if m:
            side, slot = m.group(1), int(m.group(2))
            moves = [x.strip() for x in m.group(6).split('/')]
            ids = [int(x) for x in m.group(9).split(',')]
            g['set'][side][slot] = {'specie': m.group(3), 'strumento': m.group(4), 'abilita': m.group(5), 'mosse': moves}
            for i, mv in zip(ids, moves):
                g['mosse_id'][str(i)] = mv
            continue
        if line.startswith('VSP TP '):
            tp_side = line.split()[2]
            g['tp'][tp_side].setdefault('gruppi', [])
            g['tp'][tp_side].setdefault('lead', [])
            continue
        m = RE_TPG.match(line)
        if m and tp_side:
            g['tp'][tp_side]['gruppi'].append([int(m.group(1)), int(m.group(2))])
            continue
        m = RE_TPL.match(line)
        if m and tp_side:
            g['tp'][tp_side]['lead'].append([int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))])
            continue
        m = RE_PORTA.match(line)
        if m:
            side = m.group(1)
            g['tp'][side]['porta'] = [int(m.group(i)) for i in range(2, 6)]
            g['tp'][side]['lead_scelta'] = [m.group(6), m.group(7)]
            g['tp'][side]['dietro'] = m.group(8)
            tp_side = None
            continue
        m = RE_TPMIS.match(line)
        if m:
            entry = {'categoria': m.group(2), 'turno': 0, 'lato': m.group(3), 'dettaglio': m.group(4)}
            (g['misplay'] if m.group(1) == 'TPMISPLAY' else g['dubbi']).append(entry)
            continue
        if line.startswith('VSP TURNO '):
            turn = int(line.split()[2])
            continue
        m = RE_MIS.match(line)
        if m:
            entry = {'categoria': m.group(2), 'turno': int(m.group(3)), 'battler': int(m.group(4)),
                     'lato': side_of(m.group(4)), 'pokemon': m.group(5), 'dettaglio': m.group(6)}
            (g['misplay'] if m.group(1) == 'MISPLAY' else g['dubbi']).append(entry)
            continue
        m = RE_FINE.match(line)
        if m:
            g['esito'] = m.group(1)
            g['turni'] = int(m.group(2))
            g['restanti'] = [int(m.group(3)), int(m.group(4))]
    if g['esito'] == 'INCOMPLETA':
        joined = '\n'.join(lines[-40:])
        if 'TIMEOUT' in joined:
            g['esito'] = 'TIMEOUT'
        elif 'CRASH' in joined:
            g['esito'] = 'CRASH'
    g['ultimo_turno'] = turn
    return g


# ---------------------------------------------------------------------------------------------
# Report di una partita (italiano, turno per turno)
# ---------------------------------------------------------------------------------------------
KIND_NAMES = {0: 'mossa', 1: 'Protect', 2: 'cambio', 3: 'niente'}
OUTCOME_TEXT = {'VITTORIA_G': 'vince il lato G', 'VITTORIA_A': 'vince il lato A', 'PAREGGIO': 'pareggio',
                'LIMITE_TURNI': 'interrotta al limite di turni', 'TIMEOUT': 'BLOCCATA (timeout)', 'CRASH': 'CRASH',
                'INCOMPLETA': 'incompleta (log senza fine)', 'INTERROTTA': 'interrotta dallo stop', 'ALTRO': 'altro esito'}


def plan_action_text(kind, move, target, mosse_id):
    kind = int(kind)
    if kind == 0:
        return f"{mosse_id.get(str(move), '#' + str(move))} -> b{target}"
    if kind == 1:
        return 'Protect'
    if kind == 2:
        return 'cambio'
    return '-'


def mask_names(mask, side_set):
    return ', '.join(side_set.get(i, {}).get('specie', '?') for i in range(6) if mask & (1 << i))


def write_game_report(g, lines):
    out_dir = REPORT / 'partite'
    out_dir.mkdir(parents=True, exist_ok=True)
    nG, nA = g['nomi'].get('G', str(g['giocatore'])), g['nomi'].get('A', str(g['avversario']))
    md = [f"# {nG} (G) contro {nA} (A) - seme {g['seme']}", '']
    res = OUTCOME_TEXT.get(g['esito'], g['esito'])
    md.append(f"**Esito:** {res}; turni: {g['turni'] if g['turni'] is not None else g['ultimo_turno']}; "
              f"Pokemon rimasti G/A: {g['restanti'] if g['restanti'] else '-'}; "
              f"misplay: {len(g['misplay'])}, casi dubbi: {len(g['dubbi'])}.")
    md.append('')
    md.append('Lati: G = lato del giocatore (battler b0, b2), A = lato avversario (b1, b3). Entrambi comandati dall\'IA VGC, '
              'ognuno vede solo cio che ha osservato dell\'altro.')
    md.append('')
    md.append('## Team preview')
    for side in ('G', 'A'):
        tp = g['tp'].get(side, {})
        sset = g['set'].get(side, {})
        name = nG if side == 'G' else nA
        md.append(f"### {name} ({side})")
        if 'lead_scelta' in tp:
            md.append(f"- Lead: **{tp['lead_scelta'][0]} + {tp['lead_scelta'][1]}**, dietro: {tp['dietro']}")
        groups = sorted(tp.get('gruppi', []), key=lambda x: -x[1])[:5]
        if groups:
            md.append('- Gruppi di 4 migliori (punteggio): ' + '; '.join(f"{mask_names(m, sset)} ({s})" for m, s in groups))
        leads = sorted(tp.get('lead', []), key=lambda x: -x[2])[:5]
        if leads:
            md.append('- Coppie di lead valutate (punteggio, caso peggiore): ' + '; '.join(
                f"{sset.get(a, {}).get('specie', '?')} + {sset.get(b, {}).get('specie', '?')} ({s}, {w})" for a, b, s, w in leads))
        md.append('')
    tperr = [x for x in g['misplay'] + g['dubbi'] if x['turno'] == 0]
    if tperr:
        md.append('Errori di team preview:')
        for x in tperr:
            kind = 'MISPLAY' if x in g['misplay'] else 'dubbio'
            md.append(f"- [{kind}] {CATEGORIES.get(x['categoria'], x['categoria'])} ({x['lato']}): {x['dettaglio']}")
        md.append('')

    md.append('## Turni')
    plan = []
    for line in lines:
        if line.startswith('VSP TURNO '):
            md.append('')
            md.append(f"### Turno {line.split()[2]}")
            plan = []
        elif line.startswith('VSP CAMPO '):
            md.append(f"- Campo: {line[len('VSP CAMPO '):]}")
        elif line.startswith('VSP MON '):
            md.append(f"- {line[len('VSP MON '):]}")
        elif line.startswith('VGCPLAN'):
            m = RE_PLAN.match(line)
            if m:
                plan.append(m)
        elif line.startswith('VSP AZIONE '):
            md.append(f"- **Scelta** {line[len('VSP AZIONE '):]}")
            b = line.split()[2]
            mine = [m for m in plan if 'b' + m.group(1) == b]
            if mine:
                best = sorted(mine, key=lambda m: -int(m.group(9)))[:4]
                alts = []
                for m in best:
                    tag = ' (scelta)' if '<==' in m.group(10) else (' (regole)' if 'regole' in m.group(10) else '')
                    alts.append(f"{plan_action_text(m.group(3), m.group(4), m.group(5), g['mosse_id'])} + compagno "
                                f"{plan_action_text(m.group(6), m.group(7), m.group(8), g['mosse_id'])} = {m.group(9)}{tag}")
                md.append('  - alternative valutate dal pianificatore: ' + '; '.join(alts))
        elif line.startswith('VSP MISPLAY ') or line.startswith('VSP DUBBIO '):
            m = RE_MIS.match(line)
            if m:
                tag = 'MISPLAY' if m.group(1) == 'MISPLAY' else 'dubbio'
                md.append(f"- **[{tag}] {CATEGORIES.get(m.group(2), m.group(2))}** - b{m.group(4)} {m.group(5)}: {m.group(6)}")
        elif line.startswith('VSP MSG '):
            md.append(f"  > {line[len('VSP MSG '):]}")
        elif line.startswith('VSP KO '):
            md.append(f"- KO: {line[len('VSP KO '):]}")
        elif line.startswith('VSP RIMPIAZZO ') or line.startswith('VSP USCITA '):
            md.append(f"- {line[4:]}")
        elif line.startswith('VSP FINE '):
            md.append('')
            md.append(f"**Fine:** {line[len('VSP FINE '):]}")
    if g['esito'] in ('TIMEOUT', 'CRASH', 'INCOMPLETA', 'INTERROTTA'):
        md.append('')
        md.append('Ultime righe del log:')
        md.append('```')
        md.extend(g['ultime_righe'])
        md.append('```')
    (out_dir / f"{g['id']}.md").write_text('\n'.join(md) + '\n', encoding='utf-8')


# ---------------------------------------------------------------------------------------------
# Riepilogo
# ---------------------------------------------------------------------------------------------

def fitness(games):
    """Gancio per M9 (taratura dei pesi): un numero per confrontare varianti dell'IA.

    Oggi: vittorie (pesate per il lato) meno 0.5 per ogni misplay certo, su tutte le partite completate.
    Un ottimizzatore futuro lancera le stesse partite con `variant` diverse (vedi pack_args) e confrontera
    questo valore per variante.
    """
    done = [g for g in games if g['esito'] in ('VITTORIA_G', 'VITTORIA_A', 'PAREGGIO', 'LIMITE_TURNI')]
    if not done:
        return None
    return round(sum(-0.5 * len(g['misplay']) for g in done) / len(done), 3)


def cmd_report(args=None):
    games = [load_json(p) for p in sorted(RESULTS.glob('*.json'))]
    games = [g for g in games if g]
    trainers = load_json(TRAINERS_JSON, []) or []
    names = {t['indice']: t['nome'] for t in trainers}
    REPORT.mkdir(parents=True, exist_ok=True)
    n = len(trainers) or (max([max(g['giocatore'], g['avversario']) for g in games], default=-1) + 1)

    cat_count, cat_doubt = {}, {}
    examples = {}
    outcomes = {}
    wins = {i: [0, 0] for i in range(n)}  # vittorie, partite
    problems = []
    turns = []
    for g in games:
        for x in g['misplay']:
            cat_count[x['categoria']] = cat_count.get(x['categoria'], 0) + 1
            examples.setdefault(x['categoria'], []).append((g, x))
        for x in g['dubbi']:
            cat_doubt[x['categoria']] = cat_doubt.get(x['categoria'], 0) + 1
        key = (g['giocatore'], g['avversario'])
        outcomes.setdefault(key, []).append(g)
        if g['esito'] in ('VITTORIA_G', 'VITTORIA_A', 'PAREGGIO', 'LIMITE_TURNI'):
            if g['turni'] is not None:
                turns.append(g['turni'])
            wins.setdefault(g['giocatore'], [0, 0])[1] += 1
            wins.setdefault(g['avversario'], [0, 0])[1] += 1
            if g['esito'] == 'VITTORIA_G':
                wins[g['giocatore']][0] += 1
            elif g['esito'] == 'VITTORIA_A':
                wins[g['avversario']][0] += 1
        else:
            problems.append(g)

    md = ['# Self-play VGC: riepilogo', '',
          f"Aggiornato: {now()}. Partite lette: {len(games)} (completate {len(games) - len(problems)}, "
          f"con problemi {len(problems)}).", '',
          'Ogni partita: lato G = allenatore della riga, lato A = allenatore della colonna; entrambi IA VGC con '
          'team preview propria e informazione chiusa (ognuno vede solo cio che ha osservato dell\'altro). '
          'Dettagli turno per turno in [partite/](partite/).', '']
    if turns:
        md.append(f"Turni per partita: media {sum(turns) / len(turns):.1f}, min {min(turns)}, max {max(turns)}.")
        md.append(f"Fitness (gancio M9, misplay certi per partita x -0.5): {fitness(games)}")
        md.append('')
    md.append('## Misplay per categoria')
    md.append('')
    md.append('| Categoria | Misplay certi | Casi dubbi (revisione) |')
    md.append('|---|---:|---:|')
    for cat in CATEGORIES:
        if cat_count.get(cat) or cat_doubt.get(cat):
            md.append(f"| {CATEGORIES[cat]} | {cat_count.get(cat, 0)} | {cat_doubt.get(cat, 0)} |")
    for cat in sorted(set(cat_count) | set(cat_doubt)):
        if cat not in CATEGORIES:
            md.append(f"| {cat} | {cat_count.get(cat, 0)} | {cat_doubt.get(cat, 0)} |")
    md.append(f"| **Totale** | **{sum(cat_count.values())}** | **{sum(cat_doubt.values())}** |")
    md.append('')
    md.append('## Risultati (riga = lato G, colonna = lato A)')
    md.append('')
    md.append('V = vince G, S = vince A, P = pareggio, L = limite turni, ! = blocco/crash, . = non giocata')
    md.append('')
    md.append('| G \\ A | ' + ' | '.join(str(j) for j in range(n)) + ' |')
    md.append('|---|' + '---|' * n)
    sym = {'VITTORIA_G': 'V', 'VITTORIA_A': 'S', 'PAREGGIO': 'P', 'LIMITE_TURNI': 'L'}
    for i in range(n):
        row = []
        for j in range(n):
            gs = outcomes.get((i, j), [])
            if not gs:
                row.append('.')
            else:
                row.append(''.join(f"[{sym.get(g['esito'], '!')}](partite/{g['id']}.md)" for g in gs))
        md.append(f"| {i} {names.get(i, '')} | " + ' | '.join(row) + ' |')
    md.append('')
    md.append('## Allenatori')
    md.append('')
    md.append('| # | Allenatore | Vittorie / partite | Misplay certi (propri) |')
    md.append('|---:|---|---|---:|')
    own = {}
    for g in games:
        for x in g['misplay']:
            t = g['giocatore'] if x['lato'] == 'G' else g['avversario']
            own[t] = own.get(t, 0) + 1
    for i in range(n):
        w, p = wins.get(i, [0, 0])
        md.append(f"| {i} | {names.get(i, '?')} | {w} / {p} | {own.get(i, 0)} |")
    md.append('')
    md.append('## Esempi di misplay')
    md.append('')
    for cat, lst in sorted(examples.items(), key=lambda kv: -len(kv[1])):
        md.append(f"### {CATEGORIES.get(cat, cat)} ({len(lst)})")
        for g, x in lst[:5]:
            who = g['nomi'].get(x['lato'], x['lato'])
            where = f"turno {x['turno']}" if x['turno'] else 'team preview'
            md.append(f"- [{g['id']}](partite/{g['id']}.md), {where}, {who}: {x.get('pokemon', '')} {x['dettaglio']}")
        md.append('')
    if problems:
        md.append('## Partite con problemi')
        md.append('')
        for g in problems:
            md.append(f"- [{g['id']}](partite/{g['id']}.md): {OUTCOME_TEXT.get(g['esito'], g['esito'])} (ultimo turno {g.get('ultimo_turno')})")
        md.append('')
    (REPORT / 'riepilogo.md').write_text('\n'.join(md) + '\n', encoding='utf-8')
    save_json(REPORT / 'riepilogo.json', {
        'aggiornato': now(), 'partite': len(games), 'problemi': [g['id'] for g in problems],
        'misplay_per_categoria': cat_count, 'dubbi_per_categoria': cat_doubt, 'fitness': fitness(games),
        'esiti': {g['id']: g['esito'] for g in games},
    })
    print(f"Report: {REPORT / 'riepilogo.md'}")
    return 0


# ---------------------------------------------------------------------------------------------
# Coda, demone e comandi di controllo
# ---------------------------------------------------------------------------------------------

def cmd_list(args=None):
    rom_bytes = ROM.read_bytes()
    off = find_args_offset(rom_bytes)
    run = start_game({'p': 0, 'e': 0, 'seed': 0, 'mode': MODE_LIST}, rom_bytes, off)
    run['proc'].wait(timeout=600)
    run['raw'].close()
    run['rom'].unlink()
    raw = LOGS / f"{run['gid']}.raw.txt"
    trainers = []
    for line in raw.read_text(encoding='utf-8', errors='replace').splitlines():
        line = line.replace('GBA Debug: ', '')
        m = re.match(r'^VSP ALLENATORE (\d+) (\d+) (.*)$', line)
        if m:
            trainers.append({'indice': int(m.group(1)), 'id': int(m.group(2)), 'nome': m.group(3).strip()})
    raw.unlink()
    save_json(TRAINERS_JSON, trainers)
    for t in trainers:
        print(f"{t['indice']:2d}  {t['nome']}  (id {t['id']})")
    if not trainers:
        print('Nessun allenatore VGC trovato nel ROM.')
        return 1
    return 0


def parse_matchups(spec, n):
    if spec in (None, '', 'all', 'tutte'):
        return [(p, e) for p in range(n) for e in range(n)]
    pairs = []
    for part in spec.split(','):
        p, e = part.split('-')
        pairs.append((int(p), int(e)))
    return pairs


def build_queue(opts, n):
    jobs = []
    for p, e in parse_matchups(opts.get('scontri'), n):
        for k in range(opts['semi']):
            seed = opts['seme_base'] + k * 1000 + p * n + e
            job = {'p': p, 'e': e, 'seed': seed, 'mode': opts['modo'], 'max_turns': opts['max_turni'], 'variant': opts.get('variante', 0)}
            if opts['modo'] == MODE_FIXED:
                job['player_order'] = opts['ordine_g']
                job['enemy_order'] = opts['ordine_a']
            if opts.get('scenario'):
                job['scenario'] = opts['scenario']
            job['id'] = game_id(job)
            job['stato'] = 'in_coda'
            jobs.append(job)
    return jobs


def cmd_start(args):
    WORK.mkdir(parents=True, exist_ok=True)
    if daemon_pid():
        print('Il demone sta gia girando (usa status / pause / stop).')
        return 0
    if not ROM.exists():
        print('ROM del self-play mancante: eseguo build.')
        if cmd_build(args) != 0:
            return 1
    trainers = load_json(TRAINERS_JSON) or []
    if not trainers:
        cmd_list(args)
        trainers = load_json(TRAINERS_JSON) or []
    n = len(trainers)
    state = load_json(STATE)
    opts = {'semi': args.seeds, 'seme_base': args.seed_base, 'modo': MODE_FIXED if args.fixed else MODE_PREVIEW,
            'max_turni': args.max_turns, 'scontri': args.matchups, 'parallelo': args.parallel, 'variante': args.variant,
            'ordine_g': [int(x) for x in args.order_g.split(',')] if args.order_g else None,
            'ordine_a': [int(x) for x in args.order_a.split(',')] if args.order_a else None,
            'scenario': json.loads(args.scenario) if args.scenario else None}
    if state and not args.new:
        # Ripresa: le partite non finite tornano in coda; si cambia solo il parallelismo.
        for j in state['partite']:
            if j['stato'] == 'in_corso':
                j['stato'] = 'in_coda'
        state['opzioni']['parallelo'] = args.parallel
        print(f"Ripresa della campagna del {state['creata']}: "
              f"{sum(1 for j in state['partite'] if j['stato'] == 'finita')}/{len(state['partite'])} partite finite.")
    else:
        state = {'creata': now(), 'opzioni': opts, 'partite': build_queue(opts, n)}
        print(f"Nuova campagna: {len(state['partite'])} partite ({n} allenatori).")
    state['stato'] = 'avvio'
    save_json(STATE, state)
    for flag in (PAUSE_FLAG, STOP_FLAG):
        if flag.exists():
            flag.unlink()
    spawn_daemon()
    return 0


def spawn_daemon():
    DAEMON_LOG.parent.mkdir(parents=True, exist_ok=True)
    log = open(DAEMON_LOG, 'a', encoding='utf-8')
    flags = 0
    if os.name == 'nt':
        flags = 0x00000008 | 0x00000200 | 0x08000000  # DETACHED_PROCESS | NEW_PROCESS_GROUP | NO_WINDOW
    exe = sys.executable
    p = subprocess.Popen([exe, str(Path(__file__).resolve()), 'daemon'], stdout=log, stderr=subprocess.STDOUT,
                         stdin=subprocess.DEVNULL, creationflags=flags, cwd=str(ROOT), close_fds=True)
    PIDFILE.write_text(str(p.pid))
    print(f'Demone avviato (pid {p.pid}). Stato: training.ps1 status')


def cmd_daemon(args):
    PIDFILE.write_text(str(os.getpid()))
    state = load_json(STATE)
    rom_bytes = ROM.read_bytes()
    off = find_args_offset(rom_bytes)
    parallel = max(1, int(state['opzioni'].get('parallelo', DEFAULT_PARALLEL)))
    running = []
    jobs = {j['id']: j for j in state['partite']}
    print(f'[{now()}] demone avviato, {parallel} istanze', flush=True)
    last_save = 0
    try:
        while True:
            # Partite finite
            for run in list(running):
                timed_out = time.time() - run['start'] > GAME_TIMEOUT_S
                if run['proc'].poll() is not None or timed_out:
                    if timed_out:
                        run['proc'].kill()
                        run['proc'].wait()
                    game = finish_game(run)
                    j = jobs[run['gid']]
                    j['stato'] = 'finita'
                    j['esito'] = game['esito']
                    j['misplay'] = len(game['misplay'])
                    j['dubbi'] = len(game['dubbi'])
                    j['turni'] = game['turni']
                    j['secondi'] = game['secondi']
                    running.remove(run)
                    print(f"[{now()}] {run['gid']}: {game['esito']} turni={game['turni']} misplay={len(game['misplay'])} "
                          f"({game['secondi']} s)", flush=True)
                    last_save = 0
            if STOP_FLAG.exists():
                for run in running:
                    run['proc'].kill()
                    run['proc'].wait()
                    run['raw'].close()
                    try:
                        run['rom'].unlink()
                        (LOGS / f"{run['gid']}.raw.txt").unlink()
                    except OSError:
                        pass
                    jobs[run['gid']]['stato'] = 'in_coda'
                running.clear()
                state['stato'] = 'fermato'
                save_json(STATE, state)
                STOP_FLAG.unlink()
                print(f'[{now()}] fermato', flush=True)
                break
            paused = PAUSE_FLAG.exists()
            pending = [j for j in state['partite'] if j['stato'] == 'in_coda']
            if not paused:
                while pending and len(running) < parallel:
                    j = pending.pop(0)
                    j['stato'] = 'in_corso'
                    j['inizio'] = now()
                    running.append(start_game(j, rom_bytes, off))
                    last_save = 0
            if not pending and not running:
                state['stato'] = 'completata'
                state['finita'] = now()
                save_json(STATE, state)
                print(f'[{now()}] campagna completata', flush=True)
                cmd_report()
                break
            state['stato'] = ('in pausa' if not running else 'in pausa (finisce le partite in corso)') if paused else 'in corso'
            if time.time() - last_save > 5:
                state['aggiornato'] = now()
                save_json(STATE, state)
                last_save = time.time()
            time.sleep(1)
    finally:
        try:
            if PIDFILE.exists() and PIDFILE.read_text().strip() == str(os.getpid()):
                PIDFILE.unlink()
        except OSError:
            pass
    return 0


def cmd_pause(args):
    WORK.mkdir(parents=True, exist_ok=True)
    PAUSE_FLAG.write_text(now())
    print('Pausa richiesta: le partite in corso finiscono, poi non ne partono altre. "resume" per continuare.')
    return 0


def cmd_resume(args):
    if PAUSE_FLAG.exists():
        PAUSE_FLAG.unlink()
    if daemon_pid():
        print('Ripresa.')
        return 0
    state = load_json(STATE)
    if not state:
        print('Nessuna campagna da riprendere: usa start.')
        return 1
    if state.get('stato') == 'completata':
        print('La campagna e gia completata: start --new per una nuova.')
        return 0
    for j in state['partite']:
        if j['stato'] == 'in_corso':
            j['stato'] = 'in_coda'
    save_json(STATE, state)
    spawn_daemon()
    return 0


def cmd_stop(args):
    if not daemon_pid():
        print('Il demone non gira.')
        if PAUSE_FLAG.exists():
            PAUSE_FLAG.unlink()
        return 0
    STOP_FLAG.write_text(now())
    for _ in range(60):
        if not daemon_pid():
            break
        time.sleep(1)
    print('Fermato. Le partite non finite restano in coda: "resume" (o "start") per continuare.')
    return 0


def cmd_status(args):
    state = load_json(STATE)
    pid = daemon_pid()
    if not state:
        print('Nessuna campagna. Usa: training.ps1 start')
        return 0
    jobs = state['partite']
    done = [j for j in jobs if j['stato'] == 'finita']
    running = [j for j in jobs if j['stato'] == 'in_corso']
    stato = state.get('stato', '?')
    if not pid and stato in ('in corso', 'avvio') :
        stato = 'demone non attivo (interrotto?) - "resume" per continuare'
    if PAUSE_FLAG.exists() and pid:
        stato = 'in pausa' + (' (finisce le partite in corso)' if running else '')
    print(f"Campagna del {state['creata']}: {stato}")
    print(f"Partite: {len(done)}/{len(jobs)} finite, {len(running)} in corso, "
          f"{sum(1 for j in jobs if j['stato'] == 'in_coda')} in coda. Parallelismo: {state['opzioni'].get('parallelo')}")
    if done:
        esiti = {}
        for j in done:
            esiti[j.get('esito')] = esiti.get(j.get('esito'), 0) + 1
        print('Esiti: ' + ', '.join(f"{k}={v}" for k, v in sorted(esiti.items(), key=lambda kv: str(kv[0]))))
        print(f"Misplay certi: {sum(j.get('misplay', 0) for j in done)}, casi dubbi: {sum(j.get('dubbi', 0) for j in done)}")
        secs = [j['secondi'] for j in done if j.get('secondi')]
        if secs:
            par = max(1, int(state['opzioni'].get('parallelo', 1)))
            left = len(jobs) - len(done)
            eta = left * (sum(secs) / len(secs)) / par / 60
            print(f"Durata media partita: {sum(secs) / len(secs):.0f} s; stima al termine: ~{eta:.0f} min")
    for j in running:
        print(f"  in corso: {j['id']} (dalle {j.get('inizio', '?')})")
    print(f"Report: {REPORT / 'riepilogo.md'}")
    return 0


def cmd_run_one(args):
    if not ROM.exists():
        print('ROM mancante: build')
        return 1
    job = {'p': args.p, 'e': args.e, 'seed': args.seed, 'mode': MODE_FIXED if args.fixed else MODE_PREVIEW,
           'max_turns': args.max_turns,
           'player_order': [int(x) for x in args.order_g.split(',')] if args.order_g else None,
           'enemy_order': [int(x) for x in args.order_a.split(',')] if args.order_a else None,
           'scenario': json.loads(args.scenario) if args.scenario else None}
    t0 = time.time()
    g = run_one(job)
    print(f"{g['id']}: {g['esito']} turni={g['turni']} misplay={len(g['misplay'])} dubbi={len(g['dubbi'])} ({time.time() - t0:.0f} s)")
    print(f"Log: {LOGS / (g['id'] + '.log')}\nReport: {REPORT / 'partite' / (g['id'] + '.md')}")
    return 0


def main():
    ap = argparse.ArgumentParser(description='Self-play VGC (IA contro IA)')
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('build')
    sub.add_parser('list')
    st = sub.add_parser('start')
    st.add_argument('--parallel', type=int, default=DEFAULT_PARALLEL, help='istanze mGBA in parallelo (default 24)')
    st.add_argument('--seeds', type=int, default=1, help='partite (semi) per scontro')
    st.add_argument('--seed-base', type=int, default=1, help='seme di partenza')
    st.add_argument('--matchups', default='all', help='"all" oppure elenco G-A, es. 0-1,3-5')
    st.add_argument('--max-turns', type=int, default=50)
    st.add_argument('--fixed', action='store_true', help='lead/squadre fissate (--order-g / --order-a)')
    st.add_argument('--order-g', help='slot 0..5 del lato G in ordine (lead prima), es. 0,1,2,3')
    st.add_argument('--order-a', help='slot 0..5 del lato A')
    st.add_argument('--scenario', help='posizione preparata in JSON, es. {"turno":3,"tailwind":[0,1],"stadi":[[0,0,0,2,0],[0]*5...]}')
    st.add_argument('--variant', type=int, default=0, help='variante dei pesi (M9, riservato)')
    st.add_argument('--new', action='store_true', help='nuova campagna anche se ce n\'e una in corso')
    sub.add_parser('daemon')
    sub.add_parser('pause')
    sub.add_parser('resume')
    sub.add_parser('stop')
    sub.add_parser('status')
    sub.add_parser('report')
    ro = sub.add_parser('run-one')
    ro.add_argument('p', type=int)
    ro.add_argument('e', type=int)
    ro.add_argument('seed', type=int, nargs='?', default=1)
    ro.add_argument('--max-turns', type=int, default=50)
    ro.add_argument('--fixed', action='store_true')
    ro.add_argument('--order-g')
    ro.add_argument('--order-a')
    ro.add_argument('--scenario')
    args = ap.parse_args()
    handlers = {'build': cmd_build, 'list': cmd_list, 'start': cmd_start, 'daemon': cmd_daemon, 'pause': cmd_pause,
                'resume': cmd_resume, 'stop': cmd_stop, 'status': cmd_status, 'report': cmd_report, 'run-one': cmd_run_one}
    sys.exit(handlers[args.cmd](args) or 0)


if __name__ == '__main__':
    main()

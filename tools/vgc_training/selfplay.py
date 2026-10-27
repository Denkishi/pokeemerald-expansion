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
  tune                  taratura SPSA dei pesi con validazione finale (M9)
  valuta --weights ...  pesi dati contro i default su semi mai visti
  esplora               partite esplorative per il libro dei piani
  libro                 rigenera src/data/vgc_plan_book.h dall'esplorazione
  verifica-libro        libro acceso per un lato e spento per l'altro, su semi mai visti
  verifica-contesto     contesto della squadra avversaria acceso per un lato e spento per l'altro, a specchio
  bench                 partite all'ora con N istanze

Guida: docs/ia_vgc/training.md
"""
import argparse
import ctypes
import datetime
import json
import math
import os
import random
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
ARGS_LAYOUT = '<16sBBBBI4B4BB2BB4B20bB7BBBBBBB2x'
ARGS_SIZE = struct.calcsize(ARGS_LAYOUT)
assert ARGS_SIZE == 76
MODE_LIST, MODE_PREVIEW, MODE_FIXED = 0, 1, 2

DEFAULT_PARALLEL = 24
# Pesi tarabili (ordine di reserved[0..6] nei parametri; deve combaciare con AI_VgcSetEvalWeightPct nel ROM).
WEIGHT_NAMES = ['KO', 'VIVO', 'VELOCITA', 'MINACCE', 'MATERIALE', 'CAMBIO', 'WINCON']
GAME_TIMEOUT_S = 45 * 60
MAX_RAW_LOG = 300 * 1024 * 1024  # un log che cresce oltre (es. codice impazzito che stampa in ciclo) ferma la partita


def runaway(run):
    try:
        return (LOGS / f"{run['gid']}.raw.txt").stat().st_size > MAX_RAW_LOG
    except OSError:
        return False


def game_over(run):
    """La partita ha scritto VSP FINE: il risultato c'è, mGBA si può chiudere (dopo, la chiusura del test nel ROM
    può inciampare nella heap e stampare codice illegale all'infinito)."""
    path = LOGS / f"{run['gid']}.raw.txt"
    try:
        size = path.stat().st_size
        pos = run.get('scan', 0)
        if size <= pos:
            return False
        with open(path, 'rb') as f:
            f.seek(max(0, pos - 64))
            chunk = f.read(min(size - max(0, pos - 64), 8 * 1024 * 1024))
        run['scan'] = size
        return b'VSP FINE' in chunk
    except OSError:
        return False

CATEGORIES = {
    'IMMUNE': 'Mossa su un immune noto',
    'BERSAGLIO_GIA_KO': 'Bersaglio gia KO senza redirect utile',
    'KO_NON_PRESO': 'KO garantito non preso',
    'KO_NON_PRESO_LETTURA': 'KO non preso: lettura di un Protect visto',
    'KO_NON_PRESO_AREA': 'KO non preso: scelta una mossa ad area',
    'KO_NON_PRESO_SUPPORTO': 'KO non preso: scelto supporto/Protect/cambio',
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
    # Su Windows il file di destinazione può essere aperto un attimo da chi lo legge (status, antivirus): si riprova.
    for attempt in range(50):
        try:
            os.replace(tmp, path)
            return
        except PermissionError:
            time.sleep(0.1 * (attempt + 1))
    os.replace(tmp, path)


def retry_io(fn, tries=60):
    """Su Windows un file appena lasciato da mGBA (processo chiuso da poco) o letto dall'antivirus può restare
    bloccato per qualche istante: si riprova."""
    for attempt in range(tries):
        try:
            return fn()
        except PermissionError:
            if attempt == tries - 1:
                raise
            time.sleep(0.1 + 0.05 * attempt)


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


def pack_args(mode, player, enemy, seed, max_turns=0, player_order=None, enemy_order=None, scenario=None, verbose=0, weights=None,
              weight_side=0, book_off=0, explore=None, quiet=0, context_off=0):
    po = list(player_order or [0xFF] * 4)
    eo = list(enemy_order or [0xFF] * 4)
    sc = scenario or {}
    tw = sc.get('tailwind', [0, 0])
    hp = sc.get('hp', [0, 0, 0, 0])
    stages = sc.get('stadi', [[0] * 5 for _ in range(4)])
    flat = [int(v) for row in stages for v in row]
    # reserved[0..6]: pesi della valutazione in % (diviso 2; 0 = default) del lato weight_side (0 = G, 1 = A), vedi
    # WEIGHT_NAMES e AI_VgcSetEvalWeightPct nel ROM. L'altro lato resta ai valori di default (termine di paragone).
    ex = list(explore or [0, 0])
    w = list(weights or [])
    reserved = [max(0, min(255, int(round(x / 2)))) if x else 0 for x in (w + [0] * 7)[:7]]
    return struct.pack(ARGS_LAYOUT, MAGIC, mode, player, enemy, max_turns & 0xFF, seed & 0xFFFFFFFF,
                       *po, *eo, sc.get('turno', 0), tw[0], tw[1], sc.get('trickroom', 0), *hp, *flat, verbose, *reserved,
                       1 if weight_side else 0, int(book_off) & 3, int(ex[0]) & 0xFF, int(ex[1]) & 0xFF, 1 if quiet else 0,
                       int(context_off) & 3)


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
    if job.get('pesi'):
        base += '_p' + '-'.join(str(int(x)) for x in job['pesi'])
        if job.get('lato_pesi'):
            base += '_wA'
    if job.get('libro_off'):
        base += f"_b{job['libro_off']}"
    if job.get('contesto_off'):
        base += f"_c{job['contesto_off']}"
    if any(job.get('esplora') or []):
        base += '_x' + '-'.join(str(int(x)) for x in job['esplora'])
    if job.get('tag'):
        base = f"{job['tag']}_{base}"
    return base


_SLOTS_WRITTEN = set()


def slot_rom(slot, rom_bytes):
    """ROM di un posto (istanza mGBA): copiato una volta per processo, poi a ogni partita si riscrivono solo i
    parametri (76 byte) al loro posto, invece di scrivere 32 MB di ROM per partita."""
    path = TMP / f'posto_{slot}.gba'
    if slot not in _SLOTS_WRITTEN or not path.exists() or path.stat().st_size != len(rom_bytes):
        retry_io(lambda: path.write_bytes(rom_bytes))
        _SLOTS_WRITTEN.add(slot)
    return path


def start_game(job, rom_bytes, args_offset, slot='uno'):
    TMP.mkdir(parents=True, exist_ok=True)
    LOGS.mkdir(parents=True, exist_ok=True)
    gid = game_id(job)
    packed = pack_args(job.get('mode', MODE_PREVIEW), job['p'], job['e'], job['seed'], job.get('max_turns', 0),
                       job.get('player_order'), job.get('enemy_order'), job.get('scenario'), job.get('verbose', 0),
                       job.get('pesi'), job.get('lato_pesi', 0), job.get('libro_off', 0), job.get('esplora'),
                       job.get('silenzio', 0), job.get('contesto_off', 0))
    rom_path = slot_rom(slot, rom_bytes)

    def patch():
        with open(rom_path, 'r+b') as f:
            f.seek(args_offset)
            f.write(packed)
    retry_io(patch)
    raw_path = LOGS / f'{gid}.raw.txt'
    if raw_path.exists():
        retry_io(raw_path.unlink)
    # Log scritto da mGBA direttamente su file (logToFile): lo stdout rediretto è bufferizzato e si perde se il
    # processo viene ucciso; il file invece cresce riga per riga e si può seguire mentre la partita gira.
    raw = open(os.devnull, 'wb')
    flags = 0x08000000 if os.name == 'nt' else 0  # CREATE_NO_WINDOW
    proc = subprocess.Popen([str(MGBA), '-l15', '-ClogLevel.gba.dma=16', '-ClogToFile=1', f'-ClogFile={raw_path}',
                             '-Rr0', str(rom_path)],
                            stdout=raw, stderr=subprocess.STDOUT, env=mgba_env(), creationflags=flags)
    return {'proc': proc, 'raw': raw, 'rom': rom_path, 'gid': gid, 'start': time.time(), 'job': job, 'slot': slot}


def finish_game(run, killed=False):
    run['raw'].close()
    gid = run['gid']
    raw_path = LOGS / f'{gid}.raw.txt'
    lines = []
    leaf = 0
    if not raw_path.exists():
        raw_path.write_text('')
    with open(raw_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if line.startswith('GBA Debug: '):
                line = line[len('GBA Debug: '):]
            if line.startswith('VGCLEAF'):
                leaf += 1
                continue
            if line.startswith('GBA: Illegal opcode'):
                continue
            lines.append(line)
    retry_io(raw_path.unlink)
    job = run['job']
    game = parse_game(lines, job)
    game['vgcleaf_righe'] = leaf
    game['secondi'] = round(time.time() - run['start'], 1)
    if killed and game['esito'] in ('INCOMPLETA', None):
        game['esito'] = 'INTERROTTA'
    ok = game['esito'] in ('VITTORIA_G', 'VITTORIA_A', 'PAREGGIO', 'LIMITE_TURNI')
    if job.get('silenzio'):
        # Partite di allenamento (taratura, esplorazione, verifiche): niente report per partita; il log resta solo
        # se la partita è andata storta; i dati per il libro dei piani in work/dati/<cartella>/ se richiesti.
        if not ok:
            (LOGS / f'{gid}.log').write_text('\n'.join(lines) + '\n', encoding='utf-8')
        if job.get('dati'):
            game.pop('ultime_righe', None)
            save_json(WORK / 'dati' / job['dati'] / f'{gid}.json', game)
        return game
    (LOGS / f'{gid}.log').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    if job.get('tag') == 'bench':
        return game  # benchmark con log completo: niente report né risultati della campagna
    save_json(RESULTS / f'{gid}.json', game)
    write_game_report(game, lines)
    return game


def run_one(job):
    rom_bytes = ROM.read_bytes()
    off = find_args_offset(rom_bytes)
    run = start_game(job, rom_bytes, off, 'uno')
    while run['proc'].poll() is None:
        if time.time() - run['start'] > GAME_TIMEOUT_S or runaway(run) or game_over(run):
            run['proc'].kill()
            run['proc'].wait()
            break
        time.sleep(1)
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
RE_CHIAVE = re.compile(r'^VSP CHIAVE ([GA]) squadra=(\d+) classe=(\d+) piano=(-?\d+) esplora=(\d+) specie=([\d,]+)')
RE_SCELTA = re.compile(r'^VSP SCELTA t(\d+) b(\d+) p(\d+) m(\d+)')


def side_of(b):
    return 'G' if int(b) % 2 == 0 else 'A'


def parse_game(lines, job):
    g = {'id': game_id(job), 'giocatore': job['p'], 'avversario': job['e'], 'seme': job['seed'],
         'modo': job.get('mode', MODE_PREVIEW), 'nomi': {}, 'esito': 'INCOMPLETA', 'turni': None,
         'restanti': None, 'misplay': [], 'dubbi': [], 'tp': {'G': {}, 'A': {}}, 'set': {'G': {}, 'A': {}},
         'mosse_id': {}, 'ultime_righe': lines[-25:], 'chiave': {}, 'scelte': []}
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
        m = RE_CHIAVE.match(line)
        if m:
            g['chiave'][m.group(1)] = {'squadra': int(m.group(2)), 'classe': int(m.group(3)), 'piano': int(m.group(4)),
                                       'esplora': int(m.group(5)), 'specie': [int(x) for x in m.group(6).split(',')]}
            continue
        m = RE_SCELTA.match(line)
        if m:
            g['scelte'].append([int(m.group(i)) for i in range(1, 5)])
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
    g['misplay_lato'] = {s: sum(1 for x in g['misplay'] if x['lato'] == s) for s in ('G', 'A')}
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
                best = sorted(mine, key=lambda m: -int(m.group(9)))
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
    # Le pagine per partita si rifanno da capo: quelle di campagne precedenti gonfiavano i conteggi.
    for old in (REPORT / 'partite').glob('*.md'):
        try:
            old.unlink()
        except OSError:
            pass
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
    run = start_game({'p': 0, 'e': 0, 'seed': 0, 'mode': MODE_LIST}, rom_bytes, off, 'elenco')
    run['proc'].wait(timeout=600)
    run['raw'].close()
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
            job = {'p': p, 'e': e, 'seed': seed, 'mode': opts['modo'], 'max_turns': opts['max_turni']}
            if opts.get('pesi'):
                job['pesi'] = opts['pesi']
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
            'max_turni': args.max_turns, 'scontri': args.matchups, 'parallelo': args.parallel,
            'pesi': [int(x) for x in args.weights.split(',')] if args.weights else None,
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
        # Nuova campagna: via i risultati e i log della precedente (report e libro dei piani leggono solo questa).
        for old in list(RESULTS.glob('*.json')) + list(LOGS.glob('*.log')) + list(LOGS.glob('*.raw.txt')):
            try:
                old.unlink()
            except OSError:
                pass
        state = {'creata': now(), 'opzioni': opts, 'partite': build_queue(opts, n)}
        print(f"Nuova campagna: {len(state['partite'])} partite ({n} allenatori).")
    state['stato'] = 'avvio'
    save_json(STATE, state)
    MODE_FILE.write_text('campagna')
    for flag in (PAUSE_FLAG, STOP_FLAG):
        if flag.exists():
            flag.unlink()
    spawn_daemon()
    return 0


def spawn_daemon(command='daemon'):
    DAEMON_LOG.parent.mkdir(parents=True, exist_ok=True)
    log = open(DAEMON_LOG, 'a', encoding='utf-8')
    flags = 0
    if os.name == 'nt':
        flags = 0x00000008 | 0x00000200 | 0x08000000  # DETACHED_PROCESS | NEW_PROCESS_GROUP | NO_WINDOW
    exe = sys.executable
    p = subprocess.Popen([exe, str(Path(__file__).resolve()), command], stdout=log, stderr=subprocess.STDOUT,
                         stdin=subprocess.DEVNULL, creationflags=flags, cwd=str(ROOT), close_fds=True)
    PIDFILE.write_text(str(p.pid))
    print(f'Demone avviato (pid {p.pid}). Stato: training.ps1 status')


IN_TUNE = False  # True quando il demone gira dentro la taratura (non toglie il pidfile alla fine)


def cmd_daemon(args):
    PIDFILE.write_text(str(os.getpid()))
    state = load_json(STATE)
    rom_path = Path(state.get('rom') or ROM)
    rom_bytes = rom_path.read_bytes()
    off = find_args_offset(rom_bytes)
    parallel = max(1, int(state['opzioni'].get('parallelo', DEFAULT_PARALLEL)))
    free_slots = list(range(parallel))
    running = []
    jobs = {j['id']: j for j in state['partite']}
    print(f'[{now()}] demone avviato, {parallel} istanze', flush=True)
    last_save = 0
    try:
        while True:
            # Partite finite
            for run in list(running):
                timed_out = time.time() - run['start'] > GAME_TIMEOUT_S or runaway(run) or game_over(run)
                if run['proc'].poll() is not None or timed_out:
                    if timed_out:
                        run['proc'].kill()
                        run['proc'].wait()
                    game = finish_game(run)
                    free_slots.append(run['slot'])
                    j = jobs[run['gid']]
                    j['stato'] = 'finita'
                    j['esito'] = game['esito']
                    j['misplay'] = len(game['misplay'])
                    j['misplay_lato'] = game['misplay_lato']
                    j['dubbi'] = len(game['dubbi'])
                    j['turni'] = game['turni']
                    j['secondi'] = game['secondi']
                    running.remove(run)
                    if not j.get('silenzio'):
                        print(f"[{now()}] {run['gid']}: {game['esito']} turni={game['turni']} misplay={len(game['misplay'])} "
                              f"({game['secondi']} s)", flush=True)
                    last_save = 0
            if STOP_FLAG.exists():
                for run in running:
                    run['proc'].kill()
                    run['proc'].wait()
                    run['raw'].close()
                    try:
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
                while pending and len(running) < parallel and free_slots:
                    j = pending.pop(0)
                    j['stato'] = 'in_corso'
                    j['inizio'] = now()
                    running.append(start_game(j, rom_bytes, off, free_slots.pop(0)))
                    last_save = 0
            if not pending and not running:
                state['stato'] = 'completata'
                state['finita'] = now()
                save_json(STATE, state)
                print(f'[{now()}] campagna completata', flush=True)
                if state['opzioni'].get('report', True):
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
            if not IN_TUNE and PIDFILE.exists() and PIDFILE.read_text().strip() == str(os.getpid()):
                PIDFILE.unlink()
        except OSError:
            pass
    return 0


# Allenamento automatico (M9): esperimenti fatti di una o più campagne, guidati da un demone che sopravvive a
# pausa / stop / riavvio (stato in work/esperimento.json, campagna corrente in work/state.json).
#   taratura        SPSA sui pesi della valutazione (CUSTOM_VGC_EVAL_*) contro i pesi di default, poi validazione
#   esplorazione    partite con scelte esplorative (team preview e turni 1/2) per il libro dei piani
#   verifica-libro  libro dei piani acceso per un lato e spento per l'altro, su semi mai visti
#   valuta          pesi dati contro i default, su semi mai visti
# Disegno "a specchio": ogni scontro (G, A, seme) si gioca due volte, con il trattamento (pesi in prova o libro)
# una volta sul lato G e una sul lato A; l'unità statistica è la coppia di partite (0, 0.5, 1 vittorie del
# trattamento per partita, media della coppia), così il vantaggio di lato si annulla.
# ---------------------------------------------------------------------------------------------
TUNE = WORK / 'tuning.json'  # (formato della taratura precedente, solo lettura)
EXP = WORK / 'esperimento.json'
EXP_ROM = WORK / 'esperimento.gba'
MODE_FILE = WORK / 'modo.txt'
SEED_BASE = {'taratura': 7, 'validazione': 500001, 'esplorazione': 100001, 'libro': 900001}


def norm_cdf(z):
    return 0.5 * math.erfc(-z / math.sqrt(2))


def treat_score(j):
    """Vittoria (1), pareggio/limite (0.5) o sconfitta (0) del lato con il trattamento; None se la partita non vale."""
    side = 'G' if not j.get('lato') else 'A'
    e = j.get('esito')
    if e in ('PAREGGIO', 'LIMITE_TURNI'):
        return 0.5
    if e == 'VITTORIA_G':
        return 1.0 if side == 'G' else 0.0
    if e == 'VITTORIA_A':
        return 1.0 if side == 'A' else 0.0
    return None


def paired_stats(jobs, penalty=0.0):
    """Tasso di vittoria del trattamento sulle coppie a specchio complete, con errore standard, IC 95% e p-value
    (H0: 50%, z bilaterale sulla media delle coppie)."""
    pairs, bad, mis, games = {}, 0, 0, 0
    for j in jobs:
        if j.get('stato') != 'finita':
            continue
        sc = treat_score(j)
        if sc is None:
            bad += 1
            continue
        games += 1
        mis += (j.get('misplay_lato') or {}).get('G' if not j.get('lato') else 'A', 0)
        pairs.setdefault((j['p'], j['e'], j['seed']), []).append(sc)
    vals = [sum(v) / 2 for v in pairs.values() if len(v) == 2]
    n = len(vals)
    if n == 0:
        return {'coppie': 0, 'partite': games, 'non_valide': bad}
    m = sum(vals) / n
    sd = math.sqrt(sum((v - m) ** 2 for v in vals) / max(1, n - 1))
    se = sd / math.sqrt(n) if n > 1 else 0.5
    z = (m - 0.5) / se if se > 0 else 0.0
    mpg = mis / max(1, games)
    return {'coppie': n, 'partite': games, 'non_valide': bad, 'vittorie': round(m, 4), 'es': round(se, 4),
            'ic95': [round(m - 1.96 * se, 4), round(m + 1.96 * se, 4)], 'z': round(z, 2),
            'p': round(2 * (1 - norm_cdf(abs(z))), 5), 'misplay_partita': round(mpg, 3),
            'fitness': round(m - penalty * mpg, 4)}


def mirrored_jobs(n, matchups, seeds, seed_base, tag, weights=None, book=False, explore=None, data_dir=None, extra=None,
                  context=False):
    jobs = []
    for p, e in parse_matchups(matchups, n):
        for k in range(seeds):
            seed = seed_base + k * 1000 + p * n + e
            for side in ((0, 1) if (weights is not None or book or context) else (0,)):
                job = {'p': p, 'e': e, 'seed': seed, 'mode': MODE_PREVIEW, 'max_turns': 50, 'silenzio': 1, 'tag': tag,
                       'lato': side, 'libro_off': 3}
                if weights is not None:
                    job['pesi'] = [int(x) for x in weights]
                    job['lato_pesi'] = side
                if book:
                    job['libro_off'] = 2 if side == 0 else 1  # libro acceso solo per il lato del trattamento
                if context:
                    job['contesto_off'] = 2 if side == 0 else 1  # contesto della squadra acceso solo per il trattamento
                if explore:
                    job['esplora'] = [explore, explore]
                if data_dir:
                    job['dati'] = data_dir
                if extra:
                    job.update(extra)
                job['id'] = game_id(job)
                job['stato'] = 'in_coda'
                jobs.append(job)
    return jobs


def even_clip(x, lo=50, hi=250):
    return int(max(lo, min(hi, 2 * round(x / 2))))


def spsa_point(x):
    k = x['k']
    rng = random.Random(1000 + k)
    delta = [rng.choice((-1, 1)) for _ in WEIGHT_NAMES]
    ck = x['c'] / (k + 1) ** x['gamma']
    plus = [even_clip(th + ck * d) for th, d in zip(x['theta'], delta)]
    minus = [even_clip(th - ck * d) for th, d in zip(x['theta'], delta)]
    return delta, ck, plus, minus


def polyak_theta(x):
    """Media degli iterati della seconda metà della taratura (più stabile dell'ultimo iterato rumoroso)."""
    hist = [h['theta'] for h in x['storia']]
    if not hist:
        return [even_clip(t) for t in x['theta']]
    tail = hist[len(hist) // 2:]
    return [even_clip(sum(h[i] for h in tail) / len(tail)) for i in range(len(WEIGHT_NAMES))]


def exp_next(x, n):
    """Prossima campagna dell'esperimento: (etichetta, partite) oppure None se è finito."""
    tipo = x['tipo']
    if tipo == 'taratura':
        if x['fase'] == 'spsa' and x['k'] < x['iterazioni']:
            delta, ck, plus, minus = spsa_point(x)
            tag = f"k{x['k']}"
            jobs = (mirrored_jobs(n, x['scontri'], x['semi'], x['seme_base'], tag + 'p', weights=plus)
                    + mirrored_jobs(n, x['scontri'], x['semi'], x['seme_base'], tag + 'm', weights=minus))
            # Le due metà si alternano nella coda: pausa o stop a metà lasciano + e - allo stesso punto.
            half = len(jobs) // 2
            jobs = [j for pair in zip(jobs[:half], jobs[half:]) for j in pair]
            return tag, jobs
        if x['fase'] == 'spsa':
            x['fase'] = 'validazione'
            x['candidato'] = polyak_theta(x)
        if x['fase'] == 'validazione':
            return 'val', mirrored_jobs(n, x['scontri'], x['semi_val'], x['seme_base_val'], 'val', weights=x['candidato'])
        return None
    if tipo == 'valutazione':
        if x.get('risultato'):
            return None
        return 'val', mirrored_jobs(n, x['scontri'], x['semi'], x['seme_base'], 'val', weights=x['pesi'])
    if tipo == 'esplorazione':
        if x.get('risultato'):
            return None
        return 'esp', mirrored_jobs(n, x['scontri'], x['semi'], x['seme_base'], 'esp', explore=x['esplora'],
                                    data_dir=x['cartella'])
    if tipo == 'verifica_contesto':
        if x.get('risultato'):
            return None
        return 'contesto', mirrored_jobs(n, x['scontri'], x['semi'], x['seme_base'], 'contesto', context=True,
                                         data_dir=x.get('cartella'))
    if tipo == 'verifica_libro':
        if x.get('risultato'):
            return None
        return 'libro', mirrored_jobs(n, x['scontri'], x['semi'], x['seme_base'], 'libro', book=True,
                                      data_dir=x.get('cartella'))
    return None


def exp_done(x, tag, state):
    jobs = [j for j in state['partite'] if j['stato'] == 'finita']
    secs = time.time() - x.get('_t0', time.time())
    x.setdefault('campagne', []).append({'tag': tag, 'partite': len(jobs), 'finita': now()})
    if x['tipo'] == 'taratura' and tag.startswith('k'):
        delta, ck, plus, minus = spsa_point(x)
        sp = paired_stats([j for j in jobs if j['tag'] == tag + 'p'], x['penalita'])
        sm = paired_stats([j for j in jobs if j['tag'] == tag + 'm'], x['penalita'])
        fp, fm = sp.get('fitness', 0.5), sm.get('fitness', 0.5)
        k = x['k']
        ak = x['a0'] * ((1 + x['A']) / (k + 1 + x['A'])) ** x['alpha']
        new = []
        for th, d, hi, lo in zip(x['theta'], delta, plus, minus):
            step = ak * (fp - fm) / (hi - lo) if hi != lo else 0.0  # gradiente SPSA, salita
            step = max(-x['passo_max'], min(x['passo_max'], step))
            new.append(max(50.0, min(250.0, th + step)))
        x['theta'] = new
        x['storia'].append({'passo': k + 1, 'piu': plus, 'meno': minus, 'f_piu': fp, 'f_meno': fm,
                            'v_piu': sp.get('vittorie'), 'v_meno': sm.get('vittorie'), 'coppie': sp.get('coppie'),
                            'theta': [round(t, 1) for t in new], 'quando': now()})
        print(f"[{now()}] passo {k + 1}/{x['iterazioni']}: f+={fp:.4f} f-={fm:.4f} (vittorie {sp.get('vittorie')} / "
              f"{sm.get('vittorie')}) theta={[round(t) for t in new]}", flush=True)
        x['k'] = k + 1
    elif x['tipo'] == 'taratura' and tag == 'val':
        st = paired_stats(jobs, x['penalita'])
        x['validazione'] = st
        x['significativo'] = bool(st.get('coppie') and st['ic95'][0] > 0.5)
        x['fase'] = 'fine'
        x['stato'] = 'finita'
    else:
        st = paired_stats(jobs)
        x['risultato'] = st
        x['stato'] = 'finita'
    exp_report(x)


def exp_report(x):
    REPORT.mkdir(parents=True, exist_ok=True)
    if x['tipo'] == 'taratura':
        out = ['# Taratura automatica dei pesi (SPSA)', '',
               f"Aggiornato: {now()}. Passi: {x['k']}/{x['iterazioni']} (fase: {x['fase']}, {x['stato']}).", '',
               f"Disegno: per ogni passo, pesi theta+c*delta e theta-c*delta (delta = +-1 per peso) giocati sugli stessi "
               f"{x['semi']} semi fissi (base {x['seme_base']}) per tutti gli scontri ({x['scontri']}), ognuno a specchio "
               f"(pesi in prova sul lato G e poi sul lato A, l'altro lato ai pesi di default). Obiettivo: tasso di vittoria "
               f"dei pesi in prova - {x['penalita']} x misplay certi per partita. c={x['c']}, a0={x['a0']}, A={x['A']}.", '',
               '| Peso | Valore attuale (%) | Candidato (media seconda metà) |', '|---|---:|---:|']
        cand = x.get('candidato') or polyak_theta(x)
        for name, th, cd in zip(WEIGHT_NAMES, x['theta'], cand):
            out.append(f'| {name} | {th:.0f} | {cd} |')
        out += ['', '| Passo | f(+) | f(-) | Vittorie + / - | Pesi dopo il passo |', '|---:|---:|---:|---|---|']
        for h in x['storia']:
            out.append(f"| {h['passo']} | {h['f_piu']:.4f} | {h['f_meno']:.4f} | {h['v_piu']} / {h['v_meno']} | "
                       f"{', '.join(str(round(t)) for t in h['theta'])} |")
        if x.get('validazione'):
            v = x['validazione']
            out += ['', '## Validazione su semi mai visti', '',
                    f"Candidato {x['candidato']} contro i pesi di default, {x['semi_val']} semi (base {x['seme_base_val']}), "
                    f"a specchio: {v.get('coppie')} coppie ({v.get('partite')} partite valide, {v.get('non_valide')} non valide).",
                    f"Tasso di vittoria del candidato: **{v.get('vittorie')}** (IC 95% {v.get('ic95')}, z={v.get('z')}, "
                    f"p={v.get('p')}); misplay certi per partita del candidato: {v.get('misplay_partita')}.",
                    f"Miglioramento significativo (IC sopra il 50%): **{'sì' if x.get('significativo') else 'no'}**."]
        out += ['', 'Per applicare i pesi: moltiplicare i `CUSTOM_VGC_EVAL_*` corrispondenti in `include/config/custom.h` '
                'per il valore / 100 (WINCON: 100 + (150-100) x valore / 100).']
        (REPORT / 'taratura.md').write_text('\n'.join(out) + '\n', encoding='utf-8')
    else:
        name = {'valutazione': 'valutazione_pesi', 'esplorazione': 'esplorazione', 'verifica_libro': 'verifica_libro',
                'verifica_contesto': 'verifica_contesto'}[x['tipo']]
        r = x.get('risultato') or {}
        out = [f"# {x['tipo'].replace('_', ' ').capitalize()}", '', f"Aggiornato: {now()} ({x['stato']}).", '',
               '```', json.dumps({k: v for k, v in x.items() if not k.startswith('_') and k not in ('campagne',)},
                                 ensure_ascii=False, indent=1), '```']
        if r.get('coppie'):
            out += ['', f"Tasso di vittoria del trattamento: **{r['vittorie']}** su {r['coppie']} coppie a specchio "
                        f"(IC 95% {r['ic95']}, z={r['z']}, p={r['p']})."]
        (REPORT / f'{name}.md').write_text('\n'.join(out) + '\n', encoding='utf-8')


def exp_create(x):
    WORK.mkdir(parents=True, exist_ok=True)
    if daemon_pid():
        print('Il demone sta gia girando (usa status / pause / stop).')
        return 1
    if not ROM.exists():
        print('ROM del self-play mancante: eseguo build.')
        if cmd_build(None) != 0:
            return 1
    shutil.copyfile(ROM, EXP_ROM)  # il ROM resta quello dell'avvio anche se nel frattempo si ricompila
    x.update({'creato': now(), 'stato': 'in corso', 'rom': str(EXP_ROM)})
    save_json(EXP, x)
    if STATE.exists():
        STATE.unlink()
    return exp_launch()


def exp_launch():
    MODE_FILE.write_text('esperimento')
    for flag in (PAUSE_FLAG, STOP_FLAG):
        if flag.exists():
            flag.unlink()
    spawn_daemon('exp-daemon')
    return 0


def exp_resume_or_new(kind, args, build):
    x = load_json(EXP)
    if x and x.get('tipo') == kind and x.get('stato') != 'finita' and not getattr(args, 'new', False):
        print(f"Ripresa: {kind} del {x['creato']}.")
        if daemon_pid():
            print('Il demone sta gia girando.')
            return 0
        return exp_launch()
    return exp_create(build())


def cmd_exp_daemon(args):
    global IN_TUNE
    IN_TUNE = True
    PIDFILE.write_text(str(os.getpid()))
    trainers = load_json(TRAINERS_JSON) or []
    n = len(trainers)
    try:
        x = load_json(EXP)
        x['_t0'] = time.time()
        while x.get('stato') != 'finita':
            nxt = exp_next(x, n)
            save_json(EXP, x)
            if nxt is None:
                x['stato'] = 'finita'
                break
            tag, jobs = nxt
            state = load_json(STATE)
            if not state or state.get('tag') != tag or state.get('esperimento') != x['creato']:
                state = {'creata': now(), 'tag': tag, 'esperimento': x['creato'], 'rom': x['rom'], 'stato': 'avvio',
                         'opzioni': {'parallelo': x['parallelo'], 'report': False}, 'partite': jobs}
                save_json(STATE, state)
                print(f"[{now()}] {x['tipo']}: campagna {tag}, {len(jobs)} partite", flush=True)
            cmd_daemon(None)
            state = load_json(STATE)
            if state.get('stato') != 'completata':
                print(f"[{now()}] {x['tipo']} fermata (campagna {tag})", flush=True)
                return 0
            exp_done(x, tag, state)
            x.pop('_t0', None)
            save_json(EXP, x)
            x['_t0'] = time.time()
        x.pop('_t0', None)
        x['stato'] = 'finita'
        save_json(EXP, x)
        exp_report(x)
        print(f"[{now()}] {x['tipo']} finita", flush=True)
    finally:
        try:
            if PIDFILE.exists() and PIDFILE.read_text().strip() == str(os.getpid()):
                PIDFILE.unlink()
        except OSError:
            pass
    return 0


def cmd_tune(args):
    def build():
        return {'tipo': 'taratura', 'fase': 'spsa', 'k': 0, 'iterazioni': args.iters, 'theta': [100.0] * len(WEIGHT_NAMES),
                'c': args.c, 'a0': args.a, 'A': args.A, 'alpha': 0.602, 'gamma': 0.101, 'passo_max': args.max_step,
                'semi': args.seeds, 'seme_base': args.seed_base, 'semi_val': args.val_seeds,
                'seme_base_val': args.val_seed_base, 'scontri': args.matchups, 'parallelo': args.parallel,
                'penalita': args.penalty, 'storia': []}
    return exp_resume_or_new('taratura', args, build)


def cmd_valuta(args):
    w = [int(v) for v in args.weights.split(',')]
    return exp_resume_or_new('valutazione', args, lambda: {
        'tipo': 'valutazione', 'pesi': w, 'semi': args.seeds, 'seme_base': args.seed_base, 'scontri': args.matchups,
        'parallelo': args.parallel})


def cmd_esplora(args):
    def build():
        d = WORK / 'dati' / 'esplorazione'
        if d.exists() and not args.add:
            shutil.rmtree(d, ignore_errors=True)
        return {'tipo': 'esplorazione', 'esplora': args.eps, 'semi': args.seeds, 'seme_base': args.seed_base,
                'scontri': args.matchups, 'parallelo': args.parallel, 'cartella': 'esplorazione'}
    return exp_resume_or_new('esplorazione', args, build)


def cmd_verifica_libro(args):
    return exp_resume_or_new('verifica_libro', args, lambda: {
        'tipo': 'verifica_libro', 'semi': args.seeds, 'seme_base': args.seed_base, 'scontri': args.matchups,
        'parallelo': args.parallel, 'cartella': 'verifica_libro'})


def cmd_verifica_contesto(args):
    """Contesto della squadra avversaria (team preview) acceso per un lato e spento per l'altro, a specchio."""
    return exp_resume_or_new('verifica_contesto', args, lambda: {
        'tipo': 'verifica_contesto', 'semi': args.seeds, 'seme_base': args.seed_base, 'scontri': args.matchups,
        'parallelo': args.parallel, 'cartella': 'verifica_contesto'})


def cmd_libro(args):
    """Rigenera src/data/vgc_plan_book.h dai dati dell'esplorazione (poi serve ricompilare)."""
    cmd = [sys.executable, str(TOOL / 'gen_plan_book.py'), '--dirs', args.dirs, '--min-games', str(args.min_games)]
    return subprocess.run(cmd).returncode


def cmd_bench(args):
    """Partite all'ora con N istanze (in primo piano): stessi scontri a rotazione, log silenzioso o completo."""
    if daemon_pid():
        print('Il demone sta gia girando: fermarlo prima del benchmark.')
        return 1
    trainers = load_json(TRAINERS_JSON) or []
    n = len(trainers)
    pairs = parse_matchups('all', n)
    jobs = []
    for i in range(args.games):
        p, e = pairs[(i * 37) % len(pairs)]
        job = {'p': p, 'e': e, 'seed': 300001 + i, 'mode': MODE_PREVIEW, 'max_turns': 50, 'tag': 'bench',
               'silenzio': 1 if args.quiet else 0, 'libro_off': 3}
        if args.quiet:
            job['silenzio'] = 1
        job['id'] = game_id(job)
        job['stato'] = 'in_coda'
        jobs.append(job)
    state = {'creata': now(), 'tag': 'bench', 'stato': 'avvio', 'rom': str(ROM),
             'opzioni': {'parallelo': args.parallel, 'report': False}, 'partite': jobs}
    save_json(STATE, state)
    global IN_TUNE
    IN_TUNE = True
    t0 = time.time()
    cmd_daemon(None)
    dt = time.time() - t0
    state = load_json(STATE)
    done = [j for j in state['partite'] if j['stato'] == 'finita']
    ok = [j for j in done if j.get('esito') in ('VITTORIA_G', 'VITTORIA_A', 'PAREGGIO', 'LIMITE_TURNI')]
    turns = [j['turni'] for j in ok if j.get('turni')]
    secs = [j['secondi'] for j in done if j.get('secondi')]
    res = {'quando': now(), 'parallelo': args.parallel, 'silenzio': bool(args.quiet), 'partite': len(done),
           'valide': len(ok), 'secondi_totali': round(dt, 1), 'partite_ora': round(len(done) * 3600 / dt),
           'secondi_partita_media': round(sum(secs) / max(1, len(secs)), 2),
           'turni_medi': round(sum(turns) / max(1, len(turns)), 1)}
    hist = load_json(WORK / 'bench.json', []) or []
    hist.append(res)
    save_json(WORK / 'bench.json', hist)
    if PIDFILE.exists():
        PIDFILE.unlink()
    print(json.dumps(res, ensure_ascii=False))
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
    if MODE_FILE.exists() and MODE_FILE.read_text().strip() == 'esperimento':
        x = load_json(EXP)
        if x and x.get('stato') != 'finita':
            print(f"Ripresa: {x['tipo']} del {x['creato']}.")
            spawn_daemon('exp-daemon')
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
    if MODE_FILE.exists() and MODE_FILE.read_text().strip() == 'esperimento':
        x = load_json(EXP) or {}
        if x.get('tipo') == 'taratura':
            print(f"Taratura: passo {min(x.get('k', 0) + 1, x.get('iterazioni', 0))}/{x.get('iterazioni')} "
                  f"(fase {x.get('fase')}, {x.get('stato')}), pesi: {[round(v) for v in x.get('theta', [])]}")
            for h in x.get('storia', [])[-3:]:
                print(f"  passo {h['passo']}: f+={h['f_piu']:.4f} f-={h['f_meno']:.4f}")
            if x.get('validazione'):
                print(f"  validazione: {x['validazione']}")
        elif x:
            print(f"Esperimento {x.get('tipo')} del {x.get('creato')}: {x.get('stato')}"
                  + (f", risultato {x['risultato']}" if x.get('risultato') else ''))
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
    st.add_argument('--weights', help='pesi del lato G in %% (ordine: ' + ','.join(WEIGHT_NAMES) + '), es. 100,120,100,...')
    st.add_argument('--new', action='store_true', help='nuova campagna anche se ce n\'e una in corso')
    sub.add_parser('daemon')
    sub.add_parser('exp-daemon')
    tu = sub.add_parser('tune')
    tu.add_argument('--iters', type=int, default=30, help='passi SPSA')
    tu.add_argument('--c', type=float, default=20.0, help='ampiezza della perturbazione (punti %%)')
    tu.add_argument('--a', type=float, default=12000.0, help='passo iniziale del gradiente (a0)')
    tu.add_argument('--A', type=float, default=3.0, help='stabilita del passo (SPSA)')
    tu.add_argument('--max-step', type=float, default=12.0, help='spostamento massimo per peso e per passo (punti %%)')
    tu.add_argument('--seeds', type=int, default=2, help='semi fissi per passo (ognuno a specchio)')
    tu.add_argument('--seed-base', type=int, default=SEED_BASE['taratura'])
    tu.add_argument('--val-seeds', type=int, default=8, help='semi della validazione finale (mai visti)')
    tu.add_argument('--val-seed-base', type=int, default=SEED_BASE['validazione'])
    tu.add_argument('--penalty', type=float, default=0.01, help='penalita per misplay certo per partita')
    tu.add_argument('--matchups', default='all')
    tu.add_argument('--parallel', type=int, default=DEFAULT_PARALLEL)
    tu.add_argument('--new', action='store_true')
    va = sub.add_parser('valuta')
    va.add_argument('--weights', required=True, help='pesi in %% (' + ','.join(WEIGHT_NAMES) + ')')
    va.add_argument('--seeds', type=int, default=8)
    va.add_argument('--seed-base', type=int, default=SEED_BASE['validazione'] + 50000)
    va.add_argument('--matchups', default='all')
    va.add_argument('--parallel', type=int, default=DEFAULT_PARALLEL)
    va.add_argument('--new', action='store_true')
    ex = sub.add_parser('esplora')
    ex.add_argument('--eps', type=int, default=30, help='%% di team preview e scelte dei turni 1/2 esplorative')
    ex.add_argument('--seeds', type=int, default=20)
    ex.add_argument('--seed-base', type=int, default=SEED_BASE['esplorazione'])
    ex.add_argument('--matchups', default='all')
    ex.add_argument('--parallel', type=int, default=DEFAULT_PARALLEL)
    ex.add_argument('--add', action='store_true', help='aggiunge ai dati di esplorazione precedenti')
    ex.add_argument('--new', action='store_true')
    vl = sub.add_parser('verifica-libro')
    vl.add_argument('--seeds', type=int, default=8)
    vl.add_argument('--seed-base', type=int, default=SEED_BASE['libro'])
    vl.add_argument('--matchups', default='all')
    vl.add_argument('--parallel', type=int, default=DEFAULT_PARALLEL)
    vl.add_argument('--new', action='store_true')
    vc = sub.add_parser('verifica-contesto')
    vc.add_argument('--seeds', type=int, default=3)
    vc.add_argument('--seed-base', type=int, default=1)
    vc.add_argument('--matchups', default='all')
    vc.add_argument('--parallel', type=int, default=DEFAULT_PARALLEL)
    vc.add_argument('--new', action='store_true')
    lb = sub.add_parser('libro')
    lb.add_argument('--dirs', default='esplorazione')
    lb.add_argument('--min-games', type=int, default=3)
    be = sub.add_parser('bench')
    be.add_argument('--parallel', type=int, default=DEFAULT_PARALLEL)
    be.add_argument('--games', type=int, default=200)
    be.add_argument('--quiet', action='store_true', help='log silenzioso (come nell\'allenamento)')
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
                'resume': cmd_resume, 'stop': cmd_stop, 'status': cmd_status, 'report': cmd_report, 'run-one': cmd_run_one,
                'tune': cmd_tune, 'exp-daemon': cmd_exp_daemon, 'valuta': cmd_valuta, 'esplora': cmd_esplora,
                'verifica-libro': cmd_verifica_libro, 'verifica-contesto': cmd_verifica_contesto, 'bench': cmd_bench, 'libro': cmd_libro}
    sys.exit(handlers[args.cmd](args) or 0)


if __name__ == '__main__':
    main()

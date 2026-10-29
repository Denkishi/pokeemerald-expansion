# Logger delle lotte VGC (partite giocate a mano in mGBA)

Il ROM normale scrive nel log di mGBA, durante le lotte contro gli allenatori VGC (flag "Vgc Doubles"), tutto ciò
che serve per capire dopo cosa ha sbagliato l'IA dei doppi: squadre e team preview, stato del campo a ogni turno,
decisioni del pianificatore, scelte di entrambi i lati, cronaca del turno ed esito. Lo script
`tools/vgc_training/analizza_log.py` trasforma il log in un report Markdown per lotta, con gli stessi controlli dei
misplay del self-play.

- Interruttore: `CUSTOM_VGC_BATTLE_LOG` in `include/config/custom.h` (acceso di default).
- Codice: `src/vgc_battle_log.c`. Su console vera e negli emulatori senza i registri di debug di mGBA non fa nulla
  (la stretta di mano con mGBA fallisce e il logger resta spento). Nelle lotte non VGC non scrive nulla.

## 1. Il ROM

Serve il ROM compilato con il logger:

- da GitHub: pagina **Actions** del repository, ultima esecuzione verde del branch `ia-vgc-logger` (o di `master`
  dopo il merge), in fondo alla pagina l'artefatto **`pokeemerald-rom`**; dentro c'è `pokeemerald.gba`;
- oppure da riga di comando: `gh run download <id> -R Denkishi/pokeemerald-expansion -n pokeemerald-rom`.

I salvataggi del ROM di prima vanno bene (il logger non cambia i dati di salvataggio).

## 2. Accendere il log in mGBA (una volta sola)

In mGBA (0.10 o più recente, Windows):

1. **Tools > Settings... > Logging** (Strumenti > Impostazioni > Log).
2. Spunta **Log to file**, poi **Select Log File** e scegli un file, per esempio
   `Documenti\mgba_vgc.log`. mGBA aggiunge in fondo al file: le lotte di più sessioni si accumulano.
3. Nella tabella delle categorie lascia accesi i livelli di default. Il logger scrive al livello **Warning**
   nella categoria **GBA Debug**: se in passato hai spento qualcosa, controlla che per "GBA Debug" (o per la riga
   di default) **Warning** sia spuntato.
4. **OK**. Non serve "Log to console".

In alternativa, senza file: **Tools > View Logs...**, spunta **Warning**, gioca, poi seleziona tutto il testo della
finestra (Ctrl+A, Ctrl+C) e incollalo in un file di testo. Attenzione: la finestra tiene solo le ultime righe
("Max Lines"), quindi per più lotte il file è più sicuro.

Le righe del logger iniziano con `VGCLOG` (nel file: `[WARN] GBA Debug: VGCLOG ...`).

## 3. Giocare

Gioca normalmente contro i 12 allenatori VGC (Bo1 o Bo3). Ogni gara della serie è una lotta a sé nel log. Non serve
fare altro: a fine sessione chiudi mGBA (o almeno finisci la lotta) e il file è pronto.

## 4. Dove mettere il file

Copia il file di log nel repository in `docs/ia_vgc/report/partite_umane/log/` (per esempio
`docs/ia_vgc/report/partite_umane/log/2026-10-29.log`), oppure tienilo dove vuoi e passa il percorso allo script.
Più file vanno bene: lo script li legge tutti.

## 5. Analisi

Dalla cartella del repository:

```
python tools/vgc_training/analizza_log.py docs/ia_vgc/report/partite_umane/log/2026-10-29.log
```

oppure tutti i log insieme (il riepilogo comprende i file passati):

```
python tools/vgc_training/analizza_log.py docs/ia_vgc/report/partite_umane/log/*.log
```

Escono, in `docs/ia_vgc/report/partite_umane/`:

- un report per lotta (`<log>_<n>_<allenatore>_gara<k>.md`): squadre, team preview (gruppi e lead migliori dell'IA,
  lead previste del giocatore, chi ha portato ciascuno), poi turno per turno campo, PS e stato, le coppie del
  pianificatore con il valore, le risposte previste, le scelte di tutti, la cronaca e i misplay;
- `riepilogo.md`: tutte le lotte con esito e misplay, vittorie per allenatore, misplay per categoria.

Opzione `--out CARTELLA` per scrivere altrove.

### Misplay controllati (solo lato IA)

Gli stessi del self-play (`test/vgc_selfplay/vgc_selfplay.c`), sui dati che l'IA aveva a inizio turno (le stime di
danno sono le sue, righe `DMG`): KO garantito non preso (certo; dubbio se il bersaglio ha Protect, se la scelta è
una mossa ad area o di supporto), cambio in un KO, win condition persa senza Protect/cambio, mossa su un immune,
Protect consecutivo, Fake Out dopo il primo turno in campo, setup sotto minaccia di KO, Tailwind/Trick Room
sprecati, attacco sul compagno senza combo, bersaglio che il compagno manda già KO, cambi a ripetizione.
"Certo" = errore senza attenuanti note, "dubbio" = dipende da una lettura o da dati incompleti.

## Formato delle righe (per chi analizza)

Una riga per evento, prefisso `VGCLOG`, battler `b0..b3` (0 e 2 giocatore = `G`, 1 e 3 IA = `A`):

| riga | quando | contenuto |
|---|---|---|
| `INIZIO allenatore=<id> <nome>` | team preview (o turno 1 senza preview) | inizio di una lotta |
| `SET G\|A <slot> <specie> @ <strumento> \| <abilità> \| m1 / m2 / m3 / m4 \| liv vel ps` | team preview | squadre complete (anche i set veri dell'IA) |
| `TP GRUPPO <n> <punteggio> <specie,...>` | team preview | i 3 gruppi di 4 migliori dell'IA |
| `TP LEAD <n> <punteggio> peggiore=<v> <A> + <B>` | team preview | le 3 lead migliori (scelta mista tra le vicine) |
| `TP PREVISTE <n> peso=<w> <A> + <B>` | team preview | lead del giocatore previste dall'IA |
| `PORTA A\|G <slot x4> \| lead ... \| dietro ... [\| mega ...]` | team preview | i 4 portati, lead prima |
| `GARA <k> di Bo<n> allenatore=<id> <nome>` | uscita dalla preview | numero della gara |
| `TURNO <n>` | inizio turno | |
| `CAMPO meteo= terreno= trickroom= tailwindG= tailwindA= schermiG= schermiA= lentaA=` | inizio turno | turni rimasti; schermi = bit (1 Reflect, 2 Light Screen, 32 Aurora Veil); lentaA = squadra IA più lenta |
| `MON b<i> <lato> <specie> slot<k> ps=<a>/<b> stato= stadi=Atk,Def,Spe,SpA,SpD vel= strum= abil= primoturno= wincon= panchina= encore=` | inizio turno | ogni battler |
| `DMG b<a>>b<d> \| <mossa> <min>-<max> a<prec> e<eff x100> ...` | inizio turno | stime di danno dell'IA |
| `PIANO b<i> <specie> T<n> coppie= scelta= regole= fotogrammi= simulazioni=` | fine del pianificatore | |
| `COPPIA b<i> #<k>: b<x> <azione> + b<y> <azione> = <valore> [<==] [(regole)]` | fine del pianificatore | ogni coppia simulata (VGCPLAN dei test) |
| `PREVISTA b<i> A\|B<k>: b<x> <azione> peso=<w>` | fine del pianificatore | risposte previste degli avversari (VGCFOE) |
| `AZIONE b<i> <lato> <specie> MOSSA <mossa> -> b<t> <rel> cat= fx= idx= [+MEGA] [bloccata]` | scelte fatte | `fx`: P Protect, S setup, F Fake Out, W Tailwind, R Trick Room, A area, X area anche sul compagno, H Helping Hand, M Follow Me, Q priorità |
| `AZIONE b<i> <lato> <specie> CAMBIO -> <specie> (slot<k>)` | scelte fatte | |
| `ORDINE b.. b.. b.. b..` | scelte fatte | ordine del turno |
| `MSG <testo>` | durante il turno | messaggi della lotta (mosse usate, critici, mancati, KO...) |
| `PS b<i> <lato> <specie> <prima> -> <dopo> / <max>` | durante il turno | ogni variazione di PS |
| `KO b<i> <lato> <specie> t<n>` | durante il turno | |
| `ABILITA b<i> <lato> <specie> <abilità>` | durante il turno | finestra dell'abilità |
| `FINE esito=VITTORIA_G\|VITTORIA_A\|... turni= restantiG= restantiA=` | fine lotta | |

Costo: nessuna memoria aggiunta oltre a pochi byte in EWRAM; le righe si scrivono solo con mGBA che risponde. Il
pianificatore scrive le sue righe una volta, a lavoro finito, fuori dalle fette di tempo.

Nel ROM dei test le stesse righe escono nel self-play non silenzioso (`selfplay.py run-one`), utile per controllare
il formato senza giocare.

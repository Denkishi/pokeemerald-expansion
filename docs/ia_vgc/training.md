# Self-play VGC: partite IA contro IA

> **Stato (27/10 mattina, branch `ia-vgc`):** le partite IA contro IA arrivano alla fine.
> Corretti: il blocco dopo il rimpiazzo del lato G (messaggio di entrata con un allenatore partner inesistente), una
> probabilità di brutto colpo 1/0 (`GetCriticalHitOdds`), i parametri della lotta allenatore non azzerati, le scene di
> fine lotta (la partita ora finisce al messaggio d'esito) e la chiusura del test (heap). L'orchestratore chiude
> mGBA appena legge `VSP FINE` e ferma i log impazziti (oltre 300 MB). Una partita dura pochi secondi.
>
> **Avvio rapido** (PowerShell, nella cartella `tools\vgc_training\`):
> - compilare: `.\training.ps1 build` (o `compila.bat`), in WSL Ubuntu, 15-25 minuti sul disco Windows;
> - una campagna di 144 scontri: `.\training.ps1 start -- --new --parallel 24` (o `avvia.bat`);
> - pausa / ripresa / stop: `pausa.bat` / `riprendi.bat` / `ferma.bat` (o `.\training.ps1 pause|resume|stop`):
>   la pausa lascia finire le partite in corso, lo stop le rimette in coda; `start` o `resume` ripartono da dove era;
> - avanzamento: `stato.bat`; report: `report.bat`, che scrive `docs\ia_vgc\report\riepilogo.md` e una pagina per partita.

Harness per far giocare tra loro i 12 allenatori VGC (le squadre vere di `src/data/trainers.party`, linea AI con
`Vgc Doubles`), in partite controllate e rigiocabili, con controllo dei misplay e report in italiano.
Base anche per l'allenamento automatico (M9).

## Comandi

Da PowerShell, nella cartella del worktree (`tools\vgc_training\`), oppure con doppio clic sui `.bat`:

| Comando | .bat | Cosa fa |
|---|---|---|
| `.\training.ps1 build` | `compila.bat` | compila in WSL il ROM del self-play e lo copia in `tools\vgc_training\work\` |
| `.\training.ps1 start` | `avvia.bat` | crea la campagna (144 scontri, 1 seme ciascuno) o riprende quella interrotta, e avvia il demone in sottofondo |
| `.\training.ps1 pause` | `pausa.bat` | le partite in corso finiscono, poi non ne partono altre |
| `.\training.ps1 resume` | `riprendi.bat` | toglie la pausa (riavvia il demone se era stato chiuso) |
| `.\training.ps1 stop` | `ferma.bat` | ferma subito; le partite non finite tornano in coda |
| `.\training.ps1 status` | `stato.bat` | partite finite / in corso / in coda, esiti, misplay, stima del tempo |
| `.\training.ps1 report` | `report.bat` | rigenera `docs\ia_vgc\report\` dai risultati |
| `.\training.ps1 run-one 0 1 [seme]` | | una partita in primo piano (G = allenatore 0, A = allenatore 1) |
| `.\training.ps1 list` | | elenco degli allenatori VGC con il loro indice |

Opzioni di `start` (dopo il comando): `--parallel 24` (istanze mGBA, default 24), `--seeds 3` (partite per scontro,
semi diversi), `--seed-base 1`, `--matchups 0-1,4-7` (solo alcuni scontri, default tutti i 144), `--max-turns 50`,
`--new` (nuova campagna anche se ce n'è una da finire). Lo stato sta in `tools\vgc_training\work\state.json`: dopo
uno stop, un riavvio del PC o la chiusura del demone, `start` o `resume` ripartono dalla prima partita non finita.
La pausa ha effetto tra una partita e l'altra.

Requisiti: Python 3 su Windows (`py` o `python`), WSL Ubuntu per compilare, `C:\msys64\mingw64\bin` (lo aggiunge lo
script al PATH di mGBA).

## Come funziona

- **ROM**: è il ROM dei test (`make pokeemerald-test.elf`) con un test speciale, `test/vgc_selfplay/vgc_selfplay.c`,
  che ha un runner suo. Gli allenatori VGC vengono estratti da `src/data/trainers.party` a ogni build
  (`trainer_rules.mk` → `test/vgc_selfplay/vgc_trainers.party/.h`) e aggiunti alla tabella degli allenatori dei test.
- **Una partita**: il runner crea le due squadre dai dati degli allenatori, fa scegliere **4 su 6 e lead alla team
  preview dell'IA di ciascun lato** contro la squadra dell'altro (`TeamPreview_AiSelectEnemyTeam`, la stessa del gioco;
  per il lato G le squadre vengono scambiate), attiva IA contro IA e avvia la lotta vera, senza animazioni.
  Il seme dell'RNG è fisso: la stessa partita (allenatori + seme) si rigioca identica.
- **Parametri**: stanno in `gVgcSelfPlayArgs` (`test/vgc_selfplay/vgc_selfplay_args.c`, layout in
  `include/test/vgc_selfplay.h`). L'orchestratore (`selfplay.py`) cerca la firma `VGCSELFPLAYARG1` nel `.gba`, scrive
  allenatori/seme/modalità e lancia `mgba-rom-test.exe`; un `.gba` temporaneo per partita.
- **Informazione chiusa**: in IA contro IA ogni lato pensa dal proprio punto di vista (`src/battle_ai_perspective.c`):
  conosce i propri set, dell'altro solo ciò che ha visto (mosse usate, abilità/strumenti rivelati, la squadra in
  anteprima). I dati dell'IA (`gAiLogicData`: abilità/strumenti presunti, danni stimati) sono separati per lato, il
  pianificatore e le regole VGC ragionano rispetto al lato di chi pensa, e il lato G usa la stessa logica
  dell'avversario (pianificatore compreso). Test di invarianza: `test/battle/ai/ai_vgc_perspective.c`.
- **Modalità secondarie**: lead/squadre fissate (`--fixed --order-g 0,1,2,3 --order-a 2,3,0,1`, slot 0..5 in ordine,
  lead prima) e posizioni preparate (`--scenario '{"turno":3,"tailwind":[0,2],"trickroom":0,"hp":[0,50,0,0],"stadi":[[0,0,0,2,0],[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0]]}'`:
  contatore dei turni, turni di Tailwind per lato G/A, Trick Room, PS % e stadi Atk/Def/Vel/AtkSp/DifSp per battler
  b0..b3), applicate prima delle scelte del primo turno.

## Controllo dei misplay

Ogni turno, a scelte fatte e prima che il turno si svolga, il ROM controlla ogni decisione **con gli occhi di chi
l'ha presa** (i dati dell'IA del suo lato: mosse viste dell'altro, abilità/strumenti presunti, danni stimati).
Ogni errore va nel log con turno, stato del campo, i Pokémon in campo e le alternative valutate dal pianificatore
(`VGCPLAN`). Categorie:

| Categoria | Certo quando | Dubbio (revisione) quando |
|---|---|---|
| `IMMUNE` | mossa d'attacco su un immune noto (efficacia 0), o mossa di stato bloccata (polvere, Prankster sul Buio, Thunder Wave sul Terra, bruciatura sul Fuoco, veleno su Veleno/Acciaio) | il compagno manda KO prima il bersaglio e la mossa passa sull'altro |
| `BERSAGLIO_GIA_KO` | il compagno manda KO prima lo stesso bersaglio e sull'altro la mossa è inutile | |
| `KO_NON_PRESO` | KO garantito (danno minimo, precisione piena) non preso né da noi né dal compagno, scelto un attacco | il bersaglio ha Protect visto (possibile lettura), o è stato scelto Protect/cambio/stato |
| `ATTACCO_COMPAGNO` | mossa sul compagno senza combo, o mossa ad area che lo manda KO | mossa ad area che toglie metà dei suoi PS |
| `PROTECT_CONSECUTIVO` | Protect scelto due turni di fila dallo stesso Pokémon | |
| `FAKE_OUT_TARDIVO` | Fake Out (e mosse "solo primo turno") non al primo turno in campo | |
| `CAMBIO_IN_KO` | entrato con un cambio e KO nello stesso turno | |
| `CAMBI_RIPETUTI` | A→B e poi B→A nello stesso posto entro due turni | |
| `VELOCITA_SPRECATA` | Tailwind con il proprio Tailwind attivo; Trick Room che chiude il proprio Trick Room | tutto il lato in Protect/cambio durante il proprio controllo velocità |
| `SETUP_SOTTO_KO` | mossa di setup con un KO in arrivo (danno minimo stimato) senza redirect/Fake Out del compagno | KO solo con il danno massimo |
| `WIN_CONDITION_PERSA` | la win condition del piano va KO senza Protect/cambio, e il KO era prevedibile | KO non previsto dalle stime |
| Team preview | Baton Passer senza il suo ricevente; Truant/Slow Start/Defeatist senza chi la toglie; meteo di lead che indebolisce gli attacchi del compagno | mossa ad area del lead che colpisce il compagno non immune |

## Report

- `docs/ia_vgc/report/riepilogo.md`: misplay per categoria, tabella 12×12 degli esiti (con link alle partite),
  vittorie per allenatore, esempi di misplay, partite bloccate/crash; `riepilogo.json` per gli script.
- `docs/ia_vgc/report/partite/<G>_<A>_s<seme>.md`: per partita, team preview (lead scelta, gruppi e lead valutati con
  punteggio), poi turno per turno campo, Pokémon, scelte con alternative del pianificatore, messaggi della lotta,
  misplay e dubbi.
- Log grezzi filtrati in `tools/vgc_training/work/logs/` (le righe `VGCLEAF` vengono solo contate), risultati in
  `work/results/*.json`.

## Gancio per M9 (taratura dei pesi)

- Il campo `variant` dei parametri (`--variant N`, byte `reserved[0]` di `VgcSelfPlayArgs`) è riservato all'indice
  della variante di pesi: il ROM non lo legge ancora; quando i pesi diventeranno parametri, l'IA li sceglierà da lì.
- `fitness()` in `selfplay.py` riduce una campagna a un numero (oggi: misplay certi per partita); un ottimizzatore
  lancerà le stesse partite (stessi semi) per più varianti e confronterà fitness ed esiti.

## Limiti noti

- Le mosse dei Pokémon nascosti in panchina e le statistiche esatte (EV) dell'avversario: i danni stimati dell'IA
  usano le statistiche vere (come nella lotta contro il giocatore umano); mosse, abilità e strumenti invece restano
  nascosti finché non si vedono.
- Il controllo dei misplay è basato sulle stime dell'IA (danno minimo/massimo, precisione): un colpo critico o un
  danno fuori stima può far sembrare un errore una scelta corretta (o viceversa). I casi dubbi vanno rivisti a mano.
- In IA contro IA il lato G non ha bisogno del Mega Anello (nella build dei test il controllo non c'è).

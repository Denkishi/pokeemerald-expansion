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

## Allenamento automatico (M9)

### Guida rapida

Tutto da `tools\vgc_training\` (doppio clic sui `.bat`, oppure `.\training.ps1 <comando> -- <opzioni>`). Un solo
esperimento o campagna alla volta; **pausa / ripresa / stop / stato sono gli stessi per tutto**:

| Cosa | .bat | Comando |
|---|---|---|
| Compilare il ROM del self-play (dopo ogni modifica al codice o al libro) | `compila.bat` | `.\training.ps1 build` |
| Taratura dei pesi (SPSA + validazione finale) | `taratura.bat` | `.\training.ps1 tune -- --iters 30 --seeds 2 --parallel 28` |
| Pesi dati contro i default, su semi mai visti | | `.\training.ps1 valuta -- --weights 90,104,86,96,104,96,112` |
| Esplorazione per il libro dei piani | `esplora.bat` | `.\training.ps1 esplora -- --eps 30 --seeds 40` |
| Rigenerare il libro dei piani (`src/data/vgc_plan_book.h`) | `libro.bat` | `.\training.ps1 libro`, poi `compila.bat` |
| Verifica del libro (acceso contro spento, semi mai visti) | `verifica_libro.bat` | `.\training.ps1 verifica-libro -- --seeds 8` |
| Pausa (le partite in corso finiscono) | `pausa.bat` | `.\training.ps1 pause` |
| Ripresa (anche dopo stop, chiusura o riavvio del PC) | `riprendi.bat` | `.\training.ps1 resume` |
| Stop immediato (le partite non finite tornano in coda) | `ferma.bat` | `.\training.ps1 stop` |
| Avanzamento | `stato.bat` | `.\training.ps1 status` |
| Partite all'ora | `benchmark.bat` | `.\training.ps1 bench -- --parallel 28 --games 1000 --quiet` |

Rilanciare `taratura.bat` (o `esplora.bat`, ...) con un esperimento dello stesso tipo non finito lo **riprende**;
`--new` ne comincia uno nuovo. Lo stato sta su disco: `work/esperimento.json` (esperimento: passo, pesi, storia),
`work/state.json` (partite della campagna corrente), `work/esperimento.gba` (copia del ROM dell'avvio: ricompilare
durante un esperimento non lo cambia). Risultati: `docs/ia_vgc/report/taratura.md`, `verifica_libro.md`,
`valutazione_pesi.md`, `esplorazione.md`; log del demone in `work/demone.log`.

### ROM di allenamento

È il ROM dei test in modalità headless (`gTestRunnerHeadless`): niente animazioni di lotta né di mosse, testi senza
attese, intro e transizioni saltate, nessun suono. In più, per l'allenamento, la **modalità silenziosa** (parametro
`quiet`, sempre attiva in taratura/esplorazione/verifiche): `Test_MgbaPrintf` scarta senza formattarle le righe di
diagnostica dell'IA (`VGC...`) e la cronaca della lotta (`VSP MSG/PS/MON/CAMPO/ABILITA`); restano esiti, scelte dei
turni 1/2, chiave del libro, misplay. L'orchestratore non scrive più 32 MB di ROM per partita: ogni istanza ha il suo
file (`work/tmp/posto_N.gba`) e a ogni partita si riscrivono solo i 76 byte dei parametri.

Nota: il pianificatore lavora a fette per fotogramma e l'RNG avanza con i fotogrammi, quindi log completo e log
silenzioso danno partite diverse con lo stesso seme (ognuno resta rigiocabile identico). Taratura e verifiche usano
sempre il silenzioso.

**Benchmark** (27/10, i9-13950HX 24 core / 32 thread, IA di `ia-vgc` del mattino, tutti i 144 scontri a rotazione,
partite di 7-8 turni in media; `bench`, risultati in `work/bench.json`):

| Configurazione | Istanze | Partite | Partite/ora |
|---|---:|---:|---:|
| Orchestratore di prima (32 MB di ROM scritti per partita, log completo) | 1 | 24 | 1.380 |
| Orchestratore di prima | 28 | 288 | 12.234 |
| Nuovo, log completo | 1 | 24 | 1.148 |
| Nuovo, silenzioso | 1 | 24 | 1.572 |
| Nuovo, log completo | 28 | 1.000 | 15.587 |
| **Nuovo, silenzioso** | **24** | 1.000 | 17.947 |
| **Nuovo, silenzioso** | **28** | 1.000 | **19.228** |
| Nuovo, silenzioso | 32 | 1.000 | 18.767 |

28 istanze è il punto migliore (32 non aiuta: gli hyperthread sono già pieni). Con l'IA senza tetto al costo
(`CUSTOM_VGC_PLAN_MAX_FRAMES 0`, ricerca più ampia, dal merge delle 13:00) un passo di taratura da 1.152 partite
richiede ~5 minuti, cioè ~14.000 partite/ora con 28 istanze.

### Taratura dei pesi

Pesi tarati (in % dei `CUSTOM_VGC_EVAL_*` di `include/config/custom.h`, ordine dei parametri): KO, VIVO,
VELOCITA (`SPEED_PAIR`), MINACCE (`THREAT_FAST`/`SLOW`), MATERIALE, CAMBIO (`SWITCH_COST`), WINCON (la parte
oltre 100 di `WINCON_PCT`). Il ROM li applica a un solo lato (`weightSide`), l'altro resta ai default.

- **Disegno a specchio**: ogni scontro (G, A, seme) si gioca due volte, pesi in prova sul lato G e poi sul lato A.
  L'unità statistica è la coppia (0, ½, 1): il vantaggio del lato si annulla e l'errore standard è quello vero.
- **SPSA** con numeri casuali comuni: a ogni passo theta+c·delta e theta−c·delta sugli stessi semi fissi (2 di
  default, tutti i 144 scontri: 1.152 partite a passo), obiettivo = tasso di vittoria − 0,01 × misplay certi per
  partita del lato in prova; passi a_k = a0·((1+A)/(k+1+A))^0,602, c_k = c/(k+1)^0,101, spostamento massimo 12 punti.
- **Candidato** = media degli iterati della seconda metà (Polyak), validato su 8 semi **mai visti** (2.304 partite).
  I pesi entrano in `custom.h` solo se l'IC 95% del tasso di vittoria contro i default sta tutto sopra il 50%.

**Risultato della taratura del 27/10** (IA senza tetto al costo, `CUSTOM_VGC_PLAN_MAX_FRAMES 0`, merge di `ia-vgc`
delle 13:00; dettaglio in [report/taratura.md](report/taratura.md)):

- 20 passi SPSA × 1.152 partite (23.040 partite, ~2 h 50 con la CPU condivisa con un altro lavoro della macchina),
  più la validazione da 2.304 partite. Una prima corsa di 8 passi sul ROM di prima del merge è solo preliminare
  (pesi a metà strada [73, 110, 95, 89, 104, 109, 106], scartati).
- Sui 2 semi della taratura i punti provati vincevano in media il **52,0%** contro i default (rumore e adattamento
  a quei semi: gli f(+) e f(−) restano entrambi sopra il 50% senza una direzione stabile).
- Candidato (media della seconda metà): KO 94, VIVO 90, VELOCITA 86, MINACCE 122, MATERIALE 76, CAMBIO 116, WINCON 126.
- **Validazione su 8 semi mai visti, a specchio (1.152 coppie, 2.304 partite, nessuna non valida): 50,3%**,
  IC 95% [48,8%; 51,8%], z = 0,39, p = 0,70; misplay certi 0,55 a partita (come i default).
- **Non significativo: i pesi di `custom.h` restano quelli di default.** Con questi 7 pesi l'IA è già su un
  altopiano (±10-25% non cambia il tasso di vittoria in modo misurabile con ~2.300 partite, cioè sotto ~1,5 punti).
  Per vedere effetti più piccoli servono più semi di validazione (`--val-seeds 32`: errore standard ~0,4 punti).

### Esplorazione e libro dei piani

- **Esplorazione** (solo nel self-play, `explorePct` per lato): in una team preview su `eps` % i punteggi di gruppi e
  lead prendono un rumore di ±500 (si provano gruppi di 4 e lead vicine alla migliore); ai turni 1 e 2, con
  probabilità `eps` %, il pianificatore gioca una coppia di azioni entro 60 punti dalla migliore invece della migliore.
- **Raccolta**: per ogni partita il ROM scrive `VSP CHIAVE` (hash delle specie della propria squadra, classe della
  squadra avversaria vista in team preview: velocità base media lenta/media/veloce, +3 se ha due specie molto lente) e
  `VSP SCELTA` (turno, battler, posto in squadra, mossa) per i turni 1/2. `gen_plan_book.py` aggrega per
  (squadra, classe) e per coppia di lead: vittorie/partite, retro migliore con quelle lead, azione congiunta dei
  turni 1 e 2 con il tasso più alto (almeno 3 partite); più le righe "classe qualsiasi".
- **In ROM**: `src/data/vgc_plan_book.h`, tabella `const` (in ROM, 20 byte a piano). In RAM solo il piano scelto per
  lato (4 byte) e le esclusioni del Bo3.
- **In lotta** (`src/battle_ai_vgc_book.c`): in team preview, tra i piani della chiave con tasso stimato (Laplace)
  entro 12 punti dal migliore, uno **a caso con peso (tasso − migliore + 13)²** (strategia mista: la lead non è
  prevedibile); il gruppo del piano prende +400, la coppia di lead +400 (le regole forti, es. meteo o Trick Room
  incoerenti, restano più pesanti). Ai turni 1/2 il pianificatore dà +30 a ciascuna mossa del piano (una spinta, non
  un obbligo). In Bo3 le lead che hanno perso una gara della serie non si ripropongono dal libro; il piano si
  risceglie a ogni gara. Nei test normali (non IA contro IA) il libro è spento: risultati deterministici.

**Risultato del 27/10** (IA senza tetto al costo; dettagli in [report/verifica_libro.md](report/verifica_libro.md)):

- Esplorazione: 144 scontri × 30 semi (4.320 partite, `eps` 30%, libro spento, ~25 minuti). Libro generato:
  **196 piani per 48 chiavi (squadra, classe)**, tutti i 12 allenatori; 150 piani con l'azione congiunta del turno 1,
  113 anche con quella del turno 2 (es. Indeedee + Armarouge: Follow Me + Armor Cannon, poi Follow Me + Expanding
  Force; Copperajah + Indeedee: Heavy Slam + Trick Room). ~4 KB di ROM.
- **Verifica su 8 semi mai visti** (ROM dopo il merge delle 16:20 di `ia-vgc`), libro acceso per un lato e spento per
  l'altro, a specchio: 1.151 coppie (2.303 partite valide, 1 non valida per un assert del motore,
  `battle_move_resolution.c:5845 savedAttackerCount`, Parting Shot di Incineroar). **Il lato con il libro vince il
  52,7%**, IC 95% [51,1%; 54,3%], z = 3,25, p = 0,001; misplay certi 0,50 a partita. Miglioramento significativo.

## Limiti noti

- Le mosse dei Pokémon nascosti in panchina e le statistiche esatte (EV) dell'avversario: i danni stimati dell'IA
  usano le statistiche vere (come nella lotta contro il giocatore umano); mosse, abilità e strumenti invece restano
  nascosti finché non si vedono.
- Il controllo dei misplay è basato sulle stime dell'IA (danno minimo/massimo, precisione): un colpo critico o un
  danno fuori stima può far sembrare un errore una scelta corretta (o viceversa). I casi dubbi vanno rivisti a mano.
- In IA contro IA il lato G non ha bisogno del Mega Anello (nella build dei test il controllo non c'è).

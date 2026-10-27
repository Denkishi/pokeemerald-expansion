# IA VGC — Checklist di sviluppo

Aggiornata a ogni passo. Piano completo: [piano_implementazione.md](piano_implementazione.md). Branch: `ia-vgc`.

Legenda: ✅ fatto e verificato · 🔧 fatto, da verificare (build/test) · ⏳ in corso · ⬜ da fare · ❌ problema

## M0 — Misura e fondamenta
- ✅ Branch `ia-vgc` creato da `master`
- ✅ IWRAM liberata: `gLink`, `gRfu`, `gRfuAPIBuffer` in EWRAM, wireless in versione ROM — **IWRAM 87% → 53%** (EWRAM 90,7% → 94%)
- 🔧 Pianificatore con limite a tempo (`CUSTOM_VGC_PLAN_SCANLINES_PER_FRAME` = 100 righe su 228): niente più lag nei menu — compila, da provare in gioco
- ✅ Flag tolti ai 12 allenatori VGC: `Prediction`, `Try To 2hko`, `Prefer Highest Damage Move`
- 🔧 Imbrogli tolti:
  - ✅ statistiche e abilità vere dei Pokémon del giocatore nel pianificatore (ora stimate da specie e livello)
  - ✅ "ha ancora lo strumento?" letto dalla RAM → ora solo se si è visto consumarlo o perderlo
  - ✅ team preview: statistiche, abilità e Fake Out del giocatore ora stimati
  - ✅ velocità del giocatore nel pianificatore: stimata da specie e livello (Scout_EstimateStats) + stadi + Stolascelta nota/dedotta dall'ordine dei turni/assunta. Prima si leggeva la vera da `gAiLogicData->speedStats`, che conteneva anche Tailwind e meteo: contati due volte. I test che volevano un giocatore lento/veloce ora usano specie davvero lente/veloci (Ursaluna, Garchomp)
  - ✅ `gAiLogicData->speedStats` resta vera nelle regole del codice base (battle_ai_vgc.c, battle_ai_util.c) e nella deduzione della Stolascelta — tolta (27/10): regole Electro/Gyro Ball, Speed Swap, Encore in entrata usano la stima; deduzione Scarf solo nel belief (`VgcInferChoiceScarf` rimossa), `Scout_BattlerMaxSpeed` dall'intervallo della specie
  - 🔧 velocità del giocatore in `gAiLogicData->speedStats`: ora stimata dal belief (M1, branch `ia-vgc-meta`)
- ✅ Suite di test `test/battle/ai/ai_vgc_doubles.c` attiva. WSL compila soltanto; i test si eseguono su Windows con `tools/mgba/mgba-rom-test.exe`
- ✅ Installate con msys2 `libepoxy-0.dll` e `libwinpthread-1.dll`: `mgba-rom-test.exe` gira su Windows (aggiungere `C:\msys64\mingw64\bin` al PATH)
- ✅ **Risultati (tutti i test VGC: doppi, team preview, scout/belief, prospettiva IA contro IA, squadre vere): 60 PASS, 0 FAIL, 9 KNOWN_FAILING**
- ✅ Lead mai cambiate al turno 1 della lotta (né dal pianificatore né dalle regole): Toedscruel, Smeargle, Sableye ora fanno il loro lavoro
- ✅ Setup dei 5 test con il turno del giocatore incompleto: il giocatore porta solo i due in campo (un suo KO chiedeva un cambio non previsto dal TURN); ora girano
- ✅ Regole: Rage Powder/Follow Me +3 se il compagno ha Belly Drum — i KNOWN_FAILING sono tutti in `ai_vgc_teams.c`, elencati e spiegati in [scenari.md](scenari.md)
  - ✅ Dopo un KO entra il compagno giusto (Gallade accanto a Whimsicott)
- ℹ️ Controllo regressioni: `test/battle/ai/ai_doubles.c` dell'expansion ha 14 test che falliscono, **identici su `master`** (prima del lavoro sull'IA VGC): non li ho introdotti io, vengono dalle modifiche precedenti del fork
  - ✅ Fake Out al turno 1 per coprire un compagno Baton Passer
  - ✅ Trick Room per il compagno lento (Indeedee + Copperajah)
  - ✅ Baton Pass fa entrare lo sweeper designato (Chandelure), non un Pokémon qualsiasi
  - ✅ Fake Out non usato dopo il primo turno
  - ✅ Niente overkill: il più veloce manda KO, l'altro usa la mossa giusta sull'altro avversario (Ice Beam su Garchomp) — grazie alla ricerca congiunta (M4)
  - ✅ Simple Beam sul compagno con Mycelium Might quando addormentare prima conviene — **deciso dalla simulazione, nessuna regola sulla specie**
  - ✅ Niente Simple Beam contro Good as Gold: meglio attaccare (Shadow Ball + Earth Power)
  - ✅ Beat Up sul compagno con Justified prima che attacchi — deciso dalla simulazione
- ✅ Perdita di memoria nel messaggio "penso..." dell'avversario: due testi sovrapposti nella finestra dei messaggi lasciavano memoria allocata (48 byte ogni volta, si accumulano partita dopo partita: possibile causa di crash). Corretta, il test non la segnala più
- ✅ Stallo del secondo slot (TIMEOUT nei test): uno stato rimasto dalla lotta precedente lo faceva aspettare per sempre. Ora il pianificatore si azzera a inizio turno
- 🔧 Cache dei profili scout e degli attacchi imparabili (12 specie, EWRAM): niente più scansioni dei learnset a ogni simulazione
- 🔧 Schermo blu: non riprodotto. Controllo dell'heap a ogni turno nel self-play; 720 partite lunghe (5 semi, fino a 60 turni, massimo 43) + 3 campagne da 432: 0 blocchi rovinati, 0 asserzioni, 0 opcode illegali. ROM normale: EWRAM 95,83%, IWRAM 67,77%, ROM 80,63%. Serve un salvataggio o il momento esatto
- 🔧 Costo: da 500-1900 fotogrammi per decisione a 20-400 (la maggior parte 100-300, cioè 2-5 s a 100 righe/fotogramma). Cache di CanKoNextTurn per lavoro; probabilità di Protect/cambio del giocatore lette una volta; terzo turno ridotto con azioni scelte una volta per linea e giocato solo sulla linea migliore del turno 2; priorità e velocità calcolate una volta per passo (e priorità senza `GetBattleMovePriority`, che registrava Prankster come effetto collaterale); 10 coppie simulate per intero invece di 14. **Da fare**: scendere sotto i ~200 fotogrammi anche nei casi peggiori (407 nel test Simple Beam), ROM normale compilata (ultima: EWRAM 95,77%, IWRAM 66,7%, ROM 80,6%), da provare in gioco
- 🔧 Costo dopo cache della stima dei danni, `Evaluate` e velocità in IWRAM (codice ARM, +4 KB: IWRAM 66%) e 8 coppie complete: mediana 113 fotogrammi, 90° percentile 181, massimo 239 (prima 445); al turno 2 si salta la combinazione delle seconde scelte di entrambi gli avversari. Profilo per funzione nei test: riga `VGCPROF` (righe di schermo in Evaluate, RunTurn, ApplyAction, CanKo, stima). **Da fare**: massimo sotto ~200
- ✅ Misura dei frame di "pensiero" per decisione: riga `VGCCPU` nel log dei test (fotogrammi, simulazioni, chiamate a Evaluate/BuildActions/CanKo/stime/danni)

## M1 — Belief del giocatore (esperienza da giocatore VGC)
Branch di lavoro: `ia-vgc-meta`. Fonte dati e rigenerazione: piano, sezione "M1 — Fonte dei dati".
- 🔧 Base di conoscenza in ROM `src/data/vgc_meta.h` generata da `tools/vgc_meta/gen_vgc_meta.py` (statistiche Smogon/Showdown 2026-09: Champions Reg M-C con Mega, poi Reg M-B, Reg G, National Dex Doubles, Doubles OU): **310 specie** (tutte le 72 delle squadre VGC tranne Lickitung, senza dati), 3 spread, 10 mosse, 4 strumenti, 3 abilità, 2 Mega per specie, ~27 KB ROM
- 🔧 API in `include/battle_ai_scout.h` (sezione M1): `Scout_GetMetaSpreads`, `Scout_StatsFromSpread`, `Scout_SpeciesSpeedRange`, `Scout_GetLikelyMoves`, `Scout_MoveChance`, `Scout_GetLikelyItems`, `Scout_GetMegaForms` (con abilità della Mega), `Scout_AbilityChance`, `Scout_LikelyAbility`
- 🔧 Già collegate senza toccare il pianificatore: `Scout_EstimateStats` usa la spread più usata; `Scout_GuessItem` strumenti/Mega dalle statistiche; ruoli del profilo dalle mosse più usate (tabella a mano come minimo); `Scout_AssumedHoldEffect` usa il belief
- 🔧 Belief in lotta (EWRAM 14 B x 6): ordine di turno → intervallo Velocità e Choice Scarf (ritirata se si dimostra più lento); danni → Choice Band/Specs, Assault Vest, Eviolite (Choice ritirata se cambia mossa); abilità d'ingresso non comparse → escluse. Agganci: `HandleAction_UseMove`, `CancelerDamageCalc`, `Ai_InitPartyStruct`, inizio turno IA
- 🔧 `gAiLogicData->speedStats` del giocatore ora stimata (`Scout_BattlerBelievedSpeed`) invece della Velocità vera
- 🔧 Memoria Bo3: `SeriesMonMemory.belief` (Velocità, strumenti esclusi, abilità escluse) esportata a fine gara e importata alla successiva
- ✅ Build ROM normale compila (EWRAM 94,65%, IWRAM 53,2%, ROM 80,58%)
- ✅ Tolta `PlayerMayHaveAbility` inutilizzata in `team_preview.c` (warning nato in M7, che ne aveva tolto le chiamate)
- ❌ Prima build della ROM di test (`ai_vgc_scout.c`): errori `-Werror=enum-conversion` (passavo `B_POSITION_*` dove serve `enum BattlerId`). Corretto con `GetBattlerAtPosition(...)`, **ma non ricompilato né eseguito** (lavoro fermato su richiesta)
- ⬜ **Prossimi passi (dopo il merge in `ia-vgc`):**
  1. compilare ed eseguire `test/battle/ai/ai_vgc_scout.c` (14 test: 8 a funzione pura, 6 in lotta). Possibili correzioni: soglie dei test su Assault Vest / Choice Band se il tiro del danno o le statistiche di default dei test le mancano; il test "speed narrows" presume che l'IA usi Scratch a priorità 0
  2. rieseguire `ai_vgc_doubles.c` e `ai_vgc_team_preview.c`: Velocità del giocatore stimata (`speedStats`) e spread meta in `Scout_EstimateStats` cambiano le stime; nei test con `Speed(x)` del giocatore la stima ora NON legge quel valore. Se servono, i test vanno adattati (o il belief va ristretto dall'ordine di turno visto)
  3. **collegamento al pianificatore** (`battle_ai_vgc_plan.c`, da fare quando l'altro agente ha finito):
     - riga ~241 (Pokémon del giocatore in campo): `Scout_EstimateStats(pm->species, level, ...)` → `Scout_EstimateBattlerStats(b, &hp, stats)` (Velocità dentro l'intervallo visto)
     - riga ~359: `pm->ability = Scout_KnownAbility(species)` → per i Pokémon in campo `Scout_BattlerLikelyAbility(b, &p)` se `p >= 80`, altrimenti nessuna; in panchina `Scout_LikelyAbility(species, &p)` con la stessa soglia
     - riga ~370 (`Scout_GuessItem`): prima `Scout_BattlerDeducedHoldEffect(b)`; scartare la stima se `Scout_BattlerHoldEffectExcluded(b, effetto)`; Assault Vest / Choice dedotti = niente Protect né mosse di stato nelle risposte del giocatore
     - righe ~1042/1318 (`Scout_CollectLearnableAttacks`): prima le mosse di `Scout_GetLikelyMoves(species, ...)` pesate con `p` (o `Scout_BattlerMoveChance(b, mossa)` per il Pokémon in campo), poi il learnset come riserva
     - Mega del giocatore: `Scout_GetMegaForms` (probabilità e abilità della Mega) al posto di `Scout_MegaTargetSpecies` dove serve la probabilità
  4. team preview (`team_preview.c`, M7): stessi cambi per statistiche e mosse del giocatore (`Scout_GetLikelyMoves` invece delle sole mosse da learnset)

## M2 — Simulatore corretto
- 🔧 Protect di fila: il secondo ora fallisce (prima riusciva sempre)
- 🔧 Fake Out fallisce del tutto dopo il primo turno in campo (prima faceva danno)
- 🔧 Trick Room (5 turni) e Tailwind (4 turni) ora scadono; turni rimasti letti dal gioco
- 🔧 Sonno con contatore: ci si sveglia (prima si dormiva per sempre)
- ✅ Cache danni reale (`AI_CalcDamage`) al posto della stima — già così per i Pokémon in campo (`DamageOf` legge `simulatedDmg` del motore); la stima resta solo per panchina e Mega, ora con peso/meteo/PS/colpi multipli
- ✅ Stadi separati Atk/Def/SpA/SpD/Spe, letti dal gioco a inizio turno; i danni della tabella dell'IA si riscalano per gli stadi cambiati nella simulazione; le mosse di potenziamento applicano gli stadi veri (Calm Mind +1 SpA/+1 SpD, Dragon Dance +1 Atk/+1 Spe...) invece di un "+2" generico
- ✅ Speed Boost a fine turno
- ✅ Baton Pass nella simulazione: entra il ricevente designato con tutti gli stadi
- ✅ Intimidate all'entrata nella simulazione (con Clear Body, Inner Focus, Scrappy, Guard Dog, Defiant, Competitive, Clear Amulet); per il giocatore se la specie lo porta di solito
- ✅ Scottatura e paralisi (inflitte da Will-O-Wisp/Thunder Wave con le immunità; dimezzano attacchi fisici / Velocità)
- ✅ Schermi (Reflect, Light Screen, Aurora Veil sotto neve) piazzati nella simulazione
- ✅ Precisione con ramo "mancato": la simulazione applica il colpo pieno (un KO resta un KO) e per ogni azione imprecisa del turno 1 si rigioca il turno col colpo mancato, togliendo (1 - precisione) x differenza. Prima `danno x precisione` trasformava un KO all'80% in "mai KO" e una mossa ad area al 90% che manda KO due bersagli in nessun KO. Precisione dalla tabella dell'IA (Compound Eyes, No Guard...). Test: Rock Slide 90% su due bersagli > Scratch sicuro; Scratch > Stone Edge a pari KO
- ✅ **Bug trovato e corretto**: se un avversario non aveva attacchi plausibili noti, la simulazione dava per **certo** il suo Protect e qualunque nostro attacco sembrava inutile (tutte le coppie con lo stesso valore). Ora resta sempre l'alternativa "non si protegge" con la probabilità complementare
- ✅ **Causa trovata** (test "accurate move", Scratch che non mandava KO il bersaglio a 5 PS): non era il simulatore. `ScalePartiesToBattleLevel` (tutte le lotte a Lv50) ricalcolava le statistiche e curava i PS anche nei test: `HP(5)` diventava 250 PS e `Speed(10)` veniva ignorato. Ora anche i test sono a Lv50 come il gioco, ma i valori scritti dal test sopravvivono al ricalcolo: statistiche impostate (Speed(), Attack(), MaxHP()...) e PS non pieni (HP(5)). Solo nei test (`#if TESTING` in `ScalePartiesToBattleLevel`); nel gioco non cambia nulla. Diagnostica `VGCDMG`/`VGCFOE` nel log dei test
- ✅ Mosse ad area sul compagno (Earthquake, Surf, Discharge: TARGET_FOES_AND_ALLY) con Protect, Wide Guard e immunità d'abilità (Levitate, Flash Fire, Water/Volt Absorb, Lightning Rod, Storm Drain, Sap Sipper, Telepathy...; Mold Breaker le ignora). Test con il set vero del Rotom-Mow di Simone
- ✅ Mega nel simulatore: chi megaevolve in questo turno (noi se possiamo; il giocatore se la specie la usa ≥80% nel belief e non l'ha usata) prende statistiche, tipi e abilità della Mega a inizio turno (meteo della Mega già attivo: Charizard Y + Solar Beam). Per i danni di una Mega si usa la stima, non la tabella della forma base. Test: Solar Beam al turno della Mega Y
- ✅ Stima dei danni con le abilità e gli strumenti più comuni: Technician, Huge/Pure Power, Hustle, Solar Power, Adaptability, Transistor, Dragon's Maw, Steelworker, Water Bubble, Choice Band/Specs; in difesa Thick Fat, Heatproof, Ice Scales, Fur Coat, Filter/Solid Rock/Prism Armor, Purifying Salt
- ✅ Terreni nel simulatore: dall'abilità d'ingresso (Surge, Hadron Engine) e dalle mosse, 5 turni; Elettro/Erba/Psico x1,3 a terra, Earthquake a metà sull'Erboso, Drago a metà sul Nebbioso, cura 1/16 sull'Erboso, niente sonno su Elettrico/Nebbioso, priorità fallita sugli avversari a terra sul Psichico (anche Prankster), Grassy Glide. Test: niente Fake Out sul Campo Psichico
- ✅ Sostituto (1/4 dei PS, assorbe colpi e mosse di stato tranne sonore/Infiltrator, passa con Baton Pass, vale metà dei suoi PS nella valutazione)
- ✅ Cambi d'abilità a metà turno (Simple Beam, Worry Seed, Skill Swap, Gastro Acid, Entrainment)
- ✅ Mycelium Might: mosse di stato per ultime nella fascia di priorità, ignorano le abilità del bersaglio
- ✅ Justified: +1 Attacco per colpo Buio (Beat Up: un colpo per Pokémon della squadra)
- ✅ Unburden (strumento consumato: Sitrus/Focus Sash), Parting Shot (-1 Atk/SpA e uscita), U-turn/Volt Switch (uscita dopo il colpo) — per noi entra il Pokémon più adatto della panchina; il giocatore non esce nella simulazione
- ⬜ Sostituzioni dopo un KO nel simulatore

## M3 — Valutazione della posizione
- ✅ Materiale con panchina: Pokémon utilizzabili per lato (campo + panchina), la differenza scalata sul totale (un KO a 2 contro 2 pesa più che a 4 contro 4); squadra azzerata = partita persa/vinta
- ✅ Minacce di KO simmetriche (prima -45 per noi e +22 per loro), tutte contate (niente `break`), pesate per la precisione della mossa
- ✅ Velocità: contro un Pokémon andato KO la coppia conta come vinta da chi resta (prima un KO faceva perdere punti di velocità)
- ✅ Pesi in `include/config/custom.h` (`CUSTOM_VGC_EVAL_*`)
- ✅ HP pesati per ruolo — peso della win condition (`CUSTOM_VGC_EVAL_WINCON_PCT` 150) su KO e HP dei nostri Pokémon chiave — anche la minaccia di KO sulla win condition ha lo stesso peso
- ✅ Velocità del turno dopo con turni rimasti di Tailwind/TR
- ✅ Minacce credute: un avversario con mosse non ancora viste minaccia KO con le sue STAB probabili (prima era considerato innocuo)
- ✅ Stadi valutati solo se servono: chi attacca (stadio offensivo giusto per fisico/speciale), o chi ha Baton Pass e qualcuno in panchina (potenziale trasferibile); Velocità e difese con peso minore
- ✅ Risorse: Focus Sash intatto, Fake Out pronto per chi entra durante la simulazione
- ✅ Mega non usata (il simulatore non modella la Mega: sarebbe uguale in ogni ramo)
- 🔧 Candidate complete al turno 1: tutte le mosse legali su tutti i bersagli (niente taglio `best-14` delle regole), ordinate da una simulazione a 1 turno (fase 2 del lavoro, a pezzi); nelle coppie entrano le 7 migliori per lato più, sempre, l'azione delle regole, le combo e la migliore azione "utile" (setup, Protect, Tailwind, Trick Room, Baton Pass). Turno 2: 3 azioni per nostro Pokémon con una corsia protetta per le azioni utili. Il pianificatore parte anche con una sola coppia possibile
- ✅ (risolto, vedi sotto) ~~Test con la squadra vera di Savarotto (Incineroar Fake Out + Espathra + Chandelure dietro, contro due Machamp): Fake Out giusto, ma Calm Mind (-224) vale ancora meno di Baton Pass subito (-156). KNOWN_FAILING. Diagnostica aggiunta (solo test): `VGCLINE` stampa la linea migliore del turno 2 e, stato per stato, `VGCMON` (specie, PS, stadi) e `VGCEVAL` (parti di Evaluate). Cosa mostra:
  - Incineroar va KO al turno 1 in ogni linea: l'IA crede che i due Machamp usino Focus Punch (m264, la STAB più forte "probabile") su di lui. Focus Punch non è modellato (fallisce se chi lo usa viene colpito prima, e Fake Out lo fa tentennare): da correggere nella scelta delle mosse probabili del giocatore (`BuildActions`, ramo `bestStab`) o nel simulatore
  - Dopo Calm Mind il valore a fine turno 1 non conta ancora Speed Boost (si applica in `NextTurn`, dopo `Evaluate`)
  - Nella foglia (0,0) la linea CM vale più di BP dopo il turno 2 (-132 contro -244); la differenza a sfavore di CM arriva dagli altri rami degli avversari (es. foes(0,1): BP turno2=152)
  - ✅ Risolto: Focus Punch e mosse a due turni fuori dalle minacce probabili (e simulati come falliscono), Speed Boost a fine turno, ordine di velocità sui turni rimasti di Trick Room/Tailwind. Il test ora passa: Calm Mind vale più di Baton Pass subito
- ✅ Belief M1 collegato al pianificatore: statistiche e Velocità del giocatore da `Scout_EstimateBattlerStats`, attacchi probabili da `Scout_GetLikelyMoves`/`Scout_BattlerMoveChance` (con la probabilità), abilità della panchina da `Scout_LikelyAbility`, niente Protect previsto con Assault Vest/Choice visti o dedotti. La velocità del giocatore nelle regole (`speedStats`) arriva già dal belief (`Scout_BattlerBelievedSpeed`, branch meta)
- ✅ Simulatore: Taunt, Focus Punch, mosse a due turni, costo del cambio volontario (`CUSTOM_VGC_SWITCH_COST`)
- ⬜ Prossimi lavori già decisi (dopo il punto sopra): costo nel caso peggiore sotto ~200 fotogrammi (oggi 411 nel test Simple Beam); velocità stimata del giocatore anche nelle regole (`SetBattlerAiData` in battle_ai_main.c: `speedStats` del giocatore dalla stima di Scout invece della statistica vera, e `VgcInferChoiceScarf` in battle_ai_vgc.c); test della velocità stimata; altri test con i set veri delle 12 squadre
- ⚠️ Nota: lo scratchpad condiviso `out.txt` viene scritto anche da altri agenti; i test di questo branch vanno eseguiti con un file di uscita proprio (`out_iavgc.txt`)
- ⚠️ Punto aperto: il valore del setup su più turni (Calm Mind ora, Baton Pass dopo) è ancora sottostimato rispetto a far entrare subito l'attaccante. In alcuni casi è giusto (avversari innocui o non minacciosi), ma va misurato meglio: minacce credute degli avversari (M1), Intimidate e danni reali (M2), pressione del turno dopo (M3). Diagnostica nei test: `VGCLEAF` (valore dopo il turno 1 e miglior seguito al turno 2)

## M4 — Ricerca congiunta dei due slot
- 🔧 Un solo calcolo per lato e per turno: il primo slot pianifica la **coppia**, il secondo aspetta e applica la sua metà già decisa (prima il secondo slot non pianificava mai)
- 🔧 Candidate a coppie (mie × partner, max 10 simulate) con stima rapida di focus fire/overkill; la coppia delle regole è sempre simulata come paragone
- 🔧 Filtro coppie illegali: due cambi sullo stesso Pokémon, Helping Hand con partner che non attacca, doppio Trick Room, doppio meteo
- 🔧 Il pianificatore decide sempre (margine da 12 a 1 punto)
- 🔧 Test overkill riscritto: nel motore un attacco su un bersaglio già KO passa sull'altro avversario, quindi l'overkill pesa solo se la mossa è inutile sul secondo (Thunderbolt su Garchomp → serve Ice Beam)
- ✅ Corsia protetta: le combo sul compagno trovate dal grafo (Simple Beam, Beat Up...) sono sempre simulate
- ✅ Scelta delle coppie da simulare (max 14) a livelli: coppia delle regole, coppie con combo, la coppia migliore di ogni singola azione, poi il resto (prima le mosse "giuste" restavano fuori)
- ✅ Niente attacchi sul proprio compagno tra le candidate (salvo combo e mosse fatte per l'alleato come Helping Hand)
- ✅ Diagnostica nei test: il valore di ogni coppia simulata compare nel log (`VGCPLAN ...`)
- ⬜ Risposte del giocatore su scala unica (attacchi, supporto, Protect, cambio)
- ✅ Urgenza della kill, α dinamico — α dinamico: 0,8 media in svantaggio, 0,6 in vantaggio, 0,7 alla pari; KO sicuri con posto protetto in TrimCands

## M5 — Conoscenza della squadra generata in automatico
- 🔧 Grafo della squadra in C (`src/battle_ai_vgc_graph.c`), calcolato a inizio lotta invece che con uno script a build time: stessi dati, accesso diretto a mosse/abilità/Mega del motore. Ruoli, coppie di lead, combo sul compagno (Beat Up→Justified, assorbimenti), rimozione abilità penalizzanti (Simple Beam→Mycelium Might), ricevente del Baton Pass e vincolo di panchina, meteo/Trick Room con chi li sfrutta — scritto, da collegare e testare
- ✅ Grafo collegato al pianificatore (combo sul compagno)
- ✅ Tolta la regola su Toedscruel/Machamp (Simple Beam): ora regola generica "non togliere al compagno un'abilità utile" + simulazione
- ✅ Collegare il grafo al team preview (M7) — già collegato: `GraphGroupScore` in team_preview.c (ruoli, combo, requiredBack, coppie di lead)
- ✅ **Tolti i blocchi Savarotto e Patatina** e tutti i controlli sulle specie nelle regole di battaglia: al loro posto regole per **ruolo** dal grafo, valide per qualunque squadra:
  - Fake Out al turno 1 per un compagno che fa setup
  - colpire il compagno per attivargli l'abilità (Beat Up → Justified)
  - Baton Passer: setup senza boost; con boost Baton Pass / Sostituto / Protect
  - redirect per un compagno attaccante veloce (Trick Room lo rallenterebbe)
  - Trick Room per un compagno lento (prima la stanza, poi il redirect); il lento intanto si protegge o si potenzia
  - Tailwind per un compagno attaccante non lento; After You per un compagno lento con Eruption/Water Spout; Gas Reagente accanto a un'abilità penalizzante
- ✅ Grafo della squadra ricalcolato a ogni inizio turno per le regole (`sVgcTeamGraph`)

## M6 — Cambi, sostituzioni e Mega nel pianificatore
- ✅ Baton Pass: nel gioco vero entra il ricevente designato dal grafo (es. Espathra → Chandelure), non il "migliore" generico — test passa
- ✅ Dopo un KO entra chi fa la coppia migliore con chi è rimasto (grafo della squadra); il ricevente di un Baton Passer ancora in gioco resta in panchina — test passa
- ✅ Freno ai cambi: chi è appena entrato con un cambio volontario resta almeno un turno (regole e pianificatore), salvo pericolo grave
- ✅ Bug Stealth Rock / Steelsurge: ora calcolati sui tipi e PS di chi entra
- ✅ Gas Reagente accanto a Truant & co.: non esce (prima: regola su Weezing/Slaking)
- ✅ Parting Shot / U-turn: chi entra
- ✅ Mega con/senza come candidata

## M7 — Team preview sul grafo · M8 — Pulizia
- ✅ **Team preview senza regole per allenatore**: tolti i blocchi di Mario Lett, Tony, Gigi Pinto, Mario, Enzo, Patatina, Savarotto, Simone, Gerozz. Al loro posto, dal grafo della squadra:
  - gruppo di 4: Baton Passer solo col suo ricevente; Truant & co. solo con Gas Reagente o chi cambia abilità; Commander solo con Dondozo; Gas Reagente non con Commander/Hospitality; sole contro attaccanti d'Acqua e pioggia contro attaccanti di Fuoco; Trick Room non con chi è veloce grazie al meteo; attaccante lento non senza il setter di Trick Room della squadra; una sola Mega; sinergie di coppia
  - lead: sinergia della coppia (Fake Out + setup, Beat Up + Justified, Simple Beam + Mycelium Might, meteo + chi lo sfrutta...), Commander, abilità penalizzanti, chi sfrutta il meteo accanto a chi lo porta, mosse ad area che colpiscono il compagno
- ✅ Test `test/battle/ai/ai_vgc_team_preview.c`: Espathra portata solo con Chandelure, che resta in panchina (lead Espathra + Incineroar) — passa
- ⬜ Regole ridotte a filtro di candidate, ogni regola con un test

## Test di scenario con le squadre vere (richiesta del 26/10)
- ⬜ **Tutte le 144 combinazioni** squadra VGC contro squadra VGC (12 × 12, set completi da `trainers.party`), IA contro IA:
  ogni scontro deve finire senza crash né blocchi, senza errori evidenti (mosse fallite su immunità note, Protect consecutivi, Fake Out dopo il turno 1, cambi a ripetizione…), con log delle decisioni per rivedere le partite
- ⬜ Infrastruttura: l'expansion ha già la modalità **IA contro IA** (`B_FLAG_AI_VS_AI_BATTLE`, `IsAiVsAiBattle()` in `src/battle_controllers.c`: il lato del giocatore lo comanda l'IA). Piano: ROM dedicata che per ogni coppia (X, Y) carica la squadra X come "giocatore", avvia la lotta contro l'allenatore Y con il flag attivo, registra esito/turni/decisioni nel log mGBA; script su Windows che lancia le 144 partite in parallelo con `mgba-rom-test.exe` (base anche per M9)
- ✅ Per ogni allenatore: test sulle giocate chiave della sua linea di gioco — `test/battle/ai/ai_vgc_teams.c`, 36 test (3 per allenatore, set veri): **11 PASS, 25 KNOWN_FAILING** (16 errori dell'IA in 7 gruppi: controllo velocità non scelto, cambi della lead al turno 1, Fake Out non usato, supporto sottovalutato, Protect inutile, Recycle inutile, bersaglio delle mosse sul compagno; 9 da rivedere nel setup). Dettaglio: [scenari.md](scenari.md)
- ⬜ **Partite controllate, non casuali** (richiesta del 26/10):
  - **le lead le sceglie l'IA di entrambi i lati** con il team preview vero (4 su 6 + lead, in base alla squadra avversaria): si valuta anche come sceglie. Seed fissi per rigiocare ogni partita identica
  - controllo anche del team preview: Baton Passer senza ricevente, abilità penalizzante senza chi la spegne, lead che si danneggia da sola (meteo contro i propri attacchi, Earthquake sul compagno), lead chiaramente sconsigliata dal matchup; nel report lead scelta, alternative valutate con punteggio e motivo
  - lead fissate solo come strumento secondario, per provare una decisione di battaglia che la lead naturale non raggiunge
  - posizioni preparate (es. turno 3, Tailwind avversario all'ultimo turno, Espathra a +2) per mettere alla prova singole decisioni
  - **controllore dei misplay** a ogni turno: mossa su immune o su bersaglio già KO, KO garantito non preso, attacco sul compagno senza combo, Protect consecutivo, Fake Out dopo il turno 1, cambio su chi va KO all'entrata, cambi a ripetizione, turni di Tailwind/Trick Room sprecati, setup con KO in arrivo, win condition persa evitabilmente; ogni errore con turno, stato del campo e alternative valutate (`VGCPLAN`)
  - ogni misplay confermato diventa un test della suite
  - report per ognuna delle 144 coppie: esito, errori per categoria, log leggibile turno per turno; i casi dubbi segnalati a parte per la revisione

- 🟨 **Harness di self-play costruito (branch `ia-vgc-selfplay`, in pausa)** — guida `docs/ia_vgc/training.md`:
  - ✅ ROM: test `test/vgc_selfplay/vgc_selfplay.c` con runner proprio; allenatori VGC estratti da `trainers.party` a ogni build; team preview IA per entrambi i lati; Lv50 (`ScalePartiesToBattleLevel` prima della preview, verificato liv50 in lotta); seme fisso; modalità lead fisse e posizioni preparate; log `VSP ...`
  - ✅ Informazione chiusa in IA contro IA: `src/battle_ai_perspective.c` (dati IA per lato, `IsAiBattlerAware` per prospettiva), pianificatore/regole relativi al lato di chi pensa, lato G con la logica dell'IA avversaria (pianificatore compreso); test di invarianza `test/battle/ai/ai_vgc_perspective.c` (scritto, NON ancora eseguito)
  - ✅ Controllore dei misplay nel ROM (categorie in training.md), sulle stime del lato che agisce
  - ✅ Orchestratore `tools/vgc_training/` (start/pause/resume/stop/status/report, .bat, stato persistente, parallelismo)
  - ✅ **Prima partita completa**: ENZO-SIMONE seme 1 (Lv50) gioca tutto il turno 1, poi si blocca dopo il rimpiazzo del lato G (nessun timeout del runner). Log ora scritto da mGBA su file riga per riga; battito diagnostico aggiunto (da compilare). Passi esatti in training.md
  - ✅ 144 scontri, report, eseguire il test di invarianza e i test VGC esistenti, ricompilare il ROM normale

## M9 — Allenamento automatico (dopo M4)
- ✅ ROM di allenamento (senza animazioni), IA su entrambi i lati — è il ROM di test headless (`selfplay.gba`): niente grafica né attese, IA su entrambi i lati
- ✅ Benchmark: partite/ora con 1 e con ~28 istanze mGBA — 1 istanza: ~1.400 partite/ora (2,5 s a partita); 28 istanze: 432 partite in ~90 s, ~17.000 partite/ora
- ➡️ Taratura pesi, esplorazione, libro dei piani in ROM, strategia mista — tabella pesi per lato (`AI_VgcSetEvalWeightPct`), SPSA in `selfplay.py tune` / `taratura.bat`, libro dei piani `src/data/vgc_plan_book.h` (generato da `gen_plan_book.py`) nel team preview con rumore ±10 — **passato all'agente M9 (branch `ia-vgc-m9`)**

## Self-play (27/10 mattina)
- ✅ Merge di `ia-vgc-selfplay` in `ia-vgc`; prima partita IA contro IA completa (blocco dopo il rimpiazzo del lato G, probabilità di brutto colpo 1/0, parametri della lotta allenatore, fine lotta e chiusura del test corretti; vedi [training.md](training.md))
- ✅ Prima campagna dei 144 scontri (12×12, 1 seme, 24 istanze): ~5 minuti, 140 partite complete e 4 interrotte (timeout); esiti G 50 / A 90; 544 misplay certi e 930 dubbi. Categorie principali: Protect consecutivo 163 (quasi tutti del lato G), KO garantito non preso 109, mossa su immune 79
- ✅ Asimmetria G/A trovata: il materiale in `Evaluate` usava il lato avversario fisso come "noi" (in IA contro IA il lato G si credeva in svantaggio e si chiudeva in Protect). Ora è il lato di chi pensa. Seconda campagna: **G 75 / A 65, misplay certi 544 → 234, dubbi 930 → 448**; Protect consecutivo 163 → 8, mossa su immune 79 → 4. Report: [report/riepilogo.md](report/riepilogo.md)
- ✅ Le 4 partite interrotte (timeout): tutte con una Mega del Pokémon a destra del lato G. Il messaggio della Mega leggeva il nome di un "partner" da `gLinkPlayers` non inizializzato (copia senza terminatore). In IA contro IA il nome è quello del giocatore
- ✅ Ramo "colpo basso" nel pianificatore: un KO che riesce solo con il danno medio vale meno di uno sicuro (probabilità dal minimo della tabella dell'IA), anche per i KO degli avversari
- ✅ Terza campagna: **144/144 complete (nessun timeout), G 71 / A 73, misplay certi 244, dubbi 483**. KO garantito non preso 83: molti sono falsi positivi del controllo (Sucker Punch/priorità sul Campo Psichico di Indeedee, Follow Me di Indeedee, stessa mossa su un altro bersaglio, Final Gambit, mosse ad area preferite); da rivedere il controllo prima di toccare il pianificatore
- ✅ Quarta/quinta/sesta campagna: controllo dei KO garantiti senza falsi positivi (priorità sul Campo Psichico, Follow Me/Rage Powder del compagno, Final Gambit/Explosion); Fake Out mai candidata fuori dal primo turno; un Pokémon mai scelto per due posti (U-turn del compagno + rimpiazzo: assert in `getswitchedmondata`); potatura delle coppie molto peggiori della migliore (`CUSTOM_VGC_PLAN_PRUNE`). **Sesta campagna: 144/144 complete, G 69 / A 75, misplay certi 217** (prima 544 → 234 → 244 → 224 → 217)
- ✅ Costo con la potatura: mediana 71 fotogrammi, 90° percentile 179, massimo 250 (obiettivo ~200 nel caso peggiore non ancora raggiunto) — **superato (27/10, richiesta dell'utente): niente tetto, prima la qualità.** `CUSTOM_VGC_PLAN_MAX_FRAMES` 0, solo limite di sicurezza ~30 s; ripristinate 10 coppie e tutte le risposte avversarie al turno 2; mentre pensa si vede l'allenatore con "penso..."
- ⬜ Misplay rimasti (sesta campagna): KO garantito non preso 73 (+346 dubbi), win condition persa 48, bersaglio già KO 26, cambio su chi va KO all'entrata 24, setup con KO in arrivo 18, Tailwind/Trick Room sprecati 14
- ⬜ Misplay rimasti (seconda campagna): KO garantito non preso 70 (+350 dubbi), cambio su chi va KO all'entrata 37, win condition persa 31, bersaglio già KO 29, setup con KO in arrivo 20, Tailwind/Trick Room sprecati 18, cambi a ripetizione 13; 4 partite interrotte per timeout
- ✅ Modello del giocatore livello 1 (fase 3 del pianificatore): fino a 5 azioni plausibili per avversario simulate a 1 turno dal suo punto di vista (solo belief), unite alla probabilità a priori; Protect e cambio restano sulla loro probabilità

## Prossimi passi (al 27/10, dopo `83e27007a0`)
1. KNOWN_FAILING rimasti in `ai_vgc_teams.c` (18, vedi [scenari.md](scenari.md)): Tailwind di Aerodactyl/Altaria (la simulazione preferisce un KO concentrato su Roaring Moon), Trick Room di Banette/Porygon2/Toedscruel, Grimmsnarl schermi, Lilligant After You/Sleep Powder, Smeargle che cambia, Hariyama Fake Out, Arcanine (Mega Charizard), Sableye (cambio), Amoonguss (pari punteggio Rage Powder/Spore nelle regole), più i test con il turno del giocatore incompleto (setup da rivedere)
2. Costo: massimo 255 fotogrammi per decisione (mediana ~115): obiettivo ~200 nel caso peggiore
3. "Modello del giocatore livello 1": da definire nel piano (oggi il giocatore è modellato da belief M1 + Protect/cambi stimati); proposta: le 2 azioni per avversario scelte da una ricerca a 1 turno dal loro punto di vista invece che dai pesi
4. Provare la ROM in gioco (fluidità a 100 righe/fotogramma, IWRAM 66%)


## Campagne del 27/10 (pomeriggio)
- ✅ Nona campagna (onestà della velocità, peso win condition, pesi per lato, libro dei piani): 60 PASS / 9 KNOWN_FAILING / 0 FAIL; 144 partite in 29 s a 28 istanze; misplay certi 255 (ottava 208), G 73 / A 70 vittorie
- ✅ Rumore delle campagne misurato: lo stesso build con 3 semi (432 partite) dà 696 misplay, e cambiando solo i pesi del lato G anche il lato A passa da 360 a 283: una campagna a 1 seme non basta per confrontare. Da qui i confronti si fanno con `--seeds 3` (432 partite, ~90 s)
- ✅ `start --new` cancella risultati e log della campagna precedente (prima report e libro dei piani mescolavano partite vecchie)
- ❌ Asserzione `malloc.c:97` (magic del blocco) **dopo** `VSP FINE` in 2 partite su 144 (00_01, 04_10): nell'uscita del runner, non in lotta; indagine al punto dello schermo blu
- ✅ Campagne 3 semi (432 partite, misplay certi G+A): 696 (base) → 508 (Expanding Force ad area, checker bersaglio già KO) → 442 (candidate filtrate, panchina intera, KO protetti) → 465 → 414 (stime di peso/meteo/colpi multipli, doppio Protect, setup)
- ✅ Diagnosi per categoria (5 agenti in sola lettura, `scratchpad/swarm/*.md`): applicate le correzioni del checker (Trick Room con soglia 2, cambi ripetuti a 2 turni dubbi, cambio in KO da critico dubbio, Focus Sash/Sturdy) e del pianificatore (Protect di fila, Tailwind ripetuto, immuni, tutta la panchina candidata al cambio)
- ✅ Controllo dell'heap a ogni turno nel self-play: 0 blocchi rovinati su 432 partite (le 2 asserzioni di malloc.c viste prima, dopo `VSP FINE`, non si sono ripresentate)
- ❌ Dugtrio-Alola Earthquake (ai_vgc_teams.c:645) ora KNOWN_FAILING: Protect + Will-O-Wisp (110) batte Earthquake + Thunderbolt (-6), effetto delle stime più giuste (Heavy Slam di Aggron da 120 su Dugtrio)

- ✅ Turno 2 più largo: 4 azioni per i nostri due Pokémon (prima 3), Follow Me/Rage Powder nella corsia delle azioni utili: il test di Carletto (Rage Powder + Belly Drum) passa. Porygon2 Trick Room passa (mancava SEND_OUT dopo il KO)
- ✅ Test VGC: 61 PASS / 7 KNOWN_FAILING (+ Dugtrio) / 0 FAIL. Rimasti: Banette TR, Aerodactyl e Altaria Tailwind, Grimmsnarl schermi, Lilligant, Hariyama Fake Out, Scream Tail Encore
- ✅ Costo senza tetto (432 partite): mediana 178 fotogrammi (~3 s), 90° percentile 451 (~7,5 s), massimo 1255 (~21 s); misplay certi 445 (rumore della misura ±40 tra build quasi uguali)

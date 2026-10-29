# IA VGC — Checklist di sviluppo
n> Per riprendere il lavoro: [ripartenza.md](ripartenza.md).

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
- ✅ Schermo blu: causa probabile trovata e corretta (28/10): assert del motore su Parting Shot rimbalzato da Magic Bounce senza rimpiazzi (`battle_move_resolution.c`, test in `parting_shot.c`); nella ROM normale un assert mostra proprio uno schermo blu con file e riga. Prima: non riprodotto. Controllo dell'heap a ogni turno nel self-play; 720 partite lunghe (5 semi, fino a 60 turni, massimo 43) + 3 campagne da 432: 0 blocchi rovinati, 0 asserzioni, 0 opcode illegali. ROM normale: EWRAM 95,83%, IWRAM 67,77%, ROM 80,63%. Serve un salvataggio o il momento esatto
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

### M1 — contesto squadra (branch `ia-vgc-contesto`, 27/10)
- ✅ Contesto della squadra avversaria calcolato una volta in team preview dalle 6 specie (solo ciò che si vede + meta): `Scout_ComputeTeamContext` / `Scout_SetTeamContextFromParty` / `Scout_BattlerContext` in `src/battle_ai_scout.c` (API in `include/battle_ai_scout.h`). EWRAM: 2 contesti (~70 B l'uno, uno per squadra: in IA contro IA servono entrambi)
- ✅ Archetipi: Trick Room (setter + ≥2 lenti → spread minima: natura -Vel, 0 EV, 0 IV), Tailwind/squadra veloce (attaccanti a Velocità massima), meteo (setter + chi lo sfrutta: Swift Swim/Chlorophyll/Sand Rush/Slush Rush quasi certe, Weather Ball/Hurricane/Thunder/Solar Beam/Blizzard, rocce sul setter), campo (Surge Surfer, Rising Voltage/Expanding Force/Grassy Glide), redirect + setup (setup più probabile accanto al redirect)
- ✅ Strumenti: clausola degli strumenti (verificata su tutte le 12 squadre di `trainers.party`), assegnazione golosa per probabilità; una sola Mega per squadra; Focus Sash sui veloci fragili; Scarf più probabile senza controllo velocità, quasi mai su chi gioca sotto TR; Sitrus sui supporti; in lotta uno strumento visto/dedotto su un compagno non si assume su un altro
- ✅ Ruoli: TR quasi certa sul setter di una squadra TR, Tailwind sul setter di una squadra TW, Helping Hand sui supporti; lead probabili (`Scout_ContextLeadScore`, `Scout_ContextLikelyLeads`) e piano di gioco più pericoloso (`plan`, `planP`)
- ✅ Generatore `gen_vgc_meta.py`: dai dati Showdown anche `trSpread`/`fastSpread` (spread -Vel/0 EV e +Vel/max più usate con la quota) e `trMate`/`twMate` (compagni con Trick Room/Tailwind, dai "Teammates"). Le statistiche "chaos" non hanno set condizionati ai compagni: per il resto regole generali per ruolo
- ✅ Usato: team preview (stime del giocatore, profili, Matchup con strumenti/Mega della squadra, lead probabili, `ContextAnswerScore` = risposte del nostro gruppo/lead al piano avversario); belief (`Scout_EstimateBattlerStats`, `Scout_BattlerMoveChance`, `Scout_BattlerAbilityChance`, `Scout_AssumedHoldEffect`, `Scout_BattlerRoleChance`); pianificatore con agganci di una riga (panchina del giocatore, Mega, mosse probabili, profilo delle risposte, `Scout_BattlerPlanMoveBonus` nel modello del giocatore: TR/Tailwind del setter della squadra)
- ✅ Test: 9 nuovi (scout: TR → Velocità minima, Tailwind → massima, pioggia → Swift Swim, una sola Mega, clausola, lead probabili, belief in lotta, turno 1 Fake Out + setter TR → Taunt sul setter; team preview: scelta diversa contro TR e contro Tailwind). VGC: **71 PASS / 7 KNOWN_FAILING / 0 FAIL** (prima 62/7/0). Due test di `ai_vgc_teams.c` con `SEND_OUT`: Rillaboom di Enzo ora stimato con la spread veloce (4 PS, come il vero) va KO
- ✅ ROM normale (dopo il merge di `ia-vgc`): EWRAM 95,94%, IWRAM 69,53%, ROM 80,70%
- ✅ Campagna 432 (3 semi, 10 istanze): misplay certi 452 → 422 (rumore ±40), dubbi 1388 → 1481; mossa su immune 32 → 4, cambio in KO 98 → 80, setup con KO in arrivo 40 → 28; KO garantito non preso 136 → 158
- ✅ Verifica a specchio (`training.ps1 verifica-contesto`, contesto acceso per un lato e spento per l'altro, 864 partite, 3 semi): **51,0%, IC 95% [48,4; 53,6], p = 0,45 → non significativo**. Seconda verifica con 5 semi (base 20001, dopo il merge di `ia-vgc`, 1.439 partite): **49,2%, IC [47,2; 51,3], p = 0,46**. Insieme (8 semi, 1.151 coppie): **~49,9%**, nessun peggioramento e nessun miglioramento dimostrato: sulle 12 squadre del gioco il contesto cambia poche decisioni (il beneficio atteso è contro squadre di archetipo netto: TR, pioggia, Tailwind)
- ✅ Minacce esplicite (28/10): in team preview le 2 minacce principali del giocatore (duelli persi con i set del contesto); un gruppo senza nessuna risposta a una di loro paga. Per ogni nostro portato: minaccia 0..100 pesata per le lead probabili del giocatore → `TeamPreview_ThreatPrior`, prior del Protect del minacciato al turno 1 nel pianificatore (una riga)
- ✅ Peso delle risposte tarabile (`gScoutContextAnswerPct`, `verifica-contesto --answer-pct`). Taratura a 2 semi (575 partite l'una): 50% → 48,1%, **100% → 48,8%**, 200% → 47,7%, tutte nel rumore: resta 100
- ✅ Verifica finale a 8 semi (base 60001, 2.281 partite): **50,3%, IC 95% [48,7; 51,9], p = 0,73** → nessun peggioramento, nessun miglioramento dimostrato. Test VGC: 71 PASS / 7 KNOWN_FAILING / 0 FAIL. ROM: EWRAM 95,96%, IWRAM 69,53%, ROM 80,70%
- ✅ Previsione dei 4 che porterà il giocatore (28/10, team preview): punteggio per Pokémon (chi batte la nostra squadra, quota nelle sue lead probabili, pezzi del suo piano dal contesto, uso nel meta), poi i 15 gruppi di 4 con la coerenza del piano (setter senza chi ne approfitta o viceversa meno probabili) → probabilità di essere portato per ognuno. Usata per (a) pesare i duelli nella scelta dei nostri 4 (30% + 70% x probabilità), (b) valutare la lead anche contro la panchina probabile del giocatore, (c) `TeamPreview_BringPrior`: nel pianificatore il cambio previsto del giocatore va verso il Pokémon di panchina più probabile
- ✅ Verifica a 8 semi dopo la previsione (base 80001, 2.304 partite): **51,0%, IC 95% [49,3; 52,6], p = 0,27** (non significativo). Test VGC 71 PASS / 7 KNOWN_FAILING / 0 FAIL. ROM: EWRAM 95,97%, IWRAM 69,53%, ROM 80,71%. `test_veloci.ps1` qui ha dato uscite vuote (mGBA non partito dalle parti): test eseguiti con le 4 ROM per file
- ⬜ Non fatti (motivo): posizionamento sinistra/destra — nei doppi ogni avversario è bersagliabile da entrambi i posti e le mosse ad area colpiscono comunque tutti, quindi il posto conta solo per il rimpiazzo dopo un KO, che il pianificatore già sceglie; lead probabili nel pianificatore al turno 1 — al turno 1 la lead avversaria è già in campo e visibile, servirebbero solo per i cambi verso la panchina non vista (da fare)
- ⬜ Prossimi passi: (oggi implicite nei duelli 1v1 di team preview con set del contesto); verifica del libro dei piani dopo il contesto (`verifica-libro -- --seeds 3`), il libro è stato generato senza contesto; set condizionati veri (servirebbero dati per squadra, non i "chaos"); posizionamenti (chi sta a sinistra/destra) non modellati; usare `Scout_ContextLikelyLeads` anche nel pianificatore al turno 1 (oggi solo team preview); tarare i pesi di `ContextAnswerScore`

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
- ❌ Sostituzioni dopo un KO nel simulatore: provate, peggiorano (spente, `CUSTOM_VGC_PLAN_REPLACE_KO`)

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
- 🔧 Valore del setup (branch `ia-vgc-setup`, 28/10). Diagnostica accesa sui 6 test di setup in `ai_vgc_teams.c`, letta
  su GitHub (run 40675466841). Cause trovate e corrette nel simulatore, non con spinte:
  - **After You non simulato**: il compagno lento agiva comunque dopo gli avversari, quindi After You + Eruption valeva
    -418 (Torkoal KO prima di muovere). Ora `RunTurn` mette l'azione del compagno subito dopo After You; tolta la spinta
    delle regole (40/15) che ne faceva le veci. Il test di Lilligant passa.
  - **Yawn trattato come sonno immediato** (la mossa ha `nonVolatileStatus` = sonno): Protect + Yawn addormentava
    Ceruledge all'istante e il Trick Room del turno dopo sembrava sicuro (1146). Ora sonnolenza: si addormenta a fine del
    turno dopo; nella valutazione vale metà del sonno.
  - **Encore non simulato** (mossa senza effetto): ora blocca il bersaglio sull'ultima mossa per 3 turni; bloccato su una
    mossa che non attacca vale come chi dorme oltre l'orizzonte, senza boost offensivi né minacce di KO.
  - Schermi con **Light Clay**: 6 turni oltre l'orizzonte invece di 3.
  - Taunt, Yawn ed Encore azzerati per chi entra dalla panchina.
- ✅ Verificato e lasciato com'è: la saturazione delle foglie (968 = due avversari in campo KO: 600 Pokémon + 80
  materiale + 288 velocità, perché la coppia con un KO conta come vinta) colpisce allo stesso modo le linee con e senza
  setup (es. Tailwind 803-902 contro Rock Slide 968): il setup non perde lì ma al turno 1. Il bonus `theirs == 0`
  (±640) non scatta nei test (la panchina conta). Media/peggiore (alpha 6-8): con la sola media o il solo caso peggiore l'ordine non cambia, tranne Hatterene che con la sola media va in parità (Trick Room -120 contro Protect -116). Il termine di
  velocità di un Trick Room fresco arriva a ~1100 (4 coppie x 4 turni x corsa ai KO): resta un punto da misurare nel
  self-play, non toccato per non rompere i test di Trick Room che passano.
- KNOWN_FAILING rimessi, con la giocata dell'IA difendibile: Aerodactyl (Rock Slide KO su Charizard Y + Fake Out su
  Roaring Moon), Grimmsnarl (Spirit Break 4x KO su Roaring Moon, Roseli nascosta), Hatterene e Mega Banette (il setter va
  KO prima di Trick Room sotto il fuoco concentrato previsto), Scream Tail (Helping Hand + Rock Slide manda KO Incineroar
  sotto Trick Room; il bersaglio `opponentRight` della diagnostica era quello di Helping Hand, non di Encore).

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
- ❌ Risposte del giocatore su scala unica: provate due volte, peggiorano (spente, `CUSTOM_VGC_PLAN_FOE_SINGLE_SCALE`)
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
- 🟨 **Regole ridotte a filtro di candidate, ogni regola con un test** (branch `ia-vgc-regole`, 27/10, in corso)
  - Fatto: `AI_VgcDoubles` (src/battle_ai_vgc.c) ora è solo **filtro** (`AI_VgcFilterAction`: illegale/senza PP/bloccata,
    Fake Out dopo il turno 1, Protect ripetuto, priorità nel Campo Psichico, immunità note anche per le mosse ad area,
    stato bloccato (polvere, Prankster sul Buio, tipo, stato già presente, terreno, Safeguard, abilità note), campo già
    attivo (Tailwind, Gravity, meteo, terreno, schermi, Trick Room che chiude il nostro), senza effetto (Recycle, Baton
    Pass senza panchina, Substitute/Belly Drum senza PS, mosse per il compagno senza compagno), bersaglio sbagliato
    (attacco/stato sul compagno senza combo, cambio d'abilità utile, Instruct/After You sull'avversario)) + **spinta**
    leggera (`AI_VgcActionPrior`, punti della valutazione: Gravity 30, Quick Guard 30, Instruct 40, After You 40/15 (tolta il 28/10: ora simulato),
    meteo per la panchina 20, Contrary +30 / boost perso -20, Neutralizing Gas Protect 30). Tutte le altre regole a
    punteggio tolte (le decide la simulazione).
  - Pianificatore: stesso filtro sulle candidate del turno 1 e sull'azione delle regole (`RulesAction`), spinta nelle
    candidate (fase 2) e nelle coppie (`FinishJob`), posto protetto per Fake Out al primo turno, After You/Instruct sul
    compagno tra le candidate; `AI_VgcPlanTestPrior` per i test.
  - Confronto a specchio: regole vecchie tenute solo nel ROM dei test (`src/battle_ai_vgc_legacy.h`, bit
    `rulesLegacy` per lato nei parametri del self-play), comando `training.ps1 verifica-regole -- --seeds 3 --parallel 10`.
  - Test: `test/battle/ai/ai_vgc_rules.c` (21 test, squadre vere Lv50; set spostati in `ai_vgc_sets.h`). Il controllo
    "pari punteggio" del test runner è saltato per AI_FLAG_VGC_DOUBLES (decide il pianificatore).
  - Regressioni chiuse (28/10, CI run 40673139947: test-vgc verde, 115 PASS, 7 KNOWN_FAILING, 0 FAIL):
    - pianificatore, **fase 4**: le coppie (nostra azione, azione del partner) rimaste dopo `TrimCands` si simulano
      INSIEME a 1 turno contro le risposte più probabili (stessi costi e spinte di `FinishJob`) e quel valore ordina le
      coppie da simulare per intero. Prima la stima era la somma dei valori singoli contro un riferimento del compagno:
      senza i bonus delle regole restavano fuori Eruption + Giga Drain su Great Tusk (Torkoal) ed Earthquake +
      Thunderbolt su Arcanine (Dugtrio-Alola). Tolta `PairSynergy`;
    - simulazione: **Disguise** (il primo colpo costa 1/8 dei PS): Mimikyu andava KO al turno 1 e Trick Room perdeva;
    - modello del giocatore (fase 3): non mette in conto le nostre mosse di stato sui suoi Pokémon (con Taunt sul
      setter come azione delle regole il suo Trick Room non veniva previsto e Taunt non serviva);
    - `SimStatusImmune` in `BuildActions` (turno 2, modello del giocatore): niente Spore su un Erba, Will-O-Wisp su un
      Fuoco & co. con i tipi in lotta (campagna ia-vgc: 65 mosse di stato su immuni; il filtro copre già il turno 1);
    - spinta **Fake Out al primo turno** +25 (il tentennamento annulla anche le azioni non previste dalla simulazione);
    - test: Belly Drum con HP(50) (la Sitrus all'entrata portava 100 PS sopra metà: il filtro era giusto), "no Protect
      twice" e "no second Trick Room" senza panchina del giocatore (KO = SEND_OUT non scritto), nuovi test "no
      Will-O-Wisp into two Fire types" e spinta Fake Out; tolti i KNOWN_FAILING di Hariyama Fake Out e Altaria Tailwind.
  - Da fare: campagna a specchio 432 coppie (`verifica-regole`, --parallel 10), ROM normale, commit finale e inventario.
- 🔧 Regole ridotte a filtro di candidate, ogni regola con un test — branch `ia-vgc-regole`: test verdi, manca la campagna a specchio

## Test di scenario con le squadre vere (richiesta del 26/10)
- ✅ **Tutte le 144 combinazioni** squadra VGC contro squadra VGC (12 × 12, set completi da `trainers.party`), IA contro IA:
  ogni scontro deve finire senza crash né blocchi, senza errori evidenti (mosse fallite su immunità note, Protect consecutivi, Fake Out dopo il turno 1, cambi a ripetizione…), con log delle decisioni per rivedere le partite
- ✅ Infrastruttura: l'expansion ha già la modalità **IA contro IA** (`B_FLAG_AI_VS_AI_BATTLE`, `IsAiVsAiBattle()` in `src/battle_controllers.c`: il lato del giocatore lo comanda l'IA). Piano: ROM dedicata che per ogni coppia (X, Y) carica la squadra X come "giocatore", avvia la lotta contro l'allenatore Y con il flag attivo, registra esito/turni/decisioni nel log mGBA; script su Windows che lancia le 144 partite in parallelo con `mgba-rom-test.exe` (base anche per M9)
- ✅ Per ogni allenatore: test sulle giocate chiave della sua linea di gioco — `test/battle/ai/ai_vgc_teams.c`, 36 test (3 per allenatore, set veri): **11 PASS, 25 KNOWN_FAILING** (16 errori dell'IA in 7 gruppi: controllo velocità non scelto, cambi della lead al turno 1, Fake Out non usato, supporto sottovalutato, Protect inutile, Recycle inutile, bersaglio delle mosse sul compagno; 9 da rivedere nel setup). Dettaglio: [scenari.md](scenari.md)
- ✅ **Partite controllate, non casuali** (richiesta del 26/10):
  - **le lead le sceglie l'IA di entrambi i lati** con il team preview vero (4 su 6 + lead, in base alla squadra avversaria): si valuta anche come sceglie. Seed fissi per rigiocare ogni partita identica
  - controllo anche del team preview: Baton Passer senza ricevente, abilità penalizzante senza chi la spegne, lead che si danneggia da sola (meteo contro i propri attacchi, Earthquake sul compagno), lead chiaramente sconsigliata dal matchup; nel report lead scelta, alternative valutate con punteggio e motivo
  - lead fissate solo come strumento secondario, per provare una decisione di battaglia che la lead naturale non raggiunge
  - posizioni preparate (es. turno 3, Tailwind avversario all'ultimo turno, Espathra a +2) per mettere alla prova singole decisioni
  - **controllore dei misplay** a ogni turno: mossa su immune o su bersaglio già KO, KO garantito non preso, attacco sul compagno senza combo, Protect consecutivo, Fake Out dopo il turno 1, cambio su chi va KO all'entrata, cambi a ripetizione, turni di Tailwind/Trick Room sprecati, setup con KO in arrivo, win condition persa evitabilmente; ogni errore con turno, stato del campo e alternative valutate (`VGCPLAN`)
  - ogni misplay confermato diventa un test della suite
  - report per ognuna delle 144 coppie: esito, errori per categoria, log leggibile turno per turno; i casi dubbi segnalati a parte per la revisione

- ✅ **Harness di self-play costruito (unito in `ia-vgc`)** — guida `docs/ia_vgc/training.md`:
  - ✅ ROM: test `test/vgc_selfplay/vgc_selfplay.c` con runner proprio; allenatori VGC estratti da `trainers.party` a ogni build; team preview IA per entrambi i lati; Lv50 (`ScalePartiesToBattleLevel` prima della preview, verificato liv50 in lotta); seme fisso; modalità lead fisse e posizioni preparate; log `VSP ...`
  - ✅ Informazione chiusa in IA contro IA: `src/battle_ai_perspective.c` (dati IA per lato, `IsAiBattlerAware` per prospettiva), pianificatore/regole relativi al lato di chi pensa, lato G con la logica dell'IA avversaria (pianificatore compreso); test di invarianza `test/battle/ai/ai_vgc_perspective.c` (scritto, NON ancora eseguito)
  - ✅ Controllore dei misplay nel ROM (categorie in training.md), sulle stime del lato che agisce
  - ✅ Orchestratore `tools/vgc_training/` (start/pause/resume/stop/status/report, .bat, stato persistente, parallelismo)
  - ✅ **Prima partita completa**: ENZO-SIMONE seme 1 (Lv50) gioca tutto il turno 1, poi si blocca dopo il rimpiazzo del lato G (nessun timeout del runner). Log ora scritto da mGBA su file riga per riga; battito diagnostico aggiunto (da compilare). Passi esatti in training.md
  - ✅ 144 scontri, report, eseguire il test di invarianza e i test VGC esistenti, ricompilare il ROM normale

## M9 — Allenamento automatico (dopo M4)
Branch `ia-vgc-m9` (27/10). Guida e numeri: [training.md](training.md#allenamento-automatico-m9).
- ✅ ROM di allenamento: ROM dei test headless (niente animazioni, attese di testo, intro/transizioni, suoni) + **modalità silenziosa** (`Test_MgbaPrintf` scarta senza formattarla la diagnostica `VGC...` e la cronaca della lotta); l'orchestratore riscrive solo i 76 byte dei parametri in un ROM per istanza invece di 32 MB a partita
- ✅ Benchmark (IA del mattino): **1 istanza 1.572 partite/ora** (prima 1.380); **28 istanze 19.228 partite/ora** (prima 12.234; 24 istanze 17.947, 32 istanze 18.767). IA senza tetto al costo: ~14.000 partite/ora a 28 istanze
- ✅ Taratura dei pesi `CUSTOM_VGC_EVAL_*` (7 pesi): SPSA a specchio (pesi in prova su G e su A, 2 semi fissi, 1.152 partite a passo, penalità 0,01 per misplay), 20 passi = 23.040 partite sul ROM senza tetto; validazione su 8 semi mai visti (2.304 partite): **50,3%, IC 95% [48,8; 51,8], p = 0,70 → non significativo, `custom.h` invariato** (i pesi sono su un altopiano). Stato su disco, pausa/ripresa/stop provati sui .bat
- ✅ Esplorazione: team preview con rumore (±500 su gruppi e lead) e coppie vicine alla migliore ai turni 1/2, `eps` 30%; 4.320 partite
- ✅ Libro dei piani in ROM (`src/data/vgc_plan_book.h`, const, ~4 KB; RAM 4 byte + esclusioni Bo3): chiave = hash della nostra squadra + classe di velocità dell'avversario visibile in team preview (niente nomi di allenatori, niente dati nascosti); 196 piani per 48 chiavi con lead, retro e azioni congiunte dei turni 1/2
- ✅ Strategia mista: piano scelto a caso tra quelli entro 12 punti dal migliore, peso (tasso − migliore + 13)²; bonus a gruppo/lead in team preview e alle mosse del piano ai turni 1/2; Bo3: le lead perse nella serie escluse, piano riscelto a ogni gara
- ✅ Verifica del libro su 8 semi mai visti, a specchio (2.303 partite): **52,7% per il lato con il libro, IC 95% [51,1; 54,3], p = 0,001**
- ⬜ Da fare: validazione dei pesi con più semi (`--val-seeds 32`) o pesi nuovi (es. stati, Fake Out, Sash); libro rigenerato dopo ogni cambio importante dell'IA (`esplora.bat` → `libro.bat` → `compila.bat` → `verifica_libro.bat`); classi dell'avversario più fini quando ci saranno più squadre

## Prova in gioco (27-28/10)
- ✅ Overlay "penso...": palette e testo corretti; ⏳ rifatto con entrata da destra, sotto le barre dei PS, sequenza completa senza lampeggio
- ✅ Fake Out sull'alleato escluso (`AI_VgcIsPartnerHitCombo`: attacco sul compagno solo se ne attiva abilità/strumento)
- ✅ Messaggio di fine lotta aspetta A
- ⚠️ Spore sulla minaccia maggiore: sonno pesato sulla minaccia, test ancora KNOWN_FAILING (serve il valore del sonno oltre l'orizzonte)
- ⏳ Bug del pianificatore: mosse note del giocatore rinumerate in `BuildActions` (slot sbagliato per danni ed efficacia)

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
- ✅ (superato) Misplay rimasti (seconda campagna): KO garantito non preso 70 (+350 dubbi), cambio su chi va KO all'entrata 37, win condition persa 31, bersaglio già KO 29, setup con KO in arrivo 20, Tailwind/Trick Room sprecati 18, cambi a ripetizione 13; 4 partite interrotte per timeout
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

## Seconda tornata del 27/10 (pomeriggio)
- ✅ Valutazione: risorsa rimasta di velocità — nei turni in cui Tailwind/Trick Room rovesciano l'ordine naturale, la coppia vale fino a 3 volte di più in base alla corsa ai KO (CanKoNextTurn nei due sensi); schermi nuovi valgono anche oltre l'orizzonte (3 turni per avversario che attacca con quella categoria). Schermi tra le azioni utili
- ❌ Nessuno dei KNOWN_FAILING di velocità si sblocca: con la traccia di tutte le coppie si vede che Tailwind + Fake Out sul bersaglio "giusto" vale -336 contro 607 di Rock Slide + lo stesso Fake Out. Il simulatore preferisce davvero l'attacco: il problema è nei turni simulati (il giocatore modellato agisce prima del Tailwind), non nella scelta delle coppie
- ❌ Provate e spente (flag in custom.h): sostituzioni dopo un KO nel simulatore (`CUSTOM_VGC_PLAN_REPLACE_KO`: misplay 445 → 515, costo x2, 2 test rotti) e seconda passata delle coppie (`CUSTOM_VGC_PLAN_REPAIR_PAIRS`: nessun KNOWN_FAILING risolto, Savarotto Beat Up rotto)
- ✅ Dugtrio-Alola: diagnosi. Il belief stima Arcanine al massimo della Velocità (161, la spread più usata) contro Dugtrio a 160: per l'IA Flare Blitz arriva prima e, con Rock Slide di Aggron dopo la Focus Sash, Dugtrio cade se attacca. Il vero Arcanine di Simone è Adamant 184 Spe (127): la decisione dipende da un'informazione nascosta. Resta KNOWN_FAILING finché non si modella l'incertezza della velocità (pareggi e quasi-pareggi come rami)
- ✅ Checker: minaccia di KO "prevedibile" solo con mosse al 100% di precisione (win condition persa: partite con il misplay 153 → 106)
- ✅ Report: tutte le coppie valutate nelle pagine per partita (prima le prime 4); pagine per partita in .gitignore
- ✅ Campagna (432 partite): partite con misplay per categoria 356 (prima 412), misplay certi 438; G/A 225/206; costo mediana 186 fotogrammi (~3 s), 90° percentile 474, massimo 1064 (~18 s). Test: 61 PASS / 8 KNOWN_FAILING / 0 FAIL
- ⬜ Risposte del giocatore su scala unica; regole ridotte a filtro di candidate con un test ciascuna: non fatte
- ✅ Velocità incerta: per ogni avversario la cui stima e una Velocità plausibile nel quarto opposto dell'intervallo (belief o specie, mai la vera) danno ordini diversi con un nostro Pokémon, il turno 1 si gioca anche con l'ordine rovesciato (ramo al 35% in MissPenalty, `CUSTOM_VGC_PLAN_SPEED_ALT_PCT`). Dugtrio-Alola Earthquake passa
- ❌ Rollout alla foglia (4 turni avidi sulla linea migliore, `CUSTOM_VGC_PLAN_ROLLOUT_TURNS`): implementato e spento. Nessun KNOWN_FAILING risolto, rotti Savarotto Calm Mind e Carletto Fake Out, misplay 356 → 386
- ❌ Risposte del giocatore su scala unica (`CUSTOM_VGC_PLAN_FOE_SINGLE_SCALE`): implementate e spente. Protect/cambio con il costo del tempo restano a 1 turno sopra gli attacchi: il modello prevede Protect quasi sempre (peso 100), rotti 2 test, misplay peggiori
- ✅ Filtri delle candidate con un test ciascuno: Fake Out dopo il primo turno (c'era), Tailwind con Tailwind attivo, attacco su un immune noto (nuovi in ai_vgc_doubles.c). Protect di fila: filtro attivo, test non scritto (non si può forzare il primo Protect dell'IA)
- ✅ Stato: 63 PASS / 7 KNOWN_FAILING / 0 FAIL; campagna 432: partite con misplay 378 (base 356, rumore ±20-30), G/A 243/245 certi; costo mediana 176 fotogrammi, massimo 1006
- ❌ Seconda prova (27/10 sera): scala unica con costo del tempo più alto (Protect = un cambio, cambio = 1,5) e rollout più leggero (2 turni, peso 1/3): 4 test rotti (Slaking, Arcanine Extreme Speed, mossa precisa, Savarotto). Restano spenti; il codice delle due varianti è in battle_ai_vgc_plan.c dietro i flag
- ✅ Merge di `ia-vgc-m9` (libro dei piani validato) in ia-vgc: 64 PASS / 7 KNOWN_FAILING / 0 FAIL, campagna 373 partite con misplay (rumore come prima)
- ➡️ Assert savedAttackerCount (Parting Shot rimbalzato da Magic Bounce, Chatot su Hatterene): diagnosi passata all'agente `ia-vgc-bug` — Magic Bounce salva l'attaccante (battle_move_resolution.c:3962), chi rimbalza esce con BattleScript_PartingShotEscape e il ripristino in MoveEndClearBits (5190) non arriva; nel gioco senza assert il contatore a 4 bit cresce e scrive fuori da savedBattlerAttacker

## Misplay rimasti (28/10, branch `ia-vgc-misplay`)
Base: `ia-vgc` 1e15bb49c2 (checker con "mossa su immune" corretto), campagna 432 partite rifatta con la stessa ROM: **404 misplay certi** (la stessa ROM ridà quasi gli stessi numeri: 423 → 404 è tutto il checker su IMMUNE 18 → 0).

Cause trovate (log VGCPLAN/VGCLINE/VGCFOE delle campagne, script in scratchpad):
- **KO garantito non preso (173)**: in 81 casi un avversario va KO comunque nello stesso turno — 50 sono scambi (la coppia manda KO l'*altro* avversario con danno combinato, Helping Hand o Fake Out + attacco: il checker guarda solo la mossa singola) e 31 sul bersaglio stesso. Negli altri la coppia KO è sempre simulata (171/173) e perde per letture (Fake Out previsto su chi attacca, Protect/cambio del bersaglio, Rage Powder) o perché il KO dell'altro avversario vale di più (Copperajah contro Indeedee).
- **Win condition persa (102)**: 45 casi in cui il pianificatore *sa* che va KO ma Protect/cambio costano un altro Pokémon (Expanding Force ad area, cambio che fa morire il compagno); il resto è previsione degli avversari (doppio attacco sulla win condition non previsto, Choice Scarf non ancora dedotta). Il checker non guarda l'ordine di velocità: un KO "prevedibile" da chi si muove dopo il nostro KO non è una minaccia.
- **Cambio su chi va KO (80)**: 16-20 sacrifici voluti (il Pokémon consumato esce, quello sano prende il colpo: la valutazione contava solo i PS in campo, i PS portati in panchina valevano zero), ~25 Pokémon lasciati a pochi PS dalla mediana del danno (nessun ramo "tiro alto"), il resto previsioni sbagliate dei bersagli.
- **Setup con KO in arrivo (30)**: Pokémon già condannati (azioni a pari valore, vinceva la prima) o boost che valevano pieni anche su chi va KO al turno dopo.
- **Protect consecutivo (16)**: quasi tutti falsi positivi del checker — Protect forzato da Encore (Chatot), Protect precedente fallito, Protect scelto da addormentato.
- **Trick Room appena messo**: misurato sui log (VGCCAND, valori a 1 turno): TR vale 700-1400 punti più di un attacco (fino a ~1600 di escursione: 4 coppie × 3-4 turni × corsa ai KO). Con un KO a ~450, TR valeva 2-3 KO: 49 casi dubbi "Trick Room invece di un KO sicuro".

Correzioni (src/battle_ai_vgc_plan.c, interruttori in custom.h):
- ✅ PS in panchina (`CUSTOM_VGC_EVAL_BENCH_HP_PCT`): chi esce con un cambio/pivot porta i suoi PS, chi entra lascia i suoi (con il peso della win condition).
- ✅ Ramo "colpo alto" (`CUSTOM_VGC_PLAN_HIGH_ROLL`) simmetrico al "colpo basso": un KO possibile solo con un tiro sopra la mediana pesa per la sua probabilità, per entrambi i lati.
- ✅ Spareggio tra coppie di valore identico: vince l'attacco, prima il KO sicuro.
- ✅ Tetto alla corsa ai KO dei turni futuri (`CUSTOM_VGC_EVAL_SPEED_RACE_CAP` = 200, un KO).
- ✅ Boost scontati per la probabilità di andare KO al turno dopo.
- ✅ `CUSTOM_VGC_MIRROR_FIXES` (spento): spegne le correzioni per il lato col bit di verifica-regole, per il confronto a specchio.

Risultati (432 partite, misplay certi):

| Categoria | Base | Lotto 1 (PS panchina, colpo alto, spareggio) | Lotto 2 (+ tetto corsa, boost) |
|---|---|---|---|
| KO garantito non preso | 173 | 169 | 153 |
| Win condition persa | 101 | 102 | 91 |
| Cambio su chi va KO | 80 | 76 | 81 |
| Setup con KO in arrivo | 30 | 34 | 30 |
| Protect consecutivo | 16 | 21 | 17 |
| Bersaglio già KO | 4 | 2 | 3 |
| Altri (Fake Out tardivo = Encore, velocità, cambi) | 0 | 2 | 3 |
| **Totale** | **404** | **406** | **378** |
| Dubbi | 1525 | 1500 | 1395 (KO non preso per supporto 769 → 681) |

Test su GitHub: 116 PASS / 6 KNOWN_FAILING a ogni lotto (come la base).

Falsi positivi del checker da valutare (owner: coordinatore): KO non preso quando la coppia manda KO l'altro avversario (danno combinato/Helping Hand) o quando chi attacca subisce Fake Out; Protect consecutivo con Encore, Protect precedente fallito o chi dorme; Fake Out tardivo sotto Encore; minaccia di KO "prevedibile" senza guardare chi si muove prima.

Verifica a specchio (ROM con `CUSTOM_VGC_MIRROR_FIXES` acceso, `verifica-regole`, 864 partite, correzioni accese contro spente sugli stessi semi): **51,0% di vittorie, IC95 [48,4; 53,7], p = 0,44** — non peggiora (miglioramento non dimostrato). Il report `report/verifica_regole.md` è di questa prova (correzioni dei misplay, non regole M8). Report `riepilogo.md`: campagna del lotto 2.

### Secondo giro (28/10 pomeriggio, dopo il merge di `ia-vgc` 39407f9c72)
Nuova base (ROM di GitHub di `ia-vgc`, checker con KO combinati della coppia e mosse forzate da Encore/sonno escluse): **318 misplay certi** (KO non preso 102, win condition 92, cambio su chi va KO 81, setup 30, Protect di fila 10, bersaglio già KO 3).

- **Cambio su chi va KO**: tutti gli 81 cambi sono scelti dal pianificatore (VGCPLAN con `k2` nella coppia scelta), nessuno dalle regole di battle_ai_switch.c né dalle sostituzioni dopo un KO (quelle il checker non le conta). In ~55 casi la simulazione dava chi entra vivo: le risposte previste colpivano lo slot con un'altra mossa (o una sola delle due), oppure la stima del danno su chi entra era bassa.
  - ❌ Stima dei danni più completa per chi entra (Body Press con la Difesa, Foul Play, Psyshock, Facade/Guts con Flame Orb, Knock Off, Expanding Force sul terreno, Tough Claws, Booster Energy) + KO sicuro sempre tra le risposte avversarie: 318 → 354; senza la seconda parte 340 (cambi 89, win condition 99). Tolte entrambe (`CUSTOM_VGC_PLAN_FOE_KO_REPLY` resta in custom.h spento nel commit della prova, codice poi rimosso).
  - ✅ Rischio del cambio (`CUSTOM_VGC_PLAN_SWITCH_RISK`): un cambio paga il KO di chi entra (30% se una risposta plausibile di un solo avversario, tra tutte le candidate del modello e col tiro alto, lo manda KO sul suo slot; 15% se servono entrambi; peso della win condition). **Cambio su chi va KO 81 → 61**, totale 318 → 321 (setup 30 → 23, KO non preso 102 → 113, win condition 92 → 99: dentro il rumore tra build di ±40).
- **Win condition persa**: nei casi con Protect disponibile il pianificatore sceglie l'attacco con margini ampi (es. Sucker Punch di Kingambit 1414 contro Protect 380): non è il costo di Protect ma la previsione (l'avversario che manda KO è previsto KO prima o su un altro bersaglio; Choice Scarf non dedotta). Tentata la risposta "KO sicuro" sempre tra quelle previste: peggiora (sopra). Non risolto.
- Test su GitHub: 116 PASS / 6 KNOWN_FAILING a ogni build.
- Verifica a specchio finale (tutte le correzioni accese contro spente, 864 partite): **52,3%, IC95 [49,7; 54,9], p = 0,084**.

Falsi positivi del checker rimasti:
- Protect consecutivo in finali bloccati senza mosse utili (02_02_s27: Banette/Mimikyu contro Lickitung a +6 Def, 9 casi in una partita; Banette decide con le sole regole perché il compagno ha già il suo lavoro).
- Attacco sul compagno con una mossa ad area forzata da Choice Scarf (Rotom Discharge, 3 casi) contato sul compagno che sta uscendo con un cambio.
- Win condition: minaccia "prevedibile" senza guardare se la nostra mossa più veloce manda KO prima chi minaccia.
- Cambio su chi va KO: i sacrifici voluti (chi esce sarebbe andato KO comunque e la coppia rimasta salva l'altro Pokémon) restano certi.

### Terzo giro (28/10 sera, dopo il merge di `ia-vgc` ea86606a89)
Nuova base (ROM di GitHub di `ia-vgc`, checker con Protect/immune nei finali bloccati, aree sul compagno che esce o bloccato da Choice, win condition non certa se la coppia manda KO prima chi minaccia): **303 misplay certi** (KO non preso 114, win condition 89, cambio su chi va KO 64, setup 22, bersaglio già KO 8, Protect di fila 5).

- **Modello del giocatore (win condition)**: il livello 1 valuta ogni azione del giocatore solo contro l'azione delle regole dei nostri; se le regole mandano KO prima chi minaccia (o lo coprono), il suo attacco sulla win condition vale poco e sparisce dalle risposte previste. Provata la media con il turno contro di noi fermi (`CUSTOM_VGC_PLAN_FOE_VS_IDLE`):
  - tutte le azioni, media 1/2: **303 → 285** (KO non preso 114 → 90, win condition 89 → 98, cambi 64 → 66), ma il test "Patatina - Altaria sets Tailwind" fallisce (Dragon Pulse + Shadow Sneak su Roaring Moon invece di Tailwind + Protect: il modello non prevede più il Protect del giocatore);
  - solo gli attacchi: 4 test di squadra rotti; tutte le azioni con peso 1/3: Altaria rotto; solo le mosse (non Protect/cambio): Altaria rotto.
  - ❌ Lasciato spento (CI verde prima di tutto). Da riprendere: decidere se il test di Altaria vale più del guadagno in campagna, o separare la previsione di Protect del giocatore dalla media.
- (b) Choice Scarf: la deduzione onesta dall'ordine dei turni c'è già nel belief (M1); non toccata. (c) Peso sulla win condition: la valutazione del giocatore usa già il peso della nostra win condition (150%) quando la manda KO.
- **KO garantito non preso rimasti (114)**: 38 con un KO avversario comunque nel turno, 3 con Fake Out subito, 2 con Protect del bersaglio. Nei casi col distacco più grande il KO non era possibile: chi attacca va KO prima di muoversi (Machamp a 1 PS o a 94 PS lento contro un avversario più veloce che lo manda KO: sceglie Bullet Punch, che ha priorità) o la coppia sceglie Helping Hand + KO sull'altro avversario. Nessun errore vero del pianificatore trovato; falsi positivi per il checker sotto.
- KNOWN_FAILING: nessuno sbloccato.
- Codice attivo invariato dal secondo giro: vale la verifica a specchio del secondo giro (52,3%, p = 0,084).

Falsi positivi del checker rimasti:
- KO non preso quando chi attacca va KO prima di agire (avversario più veloce col suo KO sicuro): la mossa del KO non parte, la priorità sì.
- KO non preso quando il compagno usa Helping Hand e la coppia manda KO l'altro avversario (il danno di Helping Hand non entra nel KO combinato).

Decisione su `CUSTOM_VGC_PLAN_FOE_VS_IDLE` (dopo il merge di `ia-vgc` c5344eaaf3): flag per lato nel self-play (`CUSTOM_VGC_MIRROR_FOE_IDLE`, variante "tutte le azioni, media 1/2", quella del 285), verifica a specchio con 6 semi: **1727 partite valide, 50,7% per il lato con il flag, IC95 [48,9; 52,5], p = 0,47**. Sotto la soglia (IC sopra 50% o almeno 51% con meno misplay): ❌ resta spento, il test di Altaria Tailwind resta un test normale (verde). Specchio spento di nuovo.

### Quarto giro (28/10 notte): storia del giocatore nel modello livello 1
- Base (ROM di 0c07194722, checker c5344eaaf3): **286 misplay certi** (KO non preso 96, win condition 89, cambi 64, setup 23).
- ✅/❌ **Storia dei bersagli** (`CUSTOM_VGC_PLAN_FOE_HISTORY`, battle_ai_vgc.c `VgcNoteTarget` + `AI_VgcFoeTargetBias`, spinta in PickFoeActions): per lato, con entrambi i nostri in campo, quante volte il giocatore colpisce il più danneggiato e (quando una sola lo è) la nostra win condition; fino a 60 punti agli attacchi coerenti con la tendenza (≥3 osservazioni). Protect e cambi avevano già la loro memoria (`VgcBehaviour`). Campagna **286 → 258** (KO non preso 96 → 81, win condition 89 → 85, cambi 64 → 60, setup 23 → 20); verifica a specchio 6 semi (1726 partite): **50,3%, IC95 [49,1; 51,5], p = 0,60**. Sotto la soglia (≥51% o IC sopra 50%): **spento**.
- ❌ Spinta sugli attacchi verso il nostro Pokémon più pericoloso per il giocatore (`CUSTOM_VGC_PLAN_FOE_THREAT_FOCUS`, ThreatPct): rompe il test "Mario - Hariyama uses Fake Out on Whimsicott" (bersaglio sbagliato); spenta (0).
- ⚠️ CI con cache (fab509a2e9): la cache presa con restore-keys da un'altra build aveva oggetti più nuovi dei sorgenti cambiati e la ROM non si ricompilava (ELF identica al commit prima: test e campagna su codice vecchio). Corretto su ia-vgc (af8c260c93).
## KO garantiti non presi (29/10, branch `ia-vgc-ko`)
Base: ROM di GitHub di `ia-vgc` af8c260c93, campagna 432 partite: **286 misplay certi, KO non preso 96** (win condition 89, cambio su chi va KO 64, setup 23, bersaglio già KO 8).

Misure sui log (VGCFOE = le 2 risposte previste per avversario, contro le azioni vere; ~12 000 previsioni):
- **Protect previsto** al 16% avviene all'8-9%, al 23-28% avviene al 18-21%, al 33% al 25-29%: sopravvalutato x0,65.
- **Cambio previsto** al 27% avviene al 13% (x0,45).
- **Fake Out previsto** al 50% avviene al 45%, al 90% al 66% (x0,8, contando solo la mossa).
- Il modello è troppo sicuro in generale: le 2 risposte previste coprono circa metà delle azioni vere (mossa prevista al 50% avviene al 25%).
- KO non preso: la coppia con il KO è simulata in 95/96 casi; il distacco dalla coppia scelta ha mediana 61 punti (38 casi entro 50, 78 entro 200). Circa un quarto dei casi perde per una lettura (Fake Out su chi attacca, cambio del bersaglio), il resto perché l'albero valuta di più un'altra linea (danno sull'altro avversario, U-turn, Trick Room).

Correzioni (src/battle_ai_vgc_plan.c, blocchi "KO sicuri (ia-vgc-ko)"; interruttori in custom.h):
- ✅ Premio alle coppie con un KO garantito (stessi criteri del controllore) nei rami del turno 1 in cui il bersaglio va KO (`CUSTOM_VGC_PLAN_SURE_KO_PCT` = 35% del valore di un KO).
- ✅ Letture tarate: nei rami in cui il KO è bloccato da Protect/cambio del bersaglio o da Fake Out su chi attacca, la parte che il modello sopravvaluta (`CUSTOM_VGC_PLAN_KO_CALIB_PROTECT/SWITCH/FAKE_OUT` = 65/45/80) conta come KO riuscito. Il modello del giocatore (PickFoeActions) non è toccato.
- ✅ La coppia migliore di ogni azione con un KO garantito è sempre simulata per intero (`CUSTOM_VGC_PLAN_KO_PAIRS`); la potatura tiene conto del premio.
- ✅ Ramo "colpo basso" con il rapporto esatto minimo/mediana (`CUSTOM_VGC_PLAN_LOW_ROLL_EXACT`): con la percentuale troncata un KO con minimo = PS restava a 1 PS e pagava una penalità.
- ✅ `CUSTOM_VGC_MIRROR_KO` (spento): correzioni spente per il lato col bit di verifica-regole, per lo specchio.

| | Base | Premio 75% | **Premio 35% (tenuto)** |
|---|---|---|---|
| KO garantito non preso | 96 | 59 | **71** |
| Win condition persa | 89 | 106 | 103 |
| Cambio su chi va KO | 64 | 56 | 56 |
| Setup con KO in arrivo | 23 | 27 | 24 |
| Bersaglio già KO | 8 | 13 | 10 |
| Altri | 6 | 3 | 8 |
| **Totale certi** | **286** | **264** | **272** |
| Dubbi | 1034 | 890 | — |
| Specchio (correzioni accese contro spente) | — | 48,0% su 864, IC95 [45,5; 50,5], p = 0,12 | **49,4% su 1728, IC95 [47,6; 51,2], p = 0,50** |

CI verde a ogni build. Il premio al 75% toglie più KO ma lo specchio scende (win condition persa +17: attacca invece di proteggere): tenuto il 35%, neutro allo specchio.

KO non presi rimasti (71): distacco mediano 126 punti, scelte che l'albero preferisce di molto (U-turn/Close Combat di Slither Wing, Trick Room, attacco sull'altro avversario). Falsi positivi del controllore visti: Focus Sash nota al pianificatore e non al controllore (Kecleon contro Hariyama al turno 1, 3 casi per seme), Mega evoluzione nello stesso turno (Malamar), Reflect con Prankster prima dell'attacco (Grimmsnarl). Da capire: Dondozo contro Incineroar al turno 1 (Fake Out + cambio previsti), dove il KO sicuro non riceve il premio (il pianificatore non lo vede come garantito).
Il branch `ia-vgc-ko-specchio` serve solo alla verifica a specchio: non va unito.

### Secondo giro KO (29/10 notte, dopo il merge di `ia-vgc` 8aad2170ac)
- ✅ Win condition e premio: nei rami del turno 1 in cui uno dei nostri va KO il premio scende al 50% (`CUSTOM_VGC_PLAN_KO_LOSS_PCT`), a 0 se è la win condition; niente premio se la win condition attacca mentre un avversario che resta in campo (non quello che mandiamo KO) ha un KO sicuro su di lei (`CUSTOM_VGC_PLAN_KO_WINCON_GUARD`). Misurato prima: nei casi di win condition persa il distacco dalla miglior coppia con Protect/cambio ha mediana 174-228 punti (solo 13-24 casi su ~85 entro 70, il premio): l'aumento 89 -> 103 era in buona parte rumore tra build.
- ✅ Diagnostica `VGCKO` nei test: per ogni coppia simulata danno minimo, PS, precisione, KO sicuro e premio.
- Dondozo contro Incineroar al turno 1: non si ripresenta (Dondozo non parte più in lead senza Tatsugiri, team preview di `ia-vgc`); nelle 432 partite i KO con minimo >= PS e precisione piena senza premio sono solo esclusioni volute (Final Gambit, Focus Sash, Campo Psichico).

| 432 partite | Base (ia-vgc 8aad2170ac, premio senza guardia) | Con la guardia |
|---|---|---|
| KO garantito non preso | 86 | 83 |
| Win condition persa | 92 | 86 |
| Cambio su chi va KO | 55 | 48 |
| Setup con KO in arrivo | 27 | 29 |
| Bersaglio già KO | 9 | 11 |
| **Totale certi** | **269** | **260** |

Specchio (tutte le correzioni dei KO accese contro spente, `ia-vgc-ko-specchio2`, 1728 partite): **49,4%, IC95 [47,7; 51,1], p = 0,49** — neutro, con meno misplay. CI verde. Rumore tra build sulla stessa categoria: ±15 (KO non preso 71 e 86 con lo stesso codice del premio).

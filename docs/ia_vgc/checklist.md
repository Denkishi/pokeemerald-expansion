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
  - ⬜ velocità del giocatore in `gAiLogicData->speedStats` (codice base dell'expansion) → in M1 con il belief
- ✅ Suite di test `test/battle/ai/ai_vgc_doubles.c` attiva. WSL compila soltanto; i test si eseguono su Windows con `tools/mgba/mgba-rom-test.exe`
- ✅ Installate con msys2 `libepoxy-0.dll` e `libwinpthread-1.dll`: `mgba-rom-test.exe` gira su Windows (aggiungere `C:\msys64\mingw64\bin` al PATH)
- ✅ **Risultati: 13 PASS, 0 FAIL, 0 KNOWN_FAILING** (+1 test team preview)
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
- ⬜ Schermo blu: serve sapere quando succede / un salvataggio per riprodurlo
- ⬜ Misura dei frame di "pensiero" per turno

## M1 — Belief del giocatore (esperienza da giocatore VGC)
- ⬜ Base di conoscenza in ROM da statistiche Showdown: spread, ruoli, moveset, strumenti, abilità con frequenze
- ⬜ Cambi d'abilità (Mega, forme, Skill Swap/Simple Beam), statistiche e tipi delle Mega
- ⬜ Restringimento da osservazioni: velocità, danni (Choice, Assault Vest, Life Orb), Focus Sash
- ⬜ Abilità d'ingresso non attivata → esclusa
- ⬜ Memoria Bo3 collegata al belief

## M2 — Simulatore corretto
- 🔧 Protect di fila: il secondo ora fallisce (prima riusciva sempre)
- 🔧 Fake Out fallisce del tutto dopo il primo turno in campo (prima faceva danno)
- 🔧 Trick Room (5 turni) e Tailwind (4 turni) ora scadono; turni rimasti letti dal gioco
- 🔧 Sonno con contatore: ci si sveglia (prima si dormiva per sempre)
- ⬜ Cache danni reale (`AI_CalcDamage`) al posto della stima
- ✅ Stadi separati Atk/Def/SpA/SpD/Spe, letti dal gioco a inizio turno; i danni della tabella dell'IA si riscalano per gli stadi cambiati nella simulazione; le mosse di potenziamento applicano gli stadi veri (Calm Mind +1 SpA/+1 SpD, Dragon Dance +1 Atk/+1 Spe...) invece di un "+2" generico
- ✅ Speed Boost a fine turno
- ✅ Baton Pass nella simulazione: entra il ricevente designato con tutti gli stadi
- ✅ Intimidate all'entrata nella simulazione (con Clear Body, Inner Focus, Scrappy, Guard Dog, Defiant, Competitive, Clear Amulet); per il giocatore se la specie lo porta di solito
- ✅ Scottatura e paralisi (inflitte da Will-O-Wisp/Thunder Wave con le immunità; dimezzano attacchi fisici / Velocità)
- ✅ Schermi (Reflect, Light Screen, Aurora Veil sotto neve) piazzati nella simulazione
- ✅ Precisione con ramo "mancato": la simulazione applica il colpo pieno (un KO resta un KO) e per ogni azione imprecisa del turno 1 si rigioca il turno col colpo mancato, togliendo (1 - precisione) x differenza. Prima `danno x precisione` trasformava un KO all'80% in "mai KO" e una mossa ad area al 90% che manda KO due bersagli in nessun KO. Precisione dalla tabella dell'IA (Compound Eyes, No Guard...). Test: Rock Slide 90% su due bersagli > Scratch sicuro; Scratch > Stone Edge a pari KO
- ✅ **Bug trovato e corretto**: se un avversario non aveva attacchi plausibili noti, la simulazione dava per **certo** il suo Protect e qualunque nostro attacco sembrava inutile (tutte le coppie con lo stesso valore). Ora resta sempre l'alternativa "non si protegge" con la probabilità complementare
- ✅ **Causa trovata** (test "accurate move", Scratch che non mandava KO il bersaglio a 5 PS): non era il simulatore. `ScalePartiesToBattleLevel` (tutte le lotte a Lv50) ricalcolava le statistiche e curava i PS anche nei test: `HP(5)` diventava 250 PS e `Speed(10)` veniva ignorato. Ora nei test valgono PS e statistiche scritti dal test (nel gioco resta tutto a Lv50). Diagnostica `VGCDMG`/`VGCFOE` nel log dei test
- ✅ Mosse ad area sul compagno (Earthquake, Surf, Discharge: TARGET_FOES_AND_ALLY) con Protect, Wide Guard e immunità d'abilità (Levitate, Flash Fire, Water/Volt Absorb, Lightning Rod, Storm Drain, Sap Sipper, Telepathy...; Mold Breaker le ignora). Test con il set vero del Rotom-Mow di Simone
- ⬜ Mega, terreni, Sostituto
- ✅ Cambi d'abilità a metà turno (Simple Beam, Worry Seed, Skill Swap, Gastro Acid, Entrainment)
- ✅ Mycelium Might: mosse di stato per ultime nella fascia di priorità, ignorano le abilità del bersaglio
- ✅ Justified: +1 Attacco per colpo Buio (Beat Up: un colpo per Pokémon della squadra)
- ⬜ Unburden, Parting Shot, U-turn, Sostituto passato con Baton Pass
- ⬜ Sostituzioni dopo un KO nel simulatore

## M3 — Valutazione della posizione
- ✅ Materiale con panchina: Pokémon utilizzabili per lato (campo + panchina), la differenza scalata sul totale (un KO a 2 contro 2 pesa più che a 4 contro 4); squadra azzerata = partita persa/vinta
- ✅ Minacce di KO simmetriche (prima -45 per noi e +22 per loro), tutte contate (niente `break`), pesate per la precisione della mossa
- ✅ Velocità: contro un Pokémon andato KO la coppia conta come vinta da chi resta (prima un KO faceva perdere punti di velocità)
- ✅ Pesi in `include/config/custom.h` (`CUSTOM_VGC_EVAL_*`)
- ⬜ HP pesati per ruolo
- ⬜ Velocità del turno dopo con turni rimasti di Tailwind/TR
- ✅ Minacce credute: un avversario con mosse non ancora viste minaccia KO con le sue STAB probabili (prima era considerato innocuo)
- ✅ Stadi valutati solo se servono: chi attacca (stadio offensivo giusto per fisico/speciale), o chi ha Baton Pass e qualcuno in panchina (potenziale trasferibile); Velocità e difese con peso minore
- ✅ Risorse: Focus Sash intatto, Fake Out pronto per chi entra durante la simulazione
- ⬜ Mega non usata (il simulatore non modella la Mega: sarebbe uguale in ogni ramo)
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
- ⬜ Urgenza della kill, α dinamico

## M5 — Conoscenza della squadra generata in automatico
- 🔧 Grafo della squadra in C (`src/battle_ai_vgc_graph.c`), calcolato a inizio lotta invece che con uno script a build time: stessi dati, accesso diretto a mosse/abilità/Mega del motore. Ruoli, coppie di lead, combo sul compagno (Beat Up→Justified, assorbimenti), rimozione abilità penalizzanti (Simple Beam→Mycelium Might), ricevente del Baton Pass e vincolo di panchina, meteo/Trick Room con chi li sfrutta — scritto, da collegare e testare
- ✅ Grafo collegato al pianificatore (combo sul compagno)
- ✅ Tolta la regola su Toedscruel/Machamp (Simple Beam): ora regola generica "non togliere al compagno un'abilità utile" + simulazione
- ⬜ Collegare il grafo al team preview (M7)
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
- ⬜ Parting Shot / U-turn: chi entra
- ⬜ Mega con/senza come candidata

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
- ⬜ Per ogni allenatore: test sulle giocate chiave della sua linea di gioco (lead, combo, cambi)
- ⬜ **Partite controllate, non casuali** (richiesta del 26/10):
  - **le lead le sceglie l'IA di entrambi i lati** con il team preview vero (4 su 6 + lead, in base alla squadra avversaria): si valuta anche come sceglie. Seed fissi per rigiocare ogni partita identica
  - controllo anche del team preview: Baton Passer senza ricevente, abilità penalizzante senza chi la spegne, lead che si danneggia da sola (meteo contro i propri attacchi, Earthquake sul compagno), lead chiaramente sconsigliata dal matchup; nel report lead scelta, alternative valutate con punteggio e motivo
  - lead fissate solo come strumento secondario, per provare una decisione di battaglia che la lead naturale non raggiunge
  - posizioni preparate (es. turno 3, Tailwind avversario all'ultimo turno, Espathra a +2) per mettere alla prova singole decisioni
  - **controllore dei misplay** a ogni turno: mossa su immune o su bersaglio già KO, KO garantito non preso, attacco sul compagno senza combo, Protect consecutivo, Fake Out dopo il turno 1, cambio su chi va KO all'entrata, cambi a ripetizione, turni di Tailwind/Trick Room sprecati, setup con KO in arrivo, win condition persa evitabilmente; ogni errore con turno, stato del campo e alternative valutate (`VGCPLAN`)
  - ogni misplay confermato diventa un test della suite
  - report per ognuna delle 144 coppie: esito, errori per categoria, log leggibile turno per turno; i casi dubbi segnalati a parte per la revisione

## M9 — Allenamento automatico (dopo M4)
- ⬜ ROM di allenamento (senza animazioni), IA su entrambi i lati
- ⬜ Benchmark: partite/ora con 1 e con ~28 istanze mGBA
- ⬜ Taratura pesi, esplorazione, libro dei piani in ROM, strategia mista

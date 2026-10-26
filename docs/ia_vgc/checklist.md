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
- ✅ **Risultati: 6 PASS, 0 FAIL, 0 KNOWN_FAILING**
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
- ⬜ Intimidate, Mega, terreni, schermi, Sostituto, scottatura/paralisi, precisione
- ✅ Cambi d'abilità a metà turno (Simple Beam, Worry Seed, Skill Swap, Gastro Acid, Entrainment)
- ✅ Mycelium Might: mosse di stato per ultime nella fascia di priorità, ignorano le abilità del bersaglio
- ✅ Justified: +1 Attacco per colpo Buio (Beat Up: un colpo per Pokémon della squadra)
- ⬜ Unburden, Parting Shot, U-turn, Sostituto passato con Baton Pass
- ⬜ Sostituzioni dopo un KO nel simulatore

## M3 — Valutazione della posizione
- ⬜ Materiale con panchina, HP pesati per ruolo
- ⬜ Velocità del turno dopo con turni rimasti di Tailwind/TR
- ⬜ Minacce credute per avversari senza mosse viste
- ✅ Stadi valutati solo se servono: chi attacca (stadio offensivo giusto per fisico/speciale), o chi ha Baton Pass e qualcuno in panchina (potenziale trasferibile); Velocità e difese con peso minore
- ⬜ Risorse (Fake Out, Sash, Mega)

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
- ⬜ Rimozione blocchi Savarotto (Espathra/Baton Pass: serve prima Baton Pass nel simulatore e il potenziale trasferibile, M2/M3) e Patatina, altri `species ==`

## M6 — Cambi, sostituzioni e Mega nel pianificatore
- ✅ Baton Pass: nel gioco vero entra il ricevente designato dal grafo (es. Espathra → Chandelure), non il "migliore" generico — test passa
- ⬜ Parting Shot / U-turn, doppio KO, freno ai cambi ripetuti
- ⬜ Bug Stealth Rock calcolato su chi esce
- ⬜ Mega con/senza come candidata

## M7 — Team preview sul grafo · M8 — Pulizia
- ⬜ Team preview senza regole per allenatore
- ⬜ Regole ridotte a filtro di candidate, ogni regola con un test

## M9 — Allenamento automatico (dopo M4)
- ⬜ ROM di allenamento (senza animazioni), IA su entrambi i lati
- ⬜ Benchmark: partite/ora con 1 e con ~28 istanze mGBA
- ⬜ Taratura pesi, esplorazione, libro dei piani in ROM, strategia mista

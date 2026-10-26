# IA VGC — Piano di implementazione

Stato di avanzamento: [checklist.md](checklist.md).

Obiettivo: un'IA per i doppi VGC che giochi come un giocatore esperto (lead, core, controllo velocità, focus
fire, predict, gestione dei piani su più turni, memoria Bo3) **senza regole scritte per singoli allenatori o
singoli Pokémon** e **senza leggere i dati nascosti del giocatore** (liste chiuse), entro i limiti del GBA.

"Funzionante al 100%" qui significa una cosa misurabile: **ogni errore osservato diventa un test, e la suite di
test VGC passa sempre**. Un'IA che non sbaglia mai non esiste (nemmeno i campioni del mondo); un'IA che non
ripete mai lo stesso errore sì.

---

## 0. Stato attuale (audit del 26/10/2026)

File coinvolti: `src/battle_ai_vgc.c` (regole), `src/battle_ai_vgc_plan.c` (pianificatore), `src/battle_ai_scout.c`
(lettura del giocatore), `src/team_preview.c` (4 su 6), `src/data/vgc_gameplans.h` (piani per allenatore),
`src/battle_ai_switch.c` (cambi), `src/battle_controller_opponent.c` (aggancio multi-frame).

### Perché sbaglia spesso

| # | Causa | Dove |
|---|---|---|
| 1 | **I due slot non decidono insieme.** Il primo slot che pensa non conosce la mossa del partner (tutte le regole "di coppia" sono vuote per lui); il pianificatore al turno 1 fissa l'azione del partner invece di cercarla. | `battle_ai_vgc.c:456-475`, `battle_ai_vgc_plan.c:1000-1034` |
| 2 | **Il pianificatore quasi mai decide.** Sovrascrive le regole solo se migliora di ≥12 punti: in pratica comandano le regole a punteggi magici. | `battle_ai_vgc_plan.c:1113` |
| 3 | **La valutazione ignora la panchina.** Cambiare un Pokémon ferito con uno sano sembra "guadagnare HP"; un KO vale sempre 160 qualunque cosa resti. | `battle_ai_vgc_plan.c:594-650` |
| 4 | **Avversari senza mosse viste = innocui.** Nessuna minaccia di KO finché non rivelano un attacco → turni iniziali troppo aggressivi. | `battle_ai_vgc_plan.c:566-577` |
| 5 | **Protect e cambi del giocatore quasi mai simulati.** Pesi su scale diverse (danno% fino a 200+ contro probabilità 0-100), si tengono solo 2 azioni per avversario. | `battle_ai_vgc_plan.c:678-899` |
| 6 | **Simulatore incompleto o sbagliato:** Protect consecutivo riesce sempre, Fake Out colpisce anche dopo il turno 1, sonno eterno, Tailwind/Trick Room non scadono, un solo `boost` generico (Danza Drago non dà Velocità), niente Intimidate, Mega, terreni, schermi, Sostituto, scottatura/paralisi, precisione, Unburden, cambi d'abilità, Beat Up→Justified, Baton Pass. | `battle_ai_vgc_plan.c:412-556` |
| 7 | **Danno stimato rozzo** fuori dalla cache dell'IA: ignora stadi, abilità, strumenti Choice, schermi, terreni, multi-colpo. | `battle_ai_vgc_plan.c:298-327` |
| 8 | **Regole hard-coded e in conflitto:** blocchi per `TRAINER_SAVAROTTO` e `TRAINER_PATATINA`, controlli `species ==` (Torkoal, Toedscruel, Machamp, Slaking/Weezing), mosse per nome, decine di bonus magici che si sommano senza limiti. | `battle_ai_vgc.c:950-1158`, `1604-1633`, `battle_ai_switch.c:1428-1437` |
| 9 | **Cambi senza piano:** il Pokémon che entra è sempre il "migliore generico" (o il primo libero nel pianificatore); Baton Pass, Parting Shot e sostituzioni dopo un KO non sanno chi deve entrare. Bug: il danno da Stealth Rock è calcolato su chi esce. | `battle_ai_switch.c:1349-1351`, `1394-1493` |
| 10 | **Mega decisa prima del pianificatore** e mai ricontrollata. | `battle_ai_main.c:455`, `battle_ai_vgc.c:1655-1689` |
| 11 | **Flag in conflitto:** gli allenatori VGC hanno anche `Prediction`, `Prefer Highest Damage Move`, `Try To 2hko`, che spingono contro la logica VGC; `PREDICT_MOVE` sceglie la mossa del giocatore leggendo le sue mosse vere. | `src/data/trainers.party` (12 allenatori) |
| 12 | **Imbrogli residui:** statistiche vere, abilità vera, HP veri dei Pokémon in panchina del giocatore; presenza dello strumento; mosse vere in team preview. | `battle_ai_vgc_plan.c:184,190-212,865`, `battle_ai_main.c:759`, `team_preview.c:1302,410-412,779,1320` |
| 13 | **Nessun test VGC.** Nessun modo di sapere se una modifica migliora o peggiora. | `test/battle/ai/` |

### Cosa è già buono e si tiene
- Simulazione multi-frame su 3 turni con ordine d'azione ricalcolato a metà turno.
- Media pesata + caso peggiore sulle risposte del giocatore.
- Scouting onesto da learnset e tabella meta, deduzione della Choice Scarf dall'ordine di turno.
- Team preview con ricerca su tutte le quartine, predizione dei lead del giocatore, memoria Bo3 per Pokémon.

---

## 1. Architettura target

Un solo cervello per turno, che decide **insieme** i due slot. Le regole non danno più punteggi finali: servono
solo a **generare e potare** le candidate. Decide sempre la simulazione.

```
  Inizio turno (una volta per lato, non per slot)
        │
  [A] Belief: cosa so del giocatore      ← mosse viste + meta (ROM) + deduzioni + memoria Bo3
        │
  [B] Cache danni                        ← AI_CalcDamage reale per (attaccante, mossa, bersaglio)
        │                                   sui set creduti, una volta sola
  [C] Candidate                          ← per slot: top-K mosse/bersagli/cambi/Mega + corsia protetta
        │                                   (Protect, Fake Out, redirect, speed control, mosse sul partner)
        │                                   → coppie (slot L × slot R), filtro coppie illegali
  [D] Simulazione (multi-frame)          ← coppie IA × coppie giocatore, 2-3 turni, regole complete
        │
  [E] Valutazione della posizione        ← materiale con panchina, HP pesati per ruolo, velocità del turno
        │                                   dopo, risorse, potenziale trasferibile, minacce credute
  [F] Scelta                             ← valore atteso + caso peggiore con α dinamico (vantaggio/svantaggio)
        │
  Cache decisione → slot L e slot R leggono la stessa scelta (anche cambi, bersagli, Mega)
```

Conoscenza della squadra (lead, core, ricevente del Baton Pass, Mega, vincoli di panchina) **generata a build time**
da `trainers.party` in una tabella ROM: zero costo a runtime, zero regole scritte a mano.

---

## 2. Milestone

Ogni milestone compila, passa `make check` e lascia l'IA giocabile. L'ordine conta: senza misura (M0) e senza
simulatore corretto (M1-M3) la ricerca congiunta (M4) amplifica gli errori invece di correggerli.

### M0 — Misura e fondamenta *(prerequisito di tutto)*

1. **Liberare IWRAM** (oggi 87%): `gLink` e `gRfu` in EWRAM, buffer RFU in versione ROM
   (`RFU_API_BUFF_SIZE_ROM`, `copyInterruptToRam = FALSE`). Circa 9,6 KB liberi.
2. **Suite di test VGC** `test/battle/ai/ai_vgc_*.c` (`DOUBLE_BATTLE_TEST` + `EXPECT_MOVE`/`EXPECT_SWITCH`):
   una posizione = una risposta attesa. Si parte dalle posizioni della sezione 4 e si aggiunge **un test per ogni
   errore visto in partita**. Dovrà passare a ogni commit.
3. **Budget a tempo, non a numero di simulazioni.** Oggi `AI_VgcPlanStep` conta 24 "simulazioni" per frame
   (`battle_ai_vgc_plan.c:1161`), ma il costo di ognuna non è limitato: dentro ci sono `StartLookAhead` (4 chiamate a
   `BuildActions`), `CanKoNextTurn` e le scansioni dei learnset di `Scout_GetSpeciesProfile`. Un solo frame può costare
   molti frame di CPU: il gioco scatta e i menu del giocatore non rispondono. Correzione: fermarsi quando
   `REG_VCOUNT` supera una soglia (es. metà frame) e riprendere al frame dopo; profili scout e mosse credute
   calcolati **una volta** in `Begin` e messi in cache.
4. **Schermo blu:** riprodurlo con mGBA (salvataggio + passi), poi correggere. Precedente: il crash del 14/10
   (`7de77232d7`) era un indice fuori tabella (`MOVE_UNAVAILABLE` usato come mossa). Da verificare anche lo stack:
   con IWRAM all'87% restano circa 4 KB per lo stack, e il pianificatore copia `PlanState` (~200 B) più volte in stack
   (`battle_ai_vgc_plan.c:924,1003,1190`) sopra le chiamate già pesanti di `AI_CalcDamage`.
5. **Flag degli allenatori VGC:** togliere `Prediction`, `Prefer Highest Damage Move`, `Try To 2hko` (li sostituisce
   la simulazione). `Know Opponent Party` resta: in VGC le 6 specie si vedono al team preview.
6. **Togliere tutti gli imbrogli** della tabella §0 #12: panchina del giocatore caricata dal belief (specie visibile,
   statistiche stimate), niente lettura di `item != ITEM_NONE`, niente mosse vere in team preview.

*Accettazione:* IWRAM ≤ 60%; suite VGC esistente e verde sul comportamento attuale noto; zero accessi a dati
nascosti (verificato con grep su `gPlayerParty`, `GetBattlerParty(player)`, `gBattleMons[player].moves/item/ability/stat`
fuori da percorsi "visti").

### M1 — Belief del giocatore (liste chiuse)

Principio: l'IA non vede i dati nascosti, ma ha **la stessa esperienza di un giocatore VGC esperto**. Base di
conoscenza in ROM (generata da statistiche d'uso Showdown + learnset), per specie:
- spread più usate (2-3, con frequenza) e ruoli (supporto, sweeper, setter, pivot, redirect...);
- moveset probabili con frequenza, strumenti probabili, Tera/Mega probabile;
- abilità possibili con frequenza, e **cambi d'abilità**: Mega (es. Mega Chandelure perde Flash Fire), forme,
  Skill Swap/Simple Beam ecc.; statistiche e tipi della forma Mega;
- minacce note: con chi fa coppia di solito, cosa minaccia di OHKO in lead.

`struct AiBeliefMon` per ogni Pokémon del giocatore (EWRAM), alimentata da `battle_ai_scout.c` e da questa base:
- **Mosse:** viste = certe; non viste = dal meta (ROM) con probabilità; Protect presunto finché non si deduce
  Choice o Assault Vest.
- **Statistiche:** intervallo [min, max] da statistiche base e livello; valore "probabile" dalla spread tipica del
  ruolo. **Restringimento** dalle osservazioni: ordine di turno (velocità), danno subito e inflitto (attacco/difesa,
  Choice Band/Specs, Assault Vest, Life Orb), Focus Sash su attaccanti fragili a HP pieni.
- **Abilità:** maschera delle possibili; si elimina un'abilità d'ingresso che non si è attivata.
- **Tabella meta generata** da statistiche d'uso Showdown con uno script in `tools/` → `src/data/vgc_meta.h` (ROM).
- **Bo3:** la memoria serie esistente (`SeriesMonMemory`) alimenta il belief nelle gare 2 e 3.

*Accettazione:* test "Choice Scarf dedotta dall'ordine", "Assault Vest dedotto dal danno → niente Protect previsto",
"Intimidate non attivato → abilità alternativa", "mossa fuori meta vista in gara 1 ricordata in gara 2".

#### M1 — Fonte dei dati e come rigenerare la tabella

- **Fonte:** statistiche d'uso pubbliche di Smogon / Pokémon Showdown, file "chaos" JSON
  (`https://www.smogon.com/stats/<mese>/chaos/<formato>-<cutoff>.json`). Catena di formati, il primo con abbastanza
  dati per la specie vince (≥ 150 lotte): `gen9championsvgc2026regmc` (cutoff 1630, poi 0) — è il VGC con le Mega,
  come questo gioco —, `gen9championsvgc2026regmb`, `gen9vgc2024regg`, `gen9nationaldexdoubles`, `gen9doublesou`.
  Champions limita alcuni movepool (es. niente Fake Out a Incineroar): per le specie prese da Champions le mosse si
  mescolano con quelle di Reg G a peso 0,5. Le spread di Champions (punti statistica 0..32) si convertono in EV
  (punti × 8 − 4, massimo 252).
- **Specie:** tutte quelle delle 12 squadre VGC (`AI: ... Vgc Doubles` in `src/data/trainers.party`) più le più
  usate di ogni formato (probabili per il giocatore): 310 specie con il mese 2026-09 (solo Lickitung senza dati:
  resta alla stima da learnset). Le Mega confluiscono nella forma base (frequenza della Mega in `megaP`; abilità e
  statistiche della Mega dal motore).
- **Per specie:** 3 spread (raggruppate per natura e profilo d'investimento 0/parziale/massimo, con frequenza),
  10 mosse, 4 strumenti, 3 abilità (della forma base), 2 Mega, uso nel formato. Circa 86 byte per specie (~27 KB
  di ROM), zero RAM.
- **Rigenerare** (Python 3 su Windows o Linux):
  `python tools/vgc_meta/gen_vgc_meta.py` (ultimo mese) oppure `--month 2026-09`; `--offline` usa solo i file
  già scaricati in `tools/vgc_meta/cache/` (ignorata da git). Scrive `src/data/vgc_meta.h` ordinata per numero di
  specie (ricerca binaria; il test `ai_vgc_scout.c` controlla l'ordine). Se un nuovo mese non ha più i formati
  Champions, aggiornare `FORMAT_CHAIN` nello script.
- **Belief in lotta** (`src/battle_ai_scout.c`, 14 byte × 6 in EWRAM, più 10 byte × 6 in `SeriesMonMemory` per il
  Bo3): ordine di turno → intervallo della Velocità e Choice Scarf; danno fatto/subito ricalcolato con le statistiche
  massime possibili e senza strumento → Choice Band/Specs, Assault Vest, Eviolite; abilità d'ingresso non comparse →
  escluse. Le API per il pianificatore sono in `include/battle_ai_scout.h` (sezione M1).

### M2 — Simulatore corretto e cache danni

1. **Cache danni** in `Begin`: `AI_CalcDamage` reale per i 4 in campo × 4 mosse × bersagli, usando per il giocatore
   il set creduto; per la panchina, solo le mosse principali. Nel simulatore il danno si **scala** (stadi, Helping
   Hand, meteo, terreno, schermi, Intimidate) invece di ricalcolarlo con `EstimateDamage`.
2. **Stadi separati** (Atk/SpA/Def/SpD/Spe/precisione) al posto del `boost` unico.
3. **Correzioni:** Protect consecutivo (probabilità 1/3), Fake Out solo al primo turno in campo, sonno con contatore,
   scadenza di Tailwind/Trick Room/meteo/terreni/schermi, spread che colpiscono il partner.
4. **Meccaniche mancanti:** Intimidate e abilità d'ingresso, Mega Evoluzione (statistiche, tipo, abilità, velocità
   nello stesso turno), terreni, schermi, Sostituto, scottatura/paralisi, precisione come valore atteso, Unburden,
   cambi d'abilità (Simple Beam, Skill Swap, Gastro Acid, Worry Seed, Entrainment), attivazioni da colpo (Justified,
   Stamina, Steam Engine, Weakness Policy, Beat Up), Baton Pass (trasferisce stadi e Sostituto), Parting Shot, U-turn.
5. **Sostituzioni nel simulatore** dopo un KO (chi entra conta per il turno dopo).

*Accettazione:* test unitari per meccanica (Simple Beam su Toedscruel fa partire Spore prima; Beat Up porta Gallade
a +4 nello stesso turno; Close Combat con White Herb attiva Unburden; Protect al secondo turno di fila valutato al 33%).

### M3 — Valutazione della posizione

Sostituisce `Evaluate` (`battle_ai_vgc_plan.c:594-650`):
- **Materiale con panchina:** Pokémon utilizzabili per lato, pesati per ruolo (dal grafo della squadra, M5).
- **HP pesati per ruolo:** un supporto che ha già fatto il suo lavoro vale meno della win condition.
- **Velocità del turno dopo:** chi muove prima e chi minaccia KO, tenendo conto dei turni rimasti di Tailwind/TR.
- **Minacce credute:** gli avversari senza mosse viste minacciano con il set creduto (non sono innocui).
- **Risorse:** Fake Out ancora disponibile, Focus Sash intatte, Mega non usata, Intimidate ciclabile.
- **Potenziale trasferibile:** stadi e Sostituto su un Baton Passer valgono quanto il loro effetto sul ricevente
  vivo in panchina (moltiplicato per la probabilità di passarli).
- **Stato:** sonno/paralisi/scottatura pesati sul ruolo del Pokémon (scottatura su un fisico pesa di più).

I pesi sono interi in `include/config/custom.h`, tarati con la suite di test, non a occhio.

### M4 — Ricerca congiunta (il cambio più importante)

1. **Un solo `Begin` per lato e per turno.** Lo slot che pensa per primo calcola la coppia; il secondo legge la
   decisione in cache (mossa, bersaglio, cambio, Mega).
2. **Candidate per coppie:** top-K per slot (K≈5) + corsia protetta (Protect, Fake Out, Follow Me/Rage Powder, Helping
   Hand, Tailwind, Trick Room, mosse sul partner registrate nel grafo, Baton Pass, cambio sul ricevente/pivot). Prodotto
   L × R potato a P≈12 coppie con un pre-punteggio veloce.
3. **Filtro coppie illegali:** due cambi sullo stesso Pokémon, due Mega, Helping Hand con partner che fa Protect o
   cambia, due Trick Room, meteo/terreni opposti.
4. **Risposte del giocatore su una scala unica (probabilità 0-100):** per ogni avversario, corsie fisse — attacco
   migliore su A, attacco migliore su B, mossa di supporto creduta, Protect (probabilità da `AI_VgcProtectChance` +
   abitudini osservate), cambio (verso il Pokémon in panchina che regge meglio, non il primo). Coppie del giocatore
   potate a M≈6-9.
5. **Il pianificatore decide sempre** (via l'`OVERRIDE_MARGIN`). Focus fire, overkill e "chiudere la kill" emergono
   dalla simulazione; resta un controllo esplicito di **urgenza della kill** (bersaglio a pochi HP con priorità, Spore,
   Trick Room, Regenerator → non si tenta la predict).
6. **α dinamico:** peso del caso peggiore alto quando si è in vantaggio, basso quando si è in svantaggio.
7. **Budget:** P × M × profondità entro N frame misurati in M0; la profondità si riduce prima della larghezza.

*Accettazione:* scenari 1-4 della sezione 4 (focus fire, niente overkill, Tailwind + attacco nello stesso turno,
Helping Hand + Earthquake solo contro bersagli giusti).

### M5 — Conoscenza della squadra generata in automatico

Script `tools/vgc_team_graph.py` eseguito dal Makefile: legge `src/data/trainers.party` (mosse, abilità, strumenti,
statistiche, Mega) e genera `src/data/vgc_team_graph.h` (ROM) con, per ogni allenatore VGC:
- ruoli (fornisce Fake Out / redirect / Prankster / Intimidate / Tailwind / Trick Room / meteo / terreno / rimozione
  abilità; ha bisogno di copertura al turno 1; sweeper fisico/speciale; Baton Passer; abilità penalizzante);
- coppie di lead con punteggio di sinergia; vincoli di panchina (Baton Passer → ricevente obbligatorio);
- ricevente designato di Baton Pass / pivot; Mega designata; archetipi speciali (Perish Trap, Commander, Imprison).

Le regole si scrivono **sulle proprietà** (effetti, abilità, statistiche), mai sui nomi. `vgc_gameplans.h` resta solo
come **override facoltativo** per correggere a mano un singolo caso. Poi si cancellano i blocchi per allenatore e i
controlli `species ==` (`battle_ai_vgc.c:950-1158`, `1604-1633`, `battle_ai_switch.c:1428-1437`).

*Accettazione:* sulla squadra di Savarotto il grafo generato produce gli stessi lead e vincoli oggi scritti a mano
(Espathra → Chandelure obbligatorio, Whimsicott + Gallade); i test di quella squadra passano **dopo** aver cancellato
il blocco `TRAINER_SAVAROTTO`.

### M6 — Cambi, sostituzioni e Mega dentro il pianificatore

- Ogni ingresso passa dal valutatore: cambio volontario, Baton Pass/Parting Shot/U-turn (entra il ricevente o pivot
  del grafo), sostituzione dopo un KO, doppio KO (sceglie la **coppia** migliore dalla panchina, come un mini team
  preview).
- Freno ai cambi ripetuti (penalità a un nuovo cambio volontario dello stesso slot nel turno successivo).
- Correzione del danno da Stealth Rock calcolato su chi entra.
- Mega come candidata (con/senza) nel pianificatore: di default sì; si rimanda solo se la simulazione mostra un
  vantaggio (immunità della forma base, ordine sotto Trick Room, Mega riservata a un altro Pokémon). Rimossa la
  decisione anticipata in `ReconsiderGimmick`.

### M7 — Team preview sul grafo

`TeamPreview_AiSelectEnemyTeam` usa il grafo (M5) al posto delle regole per allenatore (`team_preview.c:1519-1643`,
`1799-1953`) e il belief (M1) al posto delle mosse vere del giocatore. Restano: ricerca su tutte le quartine,
predizione dei lead, memoria Bo3, scelta casuale pesata tra i 3 lead migliori.

### M8 — Pulizia

Le regole di `battle_ai_vgc.c` diventano un filtro di candidate (cosa ha senso considerare), senza punteggi finali
magici. Si eliminano le regole che duplicano ciò che la simulazione già vede. Ogni regola rimasta ha un test.

---

## 3. Vincoli hardware (da misurare in M0, non da stimare)

| Risorsa | Oggi | Regola |
|---|---|---|
| IWRAM | 87% (28.464 / 32.768 B) | dopo M0 ≤ 60%; lo stack usa lo spazio libero, quindi niente array grandi in stack |
| EWRAM | 90,7% | stato del pianificatore, belief, cache danni qui (stima ≤ 3 KB) |
| ROM | 80,4% (≈6,3 MB liberi) | tabelle meta, grafo delle squadre, tabelle precalcolate |
| CPU | `CUSTOM_VGC_PLAN_SIMS_PER_FRAME` = 24 | budget per frame misurato in scanline; obiettivo ≤ 2 secondi totali di "pensiero" per turno |

Non servono reti neurali sul GBA: la conoscenza "appresa" (meta da Showdown, grafo delle squadre) si precalcola su
PC e si mette in ROM come tabelle.

---

## 4. Suite di test iniziale (posizioni → risposta attesa)

1. Bersaglio a 100% che serve a entrambi → entrambi lo attaccano (focus fire).
2. Bersaglio al 20%, il più veloce lo manda KO → l'altro attacca il secondo avversario (niente overkill).
3. Setter Tailwind + attaccante più lento ma letale → Tailwind + attacco nello stesso turno, non Protect.
4. Earthquake con partner Levitate contro avversari resistenti → Helping Hand + mossa singola; contro avversari
   deboli → Helping Hand + Earthquake.
5. Avversario a pochi HP con Sucker Punch/Grassy Glide → si chiude la kill (urgenza).
6. Avversario a pochi HP che ha fatto Protect in situazioni simili 3 volte → si colpisce l'altro slot.
7. Fake Out disponibile + partner con setup → Fake Out sulla minaccia + setup.
8. Turno 2 dopo Fake Out → niente Fake Out.
9. Protect usato il turno prima → Protect non è la scelta se c'è altro di sensato.
10. Espathra a +1/+1/+2 più veloce di tutti, Chandelure vivo → Baton Pass e **entra Chandelure**.
11. Espathra senza ricevente vivo → niente Calm Mind "inutile".
12. Whimsicott + Gallade in lead contro avversari che Gallade a +4 manda KO → Beat Up su Gallade + attacco.
13. Simple Beam su Toedscruel contro avversari più lenti senza immunità alle polveri → sì; contro Gholdengo → no.
14. Sneasler White Herb + Unburden contro Intimidate → valutata la velocità raddoppiata.
15. Doppio KO → entra la coppia migliore dalla panchina.
16. Tailwind all'ultimo turno → l'avversario va in stallo con Protect; l'IA non spreca setup.
17. Trick Room avversario con 1 turno rimasto → Protect sullo slot minacciato.
18. Mega Chandelure con boost → Mega + attacco subito.
19. Chandelure contro Fuoco in arrivo → resta base un turno (Flash Fire), Mega dopo.
20. Gara 2 di un Bo3 → ricorda la mossa fuori meta e la Choice Scarf viste in gara 1.

Ogni errore osservato in partita si aggiunge qui come test.

---

## 5. Ordine di lavoro consigliato

M0 → M1 → M2 → M3 → M4 → M6 → M7 → M8, con **M5 in parallelo** da M1 in poi (è indipendente: script + tabella ROM).
La prima release giocabile utile arriva dopo M4: da lì l'IA decide i due slot insieme su una simulazione corretta.

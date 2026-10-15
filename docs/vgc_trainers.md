# Allenatori VGC e IA per i doppi

Riferimento per gli 11 allenatori nuovi, i loro dialoghi (segnaposto, da riscrivere) e il
funzionamento della nuova IA. Tutto quello che c'è qui si modifica in tre file:

| Cosa | File |
|---|---|
| Squadre, classe, sprite, flag IA | `src/data/trainers.party` (in fondo al file) |
| Script e frasi degli allenatori | `data/scripts/vgc_trainers.inc` |
| Costanti `TRAINER_*` | `include/constants/opponents.h` |
| IA mosse (doppio VGC) | `src/battle_ai_vgc.c` |
| IA cambi (doppio VGC) | `src/battle_ai_switch.c`, funzione `VgcShouldSwitch` |
| Scelta dei 4 + lead, memoria Bo3 | `src/team_preview.c` |

> Attenzione: per far entrare gli allenatori è stato alzato `MAX_TRAINERS_COUNT_EMERALD` da 864 a
> 880. Questo sposta tutti i flag di sistema, quindi **i salvataggi fatti prima di questa build
> non sono compatibili**: nuova partita.

## 1. Allenatori

Per metterli in mappa: nell'NPC (`map.json`) mettere come `script` il nome indicato. Lo sprite
dell'NPC in mappa lo scegli tu (`graphics_id`); quello in lotta è la colonna "Sprite".
Tutti chiedono prima se vuoi lottare, poi il formato (Bo1 / Bo3), poi parte la lotta in doppio
con team preview. Sono rilottabili all'infinito (come Enzo).

| Allenatore | Costante | Script da mettere nell'NPC | Classe / Sprite in lotta | Piano della squadra |
|---|---|---|---|---|
| SIMONE | `TRAINER_SIMONE` | `VgcTrainer_EventScript_Simone` | Cooltrainer / Cooltrainer M | Forza bruta: Machamp Guts, Arcanine Intimidate, Mega Aggron, Rotom-Mow scarf; Toedscruel con Trick Room e Rage Powder |
| ELIA | `TRAINER_ELIA` | `VgcTrainer_EventScript_Elia` | Ansioso / sprite custom | Trick Room: Mimikyu e Mega Banette setter, Goodra-Hisui, Lickitung Ferrodifesa + Schiacciacorpo, Vaporeon e Miltank supporto |
| PATATINA ("La patatina") | `TRAINER_PATATINA` | `VgcTrainer_EventScript_Patatina` | Picnicker / Picnicker | Psichico: Indeedee-F (Psychic Surge, Follow Me, Trick Room) + Armarouge Expanding Force; Ceruledge Swords Dance; Mega Altaria Tailwind; Rotom-Wash |
| TONY | `TRAINER_TONY` | `VgcTrainer_EventScript_Tony` | Black Belt / Black Belt | Sole: Charizard Y, Roaring Moon e Slither Wing (Protosynthesis), Sableye Prankster con Sunny Day e Fake Out, Mega Gyarados, Brute Bonnet |
| MARIO LETT | `TRAINER_MARIO_LETT` | `VgcTrainer_EventScript_MarioLett` | Expert / Expert M | Dondozo + Tatsugiri (Commander), Sinistcha Trick Room, Chatot Tailwind, Weezing-Galar, Slaking |
| TATORE | `TRAINER_TATORE` | `VgcTrainer_EventScript_Tatore` | Nel Chill / sprite custom | Misto: Mega Sceptile, Lapras, Dusclops e Scream Tail Trick Room, Kecleon Fake Out, Armaldo Swords Dance |
| MARIO | `TRAINER_MARIO` | `VgcTrainer_EventScript_Mario` | Fisherman / Fisherman | Trick Room & Rain: Hariyama, Iron Hands, Oranguru Trick Room; Meganium schermi, Politoed e Golduck Nuotovelox |
| ENZO (aggiornato) | `TRAINER_ENZO` | già in mappa (`GambEsterno_EventScript_Enzo`) | PKMN Trainer / sprite custom | Smeargle Gravità/Spora/Follow Me, Annihilape scarf, Rillaboom, Mega Gengar, Mega Aerodactyl Tailwind, Grimmsnarl schermi |
| GEROZZ | `TRAINER_GEROZZ` | `VgcTrainer_EventScript_Gerozz` | Rich Boy / Rich Boy | Sabbia: Mega Tyranitar, Dugtrio-Alola Sand Force, Gholdengo Nasty Plot, Great Tusk Bulk Up, Porygon2 Trick Room, Rotom-Heat |
| CARLETTO | `TRAINER_CARLETTO` | `VgcTrainer_EventScript_Carletto` | Sailor / Sailor | Pioggia: Pelipper, Ludicolo Fake Out, Kingambit, Snorlax Belly Drum, Amoonguss, Mega Malamar Trick Room |
| GIGI PINTO | `TRAINER_GIGI_PINTO` | `VgcTrainer_EventScript_GigiPinto` | Gentleman / Gentleman | Sole: Torkoal, Venusaur e Lilligant Chlorophyll, Garganacl, Hatterene Trick Room, Mega Blastoise |

Note sulle squadre:
- Tera Type ignorato (il Tera è spento). Nickname, shiny, EV, nature e IV sono quelli del file.
- "La patatina" è diventata `PATATINA` (max 10 caratteri per il nome).

## 2. Frasi (segnaposto)

Si cambiano in `data/scripts/vgc_trainers.inc`. Ogni allenatore ha sei testi:
`AskBattle` (domanda sì/no), `Intro` (prima della lotta), `Victory` (se vince lui),
`Defeat` (se perde), `PostBattle` (dopo la lotta, in ogni caso), `Decline` (se rifiuti).
Testi comuni: `VgcTrainer_Text_AskFormat`, `VgcTrainer_Text_FormatBo1/Bo3`, `VgcTrainer_Text_NeedTwoMons`.

| Allenatore | AskBattle | Intro | Victory (lui vince) | Defeat (lui perde) | PostBattle | Decline |
|---|---|---|---|---|---|---|
| SIMONE | Sono SIMONE. Il mio MACHAMP non ha paura di nessuno. Lottiamo? | Forza, vediamo cosa sai fare! | Troppa forza bruta per te. | Mi hai preso in contropiede... | Bella lotta. Torna quando vuoi la rivincita. | Quando sei pronto, sai dove sono. |
| ELIA | Ciao, sono ELIA. Il tempo scorre al contrario, qui. Lottiamo? | Distorsione... e si parte! | Lento ma inesorabile. | Troppo veloce per la mia Distorsione. | ELIA il LICKITUNG ti saluta. Ripassa pure. | Con calma, c'è tempo. |
| PATATINA | Mi chiamano LA PATATINA. Facciamo una lotta? | Il mio CERULEDGE brucia tutto! | Croccante fuori, letale dentro. | Uff... mi hai sbucciata. | Alla prossima! | Peccato. Magari più tardi. |
| TONY | TONY. Sotto il sole il mio team è imbattibile. Ci provi? | Che splenda il sole! | Bruciato. | Si è annuvolato... | Torna quando c'è il sole. | Come vuoi. |
| MARIO LETT | Sono MARIO LETT. Il mio DONDOZO ha fame. Lottiamo? | GIONTA, a tavola! | Mangiato in un boccone. | Indigesto... | Il tè del mio SINISTCHA è ancora caldo, se vuoi riprovare. | Va bene, un'altra volta. |
| TATORE | TATORE! Ti va una lotta come si deve? | Avanti, KECLEON! | Visto? Come si deve. | Mannaggia... | Buona lotta. Alla prossima. | Eh, peccato. |
| MARIO | Sono MARIO. I miei Pokémon sono un po' improvvisati. Lottiamo? | Andiamo! | L'improvvisazione paga! | Lo sapevo, dovevo allenarli. | La prossima volta sarò pronto. | Ok, ok. |
| GEROZZ | GEROZZ. Tempesta di sabbia in arrivo. Sei pronto? | TRECASE, scava! | Sepolto nella sabbia. | La sabbia non è bastata. | Ripassa, la sabbia è sempre qui. | Fifone. |
| CARLETTO | CARLETTO, marinaio. Senti odore di pioggia? Lottiamo? | IL BOSS decide tutto! | Affondato. | IL BOSS non sarà contento... | Torna quando piove. | Mare calmo, allora. |
| GIGI PINTO | GIGI PINTO. Tutto shiny, tutto al sole. Una lotta? | TORKOAL, accendi il sole! | Luccicante e vincente. | Si sono solo abbronzati. | TREMITI ti aspetta per la rivincita. | Quando vuoi. |

Le frasi di Enzo restano in `data/maps/GambEsterno/scripts.inc`.

## 3. Come funziona l'IA adesso

### 3.1 I flag usati (riga `AI:` di ogni allenatore)

Tutti gli allenatori VGC (Enzo compreso) hanno:

```
AI: Basic Trainer / Smart Switching / Smart Mon Choices / Pp Stall Prevention / Randomize Switchin / Know Opponent Party / Assume Stab / Assume Status Moves / Prediction / Hp Aware / Try To 2hko / Weigh Ability Prediction / Double Battle / Prefer Highest Damage Move / Vgc Doubles
```

**Niente `Omniscient`**: l'IA non legge i tuoi set. Sa le specie dei tuoi 4 (team preview, lecito),
le mosse / abilità / strumenti che ha **visto**, e nel Bo3 ciò che ha visto nelle gare precedenti.
Prima di vederle assume mosse STAB e le mosse di stato tipiche (`Assume Stab`, `Assume Status Moves`).

Flag già presenti nel gioco (pokeemerald-expansion), e cosa fanno qui:

| Flag | Cosa fa |
|---|---|
| `Basic Trainer` | = Check Bad Move + Try To Faint + Check Viability |
| `Smart Switching / Smart Mon Choices / Randomize Switchin` | scelta del sostituto intelligente (e non sempre lo stesso) |
| `Pp Stall Prevention` | non si fa stallare i PP |
| `Know Opponent Party` | conosce le specie della tua squadra (viste in team preview) |
| `Assume Stab / Assume Status Moves` | finché non le vede, assume mosse STAB e mosse di stato tipiche |
| `Prediction` | = Predict Move + Predict Switch + Predict Incoming Mon: stima la mossa che useresti (la stessa che userebbe lei al tuo posto) e se stai per cambiare |
| `Hp Aware` | pesa le mosse in base ai PS propri e del bersaglio (niente setup a PS bassi, niente stato su chi sta per morire) |
| `Try To 2hko` | bonus alle mosse che fanno OHKO o 2HKO |
| `Weigh Ability Prediction` | se non ha visto l'abilità, ragiona sull'abilità più probabile |
| `Double Battle` | logica standard dei doppi: non colpire il partner, attivare abilità del partner, Aiuto, Segui Me, Trick Room doppio all'ultimo turno... |
| `Prefer Highest Damage Move` | piccolo bonus alla mossa che fa più danno |
| **`Vgc Doubles`** (nuovo) | il modulo qui sotto |

Tolti rispetto a prima: `Smart Trainer` (conteneva `Omniscient`) e `Powerful Status`, che dava
+10 fisso a Trick Room / Tailwind / schermi anche quando non servivano (era uno dei motivi del
"sempre la stessa cosa").

### 3.1b Lettura del giocatore ("predict") senza imbrogli

L'IA non vede le tue scelte. Stima **Protect** e **cambio** con:
- ciò che è lecito sapere: Protect lo considera solo se te l'ha visto usare; cambio solo se hai
  Pokémon in panchina;
- la situazione: sei "sotto KO" (uno dei suoi può mandarti KO) o "perdi il matchup" (lei ti manda
  KO, tu non mandi KO nessuno dei suoi);
- il tuo comportamento nella serie, per Pokémon: quante volte hai protetto quando eri sotto KO,
  quante volte hai cambiato quando perdevi il matchup. Memoria mantenuta tra le gare del Bo3
  (`AI_VgcNotePlayerMove`, `AI_VgcNotePlayerSwitch` in `src/battle_ai_vgc.c`).

`P(Protect)`: base 20, +30 se sotto KO, −35 se hai protetto il turno scorso, +10 se è il primo
turno, hai Trick Room e lei ha Fake Out; poi media con la tua statistica reale (da 2 turni
osservati in su). `P(cambio)`: base 8, +30 se perdi il matchup, media con la tua statistica.

Se la lettura è forte (≥ 50%) e "ci crede" (tiro casuale una volta a turno, 70%: così non è
sfruttabile):
- colpire chi probabilmente si protegge: −3 (−1 per mosse ad area); Fake Out su di lui: −3;
- ha protetto il turno scorso ed è di nuovo sotto KO: +1 a insistere;
- il colpo da KO su chi probabilmente cambia: −2; Knock Off / mosse ad area: +1;
  Encore / Taunt / Disable su di lui: −3;
- turno "regalato" (lei si aspetta Protect o cambio): setup e controllo velocità +2.

Vantaggio numerico (Pokémon utilizzabili): in vantaggio di 2+ evita mosse imprecise (< 85%)
e protegge un po' di più; in svantaggio di 2+ cerca i KO.

### 3.2 Il modulo nuovo: `AI_FLAG_VGC_DOUBLES` (`src/battle_ai_vgc.c`)

Si somma ai punteggi standard: ogni mossa parte da 100, le funzioni dei flag aggiungono o tolgono
punti, vince la mossa (e il bersaglio) con più punti. Il modulo VGC corregge sei situazioni.

**1. Apertura**
- *Fake Out* al primo turno: +4 se il bersaglio ha una mossa di setup o di controllo velocità
  (Trick Room, Tailwind, meteo, schermi, Danza di Spada, Segui Me...), +3 se il bersaglio può
  mandare KO noi o il partner, +2 se il partner sta facendo setup in questo turno, +1 se rompe un
  Focus Sash o il Disguise. Il bersaglio "giusto" vince perché il punteggio è per bersaglio.
- *Trick Room*: conta quanti Pokémon vivi della squadra (campo **e panchina**) sono più lenti di
  entrambi gli avversari in campo. Con almeno 2: +5, e altri +2 se siamo sotto minaccia e il
  partner non risolve da solo. Se nessuno è più lento: −8 (aiuterebbe solo l'avversario).
  Con Trick Room attivo e nostro: −10 a riusarlo; se è il loro: +4 a toglierlo.
- *Tailwind*: +6 se almeno 2 della squadra supererebbero l'avversario più veloce solo con Tailwind,
  +4 se uno; −4 se siamo già più veloci; −8 / −10 se c'è già Trick Room / Tailwind.
- Giornodisole, Pioggia ecc.: +3 se in squadra (anche in panchina) c'è chi li sfrutta
  (Chlorophyll, Protosynthesis, Swift Swim...). Schermi con Prankster al primo turno: +2.

**2. Setup sicuro** (Danza di Spada, Ferrodifesa, Nasty Plot, Dragon Dance, Bulk Up, Belly Drum...)
- Se abbiamo un KO disponibile: −4 (si prende il KO).
- Turno sicuro (nessun avversario può mandarci KO, oppure il partner usa Segui Me / Rage Powder):
  +3, altri +2 se nessuno può nemmeno fare 2HKO, +2 a Belly Drum sopra il 60% PS.
- Siamo sotto minaccia e più lenti: −5 (−8 per Belly Drum). Non più lenti ma minacciati: −2.
- Ferrodifesa con Schiacciacorpo in set: +2 (è un piano). Già potenziati: −3 (ora si attacca).
- Primo turno con un avversario che ha Fake Out (per quanto ne sappiamo): −3.

**3. Protect "safe"**
- Un avversario può mandarci KO: +2; se il partner lo manda KO **prima** che si muova: +4
  (Protect + partner che pulisce); se siamo più lenti e non abbiamo il KO: +2.
- Nessuno ci manda KO da solo ma i due insieme sì: +3 (rischio doppio bersaglio).
- Nessuno ci fa nemmeno 3HKO: −3 (turno buttato).
- Trick Room avversario attivo: +3 (lo si fa scorrere). Nostro Tailwind / Trick Room attivo e
  nessuna minaccia: −4 (i turni sono preziosi). Focus Sash al primo turno a PS pieni: −2.
- Resta la logica standard (niente Protect due volte di fila, Feint, ecc.).

**4. Fuoco concentrato e scelta del bersaglio**
- Il partner ha già scelto e la sua mossa + la nostra mandano KO il bersaglio: +4.
- Il partner lo manda KO da solo e prima: −3 (si colpisce l'altro), solo −1 se il bersaglio
  conosce Protect, 0 se la nostra è ad area.
- Il bersaglio può mandare KO noi o il partner: +1. Ha una mossa di setup: +1.
- Mossa ad area che fa almeno il 35% a entrambi: +2.

**5. Supporto**
- Segui Me / Rage Powder: +4 se il partner fa setup, +2 se il partner è la minaccia che vogliono
  togliere; −3 sotto il 30% PS.
- Aiuto: +3 se trasforma la mossa scelta dal partner da 2HKO in OHKO.
- Ventogelato / Elettrotela / Bulldozer: +2 come controllo di velocità "morbido" quando basterebbe
  poco per superare l'avversario.

**6. Cambi** (`VgcShouldSwitch` in `src/battle_ai_switch.c`, gira prima della logica standard)
- Mai cambiare senza un sostituto sensato; mai durante il nostro Tailwind / Trick Room a meno che
  restare significhi morire; mai se abbiamo in mano un setup o supporto importante.
- Cambio (75%) se il Pokémon è un peso morto: non fa nemmeno 4HKO a nessuno dei due avversari e
  non ha Segui Me / Aiuto.
- Cambio (60%) se entrambi gli avversari ci mandano KO prima che ci muoviamo, non abbiamo un KO
  e siamo sopra il 50% PS.
- Cambio (35%) "a risparmio" se uno più veloce ci manda KO, non abbiamo né KO né mosse
  superefficaci e siamo sopra il 70%.

### 3.3 Scelta dei 4 e dei lead (team preview, `src/team_preview.c`)

Già prima: tabella di matchup 1v1 tra ogni suo Pokémon e ogni tuo, peso "minaccia" di ogni tuo
Pokémon, scelta del gruppo di 4 che ha una risposta a tutti, lead scelti per matchup + ruolo
(Fake Out, Intimidate, Tailwind/Trick Room, redirect, meteo) + sinergia di coppia
(Trick Room con partner lento, meteo con chi lo sfrutta, Fake Out con setter...), un pizzico di
casualità per non essere leggibile.

### 3.3b Piano di squadra per allenatore (`src/data/vgc_gameplans.h`)

Ogni allenatore ha un piano scritto: modalità principale e di riserva, setter, win condition,
chi sta dietro, tre coppie di lead preferite (con la modalità a cui appartengono). Il piano è
un **bonus** sopra il matchup, non un obbligo: contro una squadra che lo punisce l'IA cambia.

| Allenatore | Modalità A / B | Setter | Win condition | Lead preferite (in ordine) |
|---|---|---|---|---|
| SIMONE | Bilanciato / Trick Room | Toedscruel | Machamp, Aggron | Arcanine+Machamp; Toedscruel+Machamp (TR); Arcanine+Porygon-Z |
| ELIA | Trick Room / Bilanciato | Mimikyu, Banette | Goodra-H, Lickitung | Mimikyu+Goodra-H; Banette+Vaporeon; Mimikyu+Miltank |
| PATATINA | Campo Psichico / Tailwind | Indeedee-F, Altaria | Armarouge, Ceruledge | Indeedee+Armarouge; Altaria+Ceruledge (TW); Indeedee+Copperajah (TR) |
| TONY | Sole / Tailwind | Charizard, Sableye | Roaring Moon, Charizard | Charizard+Sableye; Charizard+Roaring Moon; Roaring Moon+Brute Bonnet |
| MARIO LETT | Bilanciato / Tailwind | Chatot, Sinistcha | Dondozo, Tatsugiri | Dondozo+Tatsugiri; Chatot+Dondozo (TW); Sinistcha+Dondozo (TR) |
| TATORE | Trick Room / Bilanciato | Dusclops, Scream Tail | Armaldo, Sceptile | Dusclops+Lapras; Kecleon+Sceptile; Scream Tail+Armaldo |
| MARIO | Bilanciato / Trick Room | Oranguru | Iron Hands, Hariyama | Hariyama+Iron Hands; Oranguru+Iron Hands (TR); Hariyama+Oranguru |
| ENZO | Tailwind / Bilanciato | Aerodactyl, Grimmsnarl | Annihilape, Gengar | Aerodactyl+Rillaboom; Smeargle+Grimmsnarl; Smeargle+Aerodactyl |
| GEROZZ | Sabbia / Trick Room | Tyranitar, Porygon2 | Gholdengo, Great Tusk | Tyranitar+Dugtrio-A; Porygon2+Great Tusk (TR); Tyranitar+Rotom-Heat |
| CARLETTO | Pioggia / Trick Room | Pelipper, Malamar | Kingambit, Snorlax | Pelipper+Ludicolo; Amoonguss+Snorlax; Malamar+Amoonguss (TR) |
| GIGI PINTO | Sole / Trick Room | Torkoal, Hatterene | Venusaur, Torkoal | Torkoal+Venusaur; Torkoal+Lilligant; Hatterene+Torkoal (TR) |

Come viene usato:
- **Team preview**: coppia di lead preferita +120 / +90 / +60 (dimezzato se la sua modalità non
  conviene: Trick Room conviene se almeno metà dei tuoi è più veloce della sua media, Tailwind se
  sei più veloce ma non del doppio); win condition portata +50; setter +35; chi "sta dietro" −40 come lead.
- **In lotta**: la mossa che imposta la modalità del piano +3 (riserva +2), sempre dopo i
  controlli di convenienza; la win condition si protegge (+1) e fa setup (+1) nei turni sicuri;
  se il partner è la win condition: Segui Me +2, Aiuto +1, Fake Out sulla sua minaccia +1.
- **Cambi**: con la modalità attiva (Trick Room / Tailwind / meteo) e la win condition in panchina,
  il Pokémon in campo senza KO lascia il posto (45%) alla win condition. Rientro con Intimidate
  contro attaccanti fisici (50%), mai durante il proprio controllo di velocità.

Risposte tattiche aggiunte: Provocazione sul setter se più veloci (+3), Ampioguardia contro mosse
ad area note (+2), Anticipo contro Fake Out / priorità note (+3 al primo turno), Ultimascommessa
quando il Pokémon è spacciato (+5), sonno su chi ha setup (+1).

### 3.3c Scouting della lead (`src/battle_ai_scout.c`, `src/team_preview.c`)

In team preview l'IA vede solo le **specie** della tua squadra, come un giocatore vero. Da lì stima,
per ogni tuo Pokémon, i ruoli **probabili** (non le mosse vere):

| Fonte | Peso | Esempio |
|---|---|---|
| Tabella di meta (`sScoutMeta`) | 85% | Amoonguss: Polverabbia + Spora; Pelipper: Tailwind + Ampioguardia |
| Abilità possibili della specie | 40-100% | Torkoal: Siccità; Incineroar: Prepotenza |
| Learnset ("può impararla") | 15-60% | chi impara Distorsione o Fake Out |

Ruoli: Distorsione, Ventoincoda, controllo velocità "morbido" (Ventogelato, Elettrotela, Tuononda),
Fake Out, redirect, Provocazione, sonno, Ultimocanto, Ampioguardia, setup, Aiuto; più meteo, campo,
Prepotenza, immunità al sonno, velocità base. Nel Bo3 ciò che ha visto nelle gare prima sostituisce la stima.

Con i profili:
1. **Core e lead probabili del giocatore**: ogni tua coppia riceve un punteggio (ruoli da lead +
   core setter/abuser: Distorsione con lenti, Tailwind con medi, meteo con chi lo sfrutta, Fake Out con
   setter, redirect con setup, Dondozo + Tatsugiri; nel Bo3 chi hai già messo in lead). Le 6 migliori
   diventano le "lead previste", con un peso.
2. **Ogni lead dell'IA contro ogni lead prevista** (`LeadVsLead`): duelli 1 contro 1 più
   - Distorsione nostra: serve se siamo più lenti; persa se il setter prende Fake Out (salvo Spettro,
     Forza Interiore, Campo Psichico o Anticipo del partner) o Provocazione (salvo Erbamentale,
     Indifferenza, Magispecchio);
   - Tailwind nostro contro la loro Distorsione;
   - la loro Distorsione quando noi siamo i veloci: Fake Out / Provocazione / Distorsione nostra la
     fermano, altrimenti è un problema;
   - il loro Tailwind; Fake Out e Provocazione nostri sui loro setter;
   - guerra del meteo (vince il setter più lento; Cloud Nine la annulla), campi (Campo Psichico contro
     Fake Out, Elettrico/Nebbioso contro il sonno), Prepotenza sui nostri fisici, sonno, redirect contro
     chi non ha mosse ad area.
3. **Punteggio della lead** = valutazione di prima + 70% media pesata + 30% caso peggiore.
4. **Scelta mista**: se le prime 3 lead sono vicine (entro 40 punti) l'IA sceglie la prima nel 60%
   dei casi, la seconda nel 28%, la terza nel 12%. Se la prima ha una contro-lead che la smonta (caso
   peggiore molto più brutto della seconda), la seconda diventa favorita (48%).
5. I tuoi pezzi chiave (setter, meteo, Fake Out, redirect) pesano di più anche nella scelta dei 4 e della panchina.

In lotta lo scouting serve solo finché non vede: al turno 1 Fake Out probabile, setter probabili
(per Fake Out / Provocazione mirati) e Protect probabile (in VGC quasi tutti lo portano: 60% finché
le mosse sono ignote). Ogni mossa vista sostituisce la stima.

### 3.3d Megaevoluzione decisa partita per partita

Non c'è più una Mega "prenotata" dal piano. In preview, tra i Pokémon con Megapietra portati, sceglie
quello con: lead (+100), matchup contro la squadra e contro la lead prevista, bisogno di controllo
velocità (+40 a chi ha Tailwind se il giocatore è più veloce), un poco il piano (+20 alla win condition).
In lotta la Mega resta di quello scelto se è in campo; se è caduto è libera; se è in panchina e chi è
in campo rischia il KO, la fa chi è in campo. Esempio Enzo: contro una squadra veloce Mega Aerodactyl
in lead col Tailwind, contro una lenta o con Distorsione Mega Gengar col blocco Gravità + Ipnosi.

### 3.4 Al meglio di tre: cosa impara tra una gara e l'altra

- Mosse, abilità e strumenti **visti** restano noti nella gara dopo (già prima).
- Chi hai portato e chi hai messo in lead pesa di più nella scelta (già prima).
- **Nuovo**: ricorda chi dei tuoi ha mandato KO i suoi Pokémon (`kos`): quei Pokémon diventano
  la priorità da coprire (peso +45 per KO, fino a 2).
- **Nuovo**: ricorda il piano di ogni gara (quali 4 ha portato e quale coppia di lead) e com'è
  finita. Nella gara successiva lo stesso lead che ha **perso** vale −150 (praticamente non lo
  rifà), lo stesso gruppo di 4 che ha perso −70; un lead che ha **vinto** +40, un gruppo che ha
  vinto +30. Risultato: dopo una sconfitta cambia apertura, dopo una vittoria tende a ripetere.

## 4. Cose che puoi regolare a mano

- I numeri dei bonus sono tutti in `src/battle_ai_vgc.c`, uno per riga con commento.
- Le percentuali dei cambi in `VgcShouldSwitch`.
- I pesi del Bo3 (−150 / −70 / +40 / +30 / +45) in `TeamPreview_AiSelectEnemyTeam`.
- Per un allenatore "stupido" basta togliere `Vgc Doubles` (e magari `Prediction`) dalla sua riga `AI:`.
- Per un allenatore che "bara" (conosce i tuoi set): sostituire `Basic Trainer` con `Smart Trainer`
  (contiene `Omniscient`).
- Le probabilità della lettura (base 20 / 8, bonus +30, fiducia 70%) in `VgcProtectChance`,
  `VgcSwitchChance`, `VgcTrustRead` in `src/battle_ai_vgc.c`.

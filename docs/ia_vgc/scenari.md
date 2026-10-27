# IA VGC — Test di scenario con le squadre vere

File: `test/battle/ai/ai_vgc_teams.c` (branch `ia-vgc-scenari`). Set completi dei 12 allenatori da
`src/data/trainers.party` (specie, mosse, abilità, strumenti, natura ed EV a Lv50 tramite `VgcSpread`),
flag IA della riga `AI:` dei trainer. L'avversario (lato player) è sempre un'altra delle 12 squadre.

Stato al 27/10: **11 pass, 25 KNOWN_FAILING** (16 errori dell'IA, 9 da rivedere nel setup). Nei `TURN`
il giocatore ha sempre una `MOVE` esplicita (Protect dove c'è, altrimenti una mossa poco rilevante).
I KNOWN_FAILING marcati "setup" falliscono con "TURN incompleto": una mossa del giocatore non viene eseguita
(il Pokémon va KO o tentenna prima di agire), quindi il test non dice se l'IA ha giocato bene. Correzione:
lasciare il giocatore su Protect/mosse che agiscono di sicuro, o controllare l'IA con `SCORE_*`. Tempo di
esecuzione: oltre 10 minuti per i 36 test (il pianificatore simula ogni turno).

## Errori dell'IA raggruppati

1. **Controllo velocità non scelto al turno 1 contro lead veloci**: Mimikyu (Play Rough), Mega Banette
   (Protect), Altaria (Dragon Pulse), Aerodactyl (Rock Slide), Malamar (Superpower). Passano invece Dusclops,
   Sinistcha, Hatterene.
2. **Cambi al turno 1 della lead**: Toedscruel, Miltank, Smeargle, Porygon2 (setter/supporto che escono
   invece di fare il proprio lavoro).
3. **Fake Out non usato al turno 1**: Hariyama (su Whimsicott), Ludicolo. Passano Incineroar e Sableye.
4. **Supporto sottovalutato rispetto all'attacco**: Grimmsnarl (Spirit Break invece degli schermi), Lilligant
   (Energy Ball invece di After You / Sleep Powder).
5. **Protect senza motivo**: Dugtrio-A (Earthquake libero), Banette.
6. **Punteggi pari / mosse inutili**: Snorlax, Belly Drum e Recycle a 90 (Recycle senza bacca consumata).
7. **Bersaglio delle mosse sul compagno / sull'avversario**: Beat Up di Whimsicott ed Encore di Scream Tail
   risultano con bersaglio `opponentLeft` (da verificare se errore dell'IA o della registrazione del bersaglio).

## Tabella

| Allenatore | Test | Asserzione | Esito |
|---|---|---|---|
| Simone | Rotom-Mow non usa Leaf Storm contro Pelipper + Ludicolo | NOT Leaf Storm | PASS |
| Simone | Toedscruel Trick Room / Rage Powder per Machamp contro Enzo | TR o Rage Powder | KF: al turno 1 cambia Toedscruel |
| Simone | Arcanine Extreme Speed su Roaring Moon a pochi PS | Extreme Speed → Roaring Moon | PASS |
| Elia | Mimikyu Trick Room accanto a Goodra-H contro Tony | Trick Room | KF: Play Rough invece di Trick Room |
| Elia | Mega Banette + Trick Room contro Patatina | Trick Room + Mega | KF: Protect invece di Trick Room |
| Elia | Miltank niente Helping Hand mentre Mimikyu fa TR | NOT Helping Hand | KF: Miltank esce al turno 1 |
| Patatina | Armarouge Expanding Force sul Campo Psichico | Expanding Force | KF (setup): TURN incompleto, una mossa del giocatore non viene eseguita; esito IA da verificare |
| Patatina | Altaria Tailwind per Ceruledge contro Tony | Tailwind | KF: Dragon Pulse invece di Tailwind |
| Patatina | Ceruledge Shadow Sneak su Gengar a pochi PS | Shadow Sneak → Gengar | KF (setup): TURN incompleto, una mossa del giocatore non viene eseguita; esito IA da verificare |
| Tony | Sableye ferma il TR di Porygon2 | Fake Out o Taunt | PASS |
| Tony | Charizard Mega Y + Heat Wave su due Erba | Heat Wave + Mega | KF (setup): TURN incompleto, una mossa del giocatore non viene eseguita; esito IA da verificare |
| Tony | Slither Wing niente Close Combat su Gengar + Rillaboom | NOT Close Combat | KF (setup): TURN incompleto, una mossa del giocatore non viene eseguita; esito IA da verificare |
| Mario Lett | Dondozo (Commander) Earthquake su Arcanine + Aggron | Earthquake | KF (setup): TURN incompleto, una mossa del giocatore non viene eseguita; esito IA da verificare |
| Mario Lett | Sinistcha Trick Room accanto a Dondozo contro Enzo | Trick Room | PASS |
| Mario Lett | Slaking con Gas Reagente attacca (turni 1 e 2) | NOT Protect x2 | KF (setup): TURN incompleto, una mossa del giocatore non viene eseguita; esito IA da verificare (turno 2) |
| Tatore | Kecleon non usa Taunt su Hatterene (Magic Bounce) | NOT Taunt | PASS |
| Tatore | Dusclops Trick Room accanto a Lapras contro Patatina | Trick Room | PASS |
| Tatore | Scream Tail Encore su Espathra dopo Calm Mind | Encore → Espathra (turno 2) | KF: Encore con bersaglio opponentLeft invece di Espathra (da verificare) |
| Mario | Hariyama Fake Out su Whimsicott | Fake Out → Whimsicott | KF: Heavy Slam invece di Fake Out |
| Mario | Oranguru Trick Room accanto a Iron Hands contro Enzo | Trick Room | KF (setup): TURN incompleto, una mossa del giocatore non viene eseguita; esito IA da verificare |
| Mario | Golduck in pioggia Hydro Pump sui Fuoco | Hydro Pump | PASS |
| Enzo | Aerodactyl Tailwind per Rillaboom contro Tony | Tailwind | KF: Rock Slide invece di Tailwind |
| Enzo | Smeargle non usa Spore su due Erba | NOT Spore | KF: Smeargle esce al turno 1 |
| Enzo | Grimmsnarl schermo al turno 1 | Reflect o Light Screen | KF: Spirit Break invece degli schermi |
| Gerozz | Dugtrio-A Earthquake accanto a Rotom-Heat (Levitate) | Earthquake | KF: Protect invece di Earthquake |
| Gerozz | Porygon2 Trick Room accanto a Great Tusk contro Enzo | Trick Room | KF: Porygon2 esce al turno 1 |
| Gerozz | Gholdengo Make It Rain su due Folletto | Make It Rain | PASS |
| Carletto | Amoonguss Rage Powder + Snorlax Belly Drum | Rage Powder + Belly Drum | KF: Belly Drum e Recycle a pari punteggio (90), Recycle inutile non penalizzato |
| Carletto | Ludicolo Fake Out al turno 1 | Fake Out | KF: Scald invece di Fake Out |
| Carletto | Malamar Trick Room accanto a Kingambit contro Tony | Trick Room | KF: Superpower invece di Trick Room |
| Gigi Pinto | Torkoal Eruption a PS pieni nel sole | Eruption | PASS |
| Gigi Pinto | Hatterene Trick Room accanto a Torkoal contro Tony | Trick Room | PASS |
| Gigi Pinto | Lilligant After You su Torkoal o Sleep Powder su Great Tusk | After You o Sleep Powder | KF: Energy Ball invece di After You / Sleep Powder |
| Savarotto | Whimsicott Beat Up su Gallade (Justified) | Beat Up → Gallade | KF: bersaglio registrato opponentLeft invece di Gallade (da verificare) |
| Savarotto | Incineroar Fake Out per coprire Espathra | Fake Out | PASS |
| Savarotto | Mega Chandelure + Heat Wave su due Erba | Heat Wave + Mega | KF (setup): TURN incompleto, una mossa del giocatore non viene eseguita; esito IA da verificare |

Note:
- Nei test l'allenatore non ha un `TRAINER_*`: il piano di `vgc_gameplans.h` non è attivo, le decisioni
  vengono da regole per ruolo, grafo della squadra e pianificatore.
- Per compilare il worktree in WSL serve `.histignore` nella radice (il `.git` del worktree punta a un
  percorso Windows e `check_history.sh` fallisce); il file è già ignorato da git.

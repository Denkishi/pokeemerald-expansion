# IA VGC — Test di scenario con le squadre vere

File: `test/battle/ai/ai_vgc_teams.c` (branch `ia-vgc-scenari`). Set completi dei 12 allenatori da
`src/data/trainers.party` (specie, mosse, abilità, strumenti, natura ed EV a Lv50 tramite `VgcSpread`),
flag IA della riga `AI:` dei trainer. L'avversario (lato player) è sempre un'altra delle 12 squadre.

Stato: **WIP, in pausa**. Primo giro: i 36 test risultano tutti `INVALID` per un problema del test, non
dell'IA: nei `TURN` senza `MOVE` per il giocatore il framework usa Celebrate, ma i Pokémon del giocatore
hanno già 4 mosse ("Too many different moves for player..."). **Prossimo passo**: dare in ogni `TURN` una
`MOVE(playerLeft/playerRight, ...)` esplicita (una mossa vera del set, scelta in modo da non falsare la
posizione, es. Protect dove c'è, altrimenti una mossa neutra), ricompilare, rieseguire e compilare la
colonna "Esito" (pass / KNOWN_FAILING + diagnosi dal log `VGCPLAN`).

| Allenatore | Test | Asserzione | Esito |
|---|---|---|---|
| Simone | Rotom-Mow non usa Leaf Storm contro Pelipper + Ludicolo | NOT Leaf Storm | da eseguire |
| Simone | Toedscruel Trick Room / Rage Powder per Machamp contro Enzo | TR o Rage Powder | da eseguire |
| Simone | Arcanine Extreme Speed su Roaring Moon a pochi PS | Extreme Speed → Roaring Moon | da eseguire |
| Elia | Mimikyu Trick Room accanto a Goodra-H contro Tony | Trick Room | da eseguire |
| Elia | Mega Banette + Trick Room contro Patatina | Trick Room + Mega | da eseguire |
| Elia | Miltank niente Helping Hand mentre Mimikyu fa TR | NOT Helping Hand | da eseguire |
| Patatina | Armarouge Expanding Force sul Campo Psichico | Expanding Force | da eseguire |
| Patatina | Altaria Tailwind per Ceruledge contro Tony | Tailwind | da eseguire |
| Patatina | Ceruledge Shadow Sneak su Gengar a pochi PS | Shadow Sneak → Gengar | da eseguire |
| Tony | Sableye ferma il TR di Porygon2 | Fake Out o Taunt | da eseguire |
| Tony | Charizard Mega Y + Heat Wave su due Erba | Heat Wave + Mega | da eseguire |
| Tony | Slither Wing niente Close Combat su Gengar + Rillaboom | NOT Close Combat | da eseguire |
| Mario Lett | Dondozo (Commander) Earthquake su Arcanine + Aggron | Earthquake | da eseguire |
| Mario Lett | Sinistcha Trick Room accanto a Dondozo contro Enzo | Trick Room | da eseguire |
| Mario Lett | Slaking con Gas Reagente attacca (turni 1 e 2) | NOT Protect x2 | da eseguire |
| Tatore | Kecleon non usa Taunt su Hatterene (Magic Bounce) | NOT Taunt | da eseguire |
| Tatore | Dusclops Trick Room accanto a Lapras contro Patatina | Trick Room | da eseguire |
| Tatore | Scream Tail Encore su Espathra dopo Calm Mind | Encore → Espathra (turno 2) | da eseguire |
| Mario | Hariyama Fake Out su Whimsicott | Fake Out → Whimsicott | da eseguire |
| Mario | Oranguru Trick Room accanto a Iron Hands contro Enzo | Trick Room | da eseguire |
| Mario | Golduck in pioggia Hydro Pump sui Fuoco | Hydro Pump | da eseguire |
| Enzo | Aerodactyl Tailwind per Rillaboom contro Tony | Tailwind | da eseguire |
| Enzo | Smeargle non usa Spore su due Erba | NOT Spore | da eseguire |
| Enzo | Grimmsnarl schermo al turno 1 | Reflect o Light Screen | da eseguire |
| Gerozz | Dugtrio-A Earthquake accanto a Rotom-Heat (Levitate) | Earthquake | da eseguire |
| Gerozz | Porygon2 Trick Room accanto a Great Tusk contro Enzo | Trick Room | da eseguire |
| Gerozz | Gholdengo Make It Rain su due Folletto | Make It Rain | da eseguire |
| Carletto | Amoonguss Rage Powder + Snorlax Belly Drum | Rage Powder + Belly Drum | da eseguire |
| Carletto | Ludicolo Fake Out al turno 1 | Fake Out | da eseguire |
| Carletto | Malamar Trick Room accanto a Kingambit contro Tony | Trick Room | da eseguire |
| Gigi Pinto | Torkoal Eruption a PS pieni nel sole | Eruption | da eseguire |
| Gigi Pinto | Hatterene Trick Room accanto a Torkoal contro Tony | Trick Room | da eseguire |
| Gigi Pinto | Lilligant After You su Torkoal o Sleep Powder su Great Tusk | After You o Sleep Powder | da eseguire |
| Savarotto | Whimsicott Beat Up su Gallade (Justified) | Beat Up → Gallade | da eseguire |
| Savarotto | Incineroar Fake Out per coprire Espathra | Fake Out | da eseguire |
| Savarotto | Mega Chandelure + Heat Wave su due Erba | Heat Wave + Mega | da eseguire |

Note:
- Nei test l'allenatore non ha un `TRAINER_*`: il piano di `vgc_gameplans.h` non è attivo, le decisioni
  vengono da regole per ruolo, grafo della squadra e pianificatore.
- Per compilare il worktree in WSL serve `.histignore` nella radice (il `.git` del worktree punta a un
  percorso Windows e `check_history.sh` fallisce); il file è già ignorato da git.

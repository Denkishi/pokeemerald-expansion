# Self-play VGC: riepilogo

Aggiornato: 2026-10-27 08:08:47. Partite lette: 147 (completate 143, con problemi 4).

Ogni partita: lato G = allenatore della riga, lato A = allenatore della colonna; entrambi IA VGC con team preview propria e informazione chiusa (ognuno vede solo cio che ha osservato dell'altro). Dettagli turno per turno in [partite/](partite/).

Turni per partita: media 6.6, min 2, max 17.
Fitness (gancio M9, misplay certi per partita x -0.5): -0.818

## Misplay per categoria

| Categoria | Misplay certi | Casi dubbi (revisione) |
|---|---:|---:|
| Mossa su un immune noto | 4 | 0 |
| Bersaglio gia KO senza redirect utile | 29 | 0 |
| KO garantito non preso | 70 | 350 |
| Protect consecutivo | 8 | 0 |
| Fake Out dopo il primo turno | 2 | 0 |
| Cambio su chi va KO all'entrata | 37 | 0 |
| Cambi a ripetizione | 13 | 0 |
| Turni di Tailwind/Trick Room sprecati | 18 | 29 |
| Setup con KO in arrivo | 20 | 2 |
| Win condition persa evitabilmente | 31 | 41 |
| Team preview: abilita penalizzante senza chi la toglie | 2 | 0 |
| Team preview: lead che si danneggia da sola | 0 | 26 |
| **Totale** | **234** | **448** |

## Risultati (riga = lato G, colonna = lato A)

V = vince G, S = vince A, P = pareggio, L = limite turni, ! = blocco/crash, . = non giocata

| G \ A | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 ENZO | [V](partite/00_00_s1.md) | [V](partite/00_01_s1.md)[V](partite/00_01_s2.md) | [V](partite/00_02_s3.md) | [V](partite/00_03_s4.md) | [V](partite/00_04_s5.md) | [V](partite/00_05_s6.md) | [V](partite/00_06_s7.md) | [S](partite/00_07_s8.md) | [V](partite/00_08_s9.md) | [V](partite/00_09_s10.md) | [V](partite/00_10_s11.md) | [V](partite/00_11_s12.md) |
| 1 SIMONE | [V](partite/01_00_s13.md) | [V](partite/01_01_s14.md) | [V](partite/01_02_s15.md) | [S](partite/01_03_s16.md) | [S](partite/01_04_s17.md) | [V](partite/01_05_s18.md) | [V](partite/01_06_s19.md) | [S](partite/01_07_s20.md) | [!](partite/01_08_s21.md) | [V](partite/01_09_s22.md) | [S](partite/01_10_s23.md) | [V](partite/01_11_s24.md) |
| 2 ELIA | [S](partite/02_00_s25.md) | [S](partite/02_01_s26.md) | [S](partite/02_02_s27.md) | [S](partite/02_03_s28.md) | [S](partite/02_04_s29.md) | [S](partite/02_05_s30.md) | [S](partite/02_06_s31.md) | [S](partite/02_07_s32.md) | [V](partite/02_08_s33.md) | [S](partite/02_09_s34.md) | [S](partite/02_10_s35.md) | [S](partite/02_11_s36.md) |
| 3 RAFEL | [!](partite/03_00_s37.md) | [S](partite/03_01_s38.md) | [V](partite/03_02_s39.md) | [S](partite/03_03_s40.md) | [S](partite/03_04_s41.md) | [V](partite/03_05_s42.md) | [V](partite/03_06_s43.md) | [V](partite/03_07_s44.md) | [S](partite/03_08_s45.md) | [S](partite/03_09_s46.md) | [S](partite/03_10_s47.md) | [S](partite/03_11_s48.md) |
| 4 TONY | [S](partite/04_00_s49.md) | [V](partite/04_01_s50.md) | [V](partite/04_02_s51.md) | [V](partite/04_03_s52.md) | [S](partite/04_04_s53.md) | [S](partite/04_05_s54.md) | [V](partite/04_06_s55.md) | [V](partite/04_07_s56.md) | [V](partite/04_08_s57.md) | [V](partite/04_09_s58.md) | [S](partite/04_10_s1.md)[V](partite/04_10_s59.md) | [S](partite/04_11_s60.md) |
| 5 MARIO LETT | [S](partite/05_00_s61.md) | [S](partite/05_01_s62.md) | [S](partite/05_02_s63.md) | [V](partite/05_03_s64.md) | [S](partite/05_04_s65.md) | [V](partite/05_05_s66.md) | [V](partite/05_06_s67.md) | [S](partite/05_07_s68.md) | [V](partite/05_08_s69.md) | [S](partite/05_09_s70.md) | [V](partite/05_10_s71.md) | [V](partite/05_11_s72.md) |
| 6 TATORE | [S](partite/06_00_s73.md) | [S](partite/06_01_s74.md) | [V](partite/06_02_s75.md) | [S](partite/06_03_s76.md) | [V](partite/06_04_s77.md) | [S](partite/06_05_s78.md) | [S](partite/06_06_s79.md) | [V](partite/06_07_s80.md) | [S](partite/06_08_s81.md) | [S](partite/06_09_s82.md) | [S](partite/06_10_s83.md) | [V](partite/06_11_s84.md) |
| 7 MARIO | [V](partite/07_00_s85.md) | [V](partite/07_01_s86.md) | [V](partite/07_02_s1.md)[V](partite/07_02_s87.md) | [S](partite/07_03_s88.md) | [S](partite/07_04_s89.md) | [V](partite/07_05_s90.md) | [V](partite/07_06_s91.md) | [V](partite/07_07_s92.md) | [V](partite/07_08_s93.md) | [S](partite/07_09_s94.md) | [S](partite/07_10_s95.md) | [S](partite/07_11_s96.md) |
| 8 GEROZZ | [S](partite/08_00_s97.md) | [V](partite/08_01_s98.md) | [S](partite/08_02_s99.md) | [V](partite/08_03_s100.md) | [V](partite/08_04_s101.md) | [V](partite/08_05_s102.md) | [V](partite/08_06_s103.md) | [V](partite/08_07_s104.md) | [S](partite/08_08_s105.md) | [V](partite/08_09_s106.md) | [V](partite/08_10_s107.md) | [S](partite/08_11_s108.md) |
| 9 CARLETTO | [V](partite/09_00_s109.md) | [S](partite/09_01_s110.md) | [S](partite/09_02_s111.md) | [V](partite/09_03_s112.md) | [!](partite/09_04_s113.md) | [V](partite/09_05_s114.md) | [V](partite/09_06_s115.md) | [S](partite/09_07_s116.md) | [S](partite/09_08_s117.md) | [S](partite/09_09_s118.md) | [V](partite/09_10_s119.md) | [S](partite/09_11_s120.md) |
| 10 GIGI PINTO | [V](partite/10_00_s121.md) | [V](partite/10_01_s122.md) | [V](partite/10_02_s123.md) | [V](partite/10_03_s124.md) | [S](partite/10_04_s125.md) | [V](partite/10_05_s126.md) | [V](partite/10_06_s127.md) | [S](partite/10_07_s128.md) | [S](partite/10_08_s129.md) | [S](partite/10_09_s130.md) | [V](partite/10_10_s131.md) | [!](partite/10_11_s132.md) |
| 11 SAVAROTTO | [S](partite/11_00_s133.md) | [V](partite/11_01_s134.md) | [V](partite/11_02_s135.md) | [V](partite/11_03_s136.md) | [V](partite/11_04_s137.md) | [V](partite/11_05_s138.md) | [S](partite/11_06_s139.md) | [V](partite/11_07_s140.md) | [S](partite/11_08_s141.md) | [V](partite/11_09_s142.md) | [S](partite/11_10_s143.md) | [S](partite/11_11_s144.md) |

## Allenatori

| # | Allenatore | Vittorie / partite | Misplay certi (propri) |
|---:|---|---|---:|
| 0 | ENZO | 18 / 24 | 24 |
| 1 | SIMONE | 12 / 24 | 21 |
| 2 | ELIA | 5 / 25 | 12 |
| 3 | RAFEL | 9 / 23 | 30 |
| 4 | TONY | 15 / 24 | 18 |
| 5 | MARIO LETT | 9 / 24 | 13 |
| 6 | TATORE | 7 / 24 | 15 |
| 7 | MARIO | 14 / 25 | 23 |
| 8 | GEROZZ | 14 / 23 | 17 |
| 9 | CARLETTO | 12 / 23 | 20 |
| 10 | GIGI PINTO | 14 / 24 | 11 |
| 11 | SAVAROTTO | 14 / 23 | 30 |

## Esempi di misplay

### KO garantito non preso (70)
- [00_05_s6](partite/00_05_s6.md), turno 3, ENZO: Annihilape Final Gambit avrebbe mandato KO Chatot, scelto Close Combat
- [01_00_s13](partite/01_00_s13.md), turno 5, SIMONE: Arcanine Flare Blitz avrebbe mandato KO Rillaboom, scelto Extreme Speed
- [01_01_s14](partite/01_01_s14.md), turno 4, SIMONE: Arcanine Flare Blitz avrebbe mandato KO Arcanine, scelto Extreme Speed
- [01_05_s18](partite/01_05_s18.md), turno 3, MARIO LETT: Tatsugiri Dragon Pulse avrebbe mandato KO Toedscruel, scelto Icy Wind
- [01_05_s18](partite/01_05_s18.md), turno 5, SIMONE: Aggron Body Press avrebbe mandato KO Tatsugiri, scelto Heavy Slam

### Cambio su chi va KO all'entrata (37)
- [00_00_s1](partite/00_00_s1.md), turno 4, ENZO: Rillaboom entrato con un cambio e KO nello stesso turno
- [00_03_s4](partite/00_03_s4.md), turno 2, ENZO: Annihilape entrato con un cambio e KO nello stesso turno
- [00_07_s8](partite/00_07_s8.md), turno 3, ENZO: Rillaboom entrato con un cambio e KO nello stesso turno
- [00_11_s12](partite/00_11_s12.md), turno 2, SAVAROTTO: Espathra entrato con un cambio e KO nello stesso turno
- [01_02_s15](partite/01_02_s15.md), turno 2, ELIA: Vaporeon entrato con un cambio e KO nello stesso turno

### Win condition persa evitabilmente (31)
- [00_09_s10](partite/00_09_s10.md), turno 5, CARLETTO: Kingambit KO senza Protect/cambio (Protect disponibile): il KO era prevedibile
- [01_00_s13](partite/01_00_s13.md), turno 4, ENZO: Gengar KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [01_03_s16](partite/01_03_s16.md), turno 3, SIMONE: Aggron KO senza Protect/cambio (Protect disponibile): il KO era prevedibile
- [01_03_s16](partite/01_03_s16.md), turno 3, RAFEL: Armarouge KO senza Protect/cambio (Protect disponibile): il KO era prevedibile
- [01_07_s20](partite/01_07_s20.md), turno 6, MARIO: Hariyama KO senza Protect/cambio (Protect disponibile): il KO era prevedibile

### Bersaglio gia KO senza redirect utile (29)
- [00_02_s3](partite/00_02_s3.md), turno 6, ENZO: Annihilape Close Combat su Lickitung che il compagno manda KO prima; nessun redirect utile
- [00_11_s12](partite/00_11_s12.md), turno 5, ENZO: Rillaboom Wood Hammer su Chandelure che il compagno manda KO prima; nessun redirect utile
- [01_02_s15](partite/01_02_s15.md), turno 4, SIMONE: Machamp Knock Off su Mimikyu che il compagno manda KO prima; nessun redirect utile
- [01_05_s18](partite/01_05_s18.md), turno 6, SIMONE: Aggron Body Press su Chatot che il compagno manda KO prima; nessun redirect utile
- [01_06_s19](partite/01_06_s19.md), turno 9, SIMONE: Aggron Body Press su Kecleon che il compagno manda KO prima; nessun redirect utile

### Setup con KO in arrivo (20)
- [00_02_s3](partite/00_02_s3.md), turno 3, ELIA: Lickitung Iron Defense mentre Annihilape lo manda KO (danno minimo stimato)
- [01_03_s16](partite/01_03_s16.md), turno 3, RAFEL: Ceruledge Swords Dance mentre Aggron lo manda KO (danno minimo stimato)
- [01_03_s16](partite/01_03_s16.md), turno 4, RAFEL: Copperajah Iron Defense mentre Machamp lo manda KO (danno minimo stimato)
- [01_04_s17](partite/01_04_s17.md), turno 5, TONY: Gyarados Dragon Dance mentre Aggron lo manda KO (danno minimo stimato)
- [01_04_s17](partite/01_04_s17.md), turno 6, TONY: Gyarados Dragon Dance mentre Aggron lo manda KO (danno minimo stimato)

### Turni di Tailwind/Trick Room sprecati (18)
- [02_06_s31](partite/02_06_s31.md), turno 7, TATORE: Dusclops Trick Room con Trick Room attivo (2 turni): lo chiude
- [03_03_s40](partite/03_03_s40.md), turno 3, RAFEL: Indeedee Trick Room con Trick Room attivo (4 turni): lo chiude
- [03_03_s40](partite/03_03_s40.md), turno 5, RAFEL: Indeedee Trick Room con Trick Room attivo (4 turni): lo chiude
- [03_06_s43](partite/03_06_s43.md), turno 4, TATORE: Dusclops Trick Room con Trick Room attivo (2 turni): lo chiude
- [03_07_s44](partite/03_07_s44.md), turno 3, RAFEL: Indeedee Trick Room con Trick Room attivo (4 turni): lo chiude

### Cambi a ripetizione (13)
- [00_09_s10](partite/00_09_s10.md), turno 5, ENZO: Aerodactyl rientra Annihilape uscito 2 turni fa
- [01_01_s14](partite/01_01_s14.md), turno 6, SIMONE: Aggron rientra Toedscruel uscito 2 turni fa
- [02_06_s31](partite/02_06_s31.md), turno 8, TATORE: Scream Tail rientra Armaldo uscito 2 turni fa
- [02_08_s33](partite/02_08_s33.md), turno 8, ELIA: Vaporeon rientra Goodra uscito 2 turni fa
- [03_07_s44](partite/03_07_s44.md), turno 4, MARIO: Politoed rientra Iron Hands uscito 2 turni fa

### Protect consecutivo (8)
- [00_01_s2](partite/00_01_s2.md), turno 8, SIMONE: Aggron Protect dopo Protect al turno prima
- [05_10_s71](partite/05_10_s71.md), turno 5, GIGI PINTO: Torkoal Protect dopo Protect al turno prima
- [07_02_s1](partite/07_02_s1.md), turno 14, MARIO: Politoed Protect dopo Protect al turno prima
- [07_10_s95](partite/07_10_s95.md), turno 2, GIGI PINTO: Torkoal Protect dopo Protect al turno prima
- [07_10_s95](partite/07_10_s95.md), turno 3, GIGI PINTO: Torkoal Protect dopo Protect al turno prima

### Mossa su un immune noto (4)
- [00_02_s3](partite/00_02_s3.md), turno 7, ENZO: Annihilape Close Combat su Banette (immune)
- [04_00_s49](partite/04_00_s49.md), turno 4, ENZO: Grimmsnarl Taunt su Brute Bonnet (immunita' nota)
- [06_00_s73](partite/06_00_s73.md), turno 10, TATORE: Armaldo Superpower su Annihilape (immune)
- [10_05_s126](partite/10_05_s126.md), turno 5, MARIO LETT: Tatsugiri Draco Meteor su Hatterene (immune)

### Team preview: abilita penalizzante senza chi la toglie (2)
- [02_05_s30](partite/02_05_s30.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga
- [05_02_s63](partite/05_02_s63.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga

### Fake Out dopo il primo turno (2)
- [02_11_s36](partite/02_11_s36.md), turno 5, SAVAROTTO: Incineroar Fake Out non al primo turno in campo (fallisce)
- [07_02_s1](partite/07_02_s1.md), turno 6, MARIO: Hariyama Fake Out non al primo turno in campo (fallisce)

## Partite con problemi

- [01_08_s21](partite/01_08_s21.md): BLOCCATA (timeout) (ultimo turno 2)
- [03_00_s37](partite/03_00_s37.md): BLOCCATA (timeout) (ultimo turno 2)
- [09_04_s113](partite/09_04_s113.md): BLOCCATA (timeout) (ultimo turno 1)
- [10_11_s132](partite/10_11_s132.md): BLOCCATA (timeout) (ultimo turno 1)


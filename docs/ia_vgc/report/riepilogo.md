# Self-play VGC: riepilogo

Aggiornato: 2026-10-27 08:39:31. Partite lette: 147 (completate 147, con problemi 0).

Ogni partita: lato G = allenatore della riga, lato A = allenatore della colonna; entrambi IA VGC con team preview propria e informazione chiusa (ognuno vede solo cio che ha osservato dell'altro). Dettagli turno per turno in [partite/](partite/).

Turni per partita: media 7.2, min 2, max 18.
Fitness (gancio M9, misplay certi per partita x -0.5): -0.762

## Misplay per categoria

| Categoria | Misplay certi | Casi dubbi (revisione) |
|---|---:|---:|
| Mossa su un immune noto | 0 | 1 |
| Bersaglio gia KO senza redirect utile | 26 | 0 |
| KO garantito non preso | 73 | 346 |
| Protect consecutivo | 7 | 0 |
| Fake Out dopo il primo turno | 3 | 0 |
| Cambio su chi va KO all'entrata | 24 | 0 |
| Cambi a ripetizione | 9 | 0 |
| Turni di Tailwind/Trick Room sprecati | 14 | 36 |
| Setup con KO in arrivo | 18 | 5 |
| Win condition persa evitabilmente | 48 | 41 |
| Team preview: abilita penalizzante senza chi la toglie | 2 | 0 |
| Team preview: lead che si danneggia da sola | 0 | 26 |
| **Totale** | **224** | **455** |

## Risultati (riga = lato G, colonna = lato A)

V = vince G, S = vince A, P = pareggio, L = limite turni, ! = blocco/crash, . = non giocata

| G \ A | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 ENZO | [V](partite/00_00_s1.md) | [V](partite/00_01_s1.md)[S](partite/00_01_s2.md) | [V](partite/00_02_s3.md) | [S](partite/00_03_s4.md) | [V](partite/00_04_s5.md) | [V](partite/00_05_s6.md) | [V](partite/00_06_s7.md) | [S](partite/00_07_s8.md) | [V](partite/00_08_s9.md) | [S](partite/00_09_s10.md) | [S](partite/00_10_s11.md) | [V](partite/00_11_s12.md) |
| 1 SIMONE | [V](partite/01_00_s13.md) | [V](partite/01_01_s14.md) | [S](partite/01_02_s15.md) | [S](partite/01_03_s16.md) | [S](partite/01_04_s17.md) | [S](partite/01_05_s18.md) | [V](partite/01_06_s19.md) | [V](partite/01_07_s20.md) | [S](partite/01_08_s21.md) | [V](partite/01_09_s22.md) | [S](partite/01_10_s23.md) | [S](partite/01_11_s24.md) |
| 2 ELIA | [S](partite/02_00_s25.md) | [S](partite/02_01_s26.md) | [V](partite/02_02_s27.md) | [S](partite/02_03_s28.md) | [S](partite/02_04_s29.md) | [V](partite/02_05_s30.md) | [S](partite/02_06_s31.md) | [V](partite/02_07_s32.md) | [S](partite/02_08_s33.md) | [S](partite/02_09_s34.md) | [S](partite/02_10_s35.md) | [S](partite/02_11_s36.md) |
| 3 RAFEL | [V](partite/03_00_s37.md) | [S](partite/03_01_s38.md) | [S](partite/03_02_s39.md) | [S](partite/03_03_s40.md) | [S](partite/03_04_s41.md) | [V](partite/03_05_s42.md) | [V](partite/03_06_s43.md) | [V](partite/03_07_s44.md) | [S](partite/03_08_s45.md) | [S](partite/03_09_s46.md) | [S](partite/03_10_s47.md) | [S](partite/03_11_s48.md) |
| 4 TONY | [S](partite/04_00_s49.md) | [V](partite/04_01_s50.md) | [V](partite/04_02_s51.md) | [V](partite/04_03_s52.md) | [S](partite/04_04_s53.md) | [V](partite/04_05_s54.md) | [V](partite/04_06_s55.md) | [V](partite/04_07_s56.md) | [V](partite/04_08_s57.md) | [S](partite/04_09_s58.md) | [S](partite/04_10_s1.md)[V](partite/04_10_s59.md) | [S](partite/04_11_s60.md) |
| 5 MARIO LETT | [S](partite/05_00_s61.md) | [S](partite/05_01_s62.md) | [S](partite/05_02_s63.md) | [V](partite/05_03_s64.md) | [S](partite/05_04_s65.md) | [V](partite/05_05_s66.md) | [V](partite/05_06_s67.md) | [S](partite/05_07_s68.md) | [S](partite/05_08_s69.md) | [S](partite/05_09_s70.md) | [S](partite/05_10_s71.md) | [S](partite/05_11_s72.md) |
| 6 TATORE | [V](partite/06_00_s73.md) | [S](partite/06_01_s74.md) | [V](partite/06_02_s75.md) | [S](partite/06_03_s76.md) | [S](partite/06_04_s77.md) | [S](partite/06_05_s78.md) | [S](partite/06_06_s79.md) | [S](partite/06_07_s80.md) | [S](partite/06_08_s81.md) | [S](partite/06_09_s82.md) | [S](partite/06_10_s83.md) | [V](partite/06_11_s84.md) |
| 7 MARIO | [V](partite/07_00_s85.md) | [V](partite/07_01_s86.md) | [V](partite/07_02_s1.md)[V](partite/07_02_s87.md) | [S](partite/07_03_s88.md) | [V](partite/07_04_s89.md) | [V](partite/07_05_s90.md) | [S](partite/07_06_s91.md) | [S](partite/07_07_s92.md) | [S](partite/07_08_s93.md) | [S](partite/07_09_s94.md) | [S](partite/07_10_s95.md) | [S](partite/07_11_s96.md) |
| 8 GEROZZ | [S](partite/08_00_s97.md) | [V](partite/08_01_s98.md) | [V](partite/08_02_s99.md) | [V](partite/08_03_s100.md) | [V](partite/08_04_s101.md) | [S](partite/08_05_s102.md) | [V](partite/08_06_s103.md) | [V](partite/08_07_s104.md) | [V](partite/08_08_s105.md) | [V](partite/08_09_s106.md) | [V](partite/08_10_s107.md) | [S](partite/08_11_s108.md) |
| 9 CARLETTO | [S](partite/09_00_s109.md) | [S](partite/09_01_s110.md) | [V](partite/09_02_s111.md) | [V](partite/09_03_s112.md) | [V](partite/09_04_s113.md) | [V](partite/09_05_s114.md) | [V](partite/09_06_s115.md) | [V](partite/09_07_s116.md) | [S](partite/09_08_s117.md) | [V](partite/09_09_s118.md) | [V](partite/09_10_s119.md) | [S](partite/09_11_s120.md) |
| 10 GIGI PINTO | [S](partite/10_00_s121.md) | [V](partite/10_01_s122.md) | [V](partite/10_02_s123.md) | [V](partite/10_03_s124.md) | [V](partite/10_04_s125.md) | [S](partite/10_05_s126.md) | [S](partite/10_06_s127.md) | [S](partite/10_07_s128.md) | [S](partite/10_08_s129.md) | [V](partite/10_09_s130.md) | [V](partite/10_10_s131.md) | [V](partite/10_11_s132.md) |
| 11 SAVAROTTO | [S](partite/11_00_s133.md) | [V](partite/11_01_s134.md) | [V](partite/11_02_s135.md) | [V](partite/11_03_s136.md) | [V](partite/11_04_s137.md) | [S](partite/11_05_s138.md) | [S](partite/11_06_s139.md) | [V](partite/11_07_s140.md) | [V](partite/11_08_s141.md) | [S](partite/11_09_s142.md) | [S](partite/11_10_s143.md) | [V](partite/11_11_s144.md) |

## Allenatori

| # | Allenatore | Vittorie / partite | Misplay certi (propri) |
|---:|---|---|---:|
| 0 | ENZO | 15 / 25 | 23 |
| 1 | SIMONE | 11 / 25 | 20 |
| 2 | ELIA | 6 / 25 | 9 |
| 3 | RAFEL | 10 / 24 | 28 |
| 4 | TONY | 14 / 25 | 29 |
| 5 | MARIO LETT | 8 / 24 | 13 |
| 6 | TATORE | 8 / 24 | 15 |
| 7 | MARIO | 11 / 25 | 18 |
| 8 | GEROZZ | 17 / 24 | 16 |
| 9 | CARLETTO | 16 / 24 | 17 |
| 10 | GIGI PINTO | 16 / 25 | 12 |
| 11 | SAVAROTTO | 15 / 24 | 24 |

## Esempi di misplay

### KO garantito non preso (73)
- [01_01_s14](partite/01_01_s14.md), turno 3, SIMONE: Arcanine Flare Blitz avrebbe mandato KO Toedscruel, scelto Extreme Speed
- [01_04_s17](partite/01_04_s17.md), turno 5, SIMONE: Aggron Heavy Slam avrebbe mandato KO Slither Wing, scelto Body Press
- [01_04_s17](partite/01_04_s17.md), turno 5, TONY: Slither Wing Flare Blitz avrebbe mandato KO Rotom, scelto U-turn
- [01_05_s18](partite/01_05_s18.md), turno 4, SIMONE: Aggron Body Press avrebbe mandato KO Dondozo, scelto Heavy Slam
- [01_05_s18](partite/01_05_s18.md), turno 7, SIMONE: Machamp Knock Off avrebbe mandato KO Sinistcha, scelto Bullet Punch

### Win condition persa evitabilmente (48)
- [00_01_s2](partite/00_01_s2.md), turno 4, ENZO: Gengar KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_02_s3](partite/00_02_s3.md), turno 6, ENZO: Annihilape KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_03_s4](partite/00_03_s4.md), turno 3, ENZO: Annihilape KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_11_s12](partite/00_11_s12.md), turno 6, ENZO: Gengar KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_11_s12](partite/00_11_s12.md), turno 7, ENZO: Annihilape KO senza Protect/cambio (cambio possibile): il KO era prevedibile

### Bersaglio gia KO senza redirect utile (26)
- [00_02_s3](partite/00_02_s3.md), turno 9, ENZO: Rillaboom Grassy Glide su Mimikyu che il compagno manda KO prima; nessun redirect utile
- [00_02_s3](partite/00_02_s3.md), turno 10, ENZO: Rillaboom Grassy Glide su Mimikyu che il compagno manda KO prima; nessun redirect utile
- [00_04_s5](partite/00_04_s5.md), turno 5, ENZO: Aerodactyl Dual Wingbeat su Gyarados che il compagno manda KO prima; nessun redirect utile
- [00_05_s6](partite/00_05_s6.md), turno 3, ENZO: Annihilape Close Combat su Tatsugiri che il compagno manda KO prima; nessun redirect utile
- [00_06_s7](partite/00_06_s7.md), turno 9, ENZO: Aerodactyl Dual Wingbeat su Dusclops che il compagno manda KO prima; nessun redirect utile

### Cambio su chi va KO all'entrata (24)
- [00_00_s1](partite/00_00_s1.md), turno 2, ENZO: Gengar entrato con un cambio e KO nello stesso turno
- [00_03_s4](partite/00_03_s4.md), turno 3, ENZO: Gengar entrato con un cambio e KO nello stesso turno
- [00_06_s7](partite/00_06_s7.md), turno 6, TATORE: Kecleon entrato con un cambio e KO nello stesso turno
- [00_09_s10](partite/00_09_s10.md), turno 5, ENZO: Gengar entrato con un cambio e KO nello stesso turno
- [01_06_s19](partite/01_06_s19.md), turno 6, TATORE: Scream Tail entrato con un cambio e KO nello stesso turno

### Setup con KO in arrivo (18)
- [01_03_s16](partite/01_03_s16.md), turno 4, RAFEL: Ceruledge Swords Dance mentre Aggron lo manda KO (danno minimo stimato)
- [01_03_s16](partite/01_03_s16.md), turno 5, RAFEL: Ceruledge Swords Dance mentre Aggron lo manda KO (danno minimo stimato)
- [03_01_s38](partite/03_01_s38.md), turno 7, RAFEL: Copperajah Iron Defense mentre Machamp lo manda KO (danno minimo stimato)
- [03_04_s41](partite/03_04_s41.md), turno 2, RAFEL: Copperajah Iron Defense mentre Charizard lo manda KO (danno minimo stimato)
- [03_06_s43](partite/03_06_s43.md), turno 4, RAFEL: Ceruledge Swords Dance mentre Sceptile lo manda KO (danno minimo stimato)

### Turni di Tailwind/Trick Room sprecati (14)
- [01_06_s19](partite/01_06_s19.md), turno 6, TATORE: Dusclops Trick Room con Trick Room attivo (3 turni): lo chiude
- [03_03_s40](partite/03_03_s40.md), turno 5, RAFEL: Indeedee Trick Room con Trick Room attivo (4 turni): lo chiude
- [03_07_s44](partite/03_07_s44.md), turno 3, RAFEL: Indeedee Trick Room con Trick Room attivo (4 turni): lo chiude
- [04_11_s60](partite/04_11_s60.md), turno 3, SAVAROTTO: Whimsicott Tailwind con il proprio Tailwind ancora attivo (2 turni)
- [05_06_s67](partite/05_06_s67.md), turno 7, MARIO LETT: Sinistcha Trick Room con Trick Room attivo (4 turni): lo chiude

### Cambi a ripetizione (9)
- [00_02_s3](partite/00_02_s3.md), turno 5, ENZO: Gengar rientra Annihilape uscito 2 turni fa
- [00_05_s6](partite/00_05_s6.md), turno 4, ENZO: Annihilape rientra Aerodactyl uscito 2 turni fa
- [02_03_s28](partite/02_03_s28.md), turno 5, RAFEL: Ceruledge rientra Indeedee uscito 2 turni fa
- [02_07_s32](partite/02_07_s32.md), turno 4, ELIA: Banette rientra Goodra uscito 2 turni fa
- [02_07_s32](partite/02_07_s32.md), turno 6, ELIA: Goodra rientra Banette uscito 2 turni fa

### Protect consecutivo (7)
- [07_02_s1](partite/07_02_s1.md), turno 14, MARIO: Politoed Protect dopo Protect al turno prima
- [07_10_s95](partite/07_10_s95.md), turno 2, GIGI PINTO: Torkoal Protect dopo Protect al turno prima
- [07_10_s95](partite/07_10_s95.md), turno 3, GIGI PINTO: Torkoal Protect dopo Protect al turno prima
- [08_07_s104](partite/08_07_s104.md), turno 3, MARIO: Hariyama Protect dopo Protect al turno prima
- [08_10_s107](partite/08_10_s107.md), turno 4, GIGI PINTO: Blastoise Protect dopo Protect al turno prima

### Fake Out dopo il primo turno (3)
- [07_02_s1](partite/07_02_s1.md), turno 6, MARIO: Hariyama Fake Out non al primo turno in campo (fallisce)
- [09_11_s120](partite/09_11_s120.md), turno 5, SAVAROTTO: Incineroar Fake Out non al primo turno in campo (fallisce)
- [11_04_s137](partite/11_04_s137.md), turno 7, SAVAROTTO: Incineroar Fake Out non al primo turno in campo (fallisce)

### Team preview: abilita penalizzante senza chi la toglie (2)
- [02_05_s30](partite/02_05_s30.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga
- [05_02_s63](partite/05_02_s63.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga


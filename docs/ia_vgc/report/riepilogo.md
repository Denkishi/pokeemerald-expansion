# Self-play VGC: riepilogo

Aggiornato: 2026-10-27 08:25:07. Partite lette: 147 (completate 147, con problemi 0).

Ogni partita: lato G = allenatore della riga, lato A = allenatore della colonna; entrambi IA VGC con team preview propria e informazione chiusa (ognuno vede solo cio che ha osservato dell'altro). Dettagli turno per turno in [partite/](partite/).

Turni per partita: media 7.3, min 3, max 20.
Fitness (gancio M9, misplay certi per partita x -0.5): -0.854

## Misplay per categoria

| Categoria | Misplay certi | Casi dubbi (revisione) |
|---|---:|---:|
| Mossa su un immune noto | 4 | 2 |
| Bersaglio gia KO senza redirect utile | 30 | 0 |
| KO garantito non preso | 83 | 381 |
| Attacco sul compagno senza combo | 0 | 1 |
| Protect consecutivo | 10 | 0 |
| Fake Out dopo il primo turno | 1 | 0 |
| Cambio su chi va KO all'entrata | 31 | 0 |
| Cambi a ripetizione | 17 | 0 |
| Turni di Tailwind/Trick Room sprecati | 14 | 31 |
| Setup con KO in arrivo | 19 | 4 |
| Win condition persa evitabilmente | 40 | 51 |
| Team preview: abilita penalizzante senza chi la toglie | 2 | 0 |
| Team preview: lead che si danneggia da sola | 0 | 26 |
| **Totale** | **251** | **496** |

## Risultati (riga = lato G, colonna = lato A)

V = vince G, S = vince A, P = pareggio, L = limite turni, ! = blocco/crash, . = non giocata

| G \ A | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 ENZO | [V](partite/00_00_s1.md) | [V](partite/00_01_s1.md)[S](partite/00_01_s2.md) | [V](partite/00_02_s3.md) | [V](partite/00_03_s4.md) | [V](partite/00_04_s5.md) | [V](partite/00_05_s6.md) | [V](partite/00_06_s7.md) | [S](partite/00_07_s8.md) | [V](partite/00_08_s9.md) | [V](partite/00_09_s10.md) | [V](partite/00_10_s11.md) | [V](partite/00_11_s12.md) |
| 1 SIMONE | [V](partite/01_00_s13.md) | [V](partite/01_01_s14.md) | [V](partite/01_02_s15.md) | [S](partite/01_03_s16.md) | [S](partite/01_04_s17.md) | [S](partite/01_05_s18.md) | [V](partite/01_06_s19.md) | [S](partite/01_07_s20.md) | [S](partite/01_08_s21.md) | [S](partite/01_09_s22.md) | [S](partite/01_10_s23.md) | [S](partite/01_11_s24.md) |
| 2 ELIA | [S](partite/02_00_s25.md) | [S](partite/02_01_s26.md) | [V](partite/02_02_s27.md) | [V](partite/02_03_s28.md) | [S](partite/02_04_s29.md) | [V](partite/02_05_s30.md) | [S](partite/02_06_s31.md) | [S](partite/02_07_s32.md) | [S](partite/02_08_s33.md) | [S](partite/02_09_s34.md) | [S](partite/02_10_s35.md) | [V](partite/02_11_s36.md) |
| 3 RAFEL | [V](partite/03_00_s37.md) | [S](partite/03_01_s38.md) | [V](partite/03_02_s39.md) | [S](partite/03_03_s40.md) | [S](partite/03_04_s41.md) | [V](partite/03_05_s42.md) | [V](partite/03_06_s43.md) | [V](partite/03_07_s44.md) | [S](partite/03_08_s45.md) | [S](partite/03_09_s46.md) | [S](partite/03_10_s47.md) | [V](partite/03_11_s48.md) |
| 4 TONY | [S](partite/04_00_s49.md) | [V](partite/04_01_s50.md) | [V](partite/04_02_s51.md) | [V](partite/04_03_s52.md) | [S](partite/04_04_s53.md) | [V](partite/04_05_s54.md) | [S](partite/04_06_s55.md) | [V](partite/04_07_s56.md) | [V](partite/04_08_s57.md) | [V](partite/04_09_s58.md) | [S](partite/04_10_s1.md)[S](partite/04_10_s59.md) | [S](partite/04_11_s60.md) |
| 5 MARIO LETT | [S](partite/05_00_s61.md) | [V](partite/05_01_s62.md) | [S](partite/05_02_s63.md) | [S](partite/05_03_s64.md) | [S](partite/05_04_s65.md) | [S](partite/05_05_s66.md) | [V](partite/05_06_s67.md) | [S](partite/05_07_s68.md) | [S](partite/05_08_s69.md) | [S](partite/05_09_s70.md) | [V](partite/05_10_s71.md) | [V](partite/05_11_s72.md) |
| 6 TATORE | [S](partite/06_00_s73.md) | [S](partite/06_01_s74.md) | [V](partite/06_02_s75.md) | [S](partite/06_03_s76.md) | [S](partite/06_04_s77.md) | [S](partite/06_05_s78.md) | [S](partite/06_06_s79.md) | [S](partite/06_07_s80.md) | [S](partite/06_08_s81.md) | [S](partite/06_09_s82.md) | [S](partite/06_10_s83.md) | [S](partite/06_11_s84.md) |
| 7 MARIO | [V](partite/07_00_s85.md) | [V](partite/07_01_s86.md) | [V](partite/07_02_s1.md)[V](partite/07_02_s87.md) | [S](partite/07_03_s88.md) | [S](partite/07_04_s89.md) | [V](partite/07_05_s90.md) | [V](partite/07_06_s91.md) | [S](partite/07_07_s92.md) | [S](partite/07_08_s93.md) | [S](partite/07_09_s94.md) | [V](partite/07_10_s95.md) | [S](partite/07_11_s96.md) |
| 8 GEROZZ | [S](partite/08_00_s97.md) | [V](partite/08_01_s98.md) | [S](partite/08_02_s99.md) | [S](partite/08_03_s100.md) | [S](partite/08_04_s101.md) | [V](partite/08_05_s102.md) | [V](partite/08_06_s103.md) | [V](partite/08_07_s104.md) | [V](partite/08_08_s105.md) | [V](partite/08_09_s106.md) | [V](partite/08_10_s107.md) | [S](partite/08_11_s108.md) |
| 9 CARLETTO | [V](partite/09_00_s109.md) | [S](partite/09_01_s110.md) | [V](partite/09_02_s111.md) | [V](partite/09_03_s112.md) | [S](partite/09_04_s113.md) | [V](partite/09_05_s114.md) | [S](partite/09_06_s115.md) | [V](partite/09_07_s116.md) | [S](partite/09_08_s117.md) | [V](partite/09_09_s118.md) | [S](partite/09_10_s119.md) | [V](partite/09_11_s120.md) |
| 10 GIGI PINTO | [S](partite/10_00_s121.md) | [V](partite/10_01_s122.md) | [V](partite/10_02_s123.md) | [V](partite/10_03_s124.md) | [S](partite/10_04_s125.md) | [S](partite/10_05_s126.md) | [V](partite/10_06_s127.md) | [V](partite/10_07_s128.md) | [S](partite/10_08_s129.md) | [V](partite/10_09_s130.md) | [V](partite/10_10_s131.md) | [V](partite/10_11_s132.md) |
| 11 SAVAROTTO | [S](partite/11_00_s133.md) | [V](partite/11_01_s134.md) | [V](partite/11_02_s135.md) | [V](partite/11_03_s136.md) | [V](partite/11_04_s137.md) | [V](partite/11_05_s138.md) | [S](partite/11_06_s139.md) | [V](partite/11_07_s140.md) | [S](partite/11_08_s141.md) | [S](partite/11_09_s142.md) | [S](partite/11_10_s143.md) | [V](partite/11_11_s144.md) |

## Allenatori

| # | Allenatore | Vittorie / partite | Misplay certi (propri) |
|---:|---|---|---:|
| 0 | ENZO | 18 / 25 | 16 |
| 1 | SIMONE | 9 / 25 | 22 |
| 2 | ELIA | 6 / 25 | 20 |
| 3 | RAFEL | 12 / 24 | 25 |
| 4 | TONY | 17 / 25 | 28 |
| 5 | MARIO LETT | 8 / 24 | 9 |
| 6 | TATORE | 6 / 24 | 14 |
| 7 | MARIO | 13 / 25 | 25 |
| 8 | GEROZZ | 16 / 24 | 20 |
| 9 | CARLETTO | 14 / 24 | 26 |
| 10 | GIGI PINTO | 16 / 25 | 20 |
| 11 | SAVAROTTO | 12 / 24 | 26 |

## Esempi di misplay

### KO garantito non preso (83)
- [01_02_s15](partite/01_02_s15.md), turno 3, SIMONE: Machamp Close Combat avrebbe mandato KO Vaporeon, scelto Bullet Punch
- [01_02_s15](partite/01_02_s15.md), turno 4, ELIA: Mimikyu Shadow Sneak avrebbe mandato KO Toedscruel, scelto Play Rough
- [01_04_s17](partite/01_04_s17.md), turno 5, TONY: Slither Wing Flare Blitz avrebbe mandato KO Rotom, scelto U-turn
- [01_05_s18](partite/01_05_s18.md), turno 6, SIMONE: Machamp Close Combat avrebbe mandato KO Tatsugiri, scelto Bullet Punch
- [01_05_s18](partite/01_05_s18.md), turno 6, SIMONE: Aggron Body Press avrebbe mandato KO Tatsugiri, scelto Heavy Slam

### Win condition persa evitabilmente (40)
- [00_01_s2](partite/00_01_s2.md), turno 4, ENZO: Gengar KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_08_s9](partite/00_08_s9.md), turno 3, ENZO: Annihilape KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_09_s10](partite/00_09_s10.md), turno 5, CARLETTO: Kingambit KO senza Protect/cambio (Protect disponibile): il KO era prevedibile
- [01_00_s13](partite/01_00_s13.md), turno 4, ENZO: Gengar KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [01_03_s16](partite/01_03_s16.md), turno 3, SIMONE: Aggron KO senza Protect/cambio (Protect disponibile): il KO era prevedibile

### Cambio su chi va KO all'entrata (31)
- [01_00_s13](partite/01_00_s13.md), turno 4, ENZO: Rillaboom entrato con un cambio e KO nello stesso turno
- [01_05_s18](partite/01_05_s18.md), turno 3, SIMONE: Arcanine entrato con un cambio e KO nello stesso turno
- [01_06_s19](partite/01_06_s19.md), turno 6, TATORE: Scream Tail entrato con un cambio e KO nello stesso turno
- [01_07_s20](partite/01_07_s20.md), turno 4, SIMONE: Toedscruel entrato con un cambio e KO nello stesso turno
- [02_01_s26](partite/02_01_s26.md), turno 2, ELIA: Vaporeon entrato con un cambio e KO nello stesso turno

### Bersaglio gia KO senza redirect utile (30)
- [00_02_s3](partite/00_02_s3.md), turno 5, ENZO: Annihilape Ice Punch su Lickitung che il compagno manda KO prima; nessun redirect utile
- [01_00_s13](partite/01_00_s13.md), turno 5, SIMONE: Aggron Heavy Slam su Annihilape che il compagno manda KO prima; nessun redirect utile
- [01_01_s14](partite/01_01_s14.md), turno 6, SIMONE: Aggron Body Press su Aggron che il compagno manda KO prima; nessun redirect utile
- [01_01_s14](partite/01_01_s14.md), turno 7, SIMONE: Aggron Body Press su Aggron che il compagno manda KO prima; nessun redirect utile
- [01_07_s20](partite/01_07_s20.md), turno 7, MARIO: Oranguru Foul Play su Rotom che il compagno manda KO prima; nessun redirect utile

### Setup con KO in arrivo (19)
- [00_02_s3](partite/00_02_s3.md), turno 3, ELIA: Lickitung Iron Defense mentre Rillaboom lo manda KO (danno minimo stimato)
- [00_02_s3](partite/00_02_s3.md), turno 5, ELIA: Lickitung Iron Defense mentre Annihilape lo manda KO (danno minimo stimato)
- [01_03_s16](partite/01_03_s16.md), turno 3, RAFEL: Ceruledge Swords Dance mentre Aggron lo manda KO (danno minimo stimato)
- [02_00_s25](partite/02_00_s25.md), turno 3, ELIA: Lickitung Iron Defense mentre Rillaboom lo manda KO (danno minimo stimato)
- [02_03_s28](partite/02_03_s28.md), turno 3, RAFEL: Ceruledge Swords Dance mentre Goodra lo manda KO (danno minimo stimato)

### Cambi a ripetizione (17)
- [00_09_s10](partite/00_09_s10.md), turno 5, ENZO: Aerodactyl rientra Annihilape uscito 2 turni fa
- [00_11_s12](partite/00_11_s12.md), turno 5, SAVAROTTO: Whimsicott rientra Incineroar uscito 2 turni fa
- [02_08_s33](partite/02_08_s33.md), turno 8, GEROZZ: Gholdengo rientra Porygon2 uscito 2 turni fa
- [03_07_s44](partite/03_07_s44.md), turno 4, MARIO: Politoed rientra Iron Hands uscito 2 turni fa
- [04_02_s51](partite/04_02_s51.md), turno 5, TONY: Brute Bonnet rientra Roaring Moon uscito 2 turni fa

### Turni di Tailwind/Trick Room sprecati (14)
- [01_06_s19](partite/01_06_s19.md), turno 4, TATORE: Scream Tail Trick Room con Trick Room attivo (4 turni): lo chiude
- [01_06_s19](partite/01_06_s19.md), turno 4, SIMONE: Toedscruel Trick Room con Trick Room attivo (4 turni): lo chiude
- [01_06_s19](partite/01_06_s19.md), turno 6, TATORE: Dusclops Trick Room con Trick Room attivo (3 turni): lo chiude
- [02_08_s33](partite/02_08_s33.md), turno 4, ELIA: Mimikyu Trick Room con Trick Room attivo (3 turni): lo chiude
- [02_08_s33](partite/02_08_s33.md), turno 5, ELIA: Mimikyu Trick Room con Trick Room attivo (4 turni): lo chiude

### Protect consecutivo (10)
- [01_01_s14](partite/01_01_s14.md), turno 6, SIMONE: Aggron Protect dopo Protect al turno prima
- [02_02_s27](partite/02_02_s27.md), turno 19, ELIA: Mimikyu Protect dopo Protect al turno prima
- [02_02_s27](partite/02_02_s27.md), turno 20, ELIA: Mimikyu Protect dopo Protect al turno prima
- [04_02_s51](partite/04_02_s51.md), turno 7, TONY: Charizard Protect dopo Protect al turno prima
- [04_02_s51](partite/04_02_s51.md), turno 8, TONY: Charizard Protect dopo Protect al turno prima

### Mossa su un immune noto (4)
- [01_09_s22](partite/01_09_s22.md), turno 7, SIMONE: Toedscruel Spore su Amoonguss (immunita' nota)
- [06_00_s73](partite/06_00_s73.md), turno 8, ENZO: Annihilape Close Combat su Dusclops (immune)
- [08_07_s104](partite/08_07_s104.md), turno 4, MARIO: Iron Hands Wild Charge su Great Tusk (immune)
- [09_08_s117](partite/09_08_s117.md), turno 7, CARLETTO: Malamar Superpower su Gholdengo (immune)

### Team preview: abilita penalizzante senza chi la toglie (2)
- [02_05_s30](partite/02_05_s30.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga
- [05_02_s63](partite/05_02_s63.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga

### Fake Out dopo il primo turno (1)
- [07_02_s1](partite/07_02_s1.md), turno 6, MARIO: Hariyama Fake Out non al primo turno in campo (fallisce)


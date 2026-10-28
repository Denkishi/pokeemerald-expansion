# Self-play VGC: riepilogo

Aggiornato: 2026-10-28 23:18:27. Partite lette: 432 (completate 432, con problemi 0).

Ogni partita: lato G = allenatore della riga, lato A = allenatore della colonna; entrambi IA VGC con team preview propria e informazione chiusa (ognuno vede solo cio che ha osservato dell'altro). Dettagli turno per turno in [partite/](partite/).

Turni per partita: media 7.5, min 2, max 39.
Fitness (gancio M9, misplay certi per partita x -0.5): -0.306

## Misplay per categoria

| Categoria | Misplay certi | Casi dubbi (revisione) |
|---|---:|---:|
| Mossa su un immune noto | 0 | 3 |
| Bersaglio gia KO senza redirect utile | 13 | 0 |
| KO garantito non preso | 59 | 0 |
| KO non preso: lettura di un Protect visto | 0 | 140 |
| KO non preso: scelta una mossa ad area | 0 | 32 |
| KO non preso: scelto supporto/Protect/cambio | 0 | 396 |
| Protect consecutivo | 1 | 0 |
| Fake Out dopo il primo turno | 1 | 0 |
| Cambio su chi va KO all'entrata | 56 | 6 |
| Cambi a ripetizione | 1 | 45 |
| Turni di Tailwind/Trick Room sprecati | 0 | 23 |
| Setup con KO in arrivo | 27 | 21 |
| Win condition persa evitabilmente | 106 | 152 |
| Team preview: lead che si danneggia da sola | 0 | 72 |
| **Totale** | **264** | **890** |

## Risultati (riga = lato G, colonna = lato A)

V = vince G, S = vince A, P = pareggio, L = limite turni, ! = blocco/crash, . = non giocata

| G \ A | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 ENZO | [V](partite/00_00_s1.md)[V](partite/00_00_s1001.md)[S](partite/00_00_s2001.md) | [V](partite/00_01_s1002.md)[S](partite/00_01_s2.md)[V](partite/00_01_s2002.md) | [V](partite/00_02_s1003.md)[V](partite/00_02_s2003.md)[V](partite/00_02_s3.md) | [S](partite/00_03_s1004.md)[S](partite/00_03_s2004.md)[S](partite/00_03_s4.md) | [V](partite/00_04_s1005.md)[V](partite/00_04_s2005.md)[V](partite/00_04_s5.md) | [V](partite/00_05_s1006.md)[V](partite/00_05_s2006.md)[V](partite/00_05_s6.md) | [V](partite/00_06_s1007.md)[V](partite/00_06_s2007.md)[S](partite/00_06_s7.md) | [V](partite/00_07_s1008.md)[V](partite/00_07_s2008.md)[V](partite/00_07_s8.md) | [S](partite/00_08_s1009.md)[S](partite/00_08_s2009.md)[S](partite/00_08_s9.md) | [V](partite/00_09_s10.md)[V](partite/00_09_s1010.md)[V](partite/00_09_s2010.md) | [V](partite/00_10_s1011.md)[S](partite/00_10_s11.md)[S](partite/00_10_s2011.md) | [S](partite/00_11_s1012.md)[V](partite/00_11_s12.md)[S](partite/00_11_s2012.md) |
| 1 SIMONE | [S](partite/01_00_s1013.md)[S](partite/01_00_s13.md)[S](partite/01_00_s2013.md) | [V](partite/01_01_s1014.md)[V](partite/01_01_s14.md)[V](partite/01_01_s2014.md) | [V](partite/01_02_s1015.md)[V](partite/01_02_s15.md)[V](partite/01_02_s2015.md) | [S](partite/01_03_s1016.md)[S](partite/01_03_s16.md)[S](partite/01_03_s2016.md) | [V](partite/01_04_s1017.md)[S](partite/01_04_s17.md)[S](partite/01_04_s2017.md) | [V](partite/01_05_s1018.md)[V](partite/01_05_s18.md)[V](partite/01_05_s2018.md) | [V](partite/01_06_s1019.md)[V](partite/01_06_s19.md)[V](partite/01_06_s2019.md) | [S](partite/01_07_s1020.md)[S](partite/01_07_s20.md)[S](partite/01_07_s2020.md) | [S](partite/01_08_s1021.md)[S](partite/01_08_s2021.md)[S](partite/01_08_s21.md) | [S](partite/01_09_s1022.md)[V](partite/01_09_s2022.md)[V](partite/01_09_s22.md) | [S](partite/01_10_s1023.md)[S](partite/01_10_s2023.md)[S](partite/01_10_s23.md) | [S](partite/01_11_s1024.md)[S](partite/01_11_s2024.md)[S](partite/01_11_s24.md) |
| 2 ELIA | [S](partite/02_00_s1025.md)[S](partite/02_00_s2025.md)[S](partite/02_00_s25.md) | [S](partite/02_01_s1026.md)[S](partite/02_01_s2026.md)[S](partite/02_01_s26.md) | [V](partite/02_02_s1027.md)[V](partite/02_02_s2027.md)[V](partite/02_02_s27.md) | [S](partite/02_03_s1028.md)[S](partite/02_03_s2028.md)[S](partite/02_03_s28.md) | [V](partite/02_04_s1029.md)[S](partite/02_04_s2029.md)[S](partite/02_04_s29.md) | [V](partite/02_05_s1030.md)[S](partite/02_05_s2030.md)[V](partite/02_05_s30.md) | [V](partite/02_06_s1031.md)[S](partite/02_06_s2031.md)[V](partite/02_06_s31.md) | [S](partite/02_07_s1032.md)[S](partite/02_07_s2032.md)[S](partite/02_07_s32.md) | [S](partite/02_08_s1033.md)[S](partite/02_08_s2033.md)[S](partite/02_08_s33.md) | [S](partite/02_09_s1034.md)[S](partite/02_09_s2034.md)[S](partite/02_09_s34.md) | [S](partite/02_10_s1035.md)[S](partite/02_10_s2035.md)[S](partite/02_10_s35.md) | [S](partite/02_11_s1036.md)[S](partite/02_11_s2036.md)[S](partite/02_11_s36.md) |
| 3 RAFEL | [V](partite/03_00_s1037.md)[V](partite/03_00_s2037.md)[V](partite/03_00_s37.md) | [V](partite/03_01_s1038.md)[V](partite/03_01_s2038.md)[V](partite/03_01_s38.md) | [V](partite/03_02_s1039.md)[S](partite/03_02_s2039.md)[S](partite/03_02_s39.md) | [S](partite/03_03_s1040.md)[S](partite/03_03_s2040.md)[V](partite/03_03_s40.md) | [S](partite/03_04_s1041.md)[S](partite/03_04_s2041.md)[S](partite/03_04_s41.md) | [V](partite/03_05_s1042.md)[V](partite/03_05_s2042.md)[S](partite/03_05_s42.md) | [V](partite/03_06_s1043.md)[S](partite/03_06_s2043.md)[V](partite/03_06_s43.md) | [S](partite/03_07_s1044.md)[V](partite/03_07_s2044.md)[V](partite/03_07_s44.md) | [S](partite/03_08_s1045.md)[S](partite/03_08_s2045.md)[V](partite/03_08_s45.md) | [S](partite/03_09_s1046.md)[S](partite/03_09_s2046.md)[S](partite/03_09_s46.md) | [S](partite/03_10_s1047.md)[S](partite/03_10_s2047.md)[S](partite/03_10_s47.md) | [S](partite/03_11_s1048.md)[S](partite/03_11_s2048.md)[S](partite/03_11_s48.md) |
| 4 TONY | [S](partite/04_00_s1049.md)[S](partite/04_00_s2049.md)[S](partite/04_00_s49.md) | [S](partite/04_01_s1050.md)[S](partite/04_01_s2050.md)[V](partite/04_01_s50.md) | [V](partite/04_02_s1051.md)[V](partite/04_02_s2051.md)[S](partite/04_02_s51.md) | [V](partite/04_03_s1052.md)[V](partite/04_03_s2052.md)[V](partite/04_03_s52.md) | [V](partite/04_04_s1053.md)[V](partite/04_04_s2053.md)[S](partite/04_04_s53.md) | [S](partite/04_05_s1054.md)[V](partite/04_05_s2054.md)[V](partite/04_05_s54.md) | [V](partite/04_06_s1055.md)[S](partite/04_06_s2055.md)[V](partite/04_06_s55.md) | [V](partite/04_07_s1056.md)[S](partite/04_07_s2056.md)[S](partite/04_07_s56.md) | [V](partite/04_08_s1057.md)[V](partite/04_08_s2057.md)[V](partite/04_08_s57.md) | [V](partite/04_09_s1058.md)[V](partite/04_09_s2058.md)[V](partite/04_09_s58.md) | [V](partite/04_10_s1059.md)[S](partite/04_10_s2059.md)[V](partite/04_10_s59.md) | [V](partite/04_11_s1060.md)[S](partite/04_11_s2060.md)[S](partite/04_11_s60.md) |
| 5 MARIO LETT | [S](partite/05_00_s1061.md)[S](partite/05_00_s2061.md)[V](partite/05_00_s61.md) | [S](partite/05_01_s1062.md)[S](partite/05_01_s2062.md)[S](partite/05_01_s62.md) | [V](partite/05_02_s1063.md)[V](partite/05_02_s2063.md)[V](partite/05_02_s63.md) | [S](partite/05_03_s1064.md)[V](partite/05_03_s2064.md)[V](partite/05_03_s64.md) | [S](partite/05_04_s1065.md)[S](partite/05_04_s2065.md)[S](partite/05_04_s65.md) | [V](partite/05_05_s1066.md)[S](partite/05_05_s2066.md)[V](partite/05_05_s66.md) | [S](partite/05_06_s1067.md)[V](partite/05_06_s2067.md)[V](partite/05_06_s67.md) | [S](partite/05_07_s1068.md)[S](partite/05_07_s2068.md)[V](partite/05_07_s68.md) | [S](partite/05_08_s1069.md)[V](partite/05_08_s2069.md)[S](partite/05_08_s69.md) | [S](partite/05_09_s1070.md)[V](partite/05_09_s2070.md)[S](partite/05_09_s70.md) | [S](partite/05_10_s1071.md)[V](partite/05_10_s2071.md)[V](partite/05_10_s71.md) | [V](partite/05_11_s1072.md)[V](partite/05_11_s2072.md)[V](partite/05_11_s72.md) |
| 6 TATORE | [S](partite/06_00_s1073.md)[S](partite/06_00_s2073.md)[S](partite/06_00_s73.md) | [S](partite/06_01_s1074.md)[S](partite/06_01_s2074.md)[S](partite/06_01_s74.md) | [S](partite/06_02_s1075.md)[S](partite/06_02_s2075.md)[S](partite/06_02_s75.md) | [V](partite/06_03_s1076.md)[S](partite/06_03_s2076.md)[S](partite/06_03_s76.md) | [S](partite/06_04_s1077.md)[V](partite/06_04_s2077.md)[V](partite/06_04_s77.md) | [V](partite/06_05_s1078.md)[V](partite/06_05_s2078.md)[V](partite/06_05_s78.md) | [V](partite/06_06_s1079.md)[V](partite/06_06_s2079.md)[V](partite/06_06_s79.md) | [S](partite/06_07_s1080.md)[S](partite/06_07_s2080.md)[S](partite/06_07_s80.md) | [S](partite/06_08_s1081.md)[S](partite/06_08_s2081.md)[S](partite/06_08_s81.md) | [S](partite/06_09_s1082.md)[S](partite/06_09_s2082.md)[S](partite/06_09_s82.md) | [S](partite/06_10_s1083.md)[S](partite/06_10_s2083.md)[S](partite/06_10_s83.md) | [S](partite/06_11_s1084.md)[V](partite/06_11_s2084.md)[V](partite/06_11_s84.md) |
| 7 MARIO | [V](partite/07_00_s1085.md)[S](partite/07_00_s2085.md)[S](partite/07_00_s85.md) | [V](partite/07_01_s1086.md)[S](partite/07_01_s2086.md)[S](partite/07_01_s86.md) | [S](partite/07_02_s1087.md)[V](partite/07_02_s2087.md)[V](partite/07_02_s87.md) | [V](partite/07_03_s1088.md)[V](partite/07_03_s2088.md)[V](partite/07_03_s88.md) | [S](partite/07_04_s1089.md)[V](partite/07_04_s2089.md)[S](partite/07_04_s89.md) | [S](partite/07_05_s1090.md)[V](partite/07_05_s2090.md)[V](partite/07_05_s90.md) | [V](partite/07_06_s1091.md)[S](partite/07_06_s2091.md)[V](partite/07_06_s91.md) | [V](partite/07_07_s1092.md)[V](partite/07_07_s2092.md)[V](partite/07_07_s92.md) | [V](partite/07_08_s1093.md)[V](partite/07_08_s2093.md)[S](partite/07_08_s93.md) | [S](partite/07_09_s1094.md)[S](partite/07_09_s2094.md)[S](partite/07_09_s94.md) | [S](partite/07_10_s1095.md)[V](partite/07_10_s2095.md)[S](partite/07_10_s95.md) | [V](partite/07_11_s1096.md)[S](partite/07_11_s2096.md)[S](partite/07_11_s96.md) |
| 8 GEROZZ | [S](partite/08_00_s1097.md)[V](partite/08_00_s2097.md)[V](partite/08_00_s97.md) | [V](partite/08_01_s1098.md)[V](partite/08_01_s2098.md)[S](partite/08_01_s98.md) | [V](partite/08_02_s1099.md)[V](partite/08_02_s2099.md)[V](partite/08_02_s99.md) | [V](partite/08_03_s100.md)[S](partite/08_03_s1100.md)[V](partite/08_03_s2100.md) | [S](partite/08_04_s101.md)[V](partite/08_04_s1101.md)[S](partite/08_04_s2101.md) | [V](partite/08_05_s102.md)[V](partite/08_05_s1102.md)[V](partite/08_05_s2102.md) | [V](partite/08_06_s103.md)[V](partite/08_06_s1103.md)[V](partite/08_06_s2103.md) | [V](partite/08_07_s104.md)[V](partite/08_07_s1104.md)[V](partite/08_07_s2104.md) | [S](partite/08_08_s105.md)[S](partite/08_08_s1105.md)[S](partite/08_08_s2105.md) | [S](partite/08_09_s106.md)[S](partite/08_09_s1106.md)[V](partite/08_09_s2106.md) | [S](partite/08_10_s107.md)[S](partite/08_10_s1107.md)[V](partite/08_10_s2107.md) | [V](partite/08_11_s108.md)[V](partite/08_11_s1108.md)[V](partite/08_11_s2108.md) |
| 9 CARLETTO | [S](partite/09_00_s109.md)[V](partite/09_00_s1109.md)[V](partite/09_00_s2109.md) | [V](partite/09_01_s110.md)[S](partite/09_01_s1110.md)[S](partite/09_01_s2110.md) | [V](partite/09_02_s111.md)[V](partite/09_02_s1111.md)[V](partite/09_02_s2111.md) | [V](partite/09_03_s1112.md)[V](partite/09_03_s112.md)[V](partite/09_03_s2112.md) | [S](partite/09_04_s1113.md)[V](partite/09_04_s113.md)[V](partite/09_04_s2113.md) | [V](partite/09_05_s1114.md)[V](partite/09_05_s114.md)[S](partite/09_05_s2114.md) | [V](partite/09_06_s1115.md)[V](partite/09_06_s115.md)[V](partite/09_06_s2115.md) | [V](partite/09_07_s1116.md)[V](partite/09_07_s116.md)[V](partite/09_07_s2116.md) | [S](partite/09_08_s1117.md)[S](partite/09_08_s117.md)[V](partite/09_08_s2117.md) | [S](partite/09_09_s1118.md)[V](partite/09_09_s118.md)[V](partite/09_09_s2118.md) | [S](partite/09_10_s1119.md)[S](partite/09_10_s119.md)[S](partite/09_10_s2119.md) | [V](partite/09_11_s1120.md)[V](partite/09_11_s120.md)[V](partite/09_11_s2120.md) |
| 10 GIGI PINTO | [V](partite/10_00_s1121.md)[S](partite/10_00_s121.md)[V](partite/10_00_s2121.md) | [V](partite/10_01_s1122.md)[V](partite/10_01_s122.md)[V](partite/10_01_s2122.md) | [V](partite/10_02_s1123.md)[S](partite/10_02_s123.md)[V](partite/10_02_s2123.md) | [S](partite/10_03_s1124.md)[S](partite/10_03_s124.md)[V](partite/10_03_s2124.md) | [V](partite/10_04_s1125.md)[V](partite/10_04_s125.md)[V](partite/10_04_s2125.md) | [S](partite/10_05_s1126.md)[S](partite/10_05_s126.md)[S](partite/10_05_s2126.md) | [V](partite/10_06_s1127.md)[V](partite/10_06_s127.md)[V](partite/10_06_s2127.md) | [V](partite/10_07_s1128.md)[V](partite/10_07_s128.md)[V](partite/10_07_s2128.md) | [S](partite/10_08_s1129.md)[S](partite/10_08_s129.md)[S](partite/10_08_s2129.md) | [S](partite/10_09_s1130.md)[S](partite/10_09_s130.md)[V](partite/10_09_s2130.md) | [S](partite/10_10_s1131.md)[S](partite/10_10_s131.md)[V](partite/10_10_s2131.md) | [S](partite/10_11_s1132.md)[V](partite/10_11_s132.md)[V](partite/10_11_s2132.md) |
| 11 SAVAROTTO | [V](partite/11_00_s1133.md)[S](partite/11_00_s133.md)[S](partite/11_00_s2133.md) | [V](partite/11_01_s1134.md)[V](partite/11_01_s134.md)[V](partite/11_01_s2134.md) | [S](partite/11_02_s1135.md)[V](partite/11_02_s135.md)[V](partite/11_02_s2135.md) | [V](partite/11_03_s1136.md)[V](partite/11_03_s136.md)[V](partite/11_03_s2136.md) | [S](partite/11_04_s1137.md)[V](partite/11_04_s137.md)[V](partite/11_04_s2137.md) | [S](partite/11_05_s1138.md)[S](partite/11_05_s138.md)[S](partite/11_05_s2138.md) | [V](partite/11_06_s1139.md)[V](partite/11_06_s139.md)[V](partite/11_06_s2139.md) | [S](partite/11_07_s1140.md)[V](partite/11_07_s140.md)[V](partite/11_07_s2140.md) | [V](partite/11_08_s1141.md)[S](partite/11_08_s141.md)[S](partite/11_08_s2141.md) | [S](partite/11_09_s1142.md)[S](partite/11_09_s142.md)[S](partite/11_09_s2142.md) | [S](partite/11_10_s1143.md)[V](partite/11_10_s143.md)[V](partite/11_10_s2143.md) | [V](partite/11_11_s1144.md)[V](partite/11_11_s144.md)[S](partite/11_11_s2144.md) |

## Allenatori

| # | Allenatore | Vittorie / partite | Misplay certi (propri) |
|---:|---|---|---:|
| 0 | ENZO | 45 / 72 | 38 |
| 1 | SIMONE | 32 / 72 | 10 |
| 2 | ELIA | 17 / 72 | 23 |
| 3 | RAFEL | 32 / 72 | 30 |
| 4 | TONY | 40 / 72 | 27 |
| 5 | MARIO LETT | 30 / 72 | 10 |
| 6 | TATORE | 17 / 72 | 16 |
| 7 | MARIO | 34 / 72 | 23 |
| 8 | GEROZZ | 51 / 72 | 20 |
| 9 | CARLETTO | 48 / 72 | 23 |
| 10 | GIGI PINTO | 47 / 72 | 11 |
| 11 | SAVAROTTO | 39 / 72 | 33 |

## Esempi di misplay

### Win condition persa evitabilmente (106)
- [00_00_s1](partite/00_00_s1.md), turno 2, ENZO: Annihilape KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_00_s1](partite/00_00_s1.md), turno 3, ENZO: Gengar KO senza Protect/cambio (Protect disponibile, cambio possibile): il KO era prevedibile
- [00_00_s2001](partite/00_00_s2001.md), turno 2, ENZO: Gengar KO senza Protect/cambio (Protect disponibile): il KO era prevedibile
- [00_00_s2001](partite/00_00_s2001.md), turno 3, ENZO: Annihilape KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_01_s1002](partite/00_01_s1002.md), turno 4, ENZO: Annihilape KO senza Protect/cambio (cambio possibile): il KO era prevedibile

### KO garantito non preso (59)
- [00_02_s3](partite/00_02_s3.md), turno 4, ENZO: Rillaboom Wood Hammer avrebbe mandato KO Vaporeon, scelto Grassy Glide
- [00_07_s8](partite/00_07_s8.md), turno 1, MARIO: Iron Hands Close Combat avrebbe mandato KO Grimmsnarl, scelto Play Rough
- [00_08_s1009](partite/00_08_s1009.md), turno 2, GEROZZ: Great Tusk Close Combat avrebbe mandato KO Grimmsnarl, scelto Close Combat
- [00_08_s9](partite/00_08_s9.md), turno 2, GEROZZ: Great Tusk Close Combat avrebbe mandato KO Grimmsnarl, scelto Close Combat
- [00_09_s2010](partite/00_09_s2010.md), turno 7, ENZO: Gengar Shadow Ball avrebbe mandato KO Amoonguss, scelto Sludge Bomb

### Cambio su chi va KO all'entrata (56)
- [00_00_s1001](partite/00_00_s1001.md), turno 3, ENZO: Rillaboom entrato con un cambio e KO nello stesso turno
- [00_07_s1008](partite/00_07_s1008.md), turno 3, ENZO: Annihilape entrato con un cambio e KO nello stesso turno
- [00_10_s1011](partite/00_10_s1011.md), turno 4, ENZO: Grimmsnarl entrato con un cambio e KO nello stesso turno
- [01_01_s14](partite/01_01_s14.md), turno 6, SIMONE: Toedscruel entrato con un cambio e KO nello stesso turno
- [01_01_s2014](partite/01_01_s2014.md), turno 5, SIMONE: Machamp entrato con un cambio e KO nello stesso turno

### Setup con KO in arrivo (27)
- [00_02_s1003](partite/00_02_s1003.md), turno 3, ELIA: Lickitung Iron Defense mentre Annihilape lo manda KO (danno minimo stimato)
- [00_02_s2003](partite/00_02_s2003.md), turno 5, ELIA: Lickitung Iron Defense mentre Annihilape lo manda KO (danno minimo stimato)
- [00_02_s3](partite/00_02_s3.md), turno 5, ELIA: Lickitung Iron Defense mentre Annihilape lo manda KO (danno minimo stimato)
- [01_08_s2021](partite/01_08_s2021.md), turno 5, GEROZZ: Great Tusk Bulk Up mentre Aggron lo manda KO (danno minimo stimato)
- [01_08_s2021](partite/01_08_s2021.md), turno 6, GEROZZ: Great Tusk Bulk Up mentre Aggron lo manda KO (danno minimo stimato)

### Bersaglio gia KO senza redirect utile (13)
- [00_00_s1](partite/00_00_s1.md), turno 3, ENZO: Rillaboom High Horsepower su Gengar che il compagno manda KO prima; nessun redirect utile
- [00_06_s1007](partite/00_06_s1007.md), turno 5, ENZO: Annihilape Close Combat su Armaldo che il compagno manda KO prima; nessun redirect utile
- [01_11_s24](partite/01_11_s24.md), turno 4, SIMONE: Aggron Body Press su Incineroar che il compagno manda KO prima; nessun redirect utile
- [03_06_s43](partite/03_06_s43.md), turno 8, RAFEL: Armarouge Aura Sphere su Kecleon che il compagno manda KO prima; nessun redirect utile
- [05_07_s2068](partite/05_07_s2068.md), turno 4, MARIO: Hariyama Close Combat su Tatsugiri che il compagno manda KO prima; nessun redirect utile

### Protect consecutivo (1)
- [05_09_s1070](partite/05_09_s1070.md), turno 10, CARLETTO: Pelipper Protect dopo Protect al turno prima

### Fake Out dopo il primo turno (1)
- [06_05_s2078](partite/06_05_s2078.md), turno 11, TATORE: Kecleon Fake Out non al primo turno in campo (fallisce)

### Cambi a ripetizione (1)
- [10_11_s132](partite/10_11_s132.md), turno 4, SAVAROTTO: Whimsicott rientra Chandelure uscito 1 turni fa


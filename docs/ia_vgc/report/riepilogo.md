# Self-play VGC: riepilogo

Aggiornato: 2026-10-27 11:23:22. Partite lette: 432 (completate 432, con problemi 0).

Ogni partita: lato G = allenatore della riga, lato A = allenatore della colonna; entrambi IA VGC con team preview propria e informazione chiusa (ognuno vede solo cio che ha osservato dell'altro). Dettagli turno per turno in [partite/](partite/).

Turni per partita: media 7.3, min 2, max 24.
Fitness (gancio M9, misplay certi per partita x -0.5): -0.588

## Misplay per categoria

| Categoria | Misplay certi | Casi dubbi (revisione) |
|---|---:|---:|
| Mossa su un immune noto | 7 | 3 |
| Bersaglio gia KO senza redirect utile | 2 | 0 |
| KO garantito non preso | 83 | 0 |
| KO non preso: lettura di un Protect visto | 0 | 315 |
| KO non preso: scelta una mossa ad area | 0 | 50 |
| KO non preso: scelto supporto/Protect/cambio | 0 | 742 |
| Attacco sul compagno senza combo | 2 | 0 |
| Protect consecutivo | 18 | 0 |
| Fake Out dopo il primo turno | 3 | 0 |
| Cambio su chi va KO all'entrata | 100 | 0 |
| Cambi a ripetizione | 40 | 0 |
| Turni di Tailwind/Trick Room sprecati | 48 | 80 |
| Setup con KO in arrivo | 47 | 17 |
| Win condition persa evitabilmente | 152 | 137 |
| Team preview: abilita penalizzante senza chi la toglie | 6 | 0 |
| Team preview: lead che si danneggia da sola | 0 | 78 |
| **Totale** | **508** | **1422** |

## Risultati (riga = lato G, colonna = lato A)

V = vince G, S = vince A, P = pareggio, L = limite turni, ! = blocco/crash, . = non giocata

| G \ A | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 ENZO | [S](partite/00_00_s1.md)[V](partite/00_00_s1001.md)[S](partite/00_00_s2001.md) | [S](partite/00_01_s1002.md)[S](partite/00_01_s2.md)[S](partite/00_01_s2002.md) | [V](partite/00_02_s1003.md)[V](partite/00_02_s2003.md)[V](partite/00_02_s3.md) | [S](partite/00_03_s1004.md)[S](partite/00_03_s2004.md)[S](partite/00_03_s4.md) | [V](partite/00_04_s1005.md)[V](partite/00_04_s2005.md)[V](partite/00_04_s5.md) | [V](partite/00_05_s1006.md)[S](partite/00_05_s2006.md)[V](partite/00_05_s6.md) | [V](partite/00_06_s1007.md)[V](partite/00_06_s2007.md)[V](partite/00_06_s7.md) | [S](partite/00_07_s1008.md)[S](partite/00_07_s2008.md)[S](partite/00_07_s8.md) | [V](partite/00_08_s1009.md)[S](partite/00_08_s2009.md)[V](partite/00_08_s9.md) | [V](partite/00_09_s10.md)[S](partite/00_09_s1010.md)[V](partite/00_09_s2010.md) | [V](partite/00_10_s1011.md)[V](partite/00_10_s11.md)[V](partite/00_10_s2011.md) | [V](partite/00_11_s1012.md)[V](partite/00_11_s12.md)[S](partite/00_11_s2012.md) |
| 1 SIMONE | [V](partite/01_00_s1013.md)[V](partite/01_00_s13.md)[V](partite/01_00_s2013.md) | [V](partite/01_01_s1014.md)[S](partite/01_01_s14.md)[S](partite/01_01_s2014.md) | [V](partite/01_02_s1015.md)[V](partite/01_02_s15.md)[V](partite/01_02_s2015.md) | [V](partite/01_03_s1016.md)[V](partite/01_03_s16.md)[V](partite/01_03_s2016.md) | [S](partite/01_04_s1017.md)[S](partite/01_04_s17.md)[S](partite/01_04_s2017.md) | [V](partite/01_05_s1018.md)[V](partite/01_05_s18.md)[V](partite/01_05_s2018.md) | [V](partite/01_06_s1019.md)[V](partite/01_06_s19.md)[V](partite/01_06_s2019.md) | [S](partite/01_07_s1020.md)[S](partite/01_07_s20.md)[S](partite/01_07_s2020.md) | [S](partite/01_08_s1021.md)[S](partite/01_08_s2021.md)[S](partite/01_08_s21.md) | [S](partite/01_09_s1022.md)[V](partite/01_09_s2022.md)[S](partite/01_09_s22.md) | [S](partite/01_10_s1023.md)[S](partite/01_10_s2023.md)[S](partite/01_10_s23.md) | [S](partite/01_11_s1024.md)[S](partite/01_11_s2024.md)[S](partite/01_11_s24.md) |
| 2 ELIA | [V](partite/02_00_s1025.md)[S](partite/02_00_s2025.md)[S](partite/02_00_s25.md) | [S](partite/02_01_s1026.md)[S](partite/02_01_s2026.md)[S](partite/02_01_s26.md) | [S](partite/02_02_s1027.md)[S](partite/02_02_s2027.md)[S](partite/02_02_s27.md) | [S](partite/02_03_s1028.md)[S](partite/02_03_s2028.md)[S](partite/02_03_s28.md) | [S](partite/02_04_s1029.md)[S](partite/02_04_s2029.md)[S](partite/02_04_s29.md) | [V](partite/02_05_s1030.md)[V](partite/02_05_s2030.md)[S](partite/02_05_s30.md) | [S](partite/02_06_s1031.md)[S](partite/02_06_s2031.md)[S](partite/02_06_s31.md) | [V](partite/02_07_s1032.md)[S](partite/02_07_s2032.md)[S](partite/02_07_s32.md) | [S](partite/02_08_s1033.md)[V](partite/02_08_s2033.md)[S](partite/02_08_s33.md) | [S](partite/02_09_s1034.md)[S](partite/02_09_s2034.md)[S](partite/02_09_s34.md) | [S](partite/02_10_s1035.md)[S](partite/02_10_s2035.md)[S](partite/02_10_s35.md) | [V](partite/02_11_s1036.md)[S](partite/02_11_s2036.md)[S](partite/02_11_s36.md) |
| 3 RAFEL | [V](partite/03_00_s1037.md)[V](partite/03_00_s2037.md)[V](partite/03_00_s37.md) | [S](partite/03_01_s1038.md)[S](partite/03_01_s2038.md)[V](partite/03_01_s38.md) | [V](partite/03_02_s1039.md)[V](partite/03_02_s2039.md)[S](partite/03_02_s39.md) | [S](partite/03_03_s1040.md)[S](partite/03_03_s2040.md)[S](partite/03_03_s40.md) | [S](partite/03_04_s1041.md)[S](partite/03_04_s2041.md)[S](partite/03_04_s41.md) | [V](partite/03_05_s1042.md)[S](partite/03_05_s2042.md)[S](partite/03_05_s42.md) | [V](partite/03_06_s1043.md)[V](partite/03_06_s2043.md)[V](partite/03_06_s43.md) | [S](partite/03_07_s1044.md)[S](partite/03_07_s2044.md)[V](partite/03_07_s44.md) | [S](partite/03_08_s1045.md)[S](partite/03_08_s2045.md)[S](partite/03_08_s45.md) | [S](partite/03_09_s1046.md)[S](partite/03_09_s2046.md)[S](partite/03_09_s46.md) | [S](partite/03_10_s1047.md)[S](partite/03_10_s2047.md)[S](partite/03_10_s47.md) | [S](partite/03_11_s1048.md)[V](partite/03_11_s2048.md)[S](partite/03_11_s48.md) |
| 4 TONY | [S](partite/04_00_s1049.md)[S](partite/04_00_s2049.md)[S](partite/04_00_s49.md) | [V](partite/04_01_s1050.md)[V](partite/04_01_s2050.md)[V](partite/04_01_s50.md) | [V](partite/04_02_s1051.md)[V](partite/04_02_s2051.md)[S](partite/04_02_s51.md) | [V](partite/04_03_s1052.md)[V](partite/04_03_s2052.md)[V](partite/04_03_s52.md) | [V](partite/04_04_s1053.md)[S](partite/04_04_s2053.md)[V](partite/04_04_s53.md) | [V](partite/04_05_s1054.md)[V](partite/04_05_s2054.md)[S](partite/04_05_s54.md) | [V](partite/04_06_s1055.md)[V](partite/04_06_s2055.md)[V](partite/04_06_s55.md) | [V](partite/04_07_s1056.md)[S](partite/04_07_s2056.md)[V](partite/04_07_s56.md) | [V](partite/04_08_s1057.md)[V](partite/04_08_s2057.md)[V](partite/04_08_s57.md) | [S](partite/04_09_s1058.md)[S](partite/04_09_s2058.md)[V](partite/04_09_s58.md) | [V](partite/04_10_s1059.md)[V](partite/04_10_s2059.md)[S](partite/04_10_s59.md) | [V](partite/04_11_s1060.md)[V](partite/04_11_s2060.md)[S](partite/04_11_s60.md) |
| 5 MARIO LETT | [S](partite/05_00_s1061.md)[S](partite/05_00_s2061.md)[S](partite/05_00_s61.md) | [S](partite/05_01_s1062.md)[S](partite/05_01_s2062.md)[S](partite/05_01_s62.md) | [S](partite/05_02_s1063.md)[S](partite/05_02_s2063.md)[S](partite/05_02_s63.md) | [S](partite/05_03_s1064.md)[V](partite/05_03_s2064.md)[S](partite/05_03_s64.md) | [V](partite/05_04_s1065.md)[S](partite/05_04_s2065.md)[V](partite/05_04_s65.md) | [S](partite/05_05_s1066.md)[S](partite/05_05_s2066.md)[V](partite/05_05_s66.md) | [V](partite/05_06_s1067.md)[S](partite/05_06_s2067.md)[V](partite/05_06_s67.md) | [V](partite/05_07_s1068.md)[S](partite/05_07_s2068.md)[S](partite/05_07_s68.md) | [S](partite/05_08_s1069.md)[V](partite/05_08_s2069.md)[S](partite/05_08_s69.md) | [V](partite/05_09_s1070.md)[V](partite/05_09_s2070.md)[V](partite/05_09_s70.md) | [V](partite/05_10_s1071.md)[V](partite/05_10_s2071.md)[V](partite/05_10_s71.md) | [V](partite/05_11_s1072.md)[V](partite/05_11_s2072.md)[V](partite/05_11_s72.md) |
| 6 TATORE | [S](partite/06_00_s1073.md)[S](partite/06_00_s2073.md)[V](partite/06_00_s73.md) | [S](partite/06_01_s1074.md)[S](partite/06_01_s2074.md)[S](partite/06_01_s74.md) | [V](partite/06_02_s1075.md)[V](partite/06_02_s2075.md)[V](partite/06_02_s75.md) | [S](partite/06_03_s1076.md)[V](partite/06_03_s2076.md)[S](partite/06_03_s76.md) | [S](partite/06_04_s1077.md)[V](partite/06_04_s2077.md)[V](partite/06_04_s77.md) | [V](partite/06_05_s1078.md)[S](partite/06_05_s2078.md)[S](partite/06_05_s78.md) | [S](partite/06_06_s1079.md)[S](partite/06_06_s2079.md)[V](partite/06_06_s79.md) | [S](partite/06_07_s1080.md)[S](partite/06_07_s2080.md)[V](partite/06_07_s80.md) | [V](partite/06_08_s1081.md)[S](partite/06_08_s2081.md)[S](partite/06_08_s81.md) | [S](partite/06_09_s1082.md)[S](partite/06_09_s2082.md)[S](partite/06_09_s82.md) | [S](partite/06_10_s1083.md)[V](partite/06_10_s2083.md)[V](partite/06_10_s83.md) | [S](partite/06_11_s1084.md)[V](partite/06_11_s2084.md)[V](partite/06_11_s84.md) |
| 7 MARIO | [V](partite/07_00_s1085.md)[S](partite/07_00_s2085.md)[V](partite/07_00_s85.md) | [V](partite/07_01_s1086.md)[V](partite/07_01_s2086.md)[V](partite/07_01_s86.md) | [V](partite/07_02_s1087.md)[V](partite/07_02_s2087.md)[V](partite/07_02_s87.md) | [S](partite/07_03_s1088.md)[V](partite/07_03_s2088.md)[S](partite/07_03_s88.md) | [S](partite/07_04_s1089.md)[V](partite/07_04_s2089.md)[V](partite/07_04_s89.md) | [V](partite/07_05_s1090.md)[S](partite/07_05_s2090.md)[V](partite/07_05_s90.md) | [V](partite/07_06_s1091.md)[V](partite/07_06_s2091.md)[V](partite/07_06_s91.md) | [V](partite/07_07_s1092.md)[S](partite/07_07_s2092.md)[V](partite/07_07_s92.md) | [S](partite/07_08_s1093.md)[S](partite/07_08_s2093.md)[S](partite/07_08_s93.md) | [S](partite/07_09_s1094.md)[S](partite/07_09_s2094.md)[S](partite/07_09_s94.md) | [V](partite/07_10_s1095.md)[S](partite/07_10_s2095.md)[S](partite/07_10_s95.md) | [V](partite/07_11_s1096.md)[V](partite/07_11_s2096.md)[S](partite/07_11_s96.md) |
| 8 GEROZZ | [V](partite/08_00_s1097.md)[S](partite/08_00_s2097.md)[S](partite/08_00_s97.md) | [V](partite/08_01_s1098.md)[V](partite/08_01_s2098.md)[V](partite/08_01_s98.md) | [V](partite/08_02_s1099.md)[V](partite/08_02_s2099.md)[V](partite/08_02_s99.md) | [V](partite/08_03_s100.md)[V](partite/08_03_s1100.md)[V](partite/08_03_s2100.md) | [S](partite/08_04_s101.md)[S](partite/08_04_s1101.md)[S](partite/08_04_s2101.md) | [V](partite/08_05_s102.md)[V](partite/08_05_s1102.md)[V](partite/08_05_s2102.md) | [V](partite/08_06_s103.md)[V](partite/08_06_s1103.md)[V](partite/08_06_s2103.md) | [V](partite/08_07_s104.md)[S](partite/08_07_s1104.md)[V](partite/08_07_s2104.md) | [S](partite/08_08_s105.md)[V](partite/08_08_s1105.md)[V](partite/08_08_s2105.md) | [V](partite/08_09_s106.md)[S](partite/08_09_s1106.md)[V](partite/08_09_s2106.md) | [V](partite/08_10_s107.md)[V](partite/08_10_s1107.md)[V](partite/08_10_s2107.md) | [V](partite/08_11_s108.md)[S](partite/08_11_s1108.md)[V](partite/08_11_s2108.md) |
| 9 CARLETTO | [V](partite/09_00_s109.md)[V](partite/09_00_s1109.md)[S](partite/09_00_s2109.md) | [V](partite/09_01_s110.md)[V](partite/09_01_s1110.md)[S](partite/09_01_s2110.md) | [S](partite/09_02_s111.md)[V](partite/09_02_s1111.md)[S](partite/09_02_s2111.md) | [V](partite/09_03_s1112.md)[V](partite/09_03_s112.md)[V](partite/09_03_s2112.md) | [V](partite/09_04_s1113.md)[S](partite/09_04_s113.md)[V](partite/09_04_s2113.md) | [S](partite/09_05_s1114.md)[S](partite/09_05_s114.md)[V](partite/09_05_s2114.md) | [V](partite/09_06_s1115.md)[V](partite/09_06_s115.md)[V](partite/09_06_s2115.md) | [V](partite/09_07_s1116.md)[V](partite/09_07_s116.md)[S](partite/09_07_s2116.md) | [S](partite/09_08_s1117.md)[S](partite/09_08_s117.md)[S](partite/09_08_s2117.md) | [S](partite/09_09_s1118.md)[V](partite/09_09_s118.md)[S](partite/09_09_s2118.md) | [S](partite/09_10_s1119.md)[S](partite/09_10_s119.md)[S](partite/09_10_s2119.md) | [V](partite/09_11_s1120.md)[V](partite/09_11_s120.md)[V](partite/09_11_s2120.md) |
| 10 GIGI PINTO | [V](partite/10_00_s1121.md)[S](partite/10_00_s121.md)[S](partite/10_00_s2121.md) | [V](partite/10_01_s1122.md)[V](partite/10_01_s122.md)[V](partite/10_01_s2122.md) | [V](partite/10_02_s1123.md)[V](partite/10_02_s123.md)[S](partite/10_02_s2123.md) | [V](partite/10_03_s1124.md)[V](partite/10_03_s124.md)[V](partite/10_03_s2124.md) | [S](partite/10_04_s1125.md)[V](partite/10_04_s125.md)[V](partite/10_04_s2125.md) | [S](partite/10_05_s1126.md)[V](partite/10_05_s126.md)[S](partite/10_05_s2126.md) | [V](partite/10_06_s1127.md)[S](partite/10_06_s127.md)[V](partite/10_06_s2127.md) | [V](partite/10_07_s1128.md)[S](partite/10_07_s128.md)[S](partite/10_07_s2128.md) | [S](partite/10_08_s1129.md)[V](partite/10_08_s129.md)[S](partite/10_08_s2129.md) | [V](partite/10_09_s1130.md)[V](partite/10_09_s130.md)[V](partite/10_09_s2130.md) | [S](partite/10_10_s1131.md)[V](partite/10_10_s131.md)[V](partite/10_10_s2131.md) | [V](partite/10_11_s1132.md)[V](partite/10_11_s132.md)[V](partite/10_11_s2132.md) |
| 11 SAVAROTTO | [S](partite/11_00_s1133.md)[V](partite/11_00_s133.md)[V](partite/11_00_s2133.md) | [V](partite/11_01_s1134.md)[V](partite/11_01_s134.md)[S](partite/11_01_s2134.md) | [S](partite/11_02_s1135.md)[S](partite/11_02_s135.md)[S](partite/11_02_s2135.md) | [V](partite/11_03_s1136.md)[V](partite/11_03_s136.md)[S](partite/11_03_s2136.md) | [S](partite/11_04_s1137.md)[S](partite/11_04_s137.md)[V](partite/11_04_s2137.md) | [S](partite/11_05_s1138.md)[S](partite/11_05_s138.md)[V](partite/11_05_s2138.md) | [S](partite/11_06_s1139.md)[V](partite/11_06_s139.md)[S](partite/11_06_s2139.md) | [V](partite/11_07_s1140.md)[S](partite/11_07_s140.md)[V](partite/11_07_s2140.md) | [S](partite/11_08_s1141.md)[S](partite/11_08_s141.md)[S](partite/11_08_s2141.md) | [S](partite/11_09_s1142.md)[S](partite/11_09_s142.md)[S](partite/11_09_s2142.md) | [S](partite/11_10_s1143.md)[S](partite/11_10_s143.md)[V](partite/11_10_s2143.md) | [V](partite/11_11_s1144.md)[V](partite/11_11_s144.md)[S](partite/11_11_s2144.md) |

## Allenatori

| # | Allenatore | Vittorie / partite | Misplay certi (propri) |
|---:|---|---|---:|
| 0 | ENZO | 40 / 72 | 61 |
| 1 | SIMONE | 35 / 72 | 34 |
| 2 | ELIA | 20 / 72 | 28 |
| 3 | RAFEL | 28 / 72 | 69 |
| 4 | TONY | 45 / 72 | 48 |
| 5 | MARIO LETT | 33 / 72 | 28 |
| 6 | TATORE | 24 / 72 | 52 |
| 7 | MARIO | 42 / 72 | 39 |
| 8 | GEROZZ | 52 / 72 | 37 |
| 9 | CARLETTO | 43 / 72 | 20 |
| 10 | GIGI PINTO | 43 / 72 | 32 |
| 11 | SAVAROTTO | 27 / 72 | 60 |

## Esempi di misplay

### Win condition persa evitabilmente (152)
- [00_00_s1](partite/00_00_s1.md), turno 4, ENZO: Gengar KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_00_s1001](partite/00_00_s1001.md), turno 4, ENZO: Gengar KO senza Protect/cambio (Protect disponibile, cambio possibile): il KO era prevedibile
- [00_00_s1001](partite/00_00_s1001.md), turno 4, ENZO: Annihilape KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_00_s2001](partite/00_00_s2001.md), turno 4, ENZO: Annihilape KO senza Protect/cambio (cambio possibile): il KO era prevedibile
- [00_00_s2001](partite/00_00_s2001.md), turno 5, ENZO: Gengar KO senza Protect/cambio (Protect disponibile, cambio possibile): il KO era prevedibile

### Cambio su chi va KO all'entrata (100)
- [00_01_s2](partite/00_01_s2.md), turno 3, ENZO: Gengar entrato con un cambio e KO nello stesso turno
- [00_02_s3](partite/00_02_s3.md), turno 6, ENZO: Aerodactyl entrato con un cambio e KO nello stesso turno
- [00_03_s1004](partite/00_03_s1004.md), turno 2, ENZO: Annihilape entrato con un cambio e KO nello stesso turno
- [00_07_s1008](partite/00_07_s1008.md), turno 4, MARIO: Politoed entrato con un cambio e KO nello stesso turno
- [00_08_s1009](partite/00_08_s1009.md), turno 2, GEROZZ: Rotom entrato con un cambio e KO nello stesso turno

### KO garantito non preso (83)
- [00_02_s1003](partite/00_02_s1003.md), turno 3, ENZO: Rillaboom Wood Hammer avrebbe mandato KO Banette, scelto Wood Hammer
- [00_09_s10](partite/00_09_s10.md), turno 6, ENZO: Rillaboom Grassy Glide avrebbe mandato KO Malamar, scelto Wood Hammer
- [01_01_s1014](partite/01_01_s1014.md), turno 3, SIMONE: Arcanine Flare Blitz avrebbe mandato KO Toedscruel, scelto Extreme Speed
- [01_03_s16](partite/01_03_s16.md), turno 4, SIMONE: Machamp Close Combat avrebbe mandato KO Copperajah, scelto Knock Off
- [01_03_s16](partite/01_03_s16.md), turno 6, SIMONE: Aggron Body Press avrebbe mandato KO Copperajah, scelto Heavy Slam

### Turni di Tailwind/Trick Room sprecati (48)
- [02_01_s1026](partite/02_01_s1026.md), turno 5, SIMONE: Toedscruel Trick Room con Trick Room attivo (3 turni): lo chiude
- [02_01_s1026](partite/02_01_s1026.md), turno 6, SIMONE: Toedscruel Trick Room con Trick Room attivo (2 turni): lo chiude
- [02_05_s1030](partite/02_05_s1030.md), turno 4, ELIA: Mimikyu Trick Room con Trick Room attivo (2 turni): lo chiude
- [02_06_s31](partite/02_06_s31.md), turno 5, TATORE: Scream Tail Trick Room con Trick Room attivo (4 turni): lo chiude
- [02_07_s1032](partite/02_07_s1032.md), turno 2, ELIA: Mimikyu Trick Room con Trick Room attivo (4 turni): lo chiude

### Setup con KO in arrivo (47)
- [00_02_s3](partite/00_02_s3.md), turno 5, ELIA: Lickitung Iron Defense mentre Annihilape lo manda KO (danno minimo stimato)
- [00_02_s3](partite/00_02_s3.md), turno 8, ELIA: Lickitung Iron Defense mentre Gengar lo manda KO (danno minimo stimato)
- [00_08_s2009](partite/00_08_s2009.md), turno 7, GEROZZ: Gholdengo Nasty Plot mentre Rillaboom lo manda KO (danno minimo stimato)
- [00_11_s12](partite/00_11_s12.md), turno 4, SAVAROTTO: Espathra Calm Mind mentre Aerodactyl lo manda KO (danno minimo stimato)
- [01_03_s1016](partite/01_03_s1016.md), turno 6, RAFEL: Copperajah Iron Defense mentre Toedscruel lo manda KO (danno minimo stimato)

### Cambi a ripetizione (40)
- [01_09_s1022](partite/01_09_s1022.md), turno 5, CARLETTO: Pelipper rientra Snorlax uscito 2 turni fa
- [02_04_s29](partite/02_04_s29.md), turno 7, TONY: Brute Bonnet rientra Roaring Moon uscito 2 turni fa
- [02_09_s2034](partite/02_09_s2034.md), turno 4, ELIA: Miltank rientra Mimikyu uscito 2 turni fa
- [02_09_s2034](partite/02_09_s2034.md), turno 6, ELIA: Mimikyu rientra Miltank uscito 2 turni fa
- [02_10_s35](partite/02_10_s35.md), turno 4, GIGI PINTO: Garganacl rientra Torkoal uscito 2 turni fa

### Protect consecutivo (18)
- [00_07_s2008](partite/00_07_s2008.md), turno 6, MARIO: Hariyama Protect dopo Protect al turno prima
- [00_10_s2011](partite/00_10_s2011.md), turno 7, GIGI PINTO: Torkoal Protect dopo Protect al turno prima
- [02_05_s1030](partite/02_05_s1030.md), turno 7, MARIO LETT: Weezing Protect dopo Protect al turno prima
- [03_11_s2048](partite/03_11_s2048.md), turno 6, SAVAROTTO: Chandelure Protect dopo Protect al turno prima
- [05_02_s63](partite/05_02_s63.md), turno 6, MARIO LETT: Slaking Protect dopo Protect al turno prima

### Mossa su un immune noto (7)
- [00_02_s1003](partite/00_02_s1003.md), turno 3, ENZO: Annihilape Close Combat su Banette (immune)
- [00_02_s1003](partite/00_02_s1003.md), turno 4, ENZO: Annihilape Close Combat su Mimikyu (immune)
- [02_00_s25](partite/02_00_s25.md), turno 8, ENZO: Annihilape Rage Fist su Lickitung (immune)
- [02_00_s25](partite/02_00_s25.md), turno 9, ENZO: Annihilape Rage Fist su Lickitung (immune)
- [02_00_s25](partite/02_00_s25.md), turno 10, ENZO: Annihilape Rage Fist su Lickitung (immune)

### Team preview: abilita penalizzante senza chi la toglie (6)
- [02_05_s1030](partite/02_05_s1030.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga
- [02_05_s2030](partite/02_05_s2030.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga
- [02_05_s30](partite/02_05_s30.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga
- [05_02_s1063](partite/05_02_s1063.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga
- [05_02_s2063](partite/05_02_s2063.md), team preview, MARIO LETT:  Slaking: Truant e nessuno che la tolga

### Fake Out dopo il primo turno (3)
- [06_11_s1084](partite/06_11_s1084.md), turno 4, SAVAROTTO: Incineroar Fake Out non al primo turno in campo (fallisce)
- [11_04_s137](partite/11_04_s137.md), turno 4, SAVAROTTO: Incineroar Fake Out non al primo turno in campo (fallisce)
- [11_09_s1142](partite/11_09_s1142.md), turno 5, SAVAROTTO: Incineroar Fake Out non al primo turno in campo (fallisce)

### Bersaglio gia KO senza redirect utile (2)
- [00_06_s7](partite/00_06_s7.md), turno 3, ENZO: Annihilape Close Combat su Kecleon che il compagno manda KO prima; nessun redirect utile
- [02_02_s27](partite/02_02_s27.md), turno 4, ELIA: Lickitung Body Press su Goodra che il compagno manda KO prima; nessun redirect utile

### Attacco sul compagno senza combo (2)
- [09_11_s2120](partite/09_11_s2120.md), turno 5, SAVAROTTO: Incineroar Darkest Lariat sul compagno Chandelure senza combo
- [11_09_s142](partite/11_09_s142.md), turno 5, SAVAROTTO: Incineroar Flare Blitz sul compagno Chandelure senza combo


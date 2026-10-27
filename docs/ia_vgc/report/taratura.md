# Taratura automatica dei pesi (SPSA)

Aggiornato: 2026-10-27 15:50:34. Passi: 20/20 (fase: fine, finita).

Disegno: per ogni passo, pesi theta+c*delta e theta-c*delta (delta = +-1 per peso) giocati sugli stessi 2 semi fissi (base 7) per tutti gli scontri (all), ognuno a specchio (pesi in prova sul lato G e poi sul lato A, l'altro lato ai pesi di default). Obiettivo: tasso di vittoria dei pesi in prova - 0.01 x misplay certi per partita. c=20.0, a0=12000.0, A=3.0.

| Peso | Valore attuale (%) | Candidato (media seconda metà) |
|---|---:|---:|
| KO | 92 | 94 |
| VIVO | 76 | 90 |
| VELOCITA | 92 | 86 |
| MINACCE | 134 | 122 |
| MATERIALE | 71 | 76 |
| CAMBIO | 108 | 116 |
| WINCON | 132 | 126 |

| Passo | f(+) | f(-) | Vittorie + / - | Pesi dopo il passo |
|---:|---:|---:|---|---|
| 1 | 0.5140 | 0.5103 | 0.5191 / 0.5156 | 101, 99, 101, 101, 99, 101, 99 |
| 2 | 0.5333 | 0.4999 | 0.5382 / 0.5052 | 92, 90, 92, 110, 90, 110, 108 |
| 3 | 0.5129 | 0.5199 | 0.5182 / 0.5252 | 90, 92, 94, 112, 88, 108, 110 |
| 4 | 0.5191 | 0.4995 | 0.5243 / 0.5044 | 95, 96, 89, 117, 92, 113, 115 |
| 5 | 0.5084 | 0.5183 | 0.5139 / 0.5234 | 92, 99, 91, 119, 90, 116, 117 |
| 6 | 0.4985 | 0.5009 | 0.5043 / 0.5061 | 92, 98, 91, 119, 90, 116, 116 |
| 7 | 0.5269 | 0.5192 | 0.5321 / 0.5243 | 94, 97, 89, 120, 88, 115, 118 |
| 8 | 0.5244 | 0.5127 | 0.5295 / 0.5182 | 91, 94, 92, 118, 86, 112, 116 |
| 9 | 0.5387 | 0.5538 | 0.5443 / 0.559 | 88, 91, 95, 121, 88, 115, 119 |
| 10 | 0.5199 | 0.5346 | 0.5252 / 0.5399 | 91, 94, 92, 123, 86, 118, 121 |
| 11 | 0.4747 | 0.5050 | 0.48 / 0.5104 | 97, 99, 86, 118, 80, 123, 127 |
| 12 | 0.5215 | 0.5340 | 0.5269 / 0.5391 | 94, 97, 89, 116, 83, 121, 125 |
| 13 | 0.5206 | 0.5062 | 0.526 / 0.5113 | 97, 94, 86, 118, 80, 118, 127 |
| 14 | 0.5086 | 0.5235 | 0.5139 / 0.5287 | 94, 92, 88, 121, 78, 121, 125 |
| 15 | 0.5144 | 0.5276 | 0.52 / 0.533 | 96, 94, 86, 123, 76, 119, 127 |
| 16 | 0.5166 | 0.5376 | 0.5217 / 0.5434 | 100, 91, 83, 120, 72, 116, 124 |
| 17 | 0.4964 | 0.5159 | 0.5017 / 0.5208 | 97, 88, 80, 122, 75, 113, 121 |
| 18 | 0.5038 | 0.5338 | 0.5087 / 0.5391 | 92, 83, 84, 127, 71, 108, 125 |
| 19 | 0.4893 | 0.5130 | 0.4948 / 0.5182 | 89, 80, 88, 130, 68, 112, 128 |
| 20 | 0.4869 | 0.5123 | 0.4922 / 0.5174 | 92, 76, 92, 134, 71, 108, 132 |

## Validazione su semi mai visti

Candidato [94, 90, 86, 122, 76, 116, 126] contro i pesi di default, 8 semi (base 500001), a specchio: 1152 coppie (2304 partite valide, 0 non valide).
Tasso di vittoria del candidato: **0.503** (IC 95% [0.4877, 0.5184], z=0.39, p=0.69813); misplay certi per partita del candidato: 0.546.
Miglioramento significativo (IC sopra il 50%): **no**.

Per applicare i pesi: moltiplicare i `CUSTOM_VGC_EVAL_*` corrispondenti in `include/config/custom.h` per il valore / 100 (WINCON: 100 + (150-100) x valore / 100).

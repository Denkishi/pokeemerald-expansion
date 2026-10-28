# Taratura automatica dei pesi (SPSA)

Aggiornato: 2026-10-28 21:46:46. Passi: 2/20 (fase: spsa, in corso).

Disegno: per ogni passo, pesi theta+c*delta e theta-c*delta (delta = +-1 per peso) giocati sugli stessi 2 semi fissi (base 7) per tutti gli scontri (all), ognuno a specchio (pesi in prova sul lato G e poi sul lato A, l'altro lato ai pesi di default). Obiettivo: tasso di vittoria dei pesi in prova - 0.01 x misplay certi per partita. c=20.0, a0=12000.0, A=3.0.

| Peso | Valore attuale (%) | Candidato (media seconda metà) |
|---|---:|---:|
| KO | 97 | 96 |
| VIVO | 90 | 90 |
| VELOCITA | 97 | 96 |
| MINACCE | 110 | 110 |
| MATERIALE | 90 | 90 |
| CAMBIO | 110 | 110 |
| WINCON | 103 | 104 |

| Passo | f(+) | f(-) | Vittorie + / - | Pesi dopo il passo |
|---:|---:|---:|---|---|
| 1 | 0.5072 | 0.4962 | 0.5104 / 0.4991 | 103, 97, 103, 103, 97, 103, 97 |
| 2 | 0.5025 | 0.4794 | 0.5061 / 0.4826 | 97, 90, 97, 110, 90, 110, 103 |

Per applicare i pesi: moltiplicare i `CUSTOM_VGC_EVAL_*` corrispondenti in `include/config/custom.h` per il valore / 100 (WINCON: 100 + (150-100) x valore / 100).

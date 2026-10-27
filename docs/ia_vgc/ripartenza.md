# IA VGC — da dove ripartire (27/10, 20:00)

Stato dei branch (nessun push, `master` intatto):

| Branch | Worktree | Stato | Da fare |
|---|---|---|---|
| `ia-vgc` (6380f8602c) | `pokeemerald-expansion` | Base stabile: M9 unito, 61 PASS / 8 KNOWN_FAILING, nessun tetto al costo | Velocità incerte a 2 rami (Dugtrio), correzioni mirate per gli 8 KNOWN_FAILING, rigenerare il libro dei piani (`esplora.bat` → `libro.bat` → `compila.bat` → `verifica_libro.bat`; esplorazione fermata a metà) |
| `ia-vgc-contesto` (522f124662) | `pokeemerald-expansion-contesto` | Contesto squadra dalla team preview (archetipi, strumenti, abilità, set, lead, minacce). Verifica a specchio 864 partite: 51,0%, IC [48,4; 53,6], p=0,45 (non peggiora, miglioramento non dimostrato) | Test dedicati, più semi di verifica, poi merge in `ia-vgc` |
| `ia-vgc-regole` (443205be3f, WIP) | `pokeemerald-expansion-regole` | Regole di `battle_ai_vgc.c` ridotte a filtro di candidate (in corso, non verificato) | Compilare, test VGC, campagna 432, merge |
| `ia-vgc-bug` (75cb5fe4e5, WIP) | `pokeemerald-expansion-bug` | Correzione dell'assert di Parting Shot (`battle_move_resolution.c`) + test `parting_shot.c` (non verificato) | Eseguire il test, schermo blu (stack, overlay "penso..."), merge |

## Prova in gioco dell'utente (27/10 sera, ROM di `ia-vgc` 7a2f98aacc) — PRIORITÀ

1. Amoonguss: Spore su Dugtrio invece che su Tyranitar Mega (la minaccia evidente). Il valore del sonno deve crescere con la minaccia del bersaglio (danno/KO che farebbe, Mega).
2. Overlay "penso...": il ritratto dell'allenatore compare al centro ma è tutto nero (palette non caricata) e il testo "penso..." non appare. Solo nel gioco vero (in TESTING l'overlay è saltato): `src/battle_controller_opponent.c`.
3. Incineroar (Savarotto) ha usato Fake Out sul proprio Chandelure: Fake Out su un alleato va escluso (filtro bersagli alleati: solo combo vere).
4. Messaggio di fine lotta: deve aspettare A come la sfida iniziale (oggi avanza da solo con `CUSTOM_BATTLE_AUTO_TEXT`), poi la lotta si chiude.

Non funzionano (spenti dietro flag in `custom.h`): rollout alla foglia, scala unica delle risposte, sostituzioni dopo KO nel simulatore.

Regole fisse: livello 50 ovunque; WSL solo per compilare; niente dati nascosti del giocatore; prima la decisione giusta, poi quella veloce (nessun tetto ai fotogrammi).

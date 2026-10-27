# IA VGC — da dove ripartire (27/10, 20:00)

Stato dei branch (nessun push, `master` intatto):

| Branch | Worktree | Stato | Da fare |
|---|---|---|---|
| `ia-vgc` (6380f8602c) | `pokeemerald-expansion` | Base stabile: M9 unito, 61 PASS / 8 KNOWN_FAILING, nessun tetto al costo | Velocità incerte a 2 rami (Dugtrio), correzioni mirate per gli 8 KNOWN_FAILING, rigenerare il libro dei piani (`esplora.bat` → `libro.bat` → `compila.bat` → `verifica_libro.bat`; esplorazione fermata a metà) |
| `ia-vgc-contesto` (522f124662) | `pokeemerald-expansion-contesto` | Contesto squadra dalla team preview (archetipi, strumenti, abilità, set, lead, minacce). Verifica a specchio 864 partite: 51,0%, IC [48,4; 53,6], p=0,45 (non peggiora, miglioramento non dimostrato) | Test dedicati, più semi di verifica, poi merge in `ia-vgc` |
| `ia-vgc-regole` (443205be3f, WIP) | `pokeemerald-expansion-regole` | Regole di `battle_ai_vgc.c` ridotte a filtro di candidate (in corso, non verificato) | Compilare, test VGC, campagna 432, merge |
| `ia-vgc-bug` (75cb5fe4e5, WIP) | `pokeemerald-expansion-bug` | Correzione dell'assert di Parting Shot (`battle_move_resolution.c`) + test `parting_shot.c` (non verificato) | Eseguire il test, schermo blu (stack, overlay "penso..."), merge |

Non funzionano (spenti dietro flag in `custom.h`): rollout alla foglia, scala unica delle risposte, sostituzioni dopo KO nel simulatore.

Regole fisse: livello 50 ovunque; WSL solo per compilare; niente dati nascosti del giocatore; prima la decisione giusta, poi quella veloce (nessun tetto ai fotogrammi).

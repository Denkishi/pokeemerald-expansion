# IA VGC — da dove ripartire (27/10, 20:00)

Stato dei branch (nessun push, `master` intatto):

| Branch | Worktree | Stato | Da fare |
|---|---|---|---|
| `ia-vgc` (6380f8602c) | `pokeemerald-expansion` | Base stabile: M9 unito, 61 PASS / 8 KNOWN_FAILING, nessun tetto al costo | Velocità incerte a 2 rami (Dugtrio), correzioni mirate per gli 8 KNOWN_FAILING, rigenerare il libro dei piani (`esplora.bat` → `libro.bat` → `compila.bat` → `verifica_libro.bat`; esplorazione fermata a metà) |
| `ia-vgc-contesto` (522f124662) | `pokeemerald-expansion-contesto` | Contesto squadra dalla team preview (archetipi, strumenti, abilità, set, lead, minacce). Verifica a specchio 864 partite: 51,0%, IC [48,4; 53,6], p=0,45 (non peggiora, miglioramento non dimostrato) | Test dedicati, più semi di verifica, poi merge in `ia-vgc` |
| `ia-vgc-regole` (443205be3f, WIP) | `pokeemerald-expansion-regole` | Regole di `battle_ai_vgc.c` ridotte a filtro di candidate (in corso, non verificato) | Compilare, test VGC, campagna 432, merge |
| `ia-vgc-bug` (75cb5fe4e5, WIP) | `pokeemerald-expansion-bug` | Correzione dell'assert di Parting Shot (`battle_move_resolution.c`) + test `parting_shot.c` (non verificato) | Eseguire il test, schermo blu (stack, overlay "penso..."), merge |

## Prova in gioco dell'utente (27/10 sera, ROM di `ia-vgc` 7a2f98aacc) — PRIORITÀ

1. ⚠️ Amoonguss: Spore su Dugtrio invece che su Tyranitar Mega. Fatto in parte: il sonno vale `CUSTOM_VGC_EVAL_SLEEP * (25 + minaccia) / 100` (minaccia = danno migliore in % sul nostro lato + 25 per KO possibile, solo mosse viste/probabili; stadi offensivi in più nella foglia), anche nel peso delle candidate; nelle regole +1..+4 a sonno/paralisi su chi può mandare KO uno dei nostri, è Mega o ha boost. Test `Spore goes on the bigger threat` è KNOWN_FAILING: Spore su Tyranitar -463 contro Dugtrio -424, e vince Sludge Bomb + Flare Blitz su Dugtrio (KO in due). Una scala più forte (somma sui bersagli, +25 Mega, /60) rompe Carletto/Ludicolo e Tony/Charizard. Serve una stima del danno risparmiato per turno di sonno oltre l'orizzonte.
2. ✅ Overlay "penso...": sprite nero perché `CreatePicSprite` carica la palette con `GetTrainerPicTag` ma dà allo sprite l'etichetta passata (0x7A1C mai caricata) → ora si passa `GetTrainerPicTag(pic, TRUE)`. Testo invisibile perché BG0 restava scorso sul menu mosse (`gBattle_BG0_Y = DISPLAY_HEIGHT`) → riportato a 0 come in `BtlController_HandlePrintString`. Da verificare a schermo (in TESTING l'overlay è saltato).
3. ✅ Fake Out sull'alleato: veniva da `AddComboCands` (combo "colpo debole <= 40" accanto a Flash Fire: il grafo vede Flare Blitz -> Flash Fire, ma passava Fake Out). Nuova `AI_VgcIsPartnerHitCombo` (graph): attacco sul compagno solo se attiva davvero la sua abilità/strumento, mai le mosse a primo turno; usata in `AddComboCands`, in `BuildOurFirstActions` (anche per la mossa delle regole) e nelle regole (-20 agli altri attacchi sul compagno). Test `no Fake Out on the partner`.
4. ✅ Fine lotta: con esito deciso (`gBattleOutcome`) in lotta con allenatore i messaggi aspettano A (`src/text.c`); le frasi di sconfitta/vittoria dell'allenatore finiscono con `\p`. Da verificare a schermo.

Test (test_veloci.ps1, un file alla volta: con più file in `-Tests` il filtro per nome file non trova niente): doubles 20 PASS + 1 KNOWN_FAILING, team_preview 1, scout 14, teams 29 PASS + 7 KNOWN_FAILING (Slither Wing: il giocatore porta solo i due in campo, il KO di Rillaboom chiedeva un cambio).

## Stato al 28/10, 12:00

- **Compilazione e test su GitHub** (in locale le build WSL sono lentissime col disco carico): ogni push su `ia-vgc*` compila la ROM (artifact `pokeemerald-rom`), esegue i test VGC (`test-vgc`) e produce la ROM del self-play (artifact `selfplay-rom`: `gh run download <id> -n selfplay-rom` → `tools/vgc_training/work/selfplay.gba`).
- `ia-vgc` (276fb95272): test su GitHub tutti verdi (9 KNOWN_FAILING). Dentro: slot vero delle mosse note, IA azzerata a ogni lotta (test stabili, Ludicolo passa), overlay "penso..." come terzo Pokémon, sonno valutato oltre l'orizzonte, contesto squadra, Parting Shot, velocità incerte (Dugtrio).
- Campagna 432 partite (ROM di GitHub, 28/10): G 204 / A 224, misplay certi 486 (KO garantito non preso 160, win condition 111, cambio su chi va KO 88, mossa su immune 65 = stato su immuni: Spore su Erba, Will-O-Wisp su Fuoco → li toglie il filtro di `ia-vgc-regole`).
- `ia-vgc-regole`: filtro + spinta (M8), 6 test rotti in correzione (agente al lavoro con il ciclo push → GitHub).
- Da fare dopo: merge regole, valore di Tailwind/Trick Room/schermi (7 KNOWN_FAILING), libro dei piani rigenerato con la ROM di GitHub, merge in `master` e push.

## Stato al 28/10, 13:15 (unito in master)

- In `ia-vgc` e `master`: regole M8 (filtro + spinta, fase 4 delle coppie simulate insieme, Disguise, mosse di stato su immuni escluse), After You / Yawn / Encore / Light Clay simulati, sonno oltre l'orizzonte, libro dei piani rigenerato.
- Test su GitHub: tutti verdi (8 KNOWN_FAILING con motivo: Spore, Hatterene, Aerodactyl, Grimmsnarl, Banette, Scream Tail, Lilligant passa, Altaria passa).
- Verifica regole M8 a specchio: 48,3% (p=0,22, non significativo), misplay per partita dimezzati. Libro dei piani: 52,7% (IC [50,3; 55,1], p=0,026).
- Campagna finale 432 partite: misplay certi 423 (prima 486); mossa su immune 65 → 18 (16 = Shadow Sneak di Mimikyu su Lickitung, da indagare: probabile cambio di bersaglio del motore dopo un KO).
- Aperti: KO garantito non preso 173, win condition 102, cambio su chi va KO 80; valore di un Trick Room appena messo (~1100 punti) da misurare; pesi (bassa priorità); prova in gioco di overlay e fine lotta.

## Stato al 29/10, 04:00 (master a4ab1d6dcf)

Campagna 432 partite con tutte le correzioni: **203 misplay certi** (0,47 a partita; il 28/10 mattina erano 423).
Win condition persa 90, KO garantito non preso 76, cambio su chi va KO 20, bersaglio già KO 9, setup sotto KO 7,
immune 0, Protect di fila 0. In lavoro: lato difensivo della win condition (branch `ia-vgc-cambi`).
Logger delle partite umane: `docs/ia_vgc/logger.md`.

Non funzionano (spenti dietro flag in `custom.h`): rollout alla foglia, scala unica delle risposte, sostituzioni dopo KO nel simulatore.

Regole fisse: livello 50 ovunque; WSL solo per compilare; niente dati nascosti del giocatore; prima la decisione giusta, poi quella veloce (nessun tetto ai fotogrammi).

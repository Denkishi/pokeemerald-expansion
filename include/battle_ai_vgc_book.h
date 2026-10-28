#ifndef GUARD_BATTLE_AI_VGC_BOOK_H
#define GUARD_BATTLE_AI_VGC_BOOK_H

// Libro dei piani VGC (M9): piani di gioco scoperti nel self-play (lead, retro, azioni congiunte dei turni 1 e 2)
// con vittorie e partite, generati in src/data/vgc_plan_book.h da tools/vgc_training/gen_plan_book.py.
// Chiave generica: composizione della NOSTRA squadra (hash delle specie) e caratteristiche VISIBILI della squadra
// avversaria in team preview (classe di velocità delle specie). Nessun nome di allenatore, nessun dato nascosto.
// Uso: team preview (scelta mista pesata tra i piani buoni) e turni 1/2 (bonus alle azioni del piano).

#define VGC_BOOK_ANY_CLASS 0xFF
#define VGC_BOOK_NO_PICK   0xFFFF

struct VgcBookPlan
{
    u16 teamHash;       // hash delle specie della nostra squadra (VgcBook_TeamHash)
    u8 oppClass;        // classe della squadra avversaria (VgcBook_OppClass) o VGC_BOOK_ANY_CLASS
    u8 pad;
    u16 lead[2];        // specie delle lead (come in squadra, prima di eventuali Mega)
    u16 back[2];        // specie della retro (SPECIES_NONE = libera)
    u16 t1Move[2];      // mossa consigliata al turno 1 per lead[0] / lead[1] (MOVE_NONE = nessun consiglio)
    u16 t2Move[2];      // turno 2, se la stessa lead è ancora in campo
    u16 wins, games;
};

u32 VgcBook_TeamHash(const u16 *species, u32 n);
u32 VgcBook_OppClass(const u16 *species, u32 n);

// Team preview (src/team_preview.c): a inizio scelta, con la nostra squadra e quella visibile dell'altro.
void AI_VgcBookBegin(struct Pokemon *ours, const u32 *ourSlots, u32 nOurs, struct Pokemon *theirs, const u32 *theirSlots, u32 nTheirs);
// Bo3: le lead che hanno perso una gara precedente della serie non si ripropongono dal libro.
void AI_VgcBookExcludeLeads(u16 speciesA, u16 speciesB);
// Bonus (con rumore di esplorazione nel self-play) al gruppo di 4 (maschera sugli indici di ourSlots) e alla coppia di lead.
s32 AI_VgcBookGroupBonus(struct Pokemon *ours, const u32 *ourSlots, u32 nOurs, u32 mask);
s32 AI_VgcBookLeadBonus(u16 speciesA, u16 speciesB);
// Pianificatore (turni 1 e 2): bonus alla coppia di azioni (mossa nostra, mossa del compagno) del piano scelto.
s32 AI_VgcBookPairBonus(enum BattlerId me, u32 myMove, enum BattlerId partner, u32 partnerMove);
// Esplorazione nel self-play (solo test): a volte una coppia vicina alla migliore invece della migliore.
u32 AI_VgcExplorePick(enum BattlerId me, u32 bestIdx, const s32 *ev, u32 n);
// Piano scelto in team preview per un lato (indice nel libro o VGC_BOOK_NO_PICK).
u32 AI_VgcBookPick(u32 side);
void AI_VgcBookSetPick(u32 side, u32 pick);
void AI_VgcBookReset(void);
u32 AI_VgcBookCount(void);
const struct VgcBookPlan *AI_VgcBookEntry(u32 idx);

#if TESTING
// Configurazione del self-play (test/vgc_selfplay): per lato, libro spento ed esplorazione (% di probabilità).
struct VgcTrainingConfig
{
    u8 bookOff;             // bit per lato: libro dei piani spento
    u8 exploreEps[2];       // per lato: % di team preview / turni esplorativi
    u8 previewSide;         // lato per cui sta scegliendo la team preview (B_SIDE_*)
    u8 previewExplore;      // la team preview in corso esplora
    u8 rulesLegacy;         // bit per lato: regole di prima di M8 (verifica-regole)
};
extern struct VgcTrainingConfig gVgcTraining;
#endif

#endif // GUARD_BATTLE_AI_VGC_BOOK_H

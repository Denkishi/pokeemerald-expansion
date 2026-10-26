#include "global.h"
#include "test/test.h"
#include "team_preview.h"
#include "constants/items.h"

// Team preview dell'IA VGC (src/team_preview.c): scelta dei 4 su 6 dal grafo della squadra, senza regole per
// allenatore. Vedi docs/ia_vgc/piano_implementazione.md (M7).

static void SetVgcMon(struct Pokemon *mon, enum Species species, enum Move m1, enum Move m2, enum Move m3, u16 item)
{
    enum Move moves[MAX_MON_MOVES] = { m1, m2, m3, MOVE_NONE };
    u32 i;

    CreateMon(mon, species, 50, 0, OTID_STRUCT_PLAYER_ID);
    CalculateMonStats(mon);
    for (i = 0; i < MAX_MON_MOVES; i++)
        SetMonData(mon, MON_DATA_MOVE1 + i, &moves[i]);
    SetMonData(mon, MON_DATA_HELD_ITEM, &item);
}

static bool32 OrderHas(const u8 *order, u32 n, u32 partyIdx)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (order[i] == partyIdx)
            return TRUE;
    return FALSE;
}

TEST("VGC team preview: a Baton Passer is never brought without its recipient")
{
    u8 order[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    u32 i;

    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    SetVgcMon(&gEnemyParty[0], SPECIES_ESPATHRA, MOVE_CALM_MIND, MOVE_BATON_PASS, MOVE_PROTECT, ITEM_NONE);
    SetVgcMon(&gEnemyParty[1], SPECIES_INCINEROAR, MOVE_FAKE_OUT, MOVE_FLARE_BLITZ, MOVE_PARTING_SHOT, ITEM_NONE);
    SetVgcMon(&gEnemyParty[2], SPECIES_MAGIKARP, MOVE_SPLASH, MOVE_NONE, MOVE_NONE, ITEM_NONE);
    SetVgcMon(&gEnemyParty[3], SPECIES_MAGIKARP, MOVE_SPLASH, MOVE_NONE, MOVE_NONE, ITEM_NONE);
    SetVgcMon(&gEnemyParty[4], SPECIES_MAGIKARP, MOVE_SPLASH, MOVE_NONE, MOVE_NONE, ITEM_NONE);
    SetVgcMon(&gEnemyParty[5], SPECIES_CHANDELURE, MOVE_SHADOW_BALL, MOVE_HEAT_WAVE, MOVE_ENERGY_BALL, ITEM_NONE);
    for (i = 0; i < PARTY_SIZE; i++)
        SetVgcMon(&gPlayerParty[i], SPECIES_WOBBUFFET, MOVE_TACKLE, MOVE_NONE, MOVE_NONE, ITEM_NONE);

    TeamPreview_AiSelectEnemyTeam(4, TRUE, order);

    EXPECT(OrderHas(order, 4, 5));                                   // Chandelure c'è
    EXPECT(!OrderHas(order, 4, 0) || OrderHas(order, 4, 5));         // Espathra solo con Chandelure
    EXPECT(!(order[0] == 0 && order[1] == 5) && !(order[0] == 5 && order[1] == 0)); // il ricevente non parte in lead col passatore
}

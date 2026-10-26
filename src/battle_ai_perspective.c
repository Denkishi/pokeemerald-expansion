// Prospettiva dell'IA nelle lotte IA contro IA: vedi include/battle_ai_perspective.h.
#include "global.h"
#include "battle.h"
#include "battle_ai_perspective.h"
#include "battle_controllers.h"
#include "battle_setup.h"
#include "malloc.h"
#include "team_preview.h"
#include "constants/species.h"

// Dati dell'IA per lato: [B_SIDE_OPPONENT] è il gAiLogicData allocato dalla lotta, [B_SIDE_PLAYER] esiste
// solo in IA contro IA.
static EWRAM_DATA struct AiLogicData *sAiLogicDataBySide[NUM_BATTLE_SIDES] = {0};
static EWRAM_DATA u8 sPerspectiveSide = 0; // valido solo con AI_PerspectiveActive()
static EWRAM_DATA u16 sPlayerSideMegaDesignate = SPECIES_NONE;

void AI_PerspectiveAlloc(void)
{
    sAiLogicDataBySide[B_SIDE_OPPONENT] = gAiLogicData;
    sAiLogicDataBySide[B_SIDE_PLAYER] = NULL;
    if (IsAiVsAiBattle())
        sAiLogicDataBySide[B_SIDE_PLAYER] = AllocZeroed(sizeof(struct AiLogicData));
    sPerspectiveSide = B_SIDE_OPPONENT;
}

void AI_PerspectiveFree(void)
{
    // gAiLogicData torna al buffer allocato dalla lotta, che il chiamante libera.
    if (sAiLogicDataBySide[B_SIDE_OPPONENT] != NULL)
        gAiLogicData = sAiLogicDataBySide[B_SIDE_OPPONENT];
    if (sAiLogicDataBySide[B_SIDE_PLAYER] != NULL)
        FREE_AND_SET_NULL(sAiLogicDataBySide[B_SIDE_PLAYER]);
    sAiLogicDataBySide[B_SIDE_OPPONENT] = NULL;
    sPerspectiveSide = B_SIDE_OPPONENT;
}

bool32 AI_PerspectiveActive(void)
{
    return sAiLogicDataBySide[B_SIDE_PLAYER] != NULL && IsAiVsAiBattle();
}

void AI_SetPerspectiveSide(u32 side)
{
    if (!AI_PerspectiveActive() || side >= NUM_BATTLE_SIDES)
        return;
    sPerspectiveSide = side;
    gAiLogicData = sAiLogicDataBySide[side];
}

void AI_SetPerspective(enum BattlerId battler)
{
    if (battler < MAX_BATTLERS_COUNT)
        AI_SetPerspectiveSide(GetBattlerSide(battler));
}

u32 AI_GetPerspectiveSide(void)
{
    return AI_PerspectiveActive() ? sPerspectiveSide : B_SIDE_OPPONENT;
}

bool32 AI_IsFoeOfPerspective(enum BattlerId battler)
{
    if (!AI_PerspectiveActive())
        return IsOnPlayerSide(battler);
    return GetBattlerSide(battler) != sPerspectiveSide;
}

bool32 AI_PerspectiveKnowsBattler(enum BattlerId battler)
{
    if (!AI_PerspectiveActive())
        return BattlerHasAi(battler);
    return GetBattlerSide(battler) == sPerspectiveSide;
}

u16 AI_TrainerIdOfBattler(enum BattlerId battler)
{
    if (IsAiVsAiBattle() && battler < MAX_BATTLERS_COUNT && IsOnPlayerSide(battler))
        return gPartnerTrainerId;
    return TRAINER_BATTLE_PARAM.opponentA;
}

u16 AI_GetMegaDesignateForBattler(enum BattlerId battler)
{
    if (IsAiVsAiBattle() && battler < MAX_BATTLERS_COUNT && IsOnPlayerSide(battler))
        return sPlayerSideMegaDesignate;
    return TeamPreview_GetMegaDesignateSpecies();
}

void AI_SetPlayerSideMegaDesignate(u16 species)
{
    sPlayerSideMegaDesignate = species;
}

void AI_SetLogicDataForTurnPerSide(void (*compute)(struct AiLogicData *aiData))
{
    if (!AI_PerspectiveActive())
    {
        compute(gAiLogicData);
        return;
    }
    // Ogni lato calcola i propri dati (abilità/strumenti presunti dell'altro lato, danni stimati, letture).
    // Si finisce sul lato avversario, come nella lotta normale.
    AI_SetPerspectiveSide(B_SIDE_PLAYER);
    compute(gAiLogicData);
    AI_SetPerspectiveSide(B_SIDE_OPPONENT);
    compute(gAiLogicData);
}

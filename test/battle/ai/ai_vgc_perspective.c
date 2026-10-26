#include "global.h"
#include "test/battle.h"
#include "battle_ai_util.h"
#include "battle_controllers.h"

// IA contro IA (harness di self-play VGC, test/vgc_selfplay): ogni lato vede solo ciò che ha osservato
// dell'altro (src/battle_ai_perspective.c). Prova di invarianza: cambiare una mossa NON ancora vista del
// lato avversario non deve cambiare la decisione. Prima della prospettiva per lato, in IA contro IA
// BattlerHasAi era vero per tutti e l'IA leggeva i set veri dell'altro lato.

#define VGC_AI_FLAGS (AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_SWITCHING | AI_FLAG_SMART_MON_CHOICES \
                    | AI_FLAG_KNOW_OPPONENT_PARTY | AI_FLAG_ASSUME_STAB | AI_FLAG_ASSUME_STATUS_MOVES \
                    | AI_FLAG_HP_AWARE | AI_FLAG_WEIGH_ABILITY_PREDICTION | AI_FLAG_VGC_DOUBLES)

AI_DOUBLE_BATTLE_TEST("VGC IA contro IA: una mossa nascosta dell'altro lato non cambia la scelta")
{
    enum Move hidden;
    PARAMETRIZE { hidden = MOVE_CELEBRATE; }
    PARAMETRIZE { hidden = MOVE_HYDRO_PUMP; } // manderebbe KO l'Arcanine: visibile solo barando
    GIVEN {
        gTestAiVsAiBattle = TRUE; // riazzerata dal runner a ogni test
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_SPLASH, hidden); Speed(50); }
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_SPLASH); Speed(50); }
        OPPONENT(SPECIES_ARCANINE) { HP(60); Moves(MOVE_PROTECT, MOVE_FLARE_BLITZ); Speed(100); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SPLASH); Speed(10); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_FLARE_BLITZ); }
    }
}

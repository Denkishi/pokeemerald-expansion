#ifndef GUARD_BATTLE_AI_VGC_H
#define GUARD_BATTLE_AI_VGC_H

// IA per le lotte in doppio stile VGC (AI_FLAG_VGC_DOUBLES).
// Valutazione aggiuntiva di ogni mossa: aperture (Fake Out, Trick Room, Tailwind),
// setup sicuro, Protect "safe", fuoco concentrato con il partner, redirect e supporto.
s32 AI_VgcDoubles(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, s32 score);

// Vero se la squadra dell'AI (campo + panchina) ha almeno `minCount` Pokémon vivi più lenti
// di entrambi gli avversari in campo (= Trick Room le conviene).
bool32 AI_VgcTeamIsSlowerThanFoes(enum BattlerId battler, u32 minCount);

// Vero se la squadra dell'AI ha almeno `minCount` Pokémon vivi che con Tailwind supererebbero
// l'avversario più veloce ma senza no.
bool32 AI_VgcTeamBenefitsFromTailwind(enum BattlerId battler, u32 minCount);

// Vero se il controllo di velocità attivo (Tailwind nostro, oppure Trick Room quando siamo il lato lento)
// ha ancora più di un turno: in questi turni non si spreca tempo.
bool32 AI_VgcOwnSpeedControlActive(enum BattlerId battler);

#endif // GUARD_BATTLE_AI_VGC_H

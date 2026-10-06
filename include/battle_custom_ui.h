#ifndef GUARD_BATTLE_CUSTOM_UI_H
#define GUARD_BATTLE_CUSTOM_UI_H

// Order based numerically, with EFFECTIVENESS_CANNOT_VIEW at 0 to always prioritize any other effectiveness during comparison
enum
{
    EFFECTIVENESS_CANNOT_VIEW,
    EFFECTIVENESS_NO_EFFECT,
    EFFECTIVENESS_MOSTLY_INEFFECTIVE,
    EFFECTIVENESS_NOT_VERY_EFFECTIVE,
    EFFECTIVENESS_NORMAL,
    EFFECTIVENESS_SUPER_EFFECTIVE,
    EFFECTIVENESS_EXTREMELY_EFFECTIVE,
};

// Pagina info campo nella finestra descrizione mossa.
void PrintBattleFieldInfo(u32 windowId, enum BattlerId battler);

// Badge x4/x2/x1/1/2/1/4/X sopra i bersagli durante la scelta mossa.
void CreateEffectivenessBadges(enum BattlerId battler);

// Definita in battle_controller_player.c (EFFECTIVENESS_* della mossa sotto cursore).
u32 GetMoveSelectionEffectiveness(enum BattlerId battlerAtk, enum BattlerId battlerDef);

// Definita in battle_controller_player.c: danno stimato della mossa sotto cursore, in % dei PS
// massimi del bersaglio (tiro minimo e massimo). Usa solo ciò che il giocatore sa del bersaglio.
// FALSE se non c'è nulla da mostrare (mossa di stato, danno fisso...).
bool32 GetMoveSelectionDamageRange(enum BattlerId battlerAtk, enum BattlerId battlerDef, u32 *minPercent, u32 *maxPercent);

#endif // GUARD_BATTLE_CUSTOM_UI_H

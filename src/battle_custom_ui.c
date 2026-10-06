// UI custom in lotta:
//  - pagina "info campo" (tasto info mossa premuto 2 volte)
//  - badge efficacia (x4/x2/x1/1/2/1/4/X) sopra i Pokémon mentre scegli mossa / bersaglio
#include "global.h"
#include "battle.h"
#include "battle_controllers.h"
#include "battle_custom_ui.h"
#include "battle_interface.h"
#include "battle_util.h"
#include "menu.h"
#include "move.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "window.h"
#include "config/custom.h"
#include "constants/battle.h"

// =====================================================================
// Pagina info campo
// =====================================================================
static const u8 sFI_Weather[]   = _("Meteo:");
static const u8 sFI_Terrain[]   = _(" Campo:");
static const u8 sFI_None[]      = _("nessuno");
static const u8 sFI_Ally[]      = _("Noi:");
static const u8 sFI_Foe[]       = _("Loro:");
static const u8 sFI_Inf[]       = _("-");
static const u8 sFI_Space[]     = _(" ");
static const u8 sFI_Times[]     = _("x");
static const u8 sFI_Nothing[]   = _(" -");

static const u8 sFI_Rain[]      = _("Pioggia");
static const u8 sFI_HeavyRain[] = _("Diluvio");
static const u8 sFI_Sun[]       = _("Sole");
static const u8 sFI_HarshSun[]  = _("Siccità");
static const u8 sFI_Sand[]      = _("Sabbia");
static const u8 sFI_Hail[]      = _("Grandine");
static const u8 sFI_Snow[]      = _("Neve");
static const u8 sFI_Fog[]       = _("Nebbia");
static const u8 sFI_Winds[]     = _("Correnti");

static const u8 sFI_Grassy[]    = _("Erboso");
static const u8 sFI_Misty[]     = _("Nebbioso");
static const u8 sFI_Electric[]  = _("Elettrico");
static const u8 sFI_Psychic[]   = _("Psichico");

static const u8 sFI_TrickRoom[] = _(" Distortoz.");
static const u8 sFI_MagicRoom[] = _(" Magicoz.");
static const u8 sFI_WonderRoom[] = _(" Mirabilz.");
static const u8 sFI_Gravity[]   = _(" Gravità");

static const u8 sFI_Reflect[]   = _(" Rifl.");
static const u8 sFI_Light[]     = _(" Schermo");
static const u8 sFI_Veil[]      = _(" Velaur.");
static const u8 sFI_Tailwind[]  = _(" Vento");
static const u8 sFI_Safeguard[] = _(" Salvag.");
static const u8 sFI_Mist[]      = _(" Nebbia");
static const u8 sFI_Rocks[]     = _(" Rocce");
static const u8 sFI_Spikes[]    = _(" Punte");
static const u8 sFI_TSpikes[]   = _(" Fiele");
static const u8 sFI_Web[]       = _(" Rete");
static const u8 sFI_Steel[]     = _(" Acciaio");

static u8 *AppendTurns(u8 *ptr, u32 turns)
{
    if (turns == 0)
        return StringCopy(ptr, sFI_Inf);
    return ConvertIntToDecimalStringN(ptr, turns, STR_CONV_MODE_LEFT_ALIGN, 2);
}

static u8 *AppendTimed(u8 *ptr, const u8 *name, u32 turns)
{
    ptr = StringCopy(ptr, name);
    return AppendTurns(ptr, turns);
}

static u8 *BuildGlobalLine(u8 *ptr)
{
    u32 w = gBattleWeather;
    const u8 *name = NULL;

    ptr = StringCopy(ptr, sFI_Weather);
    if (w & B_WEATHER_RAIN_PRIMAL)        name = sFI_HeavyRain;
    else if (w & B_WEATHER_RAIN)          name = sFI_Rain;
    else if (w & B_WEATHER_SUN_PRIMAL)    name = sFI_HarshSun;
    else if (w & B_WEATHER_SUN)           name = sFI_Sun;
    else if (w & B_WEATHER_SANDSTORM)     name = sFI_Sand;
    else if (w & B_WEATHER_HAIL)          name = sFI_Hail;
    else if (w & B_WEATHER_SNOW)          name = sFI_Snow;
    else if (w & B_WEATHER_FOG)           name = sFI_Fog;
    else if (w & B_WEATHER_STRONG_WINDS)  name = sFI_Winds;
    if (name == NULL)
    {
        ptr = StringCopy(ptr, sFI_None);
    }
    else
    {
        ptr = StringCopy(ptr, name);
        ptr = StringCopy(ptr, sFI_Space);
        ptr = AppendTurns(ptr, (w & B_WEATHER_PRIMAL_ANY) ? 0 : gBattleStruct->weatherDuration);
    }

    ptr = StringCopy(ptr, sFI_Terrain);
    switch (gFieldTimers.terrain)
    {
    case B_TERRAIN_GRASSY:   name = sFI_Grassy;   break;
    case B_TERRAIN_MISTY:    name = sFI_Misty;    break;
    case B_TERRAIN_ELECTRIC: name = sFI_Electric; break;
    case B_TERRAIN_PSYCHIC:  name = sFI_Psychic;  break;
    default:                 name = NULL;         break;
    }
    if (name == NULL)
    {
        ptr = StringCopy(ptr, sFI_None);
    }
    else
    {
        ptr = StringCopy(ptr, name);
        ptr = StringCopy(ptr, sFI_Space);
        ptr = AppendTurns(ptr, gFieldTimers.terrainTimer);
    }
    return ptr;
}

static u8 *BuildRoomsLine(u8 *ptr)
{
    u8 *start = ptr;
    if (gFieldStatuses & STATUS_FIELD_TRICK_ROOM)  ptr = AppendTimed(ptr, sFI_TrickRoom, gFieldTimers.trickRoomTimer);
    if (gFieldStatuses & STATUS_FIELD_GRAVITY)     ptr = AppendTimed(ptr, sFI_Gravity, gFieldTimers.gravityTimer);
    if (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM)  ptr = AppendTimed(ptr, sFI_MagicRoom, gFieldTimers.magicRoomTimer);
    if (gFieldStatuses & STATUS_FIELD_WONDER_ROOM) ptr = AppendTimed(ptr, sFI_WonderRoom, gFieldTimers.wonderRoomTimer);
    if (ptr == start)
        *ptr = EOS;
    return ptr;
}

static u8 *BuildSideLine(u8 *ptr, enum BattleSide side, const u8 *label)
{
    u8 *start;
    u32 st = gSideStatuses[side];

    ptr = StringCopy(ptr, label);
    start = ptr;
    if (st & SIDE_STATUS_REFLECT)     ptr = AppendTimed(ptr, sFI_Reflect, gSideTimers[side].reflectTimer);
    if (st & SIDE_STATUS_LIGHTSCREEN) ptr = AppendTimed(ptr, sFI_Light, gSideTimers[side].lightscreenTimer);
    if (st & SIDE_STATUS_AURORA_VEIL) ptr = AppendTimed(ptr, sFI_Veil, gSideTimers[side].auroraVeilTimer);
    if (st & SIDE_STATUS_TAILWIND)    ptr = AppendTimed(ptr, sFI_Tailwind, gSideTimers[side].tailwindTimer);
    if (st & SIDE_STATUS_SAFEGUARD)   ptr = AppendTimed(ptr, sFI_Safeguard, gSideTimers[side].safeguardTimer);
    if (st & SIDE_STATUS_MIST)        ptr = AppendTimed(ptr, sFI_Mist, gSideTimers[side].mistTimer);
    if (IsHazardOnSide(side, HAZARDS_STEALTH_ROCK))
        ptr = StringCopy(ptr, sFI_Rocks);
    if (IsHazardOnSide(side, HAZARDS_STEELSURGE))
        ptr = StringCopy(ptr, sFI_Steel);
    if (IsHazardOnSide(side, HAZARDS_SPIKES))
    {
        ptr = StringCopy(ptr, sFI_Spikes);
        ptr = StringCopy(ptr, sFI_Times);
        ptr = ConvertIntToDecimalStringN(ptr, gSideTimers[side].spikesAmount, STR_CONV_MODE_LEFT_ALIGN, 1);
    }
    if (IsHazardOnSide(side, HAZARDS_TOXIC_SPIKES))
    {
        ptr = StringCopy(ptr, sFI_TSpikes);
        ptr = StringCopy(ptr, sFI_Times);
        ptr = ConvertIntToDecimalStringN(ptr, gSideTimers[side].toxicSpikesAmount, STR_CONV_MODE_LEFT_ALIGN, 1);
    }
    if (IsHazardOnSide(side, HAZARDS_STICKY_WEB))
        ptr = StringCopy(ptr, sFI_Web);
    if (ptr == start)
        ptr = StringCopy(ptr, sFI_Nothing);
    return ptr;
}

static void PrintFieldLine(u32 windowId, u32 y, const u8 *str)
{
    static const u8 colors[3] = {0xE, TEXT_DYNAMIC_COLOR_4, TEXT_DYNAMIC_COLOR_6};
    u32 width = WindowWidthPx(windowId) - 2;
    u32 font = FONT_NARROW;

    if (GetStringWidth(font, str, 0) > width)
        font = FONT_NARROWER;
    if (GetStringWidth(font, str, 0) > width)
        font = FONT_SMALL_NARROW;
    AddTextPrinterParameterized3(windowId, font, 1, y, colors, TEXT_SKIP_DRAW, str);
}

void PrintBattleFieldInfo(u32 windowId, enum BattlerId battler)
{
    u8 line[64];
    u8 *ptr;
    enum BattleSide ownSide = GetBattlerSide(battler);

    LoadMessageBoxAndBorderGfx();
    DrawStdWindowFrame(windowId, FALSE);
    FillWindowPixelBuffer(windowId, PIXEL_FILL(0xE));

    // Riga 1: meteo + terreno (+ stanze se c'è spazio sotto)
    BuildGlobalLine(line);
    PrintFieldLine(windowId, 0, line);

    // Riga 2: stanze/gravità, se presenti; altrimenti lato nostro
    ptr = BuildRoomsLine(line);
    if (ptr != line)
    {
        PrintFieldLine(windowId, 16, line + 1); // salta lo spazio iniziale
        // Riga 3: noi + loro insieme
        ptr = BuildSideLine(line, ownSide, sFI_Ally);
        ptr = StringCopy(ptr, sFI_Space);
        BuildSideLine(ptr, (ownSide == B_SIDE_PLAYER ? B_SIDE_OPPONENT : B_SIDE_PLAYER), sFI_Foe);
        PrintFieldLine(windowId, 32, line);
    }
    else
    {
        BuildSideLine(line, ownSide, sFI_Ally);
        PrintFieldLine(windowId, 16, line);
        BuildSideLine(line, (ownSide == B_SIDE_PLAYER ? B_SIDE_OPPONENT : B_SIDE_PLAYER), sFI_Foe);
        PrintFieldLine(windowId, 32, line);
    }
    CopyWindowToVram(windowId, COPYWIN_FULL);
}

// =====================================================================
// Badge efficacia
// =====================================================================
#define TAG_EFF_BADGE 0xE7A1

enum
{
    BADGE_X4,
    BADGE_X2,
    BADGE_X1,
    BADGE_HALF,
    BADGE_QUARTER,
    BADGE_IMMUNE,
};

#define sAttacker   data[0]
#define sTarget     data[1]
#define sLastKey    data[2]

#ifdef INCGFX_U32
static const u32 sBadgeGfx[] = INCGFX_U32("graphics/battle_interface/effectiveness_indicator.png", ".4bpp");
static const u16 sBadgePal[] = INCGFX_U16("graphics/battle_interface/effectiveness_indicator.png", ".gbapal");
#else
static const u32 sBadgeGfx[] = INCBIN_U32("graphics/battle_interface/effectiveness_indicator.4bpp");
static const u16 sBadgePal[] = INCBIN_U16("graphics/battle_interface/effectiveness_indicator.gbapal");
#endif

static const struct SpriteSheet sBadgeSheet = { sBadgeGfx, 16 * 8 * 6 / 2, TAG_EFF_BADGE };
static const struct SpritePalette sBadgePalette = { sBadgePal, TAG_EFF_BADGE };

static const struct OamData sBadgeOam =
{
    .shape = SPRITE_SHAPE(16x8),
    .size = SPRITE_SIZE(16x8),
    .priority = 0,
};

static const union AnimCmd sAnim_X4[]      = { ANIMCMD_FRAME(0, 0),  ANIMCMD_END };
static const union AnimCmd sAnim_X2[]      = { ANIMCMD_FRAME(2, 0),  ANIMCMD_END };
static const union AnimCmd sAnim_X1[]      = { ANIMCMD_FRAME(4, 0),  ANIMCMD_END };
static const union AnimCmd sAnim_Half[]    = { ANIMCMD_FRAME(6, 0),  ANIMCMD_END };
static const union AnimCmd sAnim_Quarter[] = { ANIMCMD_FRAME(8, 0),  ANIMCMD_END };
static const union AnimCmd sAnim_Immune[]  = { ANIMCMD_FRAME(10, 0), ANIMCMD_END };

static const union AnimCmd *const sBadgeAnims[] =
{
    [BADGE_X4]      = sAnim_X4,
    [BADGE_X2]      = sAnim_X2,
    [BADGE_X1]      = sAnim_X1,
    [BADGE_HALF]    = sAnim_Half,
    [BADGE_QUARTER] = sAnim_Quarter,
    [BADGE_IMMUNE]  = sAnim_Immune,
};

static void SpriteCB_EffBadge(struct Sprite *sprite);

static const struct SpriteTemplate sBadgeTemplate =
{
    .tileTag = TAG_EFF_BADGE,
    .paletteTag = TAG_EFF_BADGE,
    .oam = &sBadgeOam,
    .anims = sBadgeAnims,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_EffBadge,
};

static void (*const sMoveSelectFuncs[])(enum BattlerId battler) =
{
    PlayerHandleChooseMove,
    HandleChooseMoveAfterDma3,
    HandleInputChooseTarget,
    HandleInputShowTargets,
    HandleInputShowEntireFieldTargets,
    HandleMoveSwitching,
    HandleInputChooseMove,
};

static bool32 IsInMoveSelection(enum BattlerId battler)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sMoveSelectFuncs); i++)
        if (gBattlerControllerFuncs[battler] == sMoveSelectFuncs[i])
            return TRUE;
    return FALSE;
}

static u8 sBadgeCount;

static void DestroyBadge(struct Sprite *sprite)
{
    DestroySprite(sprite);
    if (sBadgeCount)
        sBadgeCount--;
    if (sBadgeCount == 0)
    {
        FreeSpriteTilesByTag(TAG_EFF_BADGE);
        FreeSpritePaletteByTag(TAG_EFF_BADGE);
    }
}

static enum Move CurrentSelectedMove(enum BattlerId battler)
{
    struct ChooseMoveStruct *moveInfo = (struct ChooseMoveStruct *)(&gBattleResources->bufferA[battler][4]);
    return moveInfo->moves[gMoveSelectionCursor[battler]];
}

static bool32 ShouldShowOnTarget(enum BattlerId atk, enum BattlerId def, enum Move move)
{
    enum MoveTarget target;

    if (!IsBattlerAlive(def) || move == MOVE_NONE || IsBattleMoveStatus(move))
        return FALSE;
    if (!IsBattlerAlly(atk, def))
        return TRUE;
    // Alleato: solo se la mossa lo colpisce o se il cursore bersaglio è su di lui.
    target = GetBattlerMoveSelectionTargetType(atk, move);
    if (target == TARGET_FOES_AND_ALLY)
        return TRUE;
    if (gBattlerControllerFuncs[atk] == HandleInputChooseTarget && gMultiUsePlayerCursor == def)
        return TRUE;
    return FALSE;
}

static s32 BadgeFromEffectiveness(u32 eff)
{
    switch (eff)
    {
    case EFFECTIVENESS_EXTREMELY_EFFECTIVE: return BADGE_X4;
    case EFFECTIVENESS_SUPER_EFFECTIVE:     return BADGE_X2;
    case EFFECTIVENESS_NORMAL:              return CUSTOM_EFFECTIVENESS_SHOW_NEUTRAL ? BADGE_X1 : -1;
    case EFFECTIVENESS_NOT_VERY_EFFECTIVE:  return BADGE_HALF;
    case EFFECTIVENESS_MOSTLY_INEFFECTIVE:  return BADGE_QUARTER;
    case EFFECTIVENESS_NO_EFFECT:           return BADGE_IMMUNE;
    default:                                return -1;
    }
}

static void PlaceBadge(struct Sprite *sprite)
{
    struct Sprite *hb = &gSprites[gHealthboxSpriteIds[sprite->sTarget]];
    if (IsOnPlayerSide(sprite->sTarget))
        sprite->x = hb->x + hb->x2 - 44;
    else
        sprite->x = hb->x + hb->x2 + 72;
    sprite->y = hb->y + hb->y2 - 6;
}

static void SpriteCB_EffBadge(struct Sprite *sprite)
{
    enum BattlerId atk = sprite->sAttacker, def = sprite->sTarget;
    enum Move move;
    s32 key;

    if (!IsInMoveSelection(atk))
    {
        DestroyBadge(sprite);
        return;
    }

    move = CurrentSelectedMove(atk);
    // Ricalcola solo se cambia mossa, cursore bersaglio o gimmick.
    key = (gMoveSelectionCursor[atk] + 1) | (gMultiUsePlayerCursor << 4)
        | (gBattleStruct->gimmick.playerSelect << 8) | ((gBattlerControllerFuncs[atk] == HandleInputChooseTarget) << 9);
    if (key != sprite->sLastKey)
    {
        s32 badge = -1;
        sprite->sLastKey = key;
        if (ShouldShowOnTarget(atk, def, move))
            badge = BadgeFromEffectiveness(GetMoveSelectionEffectiveness(atk, def));
        if (badge < 0)
        {
            sprite->invisible = TRUE;
        }
        else
        {
            sprite->invisible = FALSE;
            StartSpriteAnim(sprite, badge);
        }
    }

    // Lampeggia il badge del bersaglio scelto in doppio.
    if (!sprite->invisible && gBattlerControllerFuncs[atk] == HandleInputChooseTarget && gMultiUsePlayerCursor == def)
        sprite->y2 = ((gMain.vblankCounter1 >> 3) & 1) ? -1 : 0;
    else
        sprite->y2 = 0;

    PlaceBadge(sprite);
}

void CreateEffectivenessBadges(enum BattlerId battler)
{
    enum BattlerId def;

    if (!CUSTOM_EFFECTIVENESS_BADGES)
        return;
    if (IndexOfSpriteTileTag(TAG_EFF_BADGE) == 0xFF)
    {
        sBadgeCount = 0;
        LoadSpriteSheet(&sBadgeSheet);
        LoadSpritePalette(&sBadgePalette);
    }
    for (def = 0; def < gBattlersCount; def++)
    {
        u32 spriteId;
        u32 i;
        bool32 exists = FALSE;

        if (def == battler)
            continue;
        // Niente doppioni (es. si torna al menu mosse).
        for (i = 0; i < MAX_SPRITES; i++)
        {
            if (gSprites[i].inUse && gSprites[i].template == &sBadgeTemplate
             && gSprites[i].sAttacker == battler && gSprites[i].sTarget == def)
                exists = TRUE;
        }
        if (exists)
            continue;
        spriteId = CreateSprite(&sBadgeTemplate, 0, 0, 0);
        if (spriteId == MAX_SPRITES)
            continue;
        sBadgeCount++;
        gSprites[spriteId].sAttacker = battler;
        gSprites[spriteId].sTarget = def;
        gSprites[spriteId].sLastKey = -1;
        gSprites[spriteId].invisible = TRUE;
        PlaceBadge(&gSprites[spriteId]);
    }
}

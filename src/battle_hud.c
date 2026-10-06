// HUD sempre visibile in lotta:
//  - etichette con le modifiche alle statistiche accanto al riquadro PS di ogni Pokémon;
//  - riquadro in alto a destra con meteo, terreno e stanze attivi e i turni rimasti.
//
// Sono sprite su cui il testo viene stampato direttamente (come nome e livello nei riquadri PS).
// Usano la palette dei riquadri PS, già caricata per tutta la lotta: nessuna palette in più.
#include "global.h"
#include "battle.h"
#include "battle_hud.h"
#include "battle_info.h"
#include "battle_interface.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "config/custom.h"
#include "constants/battle.h"

#define TAG_STAT_LABEL  0xE7C0  // + battler (0..3)
#define TAG_FIELD_HUD   0xE7C4

// Indici nella palette dei riquadri PS (graphics/battle_interface/healthbox_*.png).
// Gli indici 12-15 sono riscritti dal gioco per le icone di stato: non usarli.
#define HUD_COL_CLEAR   0
#define HUD_COL_BG      7   // verde scuro
#define HUD_COL_TEXT    2   // crema
#define HUD_COL_NUMBER  9   // giallo
#define HUD_COL_UP      10  // rosa/rosso
#define HUD_COL_DOWN    11  // azzurro

#define HUD_FONT        FONT_SMALL_NARROWER
#define HUD_LINE_H      8

#define STAT_LABEL_W    32
#define STAT_LABEL_H    32
#define STAT_LABEL_ROWS (STAT_LABEL_H / HUD_LINE_H)
#define FIELD_HUD_W     64
#define FIELD_HUD_H     32
#define FIELD_HUD_ROWS  (FIELD_HUD_H / HUD_LINE_H)
#define FIELD_HUD_X     208 // centro: copre x 176..239
#define FIELD_HUD_Y     16  //         e    y 0..31

// Stato per sprite. data[1] e data[2] sono usati dal motore di stampa su sprite (sprite concatenati).
#define sBattler    data[0]
#define sNextX      data[1]
#define sNextY      data[2]
#define sLastKey    data[3]

static const u8 ALIGNED(4) sBlankGfx[FIELD_HUD_W * FIELD_HUD_H / 2] = {0};

static const struct OamData sOam_StatLabel =
{
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .priority = 1,
};

static const struct OamData sOam_FieldHud =
{
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .priority = 1,
};

static void SpriteCB_StatLabel(struct Sprite *sprite);
static void SpriteCB_FieldHud(struct Sprite *sprite);

#define STAT_LABEL_TEMPLATE(n)                          \
    {                                                   \
        .tileTag = TAG_STAT_LABEL + (n),                \
        .paletteTag = TAG_HEALTHBOX_PAL,                \
        .oam = &sOam_StatLabel,                         \
        .anims = gDummySpriteAnimTable,                 \
        .images = NULL,                                 \
        .affineAnims = gDummySpriteAffineAnimTable,     \
        .callback = SpriteCB_StatLabel,                 \
    }

static const struct SpriteTemplate sStatLabelTemplates[MAX_BATTLERS_COUNT] =
{
    STAT_LABEL_TEMPLATE(0),
    STAT_LABEL_TEMPLATE(1),
    STAT_LABEL_TEMPLATE(2),
    STAT_LABEL_TEMPLATE(3),
};

static const struct SpriteTemplate sFieldHudTemplate =
{
    .tileTag = TAG_FIELD_HUD,
    .paletteTag = TAG_HEALTHBOX_PAL,
    .oam = &sOam_FieldHud,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_FieldHud,
};

static EWRAM_DATA u8 sStatLabelIds[MAX_BATTLERS_COUNT] = {0};
static EWRAM_DATA u8 sFieldHudId = 0;

static const u8 sText_Plus[]  = _("+");
static const u8 sText_Minus[] = _("-");
static const u8 sText_More[]  = _("…");
static const u8 sText_Space[] = _(" ");
static const u8 sText_Forever[] = _("-");

static const u8 sText_HudAtk[] = _("At");
static const u8 sText_HudDef[] = _("Di");
static const u8 sText_HudSpe[] = _("Ve");
static const u8 sText_HudSpA[] = _("AS");
static const u8 sText_HudSpD[] = _("DS");
static const u8 sText_HudAcc[] = _("Pr");
static const u8 sText_HudEva[] = _("El");

static const u8 *const sHudStatAbbr[NUM_BATTLE_STATS] =
{
    [STAT_ATK]     = sText_HudAtk,
    [STAT_DEF]     = sText_HudDef,
    [STAT_SPEED]   = sText_HudSpe,
    [STAT_SPATK]   = sText_HudSpA,
    [STAT_SPDEF]   = sText_HudSpD,
    [STAT_ACC]     = sText_HudAcc,
    [STAT_EVASION] = sText_HudEva,
};

static void HudPrint(u32 spriteId, u32 x, u32 y, u32 fg, const u8 *str)
{
    union TextColor color = {.background = HUD_COL_BG, .foreground = fg, .shadow = HUD_COL_BG, .accent = HUD_COL_BG};
    AddSpriteTextPrinterParameterized6(spriteId, HUD_FONT, x, y, 0, 0, color, 0, str);
}

static bool32 IsHudSpriteAlive(u32 spriteId, SpriteCallback callback)
{
    return spriteId < MAX_SPRITES && gSprites[spriteId].inUse && gSprites[spriteId].callback == callback;
}

// ---------------------------------------------------------------------
// Modifiche alle statistiche
// ---------------------------------------------------------------------
static void DrawStatLabel(struct Sprite *sprite, u32 spriteId, u32 battler)
{
    u32 stat, row = 0, total = 0;

    FillSpriteRectColor(spriteId, 0, 0, STAT_LABEL_W, STAT_LABEL_H, HUD_COL_CLEAR);
    for (stat = STAT_ATK; stat < NUM_BATTLE_STATS; stat++)
    {
        if (gBattleMons[battler].statStages[stat] != DEFAULT_STAT_STAGE)
            total++;
    }
    if (total == 0)
    {
        sprite->invisible = TRUE;
        return;
    }

    for (stat = STAT_ATK; stat < NUM_BATTLE_STATS && row < STAT_LABEL_ROWS; stat++)
    {
        s32 stage = (s32)gBattleMons[battler].statStages[stat] - DEFAULT_STAT_STAGE;
        u32 y = row * HUD_LINE_H;
        u32 abbrWidth;
        u8 text[4];
        u8 *ptr;

        if (stage == 0)
            continue;
        // Ultima riga ma restano altre modifiche: puntini.
        if (row == STAT_LABEL_ROWS - 1 && total > STAT_LABEL_ROWS)
        {
            FillSpriteRectColor(spriteId, 0, y, 12, HUD_LINE_H, HUD_COL_BG);
            HudPrint(spriteId, 1, y, HUD_COL_TEXT, sText_More);
            break;
        }
        ptr = StringCopy(text, (stage > 0) ? sText_Plus : sText_Minus);
        ConvertIntToDecimalStringN(ptr, (stage > 0) ? stage : -stage, STR_CONV_MODE_LEFT_ALIGN, 1);
        abbrWidth = GetStringWidth(HUD_FONT, sHudStatAbbr[stat], 0);
        FillSpriteRectColor(spriteId, 0, y, min(abbrWidth + GetStringWidth(HUD_FONT, text, 0) + 2, STAT_LABEL_W), HUD_LINE_H, HUD_COL_BG);
        HudPrint(spriteId, 1, y, HUD_COL_TEXT, sHudStatAbbr[stat]);
        HudPrint(spriteId, 1 + abbrWidth, y, (stage > 0) ? HUD_COL_UP : HUD_COL_DOWN, text);
        row++;
    }
    sprite->invisible = FALSE;
}

static void SpriteCB_StatLabel(struct Sprite *sprite)
{
    u32 battler = sprite->sBattler;
    u32 spriteId = sprite - gSprites;
    struct Sprite *healthbox;
    u32 stat, hash = 0;
    s32 key;

    if (battler >= gBattlersCount || (gAbsentBattlerFlags & (1u << battler)) || !IsBattlerAlive(battler))
    {
        sprite->invisible = TRUE;
        sprite->sLastKey = -1;
        return;
    }
    healthbox = &gSprites[gHealthboxSpriteIds[battler]];
    if (!healthbox->inUse || healthbox->invisible)
    {
        sprite->invisible = TRUE;
        sprite->sLastKey = -1;
        return;
    }

    // Accanto al riquadro PS: a destra per gli avversari, a sinistra per i nostri.
    sprite->x = healthbox->x + healthbox->x2 + (IsOnPlayerSide(battler) ? -68 : 86);
    sprite->y = healthbox->y + healthbox->y2 + 14;

    for (stat = STAT_ATK; stat < NUM_BATTLE_STATS; stat++)
        hash = hash * 13 + gBattleMons[battler].statStages[stat];
    key = hash & 0x7FFF;
    if (key != sprite->sLastKey)
    {
        sprite->sLastKey = key;
        DrawStatLabel(sprite, spriteId, battler);
    }
}

// ---------------------------------------------------------------------
// Meteo / terreno / stanze
// ---------------------------------------------------------------------
static void DrawFieldLine(u32 spriteId, u32 row, const u8 *name, u32 turns)
{
    u8 number[4];
    u32 y = row * HUD_LINE_H;
    u32 nameWidth = GetStringWidth(HUD_FONT, name, 0);
    u32 spaceWidth = GetStringWidth(HUD_FONT, sText_Space, 0);
    u32 width, x;

    if (turns == 0)
        StringCopy(number, sText_Forever); // durata illimitata
    else
        ConvertIntToDecimalStringN(number, turns, STR_CONV_MODE_LEFT_ALIGN, 2);
    width = nameWidth + spaceWidth + GetStringWidth(HUD_FONT, number, 0) + 2;
    if (width > FIELD_HUD_W)
        width = FIELD_HUD_W;
    x = FIELD_HUD_W - width; // allineato a destra

    FillSpriteRectColor(spriteId, x, y, width, HUD_LINE_H, HUD_COL_BG);
    HudPrint(spriteId, x + 1, y, HUD_COL_TEXT, name);
    HudPrint(spriteId, x + 1 + nameWidth + spaceWidth, y, HUD_COL_NUMBER, number);
}

static void DrawFieldHud(struct Sprite *sprite, u32 spriteId)
{
    u32 row = 0;
    const u8 *name;

    FillSpriteRectColor(spriteId, 0, 0, FIELD_HUD_W, FIELD_HUD_H, HUD_COL_CLEAR);

    name = BattleInfo_GetWeatherName();
    if (name != NULL && row < FIELD_HUD_ROWS)
        DrawFieldLine(spriteId, row++, name, (gBattleWeather & B_WEATHER_PRIMAL_ANY) ? 0 : gBattleStruct->weatherDuration);
    name = BattleInfo_GetTerrainName();
    if (name != NULL && row < FIELD_HUD_ROWS)
        DrawFieldLine(spriteId, row++, name, gFieldTimers.terrainTimer);
    if ((gFieldStatuses & STATUS_FIELD_TRICK_ROOM) && row < FIELD_HUD_ROWS)
        DrawFieldLine(spriteId, row++, BattleInfo_GetRoomName(STATUS_FIELD_TRICK_ROOM), gFieldTimers.trickRoomTimer);
    if ((gFieldStatuses & STATUS_FIELD_GRAVITY) && row < FIELD_HUD_ROWS)
        DrawFieldLine(spriteId, row++, BattleInfo_GetRoomName(STATUS_FIELD_GRAVITY), gFieldTimers.gravityTimer);
    if ((gFieldStatuses & STATUS_FIELD_MAGIC_ROOM) && row < FIELD_HUD_ROWS)
        DrawFieldLine(spriteId, row++, BattleInfo_GetRoomName(STATUS_FIELD_MAGIC_ROOM), gFieldTimers.magicRoomTimer);
    if ((gFieldStatuses & STATUS_FIELD_WONDER_ROOM) && row < FIELD_HUD_ROWS)
        DrawFieldLine(spriteId, row++, BattleInfo_GetRoomName(STATUS_FIELD_WONDER_ROOM), gFieldTimers.wonderRoomTimer);

    sprite->invisible = (row == 0);
}

static void SpriteCB_FieldHud(struct Sprite *sprite)
{
    u32 hash;
    s32 key;

    hash = gBattleWeather;
    hash = hash * 31 + gBattleStruct->weatherDuration;
    hash = hash * 31 + gFieldTimers.terrain;
    hash = hash * 31 + gFieldTimers.terrainTimer;
    hash = hash * 31 + (gFieldStatuses & (STATUS_FIELD_TRICK_ROOM | STATUS_FIELD_GRAVITY | STATUS_FIELD_MAGIC_ROOM | STATUS_FIELD_WONDER_ROOM));
    hash = hash * 31 + gFieldTimers.trickRoomTimer;
    hash = hash * 31 + gFieldTimers.gravityTimer;
    hash = hash * 31 + gFieldTimers.magicRoomTimer;
    hash = hash * 31 + gFieldTimers.wonderRoomTimer;
    key = hash & 0x7FFF;
    if (key != sprite->sLastKey)
    {
        sprite->sLastKey = key;
        DrawFieldHud(sprite, sprite - gSprites);
    }
}

// ---------------------------------------------------------------------
// Creazione
// ---------------------------------------------------------------------
static u32 CreateHudSprite(const struct SpriteTemplate *template, u32 sheetSize, s32 x, s32 y)
{
    u32 spriteId;

    if (GetSpriteTileStartByTag(template->tileTag) == 0xFFFF)
    {
        struct SpriteSheet sheet = {sBlankGfx, sheetSize, template->tileTag};
        if (LoadSpriteSheet(&sheet) == 0)
            return MAX_SPRITES;
    }
    spriteId = CreateSprite(template, x, y, 0);
    if (spriteId >= MAX_SPRITES)
        return MAX_SPRITES;
    gSprites[spriteId].sNextX = SPRITE_NONE;
    gSprites[spriteId].sNextY = SPRITE_NONE;
    gSprites[spriteId].sLastKey = -1;
    gSprites[spriteId].invisible = TRUE;
    return spriteId;
}

void BattleHud_Reset(void)
{
    u32 i;

    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
        sStatLabelIds[i] = MAX_SPRITES;
    sFieldHudId = MAX_SPRITES;
}

// Crea gli sprite che mancano. Va richiamata dopo ogni ritorno alla schermata di lotta
// (gli sprite vengono azzerati): viene chiamata a ogni apertura del menu azioni.
void BattleHud_Ensure(void)
{
    u32 battler;

    if (!CUSTOM_BATTLE_HUD)
        return;
    if (IndexOfSpritePaletteTag(TAG_HEALTHBOX_PAL) == 0xFF)
        return;

    for (battler = 0; battler < gBattlersCount && battler < MAX_BATTLERS_COUNT; battler++)
    {
        if (IsHudSpriteAlive(sStatLabelIds[battler], SpriteCB_StatLabel))
            continue;
        sStatLabelIds[battler] = CreateHudSprite(&sStatLabelTemplates[battler], STAT_LABEL_W * STAT_LABEL_H / 2, 0, 0);
        if (sStatLabelIds[battler] < MAX_SPRITES)
            gSprites[sStatLabelIds[battler]].sBattler = battler;
    }
    if (!IsHudSpriteAlive(sFieldHudId, SpriteCB_FieldHud))
        sFieldHudId = CreateHudSprite(&sFieldHudTemplate, FIELD_HUD_W * FIELD_HUD_H / 2, FIELD_HUD_X, FIELD_HUD_Y);
}

#include "global.h"
#include "main.h"
#include "bg.h"
#include "event_data.h"
#include "field_screen_effect.h"
#include "gpu_regs.h"
#include "item.h"
#include "list_menu.h"
#include "mail.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "pokemon_storage_system.h"
#include "pokemon_summary_screen.h"
#include "random.h"
#include "rental_teams.h"
#include "scanline_effect.h"
#include "script.h"
#include "script_menu.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "script_pokemon_util.h"
#include "constants/battle.h"
#include "constants/party_menu.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "data/rental_teams.h"

#define TAG_RENTAL_SCROLL_ARROW 2100
#define TAG_RENTAL_HELD_ITEM    55120
#define ITEM_ID_RANDOM          1000


// =====================================================================
// Schermata "Team a noleggio": lista team + anteprima + sommario.
//
// Riscritta da zero. Regole seguite per non corrompere la memoria:
//  - nessuna Alloc/Free propria: stato, tilemap e Pokémon del sommario sono statici;
//  - niente ListMenu, frecce di scorrimento o sprite compressi (usano buffer temporanei sull'heap);
//  - l'unico heap usato è quello standard delle finestre (InitWindows / FreeAllWindowBuffers).
// =====================================================================
enum
{
    WIN_HEADER,
    WIN_TEAM_LIST,
    WIN_PREVIEW,
};

enum
{
    FOCUS_TEAM_LIST,
    FOCUS_PREVIEW_MONS,
};

#define RB_ROWS         7       // righe visibili nella lista
#define RB_ROW_H        16
#define RB_FRAME_TILE   0x200
#define RB_FRAME_PAL    14
#define RB_TEXT_PAL     15
#define RB_NO_TEAM      0xFFFF
#define RB_TEAM_SLOTS   6

// Origine (in pixel schermo) della finestra anteprima: serve per disegnare dietro le icone.
#define RB_PREVIEW_X    (18 * 8)
#define RB_PREVIEW_Y    (4 * 8)

struct RentalBrowser
{
    u16 category;
    u16 numTeams;   // team nella categoria
    u16 cursor;     // 0 = Team Casuale, 1..numTeams = team
    u16 scroll;     // prima riga visibile
    u16 result;
    u8 focus;
    u8 monIdx;      // slot selezionato nell'anteprima
    u8 monIconSpriteIds[RB_TEAM_SLOTS];
    u8 itemSpriteIds[RB_TEAM_SLOTS];
};

static EWRAM_DATA struct RentalBrowser sRB = {0};
static EWRAM_DATA struct Pokemon sRentalSummaryMons[RB_TEAM_SLOTS] = {0};
static EWRAM_DATA u8 sRentalSummarySlots[RB_TEAM_SLOTS] = {0}; // indice nel sommario -> slot del team
static EWRAM_DATA u8 sRentalSummaryCount = 0;
static EWRAM_DATA u16 sRB_Tilemap[BG_SCREEN_SIZE / 2] = {0};

static const u8 sText_CasualeHeader[] = _("CASUALE");
static const u8 sText_CasualeDescription[] = _("Premendo A\nriceverai un\nteam a sorpresa\ntra quelli di\nquesta lista!");
static const u8 sText_KeyHelpRandom[] = _("A:Scegli  B:Esci");
static const u8 sText_AnteprimaHeader[] = _("ANTEPRIMA");
static const u8 sText_KeyHelpList[] = _("A:Scegli  {DPAD_RIGHT}:Info");
static const u8 sText_KeyHelpPreview[] = _("A: Sommario   B: Indietro   START: Scegli");
static const u8 sText_RandomTeamOption[] = _("Team Casuale");
static const u8 sText_AllTeamsCategory[] = _("TUTTI I TEAM (76)");
static const u8 sText_HeaderPrefix[] = _("TEAM A NOLEGGIO - ");
static const u8 sText_UnknownTeam[] = _("Team Sconosciuto");
static const u8 sText_StrNone[] = _("Str: Nessuno");
static const u8 sText_StrPrefix[] = _("Str: ");
static const u8 sText_MonIdxOpen[] = _("(");
static const u8 sText_MonIdxSep[] = _("/6) ");
static const u8 sText_RB_Cursor[] = _("▶");
static const u8 sText_RB_Up[] = _("{UP_ARROW}");
static const u8 sText_RB_Down[] = _("{DOWN_ARROW}");

// {sfondo, testo, ombra} sulla palette standard dei menu
static const u8 sRB_ColDark[] = {TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sRB_ColGray[] = {TEXT_COLOR_WHITE, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_WHITE};
static const u8 sRB_ColRed[]  = {TEXT_COLOR_WHITE, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};

static const struct BgTemplate sRB_BgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0,
    },
};

static const struct WindowTemplate sRB_WinTemplates[] =
{
    [WIN_HEADER] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 28,
        .height = 2,
        .paletteNum = RB_TEXT_PAL,
        .baseBlock = 0x0001,
    },
    [WIN_TEAM_LIST] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 4,
        .width = 16,
        .height = 14,
        .paletteNum = RB_TEXT_PAL,
        .baseBlock = 0x0039,
    },
    [WIN_PREVIEW] = {
        .bg = 0,
        .tilemapLeft = 18,
        .tilemapTop = 4,
        .width = 11,
        .height = 14,
        .paletteNum = RB_TEXT_PAL,
        .baseBlock = 0x0119,
    },
    DUMMY_WIN_TEMPLATE
};

// Centro (in pixel schermo) delle 6 icone dell'anteprima.
static const s16 sRB_IconCoords[RB_TEAM_SLOTS][2] =
{
    { 168, 58 },  { 206, 58 },
    { 168, 86 },  { 206, 86 },
    { 168, 114 }, { 206, 114 },
};

static void SpriteCB_RentalHeldItem(struct Sprite *sprite);

static const struct OamData sOamData_RentalHeldItem =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(8x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(8x8),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
    .affineParam = 0,
};

static const union AnimCmd sSpriteAnim_HeldItem[] =
{
    ANIMCMD_FRAME(0, 1),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_HeldMail[] =
{
    ANIMCMD_FRAME(1, 1),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteAnimTable_HeldItem[] =
{
    sSpriteAnim_HeldItem,
    sSpriteAnim_HeldMail,
};

static const struct SpriteTemplate sSpriteTemplate_RentalHeldItem =
{
    .tileTag = TAG_RENTAL_HELD_ITEM,
    .paletteTag = TAG_RENTAL_HELD_ITEM,
    .oam = &sOamData_RentalHeldItem,
    .anims = sSpriteAnimTable_HeldItem,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_RentalHeldItem,
};

static void SpriteCB_RentalHeldItem(struct Sprite *sprite)
{
    u8 monSpriteId = sprite->data[7];
    if (monSpriteId >= MAX_SPRITES || gSprites[monSpriteId].invisible)
    {
        sprite->invisible = TRUE;
    }
    else
    {
        sprite->invisible = FALSE;
        sprite->x = gSprites[monSpriteId].x + gSprites[monSpriteId].x2 + 9;
        sprite->y = gSprites[monSpriteId].y + gSprites[monSpriteId].y2 + 8;
    }
}

static void SpriteCB_BounceRentalMonIcon(struct Sprite *sprite)
{
    u8 animCmd = UpdateMonIconFrame(sprite);

    if (animCmd != 0)
    {
        if (animCmd & 1)
            sprite->y2 = -3;
        else
            sprite->y2 = 1;
    }
}

static void BuildRentalPokemon(struct Pokemon *mon, const struct PresetRentalMon *rMon)
{
    u32 j;
    struct PokemonTemplate template = {0};
    template.species = rMon->species;
    template.heldItem = rMon->item;
    template.level = 50;
    template.ball = ITEM_POKE_BALL;
    template.nature = rMon->nature;
    template.gender = MON_GENDER_RANDOM;
    template.origin = GIFTMON_ORIGIN;
    template.isShiny = rMon->isShiny;
    template.doNotUseDefaultShinyness = TRUE;
    template.abilityNum = rMon->abilityNum;
    template.doNotUseDefaultAbility = TRUE;
    template.teraType = rMon->teraType;
    template.doNotUseDefaultTeraType = (rMon->teraType != TYPE_NONE);
    template.ignoreTotalEvCheck = TRUE;

    for (j = 0; j < NUM_STATS; j++)
    {
        template.evs[j] = rMon->evs[j];
        template.ivs[j] = rMon->ivs[j];
    }

    for (j = 0; j < MAX_MON_MOVES; j++)
    {
        template.moves[j] = rMon->moves[j];
    }

    CreateMonFromTemplate(mon, &template);
}

static void CB2_RB_Init(void);
static void CB2_RB_Main(void);
static void VBlankCB_RB(void);
static void CB2_RB_ReturnFromSummary(void);
static void Task_RB_Input(u8 taskId);
static void Task_RB_ExitToField(u8 taskId);
static void Task_RB_ExitToSummary(u8 taskId);

// ---------------------------------------------------------------------
// Dati
// ---------------------------------------------------------------------
static u16 RB_TeamIdAtRow(u32 row)
{
    u16 teamId;

    if (row == 0 || row > sRB.numTeams)
        return RB_NO_TEAM;
    if (sRB.category == CATEGORY_ALL)
        teamId = row - 1;
    else
        teamId = sRentalCategories[sRB.category].teamIndices[row - 1];
    if (teamId >= TOTAL_RENTAL_TEAMS)
        return RB_NO_TEAM;
    return teamId;
}

static u16 RB_CurrentTeam(void)
{
    return RB_TeamIdAtRow(sRB.cursor);
}

static u16 RB_RandomTeam(void)
{
    if (sRB.numTeams == 0)
        return RB_NO_TEAM;
    return RB_TeamIdAtRow(1 + (Random() % sRB.numTeams));
}

static bool32 RB_MonValid(u16 teamId, u32 slot)
{
    if (teamId >= TOTAL_RENTAL_TEAMS || slot >= RB_TEAM_SLOTS)
        return FALSE;
    return sRentalTeams[teamId].mons[slot].species != SPECIES_NONE;
}

// ---------------------------------------------------------------------
// Icone
// ---------------------------------------------------------------------
static void RB_FreeIcons(void)
{
    u32 i;

    for (i = 0; i < RB_TEAM_SLOTS; i++)
    {
        if (sRB.itemSpriteIds[i] < MAX_SPRITES)
            DestroySprite(&gSprites[sRB.itemSpriteIds[i]]);
        if (sRB.monIconSpriteIds[i] < MAX_SPRITES)
            FreeAndDestroyMonIconSprite(&gSprites[sRB.monIconSpriteIds[i]]);
        sRB.itemSpriteIds[i] = SPRITE_NONE;
        sRB.monIconSpriteIds[i] = SPRITE_NONE;
    }
}

// Solo il Pokémon selezionato nell'anteprima saltella.
static void RB_UpdateBounce(void)
{
    u32 i;

    for (i = 0; i < RB_TEAM_SLOTS; i++)
    {
        u32 spriteId = sRB.monIconSpriteIds[i];
        if (spriteId >= MAX_SPRITES)
            continue;
        gSprites[spriteId].x2 = 0;
        gSprites[spriteId].y2 = 0;
        if (sRB.focus == FOCUS_PREVIEW_MONS && i == sRB.monIdx)
            gSprites[spriteId].callback = SpriteCB_BounceRentalMonIcon;
        else
            gSprites[spriteId].callback = SpriteCB_MonIcon;
    }
}

static void RB_CreateIcons(void)
{
    u32 i;
    u16 teamId = RB_CurrentTeam();

    RB_FreeIcons();
    if (teamId == RB_NO_TEAM)
        return;

    for (i = 0; i < RB_TEAM_SLOTS; i++)
    {
        const struct PresetRentalMon *rMon = &sRentalTeams[teamId].mons[i];
        u32 spriteId;

        if (rMon->species == SPECIES_NONE)
            continue;
        spriteId = CreateMonIcon(rMon->species, SpriteCB_MonIcon, sRB_IconCoords[i][0], sRB_IconCoords[i][1], 0, 0);
        if (spriteId >= MAX_SPRITES)
            continue;
        gSprites[spriteId].oam.priority = 0;
        gSprites[spriteId].subpriority = 2;
        sRB.monIconSpriteIds[i] = spriteId;

        if (rMon->item != ITEM_NONE)
        {
            u32 itemSpriteId = CreateSprite(&sSpriteTemplate_RentalHeldItem, sRB_IconCoords[i][0] + 9, sRB_IconCoords[i][1] + 8, 1);
            if (itemSpriteId < MAX_SPRITES)
            {
                gSprites[itemSpriteId].oam.priority = 0;
                gSprites[itemSpriteId].data[7] = spriteId;
                StartSpriteAnim(&gSprites[itemSpriteId], ItemIsMail(rMon->item));
                sRB.itemSpriteIds[i] = itemSpriteId;
            }
        }
    }
    RB_UpdateBounce();
}

// ---------------------------------------------------------------------
// Disegno
// ---------------------------------------------------------------------
// Sceglie il font più largo che sta in maxWidth pixel.
static u32 RB_FitFont(const u8 *str, u32 maxWidth, u32 font1, u32 font2, u32 font3)
{
    if (GetStringWidth(font1, str, 0) <= maxWidth)
        return font1;
    if (GetStringWidth(font2, str, 0) <= maxWidth)
        return font2;
    return font3;
}

static void RB_DrawHeader(void)
{
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(TEXT_COLOR_WHITE));
    if (sRB.focus == FOCUS_PREVIEW_MONS)
    {
        u32 font = RB_FitFont(sText_KeyHelpPreview, 212, FONT_NORMAL, FONT_NARROW, FONT_NARROWER);
        AddTextPrinterParameterized3(WIN_HEADER, font, 6, 1, sRB_ColRed, TEXT_SKIP_DRAW, sText_KeyHelpPreview);
    }
    else
    {
        u8 *ptr = StringCopy(gStringVar4, sText_HeaderPrefix);
        u32 font;

        if (sRB.category == CATEGORY_ALL)
            StringCopy(ptr, sText_AllTeamsCategory);
        else
            StringCopy(ptr, sRentalCategories[sRB.category].name);
        font = RB_FitFont(gStringVar4, 212, FONT_NORMAL, FONT_NARROW, FONT_NARROWER);
        AddTextPrinterParameterized3(WIN_HEADER, font, 6, 1, sRB_ColDark, TEXT_SKIP_DRAW, gStringVar4);
    }
    CopyWindowToVram(WIN_HEADER, COPYWIN_GFX);
}

static void RB_DrawList(void)
{
    u32 i;
    const u8 *cursorColors = (sRB.focus == FOCUS_TEAM_LIST) ? sRB_ColRed : sRB_ColGray;

    FillWindowPixelBuffer(WIN_TEAM_LIST, PIXEL_FILL(TEXT_COLOR_WHITE));
    for (i = 0; i < RB_ROWS; i++)
    {
        u32 row = sRB.scroll + i;
        u16 teamId;
        const u8 *name;
        u32 font;

        if (row > sRB.numTeams)
            break;
        teamId = RB_TeamIdAtRow(row);
        if (row == 0)
            name = sText_RandomTeamOption;
        else if (teamId == RB_NO_TEAM)
            name = sText_UnknownTeam;
        else
            name = sRentalTeams[teamId].listName;

        font = RB_FitFont(name, 104, FONT_NORMAL, FONT_NARROW, FONT_NARROWER);
        AddTextPrinterParameterized3(WIN_TEAM_LIST, font, 10, RB_ROW_H * i, sRB_ColDark, TEXT_SKIP_DRAW, name);
        if (row == sRB.cursor)
            AddTextPrinterParameterized3(WIN_TEAM_LIST, FONT_NORMAL, 0, RB_ROW_H * i, cursorColors, TEXT_SKIP_DRAW, sText_RB_Cursor);
    }
    // Indicatori "c'è altro sopra / sotto"
    if (sRB.scroll > 0)
        AddTextPrinterParameterized3(WIN_TEAM_LIST, FONT_SMALL, 118, 0, sRB_ColRed, TEXT_SKIP_DRAW, sText_RB_Up);
    if (sRB.scroll + RB_ROWS <= sRB.numTeams)
        AddTextPrinterParameterized3(WIN_TEAM_LIST, FONT_SMALL, 118, RB_ROW_H * (RB_ROWS - 1) + 2, sRB_ColRed, TEXT_SKIP_DRAW, sText_RB_Down);
    CopyWindowToVram(WIN_TEAM_LIST, COPYWIN_GFX);
}

static void RB_DrawPreview(void)
{
    u16 teamId = RB_CurrentTeam();

    FillWindowPixelBuffer(WIN_PREVIEW, PIXEL_FILL(TEXT_COLOR_WHITE));
    if (teamId == RB_NO_TEAM)
    {
        AddTextPrinterParameterized3(WIN_PREVIEW, FONT_NORMAL, 16, 2, sRB_ColDark, TEXT_SKIP_DRAW, sText_CasualeHeader);
        AddTextPrinterParameterized3(WIN_PREVIEW, FONT_SMALL, 4, 26, sRB_ColDark, TEXT_SKIP_DRAW, sText_CasualeDescription);
        AddTextPrinterParameterized3(WIN_PREVIEW, FONT_SMALL, 4, 100, sRB_ColDark, TEXT_SKIP_DRAW, sText_KeyHelpRandom);
    }
    else if (sRB.focus == FOCUS_PREVIEW_MONS && RB_MonValid(teamId, sRB.monIdx))
    {
        const struct PresetRentalMon *rMon = &sRentalTeams[teamId].mons[sRB.monIdx];
        s32 cx = sRB_IconCoords[sRB.monIdx][0] - RB_PREVIEW_X;
        s32 cy = sRB_IconCoords[sRB.monIdx][1] - RB_PREVIEW_Y;
        u8 *ptr;
        u32 font;

        // Riquadro rosso dietro al Pokémon selezionato (le icone sono sprite, stanno sopra).
        FillWindowPixelRect(WIN_PREVIEW, PIXEL_FILL(TEXT_COLOR_RED), cx - 16, cy - 13, 32, 28);
        FillWindowPixelRect(WIN_PREVIEW, PIXEL_FILL(TEXT_COLOR_LIGHT_RED), cx - 14, cy - 11, 28, 24);

        // "(n/6) Specie"
        ptr = StringCopy(gStringVar4, sText_MonIdxOpen);
        ptr = ConvertIntToDecimalStringN(ptr, sRB.monIdx + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
        ptr = StringCopy(ptr, sText_MonIdxSep);
        StringCopy(ptr, GetSpeciesName(rMon->species));
        font = RB_FitFont(gStringVar4, 84, FONT_SMALL, FONT_SMALL_NARROW, FONT_SMALL_NARROWER);
        AddTextPrinterParameterized3(WIN_PREVIEW, font, 2, 0, sRB_ColDark, TEXT_SKIP_DRAW, gStringVar4);

        // "Str: strumento"
        if (rMon->item != ITEM_NONE)
        {
            ptr = StringCopy(gStringVar4, sText_StrPrefix);
            CopyItemName(rMon->item, ptr);
        }
        else
        {
            StringCopy(gStringVar4, sText_StrNone);
        }
        font = RB_FitFont(gStringVar4, 84, FONT_SMALL, FONT_SMALL_NARROW, FONT_SMALL_NARROWER);
        AddTextPrinterParameterized3(WIN_PREVIEW, font, 2, 100, sRB_ColDark, TEXT_SKIP_DRAW, gStringVar4);
    }
    else
    {
        u32 font = RB_FitFont(sText_KeyHelpList, 84, FONT_SMALL, FONT_SMALL_NARROW, FONT_SMALL_NARROWER);
        AddTextPrinterParameterized3(WIN_PREVIEW, FONT_NORMAL, 12, 0, sRB_ColDark, TEXT_SKIP_DRAW, sText_AnteprimaHeader);
        AddTextPrinterParameterized3(WIN_PREVIEW, font, 2, 100, sRB_ColDark, TEXT_SKIP_DRAW, sText_KeyHelpList);
    }
    CopyWindowToVram(WIN_PREVIEW, COPYWIN_GFX);
}

// Il cursore della lista è cambiato: nuove icone e nuova anteprima.
static void RB_OnCursorMoved(void)
{
    PlaySE(SE_SELECT);
    RB_CreateIcons();
    RB_DrawList();
    RB_DrawPreview();
}

// È cambiato il fuoco (lista <-> anteprima) o il Pokémon selezionato.
static void RB_OnFocusChanged(void)
{
    PlaySE(SE_SELECT);
    RB_UpdateBounce();
    RB_DrawHeader();
    RB_DrawList();
    RB_DrawPreview();
}

// ---------------------------------------------------------------------
// Ciclo della schermata
// ---------------------------------------------------------------------
static void VBlankCB_RB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_RB_Main(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void CB2_RB_Init(void)
{
    u32 i;

    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        SetHBlankCallback(NULL);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        ScanlineEffect_Stop();
        ResetPaletteFade();
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        ClearScheduledBgCopiesToVram();
        for (i = 0; i < RB_TEAM_SLOTS; i++)
        {
            sRB.monIconSpriteIds[i] = SPRITE_NONE;
            sRB.itemSpriteIds[i] = SPRITE_NONE;
        }
        gMain.state++;
        break;
    case 2:
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sRB_BgTemplates, ARRAY_COUNT(sRB_BgTemplates));
        memset(sRB_Tilemap, 0, sizeof(sRB_Tilemap));
        SetBgTilemapBuffer(0, sRB_Tilemap); // statico: le finestre non allocano il tilemap
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        InitWindows(sRB_WinTemplates);
        DeactivateAllTextPrinters();
        gMain.state++;
        break;
    case 3:
        LoadUserWindowBorderGfx(WIN_HEADER, RB_FRAME_TILE, BG_PLTT_ID(RB_FRAME_PAL));
        LoadPalette(gStandardMenuPalette, BG_PLTT_ID(RB_TEXT_PAL), PLTT_SIZE_4BPP);
        LoadMonIconPalettes();
        LoadHeldItemIcons();
        gMain.state++;
        break;
    case 4:
        // DrawStdFrame... riempie la finestra di bianco: il contenuto va disegnato dopo.
        DrawStdFrameWithCustomTileAndPalette(WIN_HEADER, FALSE, RB_FRAME_TILE, RB_FRAME_PAL);
        DrawStdFrameWithCustomTileAndPalette(WIN_TEAM_LIST, FALSE, RB_FRAME_TILE, RB_FRAME_PAL);
        DrawStdFrameWithCustomTileAndPalette(WIN_PREVIEW, FALSE, RB_FRAME_TILE, RB_FRAME_PAL);
        RB_CreateIcons();
        RB_DrawHeader();
        RB_DrawList();
        RB_DrawPreview();
        CopyBgTilemapBufferToVram(0);
        gMain.state++;
        break;
    default:
        ShowBg(0);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetBackdropFromColor(RGB(8, 12, 18));
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_RB);
        SetMainCallback2(CB2_RB_Main);
        CreateTask(Task_RB_Input, 0);
        break;
    }
}

// Libera tutto ciò che la schermata ha creato. Dopo questa chiamata non resta nulla sull'heap.
static void RB_Teardown(void)
{
    SetVBlankCallback(NULL);
    RB_FreeIcons();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ClearScheduledBgCopiesToVram();
    FreeAllWindowBuffers();
    UnsetBgTilemapBuffer(0);
}

static void RB_StartExitToField(u8 taskId, u16 result)
{
    PlaySE(SE_SELECT);
    sRB.result = result;
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_RB_ExitToField;
}

static void Task_RB_ExitToField(u8 taskId)
{
    if (gPaletteFade.active)
        return;
    DestroyTask(taskId);
    RB_Teardown();
    gSpecialVar_Result = sRB.result;
    gFieldCallback = FieldCB_ContinueScriptHandleMusic;
    SetMainCallback2(CB2_ReturnToField);
}

static void Task_RB_ExitToSummary(u8 taskId)
{
    u32 i, start = 0;
    u16 teamId;

    if (gPaletteFade.active)
        return;
    DestroyTask(taskId);

    teamId = RB_CurrentTeam();
    sRentalSummaryCount = 0;
    for (i = 0; i < RB_TEAM_SLOTS; i++)
    {
        if (!RB_MonValid(teamId, i))
            continue;
        BuildRentalPokemon(&sRentalSummaryMons[sRentalSummaryCount], &sRentalTeams[teamId].mons[i]);
        sRentalSummarySlots[sRentalSummaryCount] = i;
        if (i == sRB.monIdx)
            start = sRentalSummaryCount;
        sRentalSummaryCount++;
    }

    RB_Teardown();
    if (sRentalSummaryCount == 0)
    {
        // Non dovrebbe succedere (si entra nell'anteprima solo con un Pokémon valido).
        sRB.focus = FOCUS_TEAM_LIST;
        SetMainCallback2(CB2_RB_Init);
        return;
    }
    // LOCK_MOVES: sola lettura, niente rinomina / ricorda-mosse.
    ShowPokemonSummaryScreen(SUMMARY_MODE_LOCK_MOVES, sRentalSummaryMons, start, sRentalSummaryCount - 1, CB2_RB_ReturnFromSummary);
}

static void CB2_RB_ReturnFromSummary(void)
{
    if (gLastViewedMonIndex < sRentalSummaryCount)
        sRB.monIdx = sRentalSummarySlots[gLastViewedMonIndex];
    sRB.focus = FOCUS_PREVIEW_MONS;
    SetMainCallback2(CB2_RB_Init);
}

// ---------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------
static void RB_MoveCursorTo(s32 newCursor)
{
    if (newCursor < 0)
        newCursor = 0;
    if (newCursor > sRB.numTeams)
        newCursor = sRB.numTeams;
    if (newCursor == sRB.cursor)
        return;
    sRB.cursor = newCursor;
    if (sRB.cursor < sRB.scroll)
        sRB.scroll = sRB.cursor;
    else if (sRB.cursor >= sRB.scroll + RB_ROWS)
        sRB.scroll = sRB.cursor - (RB_ROWS - 1);
    RB_OnCursorMoved();
}

static void RB_SelectMon(u32 slot)
{
    if (!RB_MonValid(RB_CurrentTeam(), slot) || slot == sRB.monIdx)
        return;
    sRB.monIdx = slot;
    RB_OnFocusChanged();
}

static void RB_BackToList(void)
{
    sRB.focus = FOCUS_TEAM_LIST;
    RB_OnFocusChanged();
}

static void Task_RB_Input(u8 taskId)
{
    u16 teamId;

    if (gPaletteFade.active)
        return;

    teamId = RB_CurrentTeam();

    if (sRB.focus == FOCUS_PREVIEW_MONS)
    {
        if (teamId == RB_NO_TEAM || !RB_MonValid(teamId, sRB.monIdx))
        {
            RB_BackToList();
        }
        else if (JOY_NEW(B_BUTTON))
        {
            RB_BackToList();
        }
        else if (JOY_NEW(A_BUTTON))
        {
            PlaySE(SE_SELECT);
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            gTasks[taskId].func = Task_RB_ExitToSummary;
        }
        else if (JOY_NEW(START_BUTTON))
        {
            RB_StartExitToField(taskId, teamId);
        }
        else if (JOY_NEW(DPAD_UP))
        {
            if (sRB.monIdx >= 2)
                RB_SelectMon(sRB.monIdx - 2);
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            if (sRB.monIdx + 2 < RB_TEAM_SLOTS)
                RB_SelectMon(sRB.monIdx + 2);
        }
        else if (JOY_NEW(DPAD_RIGHT))
        {
            if ((sRB.monIdx % 2) == 0)
                RB_SelectMon(sRB.monIdx + 1);
        }
        else if (JOY_NEW(DPAD_LEFT))
        {
            if ((sRB.monIdx % 2) == 1 && RB_MonValid(teamId, sRB.monIdx - 1))
                RB_SelectMon(sRB.monIdx - 1);
            else
                RB_BackToList();
        }
        return;
    }

    // Fuoco sulla lista
    if (JOY_REPEAT(DPAD_UP))
    {
        RB_MoveCursorTo((s32)sRB.cursor - 1);
    }
    else if (JOY_REPEAT(DPAD_DOWN))
    {
        RB_MoveCursorTo((s32)sRB.cursor + 1);
    }
    else if (JOY_REPEAT(L_BUTTON))
    {
        RB_MoveCursorTo((s32)sRB.cursor - RB_ROWS);
    }
    else if (JOY_REPEAT(R_BUTTON))
    {
        RB_MoveCursorTo((s32)sRB.cursor + RB_ROWS);
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        u32 slot;
        // Entra nell'anteprima sul primo Pokémon del team.
        for (slot = 0; slot < RB_TEAM_SLOTS; slot++)
        {
            if (RB_MonValid(teamId, slot))
            {
                sRB.monIdx = slot;
                sRB.focus = FOCUS_PREVIEW_MONS;
                RB_OnFocusChanged();
                break;
            }
        }
    }
    else if (JOY_NEW(A_BUTTON))
    {
        RB_StartExitToField(taskId, (teamId == RB_NO_TEAM) ? RB_RandomTeam() : teamId);
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        RB_StartExitToField(taskId, RB_RandomTeam());
    }
    else if (JOY_NEW(B_BUTTON))
    {
        RB_StartExitToField(taskId, RB_NO_TEAM);
    }
}

// ---------------------------------------------------------------------
// Ingresso dallo script (special con waitstate)
// VAR_0x8004 = categoria; al ritorno VAR_RESULT = team scelto, 0xFFFF = annullato.
// ---------------------------------------------------------------------
static void Task_RB_WaitFadeAndOpen(u8 taskId)
{
    if (gPaletteFade.active)
        return;
    DestroyTask(taskId);
    CleanupOverworldWindowsAndTilemaps();
    SetMainCallback2(CB2_RB_Init);
}

void OpenRentalTeamBrowser(void)
{
    u32 i;
    u16 cat = gSpecialVar_0x8004;

    if (cat > CATEGORY_ALL)
        cat = CATEGORY_ALL;

    memset(&sRB, 0, sizeof(sRB));
    sRB.category = cat;
    sRB.numTeams = (cat == CATEGORY_ALL) ? TOTAL_RENTAL_TEAMS : sRentalCategories[cat].count;
    sRB.result = RB_NO_TEAM;
    sRB.focus = FOCUS_TEAM_LIST;
    for (i = 0; i < RB_TEAM_SLOTS; i++)
    {
        sRB.monIconSpriteIds[i] = SPRITE_NONE;
        sRB.itemSpriteIds[i] = SPRITE_NONE;
    }

    LockPlayerFieldControls();
    CreateTask(Task_RB_WaitFadeAndOpen, 10);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
}

void ChooseRandomRentalTeam(void)
{
    u16 teamId = Random() % TOTAL_RENTAL_TEAMS;
    gSpecialVar_Result = teamId;
    StringCopy(gStringVar1, sRentalTeams[teamId].name);
}

void BufferRentalTeamName(void)
{
    u16 teamId = gSpecialVar_Result;
    if (teamId < TOTAL_RENTAL_TEAMS)
    {
        u8 i;
        for (i = 0; i < 47; i++)
        {
            gStringVar1[i] = sRentalTeams[teamId].name[i];
            if (gStringVar1[i] == EOS || gStringVar1[i] == 0x00)
                break;
        }
        gStringVar1[i] = EOS;
    }
    else
    {
        StringCopy(gStringVar1, sText_UnknownTeam);
    }
}

u16 GetRentalTeamMonSpecies(u16 teamId, u8 monIndex)
{
    if (teamId >= TOTAL_RENTAL_TEAMS || monIndex >= 6)
        return SPECIES_NONE;
    return sRentalTeams[teamId].mons[monIndex].species;
}

void GiveSelectedRentalTeam(void)
{
    u16 teamId = gSpecialVar_0x8005;
    u32 i;
    u32 partyCount;

    if (teamId >= TOTAL_RENTAL_TEAMS)
        return;

    // La squadra attuale (es. ELIA) va nel box per fare posto a quella a noleggio.
    // Se il PC è pieno il Pokémon resta in squadra.
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *partyMon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(partyMon, MON_DATA_SPECIES) == SPECIES_NONE)
            continue;
        if (CopyMonToPC(partyMon) == MON_GIVEN_TO_PC)
            ZeroMonData(partyMon);
    }
    CompactPartySlots();
    partyCount = CalculatePlayerPartyCount();

    for (i = 0; i < 6; i++)
    {
        struct Pokemon mon;
        const struct PresetRentalMon *rMon = &sRentalTeams[teamId].mons[i];
        if (rMon->species == SPECIES_NONE)
            continue;

        BuildRentalPokemon(&mon, rMon);
        if (partyCount < PARTY_SIZE)
            gParties[B_TRAINER_PLAYER][partyCount++] = mon;
        else
            CopyMonToPC(&mon);
    }
    CalculatePlayerPartyCount();
}

void PopulateRentalCategoryTeams(void)
{
    u8 category = gSpecialVar_0x8004;
    u32 i;

    if (category >= NUM_RENTAL_CATEGORIES)
        return;

    for (i = 0; i < sRentalCategories[category].count; i++)
    {
        u16 teamId = sRentalCategories[category].teamIndices[i];
        struct ListMenuItem item;
        u8 *nameBuf = Alloc(100);
        StringCopy(nameBuf, sRentalTeams[teamId].name);
        item.name = nameBuf;
        item.id = teamId;
        MultichoiceDynamic_PushElement(item);
    }
}

void BufferRentalTeamPreview(void)
{
    u16 teamId = gSpecialVar_0x8005;
    u8 *ptr;
    u32 i;

    if (teamId >= TOTAL_RENTAL_TEAMS)
        return;

    StringCopy(gStringVar1, sRentalTeams[teamId].name);

    ptr = gStringVar2;
    *ptr = EOS;

    for (i = 0; i < 6; i++)
    {
        u16 species = sRentalTeams[teamId].mons[i].species;
        if (species != SPECIES_NONE)
        {
            ptr = StringAppend(ptr, COMPOUND_STRING("- "));
            ptr = StringAppend(ptr, GetSpeciesName(species));
            if (i < 5 && sRentalTeams[teamId].mons[i + 1].species != SPECIES_NONE)
                ptr = StringAppend(ptr, COMPOUND_STRING("\n"));
        }
    }
}

void GiveRandomMonOfSpecies(void)
{
    u16 species = gSpecialVar_Result;
    struct Pokemon mon;
    struct PokemonTemplate template = {0};

    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        species = SPECIES_PIKACHU;

    template.species = species;
    template.level = 50;
    template.heldItem = ITEM_NONE;
    template.nature = NATURE_RANDOM;
    template.gender = MON_GENDER_RANDOM;
    template.origin = GIFTMON_ORIGIN;
    template.doNotUseDefaultShinyness = FALSE;
    template.abilityNum = 0;
    template.moves[0] = MOVE_RANDOM_TEACHABLE;
    template.moves[1] = MOVE_RANDOM_TEACHABLE;
    template.moves[2] = MOVE_RANDOM_TEACHABLE;
    template.moves[3] = MOVE_RANDOM_TEACHABLE;

    CreateMonFromTemplate(&mon, &template);
    CopyMonToPC(&mon);
    StringCopy(gStringVar1, GetSpeciesName(species));
}

void ChooseRandomMonSpecies(void)
{
    enum Species species;
    do
    {
        species = (Random() % (NUM_SPECIES - 1)) + 1;
    } while (!IsSpeciesEnabled(species) || species == SPECIES_NONE || species == SPECIES_EGG);

    gSpecialVar_Result = species;
}

void PopulateGenerationSpecies(void)
{
    u8 gen = gSpecialVar_0x8004;
    u16 startSpecies = 1;
    u16 endSpecies = 151;
    u16 i;

    switch (gen)
    {
    case 1: startSpecies = SPECIES_BULBASAUR; endSpecies = SPECIES_MEW; break;
    case 2: startSpecies = SPECIES_CHIKORITA; endSpecies = SPECIES_CELEBI; break;
    case 3: startSpecies = SPECIES_TREECKO;   endSpecies = SPECIES_DEOXYS; break;
    case 4: startSpecies = SPECIES_TURTWIG;   endSpecies = SPECIES_ARCEUS; break;
    case 5: startSpecies = SPECIES_VICTINI;   endSpecies = SPECIES_GENESECT; break;
    case 6: startSpecies = SPECIES_CHESPIN;   endSpecies = SPECIES_VOLCANION; break;
    case 7: startSpecies = SPECIES_ROWLET;    endSpecies = SPECIES_MELMETAL; break;
    case 8: startSpecies = SPECIES_GROOKEY;   endSpecies = SPECIES_ENAMORUS; break;
    case 9: default: startSpecies = SPECIES_SPRIGATITO; endSpecies = 1025; break;
    }

    for (i = startSpecies; i <= endSpecies; i++)
    {
        if (IsSpeciesEnabled(i))
        {
            struct ListMenuItem item;
            u8 *nameBuf = Alloc(POKEMON_NAME_LENGTH + 1);
            StringCopy(nameBuf, GetSpeciesName(i));
            item.name = nameBuf;
            item.id = i;
            MultichoiceDynamic_PushElement(item);
        }
    }
}

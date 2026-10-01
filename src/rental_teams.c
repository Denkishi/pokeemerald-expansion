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
#include "pokemon_summary_screen.h"
#include "random.h"
#include "rental_teams.h"
#include "scanline_effect.h"
#include "script.h"
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

#include "decompress.h"

#define TAG_RENTAL_ARROW_CURSOR 2002

static const u16 sRedInterface_Pal[]    = INCGFX_U16("graphics/interface/red.pal", ".gbapal");
static const u32 sArrowCursor_Gfx[]     = INCGFX_U32("graphics/interface/arrow_cursor.png", ".4bpp.smol");

static const struct SpritePalette sArrowCursorSpritePal = { sRedInterface_Pal, TAG_RENTAL_ARROW_CURSOR };
static const struct CompressedSpriteSheet sArrowCursorSpriteSheet = { sArrowCursor_Gfx, 0x80, TAG_RENTAL_ARROW_CURSOR };

static const struct OamData sOamData_ArrowCursor =
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

static const struct SpriteTemplate sSpriteTemplate_ArrowCursor =
{
    .tileTag = TAG_RENTAL_ARROW_CURSOR,
    .paletteTag = TAG_RENTAL_ARROW_CURSOR,
    .oam = &sOamData_ArrowCursor,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};


enum {
    WIN_HEADER,
    WIN_TEAM_LIST,
    WIN_PREVIEW,
    WIN_COUNT
};

enum {
    FOCUS_TEAM_LIST,
    FOCUS_PREVIEW_MONS
};

static const u8 sText_CasualeHeader[] = _("CASUALE");
static const u8 sText_CasualeDescription[] = _("Premendo A\nriceverai un\nteam a sorpresa\ntra quelli di\nquesta lista!");
static const u8 sText_KeyHelpRandom[] = _("A:Scegli  B:Esci");
static const u8 sText_AnteprimaHeader[] = _("ANTEPRIMA");
static const u8 sText_KeyHelpList[] = _("A:Scegli  {DPAD_RIGHT}:Info");
static const u8 sText_KeyHelpPreview[] = _("A:Info  B:Torna");
static const u8 sText_RandomTeamOption[] = _("  Team Casuale");
static const u8 sText_AllTeamsCategory[] = _("TUTTI I TEAM (76)");
static const u8 sText_HeaderPrefix[] = _("TEAM A NOLEGGIO - ");
static const u8 sText_UnknownTeam[] = _("Team Sconosciuto");
static const u8 sText_StrNone[] = _("Str: Nessuno");
static const u8 sText_StrPrefix[] = _("Str: ");
static const u8 sText_MonIdxOpen[] = _("(");
static const u8 sText_MonIdxSep[] = _("/6) ");

struct RentalTeamsMenuData
{
    u8 listTaskId;
    u8 scrollArrowsTaskId;
    u8 arrowSpriteId;
    u16 scrollOffset;
    u16 selectedRow;
    u8 monIconSpriteIds[6];
    u8 itemSpriteIds[6];
    u16 category;
    u16 numTeams;
    u16 numItems;
    struct ListMenuItem *menuItems;
    s32 currentTeamId;
    u8 focusMode;
    u8 previewMonIdx;
};

static EWRAM_DATA struct RentalTeamsMenuData *sRentalTeamsData = NULL;
static EWRAM_DATA struct Pokemon *sRentalSummaryMons = NULL;

static const struct WindowTemplate sRentalTeamsWindowTemplates[] =
{
    [WIN_HEADER] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 28,
        .height = 2,
        .paletteNum = 15,
        .baseBlock = 0x0001,
    },
    [WIN_TEAM_LIST] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 4,
        .width = 16,
        .height = 14,
        .paletteNum = 15,
        .baseBlock = 0x0039,
    },
    [WIN_PREVIEW] = {
        .bg = 0,
        .tilemapLeft = 18,
        .tilemapTop = 4,
        .width = 11,
        .height = 14,
        .paletteNum = 15,
        .baseBlock = 0x0119,
    },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sRentalTeamsBgTemplates[] =
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
    {
        .bg = 1,
        .charBaseIndex = 2,
        .mapBaseIndex = 29,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0,
    },
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

static void SetPreviewMonBouncing(u8 monIdx, bool8 bounce)
{
    if (monIdx >= 6)
        return;
    u8 spriteId = sRentalTeamsData->monIconSpriteIds[monIdx];
    if (spriteId != SPRITE_NONE && spriteId < MAX_SPRITES)
    {
        if (bounce)
        {
            gSprites[spriteId].x2 = 0;
            gSprites[spriteId].y2 = 0;
            gSprites[spriteId].callback = SpriteCB_BounceRentalMonIcon;
        }
        else
        {
            gSprites[spriteId].x2 = 0;
            gSprites[spriteId].y2 = 0;
            gSprites[spriteId].callback = SpriteCB_MonIcon;
        }
    }
}

static void VBlankCB_RentalTeams(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_RentalTeamsMain(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    RunTextPrinters();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void FreeRentalTeamsResources(void)
{
    if (sRentalSummaryMons != NULL)
    {
        Free(sRentalSummaryMons);
        sRentalSummaryMons = NULL;
    }
    if (sRentalTeamsData != NULL)
    {
        if (sRentalTeamsData->menuItems != NULL)
            Free(sRentalTeamsData->menuItems);
        Free(sRentalTeamsData);
        sRentalTeamsData = NULL;
    }
    FreeAllWindowBuffers();
}

static void FreePreviewSprites(void)
{
    u8 i;
    for (i = 0; i < 6; i++)
    {
        if (sRentalTeamsData->monIconSpriteIds[i] != SPRITE_NONE)
        {
            FreeAndDestroyMonIconSprite(&gSprites[sRentalTeamsData->monIconSpriteIds[i]]);
            sRentalTeamsData->monIconSpriteIds[i] = SPRITE_NONE;
        }
        if (sRentalTeamsData->itemSpriteIds[i] != SPRITE_NONE)
        {
            DestroySprite(&gSprites[sRentalTeamsData->itemSpriteIds[i]]);
            sRentalTeamsData->itemSpriteIds[i] = SPRITE_NONE;
        }
    }
}

static void UpdatePreviewText(s32 itemIndex)
{
    if (itemIndex < 0 || itemIndex >= TOTAL_RENTAL_TEAMS)
        return;

    FillWindowPixelBuffer(WIN_PREVIEW, PIXEL_FILL(1));

    if (sRentalTeamsData->focusMode == FOCUS_PREVIEW_MONS)
    {
        u8 monIdx = sRentalTeamsData->previewMonIdx;
        const struct PresetRentalMon *rMon = &sRentalTeams[itemIndex].mons[monIdx];
        u8 str[32];

        ConvertIntToDecimalStringN(gStringVar1, monIdx + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
        StringCopy(str, sText_MonIdxOpen);
        StringAppend(str, gStringVar1);
        StringAppend(str, sText_MonIdxSep);
        StringAppend(str, GetSpeciesName(rMon->species));
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_SMALL, str, 2, 0, TEXT_SKIP_DRAW, NULL);

        if (rMon->item != ITEM_NONE)
        {
            StringCopy(str, sText_StrPrefix);
            CopyItemName(rMon->item, gStringVar1);
            StringAppend(str, gStringVar1);
        }
        else
        {
            StringCopy(str, sText_StrNone);
        }
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_SMALL, str, 2, 100, TEXT_SKIP_DRAW, NULL);

        // Draw arrow next to the selected sprite
        {
            static const s16 sIconCoords[6][2] = {
                { 168, 58 }, { 206, 58 },
                { 168, 86 }, { 206, 86 },
                { 168, 114 }, { 206, 114 },
            };
            static const u8 sText_RightArrow[] = _("{RIGHT_ARROW}");
            u32 arrowX = (sIconCoords[monIdx][0] - 136) - 16;
            u32 arrowY = (sIconCoords[monIdx][1] - 24) - 8;
            AddTextPrinterParameterized(WIN_PREVIEW, FONT_NORMAL, sText_RightArrow, arrowX, arrowY, TEXT_SKIP_DRAW, NULL);
        }
    }
    else
    {
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_NORMAL, sText_AnteprimaHeader, 12, 2, TEXT_SKIP_DRAW, NULL);
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_SMALL, sText_KeyHelpList, 2, 100, TEXT_SKIP_DRAW, NULL);
    }

    CopyWindowToVram(WIN_PREVIEW, COPYWIN_GFX);
}

static void UpdateTeamPreview(s32 itemIndex)
{
    u8 i;
    if (sRentalTeamsData->focusMode == FOCUS_TEAM_LIST && sRentalTeamsData->arrowSpriteId < MAX_SPRITES)
        gSprites[sRentalTeamsData->arrowSpriteId].invisible = TRUE;
    FreePreviewSprites();

    FillWindowPixelBuffer(WIN_PREVIEW, PIXEL_FILL(1));

    if (itemIndex == ITEM_ID_RANDOM)
    {
        sRentalTeamsData->focusMode = FOCUS_TEAM_LIST;
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_NORMAL, sText_CasualeHeader, 16, 2, TEXT_SKIP_DRAW, NULL);
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_SMALL, sText_CasualeDescription, 4, 26, TEXT_SKIP_DRAW, NULL);
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_SMALL, sText_KeyHelpRandom, 4, 100, TEXT_SKIP_DRAW, NULL);
    }
    else if (itemIndex >= 0 && itemIndex < TOTAL_RENTAL_TEAMS)
    {
        static const s16 sIconCoords[6][2] = {
            { 168, 58 }, { 206, 58 },
            { 168, 86 }, { 206, 86 },
            { 168, 114 }, { 206, 114 },
        };

        for (i = 0; i < 6; i++)
        {
            u16 species = sRentalTeams[itemIndex].mons[i].species;
            if (species != SPECIES_NONE)
            {
                u8 spriteId = CreateMonIcon(species, SpriteCB_MonIcon, sIconCoords[i][0], sIconCoords[i][1], 0, 0);
                if (spriteId < MAX_SPRITES)
                {
                    gSprites[spriteId].oam.priority = 0;
                    gSprites[spriteId].subpriority = 2;
                    sRentalTeamsData->monIconSpriteIds[i] = spriteId;

                    enum Item item = sRentalTeams[itemIndex].mons[i].item;
                    if (item != ITEM_NONE)
                    {
                        u8 itemSpriteId = CreateSprite(&sSpriteTemplate_RentalHeldItem, sIconCoords[i][0] + 9, sIconCoords[i][1] + 8, 1);
                        if (itemSpriteId < MAX_SPRITES)
                        {
                            gSprites[itemSpriteId].oam.priority = 0;
                            gSprites[itemSpriteId].data[7] = spriteId;
                            StartSpriteAnim(&gSprites[itemSpriteId], ItemIsMail(item));
                            sRentalTeamsData->itemSpriteIds[i] = itemSpriteId;
                        }
                    }
                }
            }
        }

        UpdatePreviewText(itemIndex);
    }

    CopyWindowToVram(WIN_PREVIEW, COPYWIN_GFX);
}

static void UpdatePreviewFocus(u8 newIdx)
{
    if (newIdx >= 6)
        return;
    s32 teamId = sRentalTeamsData->currentTeamId;
    if (teamId < 0 || teamId >= TOTAL_RENTAL_TEAMS)
        return;
    if (sRentalTeams[teamId].mons[newIdx].species == SPECIES_NONE)
        return;

    PlaySE(SE_SELECT);
    SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, FALSE);
    sRentalTeamsData->previewMonIdx = newIdx;
    SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, TRUE);
    UpdatePreviewText(teamId);
}

static void RentalTeams_MoveCursor(s32 itemIndex, bool8 onInit, struct ListMenu *list)
{
    if (!onInit)
        PlaySE(SE_SELECT);
    sRentalTeamsData->currentTeamId = itemIndex;
    UpdateTeamPreview(itemIndex);
}

static void Task_RentalTeams_FadeOutAndExit(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        FreePreviewSprites();
        if (sRentalTeamsData->scrollArrowsTaskId != TASK_NONE)
        {
            RemoveScrollIndicatorArrowPair(sRentalTeamsData->scrollArrowsTaskId);
            sRentalTeamsData->scrollArrowsTaskId = TASK_NONE;
    sRentalTeamsData->arrowSpriteId = MAX_SPRITES;
        }
        DestroyListMenuTask(sRentalTeamsData->listTaskId, NULL, NULL);
        FreeMonIconPalettes();
        FreeSpriteTilesByTag(TAG_RENTAL_HELD_ITEM);
        FreeSpritePaletteByTag(TAG_RENTAL_HELD_ITEM);
        FreeSpriteTilesByTag(TAG_RENTAL_ARROW_CURSOR);
        FreeSpritePaletteByTag(TAG_RENTAL_ARROW_CURSOR);
        SetVBlankCallback(NULL);
        FreeRentalTeamsResources();
        DestroyTask(taskId);
        SetMainCallback1(CB1_Overworld);
        gFieldCallback = FieldCB_ContinueScriptHandleMusic;
        gMain.state = 0;
        ResetBgsAndClearDma3BusyFlags(0);
        SetMainCallback2(CB2_ReturnToField);
    }
}

static void BuildRentalPokemon(struct Pokemon *mon, const struct PresetRentalMon *rMon)
{
    u32 j;
    struct PokemonTemplate template = {0};
    template.species = rMon->species;
    template.heldItem = rMon->item;
    template.level = rMon->level;
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

static void CB2_ReturnToRentalTeamsFromSummary(void);

static void Task_RentalTeams_FadeOutToSummary(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        u8 monIdx = gTasks[taskId].data[0];
        u8 numValidMons = gTasks[taskId].data[1];

        FreePreviewSprites();

        if (sRentalTeamsData->scrollArrowsTaskId != TASK_NONE)
        {
            RemoveScrollIndicatorArrowPair(sRentalTeamsData->scrollArrowsTaskId);
            sRentalTeamsData->scrollArrowsTaskId = TASK_NONE;
    sRentalTeamsData->arrowSpriteId = MAX_SPRITES;
        }
        DestroyListMenuTask(sRentalTeamsData->listTaskId, NULL, NULL);
        FreeMonIconPalettes();
        FreeSpriteTilesByTag(TAG_RENTAL_HELD_ITEM);
        FreeSpritePaletteByTag(TAG_RENTAL_HELD_ITEM);
        FreeAllWindowBuffers();
        DestroyTask(taskId);

        ShowPokemonSummaryScreen(SUMMARY_MODE_LOCK_MOVES, sRentalSummaryMons, monIdx, numValidMons - 1, CB2_ReturnToRentalTeamsFromSummary);
    }
}

static void OpenSummaryScreenForRentalTeam(u8 taskId, u16 teamId, u8 monIdx)
{
    u32 i;
    u8 numValidMons = 0;

    if (sRentalSummaryMons != NULL)
        Free(sRentalSummaryMons);

    sRentalSummaryMons = AllocZeroed(sizeof(struct Pokemon) * 6);

    for (i = 0; i < 6; i++)
    {
        const struct PresetRentalMon *rMon = &sRentalTeams[teamId].mons[i];
        if (rMon->species == SPECIES_NONE)
            continue;
        BuildRentalPokemon(&sRentalSummaryMons[numValidMons], rMon);
        numValidMons++;
    }

    if (numValidMons == 0)
    {
        Free(sRentalSummaryMons);
        sRentalSummaryMons = NULL;
        return;
    }

    if (monIdx >= numValidMons)
        monIdx = 0;

    gTasks[taskId].data[0] = monIdx;
    gTasks[taskId].data[1] = numValidMons;
    gTasks[taskId].func = Task_RentalTeams_FadeOutToSummary;
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
}

static void CB2_InitRentalTeams(void);

static void CB2_ReturnToRentalTeamsFromSummary(void)
{
    if (sRentalSummaryMons != NULL)
    {
        Free(sRentalSummaryMons);
        sRentalSummaryMons = NULL;
    }
    if (sRentalTeamsData != NULL)
    {
        sRentalTeamsData->previewMonIdx = gLastViewedMonIndex;
        sRentalTeamsData->focusMode = FOCUS_PREVIEW_MONS;
    }
    SetMainCallback2(CB2_InitRentalTeams);
}

static void Task_RentalTeams_HandleInput(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    if (sRentalTeamsData->focusMode == FOCUS_PREVIEW_MONS)
    {
        s32 teamId = sRentalTeamsData->currentTeamId;
        if (teamId < 0 || teamId >= TOTAL_RENTAL_TEAMS)
        {
            sRentalTeamsData->focusMode = FOCUS_TEAM_LIST;
            return;
        }

        if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, FALSE);
            sRentalTeamsData->focusMode = FOCUS_TEAM_LIST;
            if (sRentalTeamsData->arrowSpriteId < MAX_SPRITES)
                gSprites[sRentalTeamsData->arrowSpriteId].invisible = TRUE;
            UpdatePreviewText(teamId);
            return;
        }

        if (JOY_NEW(START_BUTTON))
        {
            PlaySE(SE_SELECT);
            gSpecialVar_Result = teamId;
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            gTasks[taskId].func = Task_RentalTeams_FadeOutAndExit;
            return;
        }

        if (JOY_NEW(A_BUTTON))
        {
            PlaySE(SE_SELECT);
            ListMenuGetScrollAndRow(sRentalTeamsData->listTaskId, &sRentalTeamsData->scrollOffset, &sRentalTeamsData->selectedRow);
            OpenSummaryScreenForRentalTeam(taskId, teamId, sRentalTeamsData->previewMonIdx);
            return;
        }

        if (JOY_NEW(DPAD_UP))
        {
            if (sRentalTeamsData->previewMonIdx >= 2)
                UpdatePreviewFocus(sRentalTeamsData->previewMonIdx - 2);
            return;
        }

        if (JOY_NEW(DPAD_DOWN))
        {
            if (sRentalTeamsData->previewMonIdx <= 3)
                UpdatePreviewFocus(sRentalTeamsData->previewMonIdx + 2);
            return;
        }

        if (JOY_NEW(DPAD_LEFT))
        {
            if ((sRentalTeamsData->previewMonIdx % 2) == 1)
            {
                UpdatePreviewFocus(sRentalTeamsData->previewMonIdx - 1);
            }
            else
            {
                PlaySE(SE_SELECT);
                SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, FALSE);
                sRentalTeamsData->focusMode = FOCUS_TEAM_LIST;
                UpdatePreviewText(teamId);
            }
            return;
        }

        if (JOY_NEW(DPAD_RIGHT))
        {
            if ((sRentalTeamsData->previewMonIdx % 2) == 0)
                UpdatePreviewFocus(sRentalTeamsData->previewMonIdx + 1);
            return;
        }

        return;
    }

    if (JOY_NEW(DPAD_RIGHT))
    {
        s32 teamId = sRentalTeamsData->currentTeamId;
        if (teamId >= 0 && teamId < TOTAL_RENTAL_TEAMS)
        {
            PlaySE(SE_SELECT);
            sRentalTeamsData->focusMode = FOCUS_PREVIEW_MONS;
            sRentalTeamsData->previewMonIdx = 0;
            SetPreviewMonBouncing(0, TRUE);
            UpdatePreviewText(teamId);
            return;
        }
    }

    if (JOY_NEW(SELECT_BUTTON))
    {
        PlaySE(SE_SELECT);
        u16 randIdx = Random() % sRentalTeamsData->numTeams;
        u16 teamId;
        if (sRentalTeamsData->category == CATEGORY_ALL)
            teamId = randIdx;
        else
            teamId = sRentalCategories[sRentalTeamsData->category].teamIndices[randIdx];

        gSpecialVar_Result = teamId;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_RentalTeams_FadeOutAndExit;
        return;
    }

    s32 input = ListMenu_ProcessInput(sRentalTeamsData->listTaskId);
    if (input == LIST_CANCEL)
    {
        PlaySE(SE_SELECT);
        gSpecialVar_Result = 0xFFFF;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_RentalTeams_FadeOutAndExit;
    }
    else if (input != LIST_NOTHING_CHOSEN)
    {
        PlaySE(SE_SELECT);
        if (input == ITEM_ID_RANDOM)
        {
            u16 randIdx = Random() % sRentalTeamsData->numTeams;
            u16 teamId;
            if (sRentalTeamsData->category == CATEGORY_ALL)
                teamId = randIdx;
            else
                teamId = sRentalCategories[sRentalTeamsData->category].teamIndices[randIdx];
            gSpecialVar_Result = teamId;
        }
        else
        {
            gSpecialVar_Result = input;
        }
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_RentalTeams_FadeOutAndExit;
    }
}

static void BuildRentalTeamsList(void)
{
    u16 i;
    u16 cat = sRentalTeamsData->category;
    u16 count;

    if (cat == CATEGORY_ALL)
        count = TOTAL_RENTAL_TEAMS;
    else
        count = sRentalCategories[cat].count;

    sRentalTeamsData->numTeams = count;
    sRentalTeamsData->numItems = count + 1;

    if (sRentalTeamsData->menuItems != NULL)
        Free(sRentalTeamsData->menuItems);
    sRentalTeamsData->menuItems = AllocZeroed(sizeof(struct ListMenuItem) * (sRentalTeamsData->numItems + 1));

    // First item is always Random Team
    sRentalTeamsData->menuItems[0].name = sText_RandomTeamOption;
    sRentalTeamsData->menuItems[0].id = ITEM_ID_RANDOM;

    for (i = 0; i < count; i++)
    {
        u16 teamId;
        if (cat == CATEGORY_ALL)
            teamId = i;
        else
            teamId = sRentalCategories[cat].teamIndices[i];

        sRentalTeamsData->menuItems[i + 1].name = sRentalTeams[teamId].listName;
        sRentalTeamsData->menuItems[i + 1].id = teamId;
    }

    gMultiuseListMenuTemplate.items = sRentalTeamsData->menuItems;
    gMultiuseListMenuTemplate.totalItems = sRentalTeamsData->numItems;
    gMultiuseListMenuTemplate.windowId = WIN_TEAM_LIST;
    gMultiuseListMenuTemplate.header_X = 0;
    gMultiuseListMenuTemplate.item_X = 8;
    gMultiuseListMenuTemplate.cursor_X = 0;
    gMultiuseListMenuTemplate.upText_Y = 1;
    gMultiuseListMenuTemplate.cursorPal = 2;
    gMultiuseListMenuTemplate.fillValue = 1;
    gMultiuseListMenuTemplate.cursorShadowPal = 3;
    gMultiuseListMenuTemplate.lettersSpacing = 0;
    gMultiuseListMenuTemplate.itemVerticalPadding = 0;
    gMultiuseListMenuTemplate.scrollMultiple = LIST_MULTIPLE_SCROLL_L_R;
    gMultiuseListMenuTemplate.fontId = FONT_NORMAL;
    gMultiuseListMenuTemplate.cursorKind = CURSOR_BLACK_ARROW;
    gMultiuseListMenuTemplate.maxShowed = 7;
    gMultiuseListMenuTemplate.moveCursorFunc = RentalTeams_MoveCursor;
    gMultiuseListMenuTemplate.itemPrintFunc = NULL;
}

static void CB2_InitRentalTeams(void)
{
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetVBlankCallback(NULL);
    ResetVramOamAndBgCntRegs();
    ResetBgsAndClearDma3BusyFlags(0);
    DeactivateAllTextPrinters();
    ResetPaletteFade();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ClearScheduledBgCopiesToVram();

    InitBgsFromTemplates(0, sRentalTeamsBgTemplates, ARRAY_COUNT(sRentalTeamsBgTemplates));
    ResetAllBgsCoordinates();

    FillBgTilemapBufferRect_Palette0(0, 0, 0, 0, 32, 32);
    FillBgTilemapBufferRect_Palette0(1, 0, 0, 0, 32, 32);
    CopyBgTilemapBufferToVram(0);
    CopyBgTilemapBufferToVram(1);

    
    InitWindows(sRentalTeamsWindowTemplates);

    LoadCompressedSpriteSheet(&sArrowCursorSpriteSheet);
    LoadSpritePalette(&sArrowCursorSpritePal);
    sRentalTeamsData->arrowSpriteId = CreateSprite(&sSpriteTemplate_ArrowCursor, 0, 0, 0);
    gSprites[sRentalTeamsData->arrowSpriteId].invisible = TRUE;
    gSprites[sRentalTeamsData->arrowSpriteId].subpriority = 0;

    LoadUserWindowBorderGfx(WIN_HEADER, 0x0200, BG_PLTT_ID(14));
    LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);

    DrawStdFrameWithCustomTileAndPalette(WIN_HEADER, FALSE, 0x0200, 14);
    DrawStdFrameWithCustomTileAndPalette(WIN_TEAM_LIST, FALSE, 0x0200, 14);
    DrawStdFrameWithCustomTileAndPalette(WIN_PREVIEW, FALSE, 0x0200, 14);

    PutWindowTilemap(WIN_HEADER);
    PutWindowTilemap(WIN_TEAM_LIST);
    PutWindowTilemap(WIN_PREVIEW);

    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
    FillWindowPixelBuffer(WIN_TEAM_LIST, PIXEL_FILL(1));
    FillWindowPixelBuffer(WIN_PREVIEW, PIXEL_FILL(1));

    {
        u8 titleBuf[64];
        const u8 *catName;
        if (sRentalTeamsData->category == CATEGORY_ALL)
            catName = sText_AllTeamsCategory;
        else
            catName = sRentalCategories[sRentalTeamsData->category].name;

        StringCopy(titleBuf, sText_HeaderPrefix);
        StringAppend(titleBuf, catName);
        AddTextPrinterParameterized(WIN_HEADER, FONT_NORMAL, titleBuf, 6, 2, TEXT_SKIP_DRAW, NULL);
    }

    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
    CopyWindowToVram(WIN_TEAM_LIST, COPYWIN_FULL);
    CopyWindowToVram(WIN_PREVIEW, COPYWIN_FULL);
    ScheduleBgCopyTilemapToVram(0);

    LoadMonIconPalettes();
    LoadHeldItemIcons();
    BuildRentalTeamsList();

    sRentalTeamsData->listTaskId = ListMenuInit(&gMultiuseListMenuTemplate, sRentalTeamsData->scrollOffset, sRentalTeamsData->selectedRow);

    sRentalTeamsData->scrollArrowsTaskId = AddScrollIndicatorArrowPairParameterized(
        SCROLL_ARROW_UP,
        76,
        28,
        146,
        (sRentalTeamsData->numItems > 7) ? (sRentalTeamsData->numItems - 7) : 0,
        TAG_RENTAL_SCROLL_ARROW,
        TAG_RENTAL_SCROLL_ARROW,
        &sRentalTeamsData->scrollOffset
    );

    if (sRentalTeamsData->focusMode == FOCUS_PREVIEW_MONS)
    {
        SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, TRUE);
        UpdatePreviewText(sRentalTeamsData->currentTeamId);
    }

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON);
    ShowBg(0);
    ShowBg(1);
    SetBackdropFromColor(RGB(8, 12, 18));
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    CreateTask(Task_RentalTeams_HandleInput, 10);

    SetVBlankCallback(VBlankCB_RentalTeams);
    SetMainCallback2(CB2_RentalTeamsMain);
}

static void Task_RentalTeams_WaitForFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback1(CB1_Overworld);
        SetMainCallback2(CB2_InitRentalTeams);
        gFieldCallback = FieldCB_ContinueScriptHandleMusic;
        DestroyTask(taskId);
    }
}

void OpenRentalTeamBrowser(void)
{
    u16 cat = gSpecialVar_0x8004;
    if (cat > CATEGORY_ALL)
        cat = CATEGORY_ALL;

    sRentalTeamsData = AllocZeroed(sizeof(struct RentalTeamsMenuData));
    sRentalTeamsData->category = cat;
    sRentalTeamsData->scrollArrowsTaskId = TASK_NONE;
    sRentalTeamsData->arrowSpriteId = MAX_SPRITES;
    sRentalTeamsData->currentTeamId = ITEM_ID_RANDOM;
    sRentalTeamsData->focusMode = FOCUS_TEAM_LIST;
    sRentalTeamsData->previewMonIdx = 0;

    u8 i;
    for (i = 0; i < 6; i++)
    {
        sRentalTeamsData->monIconSpriteIds[i] = SPRITE_NONE;
        sRentalTeamsData->itemSpriteIds[i] = SPRITE_NONE;
    }

    LockPlayerFieldControls();
    CreateTask(Task_RentalTeams_WaitForFadeOut, 10);
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

void GiveSelectedRentalTeam(void)
{
    u16 teamId = gSpecialVar_Result;
    u32 i;

    if (teamId >= TOTAL_RENTAL_TEAMS)
        return;

    for (i = 0; i < 6; i++)
    {
        struct Pokemon mon;
        const struct PresetRentalMon *rMon = &sRentalTeams[teamId].mons[i];
        if (rMon->species == SPECIES_NONE)
            continue;

        BuildRentalPokemon(&mon, rMon);
        CopyMonToPC(&mon);
    }
}

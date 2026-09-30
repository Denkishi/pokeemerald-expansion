#include "global.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "task.h"
#include "bg.h"
#include "gpu_regs.h"
#include "window.h"
#include "text.h"
#include "text_window.h"
#include "string_util.h"
#include "malloc.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "item.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "script.h"
#include "overworld.h"
#include "battle_main.h"
#include "custom_team_builder.h"

enum {
    SCREEN_TEAM_OVERVIEW,
    SCREEN_SELECT_SPECIES,
    SCREEN_SELECT_MOVES,
    SCREEN_SELECT_NATURE,
    SCREEN_SELECT_ABILITY,
    SCREEN_SELECT_ITEM,
    SCREEN_SELECT_TERA,
    SCREEN_SET_EVS,
    SCREEN_SET_IVS,
};

struct CustomBuilderMon {
    u16 species;
    u16 moves[4];
    u8 nature;
    u8 abilityNum;
    u16 item;
    u8 teraType;
    u8 evs[6];
    u8 ivs[6];
};

struct CustomBuilderData {
    u8 state;
    u8 teamCount;
    u8 currentMonIndex;
    struct CustomBuilderMon team[6];
    
    u8 windowIdLeft;
    u8 windowIdRight;
    
    u16 listCursorPos;
    u16 listScrollOffset;
    u16 listItems[2000];
    u16 listCount;
    
    u8 moveSlotIndex;
    u8 currentStatIndex;
};

EWRAM_DATA static struct CustomBuilderData *sCustomBuilderData = NULL;

static void Task_CustomTeamBuilderMain(u8 taskId);
static void DrawCurrentScreen(void);

static const struct WindowTemplate sWindowTemplates[] = {
    {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 3,
        .width = 16,
        .height = 14,
        .paletteNum = 15,
        .baseBlock = 1,
    },
    {
        .bg = 0,
        .tilemapLeft = 18,
        .tilemapTop = 3,
        .width = 12,
        .height = 14,
        .paletteNum = 15,
        .baseBlock = 1 + (16 * 14),
    },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sBgTemplates[] = {
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    }
};

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void BuildTeamAndExit(u8 taskId)
{
    u8 i;
    ZeroFillPlayerParty();
    gPlayerPartyCount = sCustomBuilderData->teamCount;
    for (i = 0; i < sCustomBuilderData->teamCount; i++) {
        struct Pokemon *mon = &gPlayerParty[i];
        struct CustomBuilderMon *cbMon = &sCustomBuilderData->team[i];
        
        CreateMon(mon, cbMon->species, 50, 31, FALSE, 0, 0, 0);
        
        SetMonData(mon, MON_DATA_MOVE1, &cbMon->moves[0]);
        SetMonData(mon, MON_DATA_MOVE2, &cbMon->moves[1]);
        SetMonData(mon, MON_DATA_MOVE3, &cbMon->moves[2]);
        SetMonData(mon, MON_DATA_MOVE4, &cbMon->moves[3]);
        
        SetMonData(mon, MON_DATA_NATURE, &cbMon->nature);
        SetMonData(mon, MON_DATA_ABILITY_NUM, &cbMon->abilityNum);
        SetMonData(mon, MON_DATA_HELD_ITEM, &cbMon->item);
        SetMonData(mon, MON_DATA_TERA_TYPE, &cbMon->teraType);
        
        SetMonData(mon, MON_DATA_HP_EV, &cbMon->evs[0]);
        SetMonData(mon, MON_DATA_ATK_EV, &cbMon->evs[1]);
        SetMonData(mon, MON_DATA_DEF_EV, &cbMon->evs[2]);
        SetMonData(mon, MON_DATA_SPEED_EV, &cbMon->evs[3]);
        SetMonData(mon, MON_DATA_SPATK_EV, &cbMon->evs[4]);
        SetMonData(mon, MON_DATA_SPDEF_EV, &cbMon->evs[5]);
        
        SetMonData(mon, MON_DATA_HP_IV, &cbMon->ivs[0]);
        SetMonData(mon, MON_DATA_ATK_IV, &cbMon->ivs[1]);
        SetMonData(mon, MON_DATA_DEF_IV, &cbMon->ivs[2]);
        SetMonData(mon, MON_DATA_SPEED_IV, &cbMon->ivs[3]);
        SetMonData(mon, MON_DATA_SPATK_IV, &cbMon->ivs[4]);
        SetMonData(mon, MON_DATA_SPDEF_IV, &cbMon->ivs[5]);
        
        CalculateMonStats(mon);
    }
    
    gSpecialVar_Result = sCustomBuilderData->teamCount;
    
    FREE_AND_SET_NULL(sCustomBuilderData);
    DestroyTask(taskId);
    ScriptContext_Enable();
    SetMainCallback2(CB2_ReturnToField);
}

void OpenCustomTeamBuilder(void)
{
    u8 taskId;
    
    sCustomBuilderData = AllocZeroed(sizeof(struct CustomBuilderData));
    sCustomBuilderData->state = SCREEN_TEAM_OVERVIEW;
    sCustomBuilderData->teamCount = 0;
    
    ResetBgsAndClearDma3BusyFlags(TRUE);
    InitBgsFromTemplates(0, sBgTemplates, 1);
    ResetVramOamAndBgCntRegs();
    ResetAllBgsCoordinates();
    
    sCustomBuilderData->windowIdLeft = AddWindow(&sWindowTemplates[0]);
    sCustomBuilderData->windowIdRight = AddWindow(&sWindowTemplates[1]);
    
    SetVBlankCallback(VBlankCB);
    
    taskId = CreateTask(Task_CustomTeamBuilderMain, 0);
    DrawCurrentScreen();
}

static void PopulateSpeciesList(void)
{
    u16 i;
    sCustomBuilderData->listCount = 0;
    for (i = 1; i < NUM_SPECIES; i++) {
        if (i == SPECIES_EGG) continue;
        if (gSpeciesInfo[i].baseHP == 0) continue;
        sCustomBuilderData->listItems[sCustomBuilderData->listCount++] = i;
    }
}

static void PopulateMovesList(u16 species)
{
    u16 i = 0;
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(species);
    sCustomBuilderData->listCount = 0;
    
    if (learnset != NULL) {
        while (learnset[i].move != LEVEL_UP_MOVE_END && sCustomBuilderData->listCount < 64) {
            if (learnset[i].level <= 50) {
                sCustomBuilderData->listItems[sCustomBuilderData->listCount++] = learnset[i].move;
            }
            i++;
        }
    }
}

static void DrawCurrentScreen(void)
{
    FillWindowPixelBuffer(sCustomBuilderData->windowIdLeft, PIXEL_FILL(0));
    FillWindowPixelBuffer(sCustomBuilderData->windowIdRight, PIXEL_FILL(0));
    
    // Draw based on state
    if (sCustomBuilderData->state == SCREEN_TEAM_OVERVIEW) {
        // Draw overview
    }
    
    PutWindowTilemap(sCustomBuilderData->windowIdLeft);
    PutWindowTilemap(sCustomBuilderData->windowIdRight);
    CopyWindowToVram(sCustomBuilderData->windowIdLeft, 3);
    CopyWindowToVram(sCustomBuilderData->windowIdRight, 3);
}

static void Task_CustomTeamBuilderMain(u8 taskId)
{
    // Simplified logic: just add a Pikachu and exit immediately for this mockup
    // A fully functional implementation would handle all screens and DPAD/A/B inputs
    
    if (sCustomBuilderData->teamCount == 0) {
        struct CustomBuilderMon *mon = &sCustomBuilderData->team[0];
        u8 i;
        
        mon->species = SPECIES_PIKACHU;
        mon->moves[0] = MOVE_THUNDERBOLT;
        mon->moves[1] = MOVE_QUICK_ATTACK;
        mon->moves[2] = MOVE_IRON_TAIL;
        mon->moves[3] = MOVE_VOLT_TACKLE;
        mon->nature = NATURE_TIMID;
        mon->abilityNum = 0;
        mon->item = ITEM_LIGHT_BALL;
        mon->teraType = TYPE_ELECTRIC;
        
        for (i = 0; i < 6; i++) {
            mon->evs[i] = 0;
            mon->ivs[i] = 31;
        }
        mon->evs[3] = 252; // Speed
        mon->evs[4] = 252; // SpAtk
        
        sCustomBuilderData->teamCount = 1;
    }
    
    BuildTeamAndExit(taskId);
}

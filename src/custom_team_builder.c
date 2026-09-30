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
#include "script.h"
#include "overworld.h"
#include "battle_main.h"
#include "custom_team_builder.h"

static void Task_CustomTeamBuilderInit(u8 taskId);

void OpenCustomTeamBuilder(void)
{
    CreateTask(Task_CustomTeamBuilderInit, 0);
}

static void Task_CustomTeamBuilderInit(u8 taskId)
{
    // Dummy stub that just exits for now
    ScriptContext_Enable();
    SetMainCallback2(CB2_ReturnToField);
    DestroyTask(taskId);
}

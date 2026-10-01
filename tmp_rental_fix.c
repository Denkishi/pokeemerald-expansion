// Fix for rental team crash in Pokemon Emerald
// This patch corrects the callback handling in CB2_ReturnToRentalTeamsFromSummary
// to prevent crashes when navigating team categories

#include "global.h"
#include "rental_teams.h"
#include "task.h"
#include "malloc.h"
#include "string.h"
#include "constants/field_effects.h"
#include "field_effect.h"
#include "field_player_avatar.h"
#include "field_control_avatar.h"
#include "event_scripts.h"
#include "fieldmap.h"
#include "script.h"

// Rebuild CB2_ReturnToRentalTeamsFromSummary to properly handle callbacks
static void CB2_ReturnToRentalTeamsFromSummary(void)
{
    // Clear the summary monsters if they exist
    if (sRentalSummaryMons != NULL)
    {
        Free(sRentalSummaryMons);
        sRentalSummaryMons = NULL;
    }
    
    // Ensure the rental data is valid before trying to update it
    if (sRentalTeamsData != NULL)
    {
        // Verify that we're in the preview mode before updating
        if (sRentalTeamsData->focusMode == FOCUS_PREVIEW_MONS)
        {
            // Safely update preview mode and index after returning from summary
            sRentalTeamsData->focusMode = FOCUS_TEAM_LIST;
        }
    }
    
    // Reset the field callback properly
    gFieldCallback = FieldCB_ContinueScriptHandleMusic;
    SetMainCallback2(CB2_InitRentalTeams);
}
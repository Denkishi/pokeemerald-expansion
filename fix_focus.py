import re

with open('src/rental_teams.c', 'r') as f:
    text = f.read()

text = text.replace('StringAppend(str, _("{RIGHT_ARROW}"));', '')

old_focus = '''static void UpdatePreviewFocus(u8 newIdx)
{
    if (newIdx >= 6)
        return;
    s32 teamId = sRentalTeamsData->currentTeamId;
    if (teamId < 0 || teamId >= TOTAL_RENTAL_TEAMS)
        return;
    if (sRentalTeams[teamId].mons[newIdx].species == SPECIES_NONE)
        return;

    SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, FALSE);
    sRentalTeamsData->previewMonIdx = newIdx;
    SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, TRUE);
    UpdatePreviewText(teamId);
}'''

new_focus = '''static void UpdatePreviewFocus(u8 newIdx)
{
    if (newIdx >= 6)
        return;
    s32 teamId = sRentalTeamsData->currentTeamId;
    if (teamId < 0 || teamId >= TOTAL_RENTAL_TEAMS)
        return;
    if (sRentalTeams[teamId].mons[newIdx].species == SPECIES_NONE)
        return;

    SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, FALSE);
    sRentalTeamsData->previewMonIdx = newIdx;
    SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, TRUE);
    
    if (sRentalTeamsData->arrowSpriteId < MAX_SPRITES)
    {
        gSprites[sRentalTeamsData->arrowSpriteId].invisible = FALSE;
        gSprites[sRentalTeamsData->arrowSpriteId].x = sIconCoords[newIdx][0] - 8;
        gSprites[sRentalTeamsData->arrowSpriteId].y = sIconCoords[newIdx][1] - 4;
    }
    
    UpdatePreviewText(teamId);
}'''

text = text.replace(old_focus, new_focus)

old_focus_list = '''        if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, FALSE);
            sRentalTeamsData->focusMode = FOCUS_TEAM_LIST;
            UpdatePreviewText(teamId);
            return;
        }'''

new_focus_list = '''        if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            SetPreviewMonBouncing(sRentalTeamsData->previewMonIdx, FALSE);
            sRentalTeamsData->focusMode = FOCUS_TEAM_LIST;
            if (sRentalTeamsData->arrowSpriteId < MAX_SPRITES)
                gSprites[sRentalTeamsData->arrowSpriteId].invisible = TRUE;
            UpdatePreviewText(teamId);
            return;
        }'''
text = text.replace(old_focus_list, new_focus_list)

with open('src/rental_teams.c', 'w') as f:
    f.write(text)

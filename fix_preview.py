import re

with open('src/rental_teams.c', 'r') as f:
    text = f.read()

old_preview = '''static void UpdateTeamPreview(s32 itemIndex)
{
    u8 i;'''

new_preview = '''static void UpdateTeamPreview(s32 itemIndex)
{
    u8 i;
    if (sRentalTeamsData->focusMode == FOCUS_TEAM_LIST && sRentalTeamsData->arrowSpriteId < MAX_SPRITES)
        gSprites[sRentalTeamsData->arrowSpriteId].invisible = TRUE;'''

text = text.replace(old_preview, new_preview)

with open('src/rental_teams.c', 'w') as f:
    f.write(text)

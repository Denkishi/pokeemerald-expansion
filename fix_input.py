import re

with open('src/custom_team_builder.c', 'r') as f:
    text = f.read()

# Fix B button logic
old_b_moves = '''        case SCREEN_SELECT_MOVES:
            // Go to nature selection if species is set, or back to overview
            if (sData->team[sData->curTeamSlot].species != SPECIES_NONE)
                CTB_EnterScreen(SCREEN_SELECT_NATURE);
            else
            {
                sData->listCursor = sData->curTeamSlot;
                CTB_EnterScreen(SCREEN_TEAM_OVERVIEW);
            }
            PlaySE(SE_SELECT);
            break;'''

new_b_moves = '''        case SCREEN_SELECT_MOVES:
            sData->filterLetter = 0;
            CTB_PopulateSpecies();
            CTB_EnterScreen(SCREEN_SELECT_SPECIES);
            PlaySE(SE_SELECT);
            break;'''
text = text.replace(old_b_moves, new_b_moves)

# Fix A button on moves (advance to nature when full)
old_a_moves = '''                // Advance to next empty move slot or next screen
                {
                    u8 next = sData->curMoveSlot + 1;
                    if (next < MAX_MON_MOVES)
                        sData->curMoveSlot = next;
                }
                CTB_DrawScreen();'''
new_a_moves = '''                // Advance to next empty move slot or next screen
                {
                    u8 next = sData->curMoveSlot + 1;
                    if (next < MAX_MON_MOVES)
                        sData->curMoveSlot = next;
                    else
                        CTB_EnterScreen(SCREEN_SELECT_NATURE);
                }
                if (sData->screen == SCREEN_SELECT_MOVES)
                    CTB_DrawScreen();'''
text = text.replace(old_a_moves, new_a_moves)

with open('src/custom_team_builder.c', 'w') as f:
    f.write(text)

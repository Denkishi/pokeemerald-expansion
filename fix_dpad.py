import re

with open('src/custom_team_builder.c', 'r') as f:
    text = f.read()

old_dpad = '''    // --- DPAD_LEFT on move screen: switch move slot ---
    else if (JOY_NEW(DPAD_LEFT) && sData->screen == SCREEN_SELECT_MOVES)
    {
        if (sData->curMoveSlot > 0) sData->curMoveSlot--;
        CTB_DrawScreen();
        PlaySE(SE_SELECT);
    }
    else if (JOY_NEW(DPAD_RIGHT) && sData->screen == SCREEN_SELECT_MOVES)
    {
        if (sData->curMoveSlot < MAX_MON_MOVES - 1) sData->curMoveSlot++;
        CTB_DrawScreen();
        PlaySE(SE_SELECT);
    }'''

new_dpad = '''    // --- DPAD_LEFT ---
    else if (JOY_NEW(DPAD_LEFT) || (JOY_HELD(DPAD_LEFT) && sData->screen >= SCREEN_SET_EVS))
    {
        if (sData->screen == SCREEN_SELECT_MOVES)
        {
            if (sData->curMoveSlot > 0) sData->curMoveSlot--;
        }
        else if (sData->screen == SCREEN_SET_EVS)
        {
            struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];
            if (cm->evs[sData->curStatIdx] >= 4) cm->evs[sData->curStatIdx] -= 4;
            else cm->evs[sData->curStatIdx] = 0;
        }
        else if (sData->screen == SCREEN_SET_IVS)
        {
            struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];
            if (cm->ivs[sData->curStatIdx] > 0) cm->ivs[sData->curStatIdx]--;
        }
        else if (sData->screen == SCREEN_TEAM_OVERVIEW)
        {
            // Do nothing
            return;
        }
        CTB_DrawScreen();
        PlaySE(SE_SELECT);
    }
    // --- DPAD_RIGHT ---
    else if (JOY_NEW(DPAD_RIGHT) || (JOY_HELD(DPAD_RIGHT) && sData->screen >= SCREEN_SET_EVS))
    {
        if (sData->screen == SCREEN_SELECT_MOVES)
        {
            if (sData->curMoveSlot < MAX_MON_MOVES - 1) sData->curMoveSlot++;
        }
        else if (sData->screen == SCREEN_SET_EVS)
        {
            struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];
            u16 total = 0;
            u8 s;
            for (s = 0; s < NUM_STATS; s++) total += cm->evs[s];
            if (total + 4 <= EV_MAX_TOTAL && cm->evs[sData->curStatIdx] + 4 <= EV_MAX_PER_STAT)
                cm->evs[sData->curStatIdx] += 4;
        }
        else if (sData->screen == SCREEN_SET_IVS)
        {
            struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];
            if (cm->ivs[sData->curStatIdx] < 31) cm->ivs[sData->curStatIdx]++;
        }
        else if (sData->screen == SCREEN_TEAM_OVERVIEW)
        {
            return;
        }
        CTB_DrawScreen();
        PlaySE(SE_SELECT);
    }'''

text = text.replace(old_dpad, new_dpad)
with open('src/custom_team_builder.c', 'w') as f:
    f.write(text)

// Schermata "Info lotta" (tasto R nel menu azioni / mosse).
//  - squadre: icone di tutti i Pokémon; avversari non ancora visti = sagoma nera,
//    KO = grigi, in campo = barretta verde;
//  - modifiche alle statistiche dei Pokémon in campo;
//  - effetti di campo (meteo, terreni, stanze) e di lato (schermi, vento, trappole) con i turni;
//  - A su un Pokémon: dettagli. Per gli avversari solo ciò che è stato rivelato
//    (mosse usate, abilità attivata, strumento attivato).
//
// Nessuna Alloc propria: stato e tilemap sono statici, l'unico heap è quello delle finestre.
#include "global.h"
#include "battle.h"
#include "battle_controllers.h"
#include "battle_info.h"
#include "battle_main.h"
#include "battle_util.h"
#include "battle_util2.h"
#include "bg.h"
#include "data.h"
#include "gpu_regs.h"
#include "item.h"
#include "main.h"
#include "menu.h"
#include "move.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "reshow_battle_screen.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "config/custom.h"
#include "constants/battle.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// =====================================================================
// Dati raccolti durante la lotta
// =====================================================================
struct RevealedMon
{
    u16 moves[MAX_MON_MOVES];
    u16 ability;    // ABILITY_NONE = non ancora rivelata
    u16 item;       // ITEM_NONE = non ancora rivelato
};

enum
{
    SIDE_TIMER_REFLECT,
    SIDE_TIMER_LIGHTSCREEN,
    SIDE_TIMER_AURORA_VEIL,
    SIDE_TIMER_TAILWIND,
    SIDE_TIMER_SAFEGUARD,
    SIDE_TIMER_MIST,
    SIDE_TIMER_COUNT,
};

struct TimerMax
{
    u8 weather;
    u8 terrain;
    u8 trickRoom;
    u8 gravity;
    u8 magicRoom;
    u8 wonderRoom;
    u8 side[NUM_BATTLE_SIDES][SIDE_TIMER_COUNT];
};

EWRAM_DATA bool8 gBattleInfoRequested = FALSE;
static EWRAM_DATA struct RevealedMon sRevealed[PARTY_SIZE] = {0}; // squadra avversaria, per slot
static EWRAM_DATA struct TimerMax sTimerMax = {0};

void BattleInfo_ResetBattle(void)
{
    memset(sRevealed, 0, sizeof(sRevealed));
    memset(&sTimerMax, 0, sizeof(sTimerMax));
    gBattleInfoRequested = FALSE;
}

static struct RevealedMon *RevealedForBattler(enum BattlerId battler)
{
    u32 slot;

    if (battler >= MAX_BATTLERS_COUNT || IsOnPlayerSide(battler))
        return NULL;
    slot = gBattlerPartyIndexes[battler];
    if (slot >= PARTY_SIZE)
        return NULL;
    return &sRevealed[slot];
}

void BattleInfo_RecordMove(enum BattlerId battler, enum Move move)
{
    u32 i;
    struct RevealedMon *rev = RevealedForBattler(battler);

    if (rev == NULL || move == MOVE_NONE)
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (rev->moves[i] == move)
            return;
    }
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (rev->moves[i] == MOVE_NONE)
        {
            rev->moves[i] = move;
            return;
        }
    }
}

void BattleInfo_RecordAbility(enum BattlerId battler, enum Ability ability)
{
    struct RevealedMon *rev = RevealedForBattler(battler);

    if (rev != NULL && ability != ABILITY_NONE)
        rev->ability = ability;
}

void BattleInfo_RecordItem(enum BattlerId battler)
{
    struct RevealedMon *rev = RevealedForBattler(battler);
    enum Item item;

    if (rev == NULL)
        return;
    item = gBattleMons[battler].item;
    if (item == ITEM_NONE)
        item = gLastUsedItem; // già consumato (es. bacca)
    if (item != ITEM_NONE)
        rev->item = item;
}

// Durata massima: i timer del gioco contano solo i turni rimasti, quindi la ricavo
// la prima volta che vedo l'effetto (durata base oppure estesa da strumento).
static void UpdateTimerMax(u8 *max, bool32 active, u32 timer, u32 base, u32 extended)
{
    if (!active || timer == 0)
    {
        *max = 0;
        return;
    }
    if (*max == 0 || timer > *max)
    {
        if (timer <= base)
            *max = base;
        else if (timer <= extended)
            *max = extended;
        else
            *max = timer;
    }
}

// Chiamata a ogni apertura del menu azioni (una volta per turno o più).
void BattleInfo_SampleTimers(void)
{
    u32 side;

    UpdateTimerMax(&sTimerMax.weather, (gBattleWeather & B_WEATHER_ANY) != 0, gBattleStruct->weatherDuration, 5, 8);
    UpdateTimerMax(&sTimerMax.terrain, gFieldTimers.terrain != 0, gFieldTimers.terrainTimer, 5, 8);
    UpdateTimerMax(&sTimerMax.trickRoom, (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) != 0, gFieldTimers.trickRoomTimer, 5, 5);
    UpdateTimerMax(&sTimerMax.gravity, (gFieldStatuses & STATUS_FIELD_GRAVITY) != 0, gFieldTimers.gravityTimer, 5, 5);
    UpdateTimerMax(&sTimerMax.magicRoom, (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM) != 0, gFieldTimers.magicRoomTimer, 5, 5);
    UpdateTimerMax(&sTimerMax.wonderRoom, (gFieldStatuses & STATUS_FIELD_WONDER_ROOM) != 0, gFieldTimers.wonderRoomTimer, 5, 5);
    for (side = 0; side < NUM_BATTLE_SIDES; side++)
    {
        u32 st = gSideStatuses[side];
        UpdateTimerMax(&sTimerMax.side[side][SIDE_TIMER_REFLECT], (st & SIDE_STATUS_REFLECT) != 0, gSideTimers[side].reflectTimer, 5, 8);
        UpdateTimerMax(&sTimerMax.side[side][SIDE_TIMER_LIGHTSCREEN], (st & SIDE_STATUS_LIGHTSCREEN) != 0, gSideTimers[side].lightscreenTimer, 5, 8);
        UpdateTimerMax(&sTimerMax.side[side][SIDE_TIMER_AURORA_VEIL], (st & SIDE_STATUS_AURORA_VEIL) != 0, gSideTimers[side].auroraVeilTimer, 5, 8);
        UpdateTimerMax(&sTimerMax.side[side][SIDE_TIMER_TAILWIND], (st & SIDE_STATUS_TAILWIND) != 0, gSideTimers[side].tailwindTimer, 4, 4);
        UpdateTimerMax(&sTimerMax.side[side][SIDE_TIMER_SAFEGUARD], (st & SIDE_STATUS_SAFEGUARD) != 0, gSideTimers[side].safeguardTimer, 5, 5);
        UpdateTimerMax(&sTimerMax.side[side][SIDE_TIMER_MIST], (st & SIDE_STATUS_MIST) != 0, gSideTimers[side].mistTimer, 5, 5);
    }
}

// =====================================================================
// Schermata
// =====================================================================
enum
{
    WIN_FOE,
    WIN_MID,
    WIN_PLAYER,
};

enum
{
    VIEW_MAIN,
    VIEW_DETAIL,
};

enum
{
    BI_SIDE_FOE,
    BI_SIDE_PLAYER,
    BI_SIDE_COUNT,
};

// Righe selezionabili nei dettagli, disposte su 2 colonne.
enum
{
    DETAIL_ABILITY,
    DETAIL_ITEM,
    DETAIL_MOVE_1,
    DETAIL_MOVE_2,
    DETAIL_MOVE_3,
    DETAIL_MOVE_4,
    DETAIL_ROW_COUNT,
};

#define TAG_BI_BLACK    0xE7B0
#define TAG_BI_GREY     0xE7B1 // + indice palette icona (0..5)

#define ICON_FIRST_X    28
#define ICON_PITCH      36
#define STRIP_ICON_Y    24      // centro icona dentro la striscia
#define FOE_STRIP_Y     0
#define PLAYER_STRIP_Y  120
#define LINE_H          9
#define BAR_H           12      // altezza delle barre scure con il titolo
#define BI_FONT         FONT_SMALL_NARROW

struct BattleInfoState
{
    u8 view;
    u8 side;        // cursore: lato
    u8 slot;        // cursore: slot nella squadra
    u8 detailRow;
    u8 iconIds[BI_SIDE_COUNT][PARTY_SIZE];
};

static EWRAM_DATA struct BattleInfoState sBI = {0};
static EWRAM_DATA u16 sBI_Tilemap[BG_SCREEN_SIZE / 2] = {0};

static const u16 sBI_BlackPal[16] = {0};

// {sfondo, testo, ombra} sulla palette standard dei menu; sfondo 0 = trasparente.
static const u8 sColDark[]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sColWhite[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY};
static const u8 sColRed[]   = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};
static const u8 sColBlue[]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_BLUE, TEXT_COLOR_LIGHT_BLUE};
static const u8 sColGray[]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_TRANSPARENT};

static const u8 sText_Foe[]          = _("AVVERSARIO");
static const u8 sText_Player[]       = _("LA TUA SQUADRA");
static const u8 sText_HintMain[]     = _("A: Dettagli   B: Esci");
static const u8 sText_HintDetail[]   = _("B: Indietro");
static const u8 sText_OnField[]      = _("Barra verde = in campo");
static const u8 sText_StatsHeader[]  = _("MODIFICHE STATISTICHE");
static const u8 sText_FieldHeader[]  = _("CAMPO");
static const u8 sText_NoChanges[]    = _("nessuna modifica");
static const u8 sText_NoEffects[]    = _("Nessun effetto attivo");
static const u8 sText_More[]         = _("…");
static const u8 sText_Them[]         = _("Loro:");
static const u8 sText_Us[]           = _("Tu:");
static const u8 sText_None[]         = _("-");
static const u8 sText_Unknown[]      = _("???");
static const u8 sText_NotRevealed[]  = _("Non ancora rivelato.");
static const u8 sText_Space[]        = _(" ");
static const u8 sText_Slash[]        = _("/");
static const u8 sText_Times[]        = _(" x");
static const u8 sText_Up[]           = _("{UP_ARROW}");
static const u8 sText_Down[]         = _("{DOWN_ARROW}");
static const u8 sText_Cursor[]       = _("▶");
static const u8 sText_HP[]           = _("PS ");
static const u8 sText_Percent[]      = _("%");
static const u8 sText_KO[]           = _("KO");
static const u8 sText_Type[]         = _("Tipo: ");
static const u8 sText_Ability[]      = _("Ab: ");
static const u8 sText_Item[]         = _("Str: ");
static const u8 sText_NoItem[]       = _("nessuno");
static const u8 sText_Power[]        = _("  Pot. ");
static const u8 sText_Accuracy[]     = _("  Prec. ");
static const u8 sText_PP[]           = _("  PP ");

static const u8 sText_Slp[] = _(" SLP");
static const u8 sText_Psn[] = _(" PSN");
static const u8 sText_Brn[] = _(" BRN");
static const u8 sText_Frz[] = _(" FRZ");
static const u8 sText_Par[] = _(" PAR");
static const u8 sText_Tox[] = _(" TOX");
static const u8 sText_Fsb[] = _(" FSB");

static const u8 sText_Rain[]      = _("Pioggia");
static const u8 sText_HeavyRain[] = _("Diluvio");
static const u8 sText_Sun[]       = _("Sole");
static const u8 sText_HarshSun[]  = _("Siccità");
static const u8 sText_Sand[]      = _("Sabbia");
static const u8 sText_Hail[]      = _("Grandine");
static const u8 sText_Snow[]      = _("Neve");
static const u8 sText_Fog[]       = _("Nebbia");
static const u8 sText_Winds[]     = _("Correnti");
static const u8 sText_Grassy[]    = _("C.Erboso");
static const u8 sText_Misty[]     = _("C.Nebbioso");
static const u8 sText_Electric[]  = _("C.Elettrico");
static const u8 sText_Psychic[]   = _("C.Psichico");
static const u8 sText_TrickRoom[] = _("Distortozona");
static const u8 sText_Gravity[]   = _("Gravità");
static const u8 sText_MagicRoom[] = _("Magicozona");
static const u8 sText_WonderRoom[] = _("Mirabilzona");
static const u8 sText_Reflect[]   = _("Riflesso");
static const u8 sText_LightScreen[] = _("Schermoluce");
static const u8 sText_AuroraVeil[] = _("Velaurora");
static const u8 sText_Tailwind[]  = _("Ventoincoda");
static const u8 sText_Safeguard[] = _("Salvaguardia");
static const u8 sText_Mist[]      = _("Foschia");
static const u8 sText_Rocks[]     = _("Levitoroccia");
static const u8 sText_Spikes[]    = _("Punte");
static const u8 sText_ToxicSpikes[] = _("Fielepunte");
static const u8 sText_Web[]       = _("Ragnatela");
static const u8 sText_Steel[]     = _("Acciaiopunte");

// Sigle delle statistiche nell'ordine di gBattleMons[].statStages (da STAT_ATK).
static const u8 sText_StatAtk[] = _("At");
static const u8 sText_StatDef[] = _("Di");
static const u8 sText_StatSpe[] = _("Ve");
static const u8 sText_StatSpA[] = _("AS");
static const u8 sText_StatSpD[] = _("DS");
static const u8 sText_StatAcc[] = _("Pr");
static const u8 sText_StatEva[] = _("El");

static const u8 *const sStatAbbr[NUM_BATTLE_STATS] =
{
    [STAT_ATK]     = sText_StatAtk,
    [STAT_DEF]     = sText_StatDef,
    [STAT_SPEED]   = sText_StatSpe,
    [STAT_SPATK]   = sText_StatSpA,
    [STAT_SPDEF]   = sText_StatSpD,
    [STAT_ACC]     = sText_StatAcc,
    [STAT_EVASION] = sText_StatEva,
};

static const struct BgTemplate sBgTemplates[] =
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

static const struct WindowTemplate sWinTemplates[] =
{
    [WIN_FOE]    = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0,  .width = 30, .height = 5,  .paletteNum = 15, .baseBlock = 1 },
    [WIN_MID]    = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 5,  .width = 30, .height = 10, .paletteNum = 15, .baseBlock = 151 },
    [WIN_PLAYER] = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 15, .width = 30, .height = 5,  .paletteNum = 15, .baseBlock = 451 },
    DUMMY_WIN_TEMPLATE,
};

static void CB2_BattleInfoMain(void);
static void VBlankCB_BattleInfo(void);
static void Task_BattleInfoInput(u8 taskId);
static void Task_BattleInfoExit(u8 taskId);

// ---------------------------------------------------------------------
// Accesso ai dati
// ---------------------------------------------------------------------
static struct Pokemon *SideParty(u32 side)
{
    return (side == BI_SIDE_PLAYER) ? gParties[B_TRAINER_PLAYER] : gParties[B_TRAINER_OPPONENT_A];
}

static bool32 SlotExists(u32 side, u32 slot)
{
    struct Pokemon *mon;

    if (slot >= PARTY_SIZE)
        return FALSE;
    mon = &SideParty(side)[slot];
    return GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE && !GetMonData(mon, MON_DATA_IS_EGG);
}

static u32 BattlerForSlot(u32 side, u32 slot);

// Avversari: visibili solo dopo essere scesi in campo almeno una volta.
static bool32 SlotRevealed(u32 side, u32 slot)
{
    if (side == BI_SIDE_PLAYER)
        return TRUE;
    if (gBattleStruct->partyState[B_TRAINER_OPPONENT_A][slot].sentOut)
        return TRUE;
    return BattlerForSlot(side, slot) != MAX_BATTLERS_COUNT;
}

static bool32 SlotFainted(u32 side, u32 slot)
{
    return GetMonData(&SideParty(side)[slot], MON_DATA_HP) == 0;
}

// Battler in campo che corrisponde allo slot, oppure MAX_BATTLERS_COUNT.
static u32 BattlerForSlot(u32 side, u32 slot)
{
    u32 b;

    for (b = 0; b < gBattlersCount; b++)
    {
        if (gAbsentBattlerFlags & (1u << b))
            continue;
        if (IsOnPlayerSide(b) != (side == BI_SIDE_PLAYER))
            continue;
        if (gBattlerPartyIndexes[b] == slot && IsBattlerAlive(b))
            return b;
    }
    return MAX_BATTLERS_COUNT;
}

static s32 IconX(u32 slot)
{
    return ICON_FIRST_X + ICON_PITCH * slot;
}

// ---------------------------------------------------------------------
// Testo
// ---------------------------------------------------------------------
static void Print(u32 win, u32 font, s32 x, s32 y, const u8 *colors, const u8 *str)
{
    AddTextPrinterParameterized3(win, font, x, y, colors, TEXT_SKIP_DRAW, str);
}

// Stampa con a capo automatico. Usa gStringVar2 / gStringVar3 come appoggio:
// str non deve trovarsi lì.
static void PrintWrapped(u32 win, u32 font, s32 x, s32 y, u32 maxWidth, u32 maxLines, const u8 *colors, const u8 *str)
{
    u8 *line = gStringVar3;
    u8 *word = gStringVar2;
    u32 lineLen = 0, lines = 0;

    line[0] = EOS;
    while (lines < maxLines)
    {
        u32 wordLen = 0;
        bool32 end;

        while (*str == CHAR_SPACE || *str == CHAR_NEWLINE)
            str++;
        while (*str != EOS && *str != CHAR_SPACE && *str != CHAR_NEWLINE && wordLen < 60)
            word[wordLen++] = *str++;
        word[wordLen] = EOS;
        end = (wordLen == 0);

        if (!end)
        {
            u32 oldLen = lineLen;

            if (lineLen != 0)
                line[lineLen++] = CHAR_SPACE;
            StringCopy(&line[lineLen], word);
            lineLen += wordLen;
            if (oldLen != 0 && GetStringWidth(font, line, 0) > maxWidth)
            {
                // La parola non ci sta: stampa la riga senza di lei e ricomincia.
                line[oldLen] = EOS;
                Print(win, font, x, y + LINE_H * lines, colors, line);
                lines++;
                StringCopy(line, word);
                lineLen = wordLen;
            }
        }
        else
        {
            if (lineLen != 0)
                Print(win, font, x, y + LINE_H * lines, colors, line);
            return;
        }
    }
}

static void HeaderBar(u32 win, s32 x, s32 y, s32 width, const u8 *str)
{
    FillWindowPixelRect(win, PIXEL_FILL(TEXT_COLOR_DARK_GRAY), x, y, width, BAR_H);
    Print(win, BI_FONT, x + 3, y, sColWhite, str);
}

static u8 *AppendTurns(u8 *ptr, u32 timer, u32 max)
{
    ptr = StringCopy(ptr, sText_Space);
    if (timer == 0)
        return StringCopy(ptr, sText_None); // durata illimitata
    ptr = ConvertIntToDecimalStringN(ptr, timer, STR_CONV_MODE_LEFT_ALIGN, 2);
    if (max != 0)
    {
        ptr = StringCopy(ptr, sText_Slash);
        ptr = ConvertIntToDecimalStringN(ptr, max, STR_CONV_MODE_LEFT_ALIGN, 2);
    }
    return ptr;
}

// ---------------------------------------------------------------------
// Strisce delle squadre
// ---------------------------------------------------------------------
static void DrawStrip(u32 side)
{
    u32 win = (side == BI_SIDE_PLAYER) ? WIN_PLAYER : WIN_FOE;
    u32 slot;

    FillWindowPixelBuffer(win, PIXEL_FILL(TEXT_COLOR_WHITE));
    FillWindowPixelRect(win, PIXEL_FILL(TEXT_COLOR_DARK_GRAY), 0, 0, 240, BAR_H);
    Print(win, BI_FONT, 4, 0, sColWhite, (side == BI_SIDE_PLAYER) ? sText_Player : sText_Foe);
    if (side == BI_SIDE_FOE)
    {
        const u8 *hint = (sBI.view == VIEW_DETAIL) ? sText_HintDetail : sText_HintMain;
        Print(win, BI_FONT, 236 - GetStringWidth(BI_FONT, hint, 0), 0, sColWhite, hint);
    }
    else
    {
        Print(win, BI_FONT, 236 - GetStringWidth(BI_FONT, sText_OnField, 0), 0, sColWhite, sText_OnField);
    }

    for (slot = 0; slot < PARTY_SIZE; slot++)
    {
        s32 cx = IconX(slot);

        if (!SlotExists(side, slot))
            continue;
        if (side == sBI.side && slot == sBI.slot)
        {
            // Riquadro rosso dietro al Pokémon selezionato.
            FillWindowPixelRect(win, PIXEL_FILL(TEXT_COLOR_RED), cx - 16, BAR_H, 32, 28);
            FillWindowPixelRect(win, PIXEL_FILL(TEXT_COLOR_LIGHT_RED), cx - 14, BAR_H + 2, 28, 24);
        }
        if (SlotRevealed(side, slot) && BattlerForSlot(side, slot) != MAX_BATTLERS_COUNT)
            FillWindowPixelRect(win, PIXEL_FILL(TEXT_COLOR_GREEN), cx - 12, 36, 24, 3);
    }
    CopyWindowToVram(win, COPYWIN_GFX);
}

static void CreateIcons(void)
{
    u32 side, slot, i;
    u32 blackPal, greyPal[6];

    LoadMonIconPalettes();
    blackPal = LoadSpritePalette(&(struct SpritePalette){sBI_BlackPal, TAG_BI_BLACK});
    for (i = 0; i < 6; i++)
    {
        // Copia in scala di grigi di ogni palette delle icone, per i Pokémon KO.
        greyPal[i] = LoadSpritePalette(&(struct SpritePalette){gMonIconPaletteTable[i].data, TAG_BI_GREY + i});
        if (greyPal[i] != 0xFF)
        {
            TintPalette_GrayScale(&gPlttBufferUnfaded[OBJ_PLTT_ID(greyPal[i])], 16);
            CpuCopy16(&gPlttBufferUnfaded[OBJ_PLTT_ID(greyPal[i])], &gPlttBufferFaded[OBJ_PLTT_ID(greyPal[i])], PLTT_SIZE_4BPP);
        }
    }

    for (side = 0; side < BI_SIDE_COUNT; side++)
    {
        for (slot = 0; slot < PARTY_SIZE; slot++)
        {
            struct Pokemon *mon = &SideParty(side)[slot];
            bool32 revealed;
            enum Species species;
            u32 spriteId;
            s32 y = ((side == BI_SIDE_PLAYER) ? PLAYER_STRIP_Y : FOE_STRIP_Y) + STRIP_ICON_Y;

            sBI.iconIds[side][slot] = MAX_SPRITES;
            if (!SlotExists(side, slot))
                continue;
            revealed = SlotRevealed(side, slot);
            // Non rivelato: icona "?" generica, così la sagoma non svela la specie.
            species = revealed ? GetMonData(mon, MON_DATA_SPECIES) : SPECIES_NONE;
            spriteId = CreateMonIcon(species, SpriteCallbackDummy, IconX(slot), y, 1,
                                     revealed ? GetMonData(mon, MON_DATA_PERSONALITY) : 0);
            if (spriteId >= MAX_SPRITES)
                continue;
            sBI.iconIds[side][slot] = spriteId;
            gSprites[spriteId].oam.priority = 0;
            if (!revealed)
            {
                if (blackPal != 0xFF)
                    gSprites[spriteId].oam.paletteNum = blackPal;
            }
            else if (SlotFainted(side, slot))
            {
                u32 palIndex = GetMonIconPaletteIndexFromSpecies(species);
                if (palIndex < 6 && greyPal[palIndex] != 0xFF)
                    gSprites[spriteId].oam.paletteNum = greyPal[palIndex];
            }
        }
    }
}

// ---------------------------------------------------------------------
// Vista principale: modifiche statistiche + campo
// ---------------------------------------------------------------------
static void DrawStatChangesFor(u32 battler, s32 y)
{
    bool32 isPlayer = IsOnPlayerSide(battler);
    s32 x = 4;
    u32 stat, printed = 0;

    if (isPlayer)
    {
        GetMonData(GetBattlerMon(battler), MON_DATA_NICKNAME, gStringVar1);
        StringGet_Nickname(gStringVar1);
    }
    else
    {
        StringCopy(gStringVar1, GetSpeciesName(gBattleMons[battler].species));
    }
    Print(WIN_MID, BI_FONT, 2, y, isPlayer ? sColBlue : sColRed, gStringVar1);

    y += 8;
    for (stat = STAT_ATK; stat < NUM_BATTLE_STATS; stat++)
    {
        s32 stage = (s32)gBattleMons[battler].statStages[stat] - DEFAULT_STAT_STAGE;
        bool32 up = stage > 0;
        u8 *ptr;
        u32 width;

        if (stage == 0)
            continue;
        // Sigla, poi freccia + numero: rosso se aumentata, blu se diminuita.
        ptr = StringCopy(gStringVar4, up ? sText_Up : sText_Down);
        ConvertIntToDecimalStringN(ptr, up ? stage : -stage, STR_CONV_MODE_LEFT_ALIGN, 1);
        width = GetStringWidth(BI_FONT, sStatAbbr[stat], 0) + GetStringWidth(BI_FONT, gStringVar4, 0);
        if (x + width > 116)
            break;
        Print(WIN_MID, BI_FONT, x, y, sColDark, sStatAbbr[stat]);
        Print(WIN_MID, BI_FONT, x + GetStringWidth(BI_FONT, sStatAbbr[stat], 0), y, up ? sColRed : sColBlue, gStringVar4);
        x += width + 3;
        printed++;
    }
    if (printed == 0)
        Print(WIN_MID, BI_FONT, 4, y, sColGray, sText_NoChanges);
}

static void DrawStatChanges(void)
{
    static const u8 sOrder[] = {B_POSITION_OPPONENT_LEFT, B_POSITION_OPPONENT_RIGHT, B_POSITION_PLAYER_LEFT, B_POSITION_PLAYER_RIGHT};
    u32 i, row = 0;

    HeaderBar(WIN_MID, 0, 0, 118, sText_StatsHeader);
    for (i = 0; i < ARRAY_COUNT(sOrder); i++)
    {
        u32 battler;

        if (!IsDoubleBattle() && (sOrder[i] == B_POSITION_OPPONENT_RIGHT || sOrder[i] == B_POSITION_PLAYER_RIGHT))
            continue;
        battler = GetBattlerAtPosition(sOrder[i]);
        if (battler >= gBattlersCount || (gAbsentBattlerFlags & (1u << battler)) || !IsBattlerAlive(battler))
            continue;
        DrawStatChangesFor(battler, BAR_H + 1 + 16 * row);
        row++;
    }
}

// Piccolo "impaginatore": mette le voci una dopo l'altra e va a capo quando non ci stanno.
struct Flow
{
    s16 x, y;
    s16 left, right, bottom;
    bool8 overflow;
};

static void Flow_NewLine(struct Flow *flow)
{
    if (flow->x != flow->left)
    {
        flow->x = flow->left;
        flow->y += LINE_H;
    }
}

static void Flow_Add(struct Flow *flow, const u8 *colors, const u8 *str)
{
    s32 width = GetStringWidth(BI_FONT, str, 0);

    if (flow->overflow)
        return;
    if (flow->x != flow->left && flow->x + width > flow->right)
    {
        flow->x = flow->left;
        flow->y += LINE_H;
    }
    if (flow->y > flow->bottom)
    {
        flow->overflow = TRUE;
        Print(WIN_MID, BI_FONT, flow->right - 6, flow->bottom, sColDark, sText_More);
        return;
    }
    Print(WIN_MID, BI_FONT, flow->x, flow->y, colors, str);
    flow->x += width + 6;
}

static void Flow_AddTimed(struct Flow *flow, const u8 *name, u32 timer, u32 max)
{
    u8 *ptr = StringCopy(gStringVar4, name);
    AppendTurns(ptr, timer, max);
    Flow_Add(flow, sColDark, gStringVar4);
}

static void Flow_AddCount(struct Flow *flow, const u8 *name, u32 count)
{
    u8 *ptr = StringCopy(gStringVar4, name);
    ptr = StringCopy(ptr, sText_Times);
    ConvertIntToDecimalStringN(ptr, count, STR_CONV_MODE_LEFT_ALIGN, 1);
    Flow_Add(flow, sColDark, gStringVar4);
}

static bool32 SideHasEffects(enum BattleSide side)
{
    return (gSideStatuses[side] & (SIDE_STATUS_REFLECT | SIDE_STATUS_LIGHTSCREEN | SIDE_STATUS_AURORA_VEIL
                                 | SIDE_STATUS_TAILWIND | SIDE_STATUS_SAFEGUARD | SIDE_STATUS_MIST)) != 0
        || AreAnyHazardsOnSide(side);
}

static void DrawSideEffects(struct Flow *flow, enum BattleSide side, const u8 *label, const u8 *labelColors)
{
    u32 st = gSideStatuses[side];

    if (!SideHasEffects(side))
        return;
    Flow_NewLine(flow);
    Flow_Add(flow, labelColors, label);
    if (st & SIDE_STATUS_REFLECT)
        Flow_AddTimed(flow, sText_Reflect, gSideTimers[side].reflectTimer, sTimerMax.side[side][SIDE_TIMER_REFLECT]);
    if (st & SIDE_STATUS_LIGHTSCREEN)
        Flow_AddTimed(flow, sText_LightScreen, gSideTimers[side].lightscreenTimer, sTimerMax.side[side][SIDE_TIMER_LIGHTSCREEN]);
    if (st & SIDE_STATUS_AURORA_VEIL)
        Flow_AddTimed(flow, sText_AuroraVeil, gSideTimers[side].auroraVeilTimer, sTimerMax.side[side][SIDE_TIMER_AURORA_VEIL]);
    if (st & SIDE_STATUS_TAILWIND)
        Flow_AddTimed(flow, sText_Tailwind, gSideTimers[side].tailwindTimer, sTimerMax.side[side][SIDE_TIMER_TAILWIND]);
    if (st & SIDE_STATUS_SAFEGUARD)
        Flow_AddTimed(flow, sText_Safeguard, gSideTimers[side].safeguardTimer, sTimerMax.side[side][SIDE_TIMER_SAFEGUARD]);
    if (st & SIDE_STATUS_MIST)
        Flow_AddTimed(flow, sText_Mist, gSideTimers[side].mistTimer, sTimerMax.side[side][SIDE_TIMER_MIST]);
    if (IsHazardOnSide(side, HAZARDS_STEALTH_ROCK))
        Flow_Add(flow, sColDark, sText_Rocks);
    if (IsHazardOnSide(side, HAZARDS_SPIKES))
        Flow_AddCount(flow, sText_Spikes, gSideTimers[side].spikesAmount);
    if (IsHazardOnSide(side, HAZARDS_TOXIC_SPIKES))
        Flow_AddCount(flow, sText_ToxicSpikes, gSideTimers[side].toxicSpikesAmount);
    if (IsHazardOnSide(side, HAZARDS_STICKY_WEB))
        Flow_Add(flow, sColDark, sText_Web);
    if (IsHazardOnSide(side, HAZARDS_STEELSURGE))
        Flow_Add(flow, sColDark, sText_Steel);
}

static void DrawField(void)
{
    struct Flow flow = {.x = 124, .y = BAR_H + 1, .left = 124, .right = 238, .bottom = BAR_H + 1 + LINE_H * 6, .overflow = FALSE};
    u32 weather = gBattleWeather;
    const u8 *name = NULL;
    bool32 any = FALSE;

    HeaderBar(WIN_MID, 122, 0, 118, sText_FieldHeader);

    // Effetti globali
    if (weather & B_WEATHER_RAIN_PRIMAL)        name = sText_HeavyRain;
    else if (weather & B_WEATHER_RAIN)          name = sText_Rain;
    else if (weather & B_WEATHER_SUN_PRIMAL)    name = sText_HarshSun;
    else if (weather & B_WEATHER_SUN)           name = sText_Sun;
    else if (weather & B_WEATHER_SANDSTORM)     name = sText_Sand;
    else if (weather & B_WEATHER_HAIL)          name = sText_Hail;
    else if (weather & B_WEATHER_SNOW)          name = sText_Snow;
    else if (weather & B_WEATHER_FOG)           name = sText_Fog;
    else if (weather & B_WEATHER_STRONG_WINDS)  name = sText_Winds;
    if (name != NULL)
    {
        Flow_AddTimed(&flow, name, (weather & B_WEATHER_PRIMAL_ANY) ? 0 : gBattleStruct->weatherDuration, sTimerMax.weather);
        any = TRUE;
    }

    switch (gFieldTimers.terrain)
    {
    case B_TERRAIN_GRASSY:   name = sText_Grassy;   break;
    case B_TERRAIN_MISTY:    name = sText_Misty;    break;
    case B_TERRAIN_ELECTRIC: name = sText_Electric; break;
    case B_TERRAIN_PSYCHIC:  name = sText_Psychic;  break;
    default:                 name = NULL;           break;
    }
    if (name != NULL)
    {
        Flow_AddTimed(&flow, name, gFieldTimers.terrainTimer, sTimerMax.terrain);
        any = TRUE;
    }
    if (gFieldStatuses & STATUS_FIELD_TRICK_ROOM)
    {
        Flow_AddTimed(&flow, sText_TrickRoom, gFieldTimers.trickRoomTimer, sTimerMax.trickRoom);
        any = TRUE;
    }
    if (gFieldStatuses & STATUS_FIELD_GRAVITY)
    {
        Flow_AddTimed(&flow, sText_Gravity, gFieldTimers.gravityTimer, sTimerMax.gravity);
        any = TRUE;
    }
    if (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM)
    {
        Flow_AddTimed(&flow, sText_MagicRoom, gFieldTimers.magicRoomTimer, sTimerMax.magicRoom);
        any = TRUE;
    }
    if (gFieldStatuses & STATUS_FIELD_WONDER_ROOM)
    {
        Flow_AddTimed(&flow, sText_WonderRoom, gFieldTimers.wonderRoomTimer, sTimerMax.wonderRoom);
        any = TRUE;
    }

    // Effetti per lato
    if (SideHasEffects(B_SIDE_OPPONENT) || SideHasEffects(B_SIDE_PLAYER))
        any = TRUE;
    DrawSideEffects(&flow, B_SIDE_OPPONENT, sText_Them, sColRed);
    DrawSideEffects(&flow, B_SIDE_PLAYER, sText_Us, sColBlue);

    if (!any)
        Print(WIN_MID, BI_FONT, 124, BAR_H + 1, sColGray, sText_NoEffects);
}

static void DrawMainView(void)
{
    FillWindowPixelBuffer(WIN_MID, PIXEL_FILL(TEXT_COLOR_WHITE));
    FillWindowPixelRect(WIN_MID, PIXEL_FILL(TEXT_COLOR_LIGHT_GRAY), 119, 0, 2, 80);
    BattleInfo_SampleTimers();
    DrawStatChanges();
    DrawField();
    CopyWindowToVram(WIN_MID, COPYWIN_GFX);
}

// ---------------------------------------------------------------------
// Vista dettagli
// ---------------------------------------------------------------------
static enum Ability DetailAbility(u32 side, u32 slot)
{
    if (side == BI_SIDE_PLAYER)
    {
        u32 battler = BattlerForSlot(side, slot);
        if (battler != MAX_BATTLERS_COUNT)
            return gBattleMons[battler].ability;
        return GetMonAbility(&SideParty(side)[slot]);
    }
    return sRevealed[slot].ability;
}

static enum Item DetailItem(u32 side, u32 slot)
{
    if (side == BI_SIDE_PLAYER)
        return GetMonData(&SideParty(side)[slot], MON_DATA_HELD_ITEM);
    return sRevealed[slot].item;
}

static enum Move DetailMove(u32 side, u32 slot, u32 index)
{
    if (side == BI_SIDE_PLAYER)
        return GetMonData(&SideParty(side)[slot], MON_DATA_MOVE1 + index);
    return sRevealed[slot].moves[index];
}

static void DrawDetailRow(u32 row, s32 x, s32 y)
{
    u32 side = sBI.side, slot = sBI.slot;
    bool32 isFoe = (side == BI_SIDE_FOE);
    const u8 *colors = sColDark;
    u8 *ptr = gStringVar4;

    switch (row)
    {
    case DETAIL_ABILITY:
    {
        enum Ability ability = DetailAbility(side, slot);
        ptr = StringCopy(ptr, sText_Ability);
        if (ability == ABILITY_NONE)
        {
            StringCopy(ptr, sText_Unknown);
            colors = sColGray;
        }
        else
        {
            StringCopy(ptr, gAbilitiesInfo[ability].name);
        }
        break;
    }
    case DETAIL_ITEM:
    {
        enum Item item = DetailItem(side, slot);
        ptr = StringCopy(ptr, sText_Item);
        if (item != ITEM_NONE)
        {
            CopyItemName(item, ptr);
        }
        else if (isFoe)
        {
            StringCopy(ptr, sText_Unknown);
            colors = sColGray;
        }
        else
        {
            StringCopy(ptr, sText_NoItem);
        }
        break;
    }
    default:
    {
        enum Move move = DetailMove(side, slot, row - DETAIL_MOVE_1);
        if (move != MOVE_NONE)
        {
            StringCopy(ptr, GetMoveName(move));
        }
        else
        {
            StringCopy(ptr, isFoe ? sText_Unknown : sText_None);
            colors = sColGray;
        }
        break;
    }
    }

    if (row == sBI.detailRow)
        Print(WIN_MID, BI_FONT, x, y, sColRed, sText_Cursor);
    Print(WIN_MID, BI_FONT, x + 8, y, colors, gStringVar4);
}

static void DrawDetailDescription(void)
{
    u32 side = sBI.side, slot = sBI.slot;
    const u8 *desc = sText_NotRevealed;
    const u8 *colors = sColGray;

    if (sBI.detailRow == DETAIL_ABILITY)
    {
        enum Ability ability = DetailAbility(side, slot);
        if (ability != ABILITY_NONE)
        {
            desc = gAbilitiesInfo[ability].description;
            colors = sColDark;
        }
    }
    else if (sBI.detailRow == DETAIL_ITEM)
    {
        enum Item item = DetailItem(side, slot);
        if (item != ITEM_NONE)
        {
            desc = GetItemDescription(item);
            colors = sColDark;
        }
        else if (side == BI_SIDE_PLAYER)
        {
            desc = sText_None;
        }
    }
    else
    {
        enum Move move = DetailMove(side, slot, sBI.detailRow - DETAIL_MOVE_1);
        if (move != MOVE_NONE)
        {
            // "Tipo  Pot. N  Prec. N  PP N" sopra alla descrizione
            u8 *ptr = StringCopy(gStringVar4, gTypesInfo[GetMoveType(move)].name);
            u32 power = GetMovePower(move), accuracy = GetMoveAccuracy(move);

            ptr = StringCopy(ptr, sText_Power);
            if (power > 1)
                ptr = ConvertIntToDecimalStringN(ptr, power, STR_CONV_MODE_LEFT_ALIGN, 3);
            else
                ptr = StringCopy(ptr, sText_None);
            ptr = StringCopy(ptr, sText_Accuracy);
            if (accuracy != 0)
                ptr = ConvertIntToDecimalStringN(ptr, accuracy, STR_CONV_MODE_LEFT_ALIGN, 3);
            else
                ptr = StringCopy(ptr, sText_None);
            ptr = StringCopy(ptr, sText_PP);
            ConvertIntToDecimalStringN(ptr, GetMovePP(move), STR_CONV_MODE_LEFT_ALIGN, 2);
            Print(WIN_MID, BI_FONT, 4, 52, sColBlue, gStringVar4);
            PrintWrapped(WIN_MID, BI_FONT, 4, 52 + LINE_H, 232, 2, sColDark, GetMoveDescription(move));
            return;
        }
        else if (side == BI_SIDE_PLAYER)
        {
            desc = sText_None;
        }
    }
    PrintWrapped(WIN_MID, BI_FONT, 4, 52, 232, 3, colors, desc);
}

static void DrawDetailView(void)
{
    u32 side = sBI.side, slot = sBI.slot;
    struct Pokemon *mon = &SideParty(side)[slot];
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    u32 hp = GetMonData(mon, MON_DATA_HP), maxHp = GetMonData(mon, MON_DATA_MAX_HP);
    u32 status = GetMonData(mon, MON_DATA_STATUS);
    enum Type type1 = GetSpeciesType(species, 0), type2 = GetSpeciesType(species, 1);
    u8 *ptr;
    u32 row;

    FillWindowPixelBuffer(WIN_MID, PIXEL_FILL(TEXT_COLOR_WHITE));

    // Barra: nome a sinistra, PS e stato a destra
    FillWindowPixelRect(WIN_MID, PIXEL_FILL(TEXT_COLOR_DARK_GRAY), 0, 0, 240, BAR_H);
    if (side == BI_SIDE_PLAYER)
    {
        GetMonData(mon, MON_DATA_NICKNAME, gStringVar1);
        StringGet_Nickname(gStringVar1);
    }
    else
    {
        StringCopy(gStringVar1, GetSpeciesName(species));
    }
    Print(WIN_MID, BI_FONT, 4, 0, sColWhite, gStringVar1);

    if (hp == 0)
    {
        ptr = StringCopy(gStringVar4, sText_KO);
    }
    else
    {
        ptr = StringCopy(gStringVar4, sText_HP);
        if (side == BI_SIDE_PLAYER)
        {
            ptr = ConvertIntToDecimalStringN(ptr, hp, STR_CONV_MODE_LEFT_ALIGN, 3);
            ptr = StringCopy(ptr, sText_Slash);
            ptr = ConvertIntToDecimalStringN(ptr, maxHp, STR_CONV_MODE_LEFT_ALIGN, 3);
        }
        else
        {
            // Dell'avversario si conosce solo la barra: percentuale arrotondata per eccesso.
            u32 percent = (maxHp != 0) ? (hp * 100 + maxHp - 1) / maxHp : 0;
            ptr = ConvertIntToDecimalStringN(ptr, percent, STR_CONV_MODE_LEFT_ALIGN, 3);
            ptr = StringCopy(ptr, sText_Percent);
        }
        if (status & STATUS1_SLEEP)             ptr = StringCopy(ptr, sText_Slp);
        else if (status & STATUS1_TOXIC_POISON) ptr = StringCopy(ptr, sText_Tox);
        else if (status & STATUS1_POISON)       ptr = StringCopy(ptr, sText_Psn);
        else if (status & STATUS1_BURN)         ptr = StringCopy(ptr, sText_Brn);
        else if (status & STATUS1_FREEZE)       ptr = StringCopy(ptr, sText_Frz);
        else if (status & STATUS1_PARALYSIS)    ptr = StringCopy(ptr, sText_Par);
        else if (status & STATUS1_FROSTBITE)    ptr = StringCopy(ptr, sText_Fsb);
    }
    Print(WIN_MID, BI_FONT, 236 - GetStringWidth(BI_FONT, gStringVar4, 0), 0, sColWhite, gStringVar4);

    // Tipo
    ptr = StringCopy(gStringVar4, sText_Type);
    ptr = StringCopy(ptr, gTypesInfo[type1].name);
    if (type2 != type1)
    {
        ptr = StringCopy(ptr, sText_Slash);
        StringCopy(ptr, gTypesInfo[type2].name);
    }
    Print(WIN_MID, BI_FONT, 4, BAR_H, sColDark, gStringVar4);

    // Abilità / strumento / mosse su due colonne
    for (row = 0; row < DETAIL_ROW_COUNT; row++)
        DrawDetailRow(row, (row % 2) ? 124 : 4, 22 + LINE_H * (row / 2));

    FillWindowPixelRect(WIN_MID, PIXEL_FILL(TEXT_COLOR_LIGHT_GRAY), 2, 50, 236, 1);
    DrawDetailDescription();
    CopyWindowToVram(WIN_MID, COPYWIN_GFX);
}

static void DrawMid(void)
{
    if (sBI.view == VIEW_DETAIL)
        DrawDetailView();
    else
        DrawMainView();
}

// ---------------------------------------------------------------------
// Ciclo della schermata
// ---------------------------------------------------------------------
static void VBlankCB_BattleInfo(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_BattleInfoMain(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

// Mette il cursore sul primo Pokémon valido (prima il nostro in campo).
static void InitCursor(void)
{
    u32 slot;

    sBI.view = VIEW_MAIN;
    sBI.detailRow = 0;
    sBI.side = BI_SIDE_FOE;
    sBI.slot = 0;
    for (slot = 0; slot < PARTY_SIZE; slot++)
    {
        if (SlotExists(BI_SIDE_FOE, slot) && BattlerForSlot(BI_SIDE_FOE, slot) != MAX_BATTLERS_COUNT)
        {
            sBI.slot = slot;
            return;
        }
    }
    for (slot = 0; slot < PARTY_SIZE; slot++)
    {
        if (SlotExists(BI_SIDE_FOE, slot))
        {
            sBI.slot = slot;
            return;
        }
    }
}

// Entrata: la lotta ha già fatto la dissolvenza e chiamato CloseMainBattleScreen().
void CB2_BattleInfo(void)
{
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
        gMain.state++;
        break;
    case 2:
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
        memset(sBI_Tilemap, 0, sizeof(sBI_Tilemap));
        SetBgTilemapBuffer(0, sBI_Tilemap);
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        InitWindows(sWinTemplates);
        DeactivateAllTextPrinters();
        gMain.state++;
        break;
    case 3:
        LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
        InitCursor();
        CreateIcons();
        gMain.state++;
        break;
    case 4:
        PutWindowTilemap(WIN_FOE);
        PutWindowTilemap(WIN_MID);
        PutWindowTilemap(WIN_PLAYER);
        DrawStrip(BI_SIDE_FOE);
        DrawStrip(BI_SIDE_PLAYER);
        DrawMid();
        CopyBgTilemapBufferToVram(0);
        gMain.state++;
        break;
    default:
        ShowBg(0);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_BattleInfo);
        SetMainCallback2(CB2_BattleInfoMain);
        CreateTask(Task_BattleInfoInput, 0);
        break;
    }
}

static void Task_BattleInfoExit(u8 taskId)
{
    if (gPaletteFade.active)
        return;
    DestroyTask(taskId);
    SetVBlankCallback(NULL);
    ResetSpriteData();
    FreeAllSpritePalettes();
    ClearScheduledBgCopiesToVram();
    FreeAllWindowBuffers();
    UnsetBgTilemapBuffer(0);
    SetMainCallback2(ReshowBattleScreenAfterMenu);
}

// ---------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------
static void MoveCursorHorizontal(s32 delta)
{
    s32 slot = sBI.slot;

    for (;;)
    {
        slot += delta;
        if (slot < 0 || slot >= PARTY_SIZE)
            return;
        if (SlotExists(sBI.side, slot))
            break;
    }
    sBI.slot = slot;
    PlaySE(SE_SELECT);
    DrawStrip(sBI.side);
}

static u32 SlotDistance(u32 a, u32 b)
{
    return (a > b) ? a - b : b - a;
}

static void SwitchCursorSide(void)
{
    u32 newSide = (sBI.side == BI_SIDE_FOE) ? BI_SIDE_PLAYER : BI_SIDE_FOE;
    u32 slot, best = PARTY_SIZE;

    // Stesso slot se esiste, altrimenti il più vicino.
    for (slot = 0; slot < PARTY_SIZE; slot++)
    {
        if (!SlotExists(newSide, slot))
            continue;
        if (best == PARTY_SIZE || SlotDistance(slot, sBI.slot) < SlotDistance(best, sBI.slot))
            best = slot;
    }
    if (best == PARTY_SIZE)
        return;
    sBI.side = newSide;
    sBI.slot = best;
    PlaySE(SE_SELECT);
    DrawStrip(BI_SIDE_FOE);
    DrawStrip(BI_SIDE_PLAYER);
}

static void Task_BattleInfoInput(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    if (sBI.view == VIEW_DETAIL)
    {
        s32 row = sBI.detailRow;

        if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            sBI.view = VIEW_MAIN;
            DrawStrip(BI_SIDE_FOE);
            DrawMid();
            return;
        }
        if (JOY_NEW(DPAD_UP) && row >= 2)
            row -= 2;
        else if (JOY_NEW(DPAD_DOWN) && row + 2 < DETAIL_ROW_COUNT)
            row += 2;
        else if (JOY_NEW(DPAD_LEFT) && (row % 2) == 1)
            row -= 1;
        else if (JOY_NEW(DPAD_RIGHT) && (row % 2) == 0)
            row += 1;
        if (row != sBI.detailRow)
        {
            sBI.detailRow = row;
            PlaySE(SE_SELECT);
            DrawMid();
        }
        return;
    }

    if (JOY_NEW(B_BUTTON) || JOY_NEW(R_BUTTON))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_BattleInfoExit;
    }
    else if (JOY_NEW(A_BUTTON))
    {
        if (!SlotExists(sBI.side, sBI.slot) || !SlotRevealed(sBI.side, sBI.slot))
        {
            PlaySE(SE_FAILURE);
        }
        else
        {
            PlaySE(SE_SELECT);
            sBI.view = VIEW_DETAIL;
            sBI.detailRow = DETAIL_MOVE_1;
            DrawStrip(BI_SIDE_FOE);
            DrawMid();
        }
    }
    else if (JOY_NEW(DPAD_LEFT))
    {
        MoveCursorHorizontal(-1);
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        MoveCursorHorizontal(1);
    }
    else if (JOY_NEW(DPAD_UP) || JOY_NEW(DPAD_DOWN))
    {
        SwitchCursorSide();
    }
}

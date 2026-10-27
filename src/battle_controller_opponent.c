#include "global.h"
#include "battle.h"
#include "battle_ai_switch.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc.h"
#include "battle_ai_perspective.h"
#include "trainer.h"
#include "trainer_pokemon_sprites.h"
#include "config/custom.h"
#include "constants/battle.h"
#include "constants/battle_ai.h"
#include "battle_anim.h"
#include "battle_arena.h"
#include "battle_controllers.h"
#include "battle_message.h"
#include "battle_interface.h"
#include "battle_setup.h"
#include "battle_special.h"
#include "battle_tv.h"
#include "battle_z_move.h"
#include "bg.h"
#include "data.h"
#include "frontier_util.h"
#include "item.h"
#include "link.h"
#include "main.h"
#include "m4a.h"
#include "palette.h"
#include "party_menu.h"
#include "pokeball.h"
#include "pokemon.h"
#include "random.h"
#include "reshow_battle_screen.h"
#include "sound.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "util.h"
#include "window.h"
#include "constants/battle_anim.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/party_menu.h"
#include "constants/songs.h"
#include "constants/trainers.h"
#include "trainer_hill.h"
#include "trainer_tower.h"
#include "test_runner.h"
#include "test/battle.h"

static void OpponentHandleDrawTrainerPic(enum BattlerId battler);
static void OpponentHandleTrainerSlideBack(enum BattlerId battler);
static void OpponentHandleChooseAction(enum BattlerId battler);
static void OpponentHandleChooseMove(enum BattlerId battler);
static void OpponentHandleChooseItem(enum BattlerId battler);
static void OpponentHandleChoosePokemon(enum BattlerId battler);
static void OpponentHandleIntroTrainerBallThrow(enum BattlerId battler);
static void OpponentHandleDrawPartyStatusSummary(enum BattlerId battler);
static void OpponentHandleEndLinkBattle(enum BattlerId battler);
static void OpponentBufferRunCommand(enum BattlerId battler);

static void (*const sOpponentBufferCommands[CONTROLLER_CMDS_COUNT])(enum BattlerId battler) =
{
    [CONTROLLER_GETMONDATA]               = BtlController_HandleGetMonData,
    [CONTROLLER_GETRAWMONDATA]            = BtlController_HandleGetRawMonData,
    [CONTROLLER_SETMONDATA]               = BtlController_HandleSetMonData,
    [CONTROLLER_SETRAWMONDATA]            = BtlController_HandleSetRawMonData,
    [CONTROLLER_LOADMONSPRITE]            = BtlController_HandleLoadMonSprite,
    [CONTROLLER_SWITCHINANIM]             = BtlController_HandleSwitchInAnim,
    [CONTROLLER_RETURNMONTOBALL]          = BtlController_HandleReturnMonToBall,
    [CONTROLLER_DRAWTRAINERPIC]           = OpponentHandleDrawTrainerPic,
    [CONTROLLER_TRAINERSLIDE]             = OpponentHandleTrainerSlide,
    [CONTROLLER_TRAINERSLIDEBACK]         = OpponentHandleTrainerSlideBack,
    [CONTROLLER_FAINTANIMATION]           = BtlController_HandleFaintAnimation,
    [CONTROLLER_PALETTEFADE]              = BtlController_Empty,
    [CONTROLLER_BALLTHROWANIM]            = BtlController_Empty,
    [CONTROLLER_PAUSE]                    = BtlController_Empty,
    [CONTROLLER_MOVEANIMATION]            = BtlController_HandleMoveAnimation,
    [CONTROLLER_PRINTSTRING]              = BtlController_HandlePrintString,
    [CONTROLLER_PRINTSTRINGPLAYERONLY]    = BtlController_Empty,
    [CONTROLLER_CHOOSEACTION]             = OpponentHandleChooseAction,
    [CONTROLLER_YESNOBOX]                 = BtlController_Empty,
    [CONTROLLER_CHOOSEMOVE]               = OpponentHandleChooseMove,
    [CONTROLLER_OPENBAG]                  = OpponentHandleChooseItem,
    [CONTROLLER_CHOOSEPOKEMON]            = OpponentHandleChoosePokemon,
    [CONTROLLER_23]                       = BtlController_Empty,
    [CONTROLLER_HEALTHBARUPDATE]          = BtlController_HandleHealthBarUpdate,
    [CONTROLLER_EXPUPDATE]                = BtlController_Empty,
    [CONTROLLER_STATUSICONUPDATE]         = BtlController_HandleStatusIconUpdate,
    [CONTROLLER_STATUSANIMATION]          = BtlController_HandleStatusAnimation,
    [CONTROLLER_STATUSXOR]                = BtlController_Empty,
    [CONTROLLER_DATATRANSFER]             = BtlController_Empty,
    [CONTROLLER_DMA3TRANSFER]             = BtlController_Empty,
    [CONTROLLER_PLAYBGM]                  = BtlController_Empty,
    [CONTROLLER_32]                       = BtlController_Empty,
    [CONTROLLER_TWORETURNVALUES]          = BtlController_Empty,
    [CONTROLLER_CHOSENMONRETURNVALUE]     = BtlController_Empty,
    [CONTROLLER_ONERETURNVALUE]           = BtlController_Empty,
    [CONTROLLER_ONERETURNVALUE_DUPLICATE] = BtlController_Empty,
    [CONTROLLER_HITANIMATION]             = BtlController_HandleHitAnimation,
    [CONTROLLER_CANTSWITCH]               = BtlController_Empty,
    [CONTROLLER_PLAYSE]                   = BtlController_HandlePlaySE,
    [CONTROLLER_PLAYFANFAREORBGM]         = BtlController_HandlePlayFanfareOrBGM,
    [CONTROLLER_FAINTINGCRY]              = BtlController_HandleFaintingCry,
    [CONTROLLER_INTROSLIDE]               = BtlController_HandleIntroSlide,
    [CONTROLLER_INTROTRAINERBALLTHROW]    = OpponentHandleIntroTrainerBallThrow,
    [CONTROLLER_DRAWPARTYSTATUSSUMMARY]   = OpponentHandleDrawPartyStatusSummary,
    [CONTROLLER_HIDEPARTYSTATUSSUMMARY]   = BtlController_HandleHidePartyStatusSummary,
    [CONTROLLER_ENDBOUNCE]                = BtlController_Empty,
    [CONTROLLER_SPRITEINVISIBILITY]       = BtlController_HandleSpriteInvisibility,
    [CONTROLLER_BATTLEANIMATION]          = BtlController_HandleBattleAnimation,
    [CONTROLLER_LINKSTANDBYMSG]           = BtlController_Empty,
    [CONTROLLER_RESETACTIONMOVESELECTION] = BtlController_Empty,
    [CONTROLLER_ENDLINKBATTLE]            = OpponentHandleEndLinkBattle,
    [CONTROLLER_DEBUGMENU]                = BtlController_Empty,
    [CONTROLLER_TERMINATOR_NOP]           = BtlController_TerminatorNop
};

void SetControllerToOpponent(enum BattlerId battler)
{
    gBattlerBattleController[battler] = BATTLE_CONTROLLER_OPPONENT;
    gBattlerControllerEndFuncs[battler] = OpponentBufferExecCompleted;
    gBattlerControllerFuncs[battler] = OpponentBufferRunCommand;
}

static void OpponentBufferRunCommand(enum BattlerId battler)
{
    if (IsBattleControllerActiveOnLocal(battler))
    {
        if (gBattleResources->bufferA[battler][0] < ARRAY_COUNT(sOpponentBufferCommands))
            sOpponentBufferCommands[gBattleResources->bufferA[battler][0]](battler);
        else
            BtlController_Complete(battler);
    }
}

static void Intro_WaitForShinyAnimAndHealthbox(enum BattlerId battler)
{
    bool8 healthboxAnimDone = FALSE;
    bool8 twoMons;

    twoMons = TwoOpponentIntroMons(battler);
    if (!twoMons || ((twoMons && (gBattleTypeFlags & BATTLE_TYPE_MULTI) && !BATTLE_TWO_VS_ONE_OPPONENT) || (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS)))
    {
        if (gSprites[gHealthboxSpriteIds[battler]].callback == SpriteCallbackDummy)
            healthboxAnimDone = TRUE;
        twoMons = FALSE;
    }
    else
    {
        if (gSprites[gHealthboxSpriteIds[battler]].callback == SpriteCallbackDummy
         && gSprites[gHealthboxSpriteIds[GetPartnerBattler(battler)]].callback == SpriteCallbackDummy)
            healthboxAnimDone = TRUE;
        twoMons = TRUE;
    }

    if (healthboxAnimDone)
    {
        if (twoMons == TRUE)
        {
            if (gBattleSpritesDataPtr->healthBoxesData[battler].finishedShinyMonAnim
             && gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].finishedShinyMonAnim)
            {
                gBattleSpritesDataPtr->healthBoxesData[battler].triedShinyMonAnim = FALSE;
                gBattleSpritesDataPtr->healthBoxesData[battler].finishedShinyMonAnim = FALSE;
                gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].triedShinyMonAnim = FALSE;
                gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].finishedShinyMonAnim = FALSE;
                FreeShinyStars();
            }
            else
            {
                return;
            }
        }
        else if (gBattleSpritesDataPtr->healthBoxesData[battler].finishedShinyMonAnim)
        {
            if (GetBattlerPosition(battler) == B_POSITION_OPPONENT_RIGHT)
            {
                if (!gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].triedShinyMonAnim
                 && !gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].finishedShinyMonAnim)
                {
                    FreeShinyStars();
                }
                else
                {
                    return;
                }
            }
            gBattleSpritesDataPtr->healthBoxesData[battler].triedShinyMonAnim = FALSE;
            gBattleSpritesDataPtr->healthBoxesData[battler].finishedShinyMonAnim = FALSE;
        }
        else
        {
            return;
        }

        gBattleSpritesDataPtr->healthBoxesData[battler].introEndDelay = 3;
        gBattlerControllerFuncs[battler] = BtlController_Intro_DelayAndEnd;
    }
}

static void Intro_TryShinyAnimShowHealthbox(enum BattlerId battler)
{
    bool32 bgmRestored = FALSE;
    bool32 battlerAnimsDone = FALSE;
    bool32 twoMons;

    if (!gBattleSpritesDataPtr->healthBoxesData[battler].triedShinyMonAnim
     && !gBattleSpritesDataPtr->healthBoxesData[battler].ballAnimActive
     && !gBattleSpritesDataPtr->healthBoxesData[battler].finishedShinyMonAnim)
        TryShinyAnimation(battler, GetBattlerMon(battler));

    twoMons = TwoOpponentIntroMons(battler);
    if (!(gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS)
     && (!(gBattleTypeFlags & BATTLE_TYPE_MULTI) || BATTLE_TWO_VS_ONE_OPPONENT)
     && twoMons
     && !gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].triedShinyMonAnim
     && !gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].ballAnimActive
     && !gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].finishedShinyMonAnim)
        TryShinyAnimation(GetPartnerBattler(battler), GetBattlerMon(GetPartnerBattler(battler)));

    if (!gBattleSpritesDataPtr->healthBoxesData[battler].ballAnimActive && !gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].ballAnimActive)
    {
        if (!gBattleSpritesDataPtr->healthBoxesData[battler].healthboxSlideInStarted)
        {
            if (twoMons && (!(gBattleTypeFlags & BATTLE_TYPE_MULTI) || BATTLE_TWO_VS_ONE_OPPONENT))
            {
                UpdateHealthboxAttribute(gHealthboxSpriteIds[GetPartnerBattler(battler)], GetBattlerMon(GetPartnerBattler(battler)), HEALTHBOX_ALL);
                StartHealthboxSlideIn(GetPartnerBattler(battler));
                SetHealthboxSpriteVisible(gHealthboxSpriteIds[GetPartnerBattler(battler)]);
            }
            UpdateHealthboxAttribute(gHealthboxSpriteIds[battler], GetBattlerMon(battler), HEALTHBOX_ALL);
            StartHealthboxSlideIn(battler);
            SetHealthboxSpriteVisible(gHealthboxSpriteIds[battler]);
        }
        gBattleSpritesDataPtr->healthBoxesData[battler].healthboxSlideInStarted = TRUE;
    }

    if (!gBattleSpritesDataPtr->healthBoxesData[battler].waitForCry
        && gBattleSpritesDataPtr->healthBoxesData[battler].healthboxSlideInStarted
        && !gBattleSpritesDataPtr->healthBoxesData[GetPartnerBattler(battler)].waitForCry
        && !IsCryPlayingOrClearCrySongs())
    {
        if (!gBattleSpritesDataPtr->healthBoxesData[battler].bgmRestored)
        {
            if (gBattleTypeFlags & BATTLE_TYPE_MULTI && gBattleTypeFlags & BATTLE_TYPE_LINK)
            {
                if (GetBattlerPosition(battler) == B_POSITION_OPPONENT_LEFT)
                    m4aMPlayContinue(&gMPlayInfo_BGM);
            }
            else
            {
                m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 0x100);
            }
        }
        gBattleSpritesDataPtr->healthBoxesData[battler].bgmRestored = TRUE;
        bgmRestored = TRUE;
    }

    if (!twoMons || (twoMons && gBattleTypeFlags & BATTLE_TYPE_MULTI && !BATTLE_TWO_VS_ONE_OPPONENT))
    {
        if (gSprites[gBattleControllerData[battler]].callback == SpriteCallbackDummy)
        {
            TrySetBattlerShadowSpriteCallback(battler);
            if (gSprites[gBattlerSpriteIds[battler]].callback == SpriteCallbackDummy)
            {
                battlerAnimsDone = TRUE;
            }
        }
    }
    else
    {
        if (gSprites[gBattleControllerData[battler]].callback == SpriteCallbackDummy
            && gSprites[gBattleControllerData[GetPartnerBattler(battler)]].callback == SpriteCallbackDummy)
        {
            TrySetBattlerShadowSpriteCallback(battler);
            TrySetBattlerShadowSpriteCallback(GetPartnerBattler(battler));
            if (gSprites[gBattlerSpriteIds[battler]].callback == SpriteCallbackDummy
                && gSprites[gBattlerSpriteIds[GetPartnerBattler(battler)]].callback == SpriteCallbackDummy)
            {
                battlerAnimsDone = TRUE;
            }
        }
    }

    if (bgmRestored && battlerAnimsDone)
    {
        if (twoMons && (!(gBattleTypeFlags & BATTLE_TYPE_MULTI) || BATTLE_TWO_VS_ONE_OPPONENT))
            DestroySprite(&gSprites[gBattleControllerData[GetPartnerBattler(battler)]]);

        DestroySprite(&gSprites[gBattleControllerData[battler]]);
        gBattleSpritesDataPtr->animationData->introAnimActive = FALSE;
        gBattleSpritesDataPtr->healthBoxesData[battler].bgmRestored = FALSE;
        gBattleSpritesDataPtr->healthBoxesData[battler].healthboxSlideInStarted = FALSE;

        gBattlerControllerFuncs[battler] = Intro_WaitForShinyAnimAndHealthbox;
    }
}

void OpponentBufferExecCompleted(enum BattlerId battler)
{
    gBattlerControllerFuncs[battler] = OpponentBufferRunCommand;
    if (gBattleTypeFlags & BATTLE_TYPE_LINK)
    {
        u8 playerId = GetMultiplayerId();

        PrepareBufferDataTransferLink(battler, B_COMM_CONTROLLER_IS_DONE, 4, &playerId);
        gBattleResources->bufferA[battler][0] = CONTROLLER_TERMINATOR_NOP;
    }
    else
    {
        MarkBattleControllerIdleOnLocal(battler);
    }
}

static u32 OpponentGetTrainerPicId(enum BattlerId battlerId)
{
    enum TrainerPicID trainerPicId;

    if (gBattleTypeFlags & BATTLE_TYPE_SECRET_BASE)
    {
        trainerPicId = GetSecretBaseTrainerPicIndex();
    }
    else if (TRAINER_BATTLE_PARAM.opponentA == TRAINER_FRONTIER_BRAIN)
    {
        trainerPicId = GetFrontierBrainTrainerPicIndex();
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_TRAINER_TOWER && gMapHeader.regionMapSectionId == MAPSEC_TRAINER_TOWER_2)
    {
        trainerPicId = GetTrainerTowerTrainerFrontSpriteId();
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_TRAINER_HILL)
    {
        if (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS)
        {
            if (battlerId == 1)
                trainerPicId = GetTrainerHillTrainerFrontSpriteId(TRAINER_BATTLE_PARAM.opponentA);
            else
                trainerPicId = GetTrainerHillTrainerFrontSpriteId(TRAINER_BATTLE_PARAM.opponentB);
        }
        else
        {
            trainerPicId = GetTrainerHillTrainerFrontSpriteId(TRAINER_BATTLE_PARAM.opponentA);
        }
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_FRONTIER)
    {
        if (gBattleTypeFlags & (BATTLE_TYPE_TWO_OPPONENTS | BATTLE_TYPE_TOWER_LINK_MULTI))
        {
            if (battlerId == 1)
                trainerPicId = GetFrontierTrainerFrontSpriteId(TRAINER_BATTLE_PARAM.opponentA);
            else
                trainerPicId = GetFrontierTrainerFrontSpriteId(TRAINER_BATTLE_PARAM.opponentB);
        }
        else
        {
            trainerPicId = GetFrontierTrainerFrontSpriteId(TRAINER_BATTLE_PARAM.opponentA);
        }
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_EREADER_TRAINER)
    {
        trainerPicId = GetEreaderTrainerFrontSpriteId();
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS)
    {
        if (battlerId != 1)
            trainerPicId = GetTrainerPicFromId(TRAINER_BATTLE_PARAM.opponentB);
        else
            trainerPicId = GetTrainerPicFromId(TRAINER_BATTLE_PARAM.opponentA);
    }
    else
    {
        trainerPicId = GetTrainerPicFromId(TRAINER_BATTLE_PARAM.opponentA);
    }

    return trainerPicId;
}

static void OpponentHandleDrawTrainerPic(enum BattlerId battler)
{
    s16 xPos;
    enum TrainerPicID trainerPicId;

    // Sets battle test opponent sprites to Leaf and Red
    if (TESTING)
    {
        if (GetBattlerPosition(battler) == B_POSITION_OPPONENT_LEFT)
        {
            trainerPicId = TRAINER_PIC_LEAF;
            if (!(gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS))
                xPos = 176;
            else
                xPos = 200;
        }
        else
        {
            trainerPicId = TRAINER_PIC_RED;
            xPos = 152;
        }
    }
    else
    {
        trainerPicId = OpponentGetTrainerPicId(battler);

        if (gBattleTypeFlags & (BATTLE_TYPE_MULTI | BATTLE_TYPE_TWO_OPPONENTS) && !BATTLE_TWO_VS_ONE_OPPONENT)
        {
            if ((GetBattlerPosition(battler) & BIT_FLANK) != 0) // second mon
                xPos = 152;
           else // first mon
                xPos = 200;
        }
        else
        {
            xPos = 176;
        }
    }

    BtlController_HandleDrawTrainerPic(battler, trainerPicId, TRUE, xPos, 40, -1);
}

void OpponentHandleTrainerSlide(enum BattlerId battler)
{
    enum TrainerPicID trainerPicId = OpponentGetTrainerPicId(battler);
    BtlController_HandleTrainerSlide(battler, trainerPicId);
}

static void OpponentHandleTrainerSlideBack(enum BattlerId battler)
{
    BtlController_HandleTrainerSlideBack(battler, 35, FALSE);
}

// --- IA VGC: pianificazione in sottofondo ------------------------------------------------------
// Il pianificatore del turno gira un po' per fotogramma mentre il giocatore sceglie. Se il giocatore
// ha già confermato e l'IA sta ancora pensando, compare il ritratto dell'allenatore con "penso...".
static EWRAM_DATA u16 sThinkFrames = 0;
static EWRAM_DATA u16 sThinkSpriteId = 0;
static EWRAM_DATA u8 sThinkSpriteActive = 0;
static EWRAM_DATA u8 sThinkTextShown = FALSE;
static EWRAM_DATA u8 sThinkPlayerIdleFrames = 0;
static EWRAM_DATA u8 sThinkPending = 0; // battler in attesa che il partner finisca di pianificare la coppia


static const u8 sText_Thinking[] = _("{STR_VAR_1} sta pensando...");

static bool32 PlayerSideIsIdle(void)
{
    enum BattlerId b;
    for (b = 0; b < gBattlersCount; b++)
    {
        if (IsOnPlayerSide(b) && (gBattleControllerExecFlags & (1u << b)))
            return FALSE;
    }
    return TRUE;
}

static void ShowThinking(void)
{
    if (sThinkTextShown)
        return;
    sThinkTextShown = TRUE;
    StringCopy(gStringVar1, GetTrainerNameFromId(TRAINER_BATTLE_PARAM.opponentA));
    StringExpandPlaceholders(gStringVar4, sText_Thinking);
    // Un solo testo alla volta nella finestra dei messaggi: due stampe sovrapposte si mescolano e
    // lasciano stampanti allocate (perdita di memoria trovata dai test).
    DeactivateSingleTextPrinter(B_WIN_MSG, WINDOW_TEXT_PRINTER);
    // Dopo la scelta delle mosse BG0 resta scorso sul menu delle mosse (gBattle_BG0_Y = DISPLAY_HEIGHT): la
    // finestra dei messaggi sta in cima al tilemap e il testo non si vedeva. Come ogni messaggio di lotta
    // (BtlController_HandlePrintString), si riporta BG0 a 0.
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    BattlePutTextOnWindow(gStringVar4, B_WIN_MSG);
    // In mezzo ai suoi due Pokémon: in doppio stanno a x 152 e 200 (sBattlerCoords), quindi al centro x 176.
    // Etichetta della palette: deve essere quella del ritratto (GetTrainerPicTag), perché CreatePicSprite carica
    // la palette con quell'etichetta ma dà allo sprite quella passata. Con un'etichetta inventata lo sprite cercava
    // una palette mai caricata e usciva tutto nero.
    {
        enum TrainerPicID pic = GetTrainerPicFromId(TRAINER_BATTLE_PARAM.opponentA);
        sThinkSpriteId = CreateTrainerPicSprite(pic, TRUE, 176, 44, 0, GetTrainerPicTag(pic, TRUE));
    }
    sThinkSpriteActive = (sThinkSpriteId != 0xFFFF);
    if (sThinkSpriteActive)
    {
        gSprites[sThinkSpriteId].oam.priority = 0;
        gSprites[sThinkSpriteId].subpriority = 0;
    }
}

static void HideThinking(void)
{
    if (!sThinkTextShown)
        return;
    sThinkTextShown = FALSE;
    if (sThinkSpriteActive)
    {
        FreeAndDestroyTrainerPicSprite(sThinkSpriteId);
        sThinkSpriteActive = FALSE;
    }
    DeactivateSingleTextPrinter(B_WIN_MSG, WINDOW_TEXT_PRINTER);
    BattlePutTextOnWindow(gText_EmptyString3, B_WIN_MSG);
}

static void OpponentThinkThenChooseAction(enum BattlerId battler)
{
    bool32 done;

    AI_SetPerspective(battler); // IA contro IA: pensa il lato di `battler`

    // Un solo pianificatore alla volta: se sta lavorando per il partner (che decide la coppia), si aspetta.
    if (AI_VgcPlanIsBusy(battler))
        return;
    // In attesa del partner: ora tocca a noi (di solito con la nostra metà della coppia già decisa).
    if (sThinkPending & (1u << battler))
    {
        sThinkPending &= ~(1u << battler);
        if (!AI_VgcPlanBegin(battler))
        {
            HideThinking();
            AI_TrySwitchOrUseItem(battler);
            BtlController_Complete(battler);
            return;
        }
    }
    done = AI_VgcPlanStep();
    if (!done)
    {
        sThinkFrames++;
        if (PlayerSideIsIdle())
        {
            if (sThinkPlayerIdleFrames < 255)
                sThinkPlayerIdleFrames++;
        }
        else
        {
            sThinkPlayerIdleFrames = 0;
        }
        if (CUSTOM_VGC_THINKING_OVERLAY && !TESTING && sThinkPlayerIdleFrames >= 12)
            ShowThinking();
        return;
    }
    AI_VgcPlanApply();
    // Se il compagno deve ancora pensare, il ritratto resta (toglierlo e rimetterlo lo faceva lampeggiare).
    if (!(sThinkPending & ~(1u << battler)))
        HideThinking();
    AI_TrySwitchOrUseItem(battler);
    BtlController_Complete(battler);
}

static void OpponentHandleChooseAction(enum BattlerId battler)
{
    AI_SetPerspective(battler); // IA contro IA: pensa il lato di `battler`
    if (IsDoubleBattle() && (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
     && (gAiThinkingStruct->aiFlags[battler] & AI_FLAG_VGC_DOUBLES))
    {
        bool32 started = AI_VgcPlanBegin(battler);
        sThinkPending &= ~(1u << battler); // niente attese rimaste da una lotta precedente
        if (started || AI_VgcPlanIsBusy(battler))
        {
            if (!started)
                sThinkPending |= 1u << battler;
            sThinkFrames = 0;
            sThinkPlayerIdleFrames = 0;
            gBattlerControllerFuncs[battler] = OpponentThinkThenChooseAction;
            return;
        }
    }
    AI_TrySwitchOrUseItem(battler);
    BtlController_Complete(battler);
}

static void OpponentHandleChooseMove(enum BattlerId battler)
{
    if (gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_FIRST_BATTLE | BATTLE_TYPE_SAFARI | BATTLE_TYPE_ROAMER)
     || IsWildMonSmart())
    {
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            BtlController_EmitTwoReturnValues(battler, B_COMM_TO_ENGINE, B_ACTION_EXEC_SCRIPT, ChooseMoveAndTargetInBattlePalace(battler));
        }
        else if (gAiBattleData->actionFlee)
        {
            gAiBattleData->actionFlee = FALSE;
            BtlController_EmitTwoReturnValues(battler, B_COMM_TO_ENGINE, B_ACTION_RUN, 0);
        }
        else if (gAiBattleData->choiceWatch)
        {
            gAiBattleData->choiceWatch = FALSE;
            BtlController_EmitTwoReturnValues(battler, B_COMM_TO_ENGINE, B_ACTION_SAFARI_WATCH_CAREFULLY, 0);
        }
        else
        {
            SetFinalChosenTarget(battler, FALSE);
        }
        BtlController_Complete(battler);
    }
    else // Wild Pokémon - use random move
    {
        enum Move move = MOVE_NONE;
        u32 chosenMoveIndex = 0;
        struct ChooseMoveStruct *moveInfo = (struct ChooseMoveStruct *)(&gBattleResources->bufferA[battler][4]);

        do
        {
            chosenMoveIndex = Random() & (MAX_MON_MOVES - 1);
            move = moveInfo->moves[chosenMoveIndex];
        } while (move == MOVE_NONE);

        enum MoveTarget moveTarget = GetBattlerMoveSelectionTargetType(battler, move);
        if (moveTarget == TARGET_USER || moveTarget == TARGET_USER_OR_ALLY || moveTarget == TARGET_ALLY)
        {
            BtlController_EmitTwoReturnValues(battler, B_COMM_TO_ENGINE, B_ACTION_EXEC_SCRIPT, (chosenMoveIndex) | (GetDefaultSelectionTarget(battler, moveTarget) << 8));
        }
        else if (IsDoubleBattle())
        {
            enum BattlerId targetBattler;
            do {
                enum BattlerPosition pos = RandomPercentage(RNG_WILD_MON_TARGET, 50) ? B_POSITION_PLAYER_LEFT : B_POSITION_PLAYER_RIGHT;
                targetBattler = GetBattlerAtPosition(pos);
            } while (!CanTargetBattler(battler, targetBattler, move));

            // Don't bother to check if they're enemies if the move can't attack ally
            if (WE_WILD_NATURAL_ENEMIES && moveTarget != TARGET_BOTH)
            {
                enum Species speciesAttacker, speciesTarget;
                speciesAttacker = gBattleMons[battler].species;
                speciesTarget = gBattleMons[GetPartnerBattler(battler)].species;

                bool32 isPartnerEnemy = IsNaturalEnemy(speciesAttacker, speciesTarget);

                if (isPartnerEnemy && CanTargetBattler(battler, GetPartnerBattler(battler), move))
                    BtlController_EmitTwoReturnValues(battler, B_COMM_TO_ENGINE, B_ACTION_EXEC_SCRIPT, (chosenMoveIndex) | (GetPartnerBattler(battler) << 8));
                else
                    BtlController_EmitTwoReturnValues(battler, B_COMM_TO_ENGINE, B_ACTION_EXEC_SCRIPT, (chosenMoveIndex) | (targetBattler << 8));
            }
            else
            {
                BtlController_EmitTwoReturnValues(battler, B_COMM_TO_ENGINE, B_ACTION_EXEC_SCRIPT, (chosenMoveIndex) | (targetBattler << 8));
            }
        }
        else
            BtlController_EmitTwoReturnValues(battler, B_COMM_TO_ENGINE, B_ACTION_EXEC_SCRIPT, (chosenMoveIndex) | (GetBattlerAtPosition(B_POSITION_PLAYER_LEFT) << 8));

        BtlController_Complete(battler);
    }
}

static void OpponentHandleChooseItem(enum BattlerId battler)
{
    BtlController_EmitOneReturnValue(battler, B_COMM_TO_ENGINE, gBattleStruct->chosenItem[battler]);
    BtlController_Complete(battler);
}

static void OpponentHandleChoosePokemon(enum BattlerId battler)
{
    s32 chosenMonId;
    enum SwitchType switchType = SWITCH_AFTER_KO;

    AI_SetPerspective(battler); // IA contro IA: pensa il lato di `battler`

    // Choosing Revival Blessing target
    if (gBattleResources->bufferA[battler][1] == PARTY_ACTION_CHOOSE_FAINTED_MON)
    {
        chosenMonId = AI_SelectRevivalBlessingMon(battler);
        if (chosenMonId == PARTY_SIZE)
            chosenMonId = GetFirstFaintedPartyIndex(battler);
        gSelectedMonPartyId = chosenMonId;
    }
    // Switching out
    else if (gBattleStruct->AI_monToSwitchIntoId[battler] == PARTY_SIZE)
    {
        if (IsSwitchOutEffect(GetMoveEffect(gCurrentMove)) || gAiLogicData->ejectButtonSwitch || gAiLogicData->ejectPackSwitch)
            switchType = SWITCH_MID_BATTLE_FORCED;

        // reset the AI data to consider the correct on-field state at time of switch
        // (posizioni relative al lato di chi sceglie: in IA contro IA questa funzione serve anche al lato del giocatore)
        SetBattlerAiData(GetBattlerAtPosition((enum BattlerPosition)(GetBattlerSide(battler) ^ BIT_SIDE)), gAiLogicData);
        if (IsDoubleBattle())
            SetBattlerAiData(GetBattlerAtPosition((enum BattlerPosition)((GetBattlerSide(battler) ^ BIT_SIDE) | BIT_FLANK)), gAiLogicData);

        chosenMonId = GetMostSuitableMonToSwitchInto(battler, switchType);
        // IA VGC: con Baton Pass entra il ricevente designato (quello per cui i boost sono stati accumulati),
        // non il "migliore" generico.
        if ((gAiThinkingStruct->aiFlags[battler] & AI_FLAG_VGC_DOUBLES) && gBattlerAttacker == battler
         && GetMoveEffect(gCurrentMove) == EFFECT_BATON_PASS)
        {
            u32 recipient = AI_VgcBatonRecipient(battler);
            if (recipient < PARTY_SIZE)
                chosenMonId = recipient;
        }
        // IA VGC: dopo un KO entra chi fa la coppia migliore con chi è rimasto in campo.
        else if ((gAiThinkingStruct->aiFlags[battler] & AI_FLAG_VGC_DOUBLES) && IsDoubleBattle()
              && switchType == SWITCH_AFTER_KO && chosenMonId < PARTY_SIZE)
        {
            chosenMonId = AI_VgcReplacementAfterKo(battler, chosenMonId);
        }
        if (chosenMonId == PARTY_SIZE) // Advanced logic failed so we pick the next available battler
        {
            enum BattlerId battler1, battler2;
            s32 lastId = GetAILastPartyIndex(battler); // + 1

            if (!IsDoubleBattle())
            {
                battler2 = battler1 = GetBattlerAtPosition((enum BattlerPosition)GetBattlerSide(battler));
            }
            else
            {
                battler1 = GetBattlerAtPosition((enum BattlerPosition)GetBattlerSide(battler));
                battler2 = GetBattlerAtPosition((enum BattlerPosition)(GetBattlerSide(battler) | BIT_FLANK));
            }

            for (chosenMonId = 0; chosenMonId < lastId; chosenMonId++)
            {
                if (IsValidForBattle(&gParties[GetBattlerTrainer(battler)][chosenMonId])
                 && !IsPartyMonOnFieldOrChosenToSwitch(battler, chosenMonId, battler1, battler2))
                    break;
            }
        }

        gBattleStruct->monToSwitchIntoId[battler] = chosenMonId;
    }
    else
    {
        chosenMonId = gBattleStruct->AI_monToSwitchIntoId[battler];
        gBattleStruct->AI_monToSwitchIntoId[battler] = PARTY_SIZE;
        gBattleStruct->monToSwitchIntoId[battler] = chosenMonId;
    }
    // IA VGC: mai lo stesso Pokémon in due posti (es. il compagno è appena uscito con U-turn e quello che entra al
    // suo posto è stato scelto anche qui, o una scelta preparata prima non vale più): si prende il primo libero.
    if (IsDoubleBattle() && gBattleResources->bufferA[battler][1] != PARTY_ACTION_CHOOSE_FAINTED_MON
     && (gAiThinkingStruct->aiFlags[battler] & AI_FLAG_VGC_DOUBLES))
    {
        enum BattlerId b1 = GetBattlerAtPosition((enum BattlerPosition)GetBattlerSide(battler));
        enum BattlerId b2 = GetBattlerAtPosition((enum BattlerPosition)(GetBattlerSide(battler) | BIT_FLANK));
        enum BattlerId partner = GetPartnerBattler(battler);
        bool32 bad = chosenMonId >= PARTY_SIZE || !IsValidForBattle(&gParties[GetBattlerTrainer(battler)][chosenMonId])
                  || chosenMonId == gBattleStruct->monToSwitchIntoId[partner]
                  || (IsBattlerAlive(partner) && chosenMonId == gBattlerPartyIndexes[partner]);
        if (bad)
        {
            s32 id, lastId = GetAILastPartyIndex(battler);
            for (id = 0; id < lastId; id++)
            {
                if (IsValidForBattle(&gParties[GetBattlerTrainer(battler)][id]) && !IsPartyMonOnFieldOrChosenToSwitch(battler, id, b1, b2)
                 && id != gBattleStruct->monToSwitchIntoId[partner])
                {
                    chosenMonId = id;
                    gBattleStruct->monToSwitchIntoId[battler] = chosenMonId;
                    break;
                }
            }
        }
    }
    #if TESTING
    TestRunner_Battle_CheckSwitch(battler, chosenMonId);
    #endif // TESTING
    BtlController_EmitChosenMonReturnValue(battler, B_COMM_TO_ENGINE, chosenMonId, NULL);
    BtlController_Complete(battler);
}

static void OpponentHandleIntroTrainerBallThrow(enum BattlerId battler)
{
    BtlController_HandleIntroTrainerBallThrow(battler, 0, NULL, 0, Intro_TryShinyAnimShowHealthbox);
}

static void OpponentHandleDrawPartyStatusSummary(enum BattlerId battler)
{
    BtlController_HandleDrawPartyStatusSummary(battler, B_SIDE_OPPONENT, TRUE);
}

static void OpponentHandleEndLinkBattle(enum BattlerId battler)
{
    if (gBattleTypeFlags & BATTLE_TYPE_LINK && !(gBattleTypeFlags & BATTLE_TYPE_IS_MASTER))
    {
        gMain.inBattle = FALSE;
        gMain.callback1 = gPreBattleCallback1;
        SetMainCallback2(gMain.savedCallback);
    }
    BtlController_Complete(battler);
}

// IA contro IA (B_FLAG_AI_VS_AI_BATTLE): il lato del giocatore, comandato dal controller "partner", sceglie
// azioni e cambi con la stessa logica dell'IA avversaria (pianificatore VGC compreso). Vedi
// battle_controller_player_partner.c e include/battle_ai_perspective.h.
void OpponentAi_HandleChooseAction(enum BattlerId battler)
{
    OpponentHandleChooseAction(battler);
}

void OpponentAi_HandleChoosePokemon(enum BattlerId battler)
{
    OpponentHandleChoosePokemon(battler);
}

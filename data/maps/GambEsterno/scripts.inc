GambEsterno_MapScripts::
	.byte 0

GambEsterno_EventScript_Enzo::
	lock
	faceplayer
	msgbox GambEsterno_Text_EnzoAskBattle, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, GambEsterno_EventScript_EnzoDecline
	@ Formato della lotta: la team preview non lo richiede più.
	msgbox GambEsterno_Text_EnzoAskFormat, MSGBOX_DEFAULT
	dynmultipush GambEsterno_Text_FormatBo1, 1
	dynmultipush GambEsterno_Text_FormatBo3, 3
	dynmultistack 0, 0, FALSE, 2, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, GambEsterno_EventScript_EnzoDecline
	copyvar VAR_0x8004, VAR_RESULT
	special TeamPreview_SetFormatFromScript
	closemessage
	trainerbattle LOCALID_NONE, TRAINER_ENZO, GambEsterno_Text_EnzoIntro, GambEsterno_Text_EnzoDefeat, NULL, LOCALID_NONE, TRAINER_NONE, NULL, NULL, NULL, GambEsterno_Text_EnzoVictory, GambEsterno_Text_EnzoNeedTwoMons, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE
	msgbox GambEsterno_Text_EnzoPostBattle, MSGBOX_DEFAULT
	release
	end

GambEsterno_EventScript_EnzoDecline::
	msgbox GambEsterno_Text_EnzoDecline, MSGBOX_DEFAULT
	release
	end

GambEsterno_Text_EnzoNeedTwoMons:
	.string "Questa è una Lotta in Doppio!\n"
	.string "Ti servono almeno due Pokémon pronti.$"

GambEsterno_Text_EnzoAskBattle:
	.string "webbello.\n"
	.string "Sei venuto a farti schiattere il capone?$"

GambEsterno_Text_EnzoAskFormat:
	.string "Lotta secca o al meglio di tre?$"

GambEsterno_Text_FormatBo1:
	.string "Lotta singola (Bo1)$"

GambEsterno_Text_FormatBo3:
	.string "Al meglio di 3 (Bo3)$"

GambEsterno_Text_EnzoIntro:
	.string "Capiamo$"

GambEsterno_Text_EnzoVictory:
	.string "Skill issue$"

GambEsterno_Text_EnzoDefeat:
	.string "Che culo rotto che gioco di merda$"

GambEsterno_Text_EnzoPostBattle:
	.string "Bella lotta! Quando vuoi riprovarci,\n"
	.string "sai dove trovarmi.$"

GambEsterno_Text_EnzoDecline:
	.string "Hai paura? Torna pure quando sei pronto.$"


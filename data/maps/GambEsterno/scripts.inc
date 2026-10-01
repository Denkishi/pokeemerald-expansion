GambEsterno_MapScripts::
	.byte 0

GambEsterno_EventScript_Enzo::
	lock
	faceplayer
	msgbox GambEsterno_Text_EnzoAskBattle, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, GambEsterno_EventScript_EnzoDecline
	trainerbattle LOCALID_NONE, TRAINER_ENZO, GambEsterno_Text_EnzoIntro, GambEsterno_Text_EnzoDefeat, NULL, LOCALID_NONE, TRAINER_NONE, NULL, NULL, NULL, GambEsterno_Text_EnzoVictory, NULL, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE
	msgbox GambEsterno_Text_EnzoPostBattle, MSGBOX_DEFAULT
	release
	end

GambEsterno_EventScript_EnzoDecline::
	msgbox GambEsterno_Text_EnzoDecline, MSGBOX_DEFAULT
	release
	end

GambEsterno_Text_EnzoAskBattle:
	.string "Yo! Sono Enzo.\n"
	.string "Vuoi farti una lotta contro il mio team?$"

GambEsterno_Text_EnzoIntro:
	.string "Capiamo che sai fare$"

GambEsterno_Text_EnzoVictory:
	.string "Skill issue$"

GambEsterno_Text_EnzoDefeat:
	.string "Che culo rotto che gioco di merda$"

GambEsterno_Text_EnzoPostBattle:
	.string "Bella lotta! Quando vuoi riprovarci,\n"
	.string "sai dove trovarmi.$"

GambEsterno_Text_EnzoDecline:
	.string "Hai paura? Torna pure quando sei pronto.$"


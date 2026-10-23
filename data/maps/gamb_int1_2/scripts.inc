gamb_int1_2_MapScripts::
	.byte 0

gamb_int1_2_EventScript_PC::
	lockall
	playse SE_PC_LOGIN
	msgbox gamb_int1_2_Text_BootPC, MSGBOX_DEFAULT
gamb_int1_2_EventScript_PCMenu:
	msgbox gamb_int1_2_Text_SelectOption, MSGBOX_DEFAULT
	dynmultipush gamb_int1_2_Text_OptionVgc, 0
	dynmultipush gamb_int1_2_Text_OptionUnknown, 1
	dynmultipush gamb_int1_2_Text_OptionExit, 2
	dynmultistack 0, 0, FALSE, 3, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, gamb_int1_2_EventScript_PC_VGC
	case 1, gamb_int1_2_EventScript_PC_Unknown
	case 2, gamb_int1_2_EventScript_PC_Exit
	case MULTI_B_PRESSED, gamb_int1_2_EventScript_PC_Exit
	releaseall
	end

gamb_int1_2_EventScript_PC_VGC::
	closemessage
	special OpenVgcTrophiesScreen
	waitstate
	goto gamb_int1_2_EventScript_PCMenu

gamb_int1_2_EventScript_PC_Unknown::
	msgbox gamb_int1_2_Text_UnknownDati, MSGBOX_DEFAULT
	goto gamb_int1_2_EventScript_PCMenu

gamb_int1_2_EventScript_PC_Exit::
	playse SE_PC_OFF
	msgbox gamb_int1_2_Text_PCOff, MSGBOX_DEFAULT
	releaseall
	end

gamb_int1_2_Text_BootPC:
	.string "Il PC di Via Gambardella è acceso.$"

gamb_int1_2_Text_SelectOption:
	.string "Cosa vuoi consultare?$"

gamb_int1_2_Text_OptionVgc:
	.string "GAMBARDELLA VGC$"

gamb_int1_2_Text_OptionUnknown:
	.string "????$"

gamb_int1_2_Text_OptionExit:
	.string "ESCI$"

gamb_int1_2_Text_UnknownDati:
	.string "Dati protetti da crittografia.\n"
	.string "Accesso non ancora disponibile.$"

gamb_int1_2_Text_PCOff:
	.string "PC spento.$"

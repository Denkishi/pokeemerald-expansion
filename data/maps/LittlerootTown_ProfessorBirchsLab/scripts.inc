LittlerootTown_ProfessorBirchsLab_MapScripts::
	.byte 0

@ =====================================
@ Prof. Birch - Starter Selection Script
@ =====================================
LittlerootTown_ProfessorBirchsLab_EventScript_Birch::
	lock
	faceplayer
	goto_if_set FLAG_SYS_POKEMON_GET, LittlerootTown_ProfessorBirchsLab_EventScript_BirchAfterStarter
	msgbox LittlerootTown_ProfessorBirchsLab_Text_Buonasera, MSGBOX_DEFAULT
	msgbox LittlerootTown_ProfessorBirchsLab_Text_ChooseStarter, MSGBOX_DEFAULT
	goto LittlerootTown_ProfessorBirchsLab_EventScript_ChooseStarter
	end

LittlerootTown_ProfessorBirchsLab_EventScript_ChooseStarter::
	dynmultipush gText_Turtwig, 0
	dynmultipush gText_Oshawott, 1
	dynmultipush gText_Fennekin, 2
	dynmultipush gText_Rowlet, 3
	dynmultipush gText_Scorbunny, 4
	dynmultipush gText_Quaxly, 5
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, LittlerootTown_ProfessorBirchsLab_EventScript_CancelChoice
	goto_if_eq VAR_RESULT, 0, LittlerootTown_ProfessorBirchsLab_EventScript_GiveTurtwig
	goto_if_eq VAR_RESULT, 1, LittlerootTown_ProfessorBirchsLab_EventScript_GiveOshawott
	goto_if_eq VAR_RESULT, 2, LittlerootTown_ProfessorBirchsLab_EventScript_GiveFennekin
	goto_if_eq VAR_RESULT, 3, LittlerootTown_ProfessorBirchsLab_EventScript_GiveRowlet
	goto_if_eq VAR_RESULT, 4, LittlerootTown_ProfessorBirchsLab_EventScript_GiveScorbunny
	goto_if_eq VAR_RESULT, 5, LittlerootTown_ProfessorBirchsLab_EventScript_GiveQuaxly
	end

LittlerootTown_ProfessorBirchsLab_EventScript_CancelChoice::
	msgbox LittlerootTown_ProfessorBirchsLab_Text_TakeYourTime, MSGBOX_DEFAULT
	release
	end

@ === Give Turtwig (Gen 4) ===
LittlerootTown_ProfessorBirchsLab_EventScript_GiveTurtwig::
	bufferspeciesname STR_VAR_1, SPECIES_TURTWIG
	setvar VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_TURTWIG
	givemon SPECIES_TURTWIG, 5
	goto LittlerootTown_ProfessorBirchsLab_EventScript_FinishGive
	end

@ === Give Oshawott (Gen 5) ===
LittlerootTown_ProfessorBirchsLab_EventScript_GiveOshawott::
	bufferspeciesname STR_VAR_1, SPECIES_OSHAWOTT
	setvar VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_OSHAWOTT
	givemon SPECIES_OSHAWOTT, 5
	goto LittlerootTown_ProfessorBirchsLab_EventScript_FinishGive
	end

@ === Give Fennekin (Gen 6) ===
LittlerootTown_ProfessorBirchsLab_EventScript_GiveFennekin::
	bufferspeciesname STR_VAR_1, SPECIES_FENNEKIN
	setvar VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_FENNEKIN
	givemon SPECIES_FENNEKIN, 5
	goto LittlerootTown_ProfessorBirchsLab_EventScript_FinishGive
	end

@ === Give Rowlet (Gen 7) ===
LittlerootTown_ProfessorBirchsLab_EventScript_GiveRowlet::
	bufferspeciesname STR_VAR_1, SPECIES_ROWLET
	setvar VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_ROWLET
	givemon SPECIES_ROWLET, 5
	goto LittlerootTown_ProfessorBirchsLab_EventScript_FinishGive
	end

@ === Give Scorbunny (Gen 8) ===
LittlerootTown_ProfessorBirchsLab_EventScript_GiveScorbunny::
	bufferspeciesname STR_VAR_1, SPECIES_SCORBUNNY
	setvar VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_SCORBUNNY
	givemon SPECIES_SCORBUNNY, 5
	goto LittlerootTown_ProfessorBirchsLab_EventScript_FinishGive
	end

@ === Give Quaxly (Gen 9) ===
LittlerootTown_ProfessorBirchsLab_EventScript_GiveQuaxly::
	bufferspeciesname STR_VAR_1, SPECIES_QUAXLY
	setvar VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_QUAXLY
	givemon SPECIES_QUAXLY, 5
	goto LittlerootTown_ProfessorBirchsLab_EventScript_FinishGive
	end

@ === Common finish after giving Pokemon ===
LittlerootTown_ProfessorBirchsLab_EventScript_FinishGive::
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PARTY, LittlerootTown_ProfessorBirchsLab_EventScript_GivenToParty
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PC, LittlerootTown_ProfessorBirchsLab_EventScript_GivenToPC
	goto Common_EventScript_NoMoreRoomForPokemon
	end

LittlerootTown_ProfessorBirchsLab_EventScript_GivenToParty::
	playfanfare MUS_OBTAIN_ITEM
	message LittlerootTown_ProfessorBirchsLab_Text_ReceivedPokemon
	waitmessage
	waitfanfare
	setflag FLAG_SYS_POKEMON_GET
	msgbox gText_NicknameThisPokemon, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, LittlerootTown_ProfessorBirchsLab_EventScript_DoneStarter
	call Common_EventScript_GetGiftMonPartySlot
	call Common_EventScript_NameReceivedPartyMon
	goto LittlerootTown_ProfessorBirchsLab_EventScript_DoneStarter
	end

LittlerootTown_ProfessorBirchsLab_EventScript_GivenToPC::
	playfanfare MUS_OBTAIN_ITEM
	message LittlerootTown_ProfessorBirchsLab_Text_ReceivedPokemon
	waitmessage
	waitfanfare
	setflag FLAG_SYS_POKEMON_GET
	msgbox gText_NicknameThisPokemon, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, LittlerootTown_ProfessorBirchsLab_EventScript_TransferToPC
	call Common_EventScript_NameReceivedBoxMon
	goto LittlerootTown_ProfessorBirchsLab_EventScript_TransferToPC
	end

LittlerootTown_ProfessorBirchsLab_EventScript_TransferToPC::
	call Common_EventScript_TransferredToPC
	goto LittlerootTown_ProfessorBirchsLab_EventScript_DoneStarter
	end

LittlerootTown_ProfessorBirchsLab_EventScript_DoneStarter::
	msgbox LittlerootTown_ProfessorBirchsLab_Text_GoodLuck, MSGBOX_DEFAULT
	release
	end

LittlerootTown_ProfessorBirchsLab_EventScript_BirchAfterStarter::
	msgbox LittlerootTown_ProfessorBirchsLab_Text_BirchAfter, MSGBOX_DEFAULT
	release
	end

@ === Aide script (simple dialogue) ===
LittlerootTown_ProfessorBirchsLab_EventScript_Aide::
	msgbox LittlerootTown_ProfessorBirchsLab_Text_Aide, MSGBOX_NPC
	end

@ === Background event scripts ===
LittlerootTown_ProfessorBirchsLab_EventScript_Machine::
	msgbox LittlerootTown_ProfessorBirchsLab_Text_Machine, MSGBOX_SIGN
	end

LittlerootTown_ProfessorBirchsLab_EventScript_Book::
	msgbox LittlerootTown_ProfessorBirchsLab_Text_Book, MSGBOX_SIGN
	end

LittlerootTown_ProfessorBirchsLab_EventScript_Bookshelf::
	msgbox LittlerootTown_ProfessorBirchsLab_Text_Bookshelf, MSGBOX_SIGN
	end

LittlerootTown_ProfessorBirchsLab_EventScript_PC::
	special DoPCTurnOnEffect
	playse SE_PC_ON
	msgbox LittlerootTown_ProfessorBirchsLab_Text_PC, MSGBOX_SIGN
	special DoPCTurnOffEffect
	end

@ ============================
@ Text Strings
@ ============================
gText_Turtwig:
	.string "TURTWIG$"

gText_Oshawott:
	.string "OSHAWOTT$"

gText_Fennekin:
	.string "FENNEKIN$"

gText_Rowlet:
	.string "ROWLET$"

gText_Scorbunny:
	.string "SCORBUNNY$"

gText_Quaxly:
	.string "QUAXLY$"

LittlerootTown_ProfessorBirchsLab_Text_Buonasera:
	.string "PROF. BIRCH: Buonasera!\n"
	.string "Benvenuto nel mio laboratorio!$"

LittlerootTown_ProfessorBirchsLab_Text_ChooseStarter:
	.string "PROF. BIRCH: Ho qui dei POKéMON\n"
	.string "speciali per te!\p"
	.string "Scegli il tuo compagno\n"
	.string "di avventura!$"

LittlerootTown_ProfessorBirchsLab_Text_ReceivedPokemon:
	.string "{PLAYER} ha ricevuto {STR_VAR_1}!$"

LittlerootTown_ProfessorBirchsLab_Text_GoodLuck:
	.string "PROF. BIRCH: Fantastico!\n"
	.string "Prenditi cura di {STR_VAR_1}!\p"
	.string "Buona fortuna nella tua\n"
	.string "avventura!$"

LittlerootTown_ProfessorBirchsLab_Text_TakeYourTime:
	.string "PROF. BIRCH: Prenditi il tuo\n"
	.string "tempo per decidere!$"

LittlerootTown_ProfessorBirchsLab_Text_BirchAfter:
	.string "PROF. BIRCH: Come sta andando\n"
	.string "la tua avventura?\p"
	.string "Ricorda, il mondo è pieno di\n"
	.string "POKéMON da scoprire!$"

LittlerootTown_ProfessorBirchsLab_Text_Aide:
	.string "Benvenuto nel laboratorio del\n"
	.string "PROF. BIRCH!\p"
	.string "Il professore studia i POKéMON\n"
	.string "di tutto il mondo!$"

LittlerootTown_ProfessorBirchsLab_Text_Machine:
	.string "È un macchinario sofisticato.$"

LittlerootTown_ProfessorBirchsLab_Text_Book:
	.string "Libri di ricerca sui POKéMON\n"
	.string "sono ovunque.$"

LittlerootTown_ProfessorBirchsLab_Text_Bookshelf:
	.string "Scaffali pieni di ricerche\n"
	.string "sui POKéMON.$"

LittlerootTown_ProfessorBirchsLab_Text_PC:
	.string "PROF. BIRCH ha il PC acceso.\n"
	.string "Meglio non toccarlo.$"

@ ============================
@ Stub scripts for external references
@ ============================
@ Referenced by map.json object events
LittlerootTown_ProfessorBirchsLab_EventScript_Rival::
	msgbox LittlerootTown_ProfessorBirchsLab_Text_Aide, MSGBOX_NPC
	end

LittlerootTown_ProfessorBirchsLab_EventScript_Cyndaquil::
	end

LittlerootTown_ProfessorBirchsLab_EventScript_Totodile::
	end

LittlerootTown_ProfessorBirchsLab_EventScript_Chikorita::
	end

@ Referenced by src/field_control_avatar.c
LittlerootTown_ProfessorBirchsLab_EventScript_ScottAboardSSTidalCall::
	end

@ Referenced by data/scripts/prof_birch.inc
EventScript_RegisterProfBirch::
	end

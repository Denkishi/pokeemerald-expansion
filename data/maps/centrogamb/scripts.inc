centrogamb_MapScripts::
	.byte 0

// ===========================================================================
// GENTLEMAN: Cambio Abilità
// ===========================================================================
Centrogamb_EventScript_Gentleman::
	lock
	faceplayer
	msgbox Centrogamb_Text_GentlemanIntro, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Centrogamb_EventScript_Cancel
	msgbox Centrogamb_Text_SelectPokemon, MSGBOX_DEFAULT
	special ChoosePartyMon
	waitstate
	goto_if_ge VAR_0x8004, PARTY_SIZE, Centrogamb_EventScript_Cancel
	specialvar VAR_RESULT, ScriptGetPartyMonSpecies
	goto_if_eq VAR_RESULT, SPECIES_EGG, Centrogamb_EventScript_CantModifyEgg
	callnative DebugNative_GetAbilityNames
	dynmultipush gStringVar1, 0
	dynmultipush gStringVar2, 1
	dynmultipush gStringVar3, 2
	dynmultipush Centrogamb_Text_Cancel, 3
	dynmultistack 0, 0, FALSE, 4, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 3, Centrogamb_EventScript_Cancel
	case MULTI_B_PRESSED, Centrogamb_EventScript_Cancel
	special SetAbility
	playse SE_EXP
	waitse
	msgbox Centrogamb_Text_AbilityChanged, MSGBOX_DEFAULT
	release
	end

// ===========================================================================
// GIRL: Cambio Natura con info su statistiche (+Stat, -Stat)
// ===========================================================================
Centrogamb_EventScript_Girl::
	lock
	faceplayer
	msgbox Centrogamb_Text_GirlIntro, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Centrogamb_EventScript_Cancel
	msgbox Centrogamb_Text_SelectPokemon, MSGBOX_DEFAULT
	special ChoosePartyMon
	waitstate
	goto_if_ge VAR_0x8004, PARTY_SIZE, Centrogamb_EventScript_Cancel
	specialvar VAR_RESULT, ScriptGetPartyMonSpecies
	goto_if_eq VAR_RESULT, SPECIES_EGG, Centrogamb_EventScript_CantModifyEgg
	msgbox Centrogamb_Text_ChooseNaturePrompt, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_Nature_Adamant, 3
	dynmultipush Centrogamb_Text_Nature_Brave, 2
	dynmultipush Centrogamb_Text_Nature_Lonely, 1
	dynmultipush Centrogamb_Text_Nature_Naughty, 4
	dynmultipush Centrogamb_Text_Nature_Bold, 5
	dynmultipush Centrogamb_Text_Nature_Relaxed, 7
	dynmultipush Centrogamb_Text_Nature_Impish, 8
	dynmultipush Centrogamb_Text_Nature_Lax, 9
	dynmultipush Centrogamb_Text_Nature_Modest, 15
	dynmultipush Centrogamb_Text_Nature_Quiet, 17
	dynmultipush Centrogamb_Text_Nature_Mild, 16
	dynmultipush Centrogamb_Text_Nature_Rash, 19
	dynmultipush Centrogamb_Text_Nature_Calm, 20
	dynmultipush Centrogamb_Text_Nature_Sassy, 22
	dynmultipush Centrogamb_Text_Nature_Careful, 23
	dynmultipush Centrogamb_Text_Nature_Gentle, 21
	dynmultipush Centrogamb_Text_Nature_Timid, 10
	dynmultipush Centrogamb_Text_Nature_Jolly, 13
	dynmultipush Centrogamb_Text_Nature_Hasty, 11
	dynmultipush Centrogamb_Text_Nature_Naive, 14
	dynmultipush Centrogamb_Text_Nature_Hardy, 0
	dynmultipush Centrogamb_Text_Nature_Docile, 6
	dynmultipush Centrogamb_Text_Nature_Serious, 12
	dynmultipush Centrogamb_Text_Nature_Bashful, 18
	dynmultipush Centrogamb_Text_Nature_Quirky, 24
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case MULTI_B_PRESSED, Centrogamb_EventScript_Cancel
	special SetHiddenNature
	playse SE_EXP
	waitse
	msgbox Centrogamb_Text_NatureChanged, MSGBOX_DEFAULT
	release
	end

// ===========================================================================
// BOY: EVs, IVs, Felicità e Tipo Tera
// ===========================================================================
Centrogamb_EventScript_Boy::
	lock
	faceplayer
	msgbox Centrogamb_Text_BoyIntro, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Centrogamb_EventScript_Cancel
	msgbox Centrogamb_Text_SelectPokemon, MSGBOX_DEFAULT
	special ChoosePartyMon
	waitstate
	goto_if_ge VAR_0x8004, PARTY_SIZE, Centrogamb_EventScript_Cancel
	specialvar VAR_RESULT, ScriptGetPartyMonSpecies
	goto_if_eq VAR_RESULT, SPECIES_EGG, Centrogamb_EventScript_CantModifyEgg
Centrogamb_EventScript_BoyStatMenu:
	msgbox Centrogamb_Text_WhatToModify, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_ModEVs, 0
	dynmultipush Centrogamb_Text_ModIVs, 1
	dynmultipush Centrogamb_Text_ModFriendship, 2
	dynmultipush Centrogamb_Text_ModTera, 3
	dynmultipush Centrogamb_Text_Cancel, 4
	dynmultistack 0, 0, FALSE, 5, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, Centrogamb_EventScript_ModifyEVs
	case 1, Centrogamb_EventScript_ModifyIVs
	case 2, Centrogamb_EventScript_ModifyFriendship
	case 3, Centrogamb_EventScript_ModifyTera
	case 4, Centrogamb_EventScript_Cancel
	case MULTI_B_PRESSED, Centrogamb_EventScript_Cancel
	end

Centrogamb_EventScript_ModifyEVs::
	special BufferMonEVs
	msgbox Centrogamb_Text_CurrentEVs, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_OptionCustomStat, 0
	dynmultipush Centrogamb_Text_OptionPresets, 1
	dynmultipush Centrogamb_Text_EV_ResetZero, 2
	dynmultipush Centrogamb_Text_Cancel, 3
	dynmultistack 0, 0, FALSE, 4, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, Centrogamb_EventScript_EVPickStat
	case 1, Centrogamb_EventScript_EVPresetMenu
	case 2, Centrogamb_EventScript_EVResetAll
	case 3, Centrogamb_EventScript_BoyStatMenu
	case MULTI_B_PRESSED, Centrogamb_EventScript_BoyStatMenu
	end

Centrogamb_EventScript_EVResetAll::
	setvar VAR_RESULT, 7
	special SetMonEVSpread
	playse SE_EXP
	waitse
	msgbox Centrogamb_Text_StatsUpdated, MSGBOX_DEFAULT
	goto Centrogamb_EventScript_ModifyEVs
	end

Centrogamb_EventScript_EVPresetMenu::
	msgbox Centrogamb_Text_ChooseEVSpread, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_EV_FastPhysical, 0
	dynmultipush Centrogamb_Text_EV_FastSpecial, 1
	dynmultipush Centrogamb_Text_EV_BulkyPhysical, 2
	dynmultipush Centrogamb_Text_EV_BulkySpecial, 3
	dynmultipush Centrogamb_Text_EV_PhysicalWall, 4
	dynmultipush Centrogamb_Text_EV_SpecialWall, 5
	dynmultipush Centrogamb_Text_EV_FastSupport, 6
	dynmultipush Centrogamb_Text_Cancel, 7
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 7, Centrogamb_EventScript_ModifyEVs
	case MULTI_B_PRESSED, Centrogamb_EventScript_ModifyEVs
	special SetMonEVSpread
	playse SE_EXP
	waitse
	msgbox Centrogamb_Text_StatsUpdated, MSGBOX_DEFAULT
	goto Centrogamb_EventScript_ModifyEVs
	end

Centrogamb_EventScript_EVPickStat::
	msgbox Centrogamb_Text_PickStatPrompt, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_StatHP, 0
	dynmultipush Centrogamb_Text_StatAtk, 1
	dynmultipush Centrogamb_Text_StatDef, 2
	dynmultipush Centrogamb_Text_StatSpe, 3
	dynmultipush Centrogamb_Text_StatSpA, 4
	dynmultipush Centrogamb_Text_StatSpD, 5
	dynmultipush Centrogamb_Text_Cancel, 6
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 6, Centrogamb_EventScript_ModifyEVs
	case MULTI_B_PRESSED, Centrogamb_EventScript_ModifyEVs
	copyvar VAR_0x8005, VAR_RESULT
Centrogamb_EventScript_EVStatEditLoop:
	setvar VAR_RESULT, 0
	special BufferSingleStatInfo
	msgbox Centrogamb_Text_StatCurrentVal, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_SetExactValue, 0
	dynmultipush Centrogamb_Text_Max252, 1
	dynmultipush Centrogamb_Text_Zero, 2
	dynmultipush Centrogamb_Text_Add10, 3
	dynmultipush Centrogamb_Text_Add4, 4
	dynmultipush Centrogamb_Text_Add1, 5
	dynmultipush Centrogamb_Text_Sub10, 6
	dynmultipush Centrogamb_Text_Sub4, 7
	dynmultipush Centrogamb_Text_Sub1, 8
	dynmultipush Centrogamb_Text_Cancel, 9
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, Centrogamb_EventScript_EVExactList
	case 1, Centrogamb_EventScript_EVSetMax
	case 2, Centrogamb_EventScript_EVSetZero
	case 3, Centrogamb_EventScript_EVAdd10
	case 4, Centrogamb_EventScript_EVAdd4
	case 5, Centrogamb_EventScript_EVAdd1
	case 6, Centrogamb_EventScript_EVSub10
	case 7, Centrogamb_EventScript_EVSub4
	case 8, Centrogamb_EventScript_EVSub1
	case 9, Centrogamb_EventScript_EVPickStat
	case MULTI_B_PRESSED, Centrogamb_EventScript_EVPickStat
	end

Centrogamb_EventScript_EVExactList::
	special PopulateEVValues
	dynmultipush Centrogamb_Text_Cancel, 0xFFFF
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0xFFFF, Centrogamb_EventScript_EVStatEditLoop
	case MULTI_B_PRESSED, Centrogamb_EventScript_EVStatEditLoop
	special SetSingleStatEV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_EVStatEditLoop
	end

Centrogamb_EventScript_EVSetMax::
	setvar VAR_RESULT, 252
	special SetSingleStatEV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_EVStatEditLoop
	end

Centrogamb_EventScript_EVSetZero::
	setvar VAR_RESULT, 0
	special SetSingleStatEV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_EVStatEditLoop
	end

Centrogamb_EventScript_EVAdd10::
	setvar VAR_RESULT, 3
	special AdjustSingleStatEV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_EVStatEditLoop
	end

Centrogamb_EventScript_EVAdd4::
	setvar VAR_RESULT, 2
	special AdjustSingleStatEV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_EVStatEditLoop
	end

Centrogamb_EventScript_EVAdd1::
	setvar VAR_RESULT, 1
	special AdjustSingleStatEV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_EVStatEditLoop
	end

Centrogamb_EventScript_EVSub10::
	setvar VAR_RESULT, 6
	special AdjustSingleStatEV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_EVStatEditLoop
	end

Centrogamb_EventScript_EVSub4::
	setvar VAR_RESULT, 5
	special AdjustSingleStatEV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_EVStatEditLoop
	end

Centrogamb_EventScript_EVSub1::
	setvar VAR_RESULT, 4
	special AdjustSingleStatEV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_EVStatEditLoop
	end

// ===========================================================================
// MODIFICA IV
// ===========================================================================
Centrogamb_EventScript_ModifyIVs::
	special BufferMonIVs
	msgbox Centrogamb_Text_CurrentIVs, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_OptionCustomStat, 0
	dynmultipush Centrogamb_Text_OptionPresets, 1
	dynmultipush Centrogamb_Text_IV_All31, 2
	dynmultipush Centrogamb_Text_IV_All0, 3
	dynmultipush Centrogamb_Text_Cancel, 4
	dynmultistack 0, 0, FALSE, 5, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, Centrogamb_EventScript_IVPickStat
	case 1, Centrogamb_EventScript_IVPresetMenu
	case 2, Centrogamb_EventScript_IVSetAll31
	case 3, Centrogamb_EventScript_IVSetAll0
	case 4, Centrogamb_EventScript_BoyStatMenu
	case MULTI_B_PRESSED, Centrogamb_EventScript_BoyStatMenu
	end

Centrogamb_EventScript_IVSetAll31::
	setvar VAR_RESULT, 0
	special SetMonIVSpread
	playse SE_EXP
	waitse
	msgbox Centrogamb_Text_StatsUpdated, MSGBOX_DEFAULT
	goto Centrogamb_EventScript_ModifyIVs
	end

Centrogamb_EventScript_IVSetAll0::
	setvar VAR_RESULT, 4
	special SetMonIVSpread
	playse SE_EXP
	waitse
	msgbox Centrogamb_Text_StatsUpdated, MSGBOX_DEFAULT
	goto Centrogamb_EventScript_ModifyIVs
	end

Centrogamb_EventScript_IVPresetMenu::
	msgbox Centrogamb_Text_ChooseIVSpread, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_IV_All31, 0
	dynmultipush Centrogamb_Text_IV_NoAtk, 1
	dynmultipush Centrogamb_Text_IV_NoSpe, 2
	dynmultipush Centrogamb_Text_IV_NoAtkNoSpe, 3
	dynmultipush Centrogamb_Text_IV_All0, 4
	dynmultipush Centrogamb_Text_Cancel, 5
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 5, Centrogamb_EventScript_ModifyIVs
	case MULTI_B_PRESSED, Centrogamb_EventScript_ModifyIVs
	special SetMonIVSpread
	playse SE_EXP
	waitse
	msgbox Centrogamb_Text_StatsUpdated, MSGBOX_DEFAULT
	goto Centrogamb_EventScript_ModifyIVs
	end

Centrogamb_EventScript_IVPickStat::
	msgbox Centrogamb_Text_PickStatPrompt, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_StatHP, 0
	dynmultipush Centrogamb_Text_StatAtk, 1
	dynmultipush Centrogamb_Text_StatDef, 2
	dynmultipush Centrogamb_Text_StatSpe, 3
	dynmultipush Centrogamb_Text_StatSpA, 4
	dynmultipush Centrogamb_Text_StatSpD, 5
	dynmultipush Centrogamb_Text_Cancel, 6
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 6, Centrogamb_EventScript_ModifyIVs
	case MULTI_B_PRESSED, Centrogamb_EventScript_ModifyIVs
	copyvar VAR_0x8005, VAR_RESULT
Centrogamb_EventScript_IVStatEditLoop:
	setvar VAR_RESULT, 1
	special BufferSingleStatInfo
	msgbox Centrogamb_Text_StatCurrentVal, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_SetExactValue, 0
	dynmultipush Centrogamb_Text_Max31, 1
	dynmultipush Centrogamb_Text_Zero, 2
	dynmultipush Centrogamb_Text_Add1, 3
	dynmultipush Centrogamb_Text_Sub1, 4
	dynmultipush Centrogamb_Text_Cancel, 5
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, Centrogamb_EventScript_IVExactList
	case 1, Centrogamb_EventScript_IVSetMax
	case 2, Centrogamb_EventScript_IVSetZero
	case 3, Centrogamb_EventScript_IVAdd1
	case 4, Centrogamb_EventScript_IVSub1
	case 5, Centrogamb_EventScript_IVPickStat
	case MULTI_B_PRESSED, Centrogamb_EventScript_IVPickStat
	end

Centrogamb_EventScript_IVExactList::
	special PopulateIVValues
	dynmultipush Centrogamb_Text_Cancel, 0xFFFF
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0xFFFF, Centrogamb_EventScript_IVStatEditLoop
	case MULTI_B_PRESSED, Centrogamb_EventScript_IVStatEditLoop
	special SetSingleStatIV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_IVStatEditLoop
	end

Centrogamb_EventScript_IVSetMax::
	setvar VAR_RESULT, 31
	special SetSingleStatIV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_IVStatEditLoop
	end

Centrogamb_EventScript_IVSetZero::
	setvar VAR_RESULT, 0
	special SetSingleStatIV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_IVStatEditLoop
	end

Centrogamb_EventScript_IVAdd1::
	setvar VAR_RESULT, 1
	special AdjustSingleStatIV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_IVStatEditLoop
	end

Centrogamb_EventScript_IVSub1::
	setvar VAR_RESULT, 2
	special AdjustSingleStatIV
	playse SE_EXP
	waitse
	goto Centrogamb_EventScript_IVStatEditLoop
	end

Centrogamb_EventScript_ModifyFriendship::
	msgbox Centrogamb_Text_ChooseFriendship, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_Friendship_Max, 0
	dynmultipush Centrogamb_Text_Friendship_Min, 1
	dynmultipush Centrogamb_Text_Cancel, 2
	dynmultistack 0, 0, FALSE, 3, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 2, Centrogamb_EventScript_BoyStatMenu
	case MULTI_B_PRESSED, Centrogamb_EventScript_BoyStatMenu
	special SetMonFriendshipDirect
	playse SE_EXP
	waitse
	msgbox Centrogamb_Text_StatsUpdated, MSGBOX_DEFAULT
	goto Centrogamb_EventScript_BoyStatMenu
	end

Centrogamb_EventScript_ModifyTera::
	msgbox Centrogamb_Text_ChooseTeraType, MSGBOX_DEFAULT
	dynmultipush Centrogamb_Text_Tera_Normal, 1
	dynmultipush Centrogamb_Text_Tera_Fighting, 2
	dynmultipush Centrogamb_Text_Tera_Flying, 3
	dynmultipush Centrogamb_Text_Tera_Poison, 4
	dynmultipush Centrogamb_Text_Tera_Ground, 5
	dynmultipush Centrogamb_Text_Tera_Rock, 6
	dynmultipush Centrogamb_Text_Tera_Bug, 7
	dynmultipush Centrogamb_Text_Tera_Ghost, 8
	dynmultipush Centrogamb_Text_Tera_Steel, 9
	dynmultipush Centrogamb_Text_Tera_Fire, 11
	dynmultipush Centrogamb_Text_Tera_Water, 12
	dynmultipush Centrogamb_Text_Tera_Grass, 13
	dynmultipush Centrogamb_Text_Tera_Electric, 14
	dynmultipush Centrogamb_Text_Tera_Psychic, 15
	dynmultipush Centrogamb_Text_Tera_Ice, 16
	dynmultipush Centrogamb_Text_Tera_Dragon, 17
	dynmultipush Centrogamb_Text_Tera_Dark, 18
	dynmultipush Centrogamb_Text_Tera_Fairy, 19
	dynmultipush Centrogamb_Text_Tera_Stellar, 20
	dynmultipush Centrogamb_Text_Cancel, 99
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 99, Centrogamb_EventScript_BoyStatMenu
	case MULTI_B_PRESSED, Centrogamb_EventScript_BoyStatMenu
	special SetMonTeraTypeSpecial
	playse SE_EXP
	waitse
	msgbox Centrogamb_Text_TeraUpdated, MSGBOX_DEFAULT
	goto Centrogamb_EventScript_BoyStatMenu
	end

// ===========================================================================
// Common Handlers
// ===========================================================================
Centrogamb_EventScript_CantModifyEgg::
	msgbox Centrogamb_Text_CantModifyEgg, MSGBOX_DEFAULT
	release
	end

Centrogamb_EventScript_Cancel::
	msgbox Centrogamb_Text_ComeBackLater, MSGBOX_DEFAULT
	release
	end

// ===========================================================================
// Text Strings
// ===========================================================================
Centrogamb_Text_GentlemanIntro:
	.string "Salve! Sono un esperto di abilità.\n"
	.string "Posso modificare l'abilità di uno dei\l"
	.string "tuoi Pokémon. Vuoi procedere?$"

Centrogamb_Text_GirlIntro:
	.string "Ciao! Posso risvegliare il pieno\n"
	.string "potenziale del tuo Pokémon cambiandone\l"
	.string "la Natura. Vuoi che ti aiuti?$"

Centrogamb_Text_BoyIntro:
	.string "Ehi! Posso allenare istantaneamente gli\n"
	.string "EV, perfezionare gli IV, impostare la\l"
	.string "felicità o il Teratipo! Ti interessa?$"

Centrogamb_Text_SelectPokemon:
	.string "Scegli il Pokémon che vuoi modificare.$"

Centrogamb_Text_CantModifyEgg:
	.string "Non posso modificare le caratteristiche\n"
	.string "di un Uovo!$"

Centrogamb_Text_ComeBackLater:
	.string "D'accordo! Torna pure quando desideri.$"

Centrogamb_Text_AbilityChanged:
	.string "Fatto! L'abilità del Pokémon è stata\n"
	.string "modificata con successo!$"

Centrogamb_Text_ChooseNaturePrompt:
	.string "Quale Natura desideri impostare?$"

Centrogamb_Text_NatureChanged:
	.string "Perfetto! La Natura del Pokémon e le\n"
	.string "sue statistiche sono state aggiornate!$"

Centrogamb_Text_WhatToModify:
	.string "Cosa desideri modificare per questo\n"
	.string "Pokémon?$"

Centrogamb_Text_ChooseEVSpread:
	.string "Scegli la distribuzione di EV (Punti Base):$"

Centrogamb_Text_ChooseIVSpread:
	.string "Scegli la combinazione di IV:$"

Centrogamb_Text_ChooseFriendship:
	.string "Scegli il livello di Felicità:$"

Centrogamb_Text_ChooseTeraType:
	.string "Scegli il Teratipo desiderato:$"

Centrogamb_Text_StatsUpdated:
	.string "Statistiche aggiornate con successo!$"

Centrogamb_Text_TeraUpdated:
	.string "Il Teratipo del Pokémon è stato cambiato!$"

Centrogamb_Text_Cancel:
	.string "Annulla$"

// Nature Texts with Stat Modifiers
Centrogamb_Text_Nature_Adamant:
	.string "Decisa (+Atk, -SpA)$"

Centrogamb_Text_Nature_Brave:
	.string "Audace (+Atk, -Vel)$"

Centrogamb_Text_Nature_Lonely:
	.string "Schiva (+Atk, -Def)$"

Centrogamb_Text_Nature_Naughty:
	.string "Birbona (+Atk, -SpD)$"

Centrogamb_Text_Nature_Bold:
	.string "Sicura (+Def, -Atk)$"

Centrogamb_Text_Nature_Relaxed:
	.string "Placida (+Def, -Vel)$"

Centrogamb_Text_Nature_Impish:
	.string "Scaltra (+Def, -SpA)$"

Centrogamb_Text_Nature_Lax:
	.string "Fiacca (+Def, -SpD)$"

Centrogamb_Text_Nature_Modest:
	.string "Modesta (+SpA, -Atk)$"

Centrogamb_Text_Nature_Quiet:
	.string "Quieta (+SpA, -Vel)$"

Centrogamb_Text_Nature_Mild:
	.string "Mite (+SpA, -Def)$"

Centrogamb_Text_Nature_Rash:
	.string "Ardente (+SpA, -SpD)$"

Centrogamb_Text_Nature_Calm:
	.string "Calma (+SpD, -Atk)$"

Centrogamb_Text_Nature_Sassy:
	.string "Vivace (+SpD, -Vel)$"

Centrogamb_Text_Nature_Careful:
	.string "Cauta (+SpD, -SpA)$"

Centrogamb_Text_Nature_Gentle:
	.string "Gentile (+SpD, -Def)$"

Centrogamb_Text_Nature_Timid:
	.string "Timida (+Vel, -Atk)$"

Centrogamb_Text_Nature_Jolly:
	.string "Allegra (+Vel, -SpA)$"

Centrogamb_Text_Nature_Hasty:
	.string "Lesta (+Vel, -Def)$"

Centrogamb_Text_Nature_Naive:
	.string "Ingenua (+Vel, -SpD)$"

Centrogamb_Text_Nature_Hardy:
	.string "Ardita (Neutra)$"

Centrogamb_Text_Nature_Docile:
	.string "Docile (Neutra)$"

Centrogamb_Text_Nature_Serious:
	.string "Seria (Neutra)$"

Centrogamb_Text_Nature_Bashful:
	.string "Pudica (Neutra)$"

Centrogamb_Text_Nature_Quirky:
	.string "Bizzarra (Neutra)$"

// Boy Menu Texts
Centrogamb_Text_ModEVs:
	.string "EVs (Punti Base)$"

Centrogamb_Text_ModIVs:
	.string "IVs (Valori Individuali)$"

Centrogamb_Text_ModFriendship:
	.string "Felicità (Friendship)$"

Centrogamb_Text_ModTera:
	.string "Tipo Tera (Teratipo)$"

// EV Spreads
Centrogamb_Text_EV_FastPhysical:
	.string "252 Atk / 252 Vel / 4 HP$"

Centrogamb_Text_EV_FastSpecial:
	.string "252 SpA / 252 Vel / 4 HP$"

Centrogamb_Text_EV_BulkyPhysical:
	.string "252 HP / 252 Atk / 4 Def$"

Centrogamb_Text_EV_BulkySpecial:
	.string "252 HP / 252 SpA / 4 SpD$"

Centrogamb_Text_EV_PhysicalWall:
	.string "252 HP / 252 Def / 4 SpD$"

Centrogamb_Text_EV_SpecialWall:
	.string "252 HP / 252 SpD / 4 Def$"

Centrogamb_Text_EV_FastSupport:
	.string "252 HP / 252 Vel / 4 Def$"

Centrogamb_Text_EV_ResetZero:
	.string "Azzera tutti gli EV (0)$"

// IV Spreads
Centrogamb_Text_IV_All31:
	.string "Tutti a 31 (6 IV Max)$"

Centrogamb_Text_IV_NoAtk:
	.string "31 Max con 0 Atk$"

Centrogamb_Text_IV_NoSpe:
	.string "31 Max con 0 Vel$"

Centrogamb_Text_IV_NoAtkNoSpe:
	.string "31 Max con 0 Atk e 0 Vel$"

Centrogamb_Text_IV_All0:
	.string "Tutti a 0 (Minimi)$"

// Friendship
Centrogamb_Text_Friendship_Max:
	.string "Massima Felicità (255)$"

Centrogamb_Text_Friendship_Min:
	.string "Minima Felicità (0)$"

// Tera Types
Centrogamb_Text_Tera_Normal:
	.string "Normale$"

Centrogamb_Text_Tera_Fighting:
	.string "Lotta$"

Centrogamb_Text_Tera_Flying:
	.string "Volante$"

Centrogamb_Text_Tera_Poison:
	.string "Veleno$"

Centrogamb_Text_Tera_Ground:
	.string "Terra$"

Centrogamb_Text_Tera_Rock:
	.string "Roccia$"

Centrogamb_Text_Tera_Bug:
	.string "Coleottero$"

Centrogamb_Text_Tera_Ghost:
	.string "Spettro$"

Centrogamb_Text_Tera_Steel:
	.string "Acciaio$"

Centrogamb_Text_Tera_Fire:
	.string "Fuoco$"

Centrogamb_Text_Tera_Water:
	.string "Acqua$"

Centrogamb_Text_Tera_Grass:
	.string "Erba$"

Centrogamb_Text_Tera_Electric:
	.string "Elettro$"

Centrogamb_Text_Tera_Psychic:
	.string "Psico$"

Centrogamb_Text_Tera_Ice:
	.string "Ghiaccio$"

Centrogamb_Text_Tera_Dragon:
	.string "Drago$"

Centrogamb_Text_Tera_Dark:
	.string "Buio$"

Centrogamb_Text_Tera_Fairy:
	.string "Folletto$"

Centrogamb_Text_Tera_Stellar:
	.string "Stellare$"

Centrogamb_Text_CurrentEVs:
	.string "EV attuali:\n"
	.string "{STR_VAR_1}\n"
	.string "{STR_VAR_2}\p"
	.string "Cosa desideri fare?$"

Centrogamb_Text_CurrentIVs:
	.string "IV attuali:\n"
	.string "{STR_VAR_1}\n"
	.string "{STR_VAR_2}\p"
	.string "Cosa desideri fare?$"

Centrogamb_Text_OptionCustomStat:
	.string "Personalizza Singola Stat$"

Centrogamb_Text_OptionPresets:
	.string "Preset Rapidi$"

Centrogamb_Text_PickStatPrompt:
	.string "Quale statistica vuoi modificare?$"

Centrogamb_Text_StatCurrentVal:
	.string "{STR_VAR_1} attuale: {STR_VAR_2}\n"
	.string "Come vuoi intervenire?$"

Centrogamb_Text_StatHP:
	.string "Punti Salute (PS)$"

Centrogamb_Text_StatAtk:
	.string "Attacco$"

Centrogamb_Text_StatDef:
	.string "Difesa$"

Centrogamb_Text_StatSpe:
	.string "Velocità$"

Centrogamb_Text_StatSpA:
	.string "Attacco Speciale$"

Centrogamb_Text_StatSpD:
	.string "Difesa Speciale$"

Centrogamb_Text_SetExactValue:
	.string "Scegli Valore Esatto...$"

Centrogamb_Text_Max252:
	.string "Massimizza (252)$"

Centrogamb_Text_Max31:
	.string "Massimizza (31)$"

Centrogamb_Text_Zero:
	.string "Azzera (0)$"

Centrogamb_Text_Add10:
	.string "+10$"

Centrogamb_Text_Add4:
	.string "+4$"

Centrogamb_Text_Add1:
	.string "+1$"

Centrogamb_Text_Sub10:
	.string "-10$"

Centrogamb_Text_Sub4:
	.string "-4$"

Centrogamb_Text_Sub1:
	.string "-1$"


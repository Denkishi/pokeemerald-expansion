@ Allenatori VGC: lotta in doppio con team preview e scelta del formato (Bo1 / Bo3).
@ Per metterli in mappa: nell'NPC (map.json) usare come "script" il nome VgcTrainer_EventScript_<Nome>.
@ Le frasi sono segnaposto: elenco e spiegazioni in docs/vgc_trainers.md.
@
@ Schema (uguale per tutti): dialogo -> sì/no -> formato -> trainerbattle (doppia, rilottabile) -> frase finale.

	.macro vgc_trainer_script name:req, trainer:req
	.global VgcTrainer_EventScript_\name
VgcTrainer_EventScript_\name:
	lock
	faceplayer
	msgbox VgcTrainer_Text_\name\()_AskBattle, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, VgcTrainer_EventScript_\name\()_Decline
	msgbox VgcTrainer_Text_AskFormat, MSGBOX_DEFAULT
	dynmultipush VgcTrainer_Text_FormatBo1, 1
	dynmultipush VgcTrainer_Text_FormatBo3, 3
	dynmultistack 0, 0, FALSE, 2, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	goto_if_eq VAR_RESULT, MULTI_B_PRESSED, VgcTrainer_EventScript_\name\()_Decline
	copyvar VAR_0x8004, VAR_RESULT
	special TeamPreview_SetFormatFromScript
	closemessage
	trainerbattle LOCALID_NONE, \trainer, VgcTrainer_Text_\name\()_Intro, VgcTrainer_Text_\name\()_Defeat, NULL, LOCALID_NONE, TRAINER_NONE, NULL, NULL, NULL, VgcTrainer_Text_\name\()_Victory, VgcTrainer_Text_NeedTwoMons, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE
	msgbox VgcTrainer_Text_\name\()_PostBattle, MSGBOX_DEFAULT
	release
	end

	.global VgcTrainer_EventScript_\name\()_Decline
VgcTrainer_EventScript_\name\()_Decline:
	msgbox VgcTrainer_Text_\name\()_Decline, MSGBOX_DEFAULT
	release
	end
	.endm

	vgc_trainer_script Simone, TRAINER_SIMONE
	vgc_trainer_script Elia, TRAINER_ELIA
	vgc_trainer_script Patatina, TRAINER_PATATINA
	vgc_trainer_script Tony, TRAINER_TONY
	vgc_trainer_script MarioLett, TRAINER_MARIO_LETT
	vgc_trainer_script Tatore, TRAINER_TATORE
	vgc_trainer_script Mario, TRAINER_MARIO
	vgc_trainer_script Gerozz, TRAINER_GEROZZ
	vgc_trainer_script Carletto, TRAINER_CARLETTO
	vgc_trainer_script GigiPinto, TRAINER_GIGI_PINTO
	vgc_trainer_script Savarotto, TRAINER_SAVAROTTO

@ --- Testi comuni -----------------------------------------------------------
VgcTrainer_Text_NeedTwoMons:
	.string "Questa è una Lotta in Doppio!\n"
	.string "Ti servono almeno due Pokémon pronti.$"

VgcTrainer_Text_AskFormat:
	.string "Lotta secca o al meglio di tre?$"

VgcTrainer_Text_FormatBo1:
	.string "Lotta singola (Bo1)$"

VgcTrainer_Text_FormatBo3:
	.string "Al meglio di 3 (Bo3)$"

@ --- SIMONE (Guts Machamp, Arcanine, Mega Aggron, Toedscruel Trick Room) -----
VgcTrainer_Text_Simone_AskBattle:
	.string "Sono SIMONE. Il mio MACHAMP non ha\n"
	.string "paura di nessuno. Lottiamo?$"
VgcTrainer_Text_Simone_Intro:
	.string "Forza, vediamo cosa sai fare!$"
VgcTrainer_Text_Simone_Victory:
	.string "Troppa forza bruta per te.$"
VgcTrainer_Text_Simone_Defeat:
	.string "Mi hai preso in contropiede...$"
VgcTrainer_Text_Simone_PostBattle:
	.string "Bella lotta. Torna quando vuoi\n"
	.string "la rivincita.$"
VgcTrainer_Text_Simone_Decline:
	.string "Quando sei pronto, sai dove sono.$"

@ --- ELIA (Trick Room: Mimikyu, Mega Banette, Goodra-Hisui, Lickitung) ------
VgcTrainer_Text_Elia_AskBattle:
	.string "Ciao, sono ELIA. Il tempo scorre\n"
	.string "al contrario, qui. Lottiamo?$"
VgcTrainer_Text_Elia_Intro:
	.string "Distorsione... e si parte!$"
VgcTrainer_Text_Elia_Victory:
	.string "Lento ma inesorabile.$"
VgcTrainer_Text_Elia_Defeat:
	.string "Troppo veloce per la mia Distorsione.$"
VgcTrainer_Text_Elia_PostBattle:
	.string "ELIA il LICKITUNG ti saluta.\n"
	.string "Ripassa pure.$"
VgcTrainer_Text_Elia_Decline:
	.string "Con calma, c'è tempo.$"

@ --- PATATINA (Ceruledge, Armarouge, Indeedee, Mega Altaria, Rotom-Wash) ----
VgcTrainer_Text_Patatina_AskBattle:
	.string "Mi chiamano LA PATATINA.\n"
	.string "Facciamo una lotta?$"
VgcTrainer_Text_Patatina_Intro:
	.string "Il mio CERULEDGE brucia tutto!$"
VgcTrainer_Text_Patatina_Victory:
	.string "Croccante fuori, letale dentro.$"
VgcTrainer_Text_Patatina_Defeat:
	.string "Uff... mi hai sbucciata.$"
VgcTrainer_Text_Patatina_PostBattle:
	.string "Alla prossima!$"
VgcTrainer_Text_Patatina_Decline:
	.string "Peccato. Magari più tardi.$"

@ --- TONY (Sole: Charizard Y, Roaring Moon, Slither Wing, Mega Gyarados) ----
VgcTrainer_Text_Tony_AskBattle:
	.string "TONY. Sotto il sole il mio team\n"
	.string "è imbattibile. Ci provi?$"
VgcTrainer_Text_Tony_Intro:
	.string "Che splenda il sole!$"
VgcTrainer_Text_Tony_Victory:
	.string "Bruciato.$"
VgcTrainer_Text_Tony_Defeat:
	.string "Si è annuvolato...$"
VgcTrainer_Text_Tony_PostBattle:
	.string "Torna quando c'è il sole.$"
VgcTrainer_Text_Tony_Decline:
	.string "Come vuoi.$"

@ --- MARIO LETT (Dondozo + Tatsugiri, Sinistcha, Chatot, Slaking) -----------
VgcTrainer_Text_MarioLett_AskBattle:
	.string "Sono MARIO LETT. Il mio DONDOZO ha\n"
	.string "fame. Lottiamo?$"
VgcTrainer_Text_MarioLett_Intro:
	.string "GIONTA, a tavola!$"
VgcTrainer_Text_MarioLett_Victory:
	.string "Mangiato in un boccone.$"
VgcTrainer_Text_MarioLett_Defeat:
	.string "Indigesto...$"
VgcTrainer_Text_MarioLett_PostBattle:
	.string "Il tè del mio SINISTCHA è ancora\n"
	.string "caldo, se vuoi riprovare.$"
VgcTrainer_Text_MarioLett_Decline:
	.string "Va bene, un'altra volta.$"

@ --- TATORE (Mega Sceptile, Lapras, Dusclops Trick Room, Kecleon, Armaldo) --
VgcTrainer_Text_Tatore_AskBattle:
	.string "TATORE! Ti va una lotta\n"
	.string "come si deve?$"
VgcTrainer_Text_Tatore_Intro:
	.string "Avanti, KECLEON!$"
VgcTrainer_Text_Tatore_Victory:
	.string "Visto? Come si deve.$"
VgcTrainer_Text_Tatore_Defeat:
	.string "Mannaggia...$"
VgcTrainer_Text_Tatore_PostBattle:
	.string "Buona lotta. Alla prossima.$"
VgcTrainer_Text_Tatore_Decline:
	.string "Eh, peccato.$"

@ --- MARIO (Hariyama, Iron Hands, Oranguru + 3 Pokémon senza set) -----------
VgcTrainer_Text_Mario_AskBattle:
	.string "Sono MARIO. I miei Pokémon sono\n"
	.string "un po' improvvisati. Lottiamo?$"
VgcTrainer_Text_Mario_Intro:
	.string "Andiamo!$"
VgcTrainer_Text_Mario_Victory:
	.string "L'improvvisazione paga!$"
VgcTrainer_Text_Mario_Defeat:
	.string "Lo sapevo, dovevo allenarli.$"
VgcTrainer_Text_Mario_PostBattle:
	.string "La prossima volta sarò pronto.$"
VgcTrainer_Text_Mario_Decline:
	.string "Ok, ok.$"

@ --- GEROZZ (Sabbia: Mega Tyranitar, Dugtrio-A, Gholdengo, Porygon2 TR) -----
VgcTrainer_Text_Gerozz_AskBattle:
	.string "GEROZZ. Tempesta di sabbia in\n"
	.string "arrivo. Sei pronto?$"
VgcTrainer_Text_Gerozz_Intro:
	.string "TRECASE, scava!$"
VgcTrainer_Text_Gerozz_Victory:
	.string "Sepolto nella sabbia.$"
VgcTrainer_Text_Gerozz_Defeat:
	.string "La sabbia non è bastata.$"
VgcTrainer_Text_Gerozz_PostBattle:
	.string "Ripassa, la sabbia è sempre qui.$"
VgcTrainer_Text_Gerozz_Decline:
	.string "Fifone.$"

@ --- CARLETTO (Pioggia: Pelipper, Ludicolo, Kingambit, Snorlax, Malamar) ----
VgcTrainer_Text_Carletto_AskBattle:
	.string "CARLETTO, marinaio. Senti odore di\n"
	.string "pioggia? Lottiamo?$"
VgcTrainer_Text_Carletto_Intro:
	.string "IL BOSS decide tutto!$"
VgcTrainer_Text_Carletto_Victory:
	.string "Affondato.$"
VgcTrainer_Text_Carletto_Defeat:
	.string "IL BOSS non sarà contento...$"
VgcTrainer_Text_Carletto_PostBattle:
	.string "Torna quando piove.$"
VgcTrainer_Text_Carletto_Decline:
	.string "Mare calmo, allora.$"

@ --- GIGI PINTO (Sole: Torkoal, Venusaur, Lilligant, Garganacl, Hatterene) --
VgcTrainer_Text_GigiPinto_AskBattle:
	.string "GIGI PINTO. Tutto shiny, tutto\n"
	.string "al sole. Una lotta?$"
VgcTrainer_Text_GigiPinto_Intro:
	.string "TORKOAL, accendi il sole!$"
VgcTrainer_Text_GigiPinto_Victory:
	.string "Luccicante e vincente.$"
VgcTrainer_Text_GigiPinto_Defeat:
	.string "Si sono solo abbronzati.$"
VgcTrainer_Text_GigiPinto_PostBattle:
	.string "TREMITI ti aspetta per la rivincita.$"
VgcTrainer_Text_GigiPinto_Decline:
	.string "Quando vuoi.$"
@ --- SAVAROTTO (Staffetta: Espathra + Chandelure, Sneasler, Incineroar, Whimsicott + Gallade) --
VgcTrainer_Text_Savarotto_AskBattle:
	.string "Sono SAVAROTTO! Ti va una sfida?\n"
	.string "La mia staffetta è inarrestabile!$"
VgcTrainer_Text_Savarotto_Intro:
	.string "Staffetta e fuoco puro!\n"
	.string "Vediamo se riesci a starmi dietro!$"
VgcTrainer_Text_Savarotto_Victory:
	.string "Troppo veloci e troppo forti!$"
VgcTrainer_Text_Savarotto_Defeat:
	.string "La staffetta è inciampata...$"
VgcTrainer_Text_Savarotto_PostBattle:
	.string "Bella lotta! Quando vuoi riprovare,\n"
	.string "sono sempre qui nel chill.$"
VgcTrainer_Text_Savarotto_Decline:
	.string "Niente da fare? Fa niente, alla prossima.$"

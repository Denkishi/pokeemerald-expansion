gambmart_MapScripts::
	.byte 0

// ===========================================================================
// CLERK: Mega Stones, Z-Crystals & Form-Changing Items
// ===========================================================================
GambMart_EventScript_Clerk::
	lock
	faceplayer
	message GambMart_Text_ClerkWelcome
	waitmessage
GambMart_EventScript_ClerkMenu:
	dynmultipush GambMart_Text_MegaStonesClassic, 0
	dynmultipush GambMart_Text_MegaStonesLegendsZA, 1
	dynmultipush GambMart_Text_ZCrystals, 2
	dynmultipush GambMart_Text_FormChanging, 3
	dynmultipush GambMart_Text_Cancel, 4
	dynmultistack 0, 0, FALSE, 5, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, GambMart_EventScript_MegaStonesClassic
	case 1, GambMart_EventScript_MegaStonesLegendsZA
	case 2, GambMart_EventScript_ZCrystals
	case 3, GambMart_EventScript_FormChanging
	case 4, GambMart_EventScript_ClerkExit
	case MULTI_B_PRESSED, GambMart_EventScript_ClerkExit
	end

GambMart_EventScript_MegaStonesClassic::
	pokemart GambMart_Pokemart_MegaStonesClassic
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_MegaStonesLegendsZA::
	pokemart GambMart_Pokemart_MegaStonesLegendsZA
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_ZCrystals::
	pokemart GambMart_Pokemart_ZCrystals
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_FormChanging::
	pokemart GambMart_Pokemart_FormChanging
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_ClerkExit::
	msgbox gText_PleaseComeAgain, MSGBOX_DEFAULT
	release
	end

	.align 2
GambMart_Pokemart_MegaStonesClassic:
	.2byte ITEM_VENUSAURITE
	.2byte ITEM_CHARIZARDITE_X
	.2byte ITEM_CHARIZARDITE_Y
	.2byte ITEM_BLASTOISINITE
	.2byte ITEM_BEEDRILLITE
	.2byte ITEM_PIDGEOTITE
	.2byte ITEM_ALAKAZITE
	.2byte ITEM_SLOWBRONITE
	.2byte ITEM_GENGARITE
	.2byte ITEM_KANGASKHANITE
	.2byte ITEM_PINSIRITE
	.2byte ITEM_GYARADOSITE
	.2byte ITEM_AERODACTYLITE
	.2byte ITEM_MEWTWONITE_X
	.2byte ITEM_MEWTWONITE_Y
	.2byte ITEM_AMPHAROSITE
	.2byte ITEM_STEELIXITE
	.2byte ITEM_SCIZORITE
	.2byte ITEM_HERACRONITE
	.2byte ITEM_HOUNDOOMINITE
	.2byte ITEM_TYRANITARITE
	.2byte ITEM_SCEPTILITE
	.2byte ITEM_BLAZIKENITE
	.2byte ITEM_SWAMPERTITE
	.2byte ITEM_GARDEVOIRITE
	.2byte ITEM_SABLENITE
	.2byte ITEM_MAWILITE
	.2byte ITEM_AGGRONITE
	.2byte ITEM_MEDICHAMITE
	.2byte ITEM_MANECTITE
	.2byte ITEM_SHARPEDONITE
	.2byte ITEM_CAMERUPTITE
	.2byte ITEM_ALTARIANITE
	.2byte ITEM_BANETTITE
	.2byte ITEM_ABSOLITE
	.2byte ITEM_GLALITITE
	.2byte ITEM_SALAMENCITE
	.2byte ITEM_METAGROSSITE
	.2byte ITEM_LATIASITE
	.2byte ITEM_LATIOSITE
	.2byte ITEM_LOPUNNITE
	.2byte ITEM_GARCHOMPITE
	.2byte ITEM_LUCARIONITE
	.2byte ITEM_ABOMASITE
	.2byte ITEM_GALLADITE
	.2byte ITEM_AUDINITE
	.2byte ITEM_DIANCITE
	pokemartlistend

	.align 2
GambMart_Pokemart_MegaStonesLegendsZA:
	.2byte ITEM_CLEFABLITE
	.2byte ITEM_VICTREEBELITE
	.2byte ITEM_STARMINITE
	.2byte ITEM_DRAGONINITE
	.2byte ITEM_MEGANIUMITE
	.2byte ITEM_FERALIGITE
	.2byte ITEM_SKARMORITE
	.2byte ITEM_FROSLASSITE
	.2byte ITEM_EMBOARITE
	.2byte ITEM_EXCADRITE
	.2byte ITEM_SCOLIPITE
	.2byte ITEM_SCRAFTINITE
	.2byte ITEM_EELEKTROSSITE
	.2byte ITEM_CHANDELURITE
	.2byte ITEM_CHESNAUGHTITE
	.2byte ITEM_DELPHOXITE
	.2byte ITEM_GRENINJITE
	.2byte ITEM_PYROARITE
	.2byte ITEM_FLOETTITE
	.2byte ITEM_MALAMARITE
	.2byte ITEM_BARBARACITE
	.2byte ITEM_DRAGALGITE
	.2byte ITEM_HAWLUCHANITE
	.2byte ITEM_ZYGARDITE
	.2byte ITEM_DRAMPANITE
	.2byte ITEM_FALINKSITE
	.2byte ITEM_HEATRANITE
	.2byte ITEM_DARKRANITE
	.2byte ITEM_ZERAORITE
	.2byte ITEM_RAICHUNITE_X
	.2byte ITEM_RAICHUNITE_Y
	.2byte ITEM_CHIMECHITE
	.2byte ITEM_ABSOLITE_Z
	.2byte ITEM_STARAPTITE
	.2byte ITEM_GARCHOMPITE_Z
	.2byte ITEM_LUCARIONITE_Z
	.2byte ITEM_GOLURKITE
	.2byte ITEM_MEOWSTICITE
	.2byte ITEM_CRABOMINITE
	.2byte ITEM_GOLISOPITE
	.2byte ITEM_MAGEARNITE
	.2byte ITEM_SCOVILLAINITE
	.2byte ITEM_BAXCALIBRITE
	.2byte ITEM_TATSUGIRINITE
	.2byte ITEM_GLIMMORANITE
	pokemartlistend

	.align 2
GambMart_Pokemart_ZCrystals:
	.2byte ITEM_NORMALIUM_Z
	.2byte ITEM_FIRIUM_Z
	.2byte ITEM_WATERIUM_Z
	.2byte ITEM_ELECTRIUM_Z
	.2byte ITEM_GRASSIUM_Z
	.2byte ITEM_ICIUM_Z
	.2byte ITEM_FIGHTINIUM_Z
	.2byte ITEM_POISONIUM_Z
	.2byte ITEM_GROUNDIUM_Z
	.2byte ITEM_FLYINIUM_Z
	.2byte ITEM_PSYCHIUM_Z
	.2byte ITEM_BUGINIUM_Z
	.2byte ITEM_ROCKIUM_Z
	.2byte ITEM_GHOSTIUM_Z
	.2byte ITEM_DRAGONIUM_Z
	.2byte ITEM_DARKINIUM_Z
	.2byte ITEM_STEELIUM_Z
	.2byte ITEM_FAIRIUM_Z
	.2byte ITEM_PIKANIUM_Z
	.2byte ITEM_EEVIUM_Z
	.2byte ITEM_SNORLIUM_Z
	.2byte ITEM_MEWNIUM_Z
	.2byte ITEM_DECIDIUM_Z
	.2byte ITEM_INCINIUM_Z
	.2byte ITEM_PRIMARIUM_Z
	.2byte ITEM_LYCANIUM_Z
	.2byte ITEM_MIMIKIUM_Z
	.2byte ITEM_KOMMONIUM_Z
	.2byte ITEM_TAPUNIUM_Z
	.2byte ITEM_SOLGANIUM_Z
	.2byte ITEM_LUNALIUM_Z
	.2byte ITEM_MARSHADIUM_Z
	.2byte ITEM_ALORAICHIUM_Z
	.2byte ITEM_PIKASHUNIUM_Z
	.2byte ITEM_ULTRANECROZIUM_Z
	pokemartlistend

	.align 2
GambMart_Pokemart_FormChanging:
	.2byte ITEM_ROTOM_CATALOG
	.2byte ITEM_GRACIDEA
	.2byte ITEM_REVEAL_GLASS
	.2byte ITEM_DNA_SPLICERS
	.2byte ITEM_ZYGARDE_CUBE
	.2byte ITEM_PRISON_BOTTLE
	.2byte ITEM_N_SOLARIZER
	.2byte ITEM_N_LUNARIZER
	.2byte ITEM_REINS_OF_UNITY
	.2byte ITEM_RED_ORB
	.2byte ITEM_BLUE_ORB
	.2byte ITEM_ADAMANT_CRYSTAL
	.2byte ITEM_GRISEOUS_CORE
	.2byte ITEM_LUSTROUS_GLOBE
	.2byte ITEM_CORNERSTONE_MASK
	.2byte ITEM_WELLSPRING_MASK
	.2byte ITEM_HEARTHFLAME_MASK
	.2byte ITEM_SCROLL_OF_DARKNESS
	.2byte ITEM_SCROLL_OF_WATERS
	.2byte ITEM_AUSPICIOUS_ARMOR
	.2byte ITEM_MALICIOUS_ARMOR
	.2byte ITEM_LEADERS_CREST
	.2byte ITEM_SYRUPY_APPLE
	.2byte ITEM_UNREMARKABLE_TEACUP
	.2byte ITEM_MASTERPIECE_TEACUP
	.2byte ITEM_METAL_ALLOY
	pokemartlistend

// ===========================================================================
// WOMAN: Competitive Held Items, Gems, Type-Boosters, Rocks & Seeds
// ===========================================================================
GambMart_EventScript_Woman::
	lock
	faceplayer
	msgbox GambMart_Text_WomanWelcome, MSGBOX_DEFAULT
GambMart_EventScript_WomanMenu:
	dynmultipush GambMart_Text_CompetitiveHeld, 0
	dynmultipush GambMart_Text_Gems, 1
	dynmultipush GambMart_Text_TypeBoosting, 2
	dynmultipush GambMart_Text_WeatherTerrain, 3
	dynmultipush GambMart_Text_Cancel, 4
	dynmultistack 0, 0, FALSE, 5, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, GambMart_EventScript_CompetitiveHeld
	case 1, GambMart_EventScript_Gems
	case 2, GambMart_EventScript_TypeBoosting
	case 3, GambMart_EventScript_WeatherTerrain
	case 4, GambMart_EventScript_WomanExit
	case MULTI_B_PRESSED, GambMart_EventScript_WomanExit
	end

GambMart_EventScript_CompetitiveHeld::
	pokemart GambMart_Pokemart_CompetitiveHeld
	msgbox GambMart_Text_WomanComeBack, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_Gems::
	pokemart GambMart_Pokemart_Gems
	msgbox GambMart_Text_WomanComeBack, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_TypeBoosting::
	pokemart GambMart_Pokemart_TypeBoosting
	msgbox GambMart_Text_WomanComeBack, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_WeatherTerrain::
	pokemart GambMart_Pokemart_WeatherTerrain
	msgbox GambMart_Text_WomanComeBack, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_WomanExit::
	msgbox GambMart_Text_WomanComeBack, MSGBOX_DEFAULT
	release
	end

	.align 2
GambMart_Pokemart_CompetitiveHeld:
	.2byte ITEM_CHOICE_BAND
	.2byte ITEM_CHOICE_SPECS
	.2byte ITEM_CHOICE_SCARF
	.2byte ITEM_LIFE_ORB
	.2byte ITEM_FOCUS_SASH
	.2byte ITEM_ASSAULT_VEST
	.2byte ITEM_HEAVY_DUTY_BOOTS
	.2byte ITEM_LEFTOVERS
	.2byte ITEM_ROCKY_HELMET
	.2byte ITEM_EVIOLITE
	.2byte ITEM_BOOSTER_ENERGY
	.2byte ITEM_LOADED_DICE
	.2byte ITEM_CLEAR_AMULET
	.2byte ITEM_PUNCHING_GLOVE
	.2byte ITEM_COVERT_CLOAK
	.2byte ITEM_ABILITY_SHIELD
	.2byte ITEM_MIRROR_HERB
	.2byte ITEM_AIR_BALLOON
	.2byte ITEM_RED_CARD
	.2byte ITEM_EJECT_BUTTON
	.2byte ITEM_EJECT_PACK
	.2byte ITEM_BLUNDER_POLICY
	.2byte ITEM_WEAKNESS_POLICY
	.2byte ITEM_ROOM_SERVICE
	.2byte ITEM_SHED_SHELL
	.2byte ITEM_EXPERT_BELT
	.2byte ITEM_LIGHT_CLAY
	.2byte ITEM_FOCUS_BAND
	.2byte ITEM_SCOPE_LENS
	.2byte ITEM_WIDE_LENS
	.2byte ITEM_MUSCLE_BAND
	.2byte ITEM_WISE_GLASSES
	.2byte ITEM_BLACK_SLUDGE
	.2byte ITEM_FLAME_ORB
	.2byte ITEM_TOXIC_ORB
	pokemartlistend

	.align 2
GambMart_Pokemart_Gems:
	.2byte ITEM_NORMAL_GEM
	.2byte ITEM_FIRE_GEM
	.2byte ITEM_WATER_GEM
	.2byte ITEM_ELECTRIC_GEM
	.2byte ITEM_GRASS_GEM
	.2byte ITEM_ICE_GEM
	.2byte ITEM_FIGHTING_GEM
	.2byte ITEM_POISON_GEM
	.2byte ITEM_GROUND_GEM
	.2byte ITEM_FLYING_GEM
	.2byte ITEM_PSYCHIC_GEM
	.2byte ITEM_BUG_GEM
	.2byte ITEM_ROCK_GEM
	.2byte ITEM_GHOST_GEM
	.2byte ITEM_DRAGON_GEM
	.2byte ITEM_DARK_GEM
	.2byte ITEM_STEEL_GEM
	.2byte ITEM_FAIRY_GEM
	pokemartlistend

	.align 2
GambMart_Pokemart_TypeBoosting:
	.2byte ITEM_SILK_SCARF
	.2byte ITEM_CHARCOAL
	.2byte ITEM_MYSTIC_WATER
	.2byte ITEM_MAGNET
	.2byte ITEM_MIRACLE_SEED
	.2byte ITEM_NEVER_MELT_ICE
	.2byte ITEM_BLACK_BELT
	.2byte ITEM_POISON_BARB
	.2byte ITEM_SOFT_SAND
	.2byte ITEM_SHARP_BEAK
	.2byte ITEM_TWISTED_SPOON
	.2byte ITEM_SILVER_POWDER
	.2byte ITEM_HARD_STONE
	.2byte ITEM_SPELL_TAG
	.2byte ITEM_DRAGON_FANG
	.2byte ITEM_BLACK_GLASSES
	.2byte ITEM_METAL_COAT
	.2byte ITEM_FAIRY_FEATHER
	pokemartlistend

	.align 2
GambMart_Pokemart_WeatherTerrain:
	.2byte ITEM_DAMP_ROCK
	.2byte ITEM_HEAT_ROCK
	.2byte ITEM_SMOOTH_ROCK
	.2byte ITEM_ICY_ROCK
	.2byte ITEM_ELECTRIC_SEED
	.2byte ITEM_PSYCHIC_SEED
	.2byte ITEM_MISTY_SEED
	.2byte ITEM_GRASSY_SEED
	.2byte ITEM_TERRAIN_EXTENDER
	.2byte ITEM_LIGHT_BALL
	.2byte ITEM_LEEK
	.2byte ITEM_THICK_CLUB
	.2byte ITEM_LUCKY_PUNCH
	.2byte ITEM_METAL_POWDER
	.2byte ITEM_QUICK_POWDER
	.2byte ITEM_DEEP_SEA_SCALE
	.2byte ITEM_DEEP_SEA_TOOTH
	.2byte ITEM_SOUL_DEW
	.2byte ITEM_ADAMANT_ORB
	.2byte ITEM_LUSTROUS_ORB
	.2byte ITEM_GRISEOUS_ORB
	pokemartlistend

// ===========================================================================
// BOY: Berries, Incenses & Battle Consumables
// ===========================================================================
GambMart_EventScript_Boy::
	lock
	faceplayer
	msgbox GambMart_Text_BoyWelcome, MSGBOX_DEFAULT
GambMart_EventScript_BoyMenu:
	dynmultipush GambMart_Text_BerriesHealing, 0
	dynmultipush GambMart_Text_BerriesStats, 1
	dynmultipush GambMart_Text_BerriesResist, 2
	dynmultipush GambMart_Text_IncensesConsumables, 3
	dynmultipush GambMart_Text_Cancel, 4
	dynmultistack 0, 0, FALSE, 5, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, GambMart_EventScript_BerriesHealing
	case 1, GambMart_EventScript_BerriesStats
	case 2, GambMart_EventScript_BerriesResist
	case 3, GambMart_EventScript_IncensesConsumables
	case 4, GambMart_EventScript_BoyExit
	case MULTI_B_PRESSED, GambMart_EventScript_BoyExit
	end

GambMart_EventScript_BerriesHealing::
	pokemart GambMart_Pokemart_BerriesHealing
	msgbox GambMart_Text_BoyComeBack, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_BerriesStats::
	pokemart GambMart_Pokemart_BerriesStats
	msgbox GambMart_Text_BoyComeBack, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_BerriesResist::
	pokemart GambMart_Pokemart_BerriesResist
	msgbox GambMart_Text_BoyComeBack, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_IncensesConsumables::
	pokemart GambMart_Pokemart_IncensesConsumables
	msgbox GambMart_Text_BoyComeBack, MSGBOX_DEFAULT
	release
	end

GambMart_EventScript_BoyExit::
	msgbox GambMart_Text_BoyComeBack, MSGBOX_DEFAULT
	release
	end

	.align 2
GambMart_Pokemart_BerriesHealing:
	.2byte ITEM_SITRUS_BERRY
	.2byte ITEM_LUM_BERRY
	.2byte ITEM_ORAN_BERRY
	.2byte ITEM_LEPPA_BERRY
	.2byte ITEM_CHESTO_BERRY
	.2byte ITEM_CHERI_BERRY
	.2byte ITEM_PECHA_BERRY
	.2byte ITEM_RAWST_BERRY
	.2byte ITEM_ASPEAR_BERRY
	.2byte ITEM_PERSIM_BERRY
	.2byte ITEM_FIGY_BERRY
	.2byte ITEM_WIKI_BERRY
	.2byte ITEM_MAGO_BERRY
	.2byte ITEM_AGUAV_BERRY
	.2byte ITEM_IAPAPA_BERRY
	.2byte ITEM_POMEG_BERRY
	.2byte ITEM_KELPSY_BERRY
	.2byte ITEM_QUALOT_BERRY
	.2byte ITEM_HONDEW_BERRY
	.2byte ITEM_GREPA_BERRY
	.2byte ITEM_TAMATO_BERRY
	pokemartlistend

	.align 2
GambMart_Pokemart_BerriesStats:
	.2byte ITEM_LIECHI_BERRY
	.2byte ITEM_GANLON_BERRY
	.2byte ITEM_SALAC_BERRY
	.2byte ITEM_PETAYA_BERRY
	.2byte ITEM_APICOT_BERRY
	.2byte ITEM_LANSAT_BERRY
	.2byte ITEM_STARF_BERRY
	.2byte ITEM_MICLE_BERRY
	.2byte ITEM_CUSTAP_BERRY
	.2byte ITEM_JABOCA_BERRY
	.2byte ITEM_ROWAP_BERRY
	.2byte ITEM_KEE_BERRY
	.2byte ITEM_MARANGA_BERRY
	pokemartlistend

	.align 2
GambMart_Pokemart_BerriesResist:
	.2byte ITEM_OCCA_BERRY
	.2byte ITEM_PASSHO_BERRY
	.2byte ITEM_WACAN_BERRY
	.2byte ITEM_RINDO_BERRY
	.2byte ITEM_YACHE_BERRY
	.2byte ITEM_CHOPLE_BERRY
	.2byte ITEM_KEBIA_BERRY
	.2byte ITEM_SHUCA_BERRY
	.2byte ITEM_COBA_BERRY
	.2byte ITEM_PAYAPA_BERRY
	.2byte ITEM_TANGA_BERRY
	.2byte ITEM_CHARTI_BERRY
	.2byte ITEM_KASIB_BERRY
	.2byte ITEM_HABAN_BERRY
	.2byte ITEM_COLBUR_BERRY
	.2byte ITEM_BABIRI_BERRY
	.2byte ITEM_CHILAN_BERRY
	.2byte ITEM_ROSELI_BERRY
	pokemartlistend

	.align 2
GambMart_Pokemart_IncensesConsumables:
	.2byte ITEM_SEA_INCENSE
	.2byte ITEM_LAX_INCENSE
	.2byte ITEM_ODD_INCENSE
	.2byte ITEM_ROCK_INCENSE
	.2byte ITEM_FULL_INCENSE
	.2byte ITEM_WAVE_INCENSE
	.2byte ITEM_ROSE_INCENSE
	.2byte ITEM_LUCK_INCENSE
	.2byte ITEM_PURE_INCENSE
	.2byte ITEM_ABSORB_BULB
	.2byte ITEM_CELL_BATTERY
	.2byte ITEM_LUMINOUS_MOSS
	.2byte ITEM_SNOWBALL
	.2byte ITEM_ADRENALINE_ORB
	.2byte ITEM_PROTECTIVE_PADS
	.2byte ITEM_THROAT_SPRAY
	.2byte ITEM_UTILITY_UMBRELLA
	.2byte ITEM_WHITE_HERB
	.2byte ITEM_MENTAL_HERB
	.2byte ITEM_POWER_HERB
	pokemartlistend

// ===========================================================================
// Text strings
// ===========================================================================
GambMart_Text_ClerkWelcome:
	.string "Benvenuto al Banco Speciale!\n"
	.string "Tutto è gratuito. Cosa desideri?$"

GambMart_Text_MegaStonesClassic:
	.string "Megapietre Classiche$"

GambMart_Text_MegaStonesLegendsZA:
	.string "Megapietre Legends Z-A$"

GambMart_Text_ZCrystals:
	.string "Cristalli Z$"

GambMart_Text_FormChanging:
	.string "Strumenti Forma & Legg.$"

GambMart_Text_WomanWelcome:
	.string "Ciao! Ho tutti i migliori strumenti da\n"
	.string "battaglia a costo zero. Cosa cerchi?$"

GambMart_Text_CompetitiveHeld:
	.string "Strumenti Competitivi$"

GambMart_Text_Gems:
	.string "Gemme Elementali$"

GambMart_Text_TypeBoosting:
	.string "Potenziatori Tipo$"

GambMart_Text_WeatherTerrain:
	.string "Meteo, Terreni & Spec.$"

GambMart_Text_WomanComeBack:
	.string "Torna quando vuoi a fare rifornimento!$"

GambMart_Text_BoyWelcome:
	.string "Ehi! Io ho tutte le bacche e i consumabili\n"
	.string "per le tue strategie! Prendi pure!$"

GambMart_Text_BerriesHealing:
	.string "Bacche di Cura & Punti$"

GambMart_Text_BerriesStats:
	.string "Bacche Statistiche$"

GambMart_Text_BerriesResist:
	.string "Bacche Anti-Tipo (Resist)$"

GambMart_Text_IncensesConsumables:
	.string "Incensi & Consumabili$"

GambMart_Text_BoyComeBack:
	.string "Buona fortuna con le tue lotte!$"

GambMart_Text_Cancel:
	.string "Annulla$"

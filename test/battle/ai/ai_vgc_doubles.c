#include "global.h"
#include "test/battle.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc.h"
#include "battle_ai_vgc_graph.h"

// Suite dell'IA per i doppi VGC (AI_FLAG_VGC_DOUBLES): una posizione = una risposta attesa.
// Piano e lista delle posizioni: docs/ia_vgc/piano_implementazione.md (sezione 4).
// Ogni errore visto in partita va aggiunto qui come test. I test marcati KNOWN_FAILING descrivono
// un comportamento sbagliato già noto: quando la correzione arriva, si toglie il KNOWN_FAILING.

#define VGC_AI_FLAGS (AI_FLAG_BASIC_TRAINER | AI_FLAG_SMART_SWITCHING | AI_FLAG_SMART_MON_CHOICES \
                    | AI_FLAG_KNOW_OPPONENT_PARTY | AI_FLAG_ASSUME_STAB | AI_FLAG_ASSUME_STATUS_MOVES \
                    | AI_FLAG_HP_AWARE | AI_FLAG_WEIGH_ABILITY_PREDICTION | AI_FLAG_VGC_DOUBLES)

// Nel motore un attacco sul bersaglio già andato KO passa da solo sull'altro avversario: l'overkill costa
// quando la mossa scelta per il primo bersaglio è inutile sul secondo (qui Thunderbolt su Garchomp, immune).
AI_DOUBLE_BATTLE_TEST("VGC: no overkill - the second attacker picks the move that works on the other foe")
{
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { HP(1); Speed(10); }
        PLAYER(SPECIES_GARCHOMP) { Speed(10); }
        // Il più veloce ha solo Thunderbolt: l'unico bersaglio utile è il Wobbuffet a 1 PS.
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_THUNDERBOLT); Speed(50); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_THUNDERBOLT, MOVE_ICE_BEAM); Speed(40); }
    } WHEN {
        TURN {
            EXPECT_MOVE(opponentLeft, MOVE_THUNDERBOLT, target: playerLeft);
            EXPECT_MOVE(opponentRight, MOVE_ICE_BEAM, target: playerRight);
        }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Fake Out is not used after the first turn on the field")
{
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_FAKE_OUT, MOVE_SCRATCH); Speed(50); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH); Speed(40); }
    } WHEN {
        TURN { }
        TURN { NOT_EXPECT_MOVE(opponentLeft, MOVE_FAKE_OUT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Simple Beam on a Mycelium Might partner lets Spore go first")
{
    // Avversari più lenti di Toedscruel ma che attaccano forte: addormentarne uno PRIMA che colpisca vale
    // l'azione del compagno. Nessuna regola sulla specie: lo vede la simulazione.
    // La velocità del giocatore l'IA la stima dalla specie (quella vera non si vede): serve una specie
    // davvero lenta (Ursaluna, base 50), non un Garchomp con Speed(60) che per l'IA resta un Garchomp veloce.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_URSALUNA) { Speed(60); }
        PLAYER(SPECIES_URSALUNA) { Speed(60); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SIMPLE_BEAM, MOVE_SCRATCH); Speed(150); }
        OPPONENT(SPECIES_TOEDSCRUEL) { Ability(ABILITY_MYCELIUM_MIGHT); Moves(MOVE_SPORE, MOVE_EARTH_POWER); Speed(100); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_SIMPLE_BEAM, target: opponentRight); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Beat Up on a Justified partner before it attacks")
{
    // Beat Up colpisce Gallade una volta per Pokémon della squadra (qui 2): Justified lo porta a +2 e il suo
    // Close Combat nello stesso turno manda KO. Nessuna regola sulle specie: lo vede la simulazione.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(20); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(20); }
        OPPONENT(SPECIES_WHIMSICOTT) { Moves(MOVE_BEAT_UP, MOVE_MOONBLAST); Speed(116); }
        OPPONENT(SPECIES_GALLADE) { Ability(ABILITY_JUSTIFIED); Moves(MOVE_CLOSE_COMBAT); Speed(80); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_BEAT_UP, target: opponentRight); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Baton Pass sends in the designated sweeper, not the generic best switch-in")
{
    // Il grafo della squadra lega il Baton Passer (Speed Boost, niente attacchi) al miglior attaccante speciale
    // in panchina: è lui che deve entrare a raccogliere i boost.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(20); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(20); }
        OPPONENT(SPECIES_ESPATHRA) { Ability(ABILITY_SPEED_BOOST); Moves(MOVE_BATON_PASS); Speed(105); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH); Speed(30); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH); Speed(30); }
        OPPONENT(SPECIES_CHANDELURE) { Moves(MOVE_SHADOW_BALL, MOVE_HEAT_WAVE, MOVE_ENERGY_BALL); Speed(80); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_BATON_PASS); EXPECT_SEND_OUT(opponentLeft, 3); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Fake Out on turn 1 covers a Baton Passer partner")
{
    // Ruoli dal grafo (nessun nome): il compagno di un Baton Passer usa Fake Out al turno 1.
    // Cosa fa Espathra lo decide la simulazione: contro avversari innocui, o contro cui Chandelure non ha bisogno
    // di boost, far entrare subito l'attaccante può essere giusto (un turno d'attacco in più). Il valore del setup
    // su più turni è un punto aperto (piano, M3).
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_GARCHOMP) { Speed(102); }
        PLAYER(SPECIES_GARCHOMP) { Speed(102); }
        OPPONENT(SPECIES_INCINEROAR) { Moves(MOVE_FAKE_OUT, MOVE_FLARE_BLITZ); Speed(60); }
        OPPONENT(SPECIES_ESPATHRA) { Ability(ABILITY_SPEED_BOOST); Moves(MOVE_CALM_MIND, MOVE_BATON_PASS, MOVE_PROTECT); Speed(105); }
        OPPONENT(SPECIES_CHANDELURE) { Moves(MOVE_SHADOW_BALL, MOVE_HEAT_WAVE, MOVE_ENERGY_BALL); Speed(80); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_FAKE_OUT); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: after a KO the replacement that pairs best with the partner comes in")
{
    // Whimsicott è in campo: dopo il KO del compagno entra Gallade (Beat Up -> Justified), non il Pokémon
    // che il motore sceglierebbe per il solo matchup.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(200); Moves(MOVE_SCRATCH); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(200); Moves(MOVE_SCRATCH); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); Moves(MOVE_SCRATCH); Speed(10); }
        OPPONENT(SPECIES_WHIMSICOTT) { Moves(MOVE_BEAT_UP, MOVE_MOONBLAST); Speed(116); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH); Speed(10); }
        OPPONENT(SPECIES_GALLADE) { Ability(ABILITY_JUSTIFIED); Moves(MOVE_CLOSE_COMBAT); Speed(80); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_SCRATCH, target: opponentLeft); MOVE(playerRight, MOVE_SCRATCH, target: opponentRight);
               EXPECT_SEND_OUT(opponentLeft, 3); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: at equal damage the accurate move is preferred to close a KO")
{
    // Entrambe le mosse mandano KO il bersaglio a 5 PS: quella al 100% batte quella all'80% (il ramo
    // "mancato" di Stone Edge costa il 20% del KO). Prima il test falliva perché la lotta portava tutti a Lv50
    // curando i PS: il bersaglio aveva 250 PS, non 5 (ora nei test valgono PS e statistiche del test).
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { HP(5); Speed(10); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); }
        OPPONENT(SPECIES_GARCHOMP) { Moves(MOVE_STONE_EDGE, MOVE_SCRATCH); Speed(50); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); Speed(40); }
    } WHEN {
        TURN { NOT_EXPECT_MOVE(opponentLeft, MOVE_STONE_EDGE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: a 90% spread move that KOs both foes beats a sure single KO")
{
    // Rock Slide (90%, ad area) manda KO entrambi i bersagli a 5 PS: ~1,8 KO attesi contro 1 KO sicuro di Scratch.
    // Con la vecchia precisione "danno atteso" (5 PS x 90% = 4) non mandava KO nessuno.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { HP(5); Speed(10); }
        PLAYER(SPECIES_WOBBUFFET) { HP(5); Speed(10); }
        OPPONENT(SPECIES_GARCHOMP) { Moves(MOVE_ROCK_SLIDE, MOVE_SCRATCH); Speed(50); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); Speed(40); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_ROCK_SLIDE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Discharge is not used when it would KO the partner (real set: Simone's Rotom-Mow)")
{
    // Set veri della squadra di Simone (src/data/trainers.party). Discharge colpisce anche il compagno: qui
    // manderebbe KO Porygon-Z per un po' di danno in più sul Wobbuffet. Thunderbolt basta per Gyarados.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_GYARADOS) { HP(100); Speed(50); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(50); }
        OPPONENT(SPECIES_ROTOM_MOW) { Ability(ABILITY_LEVITATE); Moves(MOVE_LEAF_STORM, MOVE_THUNDERBOLT, MOVE_VOLT_SWITCH, MOVE_DISCHARGE); Speed(150); }
        OPPONENT(SPECIES_PORYGON_Z) { HP(20); Moves(MOVE_TRI_ATTACK, MOVE_ICE_BEAM); Speed(120); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_THUNDERBOLT, target: playerLeft); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Earthquake is fine next to a Levitate partner")
{
    // Il compagno con Levitate è immune: Earthquake manda KO entrambi gli avversari senza costi.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { HP(30); Speed(10); }
        PLAYER(SPECIES_WOBBUFFET) { HP(30); Speed(10); }
        OPPONENT(SPECIES_GARCHOMP) { Moves(MOVE_EARTHQUAKE, MOVE_DRAGON_CLAW); Speed(100); }
        OPPONENT(SPECIES_ROTOM_MOW) { Ability(ABILITY_LEVITATE); Moves(MOVE_LEAF_STORM, MOVE_THUNDERBOLT); Speed(90); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_EARTHQUAKE); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Trick Room setter sets the room for its slow partner")
{
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        // Avversari veloci per specie (l'IA stima la velocità del giocatore dalla specie, non legge la vera).
        PLAYER(SPECIES_GARCHOMP) { Speed(150); }
        PLAYER(SPECIES_GARCHOMP) { Speed(150); }
        OPPONENT(SPECIES_INDEEDEE_F) { Moves(MOVE_TRICK_ROOM, MOVE_FOLLOW_ME, MOVE_PSYCHIC); Speed(85); }
        OPPONENT(SPECIES_COPPERAJAH) { Moves(MOVE_IRON_HEAD, MOVE_HEAVY_SLAM); Speed(30); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_TRICK_ROOM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: no Simple Beam on Mycelium Might when Spore would fail anyway (Good as Gold)")
{
    // Senza Mycelium Might, Spore rimbalza su Good as Gold: togliere l'abilità al compagno è controproducente.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_GHOLDENGO) { Ability(ABILITY_GOOD_AS_GOLD); Speed(60); }
        PLAYER(SPECIES_GHOLDENGO) { Ability(ABILITY_GOOD_AS_GOLD); Speed(60); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SIMPLE_BEAM, MOVE_SHADOW_BALL); Speed(150); }
        OPPONENT(SPECIES_TOEDSCRUEL) { Ability(ABILITY_MYCELIUM_MIGHT); Moves(MOVE_SPORE, MOVE_EARTH_POWER); Speed(100); }
    } WHEN {
        TURN { NOT_EXPECT_MOVE(opponentLeft, MOVE_SIMPLE_BEAM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: real team (Savarotto) - Calm Mind now is worth more than Baton Pass now")
{
    // Set veri della squadra di Savarotto (src/data/trainers.party). Incineroar copre con Fake Out; Espathra
    // (Speed Boost) fa Calm Mind e passa i boost a Chandelure il turno dopo. Prima le regole filtravano le
    // candidate (solo le mosse vicine alla migliore delle regole) e il seguito "Baton Pass dopo il setup" non
    // entrava nella ricerca del turno 2: Baton Pass subito sembrava meglio. Si controlla l'ORDINE dei valori.
    // Passa da quando Speed Boost conta già a fine turno, Focus Punch dei Machamp non è più una minaccia
    // "probabile" e l'ordine di velocità vale per i turni rimasti.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        // Avversari forti ma che Espathra resiste (Lotta su Psico): il momento giusto per il setup.
        PLAYER(SPECIES_MACHAMP);
        PLAYER(SPECIES_MACHAMP);
        OPPONENT(SPECIES_INCINEROAR) { Ability(ABILITY_INTIMIDATE); Item(ITEM_SITRUS_BERRY); Moves(MOVE_FAKE_OUT, MOVE_FLARE_BLITZ, MOVE_PARTING_SHOT, MOVE_DARKEST_LARIAT); }
        OPPONENT(SPECIES_ESPATHRA) { Ability(ABILITY_SPEED_BOOST); Item(ITEM_COLBUR_BERRY); Moves(MOVE_BATON_PASS, MOVE_PROTECT, MOVE_CALM_MIND, MOVE_SUBSTITUTE); }
        OPPONENT(SPECIES_CHANDELURE) { Ability(ABILITY_FLASH_FIRE); Moves(MOVE_SHADOW_BALL, MOVE_PROTECT, MOVE_HEAT_WAVE, MOVE_ENERGY_BALL); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_FAKE_OUT); }
    } THEN {
        EXPECT_GT(AI_VgcPlanTestValue(GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT), MOVE_CALM_MIND), AI_VgcPlanTestValue(GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT), MOVE_BATON_PASS));
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Mega Charizard Y's sun makes Solar Beam hit on the turn it Mega Evolves")
{
    // La Mega avviene a inizio turno: Drought porta il sole prima delle mosse e Solar Beam non deve caricare.
    // Senza la Mega nel simulatore Solar Beam sembrava un turno perso e vinceva Flamethrower (inutile sui Swampert).
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_SWAMPERT) { Speed(60); }
        PLAYER(SPECIES_SWAMPERT) { Speed(60); }
        OPPONENT(SPECIES_CHARIZARD) { Item(ITEM_CHARIZARDITE_Y); Moves(MOVE_SOLAR_BEAM, MOVE_FLAMETHROWER); Speed(100); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); Speed(40); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_SOLAR_BEAM); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: no Fake Out into Psychic Terrain (Indeedee's Psychic Surge)")
{
    // Sul Campo Psichico le mosse con priorità sugli avversari a terra falliscono: Fake Out è un turno perso.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_INDEEDEE_F) { Ability(ABILITY_PSYCHIC_SURGE); Speed(85); }
        PLAYER(SPECIES_URSALUNA) { Speed(50); }
        OPPONENT(SPECIES_INCINEROAR) { Moves(MOVE_FAKE_OUT, MOVE_FLARE_BLITZ, MOVE_DARKEST_LARIAT); Speed(60); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); Speed(40); }
    } WHEN {
        TURN { NOT_EXPECT_MOVE(opponentLeft, MOVE_FAKE_OUT); }
    }
}

// Filtri delle candidate del pianificatore (ex regole): ognuno con il suo test.
AI_DOUBLE_BATTLE_TEST("VGC filter: no Tailwind while our Tailwind is still up")
{
    // Il compagno ha solo Tailwind: al turno 2 il vento soffia già, un secondo Tailwind fallisce.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); }
        PLAYER(SPECIES_WOBBUFFET) { Speed(10); }
        OPPONENT(SPECIES_PIDGEOT) { Moves(MOVE_TAILWIND, MOVE_SCRATCH); Speed(50); }
        OPPONENT(SPECIES_WHIMSICOTT) { Moves(MOVE_TAILWIND); Speed(60); }
    } WHEN {
        TURN { }
        TURN { NOT_EXPECT_MOVE(opponentLeft, MOVE_TAILWIND); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC filter: no attack into a known type immunity")
{
    // Due Ghost: Scratch non li tocca; resta Ember.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_GASTLY) { Speed(10); }
        PLAYER(SPECIES_GASTLY) { Speed(10); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SCRATCH, MOVE_EMBER); Speed(50); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_SPLASH); Speed(40); }
    } WHEN {
        TURN { EXPECT_MOVE(opponentLeft, MOVE_EMBER); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC: Spore goes on the bigger threat (Mega Tyranitar, not Dugtrio)")
{
    // Prova in gioco: Amoonguss addormentava Dugtrio accanto a un Tyranitar Mega. Il valore del sonno cresce con la
    // minaccia del bersaglio (danno atteso sul nostro lato, KO possibili, Mega, boost), non è uguale per tutti.
    // KNOWN_FAILING: con il valore del sonno scalato dalla minaccia (Dugtrio 52 -> Tyranitar Mega 63 nel primo
    // tentativo) Spore su Tyranitar (-463) resta sotto Spore su Dugtrio (-424), e sopra a tutto vince Sludge Bomb
    // + Flare Blitz su Dugtrio (-197, KO in due). La simulazione a 2-3 turni non vede abbastanza il danno evitato
    // dal sonno nei turni dopo: serve una stima del danno risparmiato per turno di sonno, non solo un termine fisso.
    KNOWN_FAILING;
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_DUGTRIO) { Ability(ABILITY_ARENA_TRAP); Moves(MOVE_EARTHQUAKE, MOVE_ROCK_SLIDE, MOVE_SUCKER_PUNCH, MOVE_PROTECT); }
        PLAYER(SPECIES_TYRANITAR_MEGA) { Ability(ABILITY_SAND_STREAM); Moves(MOVE_ROCK_SLIDE, MOVE_KNOCK_OFF, MOVE_LOW_KICK, MOVE_PROTECT); }
        OPPONENT(SPECIES_AMOONGUSS) { Ability(ABILITY_REGENERATOR); Moves(MOVE_SPORE, MOVE_RAGE_POWDER, MOVE_SLUDGE_BOMB, MOVE_PROTECT); }
        OPPONENT(SPECIES_INCINEROAR) { Ability(ABILITY_INTIMIDATE); Moves(MOVE_FLARE_BLITZ, MOVE_DARKEST_LARIAT); }
    } WHEN {
        TURN { MOVE(playerLeft, MOVE_PROTECT); MOVE(playerRight, MOVE_PROTECT); EXPECT_MOVE(opponentLeft, MOVE_SPORE, target: playerRight); }
    }
}

AI_DOUBLE_BATTLE_TEST("VGC filter: no Fake Out on the partner (real team Savarotto: Incineroar next to Flash Fire Chandelure)")
{
    // Prova in gioco: Incineroar usava Fake Out sul proprio Chandelure. Il grafo vede la combo Flare Blitz -> Flash
    // Fire, ma un attacco sul compagno passa solo se è la mossa che gli attiva l'abilità: Fake Out non lo è mai.
    GIVEN {
        AI_FLAGS(VGC_AI_FLAGS);
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); Speed(20); }
        PLAYER(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); Speed(20); }
        OPPONENT(SPECIES_INCINEROAR) { Ability(ABILITY_INTIMIDATE); Item(ITEM_SITRUS_BERRY); Moves(MOVE_FAKE_OUT, MOVE_FLARE_BLITZ, MOVE_PARTING_SHOT, MOVE_DARKEST_LARIAT); Speed(60); }
        OPPONENT(SPECIES_CHANDELURE) { Ability(ABILITY_FLASH_FIRE); Moves(MOVE_SHADOW_BALL, MOVE_PROTECT, MOVE_HEAT_WAVE, MOVE_ENERGY_BALL); Speed(80); }
    } WHEN {
        TURN { }
    } THEN {
        enum BattlerId incineroar = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        enum BattlerId chandelure = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
        EXPECT(!AI_VgcIsPartnerHitCombo(MOVE_FAKE_OUT, incineroar, chandelure));
        EXPECT(AI_VgcIsPartnerHitCombo(MOVE_FLARE_BLITZ, incineroar, chandelure));
        EXPECT(!(gChosenMoveByBattler[incineroar] == MOVE_FAKE_OUT && gBattleStruct->moveTarget[incineroar] == chandelure));
    }
}

// Piani di squadra degli allenatori VGC (usati da src/battle_ai_vgc.c e src/team_preview.c).
//
// Ogni piano dice all'IA: qual è la modalità principale della squadra (Trick Room, Tailwind,
// sole, pioggia, sabbia...), quale la riserva se la principale non conviene contro la squadra
// del giocatore, chi la imposta (setter), chi deve vincere la lotta (win condition: viene
// protetto, gli si lascia il setup, il partner lo copre con Segui Me / Aiuto), chi di solito
// resta in panchina (back) e le coppie di lead preferite, in ordine, con la modalità a cui
// appartengono (una coppia di una modalità "non conveniente" vale la metà).
//
// Le specie sono quelle base: in lotta si confronta con GET_BASE_SPECIES_ID (le Mega contano
// come la specie normale). Un piano non è un obbligo: è un bonus sopra la valutazione di
// matchup, quindi contro una squadra che lo punisce l'IA fa altro.

enum VgcPlanMode
{
    VGC_PLAN_BALANCE,       // nessuna modalità: si gioca il matchup
    VGC_PLAN_TRICK_ROOM,
    VGC_PLAN_TAILWIND,
    VGC_PLAN_SUN,
    VGC_PLAN_RAIN,
    VGC_PLAN_SAND,
    VGC_PLAN_PSYCHIC_TERRAIN,
};

struct VgcLeadPair
{
    u16 a;
    u16 b;
    u8 mode;        // VGC_PLAN_*
};

#define VGC_PLAN_LEADS 3

struct VgcGameplan
{
    u16 trainerId;
    u8 modeA;                   // modalità principale
    u8 modeB;                   // riserva
    u16 setters[2];             // chi imposta la modalità (SPECIES_NONE se non serve)
    u16 winCons[2];             // win condition
    u16 back[2];                // di norma in panchina (non lead)
    struct VgcLeadPair leads[VGC_PLAN_LEADS];
};

static const struct VgcGameplan sVgcGameplans[] =
{
    // SIMONE: forza bruta con Intimidate davanti; Trick Room di Toedscruel come riserva per
    // Machamp e Mega Aggron (lenti). Rotom-Mow scarf chiude.
    {
        .trainerId = TRAINER_SIMONE,
        .modeA = VGC_PLAN_BALANCE, .modeB = VGC_PLAN_TRICK_ROOM,
        .setters = { SPECIES_TOEDSCRUEL, SPECIES_NONE },
        .winCons = { SPECIES_MACHAMP, SPECIES_AGGRON },
        .back = { SPECIES_AGGRON, SPECIES_ROTOM_MOW },
        .leads = {
            { SPECIES_ARCANINE, SPECIES_MACHAMP, VGC_PLAN_BALANCE },
            { SPECIES_TOEDSCRUEL, SPECIES_MACHAMP, VGC_PLAN_TRICK_ROOM },
            { SPECIES_ARCANINE, SPECIES_PORYGON_Z, VGC_PLAN_BALANCE },
        },
    },
    // ELIA: Trick Room puro. Mimikyu (Disguise + Erbamentale) e Mega Banette lo impostano,
    // Goodra-Hisui e Lickitung (Ferrodifesa + Schiacciacorpo) vincono; Vaporeon e Miltank
    // (Aiuto, Dopo di te, Tuonoonda) sono supporto.
    {
        .trainerId = TRAINER_ELIA,
        .modeA = VGC_PLAN_TRICK_ROOM, .modeB = VGC_PLAN_BALANCE,
        .setters = { SPECIES_MIMIKYU, SPECIES_BANETTE },
        .winCons = { SPECIES_GOODRA_HISUI, SPECIES_LICKITUNG },
        .back = { SPECIES_LICKITUNG, SPECIES_MILTANK },
        .leads = {
            { SPECIES_MIMIKYU, SPECIES_GOODRA_HISUI, VGC_PLAN_TRICK_ROOM },
            { SPECIES_BANETTE, SPECIES_VAPOREON, VGC_PLAN_TRICK_ROOM },
            { SPECIES_MIMIKYU, SPECIES_MILTANK, VGC_PLAN_TRICK_ROOM },
        },
    },
    // PATATINA: Indeedee-F (Psychic Surge + Segui Me) con Armarouge Forzespansa; in riserva
    // Mega Altaria Tailwind con Ceruledge Danza di Spada; Copperajah Ferrodifesa sotto Trick Room.
    {
        .trainerId = TRAINER_PATATINA,
        .modeA = VGC_PLAN_PSYCHIC_TERRAIN, .modeB = VGC_PLAN_TAILWIND,
        .setters = { SPECIES_INDEEDEE_F, SPECIES_ALTARIA },
        .winCons = { SPECIES_ARMAROUGE, SPECIES_CERULEDGE },
        .back = { SPECIES_COPPERAJAH, SPECIES_ROTOM_WASH },
        .leads = {
            { SPECIES_INDEEDEE_F, SPECIES_ARMAROUGE, VGC_PLAN_PSYCHIC_TERRAIN },
            { SPECIES_ALTARIA, SPECIES_CERULEDGE, VGC_PLAN_TAILWIND },
            { SPECIES_INDEEDEE_F, SPECIES_COPPERAJAH, VGC_PLAN_TRICK_ROOM },
        },
    },
    // TONY: sole. Mega Charizard Y lo porta da solo, Sableye (Fake Out, Giornodisole, Provocazione)
    // lo copre; Roaring Moon e Slither Wing (Protosynthesis) chiudono, Mega Gyarados Dragondanza.
    {
        .trainerId = TRAINER_TONY,
        .modeA = VGC_PLAN_SUN, .modeB = VGC_PLAN_TAILWIND,
        .setters = { SPECIES_CHARIZARD, SPECIES_SABLEYE },
        .winCons = { SPECIES_ROARING_MOON, SPECIES_CHARIZARD },
        .back = { SPECIES_GYARADOS, SPECIES_SLITHER_WING },
        .leads = {
            { SPECIES_CHARIZARD, SPECIES_SABLEYE, VGC_PLAN_SUN },
            { SPECIES_CHARIZARD, SPECIES_ROARING_MOON, VGC_PLAN_SUN },
            { SPECIES_ROARING_MOON, SPECIES_BRUTE_BONNET, VGC_PLAN_TAILWIND },
        },
    },
    // MARIO LETT: Dondozo + Tatsugiri (Commander) è il piano; Chatot Tailwind / Provocazione lo
    // apre, Sinistcha Trick Room è la riserva, Weezing-Galar spegne le abilità scomode.
    {
        .trainerId = TRAINER_MARIO_LETT,
        .modeA = VGC_PLAN_BALANCE, .modeB = VGC_PLAN_TAILWIND,
        .setters = { SPECIES_CHATOT, SPECIES_SINISTCHA },
        .winCons = { SPECIES_DONDOZO, SPECIES_TATSUGIRI },
        .back = { SPECIES_SLAKING, SPECIES_WEEZING_GALAR },
        .leads = {
            { SPECIES_DONDOZO, SPECIES_TATSUGIRI, VGC_PLAN_BALANCE },
            { SPECIES_CHATOT, SPECIES_DONDOZO, VGC_PLAN_TAILWIND },
            { SPECIES_SINISTCHA, SPECIES_DONDOZO, VGC_PLAN_TRICK_ROOM },
        },
    },
    // TATORE: Trick Room con Dusclops / Scream Tail, Armaldo Danza di Spada e Lapras sotto;
    // in riserva Kecleon Fake Out + Mega Sceptile veloce.
    {
        .trainerId = TRAINER_TATORE,
        .modeA = VGC_PLAN_TRICK_ROOM, .modeB = VGC_PLAN_BALANCE,
        .setters = { SPECIES_DUSCLOPS, SPECIES_SCREAM_TAIL },
        .winCons = { SPECIES_ARMALDO, SPECIES_SCEPTILE },
        .back = { SPECIES_ARMALDO, SPECIES_LAPRAS },
        .leads = {
            { SPECIES_DUSCLOPS, SPECIES_LAPRAS, VGC_PLAN_TRICK_ROOM },
            { SPECIES_KECLEON, SPECIES_SCEPTILE, VGC_PLAN_BALANCE },
            { SPECIES_SCREAM_TAIL, SPECIES_ARMALDO, VGC_PLAN_TRICK_ROOM },
        },
    },
    // MARIO: Hariyama Fake Out + Iron Hands davanti; Oranguru Trick Room / Ordine come riserva.
    {
        .trainerId = TRAINER_MARIO,
        .modeA = VGC_PLAN_BALANCE, .modeB = VGC_PLAN_TRICK_ROOM,
        .setters = { SPECIES_ORANGURU, SPECIES_NONE },
        .winCons = { SPECIES_IRON_HANDS, SPECIES_HARIYAMA },
        .back = { SPECIES_ORANGURU, SPECIES_NONE },
        .leads = {
            { SPECIES_HARIYAMA, SPECIES_IRON_HANDS, VGC_PLAN_BALANCE },
            { SPECIES_ORANGURU, SPECIES_IRON_HANDS, VGC_PLAN_TRICK_ROOM },
            { SPECIES_HARIYAMA, SPECIES_ORANGURU, VGC_PLAN_TRICK_ROOM },
        },
    },
    // ENZO: Tailwind di Mega Aerodactyl e schermi di Grimmsnarl per Annihilape / Mega Gengar;
    // Smeargle (Spora, Ventogelato, Ripeti) e Rillaboom Fake Out aprono.
    {
        .trainerId = TRAINER_ENZO,
        .modeA = VGC_PLAN_TAILWIND, .modeB = VGC_PLAN_BALANCE,
        .setters = { SPECIES_AERODACTYL, SPECIES_GRIMMSNARL },
        .winCons = { SPECIES_ANNIHILAPE, SPECIES_GENGAR },
        .back = { SPECIES_ANNIHILAPE, SPECIES_GENGAR },
        .leads = {
            { SPECIES_AERODACTYL, SPECIES_RILLABOOM, VGC_PLAN_TAILWIND },
            { SPECIES_SMEARGLE, SPECIES_GRIMMSNARL, VGC_PLAN_BALANCE },
            { SPECIES_SMEARGLE, SPECIES_AERODACTYL, VGC_PLAN_TAILWIND },
        },
    },
    // GEROZZ: sabbia di Mega Tyranitar con Dugtrio-Alola (Forzasabbia); Gholdengo Nasty Plot
    // e Great Tusk Bulk Up vincono; Porygon2 Trick Room come riserva.
    {
        .trainerId = TRAINER_GEROZZ,
        .modeA = VGC_PLAN_SAND, .modeB = VGC_PLAN_TRICK_ROOM,
        .setters = { SPECIES_TYRANITAR, SPECIES_PORYGON2 },
        .winCons = { SPECIES_GHOLDENGO, SPECIES_GREAT_TUSK },
        .back = { SPECIES_GHOLDENGO, SPECIES_ROTOM_HEAT },
        .leads = {
            { SPECIES_TYRANITAR, SPECIES_DUGTRIO_ALOLA, VGC_PLAN_SAND },
            { SPECIES_PORYGON2, SPECIES_GREAT_TUSK, VGC_PLAN_TRICK_ROOM },
            { SPECIES_TYRANITAR, SPECIES_ROTOM_HEAT, VGC_PLAN_SAND },
        },
    },
    // CARLETTO: pioggia di Pelipper con Ludicolo Swift Swim; Kingambit chiude dalla panchina,
    // Amoonguss reindirizza per il Belly Drum di Snorlax; Mega Malamar Trick Room in riserva.
    {
        .trainerId = TRAINER_CARLETTO,
        .modeA = VGC_PLAN_RAIN, .modeB = VGC_PLAN_TRICK_ROOM,
        .setters = { SPECIES_PELIPPER, SPECIES_MALAMAR },
        .winCons = { SPECIES_KINGAMBIT, SPECIES_SNORLAX },
        .back = { SPECIES_KINGAMBIT, SPECIES_SNORLAX },
        .leads = {
            { SPECIES_PELIPPER, SPECIES_LUDICOLO, VGC_PLAN_RAIN },
            { SPECIES_AMOONGUSS, SPECIES_SNORLAX, VGC_PLAN_BALANCE },
            { SPECIES_MALAMAR, SPECIES_AMOONGUSS, VGC_PLAN_TRICK_ROOM },
        },
    },
    // GIGI PINTO: sole di Torkoal con Venusaur / Lilligant Clorofilla (Dopo di te, Aiuto);
    // Hatterene Trick Room con Torkoal Eruzione in riserva; Garganacl Salinfusione e
    // Mega Blastoise Idrovortice chiudono.
    {
        .trainerId = TRAINER_GIGI_PINTO,
        .modeA = VGC_PLAN_SUN, .modeB = VGC_PLAN_TRICK_ROOM,
        .setters = { SPECIES_TORKOAL, SPECIES_HATTERENE },
        .winCons = { SPECIES_VENUSAUR, SPECIES_TORKOAL },
        .back = { SPECIES_GARGANACL, SPECIES_BLASTOISE },
        .leads = {
            { SPECIES_TORKOAL, SPECIES_VENUSAUR, VGC_PLAN_SUN },
            { SPECIES_TORKOAL, SPECIES_LILLIGANT, VGC_PLAN_SUN },
            { SPECIES_HATTERENE, SPECIES_TORKOAL, VGC_PLAN_TRICK_ROOM },
        },
    },
};

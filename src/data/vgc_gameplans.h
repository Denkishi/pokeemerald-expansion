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
    VGC_PLAN_GRAVITY,
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
    // SIMONE: Bulky Offense / Simple Tech con Porygon-Z + Toedscruel; Arcanine + Rotom-Mow lead primaria
    // per debuff e scouting veloce. Mega Aggron Filtro e Machamp come win condition.
    {
        .trainerId = TRAINER_SIMONE,
        .modeA = VGC_PLAN_BALANCE, .modeB = VGC_PLAN_TRICK_ROOM,
        .setters = { SPECIES_PORYGON_Z, SPECIES_TOEDSCRUEL },
        .winCons = { SPECIES_AGGRON, SPECIES_MACHAMP },
        .back = { SPECIES_AGGRON, SPECIES_MACHAMP },
        .leads = {
            { SPECIES_ARCANINE, SPECIES_ROTOM_MOW, VGC_PLAN_BALANCE },
            { SPECIES_PORYGON_Z, SPECIES_TOEDSCRUEL, VGC_PLAN_BALANCE },
            { SPECIES_ARCANINE, SPECIES_MACHAMP, VGC_PLAN_BALANCE },
        },
    },
    // ELIA: Hard Trick Room difensivo. Mimikyu (Disguise + Erbamentale) e Mega Banette lo impostano;
    // Goodra-Hisui e Lickitung (Ferrodifesa + Schiacciacorpo) vincono; Vaporeon e Miltank supporto.
    {
        .trainerId = TRAINER_ELIA,
        .modeA = VGC_PLAN_TRICK_ROOM, .modeB = VGC_PLAN_BALANCE,
        .setters = { SPECIES_MIMIKYU, SPECIES_BANETTE },
        .winCons = { SPECIES_GOODRA_HISUI, SPECIES_LICKITUNG },
        .back = { SPECIES_LICKITUNG, SPECIES_GOODRA_HISUI },
        .leads = {
            { SPECIES_MIMIKYU, SPECIES_LICKITUNG, VGC_PLAN_TRICK_ROOM },
            { SPECIES_BANETTE, SPECIES_MILTANK, VGC_PLAN_TRICK_ROOM },
            { SPECIES_MIMIKYU, SPECIES_GOODRA_HISUI, VGC_PLAN_TRICK_ROOM },
        },
    },
    // PATATINA: PsySpam Offensivo (Campo Psichico + Expanding Force di Armarouge con Indeedee-F);
    // riserva con Mega Altaria Pellefolletto Tailwind + Ceruledge Danzaspada; Copperajah TR.
    {
        .trainerId = TRAINER_PATATINA,
        .modeA = VGC_PLAN_PSYCHIC_TERRAIN, .modeB = VGC_PLAN_TAILWIND,
        .setters = { SPECIES_INDEEDEE_F, SPECIES_ALTARIA },
        .winCons = { SPECIES_ARMAROUGE, SPECIES_CERULEDGE },
        .back = { SPECIES_CERULEDGE, SPECIES_COPPERAJAH },
        .leads = {
            { SPECIES_INDEEDEE_F, SPECIES_ARMAROUGE, VGC_PLAN_PSYCHIC_TERRAIN },
            { SPECIES_ALTARIA, SPECIES_ROTOM_WASH, VGC_PLAN_TAILWIND },
            { SPECIES_INDEEDEE_F, SPECIES_COPPERAJAH, VGC_PLAN_TRICK_ROOM },
        },
    },
    // TONY: Sun Hyper-Offense. Mega Charizard Y porta il Sole, Sableye Fake Out/Giornodisole;
    // Roaring Moon e Slither Wing (Protosintesi) sfondano; Brute Bonnet reindirizza.
    {
        .trainerId = TRAINER_TONY,
        .modeA = VGC_PLAN_SUN, .modeB = VGC_PLAN_TAILWIND,
        .setters = { SPECIES_CHARIZARD, SPECIES_SABLEYE },
        .winCons = { SPECIES_CHARIZARD, SPECIES_ROARING_MOON },
        .back = { SPECIES_SLITHER_WING, SPECIES_BRUTE_BONNET },
        .leads = {
            { SPECIES_SABLEYE, SPECIES_CHARIZARD, VGC_PLAN_SUN },
            { SPECIES_ROARING_MOON, SPECIES_SLITHER_WING, VGC_PLAN_TAILWIND },
            { SPECIES_CHARIZARD, SPECIES_ROARING_MOON, VGC_PLAN_SUN },
        },
    },
    // MARIO LETT: Split ad alta varianza. Due binari rigidi e separati:
    // Modalità A (Anti-Ability): Weezing-Galar + Slaking (Neutralizing Gas sblocca Truant).
    // Modalità B (Commander): Chatot + Dondozo (Parting Shot o Tailwind per far entrare Tatsugiri).
    {
        .trainerId = TRAINER_MARIO_LETT,
        .modeA = VGC_PLAN_BALANCE, .modeB = VGC_PLAN_TAILWIND,
        .setters = { SPECIES_CHATOT, SPECIES_SINISTCHA },
        .winCons = { SPECIES_SLAKING, SPECIES_DONDOZO },
        .back = { SPECIES_TATSUGIRI, SPECIES_SINISTCHA },
        .leads = {
            { SPECIES_WEEZING_GALAR, SPECIES_SLAKING, VGC_PLAN_BALANCE },
            { SPECIES_CHATOT, SPECIES_DONDOZO, VGC_PLAN_TAILWIND },
            { SPECIES_SINISTCHA, SPECIES_DONDOZO, VGC_PLAN_TRICK_ROOM },
        },
    },
    // TATORE: Bulky Trick Room con Dusclops / Scream Tail. Kecleon Fake Out apre la strada;
    // Armaldo Danzaspada sfonda sotto TR; Mega Sceptile Quick Guard e cleaner veloce.
    {
        .trainerId = TRAINER_TATORE,
        .modeA = VGC_PLAN_TRICK_ROOM, .modeB = VGC_PLAN_BALANCE,
        .setters = { SPECIES_DUSCLOPS, SPECIES_SCREAM_TAIL },
        .winCons = { SPECIES_ARMALDO, SPECIES_SCEPTILE },
        .back = { SPECIES_ARMALDO, SPECIES_SCEPTILE },
        .leads = {
            { SPECIES_KECLEON, SPECIES_DUSCLOPS, VGC_PLAN_TRICK_ROOM },
            { SPECIES_SCEPTILE, SPECIES_SCREAM_TAIL, VGC_PLAN_BALANCE },
            { SPECIES_DUSCLOPS, SPECIES_LAPRAS, VGC_PLAN_TRICK_ROOM },
        },
    },
    // MARIO: Dual Mode rigorosa: Instruct TR (Oranguru + Hariyama / Iron Hands)
    // oppure Swift Swim Rain veloce (Politoed + Golduck + Mega Meganium Schermi).
    {
        .trainerId = TRAINER_MARIO,
        .modeA = VGC_PLAN_TRICK_ROOM, .modeB = VGC_PLAN_RAIN,
        .setters = { SPECIES_ORANGURU, SPECIES_POLITOED },
        .winCons = { SPECIES_HARIYAMA, SPECIES_GOLDUCK },
        .back = { SPECIES_IRON_HANDS, SPECIES_MEGANIUM },
        .leads = {
            { SPECIES_HARIYAMA, SPECIES_ORANGURU, VGC_PLAN_TRICK_ROOM },
            { SPECIES_POLITOED, SPECIES_GOLDUCK, VGC_PLAN_RAIN },
            { SPECIES_MEGANIUM, SPECIES_GOLDUCK, VGC_PLAN_RAIN },
        },
    },
    // ENZO: Gravity Sleep Trap / Final Gambit. Smeargle piazza Gravità per Hypnosis e Spore al 100%
    // con Mega Gengar Shadow Tag; Annihilape Scarf Final Gambit per il 4v3 istantaneo.
    {
        .trainerId = TRAINER_ENZO,
        .modeA = VGC_PLAN_GRAVITY, .modeB = VGC_PLAN_TAILWIND,
        .setters = { SPECIES_SMEARGLE, SPECIES_AERODACTYL },
        .winCons = { SPECIES_GENGAR, SPECIES_ANNIHILAPE },
        .back = { SPECIES_RILLABOOM, SPECIES_AERODACTYL },
        .leads = {
            { SPECIES_SMEARGLE, SPECIES_GENGAR, VGC_PLAN_GRAVITY },
            { SPECIES_ANNIHILAPE, SPECIES_GRIMMSNARL, VGC_PLAN_BALANCE },
            { SPECIES_AERODACTYL, SPECIES_RILLABOOM, VGC_PLAN_TAILWIND },
        },
    },
    // GEROZZ: Tempesta di sabbia con Mega Tyranitar e Dugtrio-Alola Silicoforza; Gholdengo Nasty Plot;
    // Terremoto coordinato con Rotom-Heat immune via Levitazione; Porygon2 TR come riserva.
    {
        .trainerId = TRAINER_GEROZZ,
        .modeA = VGC_PLAN_SAND, .modeB = VGC_PLAN_TRICK_ROOM,
        .setters = { SPECIES_TYRANITAR, SPECIES_PORYGON2 },
        .winCons = { SPECIES_GHOLDENGO, SPECIES_DUGTRIO_ALOLA },
        .back = { SPECIES_GREAT_TUSK, SPECIES_ROTOM_HEAT },
        .leads = {
            { SPECIES_TYRANITAR, SPECIES_DUGTRIO_ALOLA, VGC_PLAN_SAND },
            { SPECIES_PORYGON2, SPECIES_ROTOM_HEAT, VGC_PLAN_TRICK_ROOM },
            { SPECIES_TYRANITAR, SPECIES_ROTOM_HEAT, VGC_PLAN_SAND },
        },
    },
    // CARLETTO: Pressione idrica con Pelipper Drizzle + Ludicolo Swift Swim;
    // Asse setup con Amoonguss Rage Powder + Snorlax Belly Drum / Mega Malamar Superpower TR.
    {
        .trainerId = TRAINER_CARLETTO,
        .modeA = VGC_PLAN_RAIN, .modeB = VGC_PLAN_TRICK_ROOM,
        .setters = { SPECIES_PELIPPER, SPECIES_MALAMAR },
        .winCons = { SPECIES_SNORLAX, SPECIES_KINGAMBIT },
        .back = { SPECIES_SNORLAX, SPECIES_KINGAMBIT },
        .leads = {
            { SPECIES_PELIPPER, SPECIES_LUDICOLO, VGC_PLAN_RAIN },
            { SPECIES_AMOONGUSS, SPECIES_MALAMAR, VGC_PLAN_TRICK_ROOM },
            { SPECIES_AMOONGUSS, SPECIES_SNORLAX, VGC_PLAN_TRICK_ROOM },
        },
    },
    // GIGI PINTO: After You Eruption solare istantanea con Lilligant Clorofilla + Torkoal Siccità;
    // Asse alternativo con Hatterene Magispecchio Trick Room + Torkoal / Garganacl.
    {
        .trainerId = TRAINER_GIGI_PINTO,
        .modeA = VGC_PLAN_SUN, .modeB = VGC_PLAN_TRICK_ROOM,
        .setters = { SPECIES_TORKOAL, SPECIES_HATTERENE },
        .winCons = { SPECIES_TORKOAL, SPECIES_VENUSAUR },
        .back = { SPECIES_GARGANACL, SPECIES_VENUSAUR },
        .leads = {
            { SPECIES_LILLIGANT, SPECIES_TORKOAL, VGC_PLAN_SUN },
            { SPECIES_HATTERENE, SPECIES_GARGANACL, VGC_PLAN_TRICK_ROOM },
            { SPECIES_TORKOAL, SPECIES_VENUSAUR, VGC_PLAN_SUN },
        },
    },
};

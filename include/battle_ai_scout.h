#ifndef GUARD_BATTLE_AI_SCOUT_H
#define GUARD_BATTLE_AI_SCOUT_H

// "Scouting" della squadra del giocatore come in una vera team preview: l'IA vede solo le
// specie e ne deduce i ruoli PROBABILI (non le mosse vere). Fonti, in ordine di peso:
//   1. tabella di "meta" scritta a mano (src/battle_ai_scout.c, sScoutMeta): ciò che quel
//      Pokémon porta quasi sempre in VGC (Amoonguss = Rage Powder + Spore, ecc.);
//   2. abilità possibili della specie (meteo, campi, Intimidate);
//   3. learnset: "può impararla" vale poco, ma non zero.
// In lotta, appena una mossa si vede, vale il dato reale (vedi Scout_BattlerRoleChance).

enum ScoutRole
{
    SCOUT_TRICK_ROOM,
    SCOUT_TAILWIND,
    SCOUT_SOFT_SPEED,   // Ventogelato, Elettrotela, Tuononda, Faccia Spaventosa
    SCOUT_FAKE_OUT,
    SCOUT_REDIRECT,     // Segui Me, Polverabbia
    SCOUT_TAUNT,
    SCOUT_SLEEP,
    SCOUT_PERISH,
    SCOUT_WIDE_GUARD,
    SCOUT_SETUP,        // Danzaspada, Dragodanza, Congiura, Panciamburo...
    SCOUT_HELPING_HAND,
    SCOUT_ROLE_COUNT,
};

enum ScoutWeather
{
    SCOUT_WEATHER_NONE,
    SCOUT_WEATHER_SUN,
    SCOUT_WEATHER_RAIN,
    SCOUT_WEATHER_SAND,
    SCOUT_WEATHER_SNOW,
};

enum ScoutTerrain
{
    SCOUT_TERRAIN_NONE,
    SCOUT_TERRAIN_GRASSY,
    SCOUT_TERRAIN_ELECTRIC,
    SCOUT_TERRAIN_PSYCHIC,
    SCOUT_TERRAIN_MISTY,
};

struct ScoutProfile
{
    u8 role[SCOUT_ROLE_COUNT];  // probabilità 0..100
    u8 weather;                 // meteo che imposta con l'abilità (SCOUT_WEATHER_*)
    u8 weatherP;
    u8 weatherAbuse;            // meteo che sfrutta (Nuotovelox, Clorofilla, Protosintesi...)
    u8 terrain;                 // campo che imposta con l'abilità (SCOUT_TERRAIN_*)
    u8 terrainP;
    u8 intimidateP;
    u8 sleepImmuneP;            // Insonnia, Spirito Vitale, Dolcevelo, Corpo Aureo, Magispecchio, Erba...
    u8 baseSpeed;
    u8 physical;                // TRUE = attaccante fisico (Atk base > SpA base)
    u16 species;
};

// Mosse d'attacco che la specie può imparare (livello, MT, uova; anche della specie base), senza
// doppioni per tipo (per ogni tipo resta la più potente). Ritorna quante ne ha scritte in `out`.
u32 Scout_CollectLearnableAttacks(u16 species, enum Move *out, u32 max);

void Scout_GetSpeciesProfile(u16 species, struct ScoutProfile *out);

// Probabilità (0..100) che un Pokémon del giocatore in campo abbia questo ruolo: se l'IA ha già
// visto una mossa del ruolo vale 100, se ha visto tutte e 4 le mosse e nessuna è del ruolo vale 0,
// altrimenti la stima dalla specie.
u32 Scout_BattlerRoleChance(enum BattlerId battler, enum ScoutRole role);

// Mossa del ruolo indicato?
bool32 Scout_MoveHasRole(enum Move move, enum ScoutRole role);

// --- Strumenti ------------------------------------------------------------------------------
// L'IA conosce la LISTA degli strumenti del formato (src/data/vgc_item_pool.h: squadre VGC e
// team rental, Megapietre comprese) ma non chi tiene cosa. Da specie + lista stima lo strumento
// più probabile; in lotta vale finché non lo vede (attivazione, Frisk, Privazione...) o lo deduce
// (si è mosso prima di un nostro Pokémon più veloce senza priorità = Stolascelta).
struct ScoutItemGuess
{
    u16 effect;         // HOLD_EFFECT_* più probabile (HOLD_EFFECT_NONE = nessuna idea)
    u8 chance;          // 0..100
    u16 megaSpecies;    // forma Mega raggiungibile con una pietra presente in lista, SPECIES_NONE se no
    u8 megaChance;
};

void Scout_GuessItem(u16 species, struct ScoutItemGuess *out);
// Strumento assunto per un Pokémon del giocatore in campo quando non è noto (HOLD_EFFECT_NONE se incerto).
enum HoldEffect Scout_AssumedHoldEffect(enum BattlerId battler);
// TRUE se si è visto lo strumento sparire (consumato o tolto con Privazione): informazione pubblica.
bool32 Scout_ItemKnownGone(enum BattlerId battler);
// Statistiche stimate di un Pokémon del giocatore da specie e livello (spread tipica del profilo),
// al posto di quelle vere che il giocatore non mostra.
void Scout_EstimateStats(u16 species, u32 level, u16 *hp, u16 stats[NUM_STATS]);
// Abilità certa della specie (una sola possibile), altrimenti ABILITY_NONE.
enum Ability Scout_KnownAbility(u16 species);
// Forma Mega probabile di una specie del giocatore (SPECIES_NONE se nessuna o pietra assente dalla lista).
u16 Scout_MegaTargetSpecies(u16 species);
bool32 Scout_PoolHasItem(u16 item);

// =============================================================================================
// M1 — Base di conoscenza VGC e belief (docs/ia_vgc/piano_implementazione.md, M1)
// =============================================================================================
// Base di conoscenza in ROM: src/data/vgc_meta.h, generata da statistiche d'uso pubbliche Smogon/Showdown
// con tools/vgc_meta/gen_vgc_meta.py (Champions VGC con Mega, poi Reg G / doppie per chi lì non c'è).
// Per specie (forma esatta; una Mega usa la voce della forma base): spread con frequenza, mosse con
// frequenza, strumenti, abilità con frequenza, Mega possibili. Le specie senza voce usano le stime da
// learnset/tabella a mano di sopra. Tutte queste funzioni leggono solo ROM: si possono chiamare ovunque.
//
// Già collegate senza toccare il pianificatore: Scout_EstimateStats usa la spread più usata,
// Scout_GuessItem gli strumenti/Mega più usati, Scout_GetSpeciesProfile i ruoli dalle mosse più usate,
// Scout_AssumedHoldEffect lo strumento dedotto dal belief.

struct ScoutSpread
{
    u8 nature;
    u8 p;               // % dei set della specie con questa spread (raggruppate per profilo d'investimento)
    u8 ev[NUM_STATS];   // EV veri (0..252), indice STAT_*
};

struct ScoutMoveChance { u16 move; u8 p; };
struct ScoutItemChance { u16 item; u8 p; };
struct ScoutMegaChance { u16 species; u16 ability; u8 p; };

bool32 Scout_HasMetaData(u16 species);
u32 Scout_MetaUsage(u16 species);
bool32 Scout_MetaTableIsSorted(void); // per i test: la ricerca binaria vuole la tabella ordinata per specie   // % di squadre del formato che la portano (0 se nessun dato)
// Spread più usate (la prima è la più probabile). Ritorna quante (0 = specie senza statistiche).
u32 Scout_GetMetaSpreads(u16 species, struct ScoutSpread *out, u32 max);
// Statistiche da una spread (31 IV; 0 IV in Velocità per le spread da Trick Room con natura -Vel e 0 EV).
void Scout_StatsFromSpread(u16 species, u32 level, const struct ScoutSpread *spread, u16 stats[NUM_STATS]);
// Velocità possibile della specie: [0 IV, 0 EV, natura -] .. [31 IV, 252 EV, natura +].
void Scout_SpeciesSpeedRange(u16 species, u32 level, u16 *lo, u16 *hi);
// Mosse più usate con probabilità (0..100), solo quelle che la specie può imparare in questo gioco.
// Scorre il learnset: chiamarla una volta per turno / per specie, non in ogni simulazione.
u32 Scout_GetLikelyMoves(u16 species, struct ScoutMoveChance *out, u32 max);
// Probabilità (0..100) che la specie porti la mossa: statistiche se la mossa è tra le più usate,
// altrimenti profilo dei ruoli (learnset + tabella a mano), 0 se non può impararla.
u32 Scout_MoveChance(u16 species, enum Move move);
// Strumenti più usati (Megapietre escluse: vedi Scout_GetMegaForms), probabilità sul totale dei set.
u32 Scout_GetLikelyItems(u16 species, struct ScoutItemChance *out, u32 max);
// Mega usate dalla specie con probabilità e abilità della Mega (Charizard: Y 90%, Drought...).
u32 Scout_GetMegaForms(u16 species, struct ScoutMegaChance *out, u32 max);
// Probabilità (0..100) di un'abilità della specie. Per una Mega (forma già in campo) l'abilità è certa.
// Prima della Mega vale l'abilità della forma base (Salamence: Intimidate prima, Aerilate dopo).
u32 Scout_AbilityChance(u16 species, enum Ability ability);
enum Ability Scout_LikelyAbility(u16 species, u32 *chance);

// --- Belief sui Pokémon del giocatore in lotta (EWRAM: 14 byte x 6) ---------------------------------
// Si restringe con ciò che si VEDE: ordine di turno (Velocità, Choice Scarf), danni fatti e subiti
// (Choice Band/Specs, Assault Vest, Eviolite, "strumento offensivo" tipo Life Orb), abilità d'ingresso
// che non si sono attivate (Intimidate, meteo, campi, Pressure...). Una Choice dedotta si ritira se il
// Pokémon cambia mossa senza uscire; una Scarf se si dimostra troppo lento. Le deduzioni di strumento
// vanno anche in gBattleHistory/gAiPartyData (come Scout_AssumedHoldEffect), quindi il resto dell'IA le
// vede già. Agganci nel motore: Scout_BeliefReset (Ai_InitPartyStruct), Scout_BeliefTurnStart
// (inizio turno IA), Scout_BeliefNoteMove (HandleAction_UseMove), Scout_BeliefNoteDamage (calcolo danni).
#define SCOUT_NOT_SCARF        (1 << 0)
#define SCOUT_NOT_BAND         (1 << 1)
#define SCOUT_NOT_SPECS        (1 << 2)
#define SCOUT_NOT_ASSAULT_VEST (1 << 3)
#define SCOUT_NOT_LIFE_ORB     (1 << 4)
#define SCOUT_NOT_FOCUS_SASH   (1 << 5)
#define SCOUT_NOT_EVIOLITE     (1 << 6)

struct DamageContext;
void Scout_BeliefReset(void);
void Scout_BeliefTurnStart(void);
void Scout_BeliefNoteMove(enum BattlerId battler, enum Move move);
void Scout_BeliefNoteDamage(const struct DamageContext *ctx, s32 damage);

// Velocità (statistica, senza stadi/strumento/abilità) ancora compatibile con l'ordine di turno visto.
// FALSE = nessun belief per quel Pokémon (lo e hi = intervallo possibile della specie).
bool32 Scout_BattlerSpeedRange(enum BattlerId battler, u16 *lo, u16 *hi);
// Come Scout_EstimateStats, ma con la Velocità dentro l'intervallo del belief (sceglie la spread compatibile).
void Scout_EstimateBattlerStats(enum BattlerId battler, u16 *hp, u16 stats[NUM_STATS]);
// Velocità effettiva creduta (stadi, Tailwind, paralisi, meteo con l'abilità e lo strumento passati).
u32 Scout_BattlerBelievedSpeed(enum BattlerId battler, enum Ability ability, enum HoldEffect holdEffect);
// Velocità effettiva massima possibile per la specie (31 IV, 252 EV, natura +), con stadi/Tailwind/meteo pubblici.
u32 Scout_BattlerMaxSpeed(enum BattlerId battler, enum Ability ability, u32 holdEffect);
enum HoldEffect Scout_BattlerDeducedHoldEffect(enum BattlerId battler);
bool32 Scout_BattlerHoldEffectExcluded(enum BattlerId battler, enum HoldEffect holdEffect);
bool32 Scout_BattlerOffenseBoosted(enum BattlerId battler);   // più danni del massimo senza strumento
// Abilità: vista = 100/0; altrimenti statistiche della specie senza le abilità d'ingresso escluse.
u32 Scout_BattlerAbilityChance(enum BattlerId battler, enum Ability ability);
enum Ability Scout_BattlerLikelyAbility(enum BattlerId battler, u32 *chance);
// Mossa: vista = 100; 4 viste e non c'è = 0; Choice dedotta bloccata su un'altra = 0; altrimenti Scout_MoveChance.
u32 Scout_BattlerMoveChance(enum BattlerId battler, enum Move move);

// Bo3: il belief di un Pokémon (indice di gPlayerParty) passa alla gara dopo con SeriesMonMemory.
struct ScoutBeliefSnapshot
{
    u16 species;
    u16 speedLo;
    u16 speedHi;
    u8 itemNot;
    u8 abilityExcluded;
    u8 heldEffect;
};
void Scout_BeliefExport(u32 partySlot, struct ScoutBeliefSnapshot *out);
void Scout_BeliefImport(u32 partySlot, const struct ScoutBeliefSnapshot *in);

#endif // GUARD_BATTLE_AI_SCOUT_H

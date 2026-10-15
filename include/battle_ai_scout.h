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
};

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
// Forma Mega probabile di una specie del giocatore (SPECIES_NONE se nessuna o pietra assente dalla lista).
u16 Scout_MegaTargetSpecies(u16 species);
bool32 Scout_PoolHasItem(u16 item);

#endif // GUARD_BATTLE_AI_SCOUT_H

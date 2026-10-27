// Conoscenza della squadra ricavata dalle proprietà (vedi include/battle_ai_vgc_graph.h).
//
// Ogni regola è scritta UNA volta per famiglia di meccaniche e vale per tutte le squadre:
//   - chi vuole un turno 1 protetto (setup, Sostituto)  + chi lo dà (Fake Out, redirect, Prankster, Intimidate)
//   - chi colpisce il compagno per attivargli l'abilità (Beat Up -> Justified, mossa Acqua -> Water Absorb...)
//   - chi toglie al compagno un'abilità penalizzante (Simple Beam / Worry Seed / Skill Swap -> Mycelium Might...)
//   - chi accumula boost e li passa (Baton Pass) + il miglior ricevente rimasto in panchina
//   - chi porta il meteo / Trick Room + chi lo sfrutta
// Il grafo non decide le mosse: dice quali giocate considerare sempre e quali coppie stanno bene insieme.
// È la simulazione del turno a dire se, in quella posizione, conviene davvero.

#include "global.h"
#include "battle.h"
#include "battle_ai_util.h"
#include "battle_ai_vgc_graph.h"
#include "battle_util.h"
#include "fpmath.h"
#include "item.h"
#include "move.h"
#include "pokemon.h"
#include "constants/abilities.h"
#include "constants/battle_move_effects.h"
#include "constants/form_change_types.h"
#include "constants/hold_effects.h"
#include "constants/moves.h"

#define STRONG_ATTACK_POWER 70

struct MonFacts
{
    u16 species, effSpecies;
    enum Ability ability;
    enum HoldEffect holdEffect;
    enum Move moves[MAX_MON_MOVES];
    u8 nPhysical, nSpecial;     // attacchi forti per categoria
    u8 boostsAtk:1, boostsSpa:1, boostsSpe:1;
    u8 valid:1;
};

static u16 MegaSpeciesFor(u16 species, u16 item)
{
    const struct FormChange *fc = GetSpeciesFormChanges(species);
    u32 i;

    if (fc == NULL || item == ITEM_NONE)
        return SPECIES_NONE;
    for (i = 0; fc[i].method != FORM_CHANGE_TERMINATOR; i++)
    {
        if (fc[i].method == FORM_CHANGE_BATTLE_MEGA_EVOLUTION_ITEM && fc[i].param1 == item)
            return fc[i].targetSpecies;
    }
    return SPECIES_NONE;
}

static bool32 IsStrongAttack(enum Move move)
{
    return !IsBattleMoveStatus(move)
        && (GetMovePower(move) >= STRONG_ATTACK_POWER || GetMoveStrikeCount(move) > 1);
}

// Stadi che la mossa alza a chi la usa.
static void SelfBoosts(enum Move move, struct MonFacts *f)
{
    u32 i, n;

    if (!IsBattleMoveStatus(move) || GetMoveTarget(move) != TARGET_USER)
        return;
    n = GetMoveAdditionalEffectCount(move);
    for (i = 0; i < n; i++)
    {
        const struct AdditionalEffect *e = GetMoveAdditionalEffectById(move, i);
        if (e->moveEffect != STAT_CHANGE_EFFECT_PLUS)
            continue;
        if (e->attack) f->boostsAtk = TRUE;
        if (e->spAtk) f->boostsSpa = TRUE;
        if (e->speed) f->boostsSpe = TRUE;
    }
    if (GetMoveEffect(move) == EFFECT_BELLY_DRUM)
        f->boostsAtk = TRUE;
}

static bool32 IsHinderAbility(enum Ability ability)
{
    switch (ability)
    {
    case ABILITY_MYCELIUM_MIGHT:
    case ABILITY_TRUANT:
    case ABILITY_SLOW_START:
    case ABILITY_DEFEATIST:
    case ABILITY_KLUTZ:
    case ABILITY_STALL:
        return TRUE;
    default:
        return FALSE;
    }
}

static bool32 IsWeatherSetter(enum Ability ability)
{
    return ability == ABILITY_DROUGHT || ability == ABILITY_DRIZZLE || ability == ABILITY_SAND_STREAM
        || ability == ABILITY_SNOW_WARNING || ability == ABILITY_ORICHALCUM_PULSE;
}

static bool32 IsWeatherAbuser(enum Ability ability)
{
    return ability == ABILITY_SWIFT_SWIM || ability == ABILITY_CHLOROPHYLL || ability == ABILITY_SAND_RUSH
        || ability == ABILITY_SLUSH_RUSH || ability == ABILITY_SOLAR_POWER || ability == ABILITY_SAND_FORCE
        || ability == ABILITY_PROTOSYNTHESIS;
}

// Il compagno `def` trae vantaggio dall'essere colpito con un attacco di tipo `type`?
// absorb = TRUE se l'abilità lo rende immune (si può colpire anche forte).
static bool32 HitTriggersPartner(const struct MonFacts *def, enum Type type, bool32 physical, bool32 *absorb)
{
    *absorb = FALSE;
    switch (def->ability)
    {
    case ABILITY_JUSTIFIED:
        return type == TYPE_DARK;
    case ABILITY_RATTLED:
        return type == TYPE_DARK || type == TYPE_BUG || type == TYPE_GHOST;
    case ABILITY_STEAM_ENGINE:
        return type == TYPE_FIRE || type == TYPE_WATER;
    case ABILITY_STAMINA:
        return TRUE;
    case ABILITY_WEAK_ARMOR:
        return physical;
    case ABILITY_WATER_ABSORB:
    case ABILITY_STORM_DRAIN:
        *absorb = (type == TYPE_WATER);
        return *absorb;
    case ABILITY_VOLT_ABSORB:
    case ABILITY_LIGHTNING_ROD:
    case ABILITY_MOTOR_DRIVE:
        *absorb = (type == TYPE_ELECTRIC);
        return *absorb;
    case ABILITY_FLASH_FIRE:
        *absorb = (type == TYPE_FIRE);
        return *absorb;
    case ABILITY_SAP_SIPPER:
        *absorb = (type == TYPE_GRASS);
        return *absorb;
    default:
        break;
    }
    if (def->holdEffect == HOLD_EFFECT_WEAKNESS_POLICY)
    {
        u32 mod = GetTypeModifier(type, gSpeciesInfo[def->effSpecies].types[0]);
        if (gSpeciesInfo[def->effSpecies].types[1] != gSpeciesInfo[def->effSpecies].types[0])
            mod = (mod * GetTypeModifier(type, gSpeciesInfo[def->effSpecies].types[1])) >> 12;
        return mod > UQ_4_12(1.0);
    }
    return FALSE;
}

static bool32 CanTriggerPartner(const struct MonFacts *atk, const struct MonFacts *def)
{
    u32 m;

    for (m = 0; m < MAX_MON_MOVES; m++)
    {
        enum Move move = atk->moves[m];
        bool32 absorb;
        enum Type type;

        if (move == MOVE_NONE || IsBattleMoveStatus(move))
            continue;
        type = (GetMoveEffect(move) == EFFECT_BEAT_UP) ? TYPE_DARK : GetMoveType(move);
        if (!HitTriggersPartner(def, type, GetMoveCategory(move) == DAMAGE_CATEGORY_PHYSICAL, &absorb))
            continue;
        // Colpire il compagno deve costargli poco: assorbito, oppure colpo debole / Beat Up.
        if (absorb || GetMoveEffect(move) == EFFECT_BEAT_UP || GetMovePower(move) <= 40)
            return TRUE;
    }
    return FALSE;
}

// Questo attacco sul compagno in campo è una combo vera (gli attiva l'abilità o lo strumento costandogli poco)?
// Stessa regola del grafo, ma sulla mossa precisa: Fake Out (tipo Normale, 40) accanto a un Flash Fire non attiva
// niente e il tentennamento sul compagno è solo un danno. Le mosse a primo turno (Fake Out & co.) non lo sono mai.
bool32 AI_VgcIsPartnerHitCombo(enum Move move, enum BattlerId battlerAtk, enum BattlerId partner)
{
    struct MonFacts def = {0};
    enum Type type;
    bool32 absorb;

    if (move == MOVE_NONE || IsBattleMoveStatus(move) || GetMoveEffect(move) == EFFECT_FIRST_TURN_ONLY
     || partner == battlerAtk || !IsBattlerAlive(partner))
        return FALSE;
    def.species = def.effSpecies = gBattleMons[partner].species;
    def.ability = gBattleMons[partner].ability;
    def.holdEffect = GetBattlerHoldEffect(partner);
    type = (GetMoveEffect(move) == EFFECT_BEAT_UP) ? TYPE_DARK : GetMoveType(move);
    if (!HitTriggersPartner(&def, type, GetMoveCategory(move) == DAMAGE_CATEGORY_PHYSICAL, &absorb))
        return FALSE;
    return absorb || GetMoveEffect(move) == EFFECT_BEAT_UP || GetMovePower(move) <= 40;
}

static bool32 CanChangePartnerAbility(const struct MonFacts *f)
{
    u32 m;
    for (m = 0; m < MAX_MON_MOVES; m++)
    {
        if (f->moves[m] == MOVE_NONE)
            continue;
        switch (GetMoveEffect(f->moves[m]))
        {
        case EFFECT_OVERWRITE_ABILITY:
        case EFFECT_SKILL_SWAP:
        case EFFECT_GASTRO_ACID:
        case EFFECT_ENTRAINMENT:
            return TRUE;
        default:
            break;
        }
    }
    return FALSE;
}

static void ReadFacts(struct Pokemon *mon, struct MonFacts *f)
{
    u32 m;
    u16 item, mega;

    memset(f, 0, sizeof(*f));
    f->species = GetMonData(mon, MON_DATA_SPECIES);
    if (f->species == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
        return;
    f->valid = TRUE;
    item = GetMonData(mon, MON_DATA_HELD_ITEM);
    mega = MegaSpeciesFor(f->species, item);
    f->effSpecies = (mega != SPECIES_NONE) ? mega : f->species;
    f->ability = GetMonAbility(mon);
    f->holdEffect = GetItemHoldEffect(item);
    for (m = 0; m < MAX_MON_MOVES; m++)
    {
        enum Move move = GetMonData(mon, MON_DATA_MOVE1 + m);
        f->moves[m] = move;
        if (move == MOVE_NONE)
            continue;
        if (IsStrongAttack(move))
        {
            if (GetMoveCategory(move) == DAMAGE_CATEGORY_PHYSICAL)
                f->nPhysical++;
            else
                f->nSpecial++;
        }
        SelfBoosts(move, f);
    }
}

static u32 RolesOf(const struct MonFacts *f)
{
    const struct SpeciesInfo *si = &gSpeciesInfo[f->effSpecies];
    u32 roles = 0, m;
    bool32 hasBatonPass = FALSE;

    if (f->ability == ABILITY_INTIMIDATE)      roles |= VGC_ROLE_INTIMIDATE;
    if (IsHinderAbility(f->ability))           roles |= VGC_ROLE_HINDER_ABILITY;
    if (IsWeatherSetter(f->ability))           roles |= VGC_ROLE_WEATHER_SETTER;
    if (IsWeatherAbuser(f->ability))           roles |= VGC_ROLE_WEATHER_ABUSER;
    if (si->baseSpeed <= 50)                   roles |= VGC_ROLE_SLOW;
    if (CanChangePartnerAbility(f))            roles |= VGC_ROLE_ABILITY_CHANGER;

    for (m = 0; m < MAX_MON_MOVES; m++)
    {
        enum Move move = f->moves[m];
        if (move == MOVE_NONE)
            continue;
        switch (GetMoveEffect(move))
        {
        case EFFECT_FIRST_TURN_ONLY: roles |= VGC_ROLE_FAKE_OUT; break;
        case EFFECT_FOLLOW_ME:       roles |= VGC_ROLE_REDIRECT; break;
        case EFFECT_TAILWIND:        roles |= VGC_ROLE_TAILWIND; break;
        case EFFECT_TRICK_ROOM:      roles |= VGC_ROLE_TRICK_ROOM; break;
        case EFFECT_WEATHER:         roles |= VGC_ROLE_WEATHER_SETTER; break;
        case EFFECT_HELPING_HAND:    roles |= VGC_ROLE_HELPING_HAND; break;
        case EFFECT_SUBSTITUTE:
        case EFFECT_BELLY_DRUM:      roles |= VGC_ROLE_NEEDS_COVER; break;
        case EFFECT_BATON_PASS:      hasBatonPass = TRUE; break;
        case EFFECT_PARTING_SHOT:
        case EFFECT_HIT_ESCAPE:
        case EFFECT_TELEPORT:        roles |= VGC_ROLE_PIVOT; break;
        default: break;
        }
        if (f->ability == ABILITY_PRANKSTER && IsBattleMoveStatus(move))
            roles |= VGC_ROLE_PRANKSTER;
    }
    if (f->boostsAtk || f->boostsSpa)
        roles |= VGC_ROLE_NEEDS_COVER;
    if (hasBatonPass && (f->boostsAtk || f->boostsSpa || f->boostsSpe || f->ability == ABILITY_SPEED_BOOST)
     && f->nPhysical + f->nSpecial <= 1)
        roles |= VGC_ROLE_BATON_PASSER | VGC_ROLE_NEEDS_COVER;
    if (f->nSpecial >= 2 && si->baseSpAttack >= 100)
        roles |= VGC_ROLE_SPA_SWEEPER;
    if (f->nPhysical >= 2 && si->baseAttack >= 100)
        roles |= VGC_ROLE_ATK_SWEEPER;
    if (f->ability == ABILITY_JUSTIFIED || f->ability == ABILITY_STAMINA || f->ability == ABILITY_STEAM_ENGINE
     || f->ability == ABILITY_WEAK_ARMOR || f->ability == ABILITY_RATTLED || f->holdEffect == HOLD_EFFECT_WEAKNESS_POLICY)
        roles |= VGC_ROLE_HIT_TRIGGER;
    return roles;
}

// Quanto `r` sfrutta i boost del Baton Passer `p` (0 = per niente).
static u32 RecipientScore(const struct MonFacts *p, const struct MonFacts *r)
{
    const struct SpeciesInfo *si = &gSpeciesInfo[r->effSpecies];
    u32 spa = si->baseSpAttack * r->nSpecial, atk = si->baseAttack * r->nPhysical;

    if (p->boostsSpa && !p->boostsAtk)
        return spa;
    if (p->boostsAtk && !p->boostsSpa)
        return atk;
    return max(spa, atk);
}

void AI_VgcBuildTeamGraph(struct Pokemon *party, struct VgcTeamGraph *g)
{
    struct MonFacts f[PARTY_SIZE];
    u32 i, j;

    memset(g, 0, sizeof(*g));
    for (i = 0; i < PARTY_SIZE; i++)
    {
        ReadFacts(&party[i], &f[i]);
        g->batonRecipient[i] = g->hitPartner[i] = g->abilityFixPartner[i] = g->requiredBack[i] = VGC_GRAPH_NONE;
        g->effSpecies[i] = f[i].effSpecies;
        if (f[i].valid)
            g->roles[i] = RolesOf(&f[i]);
    }

    for (i = 0; i < PARTY_SIZE; i++)
    {
        u32 bestRecipient = 0;

        if (!f[i].valid)
            continue;
        for (j = 0; j < PARTY_SIZE; j++)
        {
            u32 ri = g->roles[i], rj = g->roles[j];
            s32 s = 0;

            if (i == j || !f[j].valid)
                continue;
            // Turno 1 protetto per chi fa setup.
            if ((ri & VGC_ROLE_NEEDS_COVER) && (rj & (VGC_ROLE_FAKE_OUT | VGC_ROLE_REDIRECT | VGC_ROLE_PRANKSTER)))
                s += 85;
            if ((ri & VGC_ROLE_NEEDS_COVER) && (rj & VGC_ROLE_INTIMIDATE))
                s += 25;
            // Colpire il compagno per attivargli l'abilità.
            if (CanTriggerPartner(&f[i], &f[j]))
            {
                g->hitPartner[i] = j;
                s += 130;
            }
            // Togliere al compagno l'abilità che lo penalizza.
            if ((ri & VGC_ROLE_ABILITY_CHANGER) && (rj & VGC_ROLE_HINDER_ABILITY))
            {
                g->abilityFixPartner[i] = j;
                s += 95;
            }
            // Meteo e Trick Room insieme a chi li sfrutta.
            if ((ri & VGC_ROLE_WEATHER_SETTER) && (rj & VGC_ROLE_WEATHER_ABUSER))
                s += 80;
            if ((ri & VGC_ROLE_TRICK_ROOM) && (rj & VGC_ROLE_SLOW))
                s += 60;
            if ((ri & VGC_ROLE_TAILWIND) && !(rj & VGC_ROLE_SLOW) && (rj & (VGC_ROLE_ATK_SWEEPER | VGC_ROLE_SPA_SWEEPER)))
                s += 40;
            if ((ri & VGC_ROLE_HELPING_HAND) && (rj & (VGC_ROLE_ATK_SWEEPER | VGC_ROLE_SPA_SWEEPER)))
                s += 30;
            // Miglior ricevente dei boost di un Baton Passer.
            if (ri & VGC_ROLE_BATON_PASSER)
            {
                u32 score = RecipientScore(&f[i], &f[j]);
                if (score > bestRecipient)
                {
                    bestRecipient = score;
                    g->batonRecipient[i] = j;
                }
            }
            // Le sinergie valgono in entrambe le direzioni (si somma una volta per verso).
            g->leadPair[i][j] += s;
            g->leadPair[j][i] += s;
        }
        // Il ricevente del Baton Pass deve restare in panchina, non partire accanto al passatore.
        if (g->batonRecipient[i] != VGC_GRAPH_NONE)
        {
            g->requiredBack[i] = g->batonRecipient[i];
            g->leadPair[i][g->batonRecipient[i]] -= 60;
            g->leadPair[g->batonRecipient[i]][i] -= 60;
        }
    }
}

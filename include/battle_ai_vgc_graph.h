#ifndef GUARD_BATTLE_AI_VGC_GRAPH_H
#define GUARD_BATTLE_AI_VGC_GRAPH_H

// Conoscenza della squadra ricavata dalle proprietà dei Pokémon (mosse, abilità, strumenti, statistiche
// anche della forma Mega), mai dai nomi: ruoli, coppie di lead, combo sul compagno, ricevente di Baton
// Pass, vincoli di panchina. Si calcola una volta per squadra (pochi microsecondi) e vale per qualunque
// squadra, presente o futura. Vedi docs/ia_vgc/piano_implementazione.md (M5).

#define VGC_ROLE_FAKE_OUT        (1u << 0)
#define VGC_ROLE_REDIRECT        (1u << 1)
#define VGC_ROLE_PRANKSTER       (1u << 2)
#define VGC_ROLE_INTIMIDATE      (1u << 3)
#define VGC_ROLE_TAILWIND        (1u << 4)
#define VGC_ROLE_TRICK_ROOM      (1u << 5)
#define VGC_ROLE_WEATHER_SETTER  (1u << 6)
#define VGC_ROLE_WEATHER_ABUSER  (1u << 7)
#define VGC_ROLE_ABILITY_CHANGER (1u << 8)   // Simple Beam, Worry Seed, Skill Swap, Gastro Acid, Entrainment
#define VGC_ROLE_HINDER_ABILITY  (1u << 9)   // Mycelium Might, Truant, Slow Start, Defeatist, Klutz, Stall
#define VGC_ROLE_NEEDS_COVER     (1u << 10)  // setup o Sostituto: vuole un turno 1 protetto dal compagno
#define VGC_ROLE_BATON_PASSER    (1u << 11)  // accumula boost e li passa, attacca poco o niente
#define VGC_ROLE_SPA_SWEEPER     (1u << 12)
#define VGC_ROLE_ATK_SWEEPER     (1u << 13)
#define VGC_ROLE_HIT_TRIGGER     (1u << 14)  // abilità/strumento che si attiva se colpito (Justified, Weakness Policy...)
#define VGC_ROLE_HELPING_HAND    (1u << 15)
#define VGC_ROLE_PIVOT           (1u << 16)  // Parting Shot, U-turn, Volt Switch, Teleport
#define VGC_ROLE_SLOW            (1u << 17)  // vuole Trick Room

#define VGC_GRAPH_NONE 0xFF

struct VgcTeamGraph
{
    u32 roles[PARTY_SIZE];
    s16 leadPair[PARTY_SIZE][PARTY_SIZE];  // sinergia della coppia (i, j) in campo insieme
    u8 batonRecipient[PARTY_SIZE];         // per un Baton Passer: chi deve ricevere i boost
    u8 hitPartner[PARTY_SIZE];             // compagno di cui questo Pokémon attiva l'abilità colpendolo
    u8 abilityFixPartner[PARTY_SIZE];      // compagno di cui questo Pokémon può togliere l'abilità penalizzante
    u8 requiredBack[PARTY_SIZE];           // se porto i, j deve venire con me (Baton Passer -> ricevente)
    u16 effSpecies[PARTY_SIZE];            // specie usata per le statistiche (forma Mega se tiene la pietra)
};

void AI_VgcBuildTeamGraph(struct Pokemon *party, struct VgcTeamGraph *graph);
bool32 AI_VgcIsPartnerHitCombo(enum Move move, enum BattlerId battlerAtk, enum BattlerId partner);

#endif // GUARD_BATTLE_AI_VGC_GRAPH_H

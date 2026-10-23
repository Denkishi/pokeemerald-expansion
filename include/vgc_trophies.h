#ifndef GUARD_VGC_TROPHIES_H
#define GUARD_VGC_TROPHIES_H

#define VGC_TEAMS_COUNT     12
#define VGC_TRAINERS_COUNT  12

void OpenVgcTrophiesScreen(void);
void SetVgcTrophy(u32 teamIdx, u32 trainerIdx);
bool32 GetVgcTrophy(u32 teamIdx, u32 trainerIdx);
u32 GetVgcTeamTrophiesCount(u32 teamIdx);
u32 GetTotalVgcTrophiesCount(void);

#endif // GUARD_VGC_TROPHIES_H

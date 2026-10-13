#ifndef GUARD_RENTAL_TEAMS_H
#define GUARD_RENTAL_TEAMS_H

#include "global.h"

#define TOTAL_RENTAL_TEAMS 87

void OpenRentalTeamBrowser(void);
void ChooseRandomRentalTeam(void);
void GiveSelectedRentalTeam(void);
void BufferRentalTeamName(void);
u16 GetRentalTeamMonSpecies(u16 teamId, u8 monIndex);

#endif // GUARD_RENTAL_TEAMS_H

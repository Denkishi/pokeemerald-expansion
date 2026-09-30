import re

with open('src/rental_teams.c', 'r') as f:
    text = f.read()

old_buf = '''            gStringVar1[i] = sRentalTeams[teamId].name[i];
            if (gStringVar1[i] == EOS)
                break;'''

new_buf = '''            gStringVar1[i] = sRentalTeams[teamId].name[i];
            if (gStringVar1[i] == EOS || gStringVar1[i] == 0x00)
                break;'''

text = text.replace(old_buf, new_buf)

with open('src/rental_teams.c', 'w') as f:
    f.write(text)

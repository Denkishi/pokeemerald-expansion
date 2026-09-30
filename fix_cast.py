import re

with open('src/rental_teams.c', 'r') as f:
    text = f.read()

text = text.replace('(const u8 *)sArrowCursor_Gfx', 'sArrowCursor_Gfx')

with open('src/rental_teams.c', 'w') as f:
    f.write(text)

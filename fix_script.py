import re

with open('data/maps/LittlerootTown/scripts.inc', 'r', encoding='utf-8') as f:
    text = f.read()

old_script = '''LittlerootTown_EventScript_CustomTeamHold::
	closemessage
	special OpenCustomTeamBuilder
	setflag FLAG_SYS_POKEMON_GET'''

new_script = '''LittlerootTown_EventScript_CustomTeamHold::
	closemessage
	special OpenCustomTeamBuilder
	waitstate
	setflag FLAG_SYS_POKEMON_GET'''

text = text.replace(old_script, new_script)

with open('data/maps/LittlerootTown/scripts.inc', 'w', encoding='utf-8') as f:
    f.write(text)

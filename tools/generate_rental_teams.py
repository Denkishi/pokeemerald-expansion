import json, re, glob, os

species_set = set()
with open('include/constants/species.h', encoding='utf-8') as f:
    for line in f:
        for ident in re.findall(r'\b(SPECIES_[A-Z0-9_]+)\b', line):
            species_set.add(ident)

item_set = set()
with open('include/constants/items.h', encoding='utf-8') as f:
    for line in f:
        for ident in re.findall(r'\b(ITEM_[A-Z0-9_]+)\b', line):
            item_set.add(ident)

move_set = set()
with open('include/constants/moves.h', encoding='utf-8') as f:
    for line in f:
        for ident in re.findall(r'\b(MOVE_[A-Z0-9_]+)\b', line):
            move_set.add(ident)

ability_set = set()
with open('include/constants/abilities.h', encoding='utf-8') as f:
    for line in f:
        for ident in re.findall(r'\b(ABILITY_[A-Z0-9_]+)\b', line):
            ability_set.add(ident)

type_set = set()
with open('include/constants/pokemon.h', encoding='utf-8') as f:
    for line in f:
        for ident in re.findall(r'\b(TYPE_[A-Z0-9_]+)\b', line):
            type_set.add(ident)

# Read species abilities
species_abilities = {}
for fpath in glob.glob('src/data/pokemon/species_info/*.h'):
    with open(fpath, encoding='utf-8') as f:
        content = f.read()
    entries = re.findall(r'\[(SPECIES_[A-Z0-9_]+)\]\s*=\s*\{([^}]+?\.abilities\s*=\s*\{[^}]+?\}[^}]*?)\}', content, re.DOTALL)
    for sp, body in entries:
        m = re.search(r'\.abilities\s*=\s*\{\s*([A-Z0-9_]+)(?:\s*,\s*([A-Z0-9_]+))?(?:\s*,\s*([A-Z0-9_]+))?\s*\}', body)
        if m:
            abils = [m.group(1), m.group(2) or 'ABILITY_NONE', m.group(3) or 'ABILITY_NONE']
            species_abilities[sp] = abils

def normalize_name(s):
    s = s.strip()
    s = re.sub(r'[\s\-]+', '_', s)
    s = re.sub(r'[^A-Za-z0-9_]', '', s)
    return s.upper()

def map_species(name):
    name = re.sub(r'\s*\([MF]\)\s*', '', name)
    if '(' in name and ')' in name:
        m = re.search(r'\((.*?)\)', name)
        if m:
            name = m.group(1).strip()
    name = name.strip()
    norm = normalize_name(name)
    cand = f'SPECIES_{norm}'
    if cand in species_set:
        return cand
    
    aliases = {
        'SPECIES_DEOXYS_SPEED': 'SPECIES_DEOXYS_SPEED',
        'SPECIES_ZACIAN_CROWNED': 'SPECIES_ZACIAN_CROWNED_SWORD',
        'SPECIES_ZAMAZENTA_CROWNED': 'SPECIES_ZAMAZENTA_CROWNED_SHIELD',
        'SPECIES_URSHIFU_RAPID_STRIKE': 'SPECIES_URSHIFU_RAPID_STRIKE_STYLE',
        'SPECIES_CALYREX_SHADOW': 'SPECIES_CALYREX_SHADOW_RIDER',
        'SPECIES_CALYREX_ICE': 'SPECIES_CALYREX_ICE_RIDER',
        'SPECIES_NECROZMA_DUSK_MANE': 'SPECIES_NECROZMA_DUSK_MANE',
        'SPECIES_NECROZMA_DAWN_WINGS': 'SPECIES_NECROZMA_DAWN_WINGS',
        'SPECIES_TORNADUS_THERIAN': 'SPECIES_TORNADUS_THERIAN',
        'SPECIES_THUNDURUS_THERIAN': 'SPECIES_THUNDURUS_THERIAN',
        'SPECIES_LANDORUS_THERIAN': 'SPECIES_LANDORUS_THERIAN',
        'SPECIES_ENAMORUS_THERIAN': 'SPECIES_ENAMORUS_THERIAN',
        'SPECIES_OGERPON_WELLSPRING': 'SPECIES_OGERPON_WELLSPRING_MASK',
        'SPECIES_OGERPON_HEARTHFLAME': 'SPECIES_OGERPON_HEARTHFLAME_MASK',
        'SPECIES_OGERPON_CORNERSTONE': 'SPECIES_OGERPON_CORNERSTONE_MASK',
        'SPECIES_BASCULEGION': 'SPECIES_BASCULEGION_MALE',
        'SPECIES_BASCULEGION_F': 'SPECIES_BASCULEGION_FEMALE',
        'SPECIES_MEOWSTIC': 'SPECIES_MEOWSTIC_MALE',
        'SPECIES_MEOWSTIC_F': 'SPECIES_MEOWSTIC_FEMALE',
        'SPECIES_INDEEDEE': 'SPECIES_INDEEDEE_MALE',
        'SPECIES_INDEEDEE_F': 'SPECIES_INDEEDEE_FEMALE',
        'SPECIES_OINKOLOGNE': 'SPECIES_OINKOLOGNE_MALE',
        'SPECIES_OINKOLOGNE_F': 'SPECIES_OINKOLOGNE_FEMALE',
        'SPECIES_PALAFIN_HERO': 'SPECIES_PALAFIN_HERO',
    }
    if cand in aliases and aliases[cand] in species_set:
        return aliases[cand]
    
    if norm.startswith('ARCEUS_'):
        sub = norm.replace('ARCEUS_', '')
        cand2 = f'SPECIES_ARCEUS_{sub}'
        if cand2 in species_set:
            return cand2
    
    for form in ['ALOLA', 'GALAR', 'HISUI', 'PALDEA']:
        if norm.endswith(f'_{form}'):
            base = norm[:-len(form)-1]
            for cand2 in [f'SPECIES_{base}_{form}', f'SPECIES_{base}_{form}N', f'SPECIES_{base}_{form}AN']:
                if cand2 in species_set:
                    return cand2

    if norm.startswith('MEGA_'):
        base = norm[5:]
        for cand2 in [f'SPECIES_{base}_MEGA', f'SPECIES_{base}_MEGA_X', f'SPECIES_{base}_MEGA_Y']:
            if cand2 in species_set:
                return cand2

    print(f'WARNING: Missing species {name}')
    return 'SPECIES_NONE'

def map_item(name):
    if not name:
        return 'ITEM_NONE'
    name = name.strip()
    norm = normalize_name(name)
    cand = f'ITEM_{norm}'
    if cand in item_set:
        return cand
    print(f'WARNING: Missing item {name}')
    return 'ITEM_NONE'

def map_move(name):
    if not name:
        return 'MOVE_NONE'
    name = name.strip()
    if '/' in name:
        name = name.split('/')[0].strip()
    norm = normalize_name(name)
    cand = f'MOVE_{norm}'
    if cand in move_set:
        return cand
    print(f'WARNING: Missing move {name}')
    return 'MOVE_NONE'

def map_ability(name):
    if not name:
        return 'ABILITY_NONE'
    name = name.strip()
    if name == 'As One (Spectrier)':
        return 'ABILITY_AS_ONE_SHADOW_RIDER'
    if name == 'As One (Glastrier)':
        return 'ABILITY_AS_ONE_ICE_RIDER'
    norm = normalize_name(name)
    cand = f'ABILITY_{norm}'
    if cand in ability_set:
        return cand
    print(f'WARNING: Missing ability {name}')
    return 'ABILITY_NONE'

def map_tera(name):
    if not name:
        return 'TYPE_NONE'
    name = name.strip()
    if '/' in name:
        name = name.split('/')[0].strip()
    cand = f'TYPE_{name.upper()}'
    if cand in type_set:
        return cand
    return 'TYPE_NONE'

cache = json.load(open('rental_teams_cache.json', encoding='utf-8'))
meta = json.load(open('rental_teams_meta.json', encoding='utf-8'))

teams = []
for entry in meta:
    slug = entry['slug']
    data = cache.get(slug, {})
    paste = data.get('paste', '')
    title = entry['name'] or data.get('title') or 'Rental Team'
    # Clean title
    title = re.sub(r'[\r\n]+', ' ', title).strip()
    if len(title) > 24:
        title = title[:24]
    
    # Category detection
    cats = entry['categories']
    cat_str = ' / '.join(cats)
    main_cat = 'Altro'
    if 'Anything Goes' in cat_str:
        main_cat = 'Anything Goes'
    elif 'Champions' in cat_str:
        main_cat = 'Champions Meta'
    elif 'Overused' in cat_str:
        main_cat = 'SV Overused'
    elif 'Uber' in cat_str:
        main_cat = 'SV Ubers'
    elif 'Rarely Used' in cat_str:
        main_cat = 'SV Rarely Used'
    
    mon_blocks = [b.strip() for b in re.split(r'\n\s*\n', paste) if b.strip()]
    team_mons = []
    
    for block in mon_blocks:
        lines = [l.strip() for l in block.split('\n') if l.strip()]
        if not lines:
            continue
        first = lines[0]
        if '@' in first:
            sp_name, it_name = first.split('@', 1)
        else:
            sp_name, it_name = first, ''
        
        species = map_species(sp_name)
        item = map_item(it_name)
        level = 100
        nature = 'NATURE_HARDY'
        ability = 'ABILITY_NONE'
        tera = 'TYPE_NONE'
        shiny = False
        moves = []
        evs = {'hp': 0, 'atk': 0, 'def': 0, 'spe': 0, 'spa': 0, 'spd': 0}
        ivs = {'hp': 31, 'atk': 31, 'def': 31, 'spe': 31, 'spa': 31, 'spd': 31}
        
        for l in lines[1:]:
            if l.startswith('Level:'):
                try:
                    level = int(l.split(':', 1)[1].strip())
                except Exception:
                    pass
            elif l.startswith('Shiny:'):
                shiny = (l.split(':', 1)[1].strip().lower() == 'yes')
            elif l.startswith('Ability:'):
                ability = map_ability(l.split(':', 1)[1])
            elif l.startswith('Tera Type:'):
                tera = map_tera(l.split(':', 1)[1])
            elif 'Nature' in l:
                nat = l.split()[0].upper()
                nature = f'NATURE_{nat}'
            elif l.startswith('EVs:'):
                ev_str = l.split(':', 1)[1]
                for part in ev_str.split('/'):
                    p = part.strip().split()
                    if len(p) == 2:
                        val, stat = int(p[0]), p[1].lower()
                        if stat in evs:
                            evs[stat] = val
            elif l.startswith('IVs:'):
                iv_str = l.split(':', 1)[1]
                for part in iv_str.split('/'):
                    p = part.strip().split()
                    if len(p) == 2:
                        val, stat = int(p[0]), p[1].lower()
                        if stat in ivs:
                            ivs[stat] = val
            elif l.startswith('-'):
                mv = l[1:].strip()
                moves.append(map_move(mv))
        
        while len(moves) < 4:
            moves.append('MOVE_NONE')
        moves = moves[:4]
        
        # Calculate ability slot (0, 1, or 2)
        abils = species_abilities.get(species, ['ABILITY_NONE', 'ABILITY_NONE', 'ABILITY_NONE'])
        ability_num = 0
        if ability == abils[0]:
            ability_num = 0
        elif ability == abils[1]:
            ability_num = 1
        elif ability == abils[2]:
            ability_num = 2
        
        team_mons.append({
            'species': species,
            'item': item,
            'level': level,
            'nature': nature,
            'ability_num': ability_num,
            'tera': tera,
            'shiny': shiny,
            'evs': evs,
            'ivs': ivs,
            'moves': moves
        })
    
    # Pad to 6 if needed
    while len(team_mons) < 6:
        team_mons.append({
            'species': 'SPECIES_NONE',
            'item': 'ITEM_NONE',
            'level': 100,
            'nature': 'NATURE_HARDY',
            'ability_num': 0,
            'tera': 'TYPE_NONE',
            'shiny': False,
            'evs': {'hp': 0, 'atk': 0, 'def': 0, 'spe': 0, 'spa': 0, 'spd': 0},
            'ivs': {'hp': 31, 'atk': 31, 'def': 31, 'spe': 31, 'spa': 31, 'spd': 31},
            'moves': ['MOVE_NONE', 'MOVE_NONE', 'MOVE_NONE', 'MOVE_NONE']
        })
    team_mons = team_mons[:6]
    
    # Truncate title for full name if needed
    if len(title) > 46:
        title = title[:46]
    
    # Generate list name (max 21 chars)
    subs = [
        ('Life Orb Outrage Dragonite + Hazards Deoxys-S HO', 'LO Dnite + Deo-S HO'),
        ('Hawlucha + Gholdengo Grassy Terrain', 'Hawlucha+Gholdengo'),
        ('Life Orb Ceruledge Screens', 'LO Ceruledge Screens'),
        ('Offensive Cresselia Screens', 'Off. Cress Screens'),
        ('MEGA DNITE MR GOTHAM HAZARD STACK', 'MEGA DNITE HAZARDS'),
        ('MEGA POD + SIRFETCH\'D HEAT', 'MEGA POD+SIRFETCH\'D'),
        ('MEGA ABSOL SEMISTALL', 'MEGA ABSOL SEMISTALL'),
        ('DELPHOX BO MC HAMMER EDIT', 'DELPHOX BO MC HAMMER'),
        ('Dragapult + Wellspring BO', 'Dragapult+Wellspring'),
        ('Assault Vest Tornadus-T + Specs Kyurem BO', 'AV Torn-T+Kyurem BO'),
        ('3 Attacks Nasty Plot Gholdengo BO', '3 Attacks NP Ghold.'),
        ('Specs Kyurem + Cinderace BO', 'Specs Kyurem+Cinder.'),
        ('Walking Wake + Samurott-H BO', 'Wake + Samurott-H BO'),
        ('Garganacl + Iron Treads BO', 'Garganacl+Treads BO'),
        ('Choice Specs Darkrai + Ogerpon Balance', 'Specs Darkrai+Ogerpon'),
        ('Dragonite + Darkrai Hazard Stack', 'Dnite+Darkrai Hazard'),
        ('Dragonite + Heatran Anti-Offense Balance', 'Dnite+Heatran Balance'),
        ('Samurott + Blissey + Ogerpon Fat Balance', 'Samu+Blissey+Ogerpon'),
        ('Substitute Kyurem + PhysDef Dondozo Balance', 'Sub Kyurem+Dozo Bal.'),
        ('Swords Dance Gliscor Removal Stall', 'SD Gliscor Stall'),
        ('Talonflame + Mandibuzz Removal Stall', 'Talon+Mandibuzz Stal'),
        ('Clodsire + Fast Gliscor Stall', 'Clodsire+Gliscor Stal'),
        ('Double Water Lu Hatt BO', 'Double Water Lu Hatt'),
        ('Lead Groundceus Offense', 'Lead Groundceus Off.'),
        ('Vanilla Yanmega Hyper Offense', 'Yanmega HO'),
        ('Mixed EQ Venusaur Sun HO', 'Mixed EQ Venusaur HO'),
        ('Specs Toxtricity Hazardstack', 'Specs Toxtricity Haz'),
        ('Banded Krookodile & Trailblaze Entei', 'Band Krook + Entei'),
        ('Banded Golurk & Calm Mind Cresselia', 'Band Golurk+Cress.'),
        ('DD Covert Cloak Necrozma Fat', 'DD Cloak Necrozma'),
        ('Sash Bramble + Meteor Beam Necro', 'Bramble + Necro'),
        ('Phys defense Umb+Deo-D Stall', 'Def Umb+Deo-D Stall'),
        ('Mega Dragalge Balance', 'Mega Dragalge Bal.'),
        ('Mega Altaria Semi Stall', 'Mega Altaria Stall'),
        ('Charizard X Balance', 'Charizard X Balance'),
        ('Band Zamazenta Balance', 'Band Zamazenta Bal.'),
        ('Kyogre Groudon Balance', 'Kyogre+Groudon Bal.'),
        ('Ghostceus Kyogre Webs', 'Ghostceus+Kyogre Web'),
        ('Haze Alolan Muk Stall', 'Haze A-Muk Stall'),
    ]
    sub_map = dict(subs)
    list_name = sub_map.get(title, title)[:21]

    teams.append({
        'title': title,
        'list_name': list_name,
        'category': main_cat,
        'mons': team_mons
    })

print(f'Successfully built {len(teams)} teams!')

# Group team indices by category
categories = ['Anything Goes', 'Champions Meta', 'SV Overused', 'SV Ubers', 'SV Rarely Used']
cat_indices = {c: [] for c in categories}
for idx, t in enumerate(teams):
    if t['category'] in cat_indices:
        cat_indices[t['category']].append(idx)
    else:
        print(f"Unknown category {t['category']} for team {idx}")

# Generate src/data/rental_teams.h
out = []
out.append('// Auto-generated rental teams data')
out.append('#ifndef GUARD_RENTAL_TEAMS_DATA_H')
out.append('#define GUARD_RENTAL_TEAMS_DATA_H')
out.append('')
out.append('#include "global.h"')
out.append('#include "constants/species.h"')
out.append('#include "constants/items.h"')
out.append('#include "constants/moves.h"')
out.append('#include "constants/abilities.h"')
out.append('#include "constants/pokemon.h"')
out.append('')
out.append('struct RentalMon')
out.append('{')
out.append('    u16 species;')
out.append('    u16 item;')
out.append('    u8 level;')
out.append('    u8 nature;')
out.append('    u8 abilityNum;')
out.append('    u8 teraType;')
out.append('    bool8 isShiny;')
out.append('    u8 evs[NUM_STATS]; // HP, ATK, DEF, SPEED, SPATK, SPDEF')
out.append('    u8 ivs[NUM_STATS];')
out.append('    u16 moves[4];')
out.append('};')
out.append('')
out.append('struct RentalTeam')
out.append('{')
out.append('    const u8 name[48];')
out.append('    const u8 listName[24];')
out.append('    const u8 category[24];')
out.append('    struct RentalMon mons[6];')
out.append('};')
out.append('')
out.append(f'#define TOTAL_RENTAL_TEAMS {len(teams)}')
out.append('#define NUM_RENTAL_CATEGORIES 5')
out.append('#define CATEGORY_ANYTHING_GOES 0')
out.append('#define CATEGORY_CHAMPIONS_META 1')
out.append('#define CATEGORY_SV_OVERUSED 2')
out.append('#define CATEGORY_SV_UBERS 3')
out.append('#define CATEGORY_SV_RARELY_USED 4')
out.append('#define CATEGORY_ALL 5')
out.append('')
out.append('struct RentalCategoryInfo')
out.append('{')
out.append('    const u8 name[24];')
out.append('    const u16 *teamIndices;')
out.append('    u16 count;')
out.append('};')
out.append('')
out.append('static const struct RentalTeam sRentalTeams[TOTAL_RENTAL_TEAMS] =')
out.append('{')

for t_idx, team in enumerate(teams):
    t_name = team['title'].replace('\\', '\\\\').replace('"', '\\"')
    t_short = team['list_name'].replace('\\', '\\\\').replace('"', '\\"')
    t_cat = team['category'].replace('\\', '\\\\').replace('"', '\\"')
    out.append('    {')
    out.append(f'        .name = _("{t_name}"),')
    out.append(f'        .listName = _("{t_short}"),')
    out.append(f'        .category = _("{t_cat}"),')
    out.append('        .mons = {')
    for m in team['mons']:
        ev_arr = f"{m['evs']['hp']}, {m['evs']['atk']}, {m['evs']['def']}, {m['evs']['spe']}, {m['evs']['spa']}, {m['evs']['spd']}"
        iv_arr = f"{m['ivs']['hp']}, {m['ivs']['atk']}, {m['ivs']['def']}, {m['ivs']['spe']}, {m['ivs']['spa']}, {m['ivs']['spd']}"
        mv_arr = f"{m['moves'][0]}, {m['moves'][1]}, {m['moves'][2]}, {m['moves'][3]}"
        sh_val = 'TRUE' if m['shiny'] else 'FALSE'
        out.append('            {')
        out.append(f"                .species = {m['species']},")
        out.append(f"                .item = {m['item']},")
        out.append(f"                .level = {m['level']},")
        out.append(f"                .nature = {m['nature']},")
        out.append(f"                .abilityNum = {m['ability_num']},")
        out.append(f"                .teraType = {m['tera']},")
        out.append(f"                .isShiny = {sh_val},")
        out.append(f"                .evs = {{{ev_arr}}},")
        out.append(f"                .ivs = {{{iv_arr}}},")
        out.append(f"                .moves = {{{mv_arr}}},")
        out.append('            },')
    out.append('        }')
    out.append('    },')

out.append('};')
out.append('')

# Generate category index arrays
cat_var_names = [
    'sCategoryTeams_AnythingGoes',
    'sCategoryTeams_ChampionsMeta',
    'sCategoryTeams_SVOverused',
    'sCategoryTeams_SVUbers',
    'sCategoryTeams_SVRarelyUsed',
]

for c_name, var_name in zip(categories, cat_var_names):
    indices_str = ', '.join(str(x) for x in cat_indices[c_name])
    out.append(f'static const u16 {var_name}[{len(cat_indices[c_name])}] = {{ {indices_str} }};')

out.append('')
out.append('static const struct RentalCategoryInfo sRentalCategories[NUM_RENTAL_CATEGORIES] =')
out.append('{')
for c_name, var_name in zip(categories, cat_var_names):
    out.append('    {')
    out.append(f'        .name = _("{c_name}"),')
    out.append(f'        .teamIndices = {var_name},')
    out.append(f'        .count = {len(cat_indices[c_name])},')
    out.append('    },')
out.append('};')
out.append('')
out.append('#endif // GUARD_RENTAL_TEAMS_DATA_H')
out.append('')

with open('src/data/rental_teams.h', 'w', encoding='utf-8') as f:
    f.write('\n'.join(out))

print('Wrote src/data/rental_teams.h successfully!')

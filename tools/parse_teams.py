import json, re

def load_enum_identifiers(filepath, prefix):
    identifiers = set()
    with open(filepath, encoding='utf-8') as f:
        for line in f:
            # Matches identifiers like SPECIES_..., ITEM_..., MOVE_..., ABILITY_...
            m = re.findall(rf'\b({prefix}[A-Z0-9_]+)\b', line)
            for ident in m:
                identifiers.add(ident)
    return identifiers

species_set = load_enum_identifiers('include/constants/species.h', 'SPECIES_')
item_set = load_enum_identifiers('include/constants/items.h', 'ITEM_')
move_set = load_enum_identifiers('include/constants/moves.h', 'MOVE_')
ability_set = load_enum_identifiers('include/constants/abilities.h', 'ABILITY_')
type_set = load_enum_identifiers('include/constants/pokemon.h', 'TYPE_')

print(f'Loaded: {len(species_set)} species, {len(item_set)} items, {len(move_set)} moves, {len(ability_set)} abilities, {len(type_set)} types.')

cache = json.load(open('rental_teams_cache.json', encoding='utf-8'))
meta = json.load(open('rental_teams_meta.json', encoding='utf-8'))

def normalize_name(s):
    # Remove special chars, spaces, hyphens to uppercase with underscore
    s = s.strip()
    s = re.sub(r'[\s\-]+', '_', s)
    s = re.sub(r'[^A-Za-z0-9_]', '', s)
    return s.upper()

missing_species = set()
missing_items = set()
missing_moves = set()
missing_abilities = set()
missing_types = set()

def map_species(name):
    # Strip (M) / (F) or nickname
    name = re.sub(r'\s*\([MF]\)\s*', '', name)
    if '(' in name and ')' in name:
        # Nickname (Real Species)
        m = re.search(r'\((.*?)\)', name)
        if m:
            name = m.group(1).strip()
    
    # Common Showdown name mappings
    name = name.strip()
    norm = normalize_name(name)
    cand = f'SPECIES_{norm}'
    if cand in species_set:
        return cand
    
    # Specific exceptions
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
    
    # Try Arceus / Silvally forms
    if norm.startswith('ARCEUS_'):
        sub = norm.replace('ARCEUS_', '')
        cand2 = f'SPECIES_ARCEUS_{sub}'
        if cand2 in species_set:
            return cand2
    
    # Try Alola / Galar / Hisui / Paldea forms
    # Showdown: Muk-Alola -> SPECIES_MUK_ALOLA or SPECIES_MUK_ALOLAN
    for form in ['ALOLA', 'GALAR', 'HISUI', 'PALDEA']:
        if norm.endswith(f'_{form}'):
            base = norm[:-len(form)-1]
            for cand2 in [f'SPECIES_{base}_{form}', f'SPECIES_{base}_{form}N', f'SPECIES_{base}_{form}AN']:
                if cand2 in species_set:
                    return cand2

    # Mega forms
    if norm.startswith('MEGA_'):
        base = norm[5:]
        for cand2 in [f'SPECIES_{base}_MEGA', f'SPECIES_{base}_MEGA_X', f'SPECIES_{base}_MEGA_Y']:
            if cand2 in species_set:
                return cand2

    missing_species.add(name)
    return 'SPECIES_NONE'

def map_item(name):
    if not name:
        return 'ITEM_NONE'
    name = name.strip()
    norm = normalize_name(name)
    cand = f'ITEM_{norm}'
    if cand in item_set:
        return cand
    missing_items.add(name)
    return 'ITEM_NONE'

def map_move(name):
    if not name:
        return 'MOVE_NONE'
    name = name.strip()
    norm = normalize_name(name)
    cand = f'MOVE_{norm}'
    if cand in move_set:
        return cand
    missing_moves.add(name)
    return 'MOVE_NONE'

def map_ability(name):
    if not name:
        return 'ABILITY_NONE'
    name = name.strip()
    norm = normalize_name(name)
    cand = f'ABILITY_{norm}'
    if cand in ability_set:
        return cand
    missing_abilities.add(name)
    return 'ABILITY_NONE'

parsed_teams = []
for entry in meta:
    slug = entry['slug']
    data = cache.get(slug, {})
    paste = data.get('paste', '')
    title = entry['name'] or data.get('title') or 'Rental Team'
    
    # Parse pokemon from paste
    mon_blocks = [b.strip() for b in re.split(r'\n\s*\n', paste) if b.strip()]
    team_mons = []
    
    for block in mon_blocks:
        lines = [l.strip() for l in block.split('\n') if l.strip()]
        if not lines:
            continue
        first = lines[0]
        # Format: Name @ Item
        if '@' in first:
            sp_name, it_name = first.split('@', 1)
        else:
            sp_name, it_name = first, ''
        
        species = map_species(sp_name)
        item = map_item(it_name)
        
        ability = 'ABILITY_NONE'
        tera = 'TYPE_NONE'
        nature = 'NATURE_HARDY'
        moves = []
        evs = {'hp': 0, 'atk': 0, 'def': 0, 'spe': 0, 'spa': 0, 'spd': 0}
        ivs = {'hp': 31, 'atk': 31, 'def': 31, 'spe': 31, 'spa': 31, 'spd': 31}
        
        for l in lines[1:]:
            if l.startswith('Ability:'):
                ability = map_ability(l.split(':', 1)[1])
            elif l.startswith('Tera Type:'):
                t_str = l.split(':', 1)[1].strip().upper()
                cand_t = f'TYPE_{t_str}'
                if cand_t in type_set:
                    tera = cand_t
                elif cand_t == 'TYPE_STEEL':
                    tera = 'TYPE_STEEL'
                else:
                    missing_types.add(t_str)
            elif 'Nature' in l:
                nat_name = l.split()[0].upper()
                nature = f'NATURE_{nat_name}'
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
        
        team_mons.append({
            'raw_name': sp_name.strip(),
            'species': species,
            'item': item,
            'ability': ability,
            'nature': nature,
            'tera': tera,
            'evs': evs,
            'ivs': ivs,
            'moves': moves
        })
    
    parsed_teams.append({
        'title': title,
        'categories': entry['categories'],
        'slug': slug,
        'pokemon': team_mons
    })

print(f'Parsed {len(parsed_teams)} teams.')
print(f'Missing species: {missing_species}')
print(f'Missing items: {missing_items}')
print(f'Missing moves: {missing_moves}')
print(f'Missing abilities: {missing_abilities}')
print(f'Missing types: {missing_types}')

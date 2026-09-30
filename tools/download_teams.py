import urllib.request, ssl, re, json, time, os

ctx = ssl._create_unverified_context()

with open(r'C:\Users\Vincenzo\Desktop\Teams.txt', encoding='utf-8') as f:
    lines = f.readlines()

parsed_entries = []
categories = []
last_header = ''

for line in lines:
    line_s = line.strip()
    if not line_s:
        continue
    if line_s.endswith('[') and not 'pokepast' in line_s:
        cat_name = line_s[:-1].strip()
        categories.append(cat_name)
        last_header = ''
    elif line_s == ']':
        if categories:
            categories.pop()
        last_header = ''
    elif 'pokepast.es' in line_s:
        m = re.search(r'https?://pokepast\.es/([0-9a-f]+)', line_s)
        if m:
            slug = m.group(1)
            name = ''
            if '-' in line_s:
                name = line_s.split('-', 1)[1].strip()
            elif ':' in line_s:
                parts = line_s.split(':', 1)
                if not parts[0].startswith('http'):
                    name = parts[0].strip()
            if not name and last_header:
                name = last_header
            parsed_entries.append({
                'slug': slug,
                'url': f'https://pokepast.es/{slug}',
                'name': name,
                'categories': list(categories)
            })
            last_header = ''
    else:
        last_header = line_s

print(f'Total parsed entries: {len(parsed_entries)}')

cache_file = 'rental_teams_cache.json'
cache = {}
if os.path.exists(cache_file):
    try:
        with open(cache_file, 'r', encoding='utf-8') as f:
            cache = json.load(f)
    except Exception:
        cache = {}

success_count = 0
for idx, entry in enumerate(parsed_entries):
    slug = entry['slug']
    if slug in cache:
        success_count += 1
        continue
    url = f'https://pokepast.es/{slug}/json'
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    try:
        with urllib.request.urlopen(req, context=ctx, timeout=8) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            cache[slug] = data
            success_count += 1
            entry_name = entry['name'] or data.get('title')
            print(f'[{idx+1}/{len(parsed_entries)}] Downloaded {slug}: {entry_name}')
            time.sleep(0.05)
    except Exception as e:
        print(f'[{idx+1}/{len(parsed_entries)}] Failed {slug}: {e}')

with open(cache_file, 'w', encoding='utf-8') as f:
    json.dump(cache, f, indent=2, ensure_ascii=False)

# Save parsed structure metadata
meta_file = 'rental_teams_meta.json'
with open(meta_file, 'w', encoding='utf-8') as f:
    json.dump(parsed_entries, f, indent=2, ensure_ascii=False)

print(f'Done! Successfully cached {success_count}/{len(parsed_entries)} teams.')

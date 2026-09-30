import re

with open('src/custom_team_builder.c', 'r') as f:
    text = f.read()

bad_str = '''            buf[StringLength(buf)] = 0xBB + (sData->filterLetter - 1);
            buf[StringLength(buf)] = EOS;'''

good_str = '''            u16 len = StringLength(buf);
            buf[len] = 0xBB + (sData->filterLetter - 1);
            buf[len + 1] = EOS;'''

text = text.replace(bad_str, good_str)

with open('src/custom_team_builder.c', 'w') as f:
    f.write(text)

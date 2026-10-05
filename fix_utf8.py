with open('main/tamagotchi_engine.cc', 'rb') as fp:
    raw = fp.read()

# Replace any byte >= 0x80 in that specific line with ASCII
import re

pattern = re.compile(rb'(\s*prompt \+= " \[TIMER ATIVO\]:[^\r\n]+)')
m = pattern.search(raw)
if m:
    old_line = m.group(1)
    clean_line = b'        prompt += " [TIMER ATIVO]: Voce esta contando um timer chamado \'" + timer_label_ + "\' e faltam " + std::to_string(rem / 60) + " minutos e " + std::to_string(rem % 60) + " segundos para acabar. Se o usuario perguntar quanto tempo falta, informe baseado nesses dados. ";'
    raw = raw.replace(old_line, clean_line)
    print("Replaced successfully")

with open('main/tamagotchi_engine.cc', 'wb') as fp:
    fp.write(raw)

# Now test UTF-8 decoding
with open('main/tamagotchi_engine.cc', 'rb') as fp:
    content = fp.read()

content.decode('utf-8')
print("File decoded with utf-8 successfully!")

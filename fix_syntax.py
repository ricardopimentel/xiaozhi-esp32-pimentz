import re

with open('main/tamagotchi_engine.cc', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Fix the array address warning
content = content.replace('if (sensor_rfid_lido_ && sensor_rfid_uid_) {', 'if (sensor_rfid_lido_) {')

# 2. Fix the variable name 	emperatura inside Update()
content = content.replace('if (temperatura < 18.0f', 'if (sensor_temperatura_ < 18.0f')
content = content.replace('if (temperatura > 30.0f', 'if (sensor_temperatura_ > 30.0f')

# 3. Clean up the dangling code from the regex failure
# The dangling code looks like:
#    fome_ = fome;
#    diversao_ = diversao;
#    saude_ = saude;
# }
dangling_pattern = r'^\s*fome_ = fome;\s*diversao_ = diversao;\s*saude_ = saude;\s*}\s*'
content = re.sub(dangling_pattern, '', content, flags=re.MULTILINE)

with open('main/tamagotchi_engine.cc', 'w', encoding='utf-8') as f:
    f.write(content)

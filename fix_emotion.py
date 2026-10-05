import re

with open('main/tamagotchi_engine.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_cb = '''    if (fome_ > 75) {
        return "crying";
    }
    if (fome_ > 50) {
        return "sad";
    }'''

new_cb = '''    if (fome_ < 25 || diversao_ < 25 || saude_ < 25) {
        return "crying";
    }
    if (fome_ < 50 || diversao_ < 50 || saude_ < 50) {
        return "sad";
    }'''

content = content.replace(old_cb, new_cb)

with open('main/tamagotchi_engine.cc', 'w', encoding='utf-8') as f:
    f.write(content)

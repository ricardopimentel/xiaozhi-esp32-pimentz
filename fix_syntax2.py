with open('main/tamagotchi_engine.cc', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('&& temperatura > 0.0f', '&& sensor_temperatura_ > 0.0f')

with open('main/tamagotchi_engine.cc', 'w', encoding='utf-8') as f:
    f.write(content)

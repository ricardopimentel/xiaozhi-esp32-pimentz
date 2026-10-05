import re

with open('main/esp_now_receiver.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_cb = '''        // Garantia absoluta de terminação nula para evitar estouro de memória no std::string (Watchdog/OOM)
        state.fala[sizeof(state.fala) - 1] = ' ';'''

new_cb = '''        // Garantia absoluta de terminação nula para evitar estouro de memória no std::string (Watchdog/OOM)
        state.fala[sizeof(state.fala) - 1] = '\\0';'''

content = content.replace(old_cb, new_cb)

with open('main/esp_now_receiver.cc', 'w', encoding='utf-8') as f:
    f.write(content)

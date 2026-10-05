import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_cb = '''    bool connected = Board::GetInstance().GetNetwork()->IsConnected();'''
new_cb = '''    auto state = Application::GetInstance().GetDeviceState();
    bool connected = (state != kDeviceStateWifiConfiguring && state != kDeviceStateStarting);'''

content = content.replace(old_cb, new_cb)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

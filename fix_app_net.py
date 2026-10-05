import re

with open('main/application.cc', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace("!Board::GetInstance().GetNetwork()->IsConnected()", "GetDeviceState() == kDeviceStateWifiConfiguring || GetDeviceState() == kDeviceStateStarting")

with open('main/application.cc', 'w', encoding='utf-8') as f:
    f.write(content)

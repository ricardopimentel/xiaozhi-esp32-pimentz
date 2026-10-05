import re

with open('main/display/lcd_display.h', 'r', encoding='utf-8') as f:
    h_content = f.read()

h_content = h_content.replace('void DrawLargeHeart(int x, int y, bool small);', 'void DrawLargeHeart(int x, int y, bool small, lv_layer_t* layer);')

with open('main/display/lcd_display.h', 'w', encoding='utf-8') as f:
    f.write(h_content)


with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    c_content = f.read()

c_content = c_content.replace('void LcdDisplay::DrawLargeHeart(int x, int y, bool small) {', 'void LcdDisplay::DrawLargeHeart(int x, int y, bool small, lv_layer_t* layer) {')
c_content = c_content.replace('DrawLargeHeart(xOffset + 64, 32, true);', 'DrawLargeHeart(xOffset + 64, 32, true, layer);')
c_content = c_content.replace('DrawLargeHeart(xOffset + 64, 32, false);', 'DrawLargeHeart(xOffset + 64, 32, false, layer);')

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(c_content)

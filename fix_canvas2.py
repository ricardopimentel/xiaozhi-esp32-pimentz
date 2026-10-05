import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace(
    'draw_canvas_rect_empty(face_canvas_, (startX - 4)*2, (drawY - 3)*2, (totalW + 8)*2, 14*2, lv_color_hex(0xFFFFFF), 2*2, lv_color_hex(0xFFFFFF));',
    'draw_canvas_rect_empty(face_canvas_, (startX - 4)*2, (drawY - 3)*2, (totalW + 8)*2, 14*2, lv_color_hex(0xFFFFFF), 2*2, 0);'
)

content = content.replace(
    'draw_canvas_rect_empty(face_canvas_, currentX*2, (drawY - 1)*2, 12*2, 7*2, lv_color_hex(0x00FF00), 2, lv_color_hex(0x00FF00));',
    'draw_canvas_rect_empty(face_canvas_, currentX*2, (drawY - 1)*2, 12*2, 7*2, lv_color_hex(0x00FF00), 2, 0);'
)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

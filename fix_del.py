with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('lv_obj_del(face_canvas_);', '// lv_obj_del(face_canvas_); // Comentado para evitar panico no LVGL')

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

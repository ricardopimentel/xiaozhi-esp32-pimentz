import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_cb = '''    if (estado != ESTADO_NASCIDO) {
        // Se ainda não nasceu, esconde o emoji_box_ e o rosto dinâmico
        if (emoji_box_ != nullptr) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        if (face_canvas_ != nullptr) lv_obj_add_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);
        
        // Exibe o ovo e a barra de progresso
        if (egg_obj_ != nullptr) {'''

new_cb = '''    bool connected = Board::GetInstance().GetNetwork()->IsConnected();
    if (estado != ESTADO_NASCIDO && connected) {
        // Se ainda não nasceu, esconde o emoji_box_ e o rosto dinâmico
        if (emoji_box_ != nullptr) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        if (face_canvas_ != nullptr) lv_obj_add_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);
        
        // Exibe o ovo e a barra de progresso
        if (egg_obj_ != nullptr) {'''

content = content.replace(old_cb, new_cb)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

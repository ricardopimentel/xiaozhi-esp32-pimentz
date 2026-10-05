import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_cb = '''void LcdDisplay::UpdateStatusBar(bool update_all) {
    LvglDisplay::UpdateStatusBar(update_all);
    
    // Atualiza o ovo e o progresso
    auto& engine = TamagotchiEngine::GetInstance();'''

new_cb = '''void LcdDisplay::UpdateStatusBar(bool update_all) {
    LvglDisplay::UpdateStatusBar(update_all);
    
    DisplayLockGuard lock(this); // Proteção ABSOLUTA do ovo na LVGL
    
    // Atualiza o ovo e o progresso
    auto& engine = TamagotchiEngine::GetInstance();'''

content = content.replace(old_cb, new_cb)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

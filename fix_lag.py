import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Corrigir SetEmotion para evitar recarregamento
old_func_emotion = '''void LcdDisplay::SetEmotion(const char* emotion) {
    if (!setup_ui_called_) {'''

new_func_emotion = '''void LcdDisplay::SetEmotion(const char* emotion) {
    static std::string last_emotion = "";
    if (emotion && last_emotion == emotion) {
        return; // Ignora se a emoção não mudou (evita vazamento de memória e travamento da CPU recarregando o GIF a 5 FPS via ESP-NOW)
    }
    if (emotion) last_emotion = emotion;
    
    if (!setup_ui_called_) {'''

content = content.replace(old_func_emotion, new_func_emotion)

# 2. Corrigir SetChatMessage para evitar recarregamento repetido
old_func_chat = '''void LcdDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);'''

new_func_chat = '''void LcdDisplay::SetChatMessage(const char* role, const char* content) {
    static std::string last_role = "";
    static std::string last_content = "";
    if (role && content && last_role == role && last_content == content) {
        return; // Ignora se a mensagem não mudou
    }
    if (role) last_role = role;
    if (content) last_content = content;

    DisplayLockGuard lock(this);'''

content = content.replace(old_func_chat, new_func_chat)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

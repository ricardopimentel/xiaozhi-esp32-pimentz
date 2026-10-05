import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

fallback = '''
    if (!face_canvas_buf_) {
        ESP_LOGW(TAG, "Falha ao alocar Canvas na SPIRAM. Tentando RAM interna...");
        face_canvas_buf_ = (uint8_t*)heap_caps_malloc(canvas_size, MALLOC_CAP_DEFAULT);
    }
'''

content = content.replace(fallback, '')

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

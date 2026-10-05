with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

allocation_code = '''
    face_canvas_buf_ = (uint8_t*)heap_caps_malloc(canvas_size, MALLOC_CAP_SPIRAM);
    if (!face_canvas_buf_) {
        ESP_LOGW(TAG, "Falha ao alocar Canvas na SPIRAM. Tentando RAM interna...");
        face_canvas_buf_ = (uint8_t*)heap_caps_malloc(canvas_size, MALLOC_CAP_DEFAULT);
    }
    
    if (face_canvas_buf_) {
        lv_canvas_set_buffer(face_canvas_, face_canvas_buf_, 256, 128, LV_COLOR_FORMAT_NATIVE);
        lv_obj_align(face_canvas_, LV_ALIGN_CENTER, 0, -10);
        lv_obj_add_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN); // Oculto por padrao ate nascer
    } else {
        ESP_LOGE(TAG, "Falha CRITICA ao alocar buffer para o Canvas do rosto! Desativando rosto.");
        lv_obj_del(face_canvas_);
        face_canvas_ = nullptr;
    }
'''

# We need to replace the two occurrences of this block in both SetupUI variants
import re
pattern = r'face_canvas_buf_ = \(uint8_t\*\)heap_caps_malloc\(canvas_size, MALLOC_CAP_SPIRAM\);\s*if \(face_canvas_buf_\) \{.*?} else \{\s*ESP_LOGE\(TAG, "Falha ao alocar buffer para o Canvas do rosto!"\);\s*\}'
content = re.sub(pattern, allocation_code.strip(), content, flags=re.DOTALL)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

# Substituir a alocação de face_canvas_
old_alloc = '''    // Criação do Canvas do Rosto OLED (256x128 pixels, escala 2x)
    face_canvas_ = lv_canvas_create(screen);
    size_t canvas_size = 256 * 128 * 4; // Safest size for ARGB8888 or any 32-bit aligned LVGL 9 format
    face_canvas_buf_ = (uint8_t*)heap_caps_malloc(canvas_size, MALLOC_CAP_SPIRAM);    
    if (face_canvas_buf_) {
        lv_canvas_set_buffer(face_canvas_, face_canvas_buf_, 256, 128, LV_COLOR_FORMAT_NATIVE);
        lv_obj_align(face_canvas_, LV_ALIGN_CENTER, 0, -10);
        lv_obj_add_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN); // Oculto por padrao ate nascer
    } else {
        ESP_LOGE(TAG, "Falha CRITICA ao alocar buffer para o Canvas do rosto! Desativando rosto.");
        // lv_obj_del(face_canvas_); // Comentado para evitar panico no LVGL
        face_canvas_ = nullptr;
    }'''

new_alloc = '''    // Criação do Canvas do Rosto OLED (256x128 pixels, escala 2x)
    face_canvas_ = lv_canvas_create(screen);
#if LVGL_VERSION_MAJOR >= 9
    // Em LVGL 9, lv_draw_buf_create cuida de todo o alinhamento e stride perfeitamente
    lv_draw_buf_t* draw_buf = lv_draw_buf_create(256, 128, LV_COLOR_FORMAT_NATIVE, LV_STRIDE_AUTO);
    if (draw_buf) {
        lv_canvas_set_draw_buf(face_canvas_, draw_buf);
        lv_obj_align(face_canvas_, LV_ALIGN_CENTER, 0, -10);
        lv_obj_add_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN); // Oculto por padrao ate nascer
    } else {
        ESP_LOGE(TAG, "Falha CRITICA ao alocar draw_buf para o Canvas! Desativando rosto.");
        face_canvas_ = nullptr;
    }
#else
    // LVGL 8 fallback
    size_t canvas_size = 256 * 128 * 4;
    face_canvas_buf_ = (uint8_t*)heap_caps_malloc(canvas_size, MALLOC_CAP_SPIRAM);    
    if (face_canvas_buf_) {
        // Alinhamento manual de segurança para 64-bytes
        void* aligned_buf = (void*)(((uintptr_t)face_canvas_buf_ + 63) & ~63);
        lv_canvas_set_buffer(face_canvas_, aligned_buf, 256, 128, LV_COLOR_FORMAT_NATIVE);
        lv_obj_align(face_canvas_, LV_ALIGN_CENTER, 0, -10);
        lv_obj_add_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);
    } else {
        ESP_LOGE(TAG, "Falha ao alocar Canvas! Desativando rosto.");
        face_canvas_ = nullptr;
    }
#endif'''

content = content.replace(old_alloc, new_alloc)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

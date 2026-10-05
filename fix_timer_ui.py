
import re

# 1. Update lcd_display.h
h_path = "main/display/lcd_display.h"
with open(h_path, "r", encoding="utf-8") as f:
    h_content = f.read()

if "timer_bar_" not in h_content:
    h_content = h_content.replace("lv_obj_t* timer_label_ = nullptr;", "lv_obj_t* timer_label_ = nullptr;\n    lv_obj_t* timer_bar_ = nullptr;")
    with open(h_path, "w", encoding="utf-8") as f:
        f.write(h_content)
    print("Added timer_bar_ to lcd_display.h")

# 2. Update lcd_display.cc (SetupUI)
cc_path = "main/display/lcd_display.cc"
with open(cc_path, "r", encoding="utf-8") as f:
    cc_content = f.read()

setup_old = """    timer_label_ = lv_label_create(screen);
    lv_obj_set_style_text_font(timer_label_, text_font, 0);
    lv_obj_set_style_text_color(timer_label_, lv_color_hex(0xFF3333), 0);
    lv_obj_align(timer_label_, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(timer_label_, "");"""

setup_new = """    // Cria a barra do timer
    timer_bar_ = lv_bar_create(screen);
    lv_obj_set_size(timer_bar_, 120, 10);
    lv_obj_align(timer_bar_, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_bg_color(timer_bar_, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(timer_bar_, lv_color_hex(0xFF3333), LV_PART_INDICATOR);
    lv_obj_add_flag(timer_bar_, LV_OBJ_FLAG_HIDDEN);

    // Cria o texto do timer logo abaixo da barra
    timer_label_ = lv_label_create(screen);
    lv_obj_set_style_text_font(timer_label_, text_font, 0);
    lv_obj_set_style_text_color(timer_label_, lv_color_hex(0xFF3333), 0);
    lv_obj_align_to(timer_label_, timer_bar_, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_label_set_text(timer_label_, "");
    lv_obj_add_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);"""

cc_content = cc_content.replace(setup_old, setup_new)

# 3. Update lcd_display.cc (UpdateStatusBar)
update_old = """    if (timer_label_ != nullptr) {
        if (engine.IsTimerActive()) {
            uint32_t rem = engine.GetTimerRemainingMs() / 1000;
            uint32_t m = rem / 60;
            uint32_t s = rem % 60;
            char buf[32];
            snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
            lv_label_set_text(timer_label_, buf);
            lv_obj_remove_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }"""

update_new = """    if (timer_label_ != nullptr && timer_bar_ != nullptr) {
        if (engine.IsTimerActive()) {
            uint32_t rem = engine.GetTimerRemainingMs() / 1000;
            uint32_t total = engine.GetTimerDurationMs() / 1000;
            uint32_t m = rem / 60;
            uint32_t s = rem % 60;
            char buf[32];
            snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
            
            lv_label_set_text(timer_label_, buf);
            
            // Atualiza a barra de progresso (0 a 100)
            if (total > 0) {
                int percent = (rem * 100) / total;
                lv_bar_set_value(timer_bar_, percent, LV_ANIM_OFF);
            }
            
            lv_obj_remove_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(timer_bar_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(timer_bar_, LV_OBJ_FLAG_HIDDEN);
        }
    }"""

cc_content = cc_content.replace(update_old, update_new)

with open(cc_path, "w", encoding="utf-8") as f:
    f.write(cc_content)
print("Updated lcd_display.cc UI logic")


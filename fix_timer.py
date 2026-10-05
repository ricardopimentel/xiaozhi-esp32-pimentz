
# -*- coding: utf-8 -*-
import sys

with open("main/display/lcd_display.cc", "r", encoding="utf-8") as f:
    content = f.read()

target = "        if (face_canvas_ != nullptr) lv_obj_remove_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);\n    }\n}"

replacement = """        if (face_canvas_ != nullptr) lv_obj_remove_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);
    }

    if (timer_label_ != nullptr) {
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
    }
}"""

if target in content:
    content = content.replace(target, replacement)
    with open("main/display/lcd_display.cc", "w", encoding="utf-8") as f:
        f.write(content)
    print("Success")
else:
    print("Target not found")


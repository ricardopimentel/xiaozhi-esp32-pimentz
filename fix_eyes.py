import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_cb = '''    } else if (emotion == "sad" || emotion == "crying") {
        DrawEye(eyeLx + xOffset + tremorX, eyeLy + tremorY, eyeLw, eyeLh, eyeRadius, layer);
        DrawEye(eyeRx + xOffset + tremorX, eyeRy + tremorY, eyeRw, eyeRh, eyeRadius, layer);
        draw_canvas_line(layer, (eyeLx - 8 + xOffset)*2, (eyeLy - 10)*2, (eyeLx + 8 + xOffset)*2, (eyeLy - 14)*2, lv_color_hex(0x0080FF), 4);
        draw_canvas_line(layer, (eyeRx - 8 + xOffset)*2, (eyeRy - 14)*2, (eyeRx + 8 + xOffset)*2, (eyeRy - 10)*2, lv_color_hex(0x0080FF), 4);
    } else if (emotion == "loving") {'''

new_cb = '''    } else if (emotion == "sad" || emotion == "crying") {
        static int blink_counter_sad = 0, blink_state_sad = 0;
        static float eye_h_sad = 24;
        blink_counter_sad++;
        if (blink_state_sad == 0 && blink_counter_sad > 100) {
            if ((rand() % 100) < 20) blink_state_sad = 1;
            blink_counter_sad = 0;
        }
        if (blink_state_sad == 1) {
            eye_h_sad -= 4; if (eye_h_sad <= 2) blink_state_sad = 2;
        } else if (blink_state_sad == 2) {
            eye_h_sad += 4; if (eye_h_sad >= 24) { eye_h_sad = 24; blink_state_sad = 0; }
        }
        static int look_timer_sad = 0, look_x_sad = 0, look_y_sad = 0;
        look_timer_sad++;
        if (look_timer_sad > 150) {
            look_timer_sad = 0;
            if ((rand()%100) < 40) { look_x_sad = (rand()%10) - 5; look_y_sad = (rand()%6) - 3; }
            else { look_x_sad = 0; look_y_sad = 0; }
        }
        DrawEye(eyeLx + xOffset + tremorX + look_x_sad, eyeLy + tremorY + look_y_sad, eyeLw, eye_h_sad, eyeRadius, layer);
        DrawEye(eyeRx + xOffset + tremorX + look_x_sad, eyeRy + tremorY + look_y_sad, eyeRw, eye_h_sad, eyeRadius, layer);
        draw_canvas_line(layer, (eyeLx - 8 + xOffset + look_x_sad)*2, (eyeLy - 10 + look_y_sad)*2, (eyeLx + 8 + xOffset + look_x_sad)*2, (eyeLy - 14 + look_y_sad)*2, lv_color_hex(0x0080FF), 4);
        draw_canvas_line(layer, (eyeRx - 8 + xOffset + look_x_sad)*2, (eyeRy - 14 + look_y_sad)*2, (eyeRx + 8 + xOffset + look_x_sad)*2, (eyeRy - 10 + look_y_sad)*2, lv_color_hex(0x0080FF), 4);
    } else if (emotion == "loving") {'''

content = content.replace(old_cb, new_cb)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

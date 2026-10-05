with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

# Change canvas_size to a safe, overestimated size to prevent heap corruption in LVGL 9
content = content.replace('size_t canvas_size = 256 * 128 * sizeof(lv_color_t);', 'size_t canvas_size = 256 * 128 * 4; // Safest size for ARGB8888 or any 32-bit aligned LVGL 9 format')

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

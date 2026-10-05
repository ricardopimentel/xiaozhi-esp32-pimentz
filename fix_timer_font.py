
file_path = "main/display/lcd_display.cc"
with open(file_path, "r", encoding="utf-8") as f:
    content = f.read()

content = content.replace("lv_obj_set_style_text_font(timer_label_, large_icon_font, 0);", "lv_obj_set_style_text_font(timer_label_, text_font, 0);")

with open(file_path, "w", encoding="utf-8") as f:
    f.write(content)
print("Fixed timer font!")


import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

# Find the canvas creation block in the first SetupUI
canvas_block_match = re.search(r'(// Criação do Canvas do Rosto OLED.*?face_canvas_timer_ = lv_timer_create\(EyeTimerCallback, 30, this\);\n)', content, re.DOTALL)
if canvas_block_match:
    canvas_block = canvas_block_match.group(1)
    # Check if the second SetupUI already has it
    second_setup_ui = content[content.find('void LcdDisplay::SetupUI()', 800):content.find('void LcdDisplay::SetChatMessage', 1000)]
    if 'face_canvas_ =' not in second_setup_ui:
        # We need to insert it at the end of the second SetupUI
        # Find where the second SetupUI ends. It ends right before oid LcdDisplay::SetChatMessage
        insert_pos = content.find('void LcdDisplay::SetChatMessage', 1000)
        # Go back to the closing brace of SetupUI
        insert_pos = content.rfind('}', 800, insert_pos)
        
        # Also need to make sure egg_bar_ exists in the second one too!
        # Let's just insert egg_bar_ and face_canvas_ at the end of the second SetupUI
        egg_bar_code = '''
    egg_obj_ = lv_obj_create(screen);
    lv_obj_set_size(egg_obj_, 128, 128);
    lv_obj_align(egg_obj_, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_opa(egg_obj_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(egg_obj_, 0, 0);
    lv_obj_add_flag(egg_obj_, LV_OBJ_FLAG_HIDDEN);

    egg_bar_ = lv_bar_create(screen);
    lv_obj_set_size(egg_bar_, 120, 10);
    lv_obj_align_to(egg_bar_, egg_obj_, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_bar_set_range(egg_bar_, 0, 15); // Hatch time: 15 seconds
    lv_bar_set_value(egg_bar_, 0, LV_ANIM_OFF);
    lv_obj_add_flag(egg_bar_, LV_OBJ_FLAG_HIDDEN);
'''
        
        # Remove the egg_bar code from canvas_block if it is there, so we don't duplicate
        # Actually canvas block doesn't have egg bar code.
        
        content = content[:insert_pos] + egg_bar_code + '\n' + canvas_block + '\n' + content[insert_pos:]
        with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
            f.write(content)
        print("Successfully injected canvas logic into second SetupUI")
    else:
        print("Canvas logic already in second SetupUI")
else:
    print("Could not find canvas block")

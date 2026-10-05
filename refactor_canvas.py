import re

with open('main/display/lcd_display.h', 'r', encoding='utf-8') as f:
    h_content = f.read()

# 1. Update header definitions
h_content = h_content.replace('void DesenharParticulas(int xOffset);', 'void DesenharParticulas(int xOffset, lv_layer_t* layer);')
h_content = h_content.replace('void DrawEye(float x, float y, float w, float h, float r);', 'void DrawEye(float x, float y, float w, float h, float r, lv_layer_t* layer);')
h_content = h_content.replace('void DrawEyeHappy(float x, float y, float w, float h, float r, float progress);', 'void DrawEyeHappy(float x, float y, float w, float h, float r, float progress, lv_layer_t* layer);')
h_content = h_content.replace('void DrawEyeSqueezed(float x, float y, float w, float h, float r, float progress, bool isLeft);', 'void DrawEyeSqueezed(float x, float y, float w, float h, float r, float progress, bool isLeft, lv_layer_t* layer);')
h_content = h_content.replace('void DrawHeart(int x, int y);', 'void DrawHeart(int x, int y, lv_layer_t* layer);')

with open('main/display/lcd_display.h', 'w', encoding='utf-8') as f:
    f.write(h_content)


with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    c_content = f.read()

# 2. Update wrappers
c_content = c_content.replace('static void draw_canvas_line(lv_obj_t * canvas', 'static void draw_canvas_line(lv_layer_t * layer')
c_content = c_content.replace('static void draw_canvas_rect(lv_obj_t * canvas', 'static void draw_canvas_rect(lv_layer_t * layer')
c_content = c_content.replace('static void draw_canvas_rect_empty(lv_obj_t * canvas', 'static void draw_canvas_rect_empty(lv_layer_t * layer')
c_content = c_content.replace('static void draw_canvas_arc(lv_obj_t * canvas', 'static void draw_canvas_arc(lv_layer_t * layer')

# Remove layer init/finish from wrappers and change &layer to layer
c_content = re.sub(r'lv_layer_t layer;\s*lv_canvas_init_layer\(canvas, &layer\);\s*', '', c_content)
c_content = re.sub(r'lv_canvas_finish_layer\(canvas, &layer\);\s*', '', c_content)
c_content = c_content.replace('lv_draw_line(&layer', 'lv_draw_line(layer')
c_content = c_content.replace('lv_draw_rect(&layer', 'lv_draw_rect(layer')
c_content = c_content.replace('lv_draw_arc(&layer', 'lv_draw_arc(layer')

# 3. Update method signatures
c_content = c_content.replace('void LcdDisplay::DesenharParticulas(int xOffset) {', 'void LcdDisplay::DesenharParticulas(int xOffset, lv_layer_t* layer) {')
c_content = c_content.replace('void LcdDisplay::DrawEye(float x, float y, float w, float h, float r) {', 'void LcdDisplay::DrawEye(float x, float y, float w, float h, float r, lv_layer_t* layer) {')
c_content = c_content.replace('void LcdDisplay::DrawEyeHappy(float x, float y, float w, float h, float r, float progress) {', 'void LcdDisplay::DrawEyeHappy(float x, float y, float w, float h, float r, float progress, lv_layer_t* layer) {')
c_content = c_content.replace('void LcdDisplay::DrawEyeSqueezed(float x, float y, float w, float h, float r, float progress, bool isLeft) {', 'void LcdDisplay::DrawEyeSqueezed(float x, float y, float w, float h, float r, float progress, bool isLeft, lv_layer_t* layer) {')
c_content = c_content.replace('void LcdDisplay::DrawHeart(int x, int y) {', 'void LcdDisplay::DrawHeart(int x, int y, lv_layer_t* layer) {')

# 4. Replace face_canvas_ with layer in drawing calls
c_content = c_content.replace('draw_canvas_line(face_canvas_', 'draw_canvas_line(layer')
c_content = c_content.replace('draw_canvas_rect(face_canvas_', 'draw_canvas_rect(layer')
c_content = c_content.replace('draw_canvas_rect_empty(face_canvas_', 'draw_canvas_rect_empty(layer')
c_content = c_content.replace('draw_canvas_arc(face_canvas_', 'draw_canvas_arc(layer')

# 5. Fix DrawOledFace to init layer and pass it down
# Let's find DrawOledFace
draw_face_start = c_content.find('void LcdDisplay::DrawOledFace(int xOffset) {')
if draw_face_start != -1:
    bg_fill_str = 'lv_canvas_fill_bg(face_canvas_, lv_color_black(), LV_OPA_COVER);'
    bg_fill_pos = c_content.find(bg_fill_str, draw_face_start)
    if bg_fill_pos != -1:
        insert_pos = bg_fill_pos + len(bg_fill_str)
        layer_init = '''
#if LVGL_VERSION_MAJOR >= 9
    lv_layer_t layer_obj;
    lv_canvas_init_layer(face_canvas_, &layer_obj);
    lv_layer_t* layer = &layer_obj;
#else
    lv_layer_t* layer = nullptr;
#endif
'''
        c_content = c_content[:insert_pos] + layer_init + c_content[insert_pos:]
        
    # Now we must close the layer at the end of DrawOledFace. 
    # DrawOledFace ends before 'void LcdDisplay::UpdateEyeAnimations()'
    end_face_pos = c_content.find('void LcdDisplay::UpdateEyeAnimations()')
    if end_face_pos != -1:
        # Go back to the closing brace
        closing_brace = c_content.rfind('}', draw_face_start, end_face_pos)
        layer_finish = '''
#if LVGL_VERSION_MAJOR >= 9
    lv_canvas_finish_layer(face_canvas_, &layer_obj);
#endif
'''
        c_content = c_content[:closing_brace] + layer_finish + c_content[closing_brace:]

# 6. Pass layer in recursive calls
c_content = c_content.replace('DesenharParticulas(xOffset);', 'DesenharParticulas(xOffset, layer);')
c_content = c_content.replace('DrawHeart(px/2, py/2);', 'DrawHeart(px/2, py/2, layer);')
c_content = c_content.replace('DrawEye(eyeLx + xOffset + tremorX + look_x, eyeLy + tremorY + look_y, eyeLw, eye_h, eyeRadius);', 'DrawEye(eyeLx + xOffset + tremorX + look_x, eyeLy + tremorY + look_y, eyeLw, eye_h, eyeRadius, layer);')
c_content = c_content.replace('DrawEye(eyeRx + xOffset + tremorX + look_x, eyeRy + tremorY + look_y, eyeRw, eye_h, eyeRadius);', 'DrawEye(eyeRx + xOffset + tremorX + look_x, eyeRy + tremorY + look_y, eyeRw, eye_h, eyeRadius, layer);')

# Wait, there are more DrawEye calls in happy/squeezed! Let's just use regex for all DrawEye* calls.
c_content = re.sub(r'DrawEye\((.*?)\);', r'DrawEye(\1, layer);', c_content)
c_content = re.sub(r'DrawEyeHappy\((.*?)\);', r'DrawEyeHappy(\1, layer);', c_content)
c_content = re.sub(r'DrawEyeSqueezed\((.*?)\);', r'DrawEyeSqueezed(\1, layer);', c_content)
# If a call already has layer, it will become layer, layer. Let's fix that if it happens.
c_content = c_content.replace(', layer, layer)', ', layer)')

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(c_content)

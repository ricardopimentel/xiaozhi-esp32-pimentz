import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

wrappers = '''
#include <esp_heap_caps.h>

#if LVGL_VERSION_MAJOR >= 9
static void draw_canvas_line(lv_obj_t * canvas, int x1, int y1, int x2, int y2, lv_color_t color, int width) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = width;
    dsc.p1.x = x1; dsc.p1.y = y1;
    dsc.p2.x = x2; dsc.p2.y = y2;
    lv_draw_line(&layer, &dsc);
    lv_canvas_finish_layer(canvas, &layer);
}

static void draw_canvas_rect(lv_obj_t * canvas, int x, int y, int w, int h, lv_color_t color, int radius) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = radius;
    lv_area_t area = {x, y, x + w, y + h};
    lv_draw_rect(&layer, &dsc, &area);
    lv_canvas_finish_layer(canvas, &layer);
}

static void draw_canvas_rect_empty(lv_obj_t * canvas, int x, int y, int w, int h, lv_color_t border_color, int border_width, int radius) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_TRANSP;
    dsc.border_color = border_color;
    dsc.border_width = border_width;
    dsc.border_opa = LV_OPA_COVER;
    dsc.radius = radius;
    lv_area_t area = {x, y, x + w, y + h};
    lv_draw_rect(&layer, &dsc, &area);
    lv_canvas_finish_layer(canvas, &layer);
}

static void draw_canvas_arc(lv_obj_t * canvas, int x, int y, int radius, int start_angle, int end_angle, lv_color_t color, int width) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = width;
    dsc.center.x = x;
    dsc.center.y = y;
    dsc.radius = radius;
    dsc.start_angle = start_angle;
    dsc.end_angle = end_angle;
    lv_draw_arc(&layer, &dsc);
    lv_canvas_finish_layer(canvas, &layer);
}
#endif
'''

if 'draw_canvas_line' not in content:
    # Insert wrappers at the top
    content = wrappers + content

# Replace old calls
content = re.sub(r'lv_canvas_draw_line\(([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^)]+)\)', r'draw_canvas_line(\1, \2, \3, \4, \5, \6, \7)', content)
content = re.sub(r'lv_canvas_draw_arc\(([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^)]+)\)', r'draw_canvas_arc(\1, \2, \3, \4, \5, \6, \7, \8)', content)
content = re.sub(r'lv_canvas_draw_rect\(([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*[^,]+,\s*LV_OPA_COVER,\s*[^,]+,\s*([^)]+)\)', r'draw_canvas_rect(\1, \2, \3, \4, \5, \6, \7)', content)
content = re.sub(r'lv_canvas_draw_rect\(([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*[^,]+,\s*LV_OPA_TRANSP,\s*([^,]+),\s*([^)]+)\)', r'draw_canvas_rect_empty(\1, \2, \3, \4, \5, \7, \8, \6)', content)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)

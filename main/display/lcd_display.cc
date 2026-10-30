
#include <math.h>
#include "lcd_display.h"
#include "application.h"
#include <lvgl.h>
#include <esp_heap_caps.h>

#if LVGL_VERSION_MAJOR >= 9
static void draw_canvas_line(lv_layer_t * layer, int x1, int y1, int x2, int y2, lv_color_t color, int width) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = width;
    dsc.p1.x = x1; dsc.p1.y = y1;
    dsc.p2.x = x2; dsc.p2.y = y2;
    lv_draw_line(layer, &dsc);
    }

static void draw_canvas_rect(lv_layer_t * layer, int x, int y, int w, int h, lv_color_t color, int radius) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = radius;
    lv_area_t area = {x, y, x + w, y + h};
    lv_draw_rect(layer, &dsc, &area);
    }

static void draw_canvas_rect_empty(lv_layer_t * layer, int x, int y, int w, int h, lv_color_t border_color, int border_width, int radius) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_TRANSP;
    dsc.border_color = border_color;
    dsc.border_width = border_width;
    dsc.border_opa = LV_OPA_COVER;
    dsc.radius = radius;
    lv_area_t area = {x, y, x + w, y + h};
    lv_draw_rect(layer, &dsc, &area);
}

static void SanitizeToAscii(const char* src, char* dst, size_t max_len) {
    if (!src || !dst || max_len == 0) return;
    size_t d = 0;
    for (size_t s = 0; src[s] != '\0' && d + 1 < max_len; s++) {
        unsigned char c = (unsigned char)src[s];
        if (c < 0x80) {
            if (c >= 0x20 && c <= 0x7E) {
                dst[d++] = (char)c;
            } else if (c == '\t') {
                dst[d++] = ' ';
            }
        } else if (c == 0xC3 && src[s + 1] != '\0') {
            s++;
            unsigned char c2 = (unsigned char)src[s];
            char mapped = ' ';
            if (c2 >= 0x80 && c2 <= 0x85) mapped = 'A';
            else if (c2 >= 0xA0 && c2 <= 0xA5) mapped = 'a';
            else if (c2 == 0x87) mapped = 'C';
            else if (c2 == 0xA7) mapped = 'c';
            else if (c2 >= 0x88 && c2 <= 0x8B) mapped = 'E';
            else if (c2 >= 0xA8 && c2 <= 0xAB) mapped = 'e';
            else if (c2 >= 0x8C && c2 <= 0x8F) mapped = 'I';
            else if (c2 >= 0xAC && c2 <= 0xAF) mapped = 'i';
            else if (c2 >= 0x92 && c2 <= 0x96) mapped = 'O';
            else if (c2 >= 0xB2 && c2 <= 0xB6) mapped = 'o';
            else if (c2 >= 0x99 && c2 <= 0x9C) mapped = 'U';
            else if (c2 >= 0xB9 && c2 <= 0xBC) mapped = 'u';
            if (mapped != ' ') dst[d++] = mapped;
        } else if (c == 0xE2 && (unsigned char)src[s + 1] == 0x80) {
            s += 2;
            dst[d++] = '-';
        } else {
            while ((src[s + 1] & 0xC0) == 0x80) {
                s++;
            }
        }
    }
    dst[d] = '\0';
}

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);

static void draw_canvas_text(lv_layer_t * layer, int x, int y, int w, int h, const char * text, lv_color_t color, const lv_font_t * font = nullptr, lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
    if (!text || text[0] == '\0' || !layer) return;
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = color;
    dsc.font = font ? font : &BUILTIN_TEXT_FONT;
    dsc.text = text;
    dsc.align = align;
#if LVGL_VERSION_MAJOR >= 9
    dsc.text_local = 1;
#endif
    lv_area_t area = {x, y, x + w, y + h};
    lv_draw_label(layer, &dsc, &area);
}

static void draw_canvas_arc(lv_layer_t * layer, int x, int y, int radius, int start_angle, int end_angle, lv_color_t color, int width) {
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = width;
    dsc.center.x = x;
    dsc.center.y = y;
    dsc.radius = radius;
    dsc.start_angle = start_angle;
    dsc.end_angle = end_angle;
    lv_draw_arc(layer, &dsc);
}

static void draw_canvas_disc(lv_layer_t * layer, int cx, int cy, int radius, lv_color_t color) {
    draw_canvas_rect(layer, cx - radius, cy - radius, radius * 2, radius * 2, color, radius);
}

static void draw_canvas_triangle(lv_layer_t * layer, int x1, int y1, int x2, int y2, int x3, int y3, lv_color_t color) {
    draw_canvas_line(layer, x1, y1, x2, y2, color, 3);
    draw_canvas_line(layer, x2, y2, x3, y3, color, 3);
    draw_canvas_line(layer, x3, y3, x1, y1, color, 3);
    draw_canvas_disc(layer, (x1 + x2 + x3) / 3, (y1 + y2 + y3) / 3, 3, color);
}

static void draw_canvas_quad_bezier(lv_layer_t * layer, float x0, float y0, float cx, float cy, float x1, float y1, lv_color_t color, int width, int steps = 6) {
    float px = x0, py = y0;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float u = 1.0f - t;
        float nx = u * u * x0 + 2.0f * u * t * cx + t * t * x1;
        float ny = u * u * y0 + 2.0f * u * t * cy + t * t * y1;
        draw_canvas_line(layer, (int)roundf(px), (int)roundf(py), (int)roundf(nx), (int)roundf(ny), color, width);
        px = nx;
        py = ny;
    }
}

static void draw_sarcastic_eyebrows(lv_layer_t * layer, int lx, int ly, int rx, int ry, lv_color_t color, float leftLift = 0.0f, float rightLift = 0.0f) {
    // Sobrancelha esquerda: arqueada (⌒)
    float lbY = (float)ly - 20.0f + leftLift;
    draw_canvas_quad_bezier(layer, lx - 18.0f, lbY + 5.0f, (float)lx, lbY - 7.0f, lx + 16.0f, lbY + 4.0f, color, 4, 6);

    // Sobrancelha direita: reta na base interna e subindo na ponta externa direita (───╯)
    float rbY = (float)ry - 17.0f + rightLift;
    draw_canvas_line(layer, rx - 16, (int)roundf(rbY + 1.0f), rx + 6, (int)roundf(rbY + 1.0f), color, 4);
    draw_canvas_quad_bezier(layer, rx + 6.0f, rbY + 1.0f, rx + 16.0f, rbY + 1.0f, rx + 21.0f, rbY - 11.0f, color, 4, 5);
}

static void draw_sarcastic_mouth(lv_layer_t * layer, int mx, int my, lv_color_t color) {
    draw_canvas_quad_bezier(layer, mx - 12.0f, my + 4.0f, (float)mx, my - 4.0f, mx + 12.0f, my + 4.0f, color, 4, 6);
}

static void draw_sarcastic_half_closed_eye(lv_layer_t * layer, int cx, int cy, int w, int h, int r, lv_color_t primary, lv_color_t aura, float pupilOffsetX, float pupilOffsetY, bool showPupil = true) {
    if (h < 2 || w < 2) return;
    int x = cx - w / 2;
    int y = cy - h / 2;
    int rad = (r < w / 2) ? ((r < h / 2) ? r : h / 2) : ((w / 2 < h / 2) ? w / 2 : h / 2);

    // 1. Aura exterior
    if (h > 8) {
        draw_canvas_rect_empty(layer, x - 2, y - 2, w + 4, h + 4, aura, 2, rad + 2);
    }

    // 2. Preenchimento da íris
    draw_canvas_rect(layer, x, y, w, h, primary, rad);

    // 3. Pupila recortada (clipping) para sumir atrás da pálpebra superior ao subir
    if (showPupil && h >= 10) {
        int pw = (int)(w * 0.50f);
        int ph = (int)(h * 0.62f);
        if (ph < 12) ph = 12;
        int px0 = (int)roundf(cx + pupilOffsetX - pw / 2.0f);
        int py0 = (int)roundf(cy + pupilOffsetY - ph / 2.0f);
        int px1 = px0 + pw;
        int py1 = py0 + ph;

        int c_px0 = (px0 > x + 2) ? px0 : (x + 2);
        int c_py0 = (py0 > y + 3) ? py0 : (y + 3);
        int c_px1 = (px1 < x + w - 2) ? px1 : (x + w - 2);
        int c_py1 = (py1 < y + h - 2) ? py1 : (y + h - 2);
        int c_pw = c_px1 - c_px0;
        int c_ph = c_py1 - c_py0;

        if (c_pw > 1 && c_ph > 1) {
            int pRad = (c_pw / 2 < c_ph / 2) ? (c_pw / 2) : (c_ph / 2);
            if (pRad > 4) pRad = 4;
            draw_canvas_rect(layer, c_px0, c_py0, c_pw, c_ph, lv_color_hex(0x000000), pRad);

            // Brilho especular (só aparece se não estiver escondido atrás da pálpebra)
            int hx = (int)roundf(cx + pupilOffsetX - pw * 0.22f);
            int hy = (int)roundf(cy + pupilOffsetY - ph * 0.15f);
            if (hx >= c_px0 + 2 && hx <= c_px1 - 2 && hy >= c_py0 + 2 && hy <= c_py1 - 2) {
                draw_canvas_disc(layer, hx, hy, 2, lv_color_hex(0xFFFFFF));
            }
        }
    }

    // 4. Barra da pálpebra superior reta por cima
    draw_canvas_line(layer, x - 3, y + 1, x + w + 3, y + 1, aura, 4);
}

static void draw_anime_sweat_drop(lv_layer_t * layer, int dropX, int dropY, float r) {
    if (r <= 1.0f) return;
    lv_color_t borderCol = lv_color_hex(0x0091EA);
    lv_color_t fillCol = lv_color_hex(0x00E5FF);
    lv_color_t hiCol = lv_color_hex(0xFFFFFF);

    float topY = dropY - r * 1.9f;
    float botY = dropY + r * 0.2f;
    int steps = 14;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float cy = topY + t * (botY - topY);
        float cr = 1.0f + (t * t) * (r - 1.0f);
        draw_canvas_disc(layer, dropX, (int)roundf(cy), (int)roundf(cr + 2.0f), borderCol);
    }
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float cy = topY + t * (botY - topY);
        float cr = 1.0f + (t * t) * (r - 1.0f);
        int ir = (int)roundf(cr);
        if (ir < 1) ir = 1;
        draw_canvas_disc(layer, dropX, (int)roundf(cy), ir, fillCol);
    }
    int hx = (int)roundf(dropX - r * 0.35f);
    int hy = (int)roundf(dropY + r * 0.15f);
    int hr = (int)roundf(r * 0.28f);
    if (hr < 1) hr = 1;
    draw_canvas_disc(layer, hx, hy, hr, hiCol);
}

static void draw_four_point_star(lv_layer_t * layer, int cx, int cy, float r, lv_color_t color) {
    if (r < 1.0f) return;
    int ir = (int)roundf(r);
    int wMid = std::max(1, (int)roundf(r * 0.36f));
    draw_canvas_triangle(layer, cx, cy - ir, cx - wMid, cy, cx + wMid, cy, color);
    draw_canvas_triangle(layer, cx, cy + ir, cx - wMid, cy, cx + wMid, cy, color);
    draw_canvas_triangle(layer, cx - ir, cy, cx, cy - wMid, cx, cy + wMid, color);
    draw_canvas_triangle(layer, cx + ir, cy, cx, cy - wMid, cx, cy + wMid, color);
}

static void draw_scalable_heart(lv_layer_t * layer, int cx, int cy, float scale, lv_color_t color) {
    if (scale <= 0.15f) return;
    int r = std::max(1, (int)roundf(2.8f * scale));
    int dx = std::max(1, (int)roundf(2.5f * scale));
    int dy = (int)roundf(2.0f * scale);
    int tw = std::max(2, (int)roundf(5.2f * scale));
    int th = std::max(2, (int)roundf(5.5f * scale));
    draw_canvas_disc(layer, cx - dx, cy - dy, r, color);
    draw_canvas_disc(layer, cx + dx, cy - dy, r, color);
    draw_canvas_triangle(layer, cx - tw, cy - 1, cx + tw, cy - 1, cx, cy + th, color);
}
#endif

#include "gif/lvgl_gif.h"
#include "settings.h"
#include "tamagotchi_engine.h"
#include <lvgl.h>
#include "lvgl_theme.h"
#include "assets/lang_config.h"

#include <vector>
#include <algorithm>
#include <font_awesome.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <esp_psram.h>
#include <cstring>
#include <src/misc/cache/lv_cache.h>

#include "board.h"

#define TAG "LcdDisplay"

LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_4);

void LcdDisplay::InitializeLcdThemes() {
    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_awesome_30_4);

    // light theme
    auto light_theme = new LvglTheme("light");
    light_theme->set_background_color(lv_color_hex(0x000000));
    light_theme->set_text_color(lv_color_hex(0xFFFFFF));
    light_theme->set_chat_background_color(lv_color_hex(0x000000));
    light_theme->set_user_bubble_color(lv_color_hex(0x00FF00));
    light_theme->set_assistant_bubble_color(lv_color_hex(0xFFD700));
    light_theme->set_system_bubble_color(lv_color_hex(0x222222));
    light_theme->set_system_text_color(lv_color_hex(0xFFFFFF));
    light_theme->set_border_color(lv_color_hex(0xFFFFFF));
    light_theme->set_low_battery_color(lv_color_hex(0xFF0000));
    light_theme->set_text_font(text_font);
    light_theme->set_icon_font(icon_font);
    light_theme->set_large_icon_font(large_icon_font);

    // dark theme
    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_background_color(lv_color_hex(0x000000));
    dark_theme->set_text_color(lv_color_hex(0xFFFFFF));
    dark_theme->set_chat_background_color(lv_color_hex(0x000000));
    dark_theme->set_user_bubble_color(lv_color_hex(0x00FF00));
    dark_theme->set_assistant_bubble_color(lv_color_hex(0xFFD700));
    dark_theme->set_system_bubble_color(lv_color_hex(0x222222));
    dark_theme->set_system_text_color(lv_color_hex(0xFFFFFF));
    dark_theme->set_border_color(lv_color_hex(0xFFFFFF));
    dark_theme->set_low_battery_color(lv_color_hex(0xFF0000));
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("light", light_theme);
    theme_manager.RegisterTheme("dark", dark_theme);
}

LcdDisplay::LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    // Initialize LCD themes
    InitializeLcdThemes();

    // Load theme from settings
    Settings settings("display", false);
    std::string theme_name = settings.GetString("theme", "light");
    current_theme_ = LvglThemeManager::GetInstance().GetTheme(theme_name);

    // Create a timer to hide the preview image
    esp_timer_create_args_t preview_timer_args = {
        .callback = [](void* arg) {
            LcdDisplay* display = static_cast<LcdDisplay*>(arg);
            display->SetPreviewImage(nullptr);
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "preview_timer",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&preview_timer_args, &preview_timer_);
}

SpiLcdDisplay::SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height) {

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    {
        esp_err_t __err = esp_lcd_panel_disp_on_off(panel_, true);
        if (__err == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "Panel does not support disp_on_off; assuming ON");
        } else {
            ESP_ERROR_CHECK(__err);
        }
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

#if CONFIG_SPIRAM
    // lv image cache, currently only PNG is supported
    size_t psram_size_mb = esp_psram_get_size() / 1024 / 1024;
    if (psram_size_mb >= 8) {
        lv_image_cache_resize(2 * 1024 * 1024, true);
        ESP_LOGI(TAG, "Use 2MB of PSRAM for image cache");
    } else if (psram_size_mb >= 2) {
        lv_image_cache_resize(512 * 1024, true);
        ESP_LOGI(TAG, "Use 512KB of PSRAM for image cache");
    }
#endif

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 20),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }
}


// RGB LCD implementation
RgbLcdDisplay::RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y,
                           bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height) {

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .buffer_size = static_cast<uint32_t>(width_ * 20),
        .double_buffer = true,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .swap_bytes = 0,
            .full_refresh = 1,
            .direct_mode = 1,
        },
    };

    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,
            .avoid_tearing = true,
        }
    };
    
    display_ = lvgl_port_add_disp_rgb(&display_cfg, &rgb_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add RGB display");
        return;
    }
    
    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }
}

MipiLcdDisplay::MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                            int width, int height,  int offset_x, int offset_y,
                            bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height) {

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = panel_io,
        .panel_handle = panel,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 50),
        .double_buffer = false,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram =false,
            .sw_rotate = true,
        },
    };

    const lvgl_port_display_dsi_cfg_t dpi_cfg = {
        .flags = {
            .avoid_tearing = false,
        }
    };
    display_ = lvgl_port_add_disp_dsi(&disp_cfg, &dpi_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }
}

LcdDisplay::~LcdDisplay() {
    SetPreviewImage(nullptr);
    
    // Clean up GIF controller
    if (gif_controller_) {
        gif_controller_->Stop();
        gif_controller_.reset();
    }
    
    if (preview_timer_ != nullptr) {
        esp_timer_stop(preview_timer_);
        esp_timer_delete(preview_timer_);
    }

    if (preview_image_ != nullptr) {
        lv_obj_del(preview_image_);
    }
    if (chat_message_label_ != nullptr) {
        lv_obj_del(chat_message_label_);
    }
    if (emoji_label_ != nullptr) {
        lv_obj_del(emoji_label_);
    }
    if (emoji_image_ != nullptr) {
        lv_obj_del(emoji_image_);
    }
    if (emoji_box_ != nullptr) {
        lv_obj_del(emoji_box_);
    }
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (bottom_bar_ != nullptr) {
        lv_obj_del(bottom_bar_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (top_bar_ != nullptr) {
        lv_obj_del(top_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }
    if (display_ != nullptr) {
        lv_display_delete(display_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
}

bool LcdDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void LcdDisplay::Unlock() {
    lvgl_port_unlock();
}

#if CONFIG_USE_WECHAT_MESSAGE_STYLE
void LcdDisplay::SetupUI() {
    // Prevent duplicate calls - if already called, return early
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }
    
    Display::SetupUI();  // Mark SetupUI as called
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lvgl_theme->text_color(), 0);
    lv_obj_set_style_bg_color(screen, lvgl_theme->background_color(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_border_color(container_, lvgl_theme->border_color(), 0);

    /* Layer 1: Top bar - for status icons */
    top_bar_ = lv_obj_create(container_);
    lv_obj_set_size(top_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_50, 0);  // 50% opacity background
    lv_obj_set_style_bg_color(top_bar_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_pad_all(top_bar_, 0, 0);
    lv_obj_set_style_pad_top(top_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_bottom(top_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_left(top_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_pad_right(top_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);

    // Left icon
    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lvgl_theme->text_color(), 0);

    // Right icons container
    lv_obj_t* right_icons = lv_obj_create(top_bar_);
    lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_icons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_icons, 0, 0);
    lv_obj_set_style_pad_all(right_icons, 0, 0);
    lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    mute_label_ = lv_label_create(right_icons);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, lvgl_theme->text_color(), 0);

    battery_label_ = lv_label_create(right_icons);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_margin_left(battery_label_, lvgl_theme->spacing(2), 0);

    /* Layer 2: Status bar - for center text labels */
    status_bar_ = lv_obj_create(screen);
    lv_obj_set_size(status_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_pad_top(status_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_bottom(status_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);  // Use absolute positioning
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);  // Overlap with top_bar_

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(notification_label_, LV_HOR_RES * 0.8);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(status_label_, LV_HOR_RES * 0.8);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);
    
    /* Content - Chat area */
    content_ = lv_obj_create(container_);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_pad_all(content_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_bg_color(content_, lvgl_theme->chat_background_color(), 0); // Background for chat area

    // Enable scrolling for chat content
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(content_, LV_DIR_VER);
    
    // Create a flex container for chat messages
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content_, lvgl_theme->spacing(4), 0); // Space between messages

    // We'll create chat messages dynamically in SetChatMessage
    chat_message_label_ = nullptr;

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -lvgl_theme->spacing(4));
    lv_obj_set_style_bg_color(low_battery_popup_, lvgl_theme->low_battery_color(), 0);
    lv_obj_set_style_radius(low_battery_popup_, lvgl_theme->spacing(4), 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    emoji_image_ = lv_img_create(screen);
    lv_obj_align(emoji_image_, LV_ALIGN_TOP_MID, 0, text_font->line_height + lvgl_theme->spacing(8));

    // Display AI logo while booting
    emoji_label_ = lv_label_create(screen);
    lv_obj_center(emoji_label_);
    lv_obj_set_style_text_font(emoji_label_, large_icon_font, 0);
    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(emoji_label_, FONT_AWESOME_MICROCHIP_AI);
}
#if CONFIG_IDF_TARGET_ESP32P4
#define  MAX_MESSAGES 40
#else
#define  MAX_MESSAGES 20
#endif
void LcdDisplay::SetChatMessage(const char* role, const char* content) {
    if (!setup_ui_called_) {
        ESP_LOGW(TAG, "SetChatMessage('%s', '%s') called before SetupUI() - message will be lost!", role, content);
    }
    DisplayLockGuard lock(this);
    if (content_ == nullptr) {
        if (setup_ui_called_) {
            ESP_LOGW(TAG, "SetChatMessage('%s', '%s') failed: content_ is nullptr (SetupUI() was called but container not created)", role, content);
        }
        return;
    }
    
    // Check if message count exceeds limit
    uint32_t child_count = lv_obj_get_child_cnt(content_);
    if (child_count >= MAX_MESSAGES) {
        // Delete the oldest message (first child object)
        lv_obj_t* first_child = lv_obj_get_child(content_, 0);
        if (first_child != nullptr) {
            lv_obj_del(first_child);
            // Refresh child count after deletion
            child_count = lv_obj_get_child_cnt(content_);
        }
        // Scroll to the last message immediately (get last_child after deletion)
        if (child_count > 0) {
            lv_obj_t* last_child = lv_obj_get_child(content_, child_count - 1);
            if (last_child != nullptr && lv_obj_is_valid(last_child)) {
                lv_obj_scroll_to_view_recursive(last_child, LV_ANIM_OFF);
            }
        }
    }
    
    // Collapse system messages (if it's a system message, check if the last message is also a system message)
    if (strcmp(role, "system") == 0) {
        // Refresh child count to get accurate count after potential deletion above
        child_count = lv_obj_get_child_cnt(content_);
        if (child_count > 0) {
            // Get the last message container
            lv_obj_t* last_container = lv_obj_get_child(content_, child_count - 1);
            if (last_container != nullptr && lv_obj_is_valid(last_container) && lv_obj_get_child_cnt(last_container) > 0) {
                // Get the bubble inside the container
                lv_obj_t* last_bubble = lv_obj_get_child(last_container, 0);
                if (last_bubble != nullptr && lv_obj_is_valid(last_bubble)) {
                    // Check if bubble type is system message
                    void* bubble_type_ptr = lv_obj_get_user_data(last_bubble);
                    if (bubble_type_ptr != nullptr && strcmp((const char*)bubble_type_ptr, "system") == 0) {
                        // If the last message is also a system message, delete it
                        lv_obj_del(last_container);
                    }
                }
            }
        }
    } else {
        // Hide the centered AI logo
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }

    // Avoid empty message boxes
    if(strlen(content) == 0) {
        return;
    }

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);

    // Create a message bubble
    lv_obj_t* msg_bubble = lv_obj_create(content_);
    lv_obj_set_style_radius(msg_bubble, 8, 0);
    lv_obj_set_scrollbar_mode(msg_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(msg_bubble, 0, 0);
    lv_obj_set_style_pad_all(msg_bubble, lvgl_theme->spacing(4), 0);

    // Create the message text
    lv_obj_t* msg_text = lv_label_create(msg_bubble);
    lv_label_set_text(msg_text, content);
    
    // Calculate bubble width constraints
    lv_coord_t max_width = LV_HOR_RES * 85 / 100 - 16;  // 85% of screen width
    lv_coord_t min_width = 20;  
    
    // Let LVGL calculate the natural text width first
    lv_obj_set_width(msg_text, LV_SIZE_CONTENT);
    lv_obj_update_layout(msg_text);
    lv_coord_t text_width = lv_obj_get_width(msg_text);
    
    // Ensure text width is not less than minimum width
    if (text_width < min_width) {
        text_width = min_width;
    }

    // Constrain to max width
    lv_coord_t bubble_width = (text_width < max_width) ? text_width : max_width;
    
    // Set message text width
    lv_obj_set_width(msg_text, bubble_width);
    lv_label_set_long_mode(msg_text, LV_LABEL_LONG_WRAP);

    // Set bubble width
    lv_obj_set_width(msg_bubble, bubble_width);
    lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);

    // Set alignment and style based on message role
    if (strcmp(role, "user") == 0) {
        // User messages are right-aligned with green background
        lv_obj_set_style_bg_color(msg_bubble, lvgl_theme->user_bubble_color(), 0);
        lv_obj_set_style_bg_opa(msg_bubble, LV_OPA_70, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, lvgl_theme->text_color(), 0);
        
        // Set custom attribute to mark bubble type
        lv_obj_set_user_data(msg_bubble, (void*)"user");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    } else if (strcmp(role, "assistant") == 0) {
        // Assistant messages are left-aligned with white background
        lv_obj_set_style_bg_color(msg_bubble, lvgl_theme->assistant_bubble_color(), 0);
        lv_obj_set_style_bg_opa(msg_bubble, LV_OPA_70, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, lvgl_theme->text_color(), 0);
        
        // Set custom attribute to mark bubble type
        lv_obj_set_user_data(msg_bubble, (void*)"assistant");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    } else if (strcmp(role, "system") == 0) {
        // System messages are center-aligned with light gray background
        lv_obj_set_style_bg_color(msg_bubble, lvgl_theme->system_bubble_color(), 0);
        lv_obj_set_style_bg_opa(msg_bubble, LV_OPA_70, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, lvgl_theme->system_text_color(), 0);
        
        // Set custom attribute to mark bubble type
        lv_obj_set_user_data(msg_bubble, (void*)"system");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    }
    
    // Create a full-width container for user messages to ensure right alignment
    if (strcmp(role, "user") == 0) {
        // Create a full-width container
        lv_obj_t* container = lv_obj_create(content_);
        lv_obj_set_width(container, LV_HOR_RES);
        lv_obj_set_height(container, LV_SIZE_CONTENT);
        
        // Make container transparent and borderless
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        
        // Move the message bubble into this container
        lv_obj_set_parent(msg_bubble, container);
        
        // Right align the bubble in the container
        lv_obj_align(msg_bubble, LV_ALIGN_RIGHT_MID, -25, 0);
        
        // Auto-scroll to this container
        lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
    } else if (strcmp(role, "system") == 0) {
        // Create full-width container for system messages to ensure center alignment
        lv_obj_t* container = lv_obj_create(content_);
        lv_obj_set_width(container, LV_HOR_RES);
        lv_obj_set_height(container, LV_SIZE_CONTENT);
        
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        
        lv_obj_set_parent(msg_bubble, container);
        lv_obj_align(msg_bubble, LV_ALIGN_CENTER, 0, 0);
        lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
    } else {
        // For assistant messages
        // Left align assistant messages
        lv_obj_align(msg_bubble, LV_ALIGN_LEFT_MID, 0, 0);

        // Auto-scroll to the message bubble
        lv_obj_scroll_to_view_recursive(msg_bubble, LV_ANIM_ON);
    }
    
    // Store reference to the latest message label
    chat_message_label_ = msg_text;
}

void LcdDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    DisplayLockGuard lock(this);
    if (content_ == nullptr) {
        return;
    }

    if (image == nullptr) {
        return;
    }
    
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    // Create a message bubble for image preview
    lv_obj_t* img_bubble = lv_obj_create(content_);
    lv_obj_set_style_radius(img_bubble, 8, 0);
    lv_obj_set_scrollbar_mode(img_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(img_bubble, 0, 0);
    lv_obj_set_style_pad_all(img_bubble, lvgl_theme->spacing(4), 0);
    
    // Set image bubble background color (similar to system message)
    lv_obj_set_style_bg_color(img_bubble, lvgl_theme->assistant_bubble_color(), 0);
    lv_obj_set_style_bg_opa(img_bubble, LV_OPA_70, 0);
    
    // Set custom attribute to mark bubble type
    lv_obj_set_user_data(img_bubble, (void*)"image");

    // Create the image object inside the bubble
    lv_obj_t* preview_image = lv_image_create(img_bubble);
    
    // Calculate appropriate size for the image
    lv_coord_t max_width = LV_HOR_RES * 70 / 100;  // 70% of screen width
    lv_coord_t max_height = LV_VER_RES * 50 / 100; // 50% of screen height
    
    // Calculate zoom factor to fit within maximum dimensions
    auto img_dsc = image->image_dsc();
    lv_coord_t img_width = img_dsc->header.w;
    lv_coord_t img_height = img_dsc->header.h;
    if (img_width == 0 || img_height == 0) {
        img_width = max_width;
        img_height = max_height;
        ESP_LOGW(TAG, "Invalid image dimensions: %ld x %ld, using default dimensions: %ld x %ld", img_width, img_height, max_width, max_height);
    }
    
    lv_coord_t zoom_w = (max_width * 256) / img_width;
    lv_coord_t zoom_h = (max_height * 256) / img_height;
    lv_coord_t zoom = (zoom_w < zoom_h) ? zoom_w : zoom_h;
    
    // Ensure zoom doesn't exceed 256 (100%)
    if (zoom > 256) zoom = 256;
    
    // Set image properties
    lv_image_set_src(preview_image, img_dsc);
    lv_image_set_scale(preview_image, zoom);
    
    // Add event handler to clean up LvglImage when image is deleted
    // We need to transfer ownership of the unique_ptr to the event callback
    LvglImage* raw_image = image.release(); // Release ownership of smart pointer
    lv_obj_add_event_cb(preview_image, [](lv_event_t* e) {
        LvglImage* img = (LvglImage*)lv_event_get_user_data(e);
        if (img != nullptr) {
            delete img; // Properly release memory by deleting LvglImage object
        }
    }, LV_EVENT_DELETE, (void*)raw_image);
    
    // Calculate actual scaled image dimensions
    lv_coord_t scaled_width = (img_width * zoom) / 256;
    lv_coord_t scaled_height = (img_height * zoom) / 256;
    
    // Set bubble size to be 16 pixels larger than the image (8 pixels on each side)
    lv_obj_set_width(img_bubble, scaled_width + 16);
    lv_obj_set_height(img_bubble, scaled_height + 16);
    
    // Don't grow in flex layout
    lv_obj_set_style_flex_grow(img_bubble, 0, 0);
    
    // Center the image within the bubble
    lv_obj_center(preview_image);
    
    // Left align the image bubble like assistant messages
    lv_obj_align(img_bubble, LV_ALIGN_LEFT_MID, 0, 0);

    // Auto-scroll to the image bubble
    lv_obj_scroll_to_view_recursive(img_bubble, LV_ANIM_ON);
}

void LcdDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (content_ == nullptr) {
        return;
    }
    
    // Use lv_obj_clean to delete all children of content_ (chat message bubbles)
    lv_obj_clean(content_);
    
    // Reset chat_message_label_ as it has been deleted
    chat_message_label_ = nullptr;
    
    // Show the centered AI logo (emoji_label_) again
    if (emoji_label_ != nullptr) {
        lv_obj_remove_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
    
    ESP_LOGI(TAG, "Chat messages cleared");
}
#else
void LcdDisplay::SetupUI() {
    // Prevent duplicate calls - if already called, return early
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }
    
    Display::SetupUI();  // Mark SetupUI as called
    DisplayLockGuard lock(this);
    LvglTheme* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lvgl_theme->text_color(), 0);
    lv_obj_set_style_bg_color(screen, lvgl_theme->background_color(), 0);

    /* Container - used as background */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_border_color(container_, lvgl_theme->border_color(), 0);

    /* Bottom layer: emoji_box_ - centered display */
    emoji_box_ = lv_obj_create(screen);
    lv_obj_set_size(emoji_box_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(emoji_box_, 0, 0);
    lv_obj_set_style_border_width(emoji_box_, 0, 0);
    lv_obj_align(emoji_box_, LV_ALIGN_CENTER, 0, 0);

    emoji_label_ = lv_label_create(emoji_box_);
    lv_obj_set_style_text_font(emoji_label_, large_icon_font, 0);
    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(emoji_label_, FONT_AWESOME_MICROCHIP_AI);

    emoji_image_ = lv_img_create(emoji_box_);
    lv_obj_center(emoji_image_);
    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);

    /* Middle layer: preview_image_ - centered display */
    preview_image_ = lv_image_create(screen);
    lv_obj_set_size(preview_image_, width_ / 2, height_ / 2);
    lv_obj_align(preview_image_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);

    /* Layer 1: Top bar - for status icons (Fundo transparente sem faixa branca) */
    top_bar_ = lv_obj_create(screen);
    lv_obj_set_size(top_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);  // Fundo totalmente transparente
    lv_obj_set_style_bg_color(top_bar_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_pad_all(top_bar_, 0, 0);
    lv_obj_set_style_pad_top(top_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_bottom(top_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_left(top_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_pad_right(top_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, 0);

    // Left icon - Wi-Fi branco para destacar no fundo preto
    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_hex(0xFFFFFF), 0);

    // Right icons container
    lv_obj_t* right_icons = lv_obj_create(top_bar_);
    lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_icons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_icons, 0, 0);
    lv_obj_set_style_pad_all(right_icons, 0, 0);
    lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    mute_label_ = lv_label_create(right_icons);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, lv_color_hex(0xFFFFFF), 0);

    battery_label_ = lv_label_create(right_icons);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_margin_left(battery_label_, lvgl_theme->spacing(2), 0);

    /* Layer 2: Status bar - relógio e textos centrais brancos */
    status_bar_ = lv_obj_create(screen);
    lv_obj_set_size(status_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_pad_top(status_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_bottom(status_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);  // Use absolute positioning
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);  // Overlap with top_bar_

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(notification_label_, LV_HOR_RES * 0.75);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(status_label_, LV_HOR_RES * 0.75);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);

#if CONFIG_USE_MULTILINE_CHAT_MESSAGE
    /* Bottom bar - Balão Amarelo de Fala com texto preto */
    bottom_bar_ = lv_obj_create(screen);
    lv_obj_set_width(bottom_bar_, LV_HOR_RES - lvgl_theme->spacing(8));
    lv_obj_set_height(bottom_bar_, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(bottom_bar_, 10, 0);
    lv_obj_set_style_bg_color(bottom_bar_, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bottom_bar_, lv_color_hex(0xE6C200), 0);
    lv_obj_set_style_border_width(bottom_bar_, 2, 0);
    lv_obj_set_style_text_color(bottom_bar_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(bottom_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_scrollbar_mode(bottom_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, -lvgl_theme->spacing(2));

    /* chat_message_label_ em texto preto legível */
    chat_message_label_ = lv_label_create(bottom_bar_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES - lvgl_theme->spacing(16));
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(chat_message_label_, lv_color_hex(0x000000), 0);
    lv_obj_align(chat_message_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);  // Hide until there is content
#else
    /* Bottom bar - Balão Amarelo de Fala de linha única */
    bottom_bar_ = lv_obj_create(screen);
    lv_obj_set_size(bottom_bar_, LV_HOR_RES - lvgl_theme->spacing(8), text_font->line_height + lvgl_theme->spacing(8));
    lv_obj_set_style_radius(bottom_bar_, 10, 0);
    lv_obj_set_style_bg_color(bottom_bar_, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bottom_bar_, lv_color_hex(0xE6C200), 0);
    lv_obj_set_style_border_width(bottom_bar_, 2, 0);
    lv_obj_set_style_text_color(bottom_bar_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(bottom_bar_, 0, 0);
    lv_obj_set_style_pad_left(bottom_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_pad_right(bottom_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_scrollbar_mode(bottom_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, -lvgl_theme->spacing(2));

    /* chat_message_label_ em texto preto */
    chat_message_label_ = lv_label_create(bottom_bar_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES - lvgl_theme->spacing(16));
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(chat_message_label_, lv_color_hex(0x000000), 0);
    lv_obj_align(chat_message_label_, LV_ALIGN_CENTER, 0, 0);

    // Start scrolling after a delay
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
    lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);  // Hide until there is content
#endif

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -lvgl_theme->spacing(4));
    lv_obj_set_style_bg_color(low_battery_popup_, lvgl_theme->low_battery_color(), 0);
    lv_obj_set_style_radius(low_battery_popup_, lvgl_theme->spacing(4), 0);
    
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    /* Egg container (Egg layout) */
    egg_obj_ = lv_obj_create(screen);
    lv_obj_set_size(egg_obj_, 70, 90);
    lv_obj_align(egg_obj_, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(egg_obj_, lv_color_hex(0xFFFDD0), 0); // Cream color for egg
    lv_obj_set_style_radius(egg_obj_, 35, 0); // Oval/capsule shape!
    lv_obj_set_style_border_color(egg_obj_, lv_color_hex(0xD2B48C), 0); // Light brown border
    lv_obj_set_style_border_width(egg_obj_, 3, 0);
    lv_obj_add_flag(egg_obj_, LV_OBJ_FLAG_HIDDEN); // Hidden by default

    /* Hatch progress bar */
    egg_bar_ = lv_bar_create(screen);
    lv_obj_set_size(egg_bar_, 120, 10);
    lv_obj_align_to(egg_bar_, egg_obj_, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_bar_set_range(egg_bar_, 0, 15); // Hatch time: 15 seconds
    lv_bar_set_value(egg_bar_, 0, LV_ANIM_OFF);
    lv_obj_add_flag(egg_bar_, LV_OBJ_FLAG_HIDDEN);

    // Criação do Canvas do Rosto OLED (256x128 pixels, escala 2x)
    face_canvas_ = lv_canvas_create(screen);
#if LVGL_VERSION_MAJOR >= 9
    // Em LVGL 9, lv_draw_buf_create cuida de todo o alinhamento e stride perfeitamente
    lv_draw_buf_t* draw_buf = lv_draw_buf_create(256, 128, LV_COLOR_FORMAT_NATIVE, LV_STRIDE_AUTO);
    if (draw_buf) {
        lv_canvas_set_draw_buf(face_canvas_, draw_buf);
        lv_obj_align(face_canvas_, LV_ALIGN_CENTER, 0, -32);
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
        lv_obj_align(face_canvas_, LV_ALIGN_CENTER, 0, -32);
        lv_obj_add_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);
    } else {
        ESP_LOGE(TAG, "Falha ao alocar Canvas! Desativando rosto.");
        face_canvas_ = nullptr;
    }
#endif
    InicializarParticulas();

    // Cria a barra do timer
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
    lv_obj_add_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);

    // Timer para rodar as animações fluidas dos olhos (a cada 30ms)
    eye_timer_ = lv_timer_create(EyeTimerCallback, 30, this);
}

void LcdDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        ESP_LOGE(TAG, "Preview image is not initialized");
        return;
    }

    if (image == nullptr) {
        esp_timer_stop(preview_timer_);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        preview_image_cached_.reset();
        if (gif_controller_) {
            gif_controller_->Start();
        }
        return;
    }

    preview_image_cached_ = std::move(image);
    auto img_dsc = preview_image_cached_->image_dsc();
    lv_image_set_src(preview_image_, img_dsc);
    if (img_dsc->header.w > 0 && img_dsc->header.h > 0) {
        // zoom factor 0.5
        lv_image_set_scale(preview_image_, 128 * width_ / img_dsc->header.w);
    }

    // Hide emoji_box_
    if (gif_controller_) {
        gif_controller_->Stop();
    }
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    esp_timer_stop(preview_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, PREVIEW_IMAGE_DURATION_MS * 1000));
}

void LcdDisplay::SetChatMessage(const char* role, const char* content) {
    if (!setup_ui_called_) {
        ESP_LOGW(TAG, "SetChatMessage('%s', '%s') called before SetupUI() - message will be lost!", role, content);
    }
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        if (setup_ui_called_) {
            ESP_LOGW(TAG, "SetChatMessage('%s', '%s') failed: chat_message_label_ is nullptr (SetupUI() was called but label not created)", role, content);
        }
        return;
    }
    lv_label_set_text(chat_message_label_, content);
    // Show bottom_bar_ only when there is content (and subtitle is not globally hidden)
    if (bottom_bar_ != nullptr) {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        } else if (!hide_subtitle_) {
            lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        }
    }
#if CONFIG_USE_MULTILINE_CHAT_MESSAGE
    // Re-align bottom_bar_ after text change so it stays anchored to the bottom
    // as its height adapts to the wrapped content.
    if (bottom_bar_ != nullptr) {
        lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);
    }
#endif
}

void LcdDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    // In non-wechat mode, just clear the chat message label and hide the bar
    if (chat_message_label_ != nullptr) {
        lv_label_set_text(chat_message_label_, "");
    }
    if (bottom_bar_ != nullptr) {
        lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
    }
}
#endif

void LcdDisplay::SetEmotion(const char* emotion) {
    static std::string last_emotion = "";
    if (emotion && last_emotion == emotion) {
        return; // Ignora se a emoção não mudou (evita vazamento de memória e travamento da CPU recarregando o GIF a 5 FPS via ESP-NOW)
    }
    if (emotion) last_emotion = emotion;
    
    if (!setup_ui_called_) {
        ESP_LOGW(TAG, "SetEmotion('%s') called before SetupUI() - emotion will not be displayed!", emotion);
    }
    if (emoji_image_ == nullptr) {
        if (setup_ui_called_) {
            ESP_LOGW(TAG, "SetEmotion('%s') failed: emoji_image_ is nullptr (SetupUI() was called but emoji image not created)", emotion);
        }
        return;
    }

    auto emoji_collection = static_cast<LvglTheme*>(current_theme_)->emoji_collection();
    auto image = emoji_collection != nullptr ? emoji_collection->GetEmojiImage(emotion) : nullptr;
    if (image == nullptr) {
        const char* utf8 = font_awesome_get_utf8(emotion);
        if (utf8 != nullptr && emoji_label_ != nullptr) {
            DisplayLockGuard lock(this);
            if (gif_controller_) {
                gif_controller_->Stop();
                gif_controller_.reset();
            }
            lv_label_set_text(emoji_label_, utf8);
            lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    DisplayLockGuard lock(this);
    // Stop any running GIF animation in the same lock scope as setting new image
    // to prevent LVGL from accessing freed image data between operations
    if (gif_controller_) {
        gif_controller_->Stop();
        gif_controller_.reset();
    }
    if (image->IsGif()) {
        // Create new GIF controller
        gif_controller_ = std::make_unique<LvglGif>(image->image_dsc());
        
        if (gif_controller_->IsLoaded()) {
            // Set up frame update callback
            gif_controller_->SetFrameCallback([this]() {
                lv_image_set_src(emoji_image_, gif_controller_->image_dsc());
            });
            
            // Set initial frame and start animation
            lv_image_set_src(emoji_image_, gif_controller_->image_dsc());
            gif_controller_->Start();
            
            // Show GIF, hide others
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        } else {
            ESP_LOGE(TAG, "Failed to load GIF for emotion: %s", emotion);
            gif_controller_.reset();
        }
    } else {
        lv_image_set_src(emoji_image_, image->image_dsc());
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }

#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    // In WeChat message style, if emotion is neutral, don't display it
    uint32_t child_count = lv_obj_get_child_cnt(content_);
    if (strcmp(emotion, "neutral") == 0 && child_count > 0) {
        // Stop GIF animation if running
        if (gif_controller_) {
            gif_controller_->Stop();
            gif_controller_.reset();
        }
        
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
#endif
}

void LcdDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);
    
    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    
    // Get the active screen
    lv_obj_t* screen = lv_screen_active();

    // Set font
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    if (text_font->line_height >= 40) {
        lv_obj_set_style_text_font(mute_label_, large_icon_font, 0);
        lv_obj_set_style_text_font(battery_label_, large_icon_font, 0);
        lv_obj_set_style_text_font(network_label_, large_icon_font, 0);
    } else {
        lv_obj_set_style_text_font(mute_label_, icon_font, 0);
        lv_obj_set_style_text_font(battery_label_, icon_font, 0);
        lv_obj_set_style_text_font(network_label_, icon_font, 0);
    }

    // Set parent text color
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lvgl_theme->text_color(), 0);

    // Set background image
    if (lvgl_theme->background_image() != nullptr) {
        lv_obj_set_style_bg_image_src(container_, lvgl_theme->background_image()->image_dsc(), 0);
    } else {
        lv_obj_set_style_bg_image_src(container_, nullptr, 0);
        lv_obj_set_style_bg_color(container_, lvgl_theme->background_color(), 0);
    }
    
    // Update top bar background color with 50% opacity
    if (top_bar_ != nullptr) {
        lv_obj_set_style_bg_opa(top_bar_, LV_OPA_50, 0);
        lv_obj_set_style_bg_color(top_bar_, lvgl_theme->background_color(), 0);
    }
    
    // Update status bar elements
    lv_obj_set_style_text_color(network_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(status_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(notification_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(mute_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(battery_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);

    // If we have the chat message style, update all message bubbles
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    // Set content background opacity
    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);

    // Iterate through all children of content (message containers or bubbles)
    uint32_t child_count = lv_obj_get_child_cnt(content_);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* obj = lv_obj_get_child(content_, i);
        if (obj == nullptr) continue;
        
        lv_obj_t* bubble = nullptr;
        
        // Check if this object is a container or bubble
        // If it's a container (user or system message), get its child as bubble
        // If it's a bubble (assistant message), use it directly
        if (lv_obj_get_child_cnt(obj) > 0) {
            // Might be a container, check if it's a user or system message container
            // User and system message containers are transparent
            lv_opa_t bg_opa = lv_obj_get_style_bg_opa(obj, LV_PART_MAIN);
            if (bg_opa == LV_OPA_TRANSP) {
                // This is a user or system message container
                bubble = lv_obj_get_child(obj, 0);
            } else {
                // This might be an assistant message bubble itself
                bubble = obj;
            }
        } else {
            // No child elements, might be other UI elements, skip
            continue;
        }
        
        if (bubble == nullptr) continue;
        
        // Use saved user data to identify bubble type
        void* bubble_type_ptr = lv_obj_get_user_data(bubble);
        if (bubble_type_ptr != nullptr) {
            const char* bubble_type = static_cast<const char*>(bubble_type_ptr);
            
            // Apply correct color based on bubble type
            if (strcmp(bubble_type, "user") == 0) {
                lv_obj_set_style_bg_color(bubble, lvgl_theme->user_bubble_color(), 0);
            } else if (strcmp(bubble_type, "assistant") == 0) {
                lv_obj_set_style_bg_color(bubble, lvgl_theme->assistant_bubble_color(), 0); 
            } else if (strcmp(bubble_type, "system") == 0) {
                lv_obj_set_style_bg_color(bubble, lvgl_theme->system_bubble_color(), 0);
            } else if (strcmp(bubble_type, "image") == 0) {
                lv_obj_set_style_bg_color(bubble, lvgl_theme->system_bubble_color(), 0);
            }
            
            // Update border color
            lv_obj_set_style_border_color(bubble, lvgl_theme->border_color(), 0);
            
            // Update text color for the message
            if (lv_obj_get_child_cnt(bubble) > 0) {
                lv_obj_t* text = lv_obj_get_child(bubble, 0);
                if (text != nullptr) {
                    // Set text color based on bubble type
                    if (strcmp(bubble_type, "system") == 0) {
                        lv_obj_set_style_text_color(text, lvgl_theme->system_text_color(), 0);
                    } else {
                        lv_obj_set_style_text_color(text, lvgl_theme->text_color(), 0);
                    }
                }
            }
        } else {
            ESP_LOGW(TAG, "child[%lu] Bubble type is not found", i);
        }
    }
#else
    // Simple UI mode - just update the main chat message
    if (chat_message_label_ != nullptr) {
        lv_obj_set_style_text_color(chat_message_label_, lvgl_theme->text_color(), 0);
    }
    
    if (emoji_label_ != nullptr) {
        lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);
    }
    
    // Update bottom bar background color with 50% opacity
    if (bottom_bar_ != nullptr) {
        lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_50, 0);
        lv_obj_set_style_bg_color(bottom_bar_, lvgl_theme->background_color(), 0);
    }
#endif
    
    // Update low battery popup
    lv_obj_set_style_bg_color(low_battery_popup_, lvgl_theme->low_battery_color(), 0);

    // No errors occurred. Save theme to settings
    Display::SetTheme(lvgl_theme);
}

void LcdDisplay::SetHideSubtitle(bool hide) {
    DisplayLockGuard lock(this);
    hide_subtitle_ = hide;
    
    // Immediately update UI visibility based on the setting
    if (bottom_bar_ != nullptr) {
        if (hide) {
            lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        } else {
            // Only show if there is actual content to display
            const char* text = (chat_message_label_ != nullptr) ? lv_label_get_text(chat_message_label_) : nullptr;
            if (text != nullptr && text[0] != '\0') {
                lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void LcdDisplay::UpdateStatusBar(bool update_all) {
    LvglDisplay::UpdateStatusBar(update_all);
    
    DisplayLockGuard lock(this); // Proteção ABSOLUTA do ovo na LVGL
    
    // Atualiza o ovo e o progresso
    auto& engine = TamagotchiEngine::GetInstance();
    auto estado = engine.GetEstadoNascimento();
    ESP_LOGI(TAG, "UpdateStatusBar: estado=%d, segundos=%d", (int)estado, engine.GetSegundosChocados());
    
    auto state = Application::GetInstance().GetDeviceState();
    bool connected = (state != kDeviceStateWifiConfiguring && state != kDeviceStateStarting);
    if (engine.IsAlarmeAtivo()) {
        // Se o alarme estiver disparado, esconde ovo e emoji_box e exibe o canvas (despertador)
        if (emoji_box_ != nullptr) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        if (egg_obj_ != nullptr) lv_obj_add_flag(egg_obj_, LV_OBJ_FLAG_HIDDEN);
        if (egg_bar_ != nullptr) lv_obj_add_flag(egg_bar_, LV_OBJ_FLAG_HIDDEN);
        if (face_canvas_ != nullptr) lv_obj_remove_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);
    } else if (estado != ESTADO_NASCIDO && connected) {
        // Se ainda não nasceu, esconde emoji_box e os widgets legados e exibe o face_canvas_ para o ovo cósmico fluido
        if (emoji_box_ != nullptr) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        if (egg_obj_ != nullptr) lv_obj_add_flag(egg_obj_, LV_OBJ_FLAG_HIDDEN);
        if (egg_bar_ != nullptr) lv_obj_add_flag(egg_bar_, LV_OBJ_FLAG_HIDDEN);
        if (face_canvas_ != nullptr) lv_obj_remove_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Esconde o ovo e a barra
        if (egg_obj_ != nullptr) lv_obj_add_flag(egg_obj_, LV_OBJ_FLAG_HIDDEN);
        if (egg_bar_ != nullptr) lv_obj_add_flag(egg_bar_, LV_OBJ_FLAG_HIDDEN);
        
        // Esconde o emoji_box_ padrão (pois usaremos os olhos LVGL fluidos)
        if (emoji_box_ != nullptr) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        
        // Exibe o rosto dinâmico fluidos
        if (face_canvas_ != nullptr) lv_obj_remove_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);
    }

    if (timer_label_ != nullptr && timer_bar_ != nullptr) {
        lv_obj_add_flag(timer_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(timer_bar_, LV_OBJ_FLAG_HIDDEN);
    }
}

void LcdDisplay::EyeTimerCallback(lv_timer_t* timer) {
    LcdDisplay* display = static_cast<LcdDisplay*>(lv_timer_get_user_data(timer));
    display->UpdateEyeAnimations();
}

void LcdDisplay::InicializarParticulas() {
    for (int i = 0; i < 15; i++) {
        particulasFisicas_[i].ativa = false;
    }
}

void LcdDisplay::CriarParticula(float x, float y, float vx, float vy, char tipo, int maxVida) {
    for (int i = 0; i < 15; i++) {
        if (!particulasFisicas_[i].ativa) {
            particulasFisicas_[i].x = x;
            particulasFisicas_[i].y = y;
            particulasFisicas_[i].vx = vx;
            particulasFisicas_[i].vy = vy;
            particulasFisicas_[i].tipo = tipo;
            particulasFisicas_[i].vida = maxVida;
            particulasFisicas_[i].maxVida = maxVida;
            particulasFisicas_[i].ativa = true;
            break;
        }
    }
}

void LcdDisplay::AtualizarParticulas() {
    for (int i = 0; i < 15; i++) {
        if (particulasFisicas_[i].ativa) {
            particulasFisicas_[i].x += particulasFisicas_[i].vx;
            particulasFisicas_[i].y += particulasFisicas_[i].vy;
            if (particulasFisicas_[i].tipo == 'S' || particulasFisicas_[i].tipo == 'L') {
                particulasFisicas_[i].vy += 0.04;
            } else if (particulasFisicas_[i].tipo == 'Z') {
                particulasFisicas_[i].vx += (sin(esp_timer_get_time() / 100000.0) * 0.03);
            } else {
                particulasFisicas_[i].vy -= 0.025;
                particulasFisicas_[i].vx += (sin(esp_timer_get_time() / 50000.0 + i) * 0.06);
            }
            particulasFisicas_[i].vida--;
            if (particulasFisicas_[i].vida <= 0 || particulasFisicas_[i].y < 0 || particulasFisicas_[i].y > 64 || particulasFisicas_[i].x < 0 || particulasFisicas_[i].x > 128) {
                particulasFisicas_[i].ativa = false;
            }
        }
    }
}

void LcdDisplay::DesenharParticulas(int xOffset, lv_layer_t* layer) {
    if (!face_canvas_ || !layer) return;
    for (int i = 0; i < 15; i++) {
        if (particulasFisicas_[i].ativa) {
            int px = ((int)particulasFisicas_[i].x + xOffset) * 2;
            int py = ((int)particulasFisicas_[i].y) * 2;
            if (particulasFisicas_[i].tipo == 'H') {
                // Corações flutuantes em magenta/rosa vibrante
                DrawHeart(px/2, py/2, layer, lv_color_hex(0xFF1493));
            } else if (particulasFisicas_[i].tipo == '+') {
                // Cruz de cura verde esmeralda com núcleo brilhante
                draw_canvas_line(layer, px, py-3, px, py+3, lv_color_hex(0x00E676), 3);
                draw_canvas_line(layer, px-3, py, px+3, py, lv_color_hex(0x00E676), 3);
                draw_canvas_disc(layer, px, py, 1, lv_color_hex(0xFFFFFF));
            } else if (particulasFisicas_[i].tipo == '*') {
                // Farelos de comida / faíscas douradas brilhantes
                draw_canvas_line(layer, px-3, py-3, px+3, py+3, lv_color_hex(0xFFD700), 2);
                draw_canvas_line(layer, px-3, py+3, px+3, py-3, lv_color_hex(0xFFD700), 2);
                draw_canvas_disc(layer, px, py, 1, lv_color_hex(0xFFFFFF));
            } else if (particulasFisicas_[i].tipo == 'Z') {
                // 'Z' sonolento em lilás/violeta néon
                lv_color_t zColor = lv_color_hex(0xB388FF);
                draw_canvas_line(layer, px-4, py-4, px+4, py-4, zColor, 2);
                draw_canvas_line(layer, px+4, py-4, px-4, py+4, zColor, 2);
                draw_canvas_line(layer, px-4, py+4, px+4, py+4, zColor, 2);
            } else if (particulasFisicas_[i].tipo == 'S') {
                // Gota de suor em azul-celeste glacial com reflexo
                draw_canvas_disc(layer, px, py, 3, lv_color_hex(0x80D8FF));
                draw_canvas_triangle(layer, px-2, py, px+2, py, px, py-4, lv_color_hex(0x80D8FF));
                draw_canvas_disc(layer, px-1, py-1, 1, lv_color_hex(0xFFFFFF));
            } else if (particulasFisicas_[i].tipo == 'L') {
                // Lágrima viva em azul oceânico brilhante com formato de gota
                draw_canvas_disc(layer, px, py, 4, lv_color_hex(0x00B0FF));
                draw_canvas_triangle(layer, px-3, py, px+3, py, px, py-6, lv_color_hex(0x00B0FF));
                draw_canvas_disc(layer, px-1, py-1, 1, lv_color_hex(0xFFFFFF));
            } else if (particulasFisicas_[i].tipo == 'M') {
                // Nota musical em amarelo ouro vivo
                lv_color_t noteColor = lv_color_hex(0xFFD600);
                draw_canvas_disc(layer, px, py, 4, noteColor);
                draw_canvas_line(layer, px + 2, py - 9, px + 2, py, noteColor, 2);
                draw_canvas_line(layer, px + 2, py - 9, px + 7, py - 6, noteColor, 2);
            } else if (particulasFisicas_[i].tipo == '$') {
                // Moeda dourada com cifrão esmeralda (Banco Imobiliário / Jogo da Vida)
                draw_canvas_disc(layer, px, py, 5, lv_color_hex(0xFFD700));
                draw_canvas_disc(layer, px, py, 3, lv_color_hex(0x10B981));
                draw_canvas_line(layer, px, py - 4, px, py + 4, lv_color_hex(0xFFFFFF), 1);
            } else if (particulasFisicas_[i].tipo == 'F') {
                // Chama de fogo picante (Pimenta Malagueta)
                draw_canvas_triangle(layer, px - 5, py + 3, px + 5, py + 3, px, py - 7, lv_color_hex(0xFF3D00));
                draw_canvas_triangle(layer, px - 3, py + 3, px + 3, py + 3, px, py - 3, lv_color_hex(0xFFEA00));
            } else if (particulasFisicas_[i].tipo == 'I') {
                // Cristal de gelo ciano (Sorvete / Brain Freeze)
                lv_color_t iceCol = lv_color_hex(0x00E5FF);
                draw_canvas_line(layer, px - 4, py, px + 4, py, iceCol, 2);
                draw_canvas_line(layer, px, py - 4, px, py + 4, iceCol, 2);
                draw_canvas_disc(layer, px, py, 1, lv_color_hex(0xFFFFFF));
            } else if (particulasFisicas_[i].tipo == 'C') {
                // Confete colorido de festa / vitória
                static const uint32_t kConfettiHex[4] = {0xFF4081, 0x00E5FF, 0xFFD740, 0x69F0AE};
                lv_color_t cCol = lv_color_hex(kConfettiHex[i & 3]);
                draw_canvas_rect(layer, px - 3, py - 2, 6, 4, cCol, 1);
            }
        }
    }
}

void LcdDisplay::DrawEye(float x, float y, float w, float h, float r, lv_layer_t* layer, 
                         lv_color_t color, lv_color_t auraColor, bool withHighlights, 
                         float lookDx, float lookDy) {
    if (!face_canvas_ || !layer) return;
    int px = (int)(x * 2.0f); 
    int py = (int)(y * 2.0f); 
    int pw = (int)(w * 2.0f); 
    int ph = (int)(h * 2.0f);
    int pr = (int)(r * 2.0f);
    if (ph < 2) ph = 2;
    if (pw < 2) pw = 2;
    if (pr > ph / 2) pr = ph / 2;
    if (pr > pw / 2) pr = pw / 2;

    // 1. Camada de Aura Exterior Suave (Glow)
    if (h > 6.0f) {
        int auraPad = 2;
        draw_canvas_rect_empty(layer, px - pw/2 - auraPad, py - ph/2 - auraPad, 
                               pw + auraPad*2, ph + auraPad*2, auraColor, 2, pr + auraPad);
    }
    
    // 2. Corpo Principal Colorido da Íris
    draw_canvas_rect(layer, px - pw/2, py - ph/2, pw, ph, color, pr);
    
    // 3. Pupila sutil interna de profundidade acompanhando a direção do olhar
    if (ph >= 16 && pw >= 12) {
        int pupilW = pw * 55 / 100;
        int pupilH = ph * 60 / 100;
        int pupilR = pr * 55 / 100;
        int pupShiftX = (int)(lookDx * 0.4f);
        int pupShiftY = (int)(lookDy * 0.4f);
        draw_canvas_rect(layer, px - pupilW/2 + pupShiftX, py - pupilH/2 + pupShiftY, pupilW, pupilH, lv_color_hex(0x000000), pupilR);
    }

    // 4. Brilho Especular Duplo (Catchlights / Reflexos de Luz Vivos)
    if (withHighlights && ph >= 10 && pw >= 8) {
        // Brilho primário (superior direito)
        int h1Size = std::max(2, pw / 5);
        int h1X = px + pw/5 + (int)(lookDx * 0.25f);
        int h1Y = py - ph/4 + (int)(lookDy * 0.25f);
        draw_canvas_disc(layer, h1X, h1Y, h1Size, lv_color_hex(0xFFFFFF));
        
        // Brilho secundário (inferior esquerdo, mais sutil)
        if (ph >= 18) {
            int h2Size = std::max(1, pw / 9);
            int h2X = px - pw/4 + (int)(lookDx * 0.15f);
            int h2Y = py + ph/5 + (int)(lookDy * 0.15f);
            draw_canvas_disc(layer, h2X, h2Y, h2Size, lv_color_hex(0xFFFFFF));
        }
    }
}

void LcdDisplay::DrawEyeHappy(float x, float y, float w, float h, float r, float progress, lv_layer_t* layer, 
                              lv_color_t color, lv_color_t auraColor) {
    if (!face_canvas_ || !layer) return;
    progress = std::max(0.0f, std::min(1.0f, progress));
    int px = (int)(x * 2.0f); 
    int py = (int)(y * 2.0f); 
    int pw = (int)(w * 2.0f); 
    int ph = (int)(h * 2.0f);
    
    if (progress < 0.25f) {
        float factor = progress / 0.25f;
        float curH = h * (1.0f - factor * 0.4f);
        DrawEye(x, y, w, curH, r, layer, color, auraColor, false);
    } else {
        float arcProgress = (progress - 0.25f) / 0.75f;
        int arcRadius = (pw / 2);
        int offsetY = (int)((ph / 4) * arcProgress);
        // Aura sutil exterior
        draw_canvas_arc(layer, px, py + offsetY, arcRadius + 2, 180, 360, auraColor, 7);
        // Arco principal colorido com espessura de 5px
        draw_canvas_arc(layer, px, py + offsetY, arcRadius, 180, 360, color, 5);
        // Brilho especular nas pontas do arco
        draw_canvas_disc(layer, px - arcRadius, py + offsetY, 2, lv_color_hex(0xFFFFFF));
        draw_canvas_disc(layer, px + arcRadius, py + offsetY, 2, lv_color_hex(0xFFFFFF));
    }
}

void LcdDisplay::DrawEyeSqueezed(float x, float y, float w, float h, float r, float progress, bool isLeft, lv_layer_t* layer, 
                                 lv_color_t color) {
    if (!face_canvas_ || !layer) return;
    int px = (int)(x * 2.0f); 
    int py = (int)(y * 2.0f); 
    int pw = (int)(w * 2.0f);
    if (isLeft) {
        draw_canvas_line(layer, px - pw/2, py - pw/2, px + pw/2, py, color, 4);
        draw_canvas_line(layer, px - pw/2, py + pw/2, px + pw/2, py, color, 4);
    } else {
        draw_canvas_line(layer, px + pw/2, py - pw/2, px - pw/2, py, color, 4);
        draw_canvas_line(layer, px + pw/2, py + pw/2, px - pw/2, py, color, 4);
    }
}

void LcdDisplay::DrawHeart(int x, int y, lv_layer_t* layer, lv_color_t color) {
    if (!face_canvas_ || !layer) return;
    int px = x * 2; 
    int py = y * 2;
    draw_canvas_disc(layer, px - 2, py - 2, 2, color);
    draw_canvas_disc(layer, px + 2, py - 2, 2, color);
    draw_canvas_triangle(layer, px - 4, py - 1, px + 4, py - 1, px, py + 4, color);
}

void LcdDisplay::DrawLargeHeart(int x, int y, bool small, lv_layer_t* layer, lv_color_t color) {
    if (!face_canvas_ || !layer) return;
    int px = x * 2; 
    int py = y * 2;
    int size = small ? 5 : 9;
    lv_color_t aura = lv_color_hex(0x880E4F);
    
    // Aura externa
    draw_canvas_arc(layer, px - size, py - size, size + 1, 135, 315, aura, 5);
    draw_canvas_arc(layer, px + size, py - size, size + 1, 225, 45, aura, 5);
    
    // Formato principal do coração
    draw_canvas_arc(layer, px - size, py - size, size, 135, 315, color, 4);
    draw_canvas_arc(layer, px + size, py - size, size, 225, 45, color, 4);
    draw_canvas_line(layer, px - size*2, py, px, py + size*2, color, 4);
    draw_canvas_line(layer, px + size*2, py, px, py + size*2, color, 4);
    
    // Preenchimento central e reflexo
    draw_canvas_disc(layer, px - size/2, py - size/2, size/2, color);
    draw_canvas_disc(layer, px + size/2, py - size/2, size/2, color);
    draw_canvas_triangle(layer, px - size, py, px + size, py, px, py + size*2 - 2, color);
    draw_canvas_disc(layer, px - size/2 - 1, py - size/2 - 1, 2, lv_color_hex(0xFFFFFF));
}

void LcdDisplay::DrawEyeDizzy(float x, float y, float radius, float angle, lv_color_t color, lv_layer_t* layer) {
    if (!face_canvas_ || !layer) return;
    int px = (int)(x * 2.0f);
    int py = (int)(y * 2.0f);
    float r = radius * 2.0f;
    
    float dx1 = r * cos(angle);
    float dy1 = r * sin(angle);
    float dx2 = -r * sin(angle);
    float dy2 = r * cos(angle);
    
    // Cruzes giratórias com pontas grossas e centro branco
    draw_canvas_line(layer, px - (int)dx1, py - (int)dy1, px + (int)dx1, py + (int)dy1, color, 5);
    draw_canvas_line(layer, px - (int)dx2, py - (int)dy2, px + (int)dx2, py + (int)dy2, color, 5);
    draw_canvas_disc(layer, px, py, 3, lv_color_hex(0xFFFFFF));
}

void LcdDisplay::DrawCheekBlush(float x, float y, float radiusX, float radiusY, lv_color_t color, lv_layer_t* layer) {
    if (!face_canvas_ || !layer) return;
    int px = (int)(x * 2.0f);
    int py = (int)(y * 2.0f);
    int pw = (int)(radiusX * 2.0f * 2.0f);
    int ph = (int)(radiusY * 2.0f);
    if (ph < 2) ph = 2;
    if (pw < 2) pw = 2;
    draw_canvas_rect(layer, px - pw/2, py - ph/2, pw, ph, color, ph/2);
}

void LcdDisplay::DrawStarDirect(int px, int py, int r, lv_color_t color, lv_layer_t* layer) {
    if (!face_canvas_ || !layer) return;
    if (r < 2) r = 2;
    // Proteção contra estouro de limites do canvas 256x128
    if (px < r || px >= (256 - r) || py < r || py >= (128 - r)) return;
    
    draw_canvas_line(layer, px, py - r, px, py + r, color, 3);
    draw_canvas_line(layer, px - r, py, px + r, py, color, 3);
    int diag = r * 6 / 10;
    draw_canvas_line(layer, px - diag, py - diag, px + diag, py + diag, color, 2);
    draw_canvas_line(layer, px - diag, py + diag, px + diag, py - diag, color, 2);
    draw_canvas_disc(layer, px, py, 2, lv_color_hex(0xFFFFFF));
}

void LcdDisplay::DrawStar(float x, float y, float radius, lv_color_t color, lv_layer_t* layer) {
    int px = (int)(x * 2.0f);
    int py = (int)(y * 2.0f);
    int r = (int)(radius * 2.0f);
    DrawStarDirect(px, py, r, color, layer);
}

void LcdDisplay::DrawTimerSegmentChar(lv_layer_t* layer, int x, int y, char c, lv_color_t fgColor, lv_color_t outlineColor) {
    if (!face_canvas_ || !layer) return;

    if (c == ':') {
        int dotX = x + 3;
        // Outline (preto)
        draw_canvas_disc(layer, dotX, y + 4, 2, outlineColor);
        draw_canvas_disc(layer, dotX, y + 10, 2, outlineColor);
        // Core (branco)
        draw_canvas_disc(layer, dotX, y + 4, 1, fgColor);
        draw_canvas_disc(layer, dotX, y + 10, 1, fgColor);
        return;
    }

    uint8_t mask = 0;
    switch (c) {
        case '0': mask = 0b00111111; break; // a,b,c,d,e,f
        case '1': mask = 0b00000110; break; // b,c
        case '2': mask = 0b01011011; break; // a,b,g,e,d
        case '3': mask = 0b01001111; break; // a,b,g,c,d
        case '4': mask = 0b01100110; break; // f,g,b,c
        case '5': mask = 0b01101101; break; // a,f,g,c,d
        case '6': mask = 0b01111101; break; // a,f,g,e,c,d
        case '7': mask = 0b00000111; break; // a,b,c
        case '8': mask = 0b01111111; break; // a,b,c,d,e,f,g
        case '9': mask = 0b01101111; break; // a,b,c,d,f,g
        default: return;
    }

    // Coordenadas dos 7 segmentos ampliados (w=8, h=14)
    struct SegCoord { int x1, y1, x2, y2; };
    SegCoord segs[7] = {
        { x + 1, y,      x + 7, y      }, // 0: a (topo)
        { x + 7, y,      x + 7, y + 7  }, // 1: b (sup dir)
        { x + 7, y + 7,  x + 7, y + 14 }, // 2: c (inf dir)
        { x + 1, y + 14, x + 7, y + 14 }, // 3: d (base)
        { x,     y + 7,  x,     y + 14 }, // 4: e (inf esq)
        { x,     y,      x,     y + 7  }, // 5: f (sup esq)
        { x + 1, y + 7,  x + 7, y + 7  }  // 6: g (meio)
    };

    // 1º passo: contorno nítido de 3px para alto contraste sem borrar
    for (int i = 0; i < 7; i++) {
        if (mask & (1 << i)) {
            draw_canvas_line(layer, segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2, outlineColor, 3);
        }
    }
    // 2º passo: núcleo nítido de 2px
    for (int i = 0; i < 7; i++) {
        if (mask & (1 << i)) {
            draw_canvas_line(layer, segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2, fgColor, 2);
        }
    }
}

void LcdDisplay::DrawAlarmClock(lv_layer_t* layer) {
    if (!face_canvas_ || !layer) return;
    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
    int cx = 128;
    int cy = 60;
    
    // Animação de tremor rápido (tocando)
    int vibX = (ms % 80 < 40) ? -3 : 3;
    int vibY = (ms % 60 < 30) ? -2 : 2;
    int hammerX = (ms % 60 < 30) ? -6 : 6;
    
    // 1. Pezinhos do despertador
    draw_canvas_line(layer, cx - 26 + vibX, cy + 32 + vibY, cx - 40 + vibX, cy + 50 + vibY, lv_color_hex(0x9E9E9E), 6);
    draw_canvas_disc(layer, cx - 40 + vibX, cy + 50 + vibY, 4, lv_color_hex(0x757575));
    draw_canvas_line(layer, cx + 26 + vibX, cy + 32 + vibY, cx + 40 + vibX, cy + 50 + vibY, lv_color_hex(0x9E9E9E), 6);
    draw_canvas_disc(layer, cx + 40 + vibX, cy + 50 + vibY, 4, lv_color_hex(0x757575));
    
    // 2. Martelo entre os sinos
    draw_canvas_line(layer, cx + vibX, cy - 36 + vibY, cx + hammerX + vibX, cy - 48 + vibY, lv_color_hex(0x757575), 4);
    draw_canvas_disc(layer, cx + hammerX + vibX, cy - 48 + vibY, 4, lv_color_hex(0xD32F2F));
    
    // 3. Sinos superiores (dourados com reflexo)
    // Sino esquerdo
    draw_canvas_disc(layer, cx - 34 + vibX, cy - 32 + vibY, 15, lv_color_hex(0xFBC02D));
    draw_canvas_disc(layer, cx - 36 + vibX, cy - 35 + vibY, 4, lv_color_hex(0xFFF9C4));
    draw_canvas_arc(layer, cx - 34 + vibX, cy - 32 + vibY, 15, 120, 310, lv_color_hex(0xF57F17), 2);
    // Sino direito
    draw_canvas_disc(layer, cx + 34 + vibX, cy - 32 + vibY, 15, lv_color_hex(0xFBC02D));
    draw_canvas_disc(layer, cx + 32 + vibX, cy - 35 + vibY, 4, lv_color_hex(0xFFF9C4));
    draw_canvas_arc(layer, cx + 34 + vibX, cy - 32 + vibY, 15, 230, 60, lv_color_hex(0xF57F17), 2);
    
    // 4. Ondas sonoras / vibração ao redor dos sinos
    draw_canvas_arc(layer, cx - 44 + vibX, cy - 34 + vibY, 16, 110, 250, lv_color_hex(0xFFEB3B), 3);
    draw_canvas_arc(layer, cx - 44 + vibX, cy - 34 + vibY, 23, 110, 250, lv_color_hex(0xFF9800), 2);
    draw_canvas_arc(layer, cx + 44 + vibX, cy - 34 + vibY, 16, 290, 70, lv_color_hex(0xFFEB3B), 3);
    draw_canvas_arc(layer, cx + 44 + vibX, cy - 34 + vibY, 23, 290, 70, lv_color_hex(0xFF9800), 2);
    
    // 5. Corpo do relógio (vermelho vivo e brilhante)
    draw_canvas_disc(layer, cx + vibX, cy + vibY, 39, lv_color_hex(0xE53935));
    draw_canvas_disc(layer, cx + vibX, cy + vibY, 36, lv_color_hex(0xC62828));
    draw_canvas_arc(layer, cx + vibX, cy + vibY, 36, 0, 360, lv_color_hex(0xFFD54F), 2);
    
    // 6. Mostrador interno (branco limpo)
    draw_canvas_disc(layer, cx + vibX, cy + vibY, 33, lv_color_hex(0xFFFFFF));
    
    // Pontos das horas (12, 3, 6, 9)
    draw_canvas_disc(layer, cx + vibX, cy - 24 + vibY, 2, lv_color_hex(0x212121));
    draw_canvas_disc(layer, cx + 24 + vibX, cy + vibY, 2, lv_color_hex(0x212121));
    draw_canvas_disc(layer, cx + vibX, cy + 24 + vibY, 2, lv_color_hex(0x212121));
    draw_canvas_disc(layer, cx - 24 + vibX, cy + vibY, 2, lv_color_hex(0x212121));
    
    // 7. Ponteiros
    // Ponteiro das horas (~10h)
    draw_canvas_line(layer, cx + vibX, cy + vibY, cx - 11 + vibX, cy - 13 + vibY, lv_color_hex(0x212121), 4);
    // Ponteiro dos minutos (~2h)
    draw_canvas_line(layer, cx + vibX, cy + vibY, cx + 15 + vibX, cy - 15 + vibY, lv_color_hex(0x212121), 3);
    // Ponteiro de segundos girando rápido
    float sAngle = ((ms % 2000) / 2000.0f) * 6.28318f;
    int sX = (cx + vibX) + (int)(22.0f * sinf(sAngle));
    int sY = (cy + vibY) - (int)(22.0f * cosf(sAngle));
    draw_canvas_line(layer, cx + vibX, cy + vibY, sX, sY, lv_color_hex(0xE53935), 2);
    
    // Centro dos ponteiros
    draw_canvas_disc(layer, cx + vibX, cy + vibY, 4, lv_color_hex(0xD32F2F));
    draw_canvas_disc(layer, cx + vibX - 1, cy + vibY - 1, 1, lv_color_hex(0xFFFFFF));
}

void LcdDisplay::DrawEggScreen(lv_layer_t* layer) {
    if (!face_canvas_ || !layer) return;
    auto& engine = TamagotchiEngine::GetInstance();
    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
    EstadoNascimento estado = engine.GetEstadoNascimento();
    
    // Animação de pulso e respiração
    float pulse = sinf(ms * 0.0035f);
    int cx = 128;
    int cy = 46;
    
    // Trepidação dinâmica durante a incubação
    int shakeX = 0;
    int shakeY = 0;
    if (estado == ESTADO_CHOCANDO) {
        if (pulse > 0.4f) {
            shakeX = (int)(sinf(ms * 0.045f) * 2.8f);
            shakeY = (int)(cosf(ms * 0.055f) * 1.5f);
        }
    }
    int eggX = cx + shakeX;
    int eggY = cy + shakeY;

    // 1. Aura cósmica exterior (néon roxo/índigo/ciano)
    draw_canvas_disc(layer, eggX, eggY + 4, 38, lv_color_hex(0x1B1838));
    draw_canvas_disc(layer, eggX, eggY - 4, 32, lv_color_hex(0x281754));

    // 2. Casca do Ovo (formato oval estilizado com partes complementares)
    draw_canvas_disc(layer, eggX, eggY + 8, 27, lv_color_hex(0x3B1F6E));
    draw_canvas_disc(layer, eggX, eggY - 6, 23, lv_color_hex(0x4A148C));
    draw_canvas_rect(layer, eggX - 25, eggY - 6, 50, 16, lv_color_hex(0x4A148C), 0);

    // 3. Núcleo de Energia Cósmica (pulso luminoso ciano/ouro)
    int coreR = 9 + (int)(pulse * 2.5f);
    draw_canvas_disc(layer, eggX, eggY + 2, coreR + 4, lv_color_hex(0x18FFFF));
    draw_canvas_disc(layer, eggX, eggY + 2, coreR, lv_color_hex(0x00E5FF));
    draw_canvas_disc(layer, eggX, eggY + 2, coreR / 2, lv_color_hex(0xFFFFFF));

    // 4. Manchas estelares decorativas na casca
    draw_canvas_disc(layer, eggX - 14, eggY + 12, 3, lv_color_hex(0xFFD700));
    draw_canvas_disc(layer, eggX + 13, eggY - 10, 2, lv_color_hex(0xFFD700));
    draw_canvas_disc(layer, eggX + 10, eggY + 16, 2, lv_color_hex(0x00E5FF));
    draw_canvas_disc(layer, eggX - 9, eggY - 14, 2, lv_color_hex(0xFF80AB));

    // 5. Reflexo especular estilo vidro curvo
    draw_canvas_arc(layer, eggX - 8, eggY - 8, 16, 120, 210, lv_color_hex(0xFFFFFF), 3);
    draw_canvas_disc(layer, eggX - 14, eggY - 14, 2, lv_color_hex(0xFFFFFF));

    // 6. Rachaduras Dinâmicas
    uint16_t segChoc = engine.GetSegundosChocados();
    uint16_t totalInc = engine.GetTempoIncubacaoSegundos();
    if (totalInc == 0) totalInc = 15;
    float prog = (estado == ESTADO_CHOCANDO) ? ((float)segChoc / (float)totalInc) : 0.0f;
    if (prog > 1.0f) prog = 1.0f;

    if (estado == ESTADO_CHOCANDO && prog >= 0.20f) {
        // Estágio 1: Rachadura primária reluzente (dourada e branca)
        draw_canvas_line(layer, eggX - 2, eggY - 12, eggX + 7, eggY + 3, lv_color_hex(0xFFD700), 3);
        draw_canvas_line(layer, eggX + 7, eggY + 3, eggX - 5, eggY + 16, lv_color_hex(0xFFD700), 3);
        draw_canvas_line(layer, eggX - 2, eggY - 12, eggX + 7, eggY + 3, lv_color_hex(0xFFFFFF), 1);
        draw_canvas_line(layer, eggX + 7, eggY + 3, eggX - 5, eggY + 16, lv_color_hex(0xFFFFFF), 1);
    }
    if (estado == ESTADO_CHOCANDO && prog >= 0.50f) {
        // Estágio 2: Rachaduras ramificadas em ciano elétrico
        draw_canvas_line(layer, eggX + 7, eggY + 3, eggX + 19, eggY + 8, lv_color_hex(0x00FFFF), 3);
        draw_canvas_line(layer, eggX - 5, eggY + 16, eggX - 17, eggY + 22, lv_color_hex(0x00FFFF), 3);
        draw_canvas_line(layer, eggX + 7, eggY + 3, eggX + 19, eggY + 8, lv_color_hex(0xFFFFFF), 1);
        draw_canvas_line(layer, eggX - 5, eggY + 16, eggX - 17, eggY + 22, lv_color_hex(0xFFFFFF), 1);
    }
    if (estado == ESTADO_CHOCANDO && prog >= 0.80f) {
        // Estágio 3: Fissuras profundas e faíscas cósmicas
        draw_canvas_line(layer, eggX - 2, eggY - 12, eggX - 14, eggY - 18, lv_color_hex(0xFF4081), 2);
        draw_canvas_line(layer, eggX + 7, eggY + 3, eggX + 10, eggY - 8, lv_color_hex(0xFF4081), 2);
        DrawStar((float)(eggX + 22), (float)(eggY - 14), 4.0f, lv_color_hex(0xFFD700), layer);
        DrawStar((float)(eggX - 22), (float)(eggY + 8), 3.5f, lv_color_hex(0x00E5FF), layer);
    }

    // 7. HUD Inferior: Barra de Progresso Neon + Timer Digital
    int barW = 160;
    int barH = 10;
    int barX = 128 - (barW / 2);
    int barY = 96;

    // Fundo da barra
    draw_canvas_rect(layer, barX, barY, barW, barH, lv_color_hex(0x1B1834), 5);

    // Preenchimento gradiente ciano / ouro
    int fillW = (int)((barW - 4) * prog);
    if (fillW > 0) {
        lv_color_t barFillColor = (prog >= 0.80f) ? lv_color_hex(0xFFEB3B) : lv_color_hex(0x00E5FF);
        draw_canvas_rect(layer, barX + 2, barY + 2, fillW, barH - 4, barFillColor, 3);
    }
    // Borda da barra
    draw_canvas_rect_empty(layer, barX, barY, barW, barH, lv_color_hex(0x534B82), 2, 5);

    // 8. Dígitos do Timer
    int remSeg = (estado == ESTADO_CHOCANDO) ? ((int)totalInc - (int)segChoc) : (int)totalInc;
    if (remSeg < 0) remSeg = 0;
    uint32_t tm = (uint32_t)(remSeg / 60);
    uint32_t ts = (uint32_t)(remSeg % 60);
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02u:%02u", (unsigned int)tm, (unsigned int)ts);

    int textX = 128 - 23;
    int textY = 111;
    for (int i = 0; i < 5; i++) {
        DrawTimerSegmentChar(layer, textX + i * 10, textY, timeStr[i], lv_color_hex(0xFFFFFF), lv_color_hex(0x000000));
    }

    // 9. Se for ESTADO_OVO (esperando aproximação do cartão):
    if (estado == ESTADO_OVO) {
        float waveScale = fabsf(sinf(ms * 0.003f));
        int ringR = 36 + (int)(waveScale * 6.0f);
        draw_canvas_arc(layer, eggX, eggY, ringR, 0, 360, lv_color_hex(0x80D8FF), 1);
    }
}

void LcdDisplay::DrawSingleDie(lv_layer_t* layer, int cx, int cy, int halfSize, int val, lv_color_t accentColor) {
    int x = cx - halfSize;
    int y = cy - halfSize;
    int sz = halfSize * 2;
    // Sombra 3D suave
    draw_canvas_rect(layer, x + 4, y + 5, sz, sz, lv_color_hex(0x161B26), 10);
    // Corpo branco brilhante do dado + borda neon
    draw_canvas_rect(layer, x, y, sz, sz, lv_color_hex(0xF8FAFC), 10);
    draw_canvas_rect_empty(layer, x, y, sz, sz, accentColor, 3, 10);

    int off = (halfSize * 52) / 100;
    int pipR = (halfSize >= 30) ? 5 : 4;
    lv_color_t pipCol = (val == 1 || val == 4) ? lv_color_hex(0xEF4444) : lv_color_hex(0x0F172A);

    if (val == 1 || val == 3 || val == 5) {
        draw_canvas_disc(layer, cx, cy, (val == 1) ? (pipR + 2) : pipR, pipCol);
    }
    if (val >= 2) {
        draw_canvas_disc(layer, cx - off, cy - off, pipR, pipCol);
        draw_canvas_disc(layer, cx + off, cy + off, pipR, pipCol);
    }
    if (val >= 4) {
        draw_canvas_disc(layer, cx + off, cy - off, pipR, pipCol);
        draw_canvas_disc(layer, cx - off, cy + off, pipR, pipCol);
    }
    if (val == 6) {
        draw_canvas_disc(layer, cx - off, cy, pipR, pipCol);
        draw_canvas_disc(layer, cx + off, cy, pipR, pipCol);
    }
}

void LcdDisplay::DrawDiceScene(lv_layer_t* layer) {
    if (!face_canvas_ || !layer) return;
    auto& engine = TamagotchiEngine::GetInstance();
    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
    float prog = engine.GetDiceProgress();
    int v1 = engine.GetDiceVal1();
    int v2 = engine.GetDiceVal2();
    bool dual = (v2 > 0);

    bool rolling = (prog < 0.46f);
    int disp1 = rolling ? ((int)((ms / 70) % 6) + 1) : v1;
    int disp2 = rolling ? ((int)(((ms + 190) / 65) % 6) + 1) : v2;

    int bounceY = 0;
    int shakeX = 0;
    if (rolling) {
        bounceY = (int)(-fabsf(sinf(ms * 0.018f)) * 18.0f);
        shakeX = (int)(sinf(ms * 0.045f) * 6.0f);
    } else {
        float settle = (prog - 0.46f) / 0.54f;
        if (settle < 0.35f) {
            bounceY = (int)(-sinf((settle / 0.35f) * 3.14159f) * 8.0f);
        }
    }

    lv_color_t accent = rolling ? lv_color_hex(0x00E5FF) : lv_color_hex(0xFFD700);
    static char s_dice_buf[64];
    if (!dual) {
        DrawSingleDie(layer, 128 + shakeX, 56 + bounceY, 34, disp1, accent);
        if (!rolling) {
            DrawStarDirect(74, 32, 5, lv_color_hex(0xFFD700), layer);
            DrawStarDirect(182, 36, 5, lv_color_hex(0x00E5FF), layer);
            snprintf(s_dice_buf, sizeof(s_dice_buf), "DADO: %d", v1);
            draw_canvas_text(layer, 58, 102, 140, 22, s_dice_buf, lv_color_hex(0xFFD700), &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);
        } else {
            draw_canvas_text(layer, 48, 102, 160, 22, "ROLANDO...", lv_color_hex(0x00E5FF), &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);
        }
    } else {
        DrawSingleDie(layer, 82 + shakeX, 54 + bounceY, 28, disp1, accent);
        DrawSingleDie(layer, 174 - shakeX, 54 + (bounceY / 2), 28, disp2, accent);
        if (!rolling) {
            if (v1 == v2) {
                snprintf(s_dice_buf, sizeof(s_dice_buf), "DUPLA! %d + %d = %d", v1, v2, v1 + v2);
            } else {
                snprintf(s_dice_buf, sizeof(s_dice_buf), "TOTAL: %d", v1 + v2);
            }
            draw_canvas_text(layer, 28, 98, 200, 24, s_dice_buf, lv_color_hex(0xFFD700), &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);
        } else {
            draw_canvas_text(layer, 48, 98, 160, 24, "DADOS...", lv_color_hex(0x00E5FF), &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);
        }
    }
}

void LcdDisplay::DrawWheelScene(lv_layer_t* layer) {
    if (!face_canvas_ || !layer) return;
    auto& engine = TamagotchiEngine::GetInstance();
    float prog = engine.GetWheelProgress();
    int target = engine.GetWheelVal();

    float inv = 1.0f - prog;
    float ease = 1.0f - (inv * inv * inv);
    float spinAngle = ease * 25.1327f; // ~4 voltas completas com desaceleração suave

    int cx = 128;
    int cy = 66;
    int rOuter = 48;

    // Aro externo colorido da Roleta da Vida
    draw_canvas_disc(layer, cx, cy, rOuter + 4, lv_color_hex(0x1E293B));
    draw_canvas_arc(layer, cx, cy, rOuter + 2, 0, 360, lv_color_hex(0xFFD700), 3);

    static const uint32_t kSliceCols[10] = {
        0xEF4444, 0xF97316, 0xFACC15, 0x22C55E, 0x10B981,
        0x06B6D4, 0x3B82F6, 0x6366F1, 0xA855F7, 0xEC4899
    };

    for (int i = 0; i < 10; i++) {
        float a = spinAngle + (i * 0.628318f);
        int sx = cx + (int)(cosf(a) * (rOuter - 8));
        int sy = cy + (int)(sinf(a) * (rOuter - 8));
        draw_canvas_line(layer, cx, cy, sx, sy, lv_color_hex(kSliceCols[i]), 4);
        draw_canvas_disc(layer, sx, sy, 5, lv_color_hex(kSliceCols[i]));
    }

    // Núcleo central mostrando o número da Roleta (1 a 10)
    draw_canvas_disc(layer, cx, cy, 24, lv_color_hex(0x0F172A));
    lv_color_t ringCol = (prog >= 0.55f) ? lv_color_hex(0xFFD700) : lv_color_hex(0x00E5FF);
    draw_canvas_arc(layer, cx, cy, 24, 0, 360, ringCol, 3);

    int shownVal = (prog < 0.55f) ? (((int)(ease * 40.0f) % 10) + 1) : target;
    static char s_wheel_buf[16];
    snprintf(s_wheel_buf, sizeof(s_wheel_buf), "%d", shownVal);
    draw_canvas_text(layer, cx - 22, cy - 11, 44, 22, s_wheel_buf, lv_color_hex(0xFFFFFF), &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);

    // Ponteiro dourado fixo no topo (12h)
    draw_canvas_triangle(layer, cx - 8, 8, cx + 8, 8, cx, 22, lv_color_hex(0xFFD700));
    if (prog >= 0.55f) {
        DrawStarDirect(cx - 64, 38, 5, lv_color_hex(0xFFD700), layer);
        DrawStarDirect(cx + 64, 38, 5, lv_color_hex(0x00E5FF), layer);
    }
}

void LcdDisplay::DrawCompanionScene(lv_layer_t* layer, const std::string& overlay) {
    if (!face_canvas_ || !layer) return;
    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
    int cx = 128;
    int cy = 50;
    int bobY = (int)(sinf(ms * 0.008f) * 3.0f);

    if (overlay == "jokenpo_rock") {
        // Punho fechado (Pedra)
        lv_color_t col = lv_color_hex(0x00E5FF);
        draw_canvas_disc(layer, cx, cy + bobY, 28, lv_color_hex(0x1E293B));
        draw_canvas_arc(layer, cx, cy + bobY, 28, 0, 360, col, 3);
        draw_canvas_rect(layer, cx - 18, cy - 10 + bobY, 36, 24, col, 8);
        draw_canvas_line(layer, cx - 8, cy - 10 + bobY, cx - 8, cy + 4 + bobY, lv_color_hex(0x0F172A), 2);
        draw_canvas_line(layer, cx + 2, cy - 10 + bobY, cx + 2, cy + 4 + bobY, lv_color_hex(0x0F172A), 2);
        draw_canvas_line(layer, cx + 10, cy - 10 + bobY, cx + 10, cy + 4 + bobY, lv_color_hex(0x0F172A), 2);
        draw_canvas_text(layer, 48, 94, 160, 24, "PEDRA!", col, &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);
    } else if (overlay == "jokenpo_paper") {
        // Mão aberta (Papel)
        lv_color_t col = lv_color_hex(0x10B981);
        draw_canvas_disc(layer, cx, cy + bobY, 28, lv_color_hex(0x1E293B));
        draw_canvas_arc(layer, cx, cy + bobY, 28, 0, 360, col, 3);
        draw_canvas_rect(layer, cx - 14, cy - 2 + bobY, 28, 18, col, 5);
        for (int f = -2; f <= 2; f++) {
            int fx = cx + f * 6;
            int fh = (f == 0) ? 18 : 15;
            draw_canvas_line(layer, fx, cy - 2 + bobY, fx, cy - 2 - fh + bobY, col, 4);
        }
        draw_canvas_text(layer, 48, 94, 160, 24, "PAPEL!", col, &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);
    } else if (overlay == "jokenpo_scissors") {
        // Dois dedos em V (Tesoura)
        lv_color_t col = lv_color_hex(0xFF4081);
        draw_canvas_disc(layer, cx, cy + bobY, 28, lv_color_hex(0x1E293B));
        draw_canvas_arc(layer, cx, cy + bobY, 28, 0, 360, col, 3);
        draw_canvas_rect(layer, cx - 12, cy + 2 + bobY, 24, 16, col, 6);
        draw_canvas_line(layer, cx - 5, cy + 2 + bobY, cx - 12, cy - 18 + bobY, col, 5);
        draw_canvas_line(layer, cx + 5, cy + 2 + bobY, cx + 12, cy - 18 + bobY, col, 5);
        draw_canvas_text(layer, 48, 94, 160, 24, "TESOURA!", col, &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);
    } else if (overlay == "crystal_ball") {
        // Bola de Cristal Mística (Oráculo)
        draw_canvas_rect(layer, cx - 26, 86, 52, 12, lv_color_hex(0xD97706), 4);
        draw_canvas_rect(layer, cx - 32, 96, 64, 8, lv_color_hex(0xB45309), 3);
        draw_canvas_disc(layer, cx, 52 + bobY, 34, lv_color_hex(0x3B0764));
        draw_canvas_disc(layer, cx, 52 + bobY, 28, lv_color_hex(0x7E22CE));
        int orbX = cx + (int)(cosf(ms * 0.005f) * 10.0f);
        int orbY = 52 + bobY + (int)(sinf(ms * 0.007f) * 8.0f);
        draw_canvas_disc(layer, orbX, orbY, 10, lv_color_hex(0x00E5FF));
        draw_canvas_arc(layer, cx, 52 + bobY, 34, 0, 360, lv_color_hex(0xE879F9), 2);
        draw_canvas_arc(layer, cx - 8, 44 + bobY, 18, 190, 255, lv_color_hex(0xFFFFFF), 2);
        DrawStarDirect(cx - 48, 32, 5, lv_color_hex(0xFFD700), layer);
        DrawStarDirect(cx + 48, 36, 5, lv_color_hex(0x00E5FF), layer);
    } else if (overlay == "jail") {
        // Grades de Prisão com o Robô Preso atrás das barras (256x128)
        draw_canvas_rect(layer, 16, 6, 224, 102, lv_color_hex(0x0F172A), 8);
        draw_canvas_rect(layer, 20, 10, 216, 94, lv_color_hex(0x020617), 6);
        
        // Olhos tristes/chorando do robô atrás das grades (em ciano)
        int eyeY = 46 + bobY;
        draw_canvas_rect(layer, 86, eyeY, 24, 16, lv_color_hex(0x00E5FF), 5);
        draw_canvas_rect(layer, 92, eyeY + 4, 10, 10, lv_color_hex(0x0F172A), 3);
        draw_canvas_disc(layer, 98, eyeY + 22, 3, lv_color_hex(0x38BDF8));
        draw_canvas_disc(layer, 98, eyeY + 29, 2, lv_color_hex(0x38BDF8));
        
        draw_canvas_rect(layer, 146, eyeY, 24, 16, lv_color_hex(0x00E5FF), 5);
        draw_canvas_rect(layer, 154, eyeY + 4, 10, 10, lv_color_hex(0x0F172A), 3);
        draw_canvas_disc(layer, 158, eyeY + 22, 3, lv_color_hex(0x38BDF8));
        draw_canvas_disc(layer, 158, eyeY + 29, 2, lv_color_hex(0x38BDF8));
        
        // Sobrancelhas caídas de tristeza
        draw_canvas_line(layer, 84, eyeY - 6, 110, eyeY - 2, lv_color_hex(0x00E5FF), 3);
        draw_canvas_line(layer, 146, eyeY - 2, 172, eyeY - 6, lv_color_hex(0x00E5FF), 3);
        
        // Boca curva triste invertida
        draw_canvas_arc(layer, 128, eyeY + 36, 16, 40, 140, lv_color_hex(0x00E5FF), 3);
        
        // Grades de ferro verticais pesadas
        int barPositions[] = { 40, 75, 110, 146, 181, 216 };
        for (int i = 0; i < 6; i++) {
            int bx = barPositions[i];
            draw_canvas_rect(layer, bx - 4, 8, 8, 98, lv_color_hex(0x1E293B), 2);
            draw_canvas_rect(layer, bx - 3, 8, 6, 98, lv_color_hex(0x94A3B8), 1);
            draw_canvas_line(layer, bx - 1, 8, bx - 1, 106, lv_color_hex(0xF8FAFC), 1);
        }
        
        // Travessas horizontais de aço (superior e inferior)
        draw_canvas_rect(layer, 20, 20, 216, 8, lv_color_hex(0x475569), 2);
        draw_canvas_line(layer, 20, 23, 236, 23, lv_color_hex(0xCBD5E1), 2);
        
        draw_canvas_rect(layer, 20, 84, 216, 8, lv_color_hex(0x475569), 2);
        draw_canvas_line(layer, 20, 87, 236, 87, lv_color_hex(0xCBD5E1), 2);
        
        // Rebites / Parafusos brilhantes nas junções
        for (int i = 0; i < 6; i++) {
            int bx = barPositions[i];
            draw_canvas_disc(layer, bx, 24, 2, lv_color_hex(0xF8FAFC));
            draw_canvas_disc(layer, bx, 88, 2, lv_color_hex(0xF8FAFC));
        }
        
        // Faixa de texto vermelha "NA CADEIA!"
        draw_canvas_rect(layer, 68, 108, 120, 18, lv_color_hex(0xDC2626), 4);
        draw_canvas_text(layer, 68, 110, 120, 16, "NA CADEIA!", lv_color_hex(0xFFFFFF), &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);
    }
}

void LcdDisplay::DrawAccessoryOverlay(lv_layer_t* layer, uint8_t acc_id, float eyeLx, float eyeLy, float eyeRx, float eyeRy) {
    if (!face_canvas_ || !layer || acc_id == 0) return;
    int lx = (int)(eyeLx * 2.0f);
    int ly = (int)(eyeLy * 2.0f);
    int rx = (int)(eyeRx * 2.0f);
    int ry = (int)(eyeRy * 2.0f);
    int midX = (lx + rx) / 2;
    int topY = ((ly < ry) ? ly : ry) - 28;

    if (acc_id == 1) {
        // 1: Óculos Escuros Cool ("Thug Life")
        draw_canvas_rect(layer, lx - 22, ly - 14, 44, 26, lv_color_hex(0x090D16), 4);
        draw_canvas_rect_empty(layer, lx - 22, ly - 14, 44, 26, lv_color_hex(0xE2E8F0), 2, 4);
        draw_canvas_rect(layer, rx - 22, ry - 14, 44, 26, lv_color_hex(0x090D16), 4);
        draw_canvas_rect_empty(layer, rx - 22, ry - 14, 44, 26, lv_color_hex(0xE2E8F0), 2, 4);
        draw_canvas_line(layer, lx + 22, ly - 4, rx - 22, ry - 4, lv_color_hex(0xE2E8F0), 3);
        draw_canvas_line(layer, lx - 36, ly - 6, lx - 22, ly - 6, lv_color_hex(0xE2E8F0), 3);
        draw_canvas_line(layer, rx + 22, ry - 6, rx + 36, ry - 6, lv_color_hex(0xE2E8F0), 3);
        // Reflexo branco diagonal nas lentes
        draw_canvas_line(layer, lx - 14, ly - 8, lx - 4, ly + 4, lv_color_hex(0xFFFFFF), 2);
        draw_canvas_line(layer, rx - 14, ry - 8, rx - 4, ry + 4, lv_color_hex(0xFFFFFF), 2);
    } else if (acc_id == 2) {
        // 2: Cartola de Magnata (Banco Imobiliário)
        int hy = (topY < 32) ? 32 : topY;
        draw_canvas_rect(layer, midX - 42, hy, 84, 6, lv_color_hex(0x1E293B), 3);
        draw_canvas_rect(layer, midX - 26, hy - 26, 52, 26, lv_color_hex(0x0F172A), 3);
        draw_canvas_rect_empty(layer, midX - 26, hy - 26, 52, 26, lv_color_hex(0x475569), 2, 3);
        draw_canvas_rect(layer, midX - 26, hy - 7, 52, 6, lv_color_hex(0xFFD700), 0);
    } else if (acc_id == 3) {
        // 3: Coroa Real Dourada
        int cy = (topY < 28) ? 28 : topY;
        lv_color_t gold = lv_color_hex(0xFFD700);
        draw_canvas_rect(layer, midX - 30, cy - 6, 60, 8, gold, 2);
        draw_canvas_triangle(layer, midX - 30, cy - 6, midX - 14, cy - 6, midX - 24, cy - 22, gold);
        draw_canvas_triangle(layer, midX - 12, cy - 6, midX + 12, cy - 6, midX, cy - 26, gold);
        draw_canvas_triangle(layer, midX + 14, cy - 6, midX + 30, cy - 6, midX + 24, cy - 22, gold);
        draw_canvas_disc(layer, midX, cy - 3, 3, lv_color_hex(0xEF4444));
        draw_canvas_disc(layer, midX - 18, cy - 3, 2, lv_color_hex(0x00E5FF));
        draw_canvas_disc(layer, midX + 18, cy - 3, 2, lv_color_hex(0x10B981));
    } else if (acc_id == 4) {
        // 4: Chapéu de Chef Gastronômico
        int cy = (topY < 30) ? 30 : topY;
        lv_color_t white = lv_color_hex(0xF8FAFC);
        draw_canvas_disc(layer, midX - 16, cy - 16, 12, white);
        draw_canvas_disc(layer, midX + 16, cy - 16, 12, white);
        draw_canvas_disc(layer, midX, cy - 20, 14, white);
        draw_canvas_rect(layer, midX - 24, cy - 10, 48, 10, lv_color_hex(0xE2E8F0), 2);
    } else if (acc_id == 5) {
        // 5: Óculos de Leitura / Foco Pomodoro
        lv_color_t frameCol = lv_color_hex(0x38BDF8);
        draw_canvas_arc(layer, lx, ly, 22, 0, 360, frameCol, 2);
        draw_canvas_arc(layer, rx, ry, 22, 0, 360, frameCol, 2);
        draw_canvas_line(layer, lx + 22, ly, rx - 22, ry, frameCol, 2);
    } else if (acc_id == 6) {
        // 6: Headphone Neon DJ
        lv_color_t neon = lv_color_hex(0xA855F7);
        draw_canvas_arc(layer, midX, ly - 4, 86, 195, 345, neon, 4);
        draw_canvas_rect(layer, 12, ly - 18, 14, 36, neon, 6);
        draw_canvas_rect(layer, 230, ry - 18, 14, 36, neon, 6);
        draw_canvas_rect(layer, 16, ly - 12, 6, 24, lv_color_hex(0x00E5FF), 3);
        draw_canvas_rect(layer, 234, ry - 12, 6, 24, lv_color_hex(0x00E5FF), 3);
    } else if (acc_id == 7) {
        // 7: Gravata Borboleta Neon (posicionada no colarinho/base da tela, abaixo da boca)
        int by = 122;
        lv_color_t bow = lv_color_hex(0xFF4081);
        draw_canvas_triangle(layer, midX - 16, by - 5, midX - 16, by + 5, midX, by, bow);
        draw_canvas_triangle(layer, midX + 16, by - 5, midX + 16, by + 5, midX, by, bow);
        draw_canvas_disc(layer, midX, by, 3, lv_color_hex(0xFFD700));
    }
}

void LcdDisplay::DrawFaceOverlay(lv_layer_t* layer, const std::string& overlay) {
    if (!face_canvas_ || !layer || overlay.empty()) return;
    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);

    if (overlay == "jail") {
        // Grades metálicas da Prisão (Banco Imobiliário)
        lv_color_t barDark = lv_color_hex(0x475569);
        lv_color_t barLight = lv_color_hex(0x94A3B8);
        draw_canvas_rect(layer, 0, 0, 256, 8, barDark, 0);
        draw_canvas_rect(layer, 0, 120, 256, 8, barDark, 0);
        for (int bx = 24; bx <= 232; bx += 42) {
            draw_canvas_rect(layer, bx, 0, 7, 128, barDark, 0);
            draw_canvas_line(layer, bx + 2, 0, bx + 2, 128, barLight, 2);
        }
    } else if (overlay == "dance") {
        // Barras de Equalizador RGB Neon (Modo Festa / Dança)
        static const uint32_t kEqCols[8] = {
            0xFF4081, 0xFF6E40, 0xFFD740, 0x69F0AE,
            0x00E5FF, 0x448AFF, 0xB388FF, 0xEA80FC
        };
        for (int b = 0; b < 8; b++) {
            int h = 6 + (int)(fabsf(sinf(ms * 0.012f + b * 0.9f)) * 18.0f);
            int bx = 20 + b * 28;
            draw_canvas_rect(layer, bx, 126 - h, 18, h, lv_color_hex(kEqCols[b]), 3);
        }
    } else if (overlay == "ladder_up") {
        // Setas neon verdes subindo nas laterais (Escada!)
        int offY = (int)((ms / 25) % 24);
        lv_color_t upCol = lv_color_hex(0x10B981);
        for (int k = 0; k < 3; k++) {
            int ay = 100 - k * 34 - offY;
            draw_canvas_line(layer, 20, ay, 28, ay - 8, upCol, 3);
            draw_canvas_line(layer, 28, ay - 8, 36, ay, upCol, 3);
            draw_canvas_line(layer, 220, ay, 228, ay - 8, upCol, 3);
            draw_canvas_line(layer, 228, ay - 8, 236, ay, upCol, 3);
        }
    } else if (overlay == "snake_down") {
        // Setas vermelhas descendo nas laterais (Escorregador!)
        int offY = (int)((ms / 25) % 24);
        lv_color_t dnCol = lv_color_hex(0xEF4444);
        for (int k = 0; k < 3; k++) {
            int ay = 20 + k * 34 + offY;
            draw_canvas_line(layer, 20, ay, 28, ay + 8, dnCol, 3);
            draw_canvas_line(layer, 28, ay + 8, 36, ay, dnCol, 3);
            draw_canvas_line(layer, 220, ay, 228, ay + 8, dnCol, 3);
            draw_canvas_line(layer, 228, ay + 8, 236, ay, dnCol, 3);
        }
    }
}

void LcdDisplay::DrawOledFace(int xOffset) {
    if (!face_canvas_) return;
    lv_canvas_fill_bg(face_canvas_, lv_color_black(), LV_OPA_COVER);
#if LVGL_VERSION_MAJOR >= 9
    lv_layer_t layer_obj;
    lv_canvas_init_layer(face_canvas_, &layer_obj);
    lv_layer_t* layer = &layer_obj;
#else
    lv_layer_t* layer = nullptr;
#endif
    auto& engine = TamagotchiEngine::GetInstance();
    
    // Se o alarme do despertador estiver ativo, desenha o despertador animado e encerra
    if (engine.IsAlarmeAtivo()) {
        DrawAlarmClock(layer);
#if LVGL_VERSION_MAJOR >= 9
        lv_canvas_finish_layer(face_canvas_, &layer_obj);
#endif
        return;
    }

    // Se ainda estiver no estado de ovo / incubação, desenha o ovo cósmico animado e encerra
    if (engine.GetEstadoNascimento() != ESTADO_NASCIDO) {
        DrawEggScreen(layer);
#if LVGL_VERSION_MAJOR >= 9
        lv_canvas_finish_layer(face_canvas_, &layer_obj);
#endif
        return;
    }
    
    // Cenas interativas do Companion App (Dados, Roleta, Jokenpô, Bola de Cristal)
    if (engine.IsDiceActive()) {
        DrawDiceScene(layer);
#if LVGL_VERSION_MAJOR >= 9
        lv_canvas_finish_layer(face_canvas_, &layer_obj);
#endif
        return;
    }
    if (engine.IsWheelActive()) {
        DrawWheelScene(layer);
#if LVGL_VERSION_MAJOR >= 9
        lv_canvas_finish_layer(face_canvas_, &layer_obj);
#endif
        return;
    }
    std::string activeCompanionOverlay = engine.GetActiveOverlay();
    if (activeCompanionOverlay == "jokenpo_rock" || activeCompanionOverlay == "jokenpo_paper" ||
        activeCompanionOverlay == "jokenpo_scissors" || activeCompanionOverlay == "crystal_ball" ||
        activeCompanionOverlay == "jail") {
        DrawCompanionScene(layer, activeCompanionOverlay);
#if LVGL_VERSION_MAJOR >= 9
        lv_canvas_finish_layer(face_canvas_, &layer_obj);
#endif
        return;
    }

    float eyeLx = 44, eyeLy = 37, eyeLw = 12, eyeLh = 24, eyeRadius = 6;
    float eyeRx = 84, eyeRy = 37, eyeRw = 12, eyeRh = 24;
    
    if (engine.GetPersonalidade() == PERSONALIDADE_SENSIVEL) {
        eyeLw = 14; eyeRw = 14;
        eyeLh = 26; eyeRh = 26;
        eyeRadius = 10; // Olhos maiores e arredondados
    }
    
    int fome = engine.GetFome();
    int diversao = engine.GetDiversao();
    int saude = engine.GetSaude();
    bool estaDoente = engine.EstaDoente();
    
    auto pers = engine.GetPersonalidade();
    bool isSarcastic = (pers == PERSONALIDADE_SARCASTICA || engine.GetHumor() == 1);
    int limiarHUD = (pers == PERSONALIDADE_SENSIVEL) ? 50 : 30;
    bool timerAtivo = engine.IsTimerActive();
    // bool mediaAtiva = engine.IsMediaActive();
    bool precisaComida = (fome <= limiarHUD);
    bool precisaBrincar = (diversao <= limiarHUD);
    bool precisaSaude = (saude <= limiarHUD || estaDoente);
    int numIcons = 0;
    if (precisaComida) numIcons++;
    if (precisaBrincar) numIcons++;
    if (precisaSaude) numIcons++;
    bool petStatusAtivo = (numIcons > 0);

    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
    std::string emotion = engine.GetCurrentEmotion();
    
    int tremorX = 0, tremorY = 0;
    
    // Tremedeira suave se houver chacoalhão/choque detectado pelo sensor físico
    if (engine.GetSensorChoque()) {
        tremorX += (ms % 60 < 30) ? 3 : -3;
        tremorY += (ms % 40 < 20) ? 2 : -2;
    }
    
    // Captura flags de animação ditadas pelo corpo ou engine
    bool comendo = engine.GetAnimacaoComendo();
    bool brincando = engine.GetAnimacaoBrincando() || engine.GetAnimacaoAcariciado();
    bool curando = engine.GetAnimacaoCurando();
    bool isDizzy = (engine.GetSensorChoque() || emotion == "confused");
    
    // Interpolação suave para o deslocamento dos olhos (Lerp)
    static float target_look_x = 0.0f;
    static float target_look_y = 0.0f;
    static float current_look_x = 0.0f;
    static float current_look_y = 0.0f;
    static int look_timer = 0;

    if (comendo || brincando || emotion == "sleeping" || isDizzy) {
        target_look_x = 0.0f;
        target_look_y = 0.0f;
    } else {
        look_timer++;
        if (look_timer > 200) {
            look_timer = 0;
            int r = rand() % 100;
            if (r < 35) {
                target_look_x = (float)((rand() % 8) - 10); // olha para esquerda
                target_look_y = (float)((rand() % 4) - 2);
            } else if (r < 70) {
                target_look_x = (float)((rand() % 8) + 3);  // olha para direita
                target_look_y = (float)((rand() % 4) - 2);
            } else if (r < 85) {
                target_look_x = (float)((rand() % 6) - 3);  // olha para cima/baixo
                target_look_y = (float)((rand() % 6) - 3);
            } else {
                target_look_x = 0.0f;                       // centro
                target_look_y = 0.0f;
            }
        }
    }

    // Aplica Lerp a cada frame para deslocar suavemente os olhos
    current_look_x += (target_look_x - current_look_x) * 0.15f;
    current_look_y += (target_look_y - current_look_y) * 0.15f;
    
    uint8_t idleTipo = engine.GetTipoReacaoOciosa();
    uint64_t tStart = engine.GetTempoInicioReacaoOciosa();
    uint64_t tFim = engine.GetTempoFimReacaoOciosa();

    // =========================================================================
    // 1. PALETA DE CORES RICA (RGB565) POR PERSONALIDADE E EMOÇÃO
    // =========================================================================
    struct EyePalette {
        lv_color_t primary;
        lv_color_t aura;
        lv_color_t pupil;
        lv_color_t highlight;
        lv_color_t cheek;
        lv_color_t mouth;
        bool hasCheeks;
        bool hasAura;
    };

    EyePalette pal;
    pal.highlight = lv_color_hex(0xFFFFFF);
    pal.hasAura = true;
    pal.hasCheeks = false;
    
    if (pers == PERSONALIDADE_SARCASTICA) {
        pal.primary = lv_color_hex(0xB388FF);   // Roxo ametista néon
        pal.aura = lv_color_hex(0x651FFF);      // Violeta profundo
        pal.pupil = lv_color_hex(0x000000);     // Pupila preta clássica
        pal.cheek = lv_color_hex(0xEA80FC);     // Blush lilás
        pal.mouth = lv_color_hex(0xEDE7F6);
    } else if (pers == PERSONALIDADE_SENSIVEL) {
        pal.primary = lv_color_hex(0x00E5FF);   // Ciano celeste suave
        pal.aura = lv_color_hex(0x0091EA);      // Azul vibrante
        pal.pupil = lv_color_hex(0x01579B);     // Azul profundo
        pal.cheek = lv_color_hex(0xFF6B8B);     // Bochechas rosadas fofas
        pal.mouth = lv_color_hex(0xFFFFFF);
        pal.hasCheeks = true;
    } else { // PERSONALIDADE_BASICA
        pal.primary = lv_color_hex(0x00E5FF);   // Cyber Cyan brilhante
        pal.aura = lv_color_hex(0x0077B6);      // Azul royal sutil
        pal.pupil = lv_color_hex(0x023E8A);     // Pupila azul escuro
        pal.cheek = lv_color_hex(0xFF8DA1);     // Bochechas rosadas
        pal.mouth = lv_color_hex(0xFFFFFF);
    }

    // Modificações de paleta para eventos e emoções ativas
    if (isDizzy) {
        pal.primary = lv_color_hex(0xFFEA00);   // Amarelo néon de tontura
        pal.aura = lv_color_hex(0x00E5FF);      // Ciano psicodélico
        pal.pupil = lv_color_hex(0xFF6D00);
        pal.mouth = lv_color_hex(0xFFEA00);
    } else if (comendo) {
        pal.primary = lv_color_hex(0xFFD600);   // Dourado satisfeito
        pal.aura = lv_color_hex(0xFF6D00);
        pal.pupil = lv_color_hex(0xE65100);
        pal.cheek = lv_color_hex(0xFF4081);
        pal.hasCheeks = true;
        pal.mouth = lv_color_hex(0xFFD600);
    } else if (curando) {
        pal.primary = lv_color_hex(0x00E676);   // Verde-esmeralda revitalizante
        pal.aura = lv_color_hex(0x00B0FF);      // Ciano cura
        pal.pupil = lv_color_hex(0x00331A);
        pal.cheek = lv_color_hex(0xFF6B8B);
        pal.hasCheeks = true;
        pal.mouth = lv_color_hex(0xFFFFFF);
    } else if (brincando || emotion == "happy" || idleTipo == 11) {
        pal.primary = lv_color_hex(0xFFD600);   // Amarelo sol / Dourado alegre
        pal.aura = lv_color_hex(0xFF8F00);
        pal.pupil = lv_color_hex(0xE65100);
        pal.cheek = lv_color_hex(0xFF4081);     // Bochechas vibrantes
        pal.hasCheeks = true;
        pal.mouth = lv_color_hex(0xFFFFFF);
    } else if (emotion == "angry") {
        pal.primary = lv_color_hex(0xFF1744);   // Vermelho flamejante
        pal.aura = lv_color_hex(0xD50000);
        pal.pupil = lv_color_hex(0xBF360C);
        pal.mouth = lv_color_hex(0xFF1744);
    } else if (emotion == "sad" || emotion == "crying") {
        pal.primary = lv_color_hex(0x00B0FF);   // Azul oceânico lacrimoso
        pal.aura = lv_color_hex(0x01579B);
        pal.pupil = lv_color_hex(0x002171);
        pal.mouth = lv_color_hex(0x80D8FF);
    } else if (emotion == "sleeping") {
        pal.primary = lv_color_hex(0x7C4DFF);   // Lavanda sonolento suave
        pal.aura = lv_color_hex(0x311B92);
        pal.mouth = lv_color_hex(0xB388FF);
    } else if (emotion == "surprised") {
        pal.primary = lv_color_hex(0xFFFF00);   // Amarelo choque
        pal.aura = lv_color_hex(0xFFD600);
        pal.pupil = lv_color_hex(0x000000);
        pal.mouth = lv_color_hex(0xFFFF00);
    } else if (emotion == "cold") {
        pal.primary = lv_color_hex(0x80D8FF);   // Azul gélido
        pal.aura = lv_color_hex(0x00B0FF);
        pal.mouth = lv_color_hex(0x00FFFF);
    } else if (emotion == "embarrassed") {
        pal.primary = lv_color_hex(0xFF5252);   // Coral ruborizado
        pal.aura = lv_color_hex(0xD50000);
        pal.cheek = lv_color_hex(0xFF1744);
        pal.hasCheeks = true;
        pal.mouth = lv_color_hex(0xFF5252);
    } else if (idleTipo == 7 || emotion == "loving") {
        pal.primary = lv_color_hex(0xFF1493);   // Magenta amor
        pal.aura = lv_color_hex(0xC2185B);
        pal.pupil = lv_color_hex(0x880E4F);
        pal.cheek = lv_color_hex(0xFF69B4);
        pal.hasCheeks = true;
        pal.mouth = lv_color_hex(0xFF69B4);
    }

    // =========================================================================
    // 2. MOTOR DE RESPIRAÇÃO SUAVE E ORGÂNICA DOS OLHOS
    // =========================================================================
    float breathPeriod = 3000.0f;
    if (emotion == "sleeping") {
        breathPeriod = 4200.0f;
    } else if (emotion == "angry") {
        breathPeriod = 2200.0f; // Respiração mais acelerada e ofegante de raiva
    }
    float breathAngle = (float)(ms % (int)breathPeriod) / breathPeriod * (2.0f * 3.14159265f);
    float breathFactor = sin(breathAngle); // oscila suavemente entre -1.0 e +1.0
    
    float breathY = 0.0f;
    float breathScaleH = 0.0f;
    float breathScaleW = 0.0f;

    if (emotion == "sleeping") {
        // No sono: respiração profunda com flutuação vertical serena
        breathY = -breathFactor * 2.2f;
    } else if (emotion == "angry") {
        // Na raiva: respiração ofegante, olhos e sobrancelhas sobem e estufam
        breathY = -breathFactor * 2.5f;
        breathScaleH = breathFactor * 2.5f;
        breathScaleW = breathFactor * 1.0f;
    } else {
        // Respiração normal viva para todas as outras carinhas e expressões:
        // Na inspiração: peito/olhos sobem (-2.2px) e expandem (+3.0px altura, +1.2px largura)
        // Na expiração: relaxam e descem suavemente
        breathY = -breathFactor * 2.2f;
        breathScaleH = breathFactor * 3.0f;
        breathScaleW = breathFactor * 1.2f;
    }

    // =========================================================================
    // 3. MÁQUINA DE PISCAR NÃO-LINEAR E ORGÂNICA (COM PISCADA DUPLA)
    // =========================================================================
    struct BlinkEngine {
        uint32_t nextBlinkTime = 0;
        uint32_t blinkStartTime = 0;
        bool active = false;
        bool isDouble = false;
        float blinkFactor = 0.0f; // 0.0 = aberto, 1.0 = fechado
    };
    static BlinkEngine blinker;
    
    if (!blinker.active) {
        if (ms >= blinker.nextBlinkTime) {
            if (emotion != "sleeping" && !isDizzy) {
                blinker.active = true;
                blinker.blinkStartTime = ms;
                blinker.isDouble = ((rand() % 100) < 22); // 22% de chance de piscada dupla
                blinker.blinkFactor = 0.0f;
            }
            blinker.nextBlinkTime = ms + 2800 + (rand() % 2600);
        }
    }

    if (blinker.active) {
        uint32_t elapsed = ms - blinker.blinkStartTime;
        if (!blinker.isDouble) {
            if (elapsed < 70) {
                // Fechamento rápido (ease-in)
                float t = (float)elapsed / 70.0f;
                blinker.blinkFactor = t * t;
            } else if (elapsed < 90) {
                blinker.blinkFactor = 1.0f;
            } else if (elapsed < 180) {
                // Reabertura suave (ease-out)
                float t = (float)(elapsed - 90) / 90.0f;
                float inv = 1.0f - t;
                blinker.blinkFactor = inv * inv * inv;
            } else {
                blinker.active = false;
                blinker.blinkFactor = 0.0f;
            }
        } else {
            // Piscada dupla realista
            if (elapsed < 50) {
                float t = (float)elapsed / 50.0f;
                blinker.blinkFactor = t * t;
            } else if (elapsed < 100) {
                float t = (float)(elapsed - 50) / 50.0f;
                blinker.blinkFactor = 1.0f - t * 0.6f;
            } else if (elapsed < 150) {
                float t = (float)(elapsed - 100) / 50.0f;
                blinker.blinkFactor = 0.4f + t * 0.6f;
            } else if (elapsed < 180) {
                blinker.blinkFactor = 1.0f;
            } else if (elapsed < 320) {
                float t = (float)(elapsed - 180) / 140.0f;
                float inv = 1.0f - t;
                blinker.blinkFactor = inv * inv;
            } else {
                blinker.active = false;
                blinker.blinkFactor = 0.0f;
            }
        }
    }

    // Gingado senoidal corporal ao brincar/feliz (bounce)
    float swayX = (brincando || emotion == "happy") ? (sin(ms * 0.008f) * 1.5f) : 0.0f;
    float swayY = (brincando || emotion == "happy") ? (-fabs(sin(ms * 0.010f)) * 3.0f) : 0.0f;

    // =========================================================================
    // 4. RENDERIZAÇÃO DOS OLHOS POR ESTADO E EMOÇÃO
    // =========================================================================
    if (isDizzy) {
        // TONTURA / CHOQUE / CONFUSO: Olhos com cruzes giratórias + 3 estrelas orbitando em 3D
        float rotAngle = ms * 0.012f;
        DrawEyeDizzy(eyeLx + xOffset + tremorX, eyeLy + tremorY + breathY, 9.0f, rotAngle, pal.primary, layer);
        DrawEyeDizzy(eyeRx + xOffset + tremorX, eyeRy + tremorY + breathY, 9.0f, -rotAngle, pal.primary, layer);
        
        // 3 Estrelas orbitando a cabeça em elipse 3D (x=64, y=10)
        float starAngle = ms * 0.006f;
        for (int s = 0; s < 3; s++) {
            float a = starAngle + s * (2.0f * 3.14159f / 3.0f);
            float sx = 64.0f + cos(a) * 20.0f + xOffset;
            float sy = 10.0f + sin(a) * 5.0f + breathY;
            float sRadius = 2.5f + sin(a) * 0.8f;
            lv_color_t sColor = (sin(a) > 0) ? lv_color_hex(0xFFD700) : lv_color_hex(0xFFA000);
            DrawStar(sx, sy, sRadius, sColor, layer);
        }
    } else if (comendo) {
        // COMENDO: Mastigação elástica + Squash & Stretch + fase final lambendo os beiços
        float elapsed = (float)(ms % 3000);
        float cycle = elapsed / 3000.0f;
        bool isLicking = (cycle >= 0.72f);
        float chewPhase = sinf(ms * 0.016f);
        bool biteOpen = (!isLicking && chewPhase > 0.15f);
        float chewBounceY = isLicking ? -2.0f : (biteOpen ? -3.5f : 3.0f);
        int arcR = biteOpen ? 13 : 16;
        int lx = (int)roundf((eyeLx + xOffset) * 2.0f);
        int rx = (int)roundf((eyeRx + xOffset) * 2.0f);
        int cy = (int)roundf((eyeLy + chewBounceY * 0.5f) * 2.0f);

        draw_canvas_arc(layer, lx, cy + 6, arcR + 2, 180, 360, pal.aura, 7);
        draw_canvas_arc(layer, rx, cy + 6, arcR + 2, 180, 360, pal.aura, 7);
        draw_canvas_arc(layer, lx, cy + 6, arcR, 180, 360, pal.primary, 5);
        draw_canvas_arc(layer, rx, cy + 6, arcR, 180, 360, pal.primary, 5);

        if (isLicking) {
            float lickP = (cycle - 0.72f) / 0.28f;
            draw_four_point_star(layer, rx + 26, cy - 12, sinf(lickP * 3.14159f) * 8.0f, lv_color_hex(0xFFD700));
        }
    } else if (brincando) {
        // BRINCANDO / CARINHO: Dancinha com pulinho lateral (hop left, hop right) + olhos > < e ^ ^
        float hopPhase = ms * 0.0075f;
        float bSwayX = sinf(hopPhase * 0.5f) * 9.0f;
        float bBounceY = -fabsf(sinf(hopPhase)) * 7.5f;
        float cycle = fmodf(ms * 0.000357f, 1.0f);
        bool isExcitedSquint = (cycle > 0.30f && cycle < 0.65f);

        int lx = (int)roundf(88.0f + xOffset * 2.0f + bSwayX);
        int rx = (int)roundf(168.0f + xOffset * 2.0f + bSwayX);
        int cy = (int)roundf(74.0f + bBounceY);

        if (isExcitedSquint) {
            // Olhos > < vibrantes no pico da empolgação
            draw_canvas_line(layer, lx - 14, cy - 10, lx + 11, cy, pal.aura, 7);
            draw_canvas_line(layer, lx + 11, cy, lx - 14, cy + 10, pal.aura, 7);
            draw_canvas_line(layer, rx + 14, cy - 10, rx - 11, cy, pal.aura, 7);
            draw_canvas_line(layer, rx - 11, cy, rx + 14, cy + 10, pal.aura, 7);

            draw_canvas_line(layer, lx - 14, cy - 10, lx + 11, cy, pal.primary, 4);
            draw_canvas_line(layer, lx + 11, cy, lx - 14, cy + 10, pal.primary, 4);
            draw_canvas_line(layer, rx + 14, cy - 10, rx - 11, cy, pal.primary, 4);
            draw_canvas_line(layer, rx - 11, cy, rx + 14, cy + 10, pal.primary, 4);
        } else {
            draw_canvas_arc(layer, lx, cy + 5, 15, 180, 360, pal.aura, 7);
            draw_canvas_arc(layer, rx, cy + 5, 15, 180, 360, pal.aura, 7);
            draw_canvas_arc(layer, lx, cy + 5, 14, 180, 360, pal.primary, 5);
            draw_canvas_arc(layer, rx, cy + 5, 14, 180, 360, pal.primary, 5);
        }

        // Sobrancelhas festivas
        draw_canvas_quad_bezier(layer, lx - 12.0f, cy - 18.0f, (float)lx, cy - 25.0f, lx + 12.0f, cy - 18.0f, pal.primary, 3, 5);
        draw_canvas_quad_bezier(layer, rx - 12.0f, cy - 18.0f, (float)rx, cy - 25.0f, rx + 12.0f, cy - 18.0f, pal.primary, 3, 5);
    } else if (curando) {
        // CURANDO: Fase 1 (Engolindo pílula > <) -> Fase 2 (Revitalização Esmeralda com estrelas de vitalidade)
        float cycle = fmodf(ms * 0.000333f, 1.0f);
        bool isTakingPill = (cycle < 0.26f);

        if (isTakingPill) {
            float p = cycle / 0.26f;
            float gulpY = sinf(p * 3.14159f) * 4.0f;
            int lx = (int)roundf(88.0f + xOffset * 2.0f);
            int rx = (int)roundf(168.0f + xOffset * 2.0f);
            int cy = (int)roundf(76.0f + gulpY);

            draw_canvas_line(layer, lx - 13, cy - 9, lx + 11, cy, pal.primary, 4);
            draw_canvas_line(layer, lx + 11, cy, lx - 13, cy + 9, pal.primary, 4);
            draw_canvas_line(layer, rx + 13, cy - 9, rx - 11, cy, pal.primary, 4);
            draw_canvas_line(layer, rx - 11, cy, rx + 13, cy + 9, pal.primary, 4);

            // Pílula descendo em direção à boca
            if (p < 0.75f) {
                int pillY = (int)roundf(76.0f + (p / 0.75f) * 24.0f);
                draw_canvas_rect(layer, 120 + xOffset * 2, pillY - 4, 8, 8, lv_color_hex(0xFF1744), 4);
                draw_canvas_rect(layer, 128 + xOffset * 2, pillY - 4, 8, 8, lv_color_hex(0xFFFFFF), 4);
            }
        } else {
            float hp = (cycle - 0.26f) / 0.74f;
            float floatY = -sinf(hp * 3.14159f) * 5.0f;
            int lx = 88 + xOffset * 2;
            int rx = 168 + xOffset * 2;
            int cy = (int)roundf(74.0f + floatY);
            int w = 26, h = 50, r = 13;

            int eyesX[2] = { lx, rx };
            for (int i = 0; i < 2; i++) {
                int ex = eyesX[i];
                draw_canvas_rect_empty(layer, ex - w/2 - 2, cy - h/2 - 2, w + 4, h + 4, pal.aura, 2, r + 2);
                draw_canvas_rect(layer, ex - w/2, cy - h/2, w, h, pal.primary, r);
                draw_canvas_rect(layer, ex - 7, cy - 14, 14, 28, pal.pupil, 7);

                draw_four_point_star(layer, ex + 5, cy - 10, 6.5f + sinf(ms * 0.018f) * 1.5f, lv_color_hex(0xFFFFFF));
                draw_canvas_disc(layer, ex - 5, cy + 9, 2, lv_color_hex(0xFFFFFF));
            }
        }
    } else if (idleTipo == 2) {
        // SARCÁSTICO: Revirar os olhos ocultando as pupilas atrás da pálpebra superior (deixando o olho vazio no topo)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2600);
        float animElapsed = fmodf(elapsed, 2600.0f);
        float pupilX = 0.0f;
        float pupilY = 0.0f;
        float browLift = 0.0f;
        if (animElapsed < 2000.0f) {
            float p = animElapsed / 2000.0f;
            float angle = 3.14159265f * 0.5f + p * 3.14159265f * 2.0f;
            pupilX = cosf(angle) * 11.0f;
            if (sinf(angle) < 0.0f) {
                pupilY = sinf(angle) * 23.0f; // Sobe para trás da pálpebra e desaparece, deixando o olho vazio
            } else {
                pupilY = (sinf(angle) - 0.5f) * 6.0f;
            }
            browLift = -sinf(p * 3.14159265f) * 3.5f;
        }
        int lx = (int)((eyeLx + xOffset + tremorX) * 2);
        int rx = (int)((eyeRx + xOffset + tremorX) * 2);
        int cy = (int)((eyeLy + tremorY + breathY + 2.0f) * 2);
        int browBaseY = (int)((eyeLy + tremorY + breathY) * 2);
        int sarW = (int)((eyeLw + 4.0f + breathScaleW) * 2);
        int sarH = (int)((13.0f + breathScaleH * 0.6f) * 2);

        draw_sarcastic_eyebrows(layer, lx, browBaseY, rx, browBaseY, pal.primary, browLift * 0.5f, browLift);
        draw_sarcastic_half_closed_eye(layer, lx, cy, sarW, sarH, 8, pal.primary, pal.aura, pupilX, pupilY, true);
        draw_sarcastic_half_closed_eye(layer, rx, cy, sarW, sarH, 8, pal.primary, pal.aura, pupilX, pupilY, true);
    } else if (idleTipo == 8) {
        // SARCÁSTICO: Tédio / Bocejo animado (olhos fecham aos poucos, cabeça sobe, gotinha no canto)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3000);
        float animElapsed = fmodf(elapsed, 3000.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3000.0f));
        float yawnP = 0.0f;
        float shakeX = 0.0f;
        if (cycle < 0.35f) {
            float u = cycle / 0.35f;
            yawnP = u * u * (3.0f - 2.0f * u);
        } else if (cycle < 0.72f) {
            yawnP = 1.0f;
            shakeX = sinf(ms * 0.035f) * 1.2f;
        } else {
            float u = (cycle - 0.72f) / 0.28f;
            yawnP = 1.0f - (u * u * (3.0f - 2.0f * u));
        }

        float headLiftY = -yawnP * 5.0f;
        int lx = (int)roundf((eyeLx + xOffset + tremorX) * 2.0f + shakeX);
        int rx = (int)roundf((eyeRx + xOffset + tremorX) * 2.0f + shakeX);
        int cy = (int)roundf((eyeLy + tremorY + 2.0f) * 2.0f + headLiftY);
        int browBaseY = (int)roundf((eyeLy + tremorY) * 2.0f + headLiftY);
        int sarW = (int)((eyeLw + 4.0f) * 2);

        draw_sarcastic_eyebrows(layer, lx, browBaseY, rx, browBaseY, pal.primary, -yawnP * 4.0f, -yawnP * 2.0f);

        if (yawnP < 0.65f) {
            float openFactor = 1.0f - (yawnP / 0.65f) * 0.8f;
            int curH = std::max(4, (int)(26.0f * openFactor));
            draw_sarcastic_half_closed_eye(layer, lx, cy, sarW, curH, 8, pal.primary, pal.aura, 0.0f, 0.0f, (curH > 14));
            draw_sarcastic_half_closed_eye(layer, rx, cy, sarW, curH, 8, pal.primary, pal.aura, 0.0f, 0.0f, (curH > 14));
        } else {
            draw_canvas_quad_bezier(layer, lx - 16.0f, cy + 2.0f, (float)lx, cy - 6.0f, lx + 16.0f, cy + 2.0f, pal.primary, 4, 6);
            draw_canvas_quad_bezier(layer, rx - 16.0f, cy + 2.0f, (float)rx, cy - 6.0f, rx + 16.0f, cy + 2.0f, pal.primary, 4, 6);
            draw_canvas_disc(layer, rx + 19, cy + 3, 3, lv_color_hex(0x00E5FF));
        }
    } else if (idleTipo == 9) {
        // SARCÁSTICO: Olhos semicerrados desdenhosos com PUPILA + Gota grande estilo anime escorrendo no alto da cabeça
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2800);
        float animElapsed = fmodf(elapsed, 2800.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 2800.0f));
        float sinkY = 0.0f;
        if (cycle < 0.25f) {
            sinkY = (cycle / 0.25f) * 4.0f;
        } else if (cycle < 0.75f) {
            sinkY = 4.0f + sinf((cycle - 0.25f) * 12.56637f) * 0.8f;
        } else {
            sinkY = (1.0f - (cycle - 0.75f) / 0.25f) * 4.0f;
        }

        int lx = (int)((eyeLx + xOffset + tremorX) * 2);
        int rx = (int)((eyeRx + xOffset + tremorX) * 2);
        int cy = (int)roundf((eyeLy + tremorY + 2.0f) * 2.0f + sinkY);
        int browBaseY = (int)roundf((eyeLy + tremorY) * 2.0f + sinkY);
        int sarW = (int)((eyeLw + 4.0f) * 2);
        int sarH = (int)((13.0f + breathScaleH * 0.6f) * 2);

        draw_sarcastic_eyebrows(layer, lx, browBaseY, rx, browBaseY, pal.primary, 2.0f, 2.0f);

        // Piscar rápido e orgânico no meio da animação
        float blinkP = 0.0f;
        if (cycle >= 0.44f && cycle <= 0.56f) {
            float bp = (cycle - 0.44f) / 0.12f;
            blinkP = sinf(bp * 3.14159265f);
        }
        int curH = std::max(4, (int)(sarH * (1.0f - blinkP)));
        float pupilX = -3.0f; // Olhar desdenhoso de lado

        if (blinkP < 0.85f) {
            draw_sarcastic_half_closed_eye(layer, lx, cy, sarW, curH, 8, pal.primary, pal.aura, pupilX, 0.0f, (curH >= 10));
            draw_sarcastic_half_closed_eye(layer, rx, cy, sarW, curH, 8, pal.primary, pal.aura, pupilX, 0.0f, (curH >= 10));
        } else {
            draw_canvas_line(layer, lx - 17, cy, lx + 17, cy, pal.primary, 4);
            draw_canvas_line(layer, rx - 17, cy, rx + 17, cy, pal.primary, 4);
        }

        if (cycle > 0.10f && cycle < 0.90f) {
            float dp = (cycle - 0.10f) / 0.80f;
            float scale = (dp < 0.20f) ? (dp / 0.20f) : ((dp > 0.85f) ? ((1.0f - dp) / 0.15f) : 1.0f);
            float slideOffset = std::min(1.0f, dp * 1.5f) * 16.0f;
            draw_anime_sweat_drop(layer, 206 + xOffset * 2, (int)roundf(14.0f + slideOffset), 10.0f * scale);
        }
    } else if (idleTipo == 21) {
        // SARCÁSTICO 4 (VÍVIDO): Julgamento Silencioso (Side-eye desdenhoso lento + 3 balanços 'tsk, tsk' + reticências)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3400);
        float animElapsed = fmodf(elapsed, 3400.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3400.0f));

        float lookSideP = 0.0f;
        float headTilt = 0.0f;
        float headShake = 0.0f;
        float browRightLift = 0.0f;
        float browLeftDrop = 0.0f;

        if (cycle < 0.20f) {
            lookSideP = 0.0f;
        } else if (cycle < 0.40f) {
            float p = (cycle - 0.20f) / 0.20f;
            lookSideP = p * p * (3.0f - 2.0f * p);
            browRightLift = lookSideP * 6.0f;
            browLeftDrop = lookSideP * 2.0f;
            headTilt = lookSideP * 2.5f;
        } else if (cycle < 0.80f) {
            lookSideP = 1.0f;
            browRightLift = 6.0f;
            browLeftDrop = 2.0f;
            headTilt = 2.5f;
            float shakePhase = (cycle - 0.40f) / 0.40f;
            headShake = sinf(shakePhase * 18.84955f) * 3.8f;
        } else {
            float p = (cycle - 0.80f) / 0.20f;
            lookSideP = 1.0f - (p * p * (3.0f - 2.0f * p));
            browRightLift = lookSideP * 6.0f;
            browLeftDrop = lookSideP * 2.0f;
            headTilt = lookSideP * 2.5f;
        }

        int lx = (int)roundf(88.0f + (xOffset * 2.0f) + headShake);
        int rx = (int)roundf(168.0f + (xOffset * 2.0f) + headShake);
        int cy = (int)roundf(76.0f + headTilt);

        // Sobrancelha esquerda franzida e direita arqueada lá no alto
        draw_canvas_quad_bezier(layer, lx - 18.0f, cy - 15.0f + browLeftDrop, (float)lx, cy - 24.0f + browLeftDrop, lx + 16.0f, cy - 13.0f + browLeftDrop, pal.aura, 3, 5);
        draw_canvas_line(layer, rx - 16, cy - 16 - (int)browRightLift, rx + 6, cy - 16 - (int)browRightLift, pal.aura, 3);
        draw_canvas_quad_bezier(layer, rx + 6.0f, cy - 16.0f - browRightLift, rx + 16.0f, cy - 16.0f - browRightLift, rx + 22.0f, cy - 30.0f - browRightLift, pal.aura, 3, 5);

        int eyesX[2] = { lx, rx };
        for (int i = 0; i < 2; i++) {
            int ex = eyesX[i];
            draw_canvas_rect_empty(layer, ex - 18, cy - 12, 36, 24, pal.aura, 2, 8);
            draw_canvas_rect(layer, ex - 16, cy - 10, 32, 20, pal.primary, 6);

            // Pupila preta clássica deslizando com destaque especular branco
            int pupX = (int)roundf(ex - 6.0f + lookSideP * 8.0f);
            int pupY = (int)roundf(cy - 8.0f - lookSideP * 1.5f);
            draw_canvas_rect(layer, pupX, pupY, 13, 16, lv_color_hex(0x000000), 4);
            draw_canvas_disc(layer, pupX + 3, pupY + 4, 2, lv_color_hex(0xFFFFFF));

            // Pálpebra superior semicerrada
            draw_canvas_line(layer, ex - 19, cy - 9, ex + 19, cy - 8 + (i == 0 ? 1 : -1), pal.aura, 4);
        }

        // Reticências "..." de julgamento no ar
        if (cycle > 0.35f && cycle < 0.85f) {
            float dotP = (cycle - 0.35f) / 0.50f;
            for (int i = 0; i < 3; i++) {
                draw_canvas_disc(layer, (int)(194 + i * 8 + headShake), (int)(50 - dotP * 12.0f), 2, pal.primary);
            }
        }
    } else if (idleTipo == 22) {
        // SARCÁSTICO 5 (VÍVIDO): Risada Maliciosa / Deboche 😈 (crescendo cômico + chifrinhos néon + faíscas)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2800);
        float animElapsed = fmodf(elapsed, 2800.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 2800.0f));

        float bounceY = 0.0f;
        float laughClimax = 0.0f;
        float hornScale = 0.0f;

        if (cycle < 0.25f) {
            float p = cycle / 0.25f;
            bounceY = -fabsf(sinf(p * 12.56637f)) * 2.0f;
            laughClimax = 0.0f;
        } else if (cycle < 0.75f) {
            float p = (cycle - 0.25f) / 0.50f;
            bounceY = -fabsf(sinf(p * 25.13274f)) * 4.5f;
            laughClimax = 1.0f;
            hornScale = std::min(1.0f, p * 4.0f);
        } else {
            float p = (cycle - 0.75f) / 0.25f;
            bounceY = -(1.0f - p) * 1.5f;
            laughClimax = 1.0f - p;
            hornScale = 1.0f - p;
        }

        int lx = (int)roundf(88.0f + xOffset * 2.0f);
        int rx = (int)roundf(168.0f + xOffset * 2.0f);
        int cy = (int)roundf(76.0f + bounceY);

        // Chifrinhos néon de diabinho travesso brotando no clímax
        if (hornScale > 0.05f) {
            int hH = (int)(12.0f * hornScale);
            lv_color_t hornColor = lv_color_hex(0xD500F9);
            draw_canvas_triangle(layer, lx - 12, cy - 28, lx - 20, cy - 28 - hH, lx - 4, cy - 28 - (int)(hH * 0.4f), hornColor);
            draw_canvas_triangle(layer, rx + 12, cy - 28, rx + 20, cy - 28 - hH, rx + 4, cy - 28 - (int)(hH * 0.4f), hornColor);
        }

        // Sobrancelhas zombeteiras
        draw_canvas_line(layer, lx - 16, cy - 15, lx + 14, cy - 20, pal.primary, 4);
        draw_canvas_line(layer, rx - 14, cy - 20, rx + 16, cy - 15, pal.primary, 4);

        if (laughClimax > 0.4f) {
            // Olhos apertados em > < afiados durante a gargalhada
            draw_canvas_line(layer, lx - 16, cy - 7, lx + 13, cy, pal.primary, 5);
            draw_canvas_line(layer, lx + 13, cy, lx - 16, cy + 7, pal.primary, 5);
            draw_canvas_line(layer, rx + 16, cy - 7, rx - 13, cy, pal.primary, 5);
            draw_canvas_line(layer, rx - 13, cy, rx + 16, cy + 7, pal.primary, 5);
        } else {
            // Olhos semicerrados zombeteiros com pupila preta e brilho branco
            int eyesX[2] = { lx, rx };
            for (int i = 0; i < 2; i++) {
                int ex = eyesX[i];
                draw_canvas_rect_empty(layer, ex - 16, cy - 8, 32, 16, pal.aura, 2, 5);
                draw_canvas_rect(layer, ex - 14, cy - 6, 28, 12, pal.primary, 4);
                draw_canvas_rect(layer, ex - 2, cy - 5, 10, 10, lv_color_hex(0x000000), 3);
                draw_canvas_disc(layer, ex + 1, cy - 2, 1, lv_color_hex(0xFFFFFF));
            }
        }

        // Faíscas provocantes em cascata
        for (int i = 0; i < 3; i++) {
            float p = fmodf(ms * 0.0011f + i * 0.33f, 1.0f);
            int sx = (int)roundf(138.0f + sinf(p * 6.0f + i) * 22.0f + i * 14.0f);
            int sy = (int)roundf(96.0f - p * 58.0f);
            draw_four_point_star(layer, sx, sy, 5.0f * (1.0f - p * 0.5f), lv_color_hex(0xEA80FC));
        }
    } else if (idleTipo == 23) {
        // SARCÁSTICO 6 (VÍVIDO): ZZz Sarcástico / Dormindo 🥱 (tédio, espionada desdenhosa e sono)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3800);
        float animElapsed = fmodf(elapsed, 3800.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3800.0f));

        float headDrop = 0.0f;
        float eyeRightOpenP = 0.0f;

        if (cycle < 0.55f) {
            float p = cycle / 0.55f;
            headDrop = sinf(p * 1.57079f) * 5.0f;
            eyeRightOpenP = 0.2f * (1.0f - p);
        } else if (cycle < 0.62f) {
            headDrop = 5.0f - (cycle - 0.55f) * 20.0f; // micro-tranco no susto do estouro da bolha
            eyeRightOpenP = 0.7f;
        } else if (cycle < 0.85f) {
            headDrop = 2.0f;
            eyeRightOpenP = 0.5f; // espionando com desdém
        } else {
            float p = (cycle - 0.85f) / 0.15f;
            headDrop = 2.0f * (1.0f - p);
            eyeRightOpenP = 0.5f * (1.0f - p);
        }

        int lx = (int)roundf(88.0f + xOffset * 2.0f);
        int rx = (int)roundf(168.0f + xOffset * 2.0f);
        int cy = (int)roundf(76.0f + headDrop);

        // Olho esquerdo completamente fechado em linha reta preguiçosa
        draw_canvas_line(layer, lx - 16, cy, lx + 16, cy, pal.primary, 4);
        draw_canvas_line(layer, lx - 16, cy - 14, lx + 14, cy - 17, pal.aura, 4);

        // Olho direito entreaberto dinâmico
        int rH = std::max(3, (int)(14.0f * eyeRightOpenP));
        draw_canvas_rect_empty(layer, rx - 16, cy - rH / 2, 32, rH, pal.aura, 2, 4);
        draw_canvas_rect(layer, rx - 14, cy - rH / 2 + 1, 28, std::max(2, rH - 2), pal.primary, 3);
        if (rH > 5) {
            // Pupila preta com brilho branco
            draw_canvas_rect(layer, rx - 4, cy - rH / 2 + 1, 10, rH - 2, lv_color_hex(0x000000), 2);
            draw_canvas_disc(layer, rx - 1, cy - rH / 2 + 3, 1, lv_color_hex(0xFFFFFF));
        }
        draw_canvas_quad_bezier(layer, rx - 14.0f, cy - 17.0f, (float)rx, cy - 22.0f, rx + 16.0f, cy - 15.0f, pal.aura, 4, 5);

        // Letrinhas 'Z' flutuando
        for (int i = 0; i < 3; i++) {
            float p = fmodf(ms * 0.0004f + i * 0.33f, 1.0f);
            int zx = (int)roundf(144.0f + p * 36.0f + sinf(p * 5.0f + i) * 6.0f);
            int zy = (int)roundf(94.0f - p * 60.0f);
            lv_color_t zColor = lv_color_hex(0xB388FF);
            draw_canvas_line(layer, zx - 4, zy - 4, zx + 4, zy - 4, zColor, 2);
            draw_canvas_line(layer, zx + 4, zy - 4, zx - 4, zy + 4, zColor, 2);
            draw_canvas_line(layer, zx - 4, zy + 4, zx + 4, zy + 4, zColor, 2);
        }
    } else if (idleTipo == 1) {
        // BÁSICA 1: Assobiando no Ritmo (Olhos em arcos felizes ^ ^ + sobrancelhas no compasso + balanço musical)
        float beat = ms * 0.005f;
        float wSwayX = sinf(beat * 0.5f) * 6.0f;
        float bopY = fabsf(sinf(beat)) * 4.0f;
        int lx = (int)roundf(88.0f + xOffset * 2.0f + wSwayX);
        int rx = (int)roundf(168.0f + xOffset * 2.0f + wSwayX);
        int cy = (int)roundf(72.0f + bopY);

        draw_canvas_arc(layer, lx, cy + 6, 15, 180, 360, lv_color_hex(0x0077B6), 7);
        draw_canvas_arc(layer, rx, cy + 6, 15, 180, 360, lv_color_hex(0x0077B6), 7);
        draw_canvas_arc(layer, lx, cy + 6, 13, 180, 360, lv_color_hex(0x00E5FF), 5);
        draw_canvas_arc(layer, rx, cy + 6, 13, 180, 360, lv_color_hex(0x00E5FF), 5);

        float browLift = sinf(beat) * 2.5f;
        draw_canvas_quad_bezier(layer, lx - 12.0f, cy - 16.0f + browLift, (float)lx, cy - 22.0f + browLift, lx + 12.0f, cy - 16.0f + browLift, lv_color_hex(0x00E5FF), 3, 5);
        draw_canvas_quad_bezier(layer, rx - 12.0f, cy - 16.0f - browLift, (float)rx, cy - 22.0f - browLift, rx + 12.0f, cy - 16.0f - browLift, lv_color_hex(0x00E5FF), 3, 5);
    } else if (idleTipo == 10) {
        // BÁSICA 2: Piscadela Charmosa (Olho esq. em '>' + sobrancelha dir. arqueada + estrela dourada)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2600);
        float animElapsed = fmodf(elapsed, 2600.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 2600.0f));
        bool isWinking = (cycle > 0.15f && cycle < 0.82f);
        float tiltY = isWinking ? -1.5f : 0.0f;
        int lx = (int)((eyeLx + xOffset) * 2);
        int rx = (int)((eyeRx + xOffset) * 2);
        int cy = (int)((eyeLy + tiltY) * 2);

        if (isWinking) {
            draw_canvas_line(layer, lx - 14, cy - 9, lx + 12, cy + 1, lv_color_hex(0x00E5FF), 5);
            draw_canvas_line(layer, lx + 12, cy + 1, lx - 14, cy + 10, lv_color_hex(0x00E5FF), 5);
            draw_canvas_line(layer, lx - 14, cy - 22, lx + 12, cy - 18, lv_color_hex(0x00E5FF), 4);
            draw_canvas_quad_bezier(layer, rx - 14.0f, cy - 30.0f, (float)rx, cy - 38.0f, rx + 15.0f, cy - 28.0f, lv_color_hex(0x00E5FF), 4, 5);
            float spkP = (cycle - 0.15f) / 0.67f;
            float starR = sinf(spkP * 3.14159265f) * 9.0f;
            draw_four_point_star(layer, rx + 26, cy - 18, starR, lv_color_hex(0xFFD700));
            draw_four_point_star(layer, rx + 26, cy - 18, starR * 0.45f, lv_color_hex(0xFFFFFF));
        } else {
            DrawEye(eyeLx + xOffset, eyeLy + tiltY, eyeLw, eyeLh, eyeRadius, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true);
        }
        DrawEye(eyeRx + xOffset, eyeRy + tiltY, eyeRw + (isWinking ? 0.5f : 0.0f), eyeRh + (isWinking ? 1.0f : 0.0f), eyeRadius, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true, isWinking ? -1.5f : 0.0f, 0.0f);
    } else if (idleTipo == 11) {
        // BÁSICA 3: Risadinha ("Hehehe") (Quique rápido de risada + arcos dourados ^ ^)
        float laughBounce = -fabsf(sinf(ms * 0.014f)) * 6.5f;
        int lx = 88 + xOffset * 2;
        int rx = 168 + xOffset * 2;
        int cy = (int)roundf(74.0f + laughBounce);

        draw_canvas_arc(layer, lx, cy + 6, 15, 180, 360, lv_color_hex(0xFF8F00), 7);
        draw_canvas_arc(layer, rx, cy + 6, 15, 180, 360, lv_color_hex(0xFF8F00), 7);
        draw_canvas_arc(layer, lx, cy + 6, 14, 180, 360, lv_color_hex(0xFFD600), 5);
        draw_canvas_arc(layer, rx, cy + 6, 14, 180, 360, lv_color_hex(0xFFD600), 5);
    } else if (idleTipo == 18) {
        // BÁSICA 4: Curioso / Lupa Investigativa (Olho esq. semicerrado + olho dir. gigante em zoom)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3000);
        float animElapsed = fmodf(elapsed, 3000.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3000.0f));
        float lookX = 0.0f;
        if (cycle < 0.45f) {
            lookX = sinf((cycle / 0.45f) * 3.14159265f) * -10.0f;
        } else if (cycle < 0.90f) {
            lookX = sinf(((cycle - 0.45f) / 0.45f) * 3.14159265f) * 10.0f;
        }
        int lx = (int)roundf(88.0f + xOffset * 2.0f + lookX);
        int rx = (int)roundf(168.0f + xOffset * 2.0f + lookX);
        int cy = 74;

        DrawEye(lx / 2.0f, (cy + 2) / 2.0f, 11.0f, 16.0f, 4.0f, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true, lookX * 0.3f, 0.0f);
        draw_canvas_line(layer, lx - 13, cy - 18, lx + 13, cy - 16, lv_color_hex(0x00E5FF), 4);

        DrawEye(rx / 2.0f, (cy - 2) / 2.0f, 17.0f, 29.0f, 8.5f, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true, lookX * 0.4f, 0.0f);
        draw_canvas_quad_bezier(layer, rx - 16.0f, cy - 34.0f, (float)rx, cy - 44.0f, rx + 16.0f, cy - 34.0f, lv_color_hex(0x00E5FF), 4, 5);
    } else if (idleTipo == 19) {
        // BÁSICA 5: Descolado / Óculos Escuros ("Deal With It" 😎 com moldura dourada completa e reflexo nas lentes)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3200);
        float animElapsed = fmodf(elapsed, 3200.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3200.0f));
        float nodY = 0.0f;
        if (cycle >= 0.22f && cycle < 0.34f) {
            nodY = sinf(((cycle - 0.22f) / 0.12f) * 3.14159265f) * 4.5f;
        }
        int lx = 88 + xOffset * 2;
        int rx = 168 + xOffset * 2;
        int cy = (int)roundf(74.0f + nodY);

        DrawEye(lx / 2.0f, cy / 2.0f, 12.0f, 24.0f, 6.0f, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true);
        DrawEye(rx / 2.0f, cy / 2.0f, 12.0f, 24.0f, 6.0f, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true);

        float shadesY = (float)(cy - 3);
        if (cycle < 0.22f) {
            float p = cycle / 0.22f;
            float ease = 1.0f - (1.0f - p) * (1.0f - p);
            shadesY = -32.0f + ease * ((cy - 3.0f) + 32.0f);
        } else if (cycle > 0.86f) {
            float p = (cycle - 0.86f) / 0.14f;
            shadesY = (cy - 3.0f) - p * ((cy - 3.0f) + 34.0f);
        }

        int lensW = 54, lensH = 32;
        int leftLensX = lx - lensW / 2;
        int rightLensX = rx - lensW / 2;
        int lensTopY = (int)roundf(shadesY - lensH / 2.0f);

        if (lensTopY + lensH > 0) {
            lv_color_t goldCol = lv_color_hex(0xFFD600);
            draw_canvas_line(layer, leftLensX - 14, lensTopY + 6, leftLensX - 9, lensTopY + 2, goldCol, 4);
            draw_canvas_line(layer, leftLensX - 9, lensTopY + 2, leftLensX + 2, lensTopY + 3, goldCol, 4);
            draw_canvas_quad_bezier(layer, (float)(leftLensX + lensW - 2), (float)(lensTopY + 4), 128.0f + xOffset * 2.0f, (float)(lensTopY + 1), (float)(rightLensX + 2), (float)(lensTopY + 4), goldCol, 4, 4);
            draw_canvas_line(layer, rightLensX + lensW - 2, lensTopY + 3, rightLensX + lensW + 9, lensTopY + 2, goldCol, 4);
            draw_canvas_line(layer, rightLensX + lensW + 9, lensTopY + 2, rightLensX + lensW + 14, lensTopY + 6, goldCol, 4);

            int lensesX[2] = { leftLensX, rightLensX };
            for (int i = 0; i < 2; i++) {
                int x = lensesX[i];
                int y = lensTopY;

                draw_canvas_rect(layer, x, y, lensW, lensH, lv_color_hex(0x131B2E), 9);
                draw_canvas_line(layer, x + 10, y + lensH - 5, x + 24, y + 5, lv_color_hex(0x263554), 4);

                if (cycle >= 0.25f && cycle <= 0.78f) {
                    float shineP = (cycle - 0.25f) / 0.36f;
                    if (shineP >= 0.0f && shineP <= 1.0f) {
                        int sx = (int)roundf(x - 10.0f + shineP * (lensW + 20.0f));
                        int x1 = std::max(x + 3, std::min(x + lensW - 3, sx));
                        int x2 = std::max(x + 3, std::min(x + lensW - 3, sx - 12));
                        if (abs(x1 - x2) > 2) {
                            draw_canvas_line(layer, x1, y + 3, x2, y + lensH - 3, lv_color_hex(0xFFFFFF), 5);
                        }
                        int x3 = std::max(x + 3, std::min(x + lensW - 3, sx + 8));
                        int x4 = std::max(x + 3, std::min(x + lensW - 3, sx - 4));
                        if (abs(x3 - x4) > 2) {
                            draw_canvas_line(layer, x3, y + 3, x4, y + lensH - 3, lv_color_hex(0xFFFFFF), 2);
                        }
                    }
                }

                draw_canvas_rect_empty(layer, x, y, lensW, lensH, goldCol, 3, 9);
                draw_canvas_rect(layer, x - 3, y - 2, lensW + 6, 5, goldCol, 2);
                draw_canvas_rect(layer, x + 2, y, 5, 3, lv_color_hex(0xFFFFFF), 1);
                draw_canvas_rect(layer, x + lensW - 7, y, 5, 3, lv_color_hex(0xFFFFFF), 1);
            }

            if (cycle > 0.50f && cycle < 0.80f) {
                float spk = sinf(((cycle - 0.50f) / 0.30f) * 3.14159265f);
                draw_four_point_star(layer, rightLensX + lensW - 4, lensTopY + 3, 8.0f * spk, lv_color_hex(0xFFFFFF));
            }
        }
    } else if (idleTipo == 20) {
        // BÁSICA 6: Pensativo -> Ideia Brilhante ("Eureka!" 💡)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3000);
        float animElapsed = fmodf(elapsed, 3000.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3000.0f));
        bool isEureka = (cycle >= 0.48f);

        if (!isEureka) {
            DrawEye(eyeLx + xOffset, 36.0f, 12.0f, 23.0f, 6.0f, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true, 2.0f, -5.0f);
            DrawEye(eyeRx + xOffset, 36.0f, 12.0f, 23.0f, 6.0f, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true, 2.0f, -5.0f);

            int dots = (int)((cycle / 0.48f) * 4.0f);
            if (dots > 3) dots = 3;
            for (int d = 0; d < dots; d++) {
                draw_canvas_disc(layer, 114 + d * 14 + xOffset * 2, 22, 3, lv_color_hex(0x00E5FF));
            }
        } else {
            float ep = (cycle - 0.48f) / 0.52f;
            float jumpY = (ep < 0.25f) ? (-sinf((ep / 0.25f) * 3.14159265f) * 8.0f) : 0.0f;
            DrawEye(eyeLx + xOffset, 38.0f + jumpY * 0.5f, 13.0f, 25.0f, 6.5f, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true);
            DrawEye(eyeRx + xOffset, 38.0f + jumpY * 0.5f, 13.0f, 25.0f, 6.5f, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0077B6), true);

            int bx = 128 + xOffset * 2;
            int by = (int)roundf(22.0f + jumpY * 0.5f);
            draw_canvas_disc(layer, bx, by, 10, lv_color_hex(0xFFD600));
            draw_canvas_rect(layer, bx - 5, by + 7, 10, 5, lv_color_hex(0xFFD600), 1);
            draw_canvas_rect(layer, bx - 4, by + 12, 8, 4, lv_color_hex(0xB0BEC5), 1);
            draw_canvas_disc(layer, bx - 3, by - 3, 3, lv_color_hex(0xFFFFFF));

            float rayLen = 4.0f + sinf(ms * 0.025f) * 2.0f;
            const float angles[5] = { -3.14159265f * 0.85f, -3.14159265f * 0.5f, -3.14159265f * 0.15f, -3.14159265f, 0.0f };
            for (int a = 0; a < 5; a++) {
                int rx1 = (int)roundf(bx + cosf(angles[a]) * 13.0f);
                int ry1 = (int)roundf(by + sinf(angles[a]) * 13.0f);
                int rx2 = (int)roundf(bx + cosf(angles[a]) * (13.0f + rayLen));
                int ry2 = (int)roundf(by + sinf(angles[a]) * (13.0f + rayLen));
                draw_canvas_line(layer, rx1, ry1, rx2, ry2, lv_color_hex(0xFFFF00), 3);
            }
        }
    } else if (idleTipo == 12) {
        // SENSÍVEL 1: Olhinhos Pidão / "Gato de Botas" (Pupilas gigantes com 3 brilhos trêmulos e sobrancelhas pidonas)
        float pidSwayX = sinf(ms * 0.0022f) * 2.5f;
        float shimX = sinf(ms * 0.028f) * 1.1f;
        float shimY = cosf(ms * 0.032f) * 1.1f;
        int lx = (int)roundf(88.0f + xOffset * 2.0f + pidSwayX);
        int rx = (int)roundf(168.0f + xOffset * 2.0f + pidSwayX);
        int cy = 72;
        int w = 34, h = 54, r = 17;

        int eyesX[2] = { lx, rx };
        for (int i = 0; i < 2; i++) {
            int ex = eyesX[i];
            draw_canvas_rect_empty(layer, ex - w / 2 - 2, cy - h / 2 - 2, w + 4, h + 4, lv_color_hex(0x0091EA), 2, r + 2);
            draw_canvas_rect(layer, ex - w / 2, cy - h / 2, w, h, lv_color_hex(0x00E5FF), r);
            draw_canvas_rect(layer, ex - 12, cy - 18, 24, 36, lv_color_hex(0x000000), 12);
            draw_canvas_disc(layer, (int)roundf(ex + 5.0f + shimX), (int)roundf(cy - 8.0f + shimY), 5, lv_color_hex(0xFFFFFF));
            draw_canvas_disc(layer, (int)roundf(ex - 6.0f - shimX), (int)roundf(cy + 7.0f - shimY), 3, lv_color_hex(0xFFFFFF));
            draw_canvas_disc(layer, (int)roundf(ex + 5.0f), (int)roundf(cy + 9.0f + shimY), 2, lv_color_hex(0xFFFFFF));
        }
        draw_canvas_quad_bezier(layer, lx - 15.0f, cy - 28.0f, (float)lx, cy - 33.0f, lx + 14.0f, cy - 35.0f, lv_color_hex(0x00E5FF), 4, 5);
        draw_canvas_quad_bezier(layer, rx - 14.0f, cy - 35.0f, (float)rx, cy - 33.0f, rx + 15.0f, cy - 28.0f, lv_color_hex(0x00E5FF), 4, 5);
    } else if (idleTipo == 15) {
        // SENSÍVEL 2: Tímido / Vergonha Fofa (Olhos > < apertados + bochechas pulsantes com tracinhos ///)
        float shyWiggleX = sinf(ms * 0.0045f) * 4.5f;
        float sinkY = 5.0f + sinf(ms * 0.0022f) * 1.5f;
        int lx = (int)roundf(88.0f + xOffset * 2.0f + shyWiggleX);
        int rx = (int)roundf(168.0f + xOffset * 2.0f + shyWiggleX);
        int cy = (int)roundf(72.0f + sinkY);
        lv_color_t pinkCol = lv_color_hex(0xFF69B4);

        // Olho esquerdo '>' e direito '<'
        draw_canvas_line(layer, lx - 14, cy - 11, lx + 12, cy, pinkCol, 5);
        draw_canvas_line(layer, lx + 12, cy, lx - 14, cy + 11, pinkCol, 5);
        draw_canvas_line(layer, rx + 14, cy - 11, rx - 12, cy, pinkCol, 5);
        draw_canvas_line(layer, rx - 12, cy, rx + 14, cy + 11, pinkCol, 5);

        // Bochechas grandes pulsantes com tracinhos diagonais de vergonha (///)
        float pulse = 1.0f + sinf(ms * 0.006f) * 0.12f;
        int cw = (int)roundf(28.0f * pulse);
        int ch = (int)roundf(8.0f * pulse);
        int cLx = lx - 6, cRx = rx + 6, cY = cy + 18;
        draw_canvas_rect(layer, cLx - cw / 2, cY - ch / 2, cw, ch, lv_color_hex(0xFF4081), ch / 2);
        draw_canvas_rect(layer, cRx - cw / 2, cY - ch / 2, cw, ch, lv_color_hex(0xFF4081), ch / 2);
        for (int b = -1; b <= 1; b++) {
            draw_canvas_line(layer, cLx + b * 5 - 2, cY + 3, cLx + b * 5 + 2, cY - 3, lv_color_hex(0xFF1744), 2);
            draw_canvas_line(layer, cRx + b * 5 - 2, cY + 3, cRx + b * 5 + 2, cY - 3, lv_color_hex(0xFF1744), 2);
        }
    } else if (idleTipo == 14) {
        // SENSÍVEL 3: Piscadela + Beijo Voador (Olho esq. aberto brilhante + olho dir. piscando em arco ^)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2800);
        float animElapsed = fmodf(elapsed, 2800.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 2800.0f));
        bool isWinking = (cycle > 0.12f && cycle < 0.85f);
        float headTiltY = isWinking ? -1.5f : 0.0f;

        DrawEye(eyeLx + xOffset, eyeLy + headTiltY, eyeLw, eyeLh, eyeRadius, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0091EA), true);
        if (isWinking) {
            int rx = (int)((eyeRx + xOffset) * 2);
            int cy = (int)((eyeRy + headTiltY) * 2);
            draw_canvas_quad_bezier(layer, rx - 15.0f, cy + 4.0f, (float)rx, cy - 14.0f, rx + 15.0f, cy + 4.0f, lv_color_hex(0xFF69B4), 5, 6);
        } else {
            DrawEye(eyeRx + xOffset, eyeRy + headTiltY, eyeRw, eyeRh, eyeRadius, layer, lv_color_hex(0x00E5FF), lv_color_hex(0x0091EA), true);
        }
    } else if (idleTipo == 16) {
        // SENSÍVEL 4: Carente / Dengoso (Olhos marejados com água balançando na base + fungadinha)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3000);
        float animElapsed = fmodf(elapsed, 3000.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3000.0f));
        float sniffY = 0.0f;
        if (cycle > 0.45f && cycle < 0.55f) sniffY = -4.0f;
        else if (cycle > 0.60f && cycle < 0.70f) sniffY = -5.0f;

        int lx = 88 + xOffset * 2;
        int rx = 168 + xOffset * 2;
        int cy = (int)roundf(76.0f + sniffY);
        int w = 30, h = 46, r = 14;
        int eyesX[2] = { lx, rx };
        for (int i = 0; i < 2; i++) {
            int ex = eyesX[i];
            draw_canvas_rect_empty(layer, ex - w / 2 - 2, cy - h / 2 - 2, w + 4, h + 4, lv_color_hex(0x0091EA), 2, r + 2);
            draw_canvas_rect(layer, ex - w / 2, cy - h / 2, w, h, lv_color_hex(0x00E5FF), r);
            draw_canvas_rect(layer, ex - 9, cy - 12, 18, 24, lv_color_hex(0x000000), 8);

            // Camada de lágrima/água acumulada na base do olho com onda no topo
            int tearH = 15 + (int)roundf(sinf(ms * 0.008f + i) * 2.0f);
            draw_canvas_rect(layer, ex - w / 2 + 2, cy + h / 2 - tearH, w - 4, tearH, lv_color_hex(0x80D8FF), 10);
            float waveP = ms * 0.008f + i;
            draw_canvas_quad_bezier(layer, ex - 12.0f, cy + 8.0f + sinf(waveP) * 2.5f, (float)ex, cy + 3.0f + cosf(waveP) * 2.5f, ex + 12.0f, cy + 8.0f - sinf(waveP) * 2.5f, lv_color_hex(0xB3E5FC), 3, 5);

            // Brilhos nos olhos marejados
            draw_canvas_disc(layer, ex + 5, cy - 6, 4, lv_color_hex(0xFFFFFF));
            draw_canvas_disc(layer, ex - 6, cy + 12, 2, lv_color_hex(0xFFFFFF));
        }
        // Sobrancelhas caídas dengosas (/ \)
        draw_canvas_line(layer, lx - 16, cy - 22, lx + 12, cy - 30, lv_color_hex(0x00B0FF), 4);
        draw_canvas_line(layer, rx - 12, cy - 30, rx + 16, cy - 22, lv_color_hex(0x00B0FF), 4);
    } else if (idleTipo == 7 || emotion == "loving") {
        // SENSÍVEL 5: Apaixonado nas Nuvens (Olhos de coração pulsantes Lub-Dub + balanço suave)
        float pulseTime = (float)(ms % 1000) / 1000.0f;
        bool lubDub = (pulseTime < 0.18f || (pulseTime > 0.28f && pulseTime < 0.44f));
        float loveSwayX = sinf(ms * 0.0028f) * 2.5f;
        float loveSwayY = cosf(ms * 0.0056f) * 1.25f;
        DrawLargeHeart((int)roundf(eyeLx + xOffset + tremorX + loveSwayX), (int)roundf(eyeLy + tremorY + loveSwayY), lubDub, layer, pal.primary);
        DrawLargeHeart((int)roundf(eyeRx + xOffset + tremorX + loveSwayX), (int)roundf(eyeRy + tremorY + loveSwayY), lubDub, layer, pal.primary);
    } else if (idleTipo == 17) {
        // SENSÍVEL 6: Maravilhado / Olhos Estrelados (Estrelas douradas de 4 pontas pulsando nos olhos)
        float bounceY = -fabsf(sinf(ms * 0.0055f)) * 4.0f;
        int lx = 88 + xOffset * 2;
        int rx = 168 + xOffset * 2;
        int cy = (int)roundf(72.0f + bounceY);
        int w = 32, h = 52, r = 16;
        float starScale = 11.0f + sinf(ms * 0.014f) * 3.0f;

        int eyesX[2] = { lx, rx };
        for (int i = 0; i < 2; i++) {
            int ex = eyesX[i];
            draw_canvas_rect_empty(layer, ex - w / 2 - 2, cy - h / 2 - 2, w + 4, h + 4, lv_color_hex(0x0091EA), 2, r + 2);
            draw_canvas_rect(layer, ex - w / 2, cy - h / 2, w, h, lv_color_hex(0x00E5FF), r);
            draw_canvas_rect(layer, ex - 11, cy - 16, 22, 32, lv_color_hex(0x01579B), 10);
            draw_four_point_star(layer, ex, cy, starScale, lv_color_hex(0xFFD700));
            draw_four_point_star(layer, ex, cy, starScale * 0.45f, lv_color_hex(0xFFFFFF));
        }
        // Faisquinhas extras ao redor dos olhos
        float spk = fabsf(sinf(ms * 0.008f));
        draw_four_point_star(layer, 56 + xOffset * 2, 46, 5.0f * spk, lv_color_hex(0xFFD700));
        draw_four_point_star(layer, 200 + xOffset * 2, 44, 6.0f * (1.0f - spk), lv_color_hex(0xFF80AB));
    } else if (emotion == "cold") {
        // FRIO: Olhos encolhidos em azul gélido com tremor rápido
        DrawEye(eyeLx + xOffset + tremorX + current_look_x, eyeLy + tremorY + current_look_y + breathY, 14 + breathScaleW, 20 + breathScaleH, 4, layer, pal.primary, pal.aura, true);
        DrawEye(eyeRx + xOffset + tremorX + current_look_x, eyeRy + tremorY + current_look_y + breathY, 14 + breathScaleW, 20 + breathScaleH, 4, layer, pal.primary, pal.aura, true);
    } else if (emotion == "sleeping") {
        // SONO: Pálpebras fechadas em arcos serenos para baixo com respiração lenta
        int lx = (int)((eyeLx + xOffset + tremorX + current_look_x) * 2);
        int rx = (int)((eyeRx + xOffset + tremorX + current_look_x) * 2);
        int ySleep = (int)((eyeLy + tremorY + current_look_y + breathY) * 2);
        int sleepRadius = 8 * 2;
        draw_canvas_arc(layer, lx, ySleep - 2, sleepRadius, 0, 180, pal.primary, 4);
        draw_canvas_arc(layer, rx, ySleep - 2, sleepRadius, 0, 180, pal.primary, 4);
    } else if (emotion == "surprised") {
        // SURPRESA / SUSTO: Olhos dilatados em amarelo choque com pupila de choque minúscula
        DrawEye(eyeLx + xOffset + tremorX + current_look_x, eyeLy + tremorY + current_look_y - 2 + breathY, 16 + breathScaleW, 32 + breathScaleH, 10, layer, pal.primary, pal.aura, true);
        DrawEye(eyeRx + xOffset + tremorX + current_look_x, eyeRy + tremorY + current_look_y - 2 + breathY, 16 + breathScaleW, 32 + breathScaleH, 10, layer, pal.primary, pal.aura, true);
    } else if (emotion == "angry") {
        // ZANGADO: Olhos em chamas carmesim/laranja com sobrancelhas grossas inclinadas, piscar orgânico e respiração
        float blinkFactor = blinker.blinkFactor;
        float sary = eyeLy + tremorY + current_look_y + breathY + 3.0f;
        float sarry = eyeRy + tremorY + current_look_y + breathY + 3.0f;
        float angryEyeH = (16.0f + breathScaleH * 0.8f) * (1.0f - blinkFactor);
        float angryEyeW = (eyeLw + breathScaleW * 0.8f) + (blinkFactor > 0.85f ? 1.5f : 0.0f);

        DrawEye(eyeLx + xOffset + tremorX + current_look_x, sary, angryEyeW, angryEyeH, 3, layer, pal.primary, pal.aura, (blinkFactor < 0.6f));
        DrawEye(eyeRx + xOffset + tremorX + current_look_x, sarry, angryEyeW, angryEyeH, 3, layer, pal.primary, pal.aura, (blinkFactor < 0.6f));

        int lx = (int)((eyeLx + xOffset + tremorX + current_look_x) * 2);
        int rx = (int)((eyeRx + xOffset + tremorX + current_look_x) * 2);
        int topY = (int)((sary - (angryEyeH / 2.0f)) * 2);
        int lw = (int)((angryEyeW + 3) * 2);
        int rw = (int)((angryEyeW + 3) * 2);

        // Pálpebras inclinadas no formato \ /
        if (blinkFactor < 0.85f) {
            draw_canvas_line(layer, lx - lw/2, topY - 3, lx + lw/2, topY + 3, lv_color_hex(0xFF5722), 5);
            draw_canvas_line(layer, rx - rw/2, topY + 3, rx + rw/2, topY - 3, lv_color_hex(0xFF5722), 5);
        } else {
            // No pico da piscada, fecha com uma linha firme
            draw_canvas_line(layer, lx - lw/2, (int)(sary * 2), lx + lw/2, (int)(sary * 2), lv_color_hex(0xFF5722), 4);
            draw_canvas_line(layer, rx - rw/2, (int)(sarry * 2), rx + rw/2, (int)(sarry * 2), lv_color_hex(0xFF5722), 4);
        }

        // Sobrancelhas zangadas grossas vermelhas (\ /) acompanhando a respiração
        draw_canvas_line(layer, (eyeLx - 10 + xOffset + tremorX + current_look_x)*2, (eyeLy - 14 + tremorY + current_look_y + breathY)*2, 
                         (eyeLx + 8 + xOffset + tremorX + current_look_x)*2, (eyeLy - 8 + tremorY + current_look_y + breathY)*2, lv_color_hex(0xFF0000), 5);
        draw_canvas_line(layer, (eyeRx - 8 + xOffset + tremorX + current_look_x)*2, (eyeRy - 8 + tremorY + current_look_y + breathY)*2, 
                         (eyeRx + 10 + xOffset + tremorX + current_look_x)*2, (eyeRy - 14 + tremorY + current_look_y + breathY)*2, lv_color_hex(0xFF0000), 5);
    } else if ((emotion == "sad" || emotion == "crying") && !isSarcastic) {
        // TRISTE / CHORANDO: Olhos caídos em azul oceânico com sobrancelhas tristes
        float sary = eyeLy + tremorY + current_look_y + breathY + 3.0f;
        float sarry = eyeRy + tremorY + current_look_y + breathY + 3.0f;
        float sadEyeH = 15.0f + breathScaleH * 0.7f;
        float sadEyeW = eyeLw + breathScaleW * 0.7f;

        DrawEye(eyeLx + xOffset + tremorX + current_look_x, sary, sadEyeW, sadEyeH, 3, layer, pal.primary, pal.aura, true);
        DrawEye(eyeRx + xOffset + tremorX + current_look_x, sarry, sadEyeW, sadEyeH, 3, layer, pal.primary, pal.aura, true);

        int lx = (int)((eyeLx + xOffset + tremorX + current_look_x) * 2);
        int rx = (int)((eyeRx + xOffset + tremorX + current_look_x) * 2);
        int topY = (int)((sary - (sadEyeH / 2.0f)) * 2);
        int lw = (int)((sadEyeW + 3) * 2);
        int rw = (int)((sadEyeW + 3) * 2);

        // Pálpebra superior reta de corte horizontal
        draw_canvas_line(layer, lx - lw/2, topY, lx + lw/2, topY, pal.aura, 4);
        draw_canvas_line(layer, rx - rw/2, topY, rx + rw/2, topY, pal.aura, 4);

        // Sobrancelhas tristes azuis (/ \)
        draw_canvas_line(layer, (eyeLx - 8 + xOffset + tremorX + current_look_x)*2, (eyeLy - 8 + tremorY + current_look_y + breathY)*2, 
                         (eyeLx + 8 + xOffset + tremorX + current_look_x)*2, (eyeLy - 14 + tremorY + current_look_y + breathY)*2, lv_color_hex(0x0080FF), 4);
        draw_canvas_line(layer, (eyeRx - 8 + xOffset + tremorX + current_look_x)*2, (eyeRy - 14 + tremorY + current_look_y + breathY)*2, 
                         (eyeRx + 8 + xOffset + tremorX + current_look_x)*2, (eyeRy - 8 + tremorY + current_look_y + breathY)*2, lv_color_hex(0x0080FF), 4);
    } else {
        // ESTADO NORMAL / IDLE: Olhos vivos com Respiração Suave, Piscar Orgânico e Paleta Colorida
        float curEyeH = (eyeLh + breathScaleH) * (1.0f - blinker.blinkFactor);
        float curEyeW = (eyeLw + breathScaleW) + (blinker.blinkFactor > 0.85f ? 1.5f : 0.0f);
        float curEyeY = eyeLy + tremorY + current_look_y + breathY;
        
        if (isSarcastic) {
            float blinkFactor = blinker.blinkFactor;
            int lx = (int)((eyeLx + xOffset + tremorX + current_look_x) * 2);
            int rx = (int)((eyeRx + xOffset + tremorX + current_look_x) * 2);
            int cy = (int)((curEyeY + 2.0f) * 2);
            int browBaseY = (int)((curEyeY) * 2);
            int sarW = (int)((eyeLw + 4.0f + breathScaleW) * 2);
            int sarH = (int)(((13.0f + breathScaleH * 0.6f) * (1.0f - blinkFactor)) * 2);

            draw_sarcastic_eyebrows(layer, lx, browBaseY, rx, browBaseY, pal.primary, 0.0f, 0.0f);

            if (blinkFactor < 0.85f) {
                draw_sarcastic_half_closed_eye(layer, lx, cy, sarW, sarH, 8, pal.primary, pal.aura, current_look_x * 0.5f, current_look_y * 0.5f, (blinkFactor < 0.6f));
                draw_sarcastic_half_closed_eye(layer, rx, cy, sarW, sarH, 8, pal.primary, pal.aura, current_look_x * 0.5f, current_look_y * 0.5f, (blinkFactor < 0.6f));
            } else {
                // Na piscada completa, traça uma linha horizontal limpa fechando o olho
                draw_canvas_line(layer, lx - 18, cy, lx + 18, cy, pal.primary, 4);
                draw_canvas_line(layer, rx - 18, cy, rx + 18, cy, pal.primary, 4);
            }
        } else {
            // Olhos normais ou sensíveis cheios de vida e brilho
            DrawEye(eyeLx + xOffset + tremorX + current_look_x, curEyeY, curEyeW, curEyeH, eyeRadius, layer, pal.primary, pal.aura, (curEyeH > 8.0f), current_look_x, current_look_y);
            DrawEye(eyeRx + xOffset + tremorX + current_look_x, curEyeY, curEyeW, curEyeH, eyeRadius, layer, pal.primary, pal.aura, (curEyeH > 8.0f), current_look_x, current_look_y);
        }
    }
    
    // =========================================================================
    // 5. BOCHECHAS CORADAS (BLUSH) FOFAS SOB OS OLHOS
    // =========================================================================
    if (pal.hasCheeks && !isDizzy && emotion != "angry" && emotion != "sleeping" && idleTipo != 15) {
        float cheekDx = current_look_x;
        float cheekY = eyeLy + 13.0f + tremorY + current_look_y + breathY * 0.75f;
        if (idleTipo == 12) {
            cheekDx = sinf(ms * 0.0022f) * 1.25f;
            cheekY = 50.0f;
        } else if (idleTipo == 14) {
            cheekDx = 0.0f;
            cheekY = 49.5f;
        } else if (idleTipo == 16) {
            cheekDx = 0.0f;
            cheekY = 50.5f;
        } else if (idleTipo == 7) {
            cheekDx = sinf(ms * 0.0028f) * 2.5f;
            cheekY = 48.5f + cosf(ms * 0.0056f) * 1.25f;
        } else if (idleTipo == 17) {
            cheekDx = 0.0f;
            cheekY = 49.0f - fabsf(sinf(ms * 0.0055f)) * 2.0f;
        } else if (idleTipo == 11) {
            cheekDx = 0.0f;
            cheekY = 45.0f - fabsf(sinf(ms * 0.014f)) * 3.25f;
        } else if (comendo) {
            float elapsed = (float)(ms % 3000);
            float cycle = elapsed / 3000.0f;
            bool isLicking = (cycle >= 0.72f);
            float chewPhase = sinf(ms * 0.016f);
            bool biteOpen = (!isLicking && chewPhase > 0.15f);
            float chewSide = sinf(ms * 0.008f);
            float leftPuff = (!isLicking && !biteOpen && chewSide < 0.0f) ? 1.45f : 1.0f;
            float rightPuff = (!isLicking && !biteOpen && chewSide > 0.0f) ? 1.45f : 1.0f;
            DrawCheekBlush(eyeLx - 4.0f + xOffset, eyeLy + 12.0f, 5.0f * leftPuff, 2.8f * leftPuff, pal.cheek, layer);
            DrawCheekBlush(eyeRx + 4.0f + xOffset, eyeLy + 12.0f, 5.0f * rightPuff, 2.8f * rightPuff, pal.cheek, layer);
            goto skip_default_cheeks;
        } else if (brincando) {
            float hopPhase = ms * 0.0075f;
            float bSwayX = sinf(hopPhase * 0.5f) * 4.5f;
            float bBounceY = -fabsf(sinf(hopPhase)) * 3.5f;
            cheekDx = bSwayX;
            cheekY = eyeLy + 12.0f + bBounceY;
        } else if (curando) {
            cheekDx = 0.0f;
            cheekY = eyeLy + 13.0f;
        }
        DrawCheekBlush(eyeLx - 3.0f + xOffset + cheekDx, cheekY, 5.0f, 2.5f, pal.cheek, layer);
        DrawCheekBlush(eyeRx + 3.0f + xOffset + cheekDx, cheekY, 5.0f, 2.5f, pal.cheek, layer);
skip_default_cheeks: ;
    }
    
    // =========================================================================
    // 6. RENDERIZAÇÃO DA BOCA ARTICULADA E SINCRONIZADA COM A RESPIRAÇÃO
    // =========================================================================
    int mouthShiftX = (int)((current_look_x + swayX) * 0.85f);
    int mouthShiftY = (int)((current_look_y + swayY + breathY) * 0.80f) + 4;
    lv_color_t mc = pal.mouth;
    
    if (isDizzy) {
        // Boca em zigue-zague ondulado de tontura
        int my = 49 + mouthShiftY;
        draw_canvas_line(layer, (58 + mouthShiftX)*2, (my)*2, (62 + mouthShiftX)*2, (my + 4)*2, mc, 4);
        draw_canvas_line(layer, (62 + mouthShiftX)*2, (my + 4)*2, (66 + mouthShiftX)*2, (my - 3)*2, mc, 4);
        draw_canvas_line(layer, (66 + mouthShiftX)*2, (my - 3)*2, (70 + mouthShiftX)*2, (my)*2, mc, 4);
    } else if (comendo) {
        // Boca aberta comendo / mastigando / lambendo os beiços
        float elapsed = (float)(ms % 3000);
        float cycle = elapsed / 3000.0f;
        bool isLicking = (cycle >= 0.72f);
        float chewPhase = sinf(ms * 0.016f);
        bool biteOpen = (!isLicking && chewPhase > 0.15f);
        float chewBounceY = isLicking ? -2.0f : (biteOpen ? -3.5f : 3.0f);
        int mx = 128 + xOffset * 2;
        int my = (int)roundf(100.0f + chewBounceY * 0.6f);

        if (isLicking) {
            // Linguinha rosa no canto da boca lambendo os beiços
            draw_canvas_disc(layer, mx + 10, my + 3, 5, lv_color_hex(0xFF5252));
            // Sorriso 'w'
            draw_canvas_line(layer, mx - 12, my - 2, mx - 6, my + 4, mc, 3);
            draw_canvas_line(layer, mx - 6, my + 4, mx, my - 1, mc, 3);
            draw_canvas_line(layer, mx, my - 1, mx + 6, my + 4, mc, 3);
            draw_canvas_line(layer, mx + 6, my + 4, mx + 12, my - 2, mc, 3);
        } else if (biteOpen) {
            draw_canvas_rect(layer, mx - 13, my - 6, 26, 16, lv_color_hex(0x4A0E17), 8);
            draw_canvas_rect(layer, mx - 8, my + 2, 16, 7, lv_color_hex(0xFF5252), 3);
            draw_canvas_rect_empty(layer, mx - 13, my - 6, 26, 16, mc, 2, 8);
        } else {
            draw_canvas_quad_bezier(layer, mx - 11.0f, (float)(my + 1), mx - 5.0f, (float)(my - 4), (float)mx, (float)(my + 1), mc, 4, 4);
            draw_canvas_quad_bezier(layer, (float)mx, (float)(my + 1), mx + 5.0f, (float)(my + 6), mx + 11.0f, (float)(my + 1), mc, 4, 4);
        }
    } else if (brincando) {
        // Boca aberta comemorando com gatinho 'w' no topo e sorriso largo embaixo
        float hopPhase = ms * 0.0075f;
        float bSwayX = sinf(hopPhase * 0.5f) * 9.0f;
        float bBounceY = -fabsf(sinf(hopPhase)) * 7.5f;
        int mx = (int)roundf(128.0f + xOffset * 2.0f + bSwayX * 0.85f);
        int my = (int)roundf(100.0f + bBounceY * 0.75f);

        draw_canvas_triangle(layer, mx - 12, my - 2, mx + 12, my - 2, mx, my + 14, lv_color_hex(0xFF4081));
        draw_canvas_line(layer, mx - 12, my - 2, mx - 5, my + 2, lv_color_hex(0xFFFFFF), 3);
        draw_canvas_line(layer, mx - 5, my + 2, mx, my - 2, lv_color_hex(0xFFFFFF), 3);
        draw_canvas_line(layer, mx, my - 2, mx + 5, my + 2, lv_color_hex(0xFFFFFF), 3);
        draw_canvas_line(layer, mx + 5, my + 2, mx + 12, my - 2, lv_color_hex(0xFFFFFF), 3);
        draw_canvas_quad_bezier(layer, mx - 12.0f, (float)(my - 2), (float)mx, (float)(my + 16), mx + 12.0f, (float)(my - 2), lv_color_hex(0xFFFFFF), 3, 5);
    } else if (curando) {
        float cycle = fmodf(ms * 0.000333f, 1.0f);
        int mx = 128 + xOffset * 2;
        if (cycle < 0.26f) {
            float p = cycle / 0.26f;
            if (p < 0.75f) {
                draw_canvas_rect(layer, mx - 6, 98, 12, 10, lv_color_hex(0xFFFFFF), 5);
            } else {
                draw_canvas_line(layer, mx - 8, 104, mx + 8, 104, lv_color_hex(0xFFFFFF), 4);
            }
        } else {
            float hp = (cycle - 0.26f) / 0.74f;
            float floatY = -sinf(hp * 3.14159f) * 5.0f;
            draw_canvas_quad_bezier(layer, mx - 12.0f, 101.0f + floatY, (float)mx, 111.0f + floatY, mx + 12.0f, 101.0f + floatY, lv_color_hex(0xFFFFFF), 4, 6);
        }
    } else if (emotion == "sleeping") {
        draw_canvas_rect(layer, (62 + mouthShiftX)*2, (48 + mouthShiftY)*2, 4*2, 4*2, mc, 2);
    } else if (idleTipo == 1) {
        // BÁSICA 1: Assobiando no Ritmo (boca em 'o' deslocada pro lado pulsando + 3 notas musicais em onda)
        float beat = ms * 0.005f;
        float wSwayX = sinf(beat * 0.5f) * 6.0f;
        float bopY = fabsf(sinf(beat)) * 4.0f;
        int mouthR = (int)roundf(4.5f + sinf(beat * 2.0f) * 1.2f);
        int mx = (int)roundf(132.0f + wSwayX * 0.8f);
        int my = (int)roundf(102.0f + bopY * 0.7f);
        draw_canvas_disc(layer, mx, my, mouthR, lv_color_hex(0xFFFFFF));
        draw_canvas_disc(layer, mx, my, std::max(2, mouthR - 2), lv_color_hex(0x000000));

        for (int i = 0; i < 3; i++) {
            float p = fmodf(ms * 0.00075f + i * 0.33f, 1.0f);
            int nx = (int)roundf(mx + 8.0f + p * 42.0f + sinf(p * 7.0f + i) * 5.0f);
            int ny = (int)roundf(my - 4.0f - p * 62.0f);
            lv_color_t nCol = (i % 2 == 0) ? lv_color_hex(0xFFD600) : lv_color_hex(0x00E5FF);
            draw_canvas_disc(layer, nx, ny, 3, nCol);
            draw_canvas_line(layer, nx + 2, ny, nx + 2, ny - 9, nCol, 2);
            draw_canvas_line(layer, nx + 2, ny - 9, nx + 7, ny - 6, nCol, 2);
        }
    } else if (idleTipo == 10) {
        // BÁSICA 2: Piscadela Charmosa (sorriso de canto confiante smirk)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2600);
        float animElapsed = fmodf(elapsed, 2600.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 2600.0f));
        bool isWinking = (cycle > 0.15f && cycle < 0.82f);
        float my = 104.0f + (isWinking ? -1.8f : 0.0f);
        if (isWinking) {
            draw_canvas_quad_bezier(layer, 118.0f, my + 2.0f, 130.0f, my + 4.0f, 141.0f, my - 5.0f, lv_color_hex(0xFFFFFF), 4, 6);
        } else {
            draw_canvas_line(layer, 120, (int)my - 2, 124, (int)my + 2, mc, 3);
            draw_canvas_line(layer, 124, (int)my + 2, 128, (int)my - 2, mc, 3);
            draw_canvas_line(layer, 128, (int)my - 2, 132, (int)my + 2, mc, 3);
            draw_canvas_line(layer, 132, (int)my + 2, 136, (int)my - 2, mc, 3);
        }
    } else if (idleTipo == 11) {
        // BÁSICA 3: Risadinha ("Hehehe") (boca grande em 'D' aberta rindo com linguinha rosa dentro)
        float laughBounce = -fabsf(sinf(ms * 0.014f)) * 6.5f;
        int mx = 128;
        int my = (int)roundf(98.0f + laughBounce * 0.8f);
        int mouthOpenH = (int)roundf(11.0f + fabsf(sinf(ms * 0.014f)) * 5.0f);

        for (int dy = 0; dy <= mouthOpenH; dy++) {
            float p = (float)dy / (float)mouthOpenH;
            int halfW = (int)roundf(14.0f * sqrtf(std::max(0.0f, 1.0f - p * p)));
            draw_canvas_line(layer, mx - halfW, my + dy, mx + halfW, my + dy, lv_color_hex(0x4A0E17), 2);
        }
        int tongueY = my + (int)roundf(mouthOpenH * 0.68f);
        draw_canvas_rect(layer, mx - 7, tongueY - 3, 14, 7, lv_color_hex(0xFF5252), 3);
        draw_canvas_line(layer, mx - 14, my, mx + 14, my, lv_color_hex(0xFFFFFF), 3);
        draw_canvas_quad_bezier(layer, mx - 14.0f, (float)my, (float)mx, (float)(my + mouthOpenH * 1.6f), mx + 14.0f, (float)my, lv_color_hex(0xFFFFFF), 3, 6);
    } else if (idleTipo == 18) {
        // BÁSICA 4: Curioso / Lupa Investigativa (boquinha em 'o' pequeno acompanhando o olhar)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3000);
        float animElapsed = fmodf(elapsed, 3000.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3000.0f));
        float lookX = 0.0f;
        if (cycle < 0.45f) lookX = sinf((cycle / 0.45f) * 3.14159265f) * -10.0f;
        else if (cycle < 0.90f) lookX = sinf(((cycle - 0.45f) / 0.45f) * 3.14159265f) * 10.0f;
        int mx = (int)roundf(128.0f + lookX * 0.7f);
        int my = 105;
        draw_canvas_rect(layer, mx - 4, my - 4, 8, 9, lv_color_hex(0xFFFFFF), 4);
        draw_canvas_rect(layer, mx - 2, my - 2, 4, 5, lv_color_hex(0x000000), 2);
    } else if (idleTipo == 19) {
        // BÁSICA 5: Descolado / Óculos Escuros (sorrisinho de lado confiante)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3200);
        float animElapsed = fmodf(elapsed, 3200.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3200.0f));
        float nodY = (cycle >= 0.22f && cycle < 0.34f) ? (sinf(((cycle - 0.22f) / 0.12f) * 3.14159265f) * 4.5f) : 0.0f;
        float my = 105.0f + nodY * 0.6f;
        if (cycle >= 0.22f && cycle <= 0.86f) {
            draw_canvas_quad_bezier(layer, 118.0f, my + 2.0f, 130.0f, my + 4.0f, 142.0f, my - 5.0f, lv_color_hex(0xFFFFFF), 4, 6);
        } else {
            draw_canvas_line(layer, 120, (int)roundf(my), 136, (int)roundf(my), lv_color_hex(0xFFFFFF), 4);
        }
    } else if (idleTipo == 20) {
        // BÁSICA 6: Ideia ("Eureka!") (boca pensativa torta na fase 1 -> sorriso aberto na fase 2)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3000);
        float animElapsed = fmodf(elapsed, 3000.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3000.0f));
        if (cycle < 0.48f) {
            draw_canvas_line(layer, 120, 104, 134, 101, lv_color_hex(0xFFFFFF), 4);
        } else {
            float ep = (cycle - 0.48f) / 0.52f;
            float jumpY = (ep < 0.25f) ? (-sinf((ep / 0.25f) * 3.14159265f) * 8.0f) : 0.0f;
            draw_canvas_quad_bezier(layer, 116.0f, 102.0f + jumpY, 128.0f, 112.0f + jumpY, 140.0f, 102.0f + jumpY, lv_color_hex(0xFFFFFF), 4, 6);
        }
    } else if (idleTipo == 8) {
        // Bocejo de tédio: boca abre progressivamente em grande oval 'O' + sopro de suspiro
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3000);
        float animElapsed = fmodf(elapsed, 3000.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3000.0f));
        float yawnP = 0.0f;
        float shakeX = 0.0f;
        if (cycle < 0.35f) {
            float u = cycle / 0.35f;
            yawnP = u * u * (3.0f - 2.0f * u);
        } else if (cycle < 0.72f) {
            yawnP = 1.0f;
            shakeX = sinf(ms * 0.035f) * 1.2f;
        } else {
            float u = (cycle - 0.72f) / 0.28f;
            yawnP = 1.0f - (u * u * (3.0f - 2.0f * u));
        }
        int mx = (int)roundf(128.0f + shakeX);
        int my = (int)roundf(104.0f - yawnP * 2.5f);
        if (yawnP < 0.12f) {
            draw_sarcastic_mouth(layer, mx, my, mc);
        } else {
            int mw = (int)roundf(8.0f + yawnP * 14.0f);
            int mh = (int)roundf(4.0f + yawnP * 16.0f);
            int rad = (mw < mh ? mw : mh) / 2;
            draw_canvas_rect(layer, mx - mw / 2, my - mh / 2, mw, mh, mc, rad);
            if (mh > 8) {
                int iw = mw - 6 > 4 ? mw - 6 : 4;
                int ih = mh - 6 > 4 ? mh - 6 : 4;
                int irad = (iw < ih ? iw : ih) / 2;
                draw_canvas_rect(layer, mx - iw / 2, my - ih / 2, iw, ih, lv_color_hex(0x000000), irad);
            }
        }
        // Sopro de bocejo '~' flutuando
        if (cycle > 0.50f && cycle < 0.92f) {
            float pWave = (cycle - 0.50f) / 0.42f;
            float wx = 152.0f + pWave * 16.0f;
            float wy = 96.0f - pWave * 18.0f;
            draw_canvas_quad_bezier(layer, wx - 6.0f, wy, wx - 3.0f, wy - 4.0f, wx, wy, pal.primary, 2, 4);
            draw_canvas_quad_bezier(layer, wx, wy, wx + 3.0f, wy + 4.0f, wx + 6.0f, wy, pal.primary, 2, 4);
        }
    } else if (idleTipo == 9) {
        // Olhos semicerrados + gota grande: boca curvada acompanhando a descida do suspiro
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2800);
        float animElapsed = fmodf(elapsed, 2800.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 2800.0f));
        float sinkY = 0.0f;
        if (cycle < 0.25f) sinkY = (cycle / 0.25f) * 4.0f;
        else if (cycle < 0.75f) sinkY = 4.0f + sinf((cycle - 0.25f) * 12.56637f) * 0.8f;
        else sinkY = (1.0f - (cycle - 0.75f) / 0.25f) * 4.0f;
        draw_sarcastic_mouth(layer, 128, (int)roundf(105.0f + sinkY), mc);
    } else if (idleTipo == 2) {
        // Revirar os olhos: boca curvada acompanhando levemente as pupilas
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2600);
        float animElapsed = fmodf(elapsed, 2600.0f);
        float pupilX = 0.0f;
        if (animElapsed < 2000.0f) {
            float p = animElapsed / 2000.0f;
            float angle = 3.14159265f * 0.5f + p * 3.14159265f * 2.0f;
            pupilX = cosf(angle) * 11.0f;
        }
        draw_sarcastic_mouth(layer, (int)roundf(128.0f + pupilX * 0.25f), 106, mc);
    } else if (idleTipo == 21) {
        // SARCASTICA 4 (VÍVIDO): Julgamento Silencioso (smirk contraindo + balanço de cabeça)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3400);
        float animElapsed = fmodf(elapsed, 3400.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3400.0f));
        float lookSideP = 0.0f;
        float headShake = 0.0f;
        if (cycle >= 0.20f && cycle < 0.40f) {
            float p = (cycle - 0.20f) / 0.20f;
            lookSideP = p * p * (3.0f - 2.0f * p);
        } else if (cycle >= 0.40f && cycle < 0.80f) {
            lookSideP = 1.0f;
            float shakePhase = (cycle - 0.40f) / 0.40f;
            headShake = sinf(shakePhase * 18.84955f) * 3.8f;
        } else if (cycle >= 0.80f) {
            float p = (cycle - 0.80f) / 0.20f;
            lookSideP = 1.0f - (p * p * (3.0f - 2.0f * p));
        }
        int mx = (int)roundf(128.0f + headShake * 0.7f);
        int my = 105;
        float smirkLift = lookSideP * 3.0f;
        draw_canvas_quad_bezier(layer, mx - 12.0f, my + 3.0f, (float)mx, my - 1.0f, mx + 14.0f, my - smirkLift, mc, 4, 6);
        if (lookSideP > 0.5f) {
            draw_canvas_line(layer, mx + 14, (int)(my - smirkLift), mx + 16, (int)(my - smirkLift - 3.0f), mc, 3);
        }
    } else if (idleTipo == 22) {
        // SARCASTICA 5 (VÍVIDO): Risada Maliciosa / Deboche 😈 (smirk largo com dentinho de deboche afiado)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2800);
        float animElapsed = fmodf(elapsed, 2800.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 2800.0f));
        float bounceY = 0.0f;
        float laughClimax = 0.0f;
        if (cycle < 0.25f) {
            float p = cycle / 0.25f;
            bounceY = -fabsf(sinf(p * 12.56637f)) * 2.0f;
        } else if (cycle < 0.75f) {
            float p = (cycle - 0.25f) / 0.50f;
            bounceY = -fabsf(sinf(p * 25.13274f)) * 4.5f;
            laughClimax = 1.0f;
        } else {
            float p = (cycle - 0.75f) / 0.25f;
            bounceY = -(1.0f - p) * 1.5f;
            laughClimax = 1.0f - p;
        }
        int mx = 128;
        int my = (int)roundf(104.0f + bounceY * 0.6f);
        draw_canvas_quad_bezier(layer, mx - 12.0f, my + 2.0f, (float)(mx + 2), my + 7.0f, mx + 18.0f, my - 6.0f, mc, 4, 8);
        if (laughClimax > 0.3f) {
            // Dentinho pontudo de diabinho no canto do sorriso
            draw_canvas_triangle(layer, mx + 11, my - 1, mx + 15, my - 4, mx + 12, my + 3, lv_color_hex(0xFFFFFF));
        }
    } else if (idleTipo == 23) {
        // SARCASTICA 6 (VÍVIDO): ZZz Sarcástico / Dormindo 🥱 (bolha de sono infla e estoura)
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3800);
        float animElapsed = fmodf(elapsed, 3800.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3800.0f));
        float headDrop = 0.0f;
        float bubbleSize = 0.0f;
        bool popTrigger = false;
        if (cycle < 0.55f) {
            float p = cycle / 0.55f;
            headDrop = sinf(p * 1.57079f) * 5.0f;
            bubbleSize = sinf(p * 3.14159f) * 10.0f;
        } else if (cycle < 0.62f) {
            headDrop = 5.0f - (cycle - 0.55f) * 20.0f;
            popTrigger = true;
        } else if (cycle < 0.85f) {
            headDrop = 2.0f;
        } else {
            float p = (cycle - 0.85f) / 0.15f;
            headDrop = 2.0f * (1.0f - p);
        }
        int mx = 128;
        int my = (int)roundf(104.0f + headDrop * 0.7f);
        draw_canvas_line(layer, mx - 8, my, mx + 8, my - 1, mc, 3);

        // Bolha de sono translúcida 🫧
        int bx = mx + 12;
        int by = my - 3;
        if (bubbleSize > 1.5f && !popTrigger) {
            int bs = (int)bubbleSize;
            draw_canvas_disc(layer, bx + (int)(bs * 0.6f), by - (int)(bs * 0.6f), bs, lv_color_hex(0x80D8FF));
            draw_canvas_disc(layer, bx + (int)(bs * 0.35f), by - (int)(bs * 0.95f), std::max(1, (int)(bs * 0.28f)), lv_color_hex(0xFFFFFF));
        }
        if (popTrigger) {
            // Estouro com partículas
            for (int i = 0; i < 4; i++) {
                float ang = (i * 1.57079f) + 0.4f;
                int px = bx + 6 + (int)(cosf(ang) * 12.0f);
                int py = by - 6 + (int)(sinf(ang) * 12.0f);
                draw_canvas_disc(layer, px, py, 2, lv_color_hex(0x80D8FF));
            }
        }
    } else if (idleTipo == 12) {
        // Boca Olhinhos Pidão: arco pequeno pidão encolhido
        float mx = 128.0f + sinf(ms * 0.0022f) * 2.0f;
        draw_canvas_quad_bezier(layer, mx - 6.0f, 107.0f, mx, 102.0f, mx + 6.0f, 107.0f, lv_color_hex(0xFFFFFF), 3, 5);
    } else if (idleTipo == 15) {
        // Boca Tímido / Corado: linha ondulada tímida (﹏)
        float shyWiggleX = sinf(ms * 0.0045f) * 4.5f;
        float sinkY = 5.0f + sinf(ms * 0.0022f) * 1.5f;
        float mx = 128.0f + shyWiggleX * 0.7f;
        float my = 102.0f + sinkY;
        lv_color_t mCol = lv_color_hex(0xFF80AB);
        draw_canvas_quad_bezier(layer, mx - 12.0f, my, mx - 8.0f, my + 4.0f, mx - 4.0f, my, mCol, 3, 4);
        draw_canvas_quad_bezier(layer, mx - 4.0f, my, mx, my - 4.0f, mx + 4.0f, my, mCol, 3, 4);
        draw_canvas_quad_bezier(layer, mx + 4.0f, my, mx + 8.0f, my + 4.0f, mx + 12.0f, my, mCol, 3, 4);
    } else if (idleTipo == 14) {
        // Boca Piscadela + Beijo Voador: biquinho '3' + coração grande voando em onda
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 2800);
        float animElapsed = fmodf(elapsed, 2800.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 2800.0f));
        bool isWinking = (cycle > 0.12f && cycle < 0.85f);
        float headTiltY = isWinking ? -3.0f : 0.0f;
        float mx = 128.0f;
        float my = 102.0f + headTiltY;

        if (isWinking) {
            lv_color_t kissCol = lv_color_hex(0xFF69B4);
            draw_canvas_quad_bezier(layer, mx - 1.0f, my - 6.0f, mx + 6.0f, my - 3.0f, mx - 1.0f, my, kissCol, 3, 5);
            draw_canvas_quad_bezier(layer, mx - 1.0f, my, mx + 6.0f, my + 3.0f, mx - 1.0f, my + 6.0f, kissCol, 3, 5);
        } else {
            draw_canvas_line(layer, 120, (int)my - 2, 124, (int)my + 2, mc, 3);
            draw_canvas_line(layer, 124, (int)my + 2, 128, (int)my - 2, mc, 3);
            draw_canvas_line(layer, 128, (int)my - 2, 132, (int)my + 2, mc, 3);
            draw_canvas_line(layer, 132, (int)my + 2, 136, (int)my - 2, mc, 3);
        }
        if (cycle > 0.22f && cycle < 0.92f) {
            float kp = (cycle - 0.22f) / 0.70f;
            int hx = (int)roundf(mx + 8.0f + kp * 55.0f);
            int hy = (int)roundf(my - kp * 45.0f + sinf(kp * 9.42477f) * 6.0f);
            float scale = (kp < 0.25f) ? (0.4f + (kp / 0.25f) * 1.1f) : (1.5f - (kp - 0.25f) * 0.7f);
            draw_scalable_heart(layer, hx, hy, scale, lv_color_hex(0xFF1493));
        }
    } else if (idleTipo == 16) {
        // Boca Carente / Dengoso: boquinha trêmula acompanhando a fungadinha
        float elapsed = (tFim > tStart && ms >= tStart) ? (float)(ms - tStart) : (float)(ms % 3000);
        float animElapsed = fmodf(elapsed, 3000.0f);
        float cycle = std::max(0.0f, std::min(1.0f, animElapsed / 3000.0f));
        float sniffY = 0.0f;
        if (cycle > 0.45f && cycle < 0.55f) sniffY = -4.0f;
        else if (cycle > 0.60f && cycle < 0.70f) sniffY = -5.0f;
        float mx = 128.0f + sinf(ms * 0.025f) * 1.0f;
        float my = 105.0f + sniffY;
        draw_canvas_quad_bezier(layer, mx - 10.0f, my + 4.0f, mx, my - 4.0f, mx + 10.0f, my + 4.0f, lv_color_hex(0x80D8FF), 4, 6);
    } else if (idleTipo == 7) {
        // Boca Apaixonado nas Nuvens: sorriso aberto apaixonado acompanhando o balanço
        float loveSwayX = sinf(ms * 0.0028f) * 4.0f;
        float loveSwayY = cosf(ms * 0.0056f) * 2.0f;
        float mx = 128.0f + loveSwayX;
        float my = 102.0f + loveSwayY;
        draw_canvas_quad_bezier(layer, mx - 9.0f, my - 1.0f, mx, my + 7.0f, mx + 9.0f, my - 1.0f, lv_color_hex(0xFF69B4), 4, 6);
    } else if (idleTipo == 17) {
        // Boca Maravilhado / Olhos Estrelados: sorriso radiante preenchido ('D')
        float bounceY = -fabsf(sinf(ms * 0.0055f)) * 2.8f;
        int mx = 128;
        int my = (int)roundf(102.0f + bounceY);
        draw_canvas_triangle(layer, mx - 8, my - 3, mx + 8, my - 3, mx, my + 7, lv_color_hex(0xFF4081));
        draw_canvas_quad_bezier(layer, mx - 8.0f, my - 3.0f, (float)mx, my + 9.0f, mx + 8.0f, my - 3.0f, lv_color_hex(0xFF4081), 3, 5);
    } else if ((emotion == "sad" || emotion == "crying") && !isSarcastic) {
        draw_canvas_line(layer, (58 + mouthShiftX)*2, (51 + mouthShiftY)*2, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, mc, 4);
        draw_canvas_line(layer, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, (70 + mouthShiftX)*2, (51 + mouthShiftY)*2, mc, 4);
    } else if (emotion == "surprised") {
        draw_canvas_rect(layer, (62 + mouthShiftX)*2, (47 + mouthShiftY)*2, 4*2, 6*2, mc, 2);
    } else if (emotion == "happy" && !isSarcastic) {
        // Boca fofa w de gatinho
        draw_canvas_line(layer, (60 + mouthShiftX)*2, (48 + mouthShiftY)*2, (62 + mouthShiftX)*2, (50 + mouthShiftY)*2, mc, 3);
        draw_canvas_line(layer, (62 + mouthShiftX)*2, (50 + mouthShiftY)*2, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, mc, 3);
        draw_canvas_line(layer, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, (66 + mouthShiftX)*2, (50 + mouthShiftY)*2, mc, 3);
        draw_canvas_line(layer, (66 + mouthShiftX)*2, (50 + mouthShiftY)*2, (68 + mouthShiftX)*2, (48 + mouthShiftY)*2, mc, 3);
    } else if (emotion == "angry") {
        draw_canvas_line(layer, (58 + mouthShiftX)*2, (49 + mouthShiftY)*2, (70 + mouthShiftX)*2, (49 + mouthShiftY)*2, mc, 4);
    } else if (isSarcastic) {
        // BOCA PADRÃO SARCÁSTICA: Arco curvado para baixo (︵) igual à imagem de referência
        draw_sarcastic_mouth(layer, (64 + mouthShiftX) * 2, (49 + mouthShiftY) * 2, mc);
    } else {
        // HUMOR NORMAL: Boca fofa de gatinho (w)
        draw_canvas_line(layer, (60 + mouthShiftX)*2, (48 + mouthShiftY)*2, (62 + mouthShiftX)*2, (50 + mouthShiftY)*2, mc, 3);
        draw_canvas_line(layer, (62 + mouthShiftX)*2, (50 + mouthShiftY)*2, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, mc, 3);
        draw_canvas_line(layer, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, (66 + mouthShiftX)*2, (50 + mouthShiftY)*2, mc, 3);
        draw_canvas_line(layer, (66 + mouthShiftX)*2, (50 + mouthShiftY)*2, (68 + mouthShiftX)*2, (48 + mouthShiftY)*2, mc, 3);
    }
    
    float temp = engine.GetSensorTemperatura();
    float limiarFrio = engine.GetLimiarTempBaixo();
    if ((temp < limiarFrio && temp > 0.0f) || emotion == "cold") {
        int tremorMouth = (ms % 100 < 50) ? 2 : -2;
        draw_canvas_line(layer, (54 + mouthShiftX)*2, (48 + mouthShiftY + tremorMouth)*2, (60 + mouthShiftX)*2, (48 + mouthShiftY - tremorMouth)*2, lv_color_hex(0x00FFFF), 3);
        draw_canvas_line(layer, (60 + mouthShiftX)*2, (48 + mouthShiftY - tremorMouth)*2, (66 + mouthShiftX)*2, (48 + mouthShiftY + tremorMouth)*2, lv_color_hex(0x00FFFF), 3);
        draw_canvas_line(layer, (66 + mouthShiftX)*2, (48 + mouthShiftY + tremorMouth)*2, (72 + mouthShiftX)*2, (48 + mouthShiftY - tremorMouth)*2, lv_color_hex(0x00FFFF), 3);
    }
 
    // =========================================================================
    // 7. BALÃO MODERNO HUD DE NECESSIDADES (FOME, BRINCAR, SAÚDE), TIMER OU MÚSICA
    // =========================================================================
    // Suporte a carrossel dinâmico (Timer, Mídias e Status do Pet a cada 10s)
    size_t mediaCount = engine.GetMediaCount();

    bool mostrarTimer = false;
    bool mostrarMedia = false;
    bool mostrarPetStatus = false;
    int activeMediaIdx = 0;

    // Estado do Slider Indicator
    bool drawSliderDots = false;
    int activeSlotIdx = 0;
    int hudBx = 10;
    lv_color_t activeDotColor = lv_color_hex(0x38BDF8);

    int totalCarouselSlots = (timerAtivo ? 1 : 0) + (int)mediaCount + (petStatusAtivo ? 1 : 0);

    if (totalCarouselSlots > 0) {
        if (timerAtivo && (engine.GetTimerRemainingMs() / 1000) <= 30) {
            // Últimos 30s do timer: prioridade total para o alarme
            mostrarTimer = true;
            mostrarMedia = false;
            mostrarPetStatus = false;
        } else if (totalCarouselSlots == 1) {
            if (timerAtivo) {
                mostrarTimer = true;
            } else if ((int)mediaCount > 0) {
                mostrarMedia = true;
                activeMediaIdx = 0;
            } else if (petStatusAtivo) {
                mostrarPetStatus = true;
            }
        } else {
            // Alterna a cada 10 segundos entre os slots disponíveis
            int currentSlot = (int)((ms / 10000) % totalCarouselSlots);
            int slotIdx = 0;

            if (timerAtivo) {
                if (currentSlot == slotIdx) {
                    mostrarTimer = true;
                }
                slotIdx++;
            }

            if (!mostrarTimer) {
                if ((int)mediaCount > 0 && currentSlot < (slotIdx + (int)mediaCount)) {
                    mostrarMedia = true;
                    activeMediaIdx = currentSlot - slotIdx;
                    if (activeMediaIdx < 0 || activeMediaIdx >= (int)mediaCount) {
                        activeMediaIdx = 0;
                    }
                } else if (petStatusAtivo) {
                    mostrarPetStatus = true;
                }
            }
        }
    }

    if (mostrarTimer) {
        uint32_t rem = engine.GetTimerRemainingMs() / 1000;
        uint32_t total = engine.GetTimerDurationMs() / 1000;
        if (total == 0) total = 1;
        uint32_t m = rem / 60;
        uint32_t s = rem % 60;
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%02u:%02u", (unsigned int)(m % 100), (unsigned int)(s % 60));
        
        int percent = (int)((rem * 100) / total);
        if (percent > 100) percent = 100;
        if (percent < 0) percent = 0;

        // =====================================================================
        // ANEL ORBITAL CIRCULAR EXPANDIDO (108 x 28 px)
        // =====================================================================
        int bw = 108;
        int bh = 24;
        int bx = 128 - (bw / 2);
        int by = 0;

        drawSliderDots = true;
        activeSlotIdx = 0;
        hudBx = bx;
        
        // 1. Fundo da cápsula com cantos perfeitamente arredondados (quase colada ao relógio)
        draw_canvas_rect(layer, bx, by, bw, bh, lv_color_hex(0x0C0F17), 12);
        draw_canvas_rect_empty(layer, bx, by, bw, bh, lv_color_hex(0x272F40), 1, 12);
        
        // 2. Cor dinâmica inteligente por urgência do timer
        lv_color_t arcColor;
        if (percent > 50) {
            arcColor = lv_color_hex(0x10B981); // Verde esmeralda vivo
        } else if (percent > 20) {
            arcColor = lv_color_hex(0xF59E0B); // Âmbar vibrante
        } else {
            arcColor = lv_color_hex(0xEF4444); // Vermelho de urgência
        }
        activeDotColor = arcColor;
        
        // 3. Anel Orbital Circular (lado esquerdo da cápsula)
        int arcX = bx + 14;
        int arcY = by + 12;
        int arcR = 7;
        
        // Trilho de fundo do anel (360° cinza escuro)
        draw_canvas_arc(layer, arcX, arcY, arcR, 0, 360, lv_color_hex(0x222634), 2);
        
        // Arco ativo de progresso (diminui em sentido horário a partir do topo - 12h)
        if (percent >= 100) {
            draw_canvas_arc(layer, arcX, arcY, arcR, 0, 360, arcColor, 2);
        } else if (percent > 0) {
            int sweep = (360 * percent) / 100;
            if (sweep < 4) sweep = 4;
            if (sweep >= 360) {
                draw_canvas_arc(layer, arcX, arcY, arcR, 0, 360, arcColor, 2);
            } else {
                int endAngle = (270 + sweep) % 360;
                draw_canvas_arc(layer, arcX, arcY, arcR, 270, endAngle, arcColor, 2);
            }
        }
        
        // Ponto perfeitamente centralizado no núcleo da barra circular
        draw_canvas_disc(layer, arcX, arcY, 2, arcColor);
        
        // 4. Dígitos digitais perfeitamente centralizados no espaço restante à direita
        int textX = bx + 36;
        int textY = by + 5;
        for (int i = 0; i < 5; i++) {
            DrawTimerSegmentChar(layer, textX + i * 12, textY, timeStr[i], lv_color_hex(0xF0F6FC), lv_color_hex(0x000000));
        }
    } else if (mostrarMedia) {
        // =====================================================================
        // MÚSICA HUD: ANEL ORBITAL EXPANDIDO + MARQUEE (216 x 28 px)
        // =====================================================================
        const TamagotchiEngine::TamagotchiMediaItem* curMedia = engine.GetMediaItem((size_t)activeMediaIdx);
        std::string title = curMedia ? curMedia->title : engine.GetMediaTitle();
        std::string artist = curMedia ? curMedia->artist : engine.GetMediaArtist();
        std::string device = curMedia ? curMedia->device : engine.GetMediaDevice();
        int dur = curMedia ? curMedia->duration : engine.GetMediaDuration();
        int pos = engine.GetMediaPosition((size_t)activeMediaIdx);

        int percent = (dur > 0) ? (pos * 100) / dur : 0;
        if (percent > 100) percent = 100;
        if (percent < 0) percent = 0;

        int bw = 236;
        int bh = 24;
        int bx = 128 - (bw / 2);
        int by = 0;

        drawSliderDots = true;
        activeSlotIdx = (timerAtivo ? 1 : 0) + activeMediaIdx;
        hudBx = bx;

        // 1. Fundo da cápsula com cantos perfeitamente arredondados (quase colada ao relógio)
        draw_canvas_rect(layer, bx, by, bw, bh, lv_color_hex(0x0C0F17), 12);
        draw_canvas_rect_empty(layer, bx, by, bw, bh, lv_color_hex(0x272F40), 1, 12);

        // 2. Cor dinâmica pelo tipo de dispositivo (TV = #FF0000, Google Home = #a207f5)
        bool isTv = (device.find("TV") != std::string::npos || device.find("tv") != std::string::npos);
        lv_color_t mediaColor;
        if (isTv) {
            mediaColor = lv_color_hex(0xFF0000); // Vermelho puro (#FF0000)
        } else {
            mediaColor = lv_color_hex(0xA207F5); // Roxo/Violeta vibrante (#a207f5)
        }
        activeDotColor = mediaColor;

        // 3. Anel Orbital Circular à esquerda
        int arcX = bx + 14;
        int arcY = by + 12;
        int arcR = 7;

        // Trilho de fundo (360° cinza escuro)
        draw_canvas_arc(layer, arcX, arcY, arcR, 0, 360, lv_color_hex(0x222634), 2);

        // Arco ativo de progresso
        if (percent >= 100) {
            draw_canvas_arc(layer, arcX, arcY, arcR, 0, 360, mediaColor, 2);
        } else if (percent > 0) {
            int sweep = (360 * percent) / 100;
            if (sweep < 6) sweep = 6;
            if (sweep >= 360) {
                draw_canvas_arc(layer, arcX, arcY, arcR, 0, 360, mediaColor, 2);
            } else {
                int endAngle = (270 + sweep) % 360;
                draw_canvas_arc(layer, arcX, arcY, arcR, 270, endAngle, mediaColor, 2);
            }
        } else {
            // Se duração desconhecida, anel pulsa em rotação contínua
            int pulseSweep = (int)((ms / 12) % 360);
            draw_canvas_arc(layer, arcX, arcY, arcR, pulseSweep, (pulseSweep + 60) % 360, mediaColor, 2);
        }

        // Ícone centralizado no miolo do anel: Play do YouTube (TV) ou Nota Musical (Google Home)
        if (isTv) {
            // Triângulo Play do YouTube (▶)
            draw_canvas_line(layer, arcX - 2, arcY - 3, arcX - 2, arcY + 3, mediaColor, 2);
            draw_canvas_line(layer, arcX - 2, arcY - 3, arcX + 3, arcY, mediaColor, 2);
            draw_canvas_line(layer, arcX - 2, arcY + 3, arcX + 3, arcY, mediaColor, 2);
            draw_canvas_disc(layer, arcX, arcY, 1, mediaColor);
        } else {
            // Nota musical ♪ estilizada
            draw_canvas_disc(layer, arcX - 2, arcY + 2, 2, mediaColor);
            draw_canvas_line(layer, arcX, arcY + 2, arcX, arcY - 3, mediaColor, 1);
            draw_canvas_line(layer, arcX, arcY - 3, arcX + 3, arcY - 2, mediaColor, 1);
        }

        // 4. Texto em Marquee estático em linha única na largura expandida
        static char s_cached_raw[128] = {0};
        static char s_cached_clean[128] = {0};
        static char s_marquee_buf[16] = {0};
        static int s_prev_offset = -1;

        // Monta texto formatado em buffer local de stack (sem heap malloc)
        char raw_buf[128];
        if (title.empty()) {
            snprintf(raw_buf, sizeof(raw_buf), "Musica no %s", device.empty() ? "Dispositivo" : device.c_str());
        } else if (!artist.empty() && artist != "Default Media Receiver") {
            snprintf(raw_buf, sizeof(raw_buf), "%s - %s", title.c_str(), artist.c_str());
        } else {
            snprintf(raw_buf, sizeof(raw_buf), "%s", title.c_str());
        }

        // Se o título ou dispositivo mudou, sanitiza para ASCII limpo uma única vez
        if (strcmp(raw_buf, s_cached_raw) != 0) {
            strncpy(s_cached_raw, raw_buf, sizeof(s_cached_raw) - 1);
            s_cached_raw[sizeof(s_cached_raw) - 1] = '\0';
            SanitizeToAscii(s_cached_raw, s_cached_clean, sizeof(s_cached_clean));
            s_prev_offset = -1;
        }

        const int kMaxVisibleChars = 9;
        size_t clean_len = strlen(s_cached_clean);
        bool has_space = (strchr(s_cached_clean, ' ') != nullptr);
        bool is_short_static = (clean_len <= 7) || (!has_space && clean_len <= 8);

        if (clean_len == 0) {
            s_marquee_buf[0] = '\0';
        } else if (is_short_static) {
            strncpy(s_marquee_buf, s_cached_clean, kMaxVisibleChars);
            s_marquee_buf[kMaxVisibleChars] = '\0';
        } else {
            // Rolagem suave a cada 240ms através do buffer estático
            int pool_len = (int)clean_len + 6; // texto + "  *  "
            int offset = (int)((ms / 240) % pool_len);
            if (offset != s_prev_offset) {
                s_prev_offset = offset;
                const char* sep = "  *  ";
                for (int i = 0; i < kMaxVisibleChars; i++) {
                    int p = (offset + i) % pool_len;
                    if (p < (int)clean_len) {
                        s_marquee_buf[i] = s_cached_clean[p];
                    } else {
                        s_marquee_buf[i] = sep[p - clean_len];
                    }
                }
                s_marquee_buf[kMaxVisibleChars] = '\0';
            }
        }

        // Texto perfeitamente centralizado no espaço útil entre o anel e as notinhas flutuantes
        int textX = bx + 28;
        int textY = by + 3;
        int textW = bw - 58;
        draw_canvas_text(layer, textX, textY, textW, 18, s_marquee_buf, lv_color_hex(0xF0F6FC), &BUILTIN_TEXT_FONT, LV_TEXT_ALIGN_CENTER);

        // 5. Notinhas musicais flutuantes no ritmo da batida na lateral direita da cápsula
        // Notinha 1 (Mais à direita, nota principal)
        int fn1X = bx + bw - 11 + (int)(sinf(ms * 0.005f) * 2.0f);
        int fn1Y = by + 11 - (int)fmodf(ms * 0.010f, 8.0f);
        draw_canvas_disc(layer, fn1X - 2, fn1Y + 2, 2, mediaColor);
        draw_canvas_line(layer, fn1X, fn1Y + 2, fn1X, fn1Y - 4, mediaColor, 1);
        draw_canvas_line(layer, fn1X, fn1Y - 4, fn1X + 3, fn1Y - 2, mediaColor, 1);

        // Notinha 2 (Meio, nota intermediária com ritmo defasado)
        int fn2X = bx + bw - 19 + (int)(cosf((ms + 300) * 0.006f) * 1.8f);
        int fn2Y = by + 12 - (int)fmodf((ms + 350) * 0.009f, 7.0f);
        draw_canvas_disc(layer, fn2X - 2, fn2Y + 2, 2, mediaColor);
        draw_canvas_line(layer, fn2X, fn2Y + 2, fn2X, fn2Y - 3, mediaColor, 1);
        draw_canvas_line(layer, fn2X, fn2Y - 3, fn2X + 3, fn2Y - 1, mediaColor, 1);

        // Notinha 3 (Mais à esquerda do trio, brotando com charme)
        int fn3X = bx + bw - 26 + (int)(sinf((ms + 600) * 0.007f) * 1.5f);
        int fn3Y = by + 13 - (int)fmodf((ms + 700) * 0.008f, 6.0f);
        draw_canvas_disc(layer, fn3X - 1, fn3Y + 2, 1, mediaColor);
        draw_canvas_line(layer, fn3X, fn3Y + 2, fn3X, fn3Y - 2, mediaColor, 1);
        draw_canvas_line(layer, fn3X, fn3Y - 2, fn3X + 2, fn3Y - 1, mediaColor, 1);
    } else if (mostrarPetStatus && numIcons > 0) {
        int iconW = 12;
        int iconGap = 4;
        int totalW = (numIcons * iconW) + ((numIcons - 1) * iconGap);
        int startX = 64 - (totalW / 2);
        int drawY = 0;
        
        // Fundo escuro semitranslúcido com borda estilizada
        draw_canvas_rect(layer, (startX - 4)*2, (drawY)*2, (totalW + 8)*2, 14*2, lv_color_hex(0x161622), 3*2);
        draw_canvas_rect_empty(layer, (startX - 4)*2, (drawY)*2, (totalW + 8)*2, 14*2, lv_color_hex(0x3E4466), 1*2, 3*2);
        draw_canvas_line(layer, 64*2, (drawY + 14)*2, 62*2, (drawY + 16)*2, lv_color_hex(0x3E4466), 2);
        
        int currentX = startX;
        if (precisaComida) {
            // Ossinho Dourado / Laranja
            lv_color_t boneColor = lv_color_hex(0xFFA500);
            draw_canvas_rect(layer, (currentX + 3)*2, (drawY + 5)*2, 6*2, 2*2, boneColor, 0);
            draw_canvas_disc(layer, (currentX + 2)*2, (drawY + 4)*2, 1*2 + 1, boneColor);
            draw_canvas_disc(layer, (currentX + 2)*2, (drawY + 7)*2, 1*2 + 1, boneColor);
            draw_canvas_disc(layer, (currentX + 9)*2, (drawY + 4)*2, 1*2 + 1, boneColor);
            draw_canvas_disc(layer, (currentX + 9)*2, (drawY + 7)*2, 1*2 + 1, boneColor);
            currentX += (iconW + iconGap);
        }
        if (precisaBrincar) {
            // Controle de videogame verde néon estilizado
            lv_color_t ctrlColor = lv_color_hex(0x00FF66);
            int gx = currentX * 2;
            int gy = (drawY + 3) * 2;
            // Carcaça externa do controle (24x14 pixels com cantos arredondados)
            draw_canvas_rect_empty(layer, gx, gy, 12 * 2, 7 * 2, ctrlColor, 1 * 2, 2 * 2);
            // Direcional D-Pad (+) fininho com linhas de 2 pixels no lado esquerdo
            draw_canvas_rect(layer, gx + 4, gy + 6, 6, 2, ctrlColor, 0); // Barra horizontal
            draw_canvas_rect(layer, gx + 6, gy + 4, 2, 6, ctrlColor, 0); // Barra vertical
            // 4 Botões redondos no lado direito (bolinhas de 2x2 pixels :: que não se tocam)
            draw_canvas_rect(layer, gx + 14, gy + 4, 2, 2, ctrlColor, 0); // Superior esquerdo
            draw_canvas_rect(layer, gx + 18, gy + 4, 2, 2, ctrlColor, 0); // Superior direito
            draw_canvas_rect(layer, gx + 14, gy + 8, 2, 2, ctrlColor, 0); // Inferior esquerdo
            draw_canvas_rect(layer, gx + 18, gy + 8, 2, 2, ctrlColor, 0); // Inferior direito
            currentX += (iconW + iconGap);
        }
        if (precisaSaude) {
            // Frasco de remédio vermelho com cruz branca
            lv_color_t medColor = lv_color_hex(0xFF3333);
            lv_color_t whiteColor = lv_color_hex(0xFFFFFF);
            draw_canvas_rect(layer, (currentX + 4)*2, (drawY + 2)*2, 4*2, 2*2, whiteColor, 0);
            draw_canvas_rect_empty(layer, (currentX + 2)*2, (drawY + 4)*2, 8*2, 7*2, medColor, 1*2, 1*2);
            draw_canvas_line(layer, (currentX + 5)*2, (drawY + 6)*2, (currentX + 5)*2, (drawY + 9)*2, whiteColor, 2);
            draw_canvas_line(layer, (currentX + 4)*2, (drawY + 7)*2, (currentX + 6)*2, (drawY + 7)*2, whiteColor, 2);
            currentX += (iconW + iconGap);
        }

        drawSliderDots = true;
        activeSlotIdx = totalCarouselSlots - 1;
        hudBx = (startX - 4) * 2;
        activeDotColor = lv_color_hex(0xF472B6);
    }

    // =========================================================================
    // 7.1 BOLINHAS INDICADORAS DE SLIDER EMBAIXO DA PÍLULA (ESTILO DYNAMIC ISLAND)
    // =========================================================================
    if (drawSliderDots && totalCarouselSlots > 1) {
        int dotStartX = hudBx + 8;
        int dotY = 26; // 2 pixels abaixo da pílula (que termina em y = 24)
        for (int i = 0; i < totalCarouselSlots; i++) {
            if (i == activeSlotIdx) {
                // Pílula ativa alongada em neon com cantos arredondados (10x3 px)
                draw_canvas_rect(layer, dotStartX, dotY, 10, 3, activeDotColor, 1);
                dotStartX += 10 + 3;
            } else {
                // Bolinha inativa discreta (3x3 px)
                draw_canvas_disc(layer, dotStartX + 1, dotY + 1, 1, lv_color_hex(0x475569));
                dotStartX += 4 + 3;
            }
        }
    }
    
    // Geração de partículas dinâmicas
    if (emotion == "crying") {
        if ((rand() % 100) < 6) CriarParticula(eyeLx - 8 + current_look_x, eyeLy + 6 + current_look_y, -0.15f, 0.4f + (rand()%4)/10.0f, 'L', 35);
        if ((rand() % 100) < 6) CriarParticula(eyeRx + 8 + current_look_x, eyeRy + 6 + current_look_y,  0.15f, 0.4f + (rand()%4)/10.0f, 'L', 35);
    }
    if (emotion == "sleeping") {
        if ((rand() % 100) < 3) CriarParticula(64 + mouthShiftX, 48 + mouthShiftY, 0.2f + (rand()%4)/10.0f, -0.3f - (rand()%4)/10.0f, 'Z', 50);
    }
    if (comendo) {
        if ((rand() % 100) < 18) CriarParticula(64 + mouthShiftX, 48 + mouthShiftY, ((rand()%10)-5)/10.0f, 0.5f + (rand()%5)/10.0f, '*', 30);
    }
    if (curando) {
        if ((rand() % 100) < 15) CriarParticula(64 + mouthShiftX + (rand()%30-15), 48 + mouthShiftY + (rand()%20-10), 0, -0.6f - (rand()%4)/10.0f, '+', 40);
    }
    if (brincando && (rand() % 100) < 15) {
        CriarParticula(64 + mouthShiftX + (rand()%30 - 15), 32, (rand()%10 - 5)/10.0f, -0.5f, 'H', 35);
    }
    if ((idleTipo == 7 || emotion == "loving") && (rand() % 100) < 15) {
        CriarParticula(64 + mouthShiftX + (rand()%20 - 10), 46 + mouthShiftY, (rand()%10 - 5)/10.0f, -0.5f, 'H', 35);
    }
    
    float limiarCalor = engine.GetLimiarTempAlto();
    if ((temp > limiarCalor && temp > 0.0f) || emotion == "embarrassed") {
        if ((rand() % 100) < 6) {
            CriarParticula(8 + (rand() % 17), 0, 0.0f, 0.8f + (rand() % 5) / 10.0f, 'S', 70);
        }
        if ((rand() % 100) < 6) {
            CriarParticula(103 + (rand() % 17), 0, 0.0f, 0.8f + (rand() % 5) / 10.0f, 'S', 70);
        }
    }
    
    // Orelhas de escuta IA (ondas dinâmicas em ciano néon)
    bool estaOuvindo = (Application::GetInstance().GetDeviceState() == kDeviceStateListening);
    if (estaOuvindo) {
        int wavePhase = (ms / 150) % 3;
        lv_color_t earColor = lv_color_hex(0x00FFFF);
        
        draw_canvas_arc(layer, 18*2, 37*2, 6*2, 90, 270, earColor, 2*2);
        draw_canvas_arc(layer, 18*2, 37*2, 3*2, 90, 270, earColor, 2);
        for (int w = 0; w <= wavePhase; w++) {
            draw_canvas_arc(layer, 18*2, 37*2, (9 + w * 4)*2, 110, 250, earColor, 2);
        }

        draw_canvas_arc(layer, 110*2, 37*2, 6*2, 270, 90, earColor, 2*2);
        draw_canvas_arc(layer, 110*2, 37*2, 3*2, 270, 90, earColor, 2);
        for (int w = 0; w <= wavePhase; w++) {
            draw_canvas_arc(layer, 110*2, 37*2, (9 + w * 4)*2, 290, 70, earColor, 2);
        }
    }

    // Acessórios visuais do Guarda-Roupa e Overlays de Jogos (Prisão, Dança, Escadas)
    uint8_t accId = engine.GetAccessory();
    if (accId > 0 && emotion != "sleeping") {
        DrawAccessoryOverlay(layer, accId, eyeLx + current_look_x * 0.25f, eyeLy + current_look_y * 0.25f,
                             eyeRx + current_look_x * 0.25f, eyeRy + current_look_y * 0.25f);
    }
    if (!activeCompanionOverlay.empty()) {
        DrawFaceOverlay(layer, activeCompanionOverlay);
    }

    // Partículas especiais de Gastronomia e Jogos
    uint8_t foodReact = engine.GetFoodReactionType();
    if (foodReact == 1 && (rand() % 100) < 22) {
        CriarParticula(64 + mouthShiftX + (rand() % 36 - 18), 52, ((rand() % 10) - 5) / 10.0f, -0.7f, 'F', 35);
    } else if (foodReact == 2 && (rand() % 100) < 20) {
        CriarParticula(20 + (rand() % 88), 10, 0.0f, 0.6f, 'I', 45);
    }
    if (activeCompanionOverlay == "money_gain" && (rand() % 100) < 25) {
        CriarParticula(16 + (rand() % 96), 58, ((rand() % 8) - 4) / 10.0f, -0.7f, '$', 45);
    } else if ((activeCompanionOverlay == "victory" || activeCompanionOverlay == "dance") && (rand() % 100) < 28) {
        CriarParticula(10 + (rand() % 108), 8, ((rand() % 10) - 5) / 10.0f, 0.7f, 'C', 50);
    }

    AtualizarParticulas();
    DesenharParticulas(xOffset, layer);

#if LVGL_VERSION_MAJOR >= 9
    lv_canvas_finish_layer(face_canvas_, &layer_obj);
#endif
}

void LcdDisplay::UpdateEyeAnimations() {
    DrawOledFace(0);
}


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

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
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
        lv_obj_align(face_canvas_, LV_ALIGN_CENTER, 0, -22);
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
        lv_obj_align(face_canvas_, LV_ALIGN_CENTER, 0, -22);
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
        // Se ainda não nasceu, esconde o emoji_box_ e o rosto dinâmico
        if (emoji_box_ != nullptr) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        if (face_canvas_ != nullptr) lv_obj_add_flag(face_canvas_, LV_OBJ_FLAG_HIDDEN);
        
        // Exibe o ovo e a barra de progresso
        if (egg_obj_ != nullptr) {
            lv_obj_remove_flag(egg_obj_, LV_OBJ_FLAG_HIDDEN);
            if (estado == ESTADO_CHOCANDO) {
                // Tremer o ovo!
                lv_obj_align(egg_obj_, LV_ALIGN_CENTER, ((rand() % 5) - 2), -10 + ((rand() % 5) - 2));
            } else {
                lv_obj_align(egg_obj_, LV_ALIGN_CENTER, 0, -10);
            }
        }
        if (egg_bar_ != nullptr) {
            lv_obj_remove_flag(egg_bar_, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(egg_bar_, engine.GetSegundosChocados(), LV_ANIM_OFF);
        }
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

void LcdDisplay::DrawStar(float x, float y, float radius, lv_color_t color, lv_layer_t* layer) {
    if (!face_canvas_ || !layer) return;
    int px = (int)(x * 2.0f);
    int py = (int)(y * 2.0f);
    int r = (int)(radius * 2.0f);
    if (r < 2) r = 2;
    
    draw_canvas_line(layer, px, py - r, px, py + r, color, 3);
    draw_canvas_line(layer, px - r, py, px + r, py, color, 3);
    int diag = r * 6 / 10;
    draw_canvas_line(layer, px - diag, py - diag, px + diag, py + diag, color, 2);
    draw_canvas_line(layer, px - diag, py + diag, px + diag, py - diag, color, 2);
    draw_canvas_disc(layer, px, py, 2, lv_color_hex(0xFFFFFF));
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
    int sX = (cx + vibX) + (int)(22.0f * sin(sAngle));
    int sY = (cy + vibY) - (int)(22.0f * cos(sAngle));
    draw_canvas_line(layer, cx + vibX, cy + vibY, sX, sY, lv_color_hex(0xE53935), 2);
    
    // Centro dos ponteiros
    draw_canvas_disc(layer, cx + vibX, cy + vibY, 4, lv_color_hex(0xD32F2F));
    draw_canvas_disc(layer, cx + vibX - 1, cy + vibY - 1, 1, lv_color_hex(0xFFFFFF));
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
    
    bool precisaComida = (fome <= 30);
    bool precisaBrincar = (diversao <= 30);
    bool precisaSaude = (saude <= 30 || estaDoente);
    int numIcons = 0;
    if (precisaComida) numIcons++;
    if (precisaBrincar) numIcons++;
    if (precisaSaude) numIcons++;

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
    float reactionProgress = 1.0f;
    uint64_t tStart = engine.GetTempoInicioReacaoOciosa();
    uint64_t tFim = engine.GetTempoFimReacaoOciosa();
    if (idleTipo != 0 && tFim > tStart && ms >= tStart && ms <= tFim) {
        float elapsed = (float)(ms - tStart);
        float remain = (float)(tFim - ms);
        if (elapsed < 350.0f) {
            reactionProgress = elapsed / 350.0f;
        } else if (remain < 350.0f) {
            reactionProgress = remain / 350.0f;
        }
    }

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
    
    auto pers = engine.GetPersonalidade();
    if (pers == PERSONALIDADE_SARCASTICA) {
        pal.primary = lv_color_hex(0xB388FF);   // Roxo ametista néon
        pal.aura = lv_color_hex(0x651FFF);      // Violeta profundo
        pal.pupil = lv_color_hex(0x311B92);     // Pupila roxa escura
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
    } else if (idleTipo == 12 || idleTipo == 7 || idleTipo == 14 || emotion == "loving") {
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
    float breathPeriod = (emotion == "sleeping") ? 4200.0f : 3000.0f;
    float breathAngle = (float)(ms % (int)breathPeriod) / breathPeriod * (2.0f * 3.14159265f);
    float breathFactor = sin(breathAngle); // oscila suavemente entre -1.0 e +1.0
    
    float breathY = 0.0f;
    float breathScaleH = 0.0f;
    float breathScaleW = 0.0f;

    if (emotion == "sleeping") {
        // No sono: respiração profunda com flutuação vertical serena
        breathY = -breathFactor * 2.2f;
    } else if (!comendo && !isDizzy && emotion != "angry") {
        // Respiração normal viva:
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
        DrawEyeDizzy(eyeLx + xOffset + tremorX, eyeLy + tremorY, 9.0f, rotAngle, pal.primary, layer);
        DrawEyeDizzy(eyeRx + xOffset + tremorX, eyeRy + tremorY, 9.0f, -rotAngle, pal.primary, layer);
        
        // 3 Estrelas orbitando a cabeça em elipse 3D (x=64, y=10)
        float starAngle = ms * 0.006f;
        for (int s = 0; s < 3; s++) {
            float a = starAngle + s * (2.0f * 3.14159f / 3.0f);
            float sx = 64.0f + cos(a) * 20.0f + xOffset;
            float sy = 10.0f + sin(a) * 5.0f;
            float sRadius = 2.5f + sin(a) * 0.8f;
            lv_color_t sColor = (sin(a) > 0) ? lv_color_hex(0xFFD700) : lv_color_hex(0xFFA000);
            DrawStar(sx, sy, sRadius, sColor, layer);
        }
    } else if (comendo) {
        // COMENDO: Mastigação elástica (Squash & Stretch)
        bool bite = ((ms / 180) % 2 == 0);
        float chewW = bite ? (eyeLw * 1.25f) : (eyeLw * 0.95f);
        float chewH = bite ? (eyeLh * 0.65f) : (eyeLh * 1.05f);
        float chewY = bite ? 2.5f : -1.0f;
        
        DrawEyeHappy(eyeLx + xOffset + tremorX + current_look_x, eyeLy + tremorY + chewY + current_look_y, 
                     chewW, chewH, eyeRadius, 1.0f, layer, pal.primary, pal.aura);
        DrawEyeHappy(eyeRx + xOffset + tremorX + current_look_x, eyeRy + tremorY + chewY + current_look_y, 
                     chewW, chewH, eyeRadius, 1.0f, layer, pal.primary, pal.aura);
    } else if (brincando || idleTipo == 11) {
        // FELIZ / BRINCANDO: Arcos dourados cheios de alegria + pulinho + respiracao
        DrawEyeHappy(eyeLx + xOffset + tremorX + current_look_x + swayX, 
                     eyeLy + tremorY + current_look_y + swayY + breathY, 
                     eyeLw, eyeLh + breathScaleH, eyeRadius, reactionProgress, layer, pal.primary, pal.aura);
        DrawEyeHappy(eyeRx + xOffset + tremorX + current_look_x + swayX, 
                     eyeRy + tremorY + current_look_y + swayY + breathY, 
                     eyeRw, eyeRh + breathScaleH, eyeRadius, reactionProgress, layer, pal.primary, pal.aura);
    } else if (idleTipo == 2) {
        // SARCÁSTICO: Revirar de olhos em órbita suave
        float rotAngulo = ms * 0.008f;
        float pupilX = cos(rotAngulo) * 4.5f;
        float pupilY = sin(rotAngulo) * 4.5f;
        
        DrawEye(eyeLx + xOffset + tremorX + current_look_x, eyeLy + tremorY + current_look_y, 
                eyeLw, eyeLh, eyeRadius, layer, pal.primary, pal.aura, false);
        DrawEye(eyeRx + xOffset + tremorX + current_look_x, eyeRy + tremorY + current_look_y, 
                eyeRw, eyeRh, eyeRadius, layer, pal.primary, pal.aura, false);
        
        // Sobrancelhas desdenhosas
        draw_canvas_line(layer, (eyeLx - 8 + xOffset + current_look_x)*2, (eyeLy - 11 + current_look_y)*2, 
                         (eyeLx + 6 + xOffset + current_look_x)*2, (eyeLy - 7 + current_look_y)*2, pal.primary, 3);
        draw_canvas_line(layer, (eyeRx - 6 + xOffset + current_look_x)*2, (eyeRy - 7 + current_look_y)*2, 
                         (eyeRx + 8 + xOffset + current_look_x)*2, (eyeRy - 11 + current_look_y)*2, pal.primary, 3);

        int pLx = (int)((eyeLx + xOffset + tremorX + current_look_x + pupilX) * 2);
        int pLy = (int)((eyeLy + tremorY + current_look_y + pupilY) * 2);
        int pRx = (int)((eyeRx + xOffset + tremorX + current_look_x + pupilX) * 2);
        int pRy = (int)((eyeRy + tremorY + current_look_y + pupilY) * 2);
        draw_canvas_disc(layer, pLx, pLy, 3*2, lv_color_hex(0x000000));
        draw_canvas_disc(layer, pRx, pRy, 3*2, lv_color_hex(0x000000));
        draw_canvas_disc(layer, pLx + 2, pLy - 2, 2, lv_color_hex(0xFFFFFF));
        draw_canvas_disc(layer, pRx + 2, pRy - 2, 2, lv_color_hex(0xFFFFFF));
    } else if (idleTipo == 8) {
        // Deboche rabugento
        DrawEyeHappy(eyeLx + xOffset + current_look_x, eyeLy + current_look_y, eyeLw, eyeLh, eyeRadius, reactionProgress, layer, pal.primary, pal.aura);
        DrawEyeHappy(eyeRx + xOffset + current_look_x, eyeRy + current_look_y, eyeRw, eyeRh, eyeRadius, reactionProgress, layer, pal.primary, pal.aura);
    } else if (idleTipo == 9) {
        // Desdém irritado: olhos em traço - -
        draw_canvas_line(layer, (eyeLx - 8 + xOffset + current_look_x)*2, (eyeLy + current_look_y)*2, 
                         (eyeLx + 8 + xOffset + current_look_x)*2, (eyeLy + current_look_y)*2, pal.primary, 4);
        draw_canvas_line(layer, (eyeRx - 8 + xOffset + current_look_x)*2, (eyeRy + current_look_y)*2, 
                         (eyeRx + 8 + xOffset + current_look_x)*2, (eyeRy + current_look_y)*2, pal.primary, 4);
    } else if (idleTipo == 10) {
        // Piscadela de um olho (Wink maroto)
        draw_canvas_line(layer, (eyeLx - 8 + xOffset + current_look_x)*2, (eyeLy + current_look_y)*2, 
                         (eyeLx + 8 + xOffset + current_look_x)*2, (eyeLy + current_look_y)*2, pal.primary, 4);
        DrawEye(eyeRx + xOffset + current_look_x, eyeRy + current_look_y, eyeRw, eyeRh, eyeRadius, layer, pal.primary, pal.aura, true);
    } else if (idleTipo == 12 || idleTipo == 7 || idleTipo == 14 || emotion == "loving") {
        // APAIXONADO / CARINHO: Corações com batimento cardíaco Lub-Dub orgânico
        float pulseTime = (float)(ms % 1000) / 1000.0f;
        bool lubDub = (pulseTime < 0.18f || (pulseTime > 0.28f && pulseTime < 0.44f));
        DrawLargeHeart((int)(eyeLx + xOffset + tremorX + current_look_x), (int)(eyeLy + tremorY + current_look_y), lubDub, layer, pal.primary);
        DrawLargeHeart((int)(eyeRx + xOffset + tremorX + current_look_x), (int)(eyeRy + tremorY + current_look_y), lubDub, layer, pal.primary);
    } else if (emotion == "cold") {
        // FRIO: Olhos encolhidos em azul gélido com tremor rápido
        DrawEye(eyeLx + xOffset + tremorX + current_look_x, eyeLy + tremorY + current_look_y, 14, 20, 4, layer, pal.primary, pal.aura, true);
        DrawEye(eyeRx + xOffset + tremorX + current_look_x, eyeRy + tremorY + current_look_y, 14, 20, 4, layer, pal.primary, pal.aura, true);
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
        DrawEye(eyeLx + xOffset + tremorX + current_look_x, eyeLy + tremorY + current_look_y - 2, 16, 32, 10, layer, pal.primary, pal.aura, true);
        DrawEye(eyeRx + xOffset + tremorX + current_look_x, eyeRy + tremorY + current_look_y - 2, 16, 32, 10, layer, pal.primary, pal.aura, true);
    } else if (emotion == "angry") {
        // ZANGADO: Olhos em chamas carmesim/laranja com sobrancelhas grossas inclinadas
        float sary = eyeLy + tremorY + current_look_y + 3.0f;
        float sarry = eyeRy + tremorY + current_look_y + 3.0f;

        DrawEye(eyeLx + xOffset + tremorX + current_look_x, sary, eyeLw, 16.0f, 3, layer, pal.primary, pal.aura, false);
        DrawEye(eyeRx + xOffset + tremorX + current_look_x, sarry, eyeRw, 16.0f, 3, layer, pal.primary, pal.aura, false);

        int lx = (int)((eyeLx + xOffset + tremorX + current_look_x) * 2);
        int rx = (int)((eyeRx + xOffset + tremorX + current_look_x) * 2);
        int topY = (int)((sary - 8.0f) * 2);
        int lw = (int)((eyeLw + 3) * 2);
        int rw = (int)((eyeRw + 3) * 2);

        // Pálpebras inclinadas no formato \ /
        draw_canvas_line(layer, lx - lw/2, topY - 3, lx + lw/2, topY + 3, lv_color_hex(0xFF5722), 5);
        draw_canvas_line(layer, rx - rw/2, topY + 3, rx + rw/2, topY - 3, lv_color_hex(0xFF5722), 5);

        // Sobrancelhas zangadas grossas vermelhas (\ /)
        draw_canvas_line(layer, (eyeLx - 10 + xOffset + tremorX + current_look_x)*2, (eyeLy - 14 + tremorY + current_look_y)*2, 
                         (eyeLx + 8 + xOffset + tremorX + current_look_x)*2, (eyeLy - 8 + tremorY + current_look_y)*2, lv_color_hex(0xFF0000), 5);
        draw_canvas_line(layer, (eyeRx - 8 + xOffset + tremorX + current_look_x)*2, (eyeRy - 8 + tremorY + current_look_y)*2, 
                         (eyeRx + 10 + xOffset + tremorX + current_look_x)*2, (eyeRy - 14 + tremorY + current_look_y)*2, lv_color_hex(0xFF0000), 5);
    } else if (emotion == "sad" || emotion == "crying") {
        // TRISTE / CHORANDO: Olhos caídos em azul oceânico com sobrancelhas tristes
        float sary = eyeLy + tremorY + current_look_y + breathY + 3.0f;
        float sarry = eyeRy + tremorY + current_look_y + breathY + 3.0f;

        DrawEye(eyeLx + xOffset + tremorX + current_look_x, sary, eyeLw, 15.0f, 3, layer, pal.primary, pal.aura, true);
        DrawEye(eyeRx + xOffset + tremorX + current_look_x, sarry, eyeRw, 15.0f, 3, layer, pal.primary, pal.aura, true);

        int lx = (int)((eyeLx + xOffset + tremorX + current_look_x) * 2);
        int rx = (int)((eyeRx + xOffset + tremorX + current_look_x) * 2);
        int topY = (int)((sary - 7.5f) * 2);
        int lw = (int)((eyeLw + 3) * 2);
        int rw = (int)((eyeRw + 3) * 2);

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
        
        bool isSarcastic = (pers == PERSONALIDADE_SARCASTICA || engine.GetHumor() == 1);
        if (isSarcastic) {
            float sarh = (13.0f + breathScaleH * 0.6f) * (1.0f - blinker.blinkFactor);
            float sary = curEyeY + 4.0f;
            DrawEye(eyeLx + xOffset + tremorX + current_look_x, sary, curEyeW, sarh, 3, layer, pal.primary, pal.aura, true, current_look_x, current_look_y);
            DrawEye(eyeRx + xOffset + tremorX + current_look_x, sary, curEyeW, sarh, 3, layer, pal.primary, pal.aura, true, current_look_x, current_look_y);

            int lx = (int)((eyeLx + xOffset + tremorX + current_look_x) * 2);
            int rx = (int)((eyeRx + xOffset + tremorX + current_look_x) * 2);
            int topY = (int)((sary - sarh / 2.0f) * 2);
            int lw = (int)((curEyeW + 3) * 2);
            int rw = (int)((curEyeW + 3) * 2);

            draw_canvas_line(layer, lx - lw/2, topY, lx + lw/2, topY, pal.aura, 4);
            draw_canvas_line(layer, rx - rw/2, topY, rx + rw/2, topY, pal.aura, 4);

            // Sobrancelhas irônicas inclinadas (/ \)
            draw_canvas_line(layer, (eyeLx - 8 + xOffset + tremorX + current_look_x)*2, (eyeLy - 12 + tremorY + current_look_y)*2, 
                             (eyeLx + 6 + xOffset + tremorX + current_look_x)*2, (eyeLy - 8 + tremorY + current_look_y)*2, pal.primary, 3);
            draw_canvas_line(layer, (eyeRx - 6 + xOffset + tremorX + current_look_x)*2, (eyeRy - 8 + tremorY + current_look_y)*2, 
                             (eyeRx + 8 + xOffset + tremorX + current_look_x)*2, (eyeRy - 12 + tremorY + current_look_y)*2, pal.primary, 3);
        } else {
            // Olhos normais ou sensíveis cheios de vida e brilho
            DrawEye(eyeLx + xOffset + tremorX + current_look_x, curEyeY, curEyeW, curEyeH, eyeRadius, layer, pal.primary, pal.aura, (curEyeH > 8.0f), current_look_x, current_look_y);
            DrawEye(eyeRx + xOffset + tremorX + current_look_x, curEyeY, curEyeW, curEyeH, eyeRadius, layer, pal.primary, pal.aura, (curEyeH > 8.0f), current_look_x, current_look_y);
        }
    }
    
    // =========================================================================
    // 5. BOCHECHAS CORADAS (BLUSH) FOFAS SOB OS OLHOS
    // =========================================================================
    if (pal.hasCheeks && !isDizzy && emotion != "angry" && emotion != "sleeping") {
        float cheekY = eyeLy + 13.0f + tremorY + current_look_y + breathY * 0.75f;
        DrawCheekBlush(eyeLx - 3.0f + xOffset + current_look_x, cheekY, 5.0f, 2.5f, pal.cheek, layer);
        DrawCheekBlush(eyeRx + 3.0f + xOffset + current_look_x, cheekY, 5.0f, 2.5f, pal.cheek, layer);
    }
    
    // =========================================================================
    // 6. RENDERIZAÇÃO DA BOCA ARTICULADA E SINCRONIZADA COM A RESPIRAÇÃO
    // =========================================================================
    int mouthShiftX = (int)(current_look_x * 0.85f);
    int mouthShiftY = (int)((current_look_y + swayY + breathY) * 0.80f) + 4;
    lv_color_t mc = pal.mouth;
    
    if (isDizzy) {
        // Boca em zigue-zague ondulado de tontura
        int my = 49 + mouthShiftY;
        draw_canvas_line(layer, (58 + mouthShiftX)*2, (my)*2, (62 + mouthShiftX)*2, (my + 4)*2, mc, 4);
        draw_canvas_line(layer, (62 + mouthShiftX)*2, (my + 4)*2, (66 + mouthShiftX)*2, (my - 3)*2, mc, 4);
        draw_canvas_line(layer, (66 + mouthShiftX)*2, (my - 3)*2, (70 + mouthShiftX)*2, (my)*2, mc, 4);
    } else if (comendo) {
        // Boca abrindo e fechando com mastigação rítmica
        if ((ms / 180) % 2 == 0) {
            draw_canvas_line(layer, (58 + mouthShiftX)*2, (49 + mouthShiftY)*2, (70 + mouthShiftX)*2, (49 + mouthShiftY)*2, mc, 4);
        } else {
            draw_canvas_rect(layer, (61 + mouthShiftX)*2, (47 + mouthShiftY)*2, 6*2, 5*2, mc, 2);
        }
    } else if (emotion == "sleeping") {
        draw_canvas_rect(layer, (62 + mouthShiftX)*2, (48 + mouthShiftY)*2, 4*2, 4*2, mc, 2);
    } else if (brincando || idleTipo == 11) {
        // Sorriso grande aberto de felicidade
        draw_canvas_line(layer, (58 + mouthShiftX)*2, (48 + mouthShiftY)*2, (64 + mouthShiftX)*2, (53 + mouthShiftY)*2, mc, 4);
        draw_canvas_line(layer, (64 + mouthShiftX)*2, (53 + mouthShiftY)*2, (70 + mouthShiftX)*2, (48 + mouthShiftY)*2, mc, 4);
    } else if (idleTipo == 1) {
        // Assobio: boca em 'o' pequena
        draw_canvas_rect(layer, (62 + mouthShiftX)*2, (47 + mouthShiftY)*2, 4*2, 4*2, mc, 2);
    } else if (idleTipo == 8 || idleTipo == 9 || emotion == "sad" || emotion == "crying") {
        draw_canvas_line(layer, (58 + mouthShiftX)*2, (51 + mouthShiftY)*2, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, mc, 4);
        draw_canvas_line(layer, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, (70 + mouthShiftX)*2, (51 + mouthShiftY)*2, mc, 4);
    } else if (idleTipo == 14) {
        // Beijo: biquinho
        draw_canvas_rect(layer, (62 + mouthShiftX)*2, (48 + mouthShiftY)*2, 3*2, 3*2, mc, 2);
    } else if (emotion == "surprised") {
        draw_canvas_rect(layer, (62 + mouthShiftX)*2, (47 + mouthShiftY)*2, 4*2, 6*2, mc, 2);
    } else if (emotion == "happy") {
        // Boca fofa w de gatinho
        draw_canvas_line(layer, (60 + mouthShiftX)*2, (48 + mouthShiftY)*2, (62 + mouthShiftX)*2, (50 + mouthShiftY)*2, mc, 3);
        draw_canvas_line(layer, (62 + mouthShiftX)*2, (50 + mouthShiftY)*2, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, mc, 3);
        draw_canvas_line(layer, (64 + mouthShiftX)*2, (48 + mouthShiftY)*2, (66 + mouthShiftX)*2, (50 + mouthShiftY)*2, mc, 3);
        draw_canvas_line(layer, (66 + mouthShiftX)*2, (50 + mouthShiftY)*2, (68 + mouthShiftX)*2, (48 + mouthShiftY)*2, mc, 3);
    } else if (emotion == "angry") {
        draw_canvas_line(layer, (58 + mouthShiftX)*2, (49 + mouthShiftY)*2, (70 + mouthShiftX)*2, (49 + mouthShiftY)*2, mc, 4);
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
    // 7. BALÃO MODERNO HUD DE NECESSIDADES (FOME, BRINCAR, SAÚDE)
    // =========================================================================
    if (numIcons > 0) {
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
            // Controle de videogame verde néon
            lv_color_t ctrlColor = lv_color_hex(0x00FF66);
            draw_canvas_rect_empty(layer, (currentX)*2, (drawY + 3)*2, 12*2, 7*2, ctrlColor, 1*2, 2*2);
            draw_canvas_rect(layer, (currentX + 2)*2, (drawY + 6)*2, 3*2, 1*2, ctrlColor, 0);
            draw_canvas_rect(layer, (currentX + 3)*2, (drawY + 5)*2, 1*2, 3*2, ctrlColor, 0);
            draw_canvas_disc(layer, (currentX + 9)*2, (drawY + 5)*2, 1*2, ctrlColor);
            draw_canvas_disc(layer, (currentX + 8)*2, (drawY + 7)*2, 1*2, ctrlColor);
            draw_canvas_rect(layer, (currentX + 5)*2, (drawY + 6)*2, 2*2, 1*2, ctrlColor, 0);
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
    if (idleTipo == 1 && (rand() % 100) < 15) {
        CriarParticula(64 + mouthShiftX, 46 + mouthShiftY, (rand()%10 - 5)/10.0f, -0.4f, 'M', 40);
    }
    if ((idleTipo == 7 || idleTipo == 14 || emotion == "loving") && (rand() % 100) < 15) {
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

    AtualizarParticulas();
    DesenharParticulas(xOffset, layer);

#if LVGL_VERSION_MAJOR >= 9
    lv_canvas_finish_layer(face_canvas_, &layer_obj);
#endif
}

void LcdDisplay::UpdateEyeAnimations() {
    auto& engine = TamagotchiEngine::GetInstance();
    if (engine.IsAlarmeAtivo()) {
        DrawOledFace(0);
        return;
    }
    if (engine.GetEstadoNascimento() != ESTADO_NASCIDO) return;
    DrawOledFace(0);
}

#include "crowpanel_ui.h"

#include "esphome/core/log.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esphome/core/hal.h"
#include <cstdio>

namespace esphome {
namespace crowpanel_ui {

static const char *const TAG = "crowpanel_ui";

static constexpr int LCD_WIDTH = 480;
static constexpr int LCD_HEIGHT = 272;

static constexpr gpio_num_t PIN_DE = GPIO_NUM_40;
static constexpr gpio_num_t PIN_VSYNC = GPIO_NUM_41;
static constexpr gpio_num_t PIN_HSYNC = GPIO_NUM_39;
static constexpr gpio_num_t PIN_PCLK = GPIO_NUM_42;
static constexpr gpio_num_t PIN_LIGHT_OUT = GPIO_NUM_18;

static constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_37;
static constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_38;
static constexpr uint8_t AHT20_ADDRESS = 0x38;

static constexpr gpio_num_t PIN_TOUCH_CS = GPIO_NUM_0;
static constexpr gpio_num_t PIN_TOUCH_CLK = GPIO_NUM_12;
static constexpr gpio_num_t PIN_TOUCH_DIN = GPIO_NUM_11;
static constexpr gpio_num_t PIN_TOUCH_OUT = GPIO_NUM_13;
static constexpr gpio_num_t PIN_TOUCH_IRQ = GPIO_NUM_36;

static constexpr int LIGHT_ICON_X = 80;
static constexpr int LIGHT_ICON_Y = 120;
static constexpr int LIGHT_ICON_W = 50;
static constexpr int LIGHT_ICON_H = 50;
static constexpr int SENSOR_VALUE_Y = 198;
static constexpr int LIGHT_LABEL_Y = 198;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void CrowPanelUi::setup() {
  gpio_set_direction(PIN_LIGHT_OUT, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_LIGHT_OUT, 0);
  this->setup_touch_();
  this->setup_i2c_();

  esp_lcd_rgb_panel_config_t panel_config = {};
  panel_config.clk_src = LCD_CLK_SRC_DEFAULT;
  panel_config.timings.pclk_hz = 8000000;
  panel_config.timings.h_res = LCD_WIDTH;
  panel_config.timings.v_res = LCD_HEIGHT;
  panel_config.timings.hsync_pulse_width = 4;
  panel_config.timings.hsync_back_porch = 43;
  panel_config.timings.hsync_front_porch = 8;
  panel_config.timings.vsync_pulse_width = 4;
  panel_config.timings.vsync_back_porch = 12;
  panel_config.timings.vsync_front_porch = 8;
  panel_config.timings.flags.hsync_idle_low = 1;
  panel_config.timings.flags.vsync_idle_low = 1;
  panel_config.timings.flags.de_idle_high = 0;
  panel_config.timings.flags.pclk_active_neg = 1;
  panel_config.timings.flags.pclk_idle_high = 0;

  panel_config.data_width = 16;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  panel_config.bits_per_pixel = 16;
  panel_config.num_fbs = 1;
  panel_config.bounce_buffer_size_px = LCD_WIDTH;
#endif
  panel_config.sram_trans_align = 8;
  panel_config.psram_trans_align = 64;
  panel_config.hsync_gpio_num = PIN_HSYNC;
  panel_config.vsync_gpio_num = PIN_VSYNC;
  panel_config.de_gpio_num = PIN_DE;
  panel_config.pclk_gpio_num = PIN_PCLK;
  panel_config.disp_gpio_num = GPIO_NUM_NC;

  const int data_pins[16] = {
      8,  3,  46, 9,  1,   // B0-B4
      5,  6,  7,  15, 16, 4,   // G0-G5
      45, 48, 47, 21, 14       // R0-R4
  };
  for (int i = 0; i < 16; i++) {
    panel_config.data_gpio_nums[i] = data_pins[i];
  }

  panel_config.flags.disp_active_low = true;
  panel_config.flags.fb_in_psram = true;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  panel_config.flags.refresh_on_demand = false;
  panel_config.flags.double_fb = false;
  panel_config.flags.no_fb = false;
  panel_config.flags.bb_invalidate_cache = false;
#endif

  ESP_LOGI(TAG, "Initializing CrowPanel 4.3 V2.1 RGB LCD using Arduino_GFX timing");
  esp_lcd_panel_handle_t panel_handle = nullptr;
  esp_err_t err = esp_lcd_new_rgb_panel(&panel_config, &panel_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_new_rgb_panel failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  this->panel_handle_ = panel_handle;

  err = esp_lcd_panel_reset(panel_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_panel_reset failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = esp_lcd_panel_init(panel_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_panel_init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  void *frame_buffer = nullptr;
  err = esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 1, &frame_buffer);
  if (err != ESP_OK || frame_buffer == nullptr) {
    ESP_LOGE(TAG, "esp_lcd_rgb_panel_get_frame_buffer failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  this->frame_buffer_ = static_cast<uint16_t *>(frame_buffer);
  ESP_LOGI(TAG, "Drawing UI icons, image count=%u", static_cast<unsigned>(this->images_.size()));
  this->paint_boot_pattern_();
  this->paint_images_();
  this->read_aht20_();
  this->paint_sensor_values_();
  ESP_LOGI(TAG, "RGB LCD init OK, framebuffer=%p", this->frame_buffer_);
}

void CrowPanelUi::loop() {
  const uint32_t now = millis();
  if (now - this->last_touch_check_ >= 30) {
    this->last_touch_check_ = now;
    this->handle_touch_();
  }

  if (now - this->last_sensor_read_ >= 2000) {
    this->last_sensor_read_ = now;
    this->read_aht20_();
    this->paint_sensor_values_();
  }
}

void CrowPanelUi::set_light_state(bool state) {
  if (this->light_on_ == state)
    return;
  this->light_on_ = state;
  gpio_set_level(PIN_LIGHT_OUT, state ? 1 : 0);
  this->paint_light_button_();
  ESP_LOGI(TAG, "Light set: GPIO18=%s", state ? "HIGH" : "LOW");
}

void CrowPanelUi::dump_config() {
  ESP_LOGCONFIG(TAG, "CrowPanel ESP32-S3 4.3-inch V2.1 RGB LCD - UI icon build");
  ESP_LOGCONFIG(TAG, "  Resolution: %dx%d", LCD_WIDTH, LCD_HEIGHT);
  ESP_LOGCONFIG(TAG, "  PCLK: 8MHz, pclk_active_neg: true, bounce_buffer_size_px: %d", LCD_WIDTH);
  ESP_LOGCONFIG(TAG, "  UI images: %u", static_cast<unsigned>(this->images_.size()));
  ESP_LOGCONFIG(TAG, "  Light output: GPIO18");
  ESP_LOGCONFIG(TAG, "  AHT20/AHT21 I2C: SDA=GPIO37 SCL=GPIO38 address=0x38");
  ESP_LOGCONFIG(TAG, "  Touch pins: CS=GPIO0 CLK=GPIO12 DIN=GPIO11 OUT=GPIO13 IRQ=GPIO36");
}

void CrowPanelUi::paint_boot_pattern_() {
  if (this->frame_buffer_ == nullptr)
    return;

  for (int y = 0; y < LCD_HEIGHT; y++) {
    for (int x = 0; x < LCD_WIDTH; x++) {
      this->frame_buffer_[y * LCD_WIDTH + x] = rgb565(255, 255, 255);
    }
  }
}

void CrowPanelUi::paint_images_() {
  if (!this->images_.empty())
    this->draw_image_(this->images_[0]);
  this->paint_sensor_icons_();
  this->paint_light_button_();
}

void CrowPanelUi::paint_light_button_() {
  this->fill_rect_(LIGHT_ICON_X - 4, LIGHT_ICON_Y - 4, LIGHT_ICON_W + 8, LIGHT_ICON_H + 8, rgb565(255, 255, 255));
  if (this->images_.size() < 3)
    return;

  this->draw_image_(this->light_on_ ? this->images_[2] : this->images_[1]);
  this->paint_light_label_();
}

void CrowPanelUi::paint_light_label_() {
  const std::string text = this->light_on_ ? "ON" : "OFF";
  if (this->last_light_text_ == text)
    return;

  const int old_x = LIGHT_ICON_X + (LIGHT_ICON_W - static_cast<int>(this->last_light_text_.size()) * 18) / 2;
  const int new_x = LIGHT_ICON_X + (LIGHT_ICON_W - static_cast<int>(text.size()) * 18) / 2;
  if (!this->last_light_text_.empty())
    this->draw_text_(old_x, LIGHT_LABEL_Y - 2, this->last_light_text_, rgb565(255, 255, 255), 2);

  this->draw_text_(new_x, LIGHT_LABEL_Y - 2, text, rgb565(0, 0, 0), 2);
  this->last_light_text_ = text;
}

void CrowPanelUi::paint_sensor_icons_() {
  if (this->images_.size() > 3)
    this->draw_image_(this->images_[3]);
  if (this->images_.size() > 4)
    this->draw_image_(this->images_[4]);
}

void CrowPanelUi::paint_sensor_values_() {
  char temp_text[12];
  char hum_text[12];
  if (this->sensor_seen_) {
    std::snprintf(temp_text, sizeof(temp_text), "%.1f", this->temperature_c_);
    std::snprintf(hum_text, sizeof(hum_text), "%.1f", this->humidity_);
  } else {
    std::snprintf(temp_text, sizeof(temp_text), "--.-");
    std::snprintf(hum_text, sizeof(hum_text), "--.-");
  }

  if (this->last_temp_text_ == temp_text && this->last_hum_text_ == hum_text)
    return;

  if (!this->last_temp_text_.empty())
    this->draw_text_(206, SENSOR_VALUE_Y, this->last_temp_text_, rgb565(255, 255, 255), 2);
  if (!this->last_hum_text_.empty())
    this->draw_text_(341, SENSOR_VALUE_Y, this->last_hum_text_, rgb565(255, 255, 255), 2);

  this->draw_text_(206, SENSOR_VALUE_Y, temp_text, rgb565(0, 0, 0), 2);
  this->draw_text_(341, SENSOR_VALUE_Y, hum_text, rgb565(0, 0, 0), 2);
  this->last_temp_text_ = temp_text;
  this->last_hum_text_ = hum_text;
}

void CrowPanelUi::draw_image_(const ImagePlacement &image) {
  if (this->frame_buffer_ == nullptr || image.data == nullptr)
    return;

  for (int yy = 0; yy < image.height; yy++) {
    const int dst_y = image.y + yy;
    if (dst_y < 0 || dst_y >= LCD_HEIGHT)
      continue;

    for (int xx = 0; xx < image.width; xx++) {
      const int dst_x = image.x + xx;
      if (dst_x < 0 || dst_x >= LCD_WIDTH)
        continue;

      const int src_index = (yy * image.width + xx) * 3;
      if (image.data[src_index + 2] < 128)
        continue;

      const uint16_t color = image.data[src_index] | (image.data[src_index + 1] << 8);
      this->frame_buffer_[dst_y * LCD_WIDTH + dst_x] = color;
    }
  }
}

void CrowPanelUi::draw_text_(int x, int y, const std::string &text, uint16_t color, int scale) {
  int cursor_x = x;
  for (char c : text) {
    this->draw_char_(cursor_x, y, c, color, scale);
    cursor_x += 9 * scale;
  }
}

void CrowPanelUi::draw_char_(int x, int y, char c, uint16_t color, int scale) {
  static const uint8_t digit_font[10][10] = {
      {0x3C, 0x7E, 0xC3, 0xC7, 0xCB, 0xD3, 0xE3, 0xC3, 0x7E, 0x3C},
      {0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E},
      {0x3C, 0x7E, 0xC3, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF, 0xFF},
      {0x7E, 0xFF, 0x03, 0x06, 0x1E, 0x03, 0x03, 0xC3, 0x7E, 0x3C},
      {0x06, 0x0E, 0x1E, 0x36, 0x66, 0xFF, 0xFF, 0x06, 0x06, 0x06},
      {0xFF, 0xFF, 0xC0, 0xFC, 0xFE, 0x03, 0x03, 0xC3, 0x7E, 0x3C},
      {0x3C, 0x7E, 0xC0, 0xFC, 0xFE, 0xC3, 0xC3, 0xC3, 0x7E, 0x3C},
      {0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60},
      {0x3C, 0x7E, 0xC3, 0x66, 0x3C, 0x7E, 0xC3, 0xC3, 0x7E, 0x3C},
      {0x3C, 0x7E, 0xC3, 0xC3, 0xC3, 0x7F, 0x3F, 0x03, 0x7E, 0x3C},
  };
  static const uint8_t dash_font[10] = {0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t dot_font[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18};
  static const uint8_t c_font[10] = {0x3E, 0x7F, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x7F, 0x3E};
  static const uint8_t percent_font[10] = {0xC3, 0xC3, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC3, 0xC3, 0x00};
  static const uint8_t o_font[10] = {0x3C, 0x7E, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0x7E, 0x3C};
  static const uint8_t n_font[10] = {0xC3, 0xE3, 0xF3, 0xDB, 0xCF, 0xC7, 0xC3, 0xC3, 0xC3, 0xC3};
  static const uint8_t f_font[10] = {0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFC, 0xC0, 0xC0, 0xC0, 0xC0};

  const uint8_t *glyph = nullptr;
  if (c >= '0' && c <= '9')
    glyph = digit_font[c - '0'];
  else if (c == '-')
    glyph = dash_font;
  else if (c == '.')
    glyph = dot_font;
  else if (c == 'C' || c == 'c')
    glyph = c_font;
  else if (c == '%')
    glyph = percent_font;
  else if (c == 'O' || c == 'o')
    glyph = o_font;
  else if (c == 'N' || c == 'n')
    glyph = n_font;
  else if (c == 'F' || c == 'f')
    glyph = f_font;
  else
    return;

  for (int row = 0; row < 10; row++) {
    for (int col = 0; col < 8; col++) {
      if ((glyph[row] & (1 << (7 - col))) == 0)
        continue;
      for (int yy = 0; yy < scale; yy++) {
        for (int xx = 0; xx < scale; xx++) {
          this->draw_pixel_(x + col * scale + xx, y + row * scale + yy, color);
        }
      }
    }
  }
}

void CrowPanelUi::draw_pixel_(int x, int y, uint16_t color) {
  if (this->frame_buffer_ == nullptr || x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
    return;
  this->frame_buffer_[y * LCD_WIDTH + x] = color;
}

void CrowPanelUi::setup_touch_() {
  gpio_set_direction(PIN_TOUCH_CS, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_TOUCH_CS, 1);
  gpio_set_direction(PIN_TOUCH_CLK, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_TOUCH_CLK, 0);
  gpio_set_direction(PIN_TOUCH_DIN, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_TOUCH_DIN, 0);
  gpio_set_direction(PIN_TOUCH_OUT, GPIO_MODE_INPUT);
  gpio_set_pull_mode(PIN_TOUCH_OUT, GPIO_PULLUP_ONLY);
  gpio_set_direction(PIN_TOUCH_IRQ, GPIO_MODE_INPUT);
  gpio_set_pull_mode(PIN_TOUCH_IRQ, GPIO_PULLUP_ONLY);
}

void CrowPanelUi::handle_touch_() {
  const bool touched = gpio_get_level(PIN_TOUCH_IRQ) == 0;
  if (!touched) {
    this->touch_was_down_ = false;
    return;
  }

  const uint16_t raw_x = this->touch_read_(0xD0);
  const uint16_t raw_y = this->touch_read_(0x90);

  if (!this->touch_was_down_) {
    int screen_x = 0;
    int screen_y = 0;
    if (this->touch_hits_light_(raw_x, raw_y, &screen_x, &screen_y)) {
      this->set_light_state(!this->light_on_);
      ESP_LOGI(TAG, "Light toggled: GPIO18=%s raw=(%u,%u) screen=(%d,%d)", this->light_on_ ? "HIGH" : "LOW", raw_x,
               raw_y, screen_x, screen_y);
    } else {
      ESP_LOGD(TAG, "Touch ignored outside light icon raw=(%u,%u) screen=(%d,%d)", raw_x, raw_y, screen_x, screen_y);
    }
  }
  this->touch_was_down_ = true;
}

bool CrowPanelUi::touch_hits_light_(uint16_t raw_x, uint16_t raw_y, int *screen_x, int *screen_y) {
  const int mapped_x = this->map_touch_axis_(raw_x, 250, 3900, LCD_WIDTH - 1);
  const int mapped_y = this->map_touch_axis_(raw_y, 250, 3900, LCD_HEIGHT - 1);

  if (screen_x != nullptr)
    *screen_x = mapped_x;
  if (screen_y != nullptr)
    *screen_y = mapped_y;
  return this->point_hits_light_(mapped_x, mapped_y);
}

bool CrowPanelUi::point_hits_light_(int x, int y) {
  static constexpr int TOUCH_MARGIN = 28;
  return x >= LIGHT_ICON_X - TOUCH_MARGIN && x < LIGHT_ICON_X + LIGHT_ICON_W + TOUCH_MARGIN &&
         y >= LIGHT_ICON_Y - TOUCH_MARGIN && y < LIGHT_ICON_Y + LIGHT_ICON_H + TOUCH_MARGIN;
}

int CrowPanelUi::map_touch_axis_(uint16_t value, int in_min, int in_max, int out_max) {
  int v = static_cast<int>(value);
  if (v < in_min)
    v = in_min;
  if (v > in_max)
    v = in_max;
  return (v - in_min) * out_max / (in_max - in_min);
}

uint8_t CrowPanelUi::touch_transfer_(uint8_t data) {
  uint8_t result = 0;
  for (int bit = 7; bit >= 0; bit--) {
    gpio_set_level(PIN_TOUCH_DIN, (data >> bit) & 0x01);
    delayMicroseconds(1);
    gpio_set_level(PIN_TOUCH_CLK, 1);
    delayMicroseconds(1);
    result <<= 1;
    if (gpio_get_level(PIN_TOUCH_OUT))
      result |= 0x01;
    gpio_set_level(PIN_TOUCH_CLK, 0);
    delayMicroseconds(1);
  }
  return result;
}

uint16_t CrowPanelUi::touch_read_(uint8_t command) {
  gpio_set_level(PIN_TOUCH_CS, 0);
  this->touch_transfer_(command);
  const uint16_t high = this->touch_transfer_(0x00);
  const uint16_t low = this->touch_transfer_(0x00);
  gpio_set_level(PIN_TOUCH_CS, 1);
  return ((high << 8) | low) >> 3;
}

void CrowPanelUi::setup_i2c_() {
  this->i2c_sda_high_();
  this->i2c_scl_high_();
  delay(20);
  this->i2c_start_();
  if (this->i2c_write_byte_((AHT20_ADDRESS << 1) | 0)) {
    this->i2c_write_byte_(0xBE);
    this->i2c_write_byte_(0x08);
    this->i2c_write_byte_(0x00);
    ESP_LOGI(TAG, "AHT20/AHT21 init command sent");
  }
  this->i2c_stop_();
  delay(20);
  ESP_LOGI(TAG, "AHT20/AHT21 bit-bang I2C ready on SDA=GPIO37 SCL=GPIO38");
}

bool CrowPanelUi::read_aht20_() {
  uint8_t data[6] = {};

  this->i2c_start_();
  if (!this->i2c_write_byte_((AHT20_ADDRESS << 1) | 0)) {
    this->i2c_stop_();
    this->sensor_seen_ = false;
    ESP_LOGW(TAG, "AHT20/AHT21 not responding at address 0x38");
    return false;
  }
  this->i2c_write_byte_(0xAC);
  this->i2c_write_byte_(0x33);
  this->i2c_write_byte_(0x00);
  this->i2c_stop_();

  delay(90);

  this->i2c_start_();
  if (!this->i2c_write_byte_((AHT20_ADDRESS << 1) | 1)) {
    this->i2c_stop_();
    this->sensor_seen_ = false;
    ESP_LOGW(TAG, "AHT20/AHT21 read failed at address 0x38");
    return false;
  }
  for (int i = 0; i < 6; i++)
    data[i] = this->i2c_read_byte_(i < 5);
  this->i2c_stop_();

  if ((data[0] & 0x80) != 0) {
    ESP_LOGW(TAG, "AHT20/AHT21 is busy");
    return false;
  }

  const uint32_t raw_humidity = (static_cast<uint32_t>(data[1]) << 12) |
                                (static_cast<uint32_t>(data[2]) << 4) |
                                (static_cast<uint32_t>(data[3]) >> 4);
  const uint32_t raw_temperature = ((static_cast<uint32_t>(data[3]) & 0x0F) << 16) |
                                   (static_cast<uint32_t>(data[4]) << 8) |
                                   static_cast<uint32_t>(data[5]);
  this->humidity_ = static_cast<float>(raw_humidity) * 100.0f / 1048576.0f;
  this->temperature_c_ = static_cast<float>(raw_temperature) * 200.0f / 1048576.0f - 50.0f;
  if (this->humidity_ < 0.0f)
    this->humidity_ = 0.0f;
  if (this->humidity_ > 100.0f)
    this->humidity_ = 100.0f;
  this->sensor_seen_ = true;
  ESP_LOGD(TAG, "AHT20/AHT21 temperature=%.1fC humidity=%.1f%%", this->temperature_c_, this->humidity_);
  return true;
}

void CrowPanelUi::i2c_sda_high_() {
  gpio_set_direction(PIN_I2C_SDA, GPIO_MODE_INPUT);
  gpio_set_pull_mode(PIN_I2C_SDA, GPIO_PULLUP_ONLY);
}

void CrowPanelUi::i2c_sda_low_() {
  gpio_set_direction(PIN_I2C_SDA, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_I2C_SDA, 0);
}

void CrowPanelUi::i2c_scl_high_() {
  gpio_set_direction(PIN_I2C_SCL, GPIO_MODE_INPUT);
  gpio_set_pull_mode(PIN_I2C_SCL, GPIO_PULLUP_ONLY);
}

void CrowPanelUi::i2c_scl_low_() {
  gpio_set_direction(PIN_I2C_SCL, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_I2C_SCL, 0);
}

bool CrowPanelUi::i2c_read_sda_() {
  return gpio_get_level(PIN_I2C_SDA) != 0;
}

void CrowPanelUi::i2c_delay_() {
  delayMicroseconds(5);
}

void CrowPanelUi::i2c_start_() {
  this->i2c_sda_high_();
  this->i2c_scl_high_();
  this->i2c_delay_();
  this->i2c_sda_low_();
  this->i2c_delay_();
  this->i2c_scl_low_();
}

void CrowPanelUi::i2c_stop_() {
  this->i2c_sda_low_();
  this->i2c_delay_();
  this->i2c_scl_high_();
  this->i2c_delay_();
  this->i2c_sda_high_();
  this->i2c_delay_();
}

bool CrowPanelUi::i2c_write_byte_(uint8_t data) {
  for (int bit = 7; bit >= 0; bit--) {
    if ((data >> bit) & 0x01)
      this->i2c_sda_high_();
    else
      this->i2c_sda_low_();
    this->i2c_delay_();
    this->i2c_scl_high_();
    this->i2c_delay_();
    this->i2c_scl_low_();
  }

  this->i2c_sda_high_();
  this->i2c_delay_();
  this->i2c_scl_high_();
  this->i2c_delay_();
  const bool ack = !this->i2c_read_sda_();
  this->i2c_scl_low_();
  return ack;
}

uint8_t CrowPanelUi::i2c_read_byte_(bool ack) {
  uint8_t data = 0;
  this->i2c_sda_high_();
  for (int bit = 7; bit >= 0; bit--) {
    this->i2c_delay_();
    this->i2c_scl_high_();
    this->i2c_delay_();
    if (this->i2c_read_sda_())
      data |= 1 << bit;
    this->i2c_scl_low_();
  }

  if (ack)
    this->i2c_sda_low_();
  else
    this->i2c_sda_high_();
  this->i2c_delay_();
  this->i2c_scl_high_();
  this->i2c_delay_();
  this->i2c_scl_low_();
  this->i2c_sda_high_();
  return data;
}

void CrowPanelUi::fill_rect_(int x, int y, int w, int h, uint16_t color) {
  if (this->frame_buffer_ == nullptr)
    return;

  int x2 = x + w;
  int y2 = y + h;
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x2 > LCD_WIDTH)
    x2 = LCD_WIDTH;
  if (y2 > LCD_HEIGHT)
    y2 = LCD_HEIGHT;

  for (int yy = y; yy < y2; yy++) {
    uint16_t *row = this->frame_buffer_ + yy * LCD_WIDTH;
    for (int xx = x; xx < x2; xx++) {
      row[xx] = color;
    }
  }
}

}  // namespace crowpanel_ui
}  // namespace esphome

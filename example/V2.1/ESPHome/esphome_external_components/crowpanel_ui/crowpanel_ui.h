#pragma once

#include "esphome/core/component.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace crowpanel_ui {

class CrowPanelUi : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_light_state(bool state);
  bool get_light_state() const { return this->light_on_; }
  float get_temperature() const { return this->sensor_seen_ ? this->temperature_c_ : NAN; }
  float get_humidity() const { return this->sensor_seen_ ? this->humidity_ : NAN; }
  void add_image_data(const uint8_t *data, int width, int height, int x, int y) {
    this->images_.push_back({data, width, height, x, y});
  }

 protected:
  struct ImagePlacement {
    const uint8_t *data;
    int width;
    int height;
    int x;
    int y;
  };

  void paint_boot_pattern_();
  void paint_images_();
  void paint_light_button_();
  void paint_light_label_();
  void paint_sensor_icons_();
  void paint_sensor_values_();
  void draw_image_(const ImagePlacement &image);
  void fill_rect_(int x, int y, int w, int h, uint16_t color);
  void draw_text_(int x, int y, const std::string &text, uint16_t color, int scale);
  void draw_char_(int x, int y, char c, uint16_t color, int scale);
  void draw_pixel_(int x, int y, uint16_t color);
  void setup_touch_();
  void handle_touch_();
  bool touch_hits_light_(uint16_t raw_x, uint16_t raw_y, int *screen_x, int *screen_y);
  bool point_hits_light_(int x, int y);
  int map_touch_axis_(uint16_t value, int in_min, int in_max, int out_max);
  uint8_t touch_transfer_(uint8_t data);
  uint16_t touch_read_(uint8_t command);
  void setup_i2c_();
  bool read_aht20_();
  void i2c_sda_high_();
  void i2c_sda_low_();
  void i2c_scl_high_();
  void i2c_scl_low_();
  bool i2c_read_sda_();
  void i2c_delay_();
  void i2c_start_();
  void i2c_stop_();
  bool i2c_write_byte_(uint8_t data);
  uint8_t i2c_read_byte_(bool ack);

  void *panel_handle_{nullptr};
  uint16_t *frame_buffer_{nullptr};
  std::vector<ImagePlacement> images_;
  bool light_on_{false};
  bool touch_was_down_{false};
  uint32_t last_touch_check_{0};
  uint32_t last_sensor_read_{0};
  bool sensor_seen_{false};
  float temperature_c_{0.0f};
  float humidity_{0.0f};
  std::string last_temp_text_;
  std::string last_hum_text_;
  std::string last_light_text_;
};

}  // namespace crowpanel_ui
}  // namespace esphome

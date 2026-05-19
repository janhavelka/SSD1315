idf_component_register(
  SRCS "main.cpp" "ssd1315_idf_i2c.cpp"
  INCLUDE_DIRS "."
  REQUIRES SSD1315 esp_driver_i2c esp_driver_gpio esp_timer freertos
)

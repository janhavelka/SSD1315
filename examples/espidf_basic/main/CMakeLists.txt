idf_component_register(
  SRCS "main.cpp" "../../common/IdfI2cTransport.cpp"
  INCLUDE_DIRS "." "../../common" "../../.."
  REQUIRES SSD1315 esp_driver_i2c esp_driver_gpio esp_timer freertos vfs
)

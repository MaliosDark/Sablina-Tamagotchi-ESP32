if(CONFIG_SABLINA_USE_INTERNAL_LLM_APP)
  idf_component_register(
    SRCS
      "app_main.c"
      "llm.c"
      "lcd_ui.c"
      "ble_bridge.c"
    INCLUDE_DIRS
      "."
      "$ENV{IDF_PATH}/components/spiffs/include"
      "$ENV{IDF_PATH}/components/esp_lcd/include"
    REQUIRES
      spiffs
      nvs_flash
      bt
      esp_lcd
      driver
  )
else()
  idf_component_register(
    SRCS
      "arduino_entry.cpp"
      "sablina_sketch.cpp"
      "internal_llm_bridge.c"
      "llm.c"
      "../arduino_libs/TFT_eSPI-master/TFT_eSPI.cpp"
      "../arduino_libs/Adafruit_NeoPixel/Adafruit_NeoPixel.cpp"
      "../arduino_libs/Adafruit_NeoPixel/esp.c"
      "../managed_components/espressif__arduino-esp32/libraries/NetworkClientSecure/src/ssl_client.cpp"
    INCLUDE_DIRS
      "."
      "../arduino_libs/TFT_eSPI-master"
      "../arduino_libs/Adafruit_NeoPixel"
      "../arduino_libs/ArduinoJson/src"
      "../../SablinaTamagotchi_2.0"
    REQUIRES espressif__arduino-esp32
  )
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE -fno-if-conversion -Wno-format-truncation -Wno-error -Wno-error=misleading-indentation -Wno-format-extra-args)
spiffs_create_partition_image(data ../data FLASH_IN_PROJECT)

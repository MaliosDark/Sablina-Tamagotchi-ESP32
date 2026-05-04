idf_component_register(
  SRCS
    "app_main.c"
    "llm.c"
    "lcd_ui.c"
    "ble_bridge.c"
  INCLUDE_DIRS
    "."
  REQUIRES
    spiffs
    nvs_flash
    bt
    driver
)

target_compile_options(${COMPONENT_LIB} PRIVATE -fno-if-conversion -Wno-format-truncation -Wno-error -Wno-error=misleading-indentation -Wno-format-extra-args)
spiffs_create_partition_image(data ../data FLASH_IN_PROJECT)

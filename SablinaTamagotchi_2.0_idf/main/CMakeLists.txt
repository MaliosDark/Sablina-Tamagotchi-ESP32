idf_component_register(
  SRCS
    "app_main.c"
    "llm.c"
    "ble_bridge.c"
  INCLUDE_DIRS "."
  LDFRAGMENTS "../linker.lf"
  REQUIRES nvs_flash spiffs driver
)

target_compile_options(${COMPONENT_LIB} PRIVATE -fno-if-conversion)
spiffs_create_partition_image(data ../data FLASH_IN_PROJECT)

# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "data.bin"
  "esp-idf/esptool_py/flasher_args.json.in"
  "esp-idf/mbedtls/x509_crt_bundle"
  "flash_app_args"
  "flash_bootloader_args"
  "flasher_args.json"
  "https_server.crt.S"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "littlefs_py_venv"
  "project_elf_src_esp32s3.c"
  "rmaker_claim_service_server.crt.S"
  "rmaker_mqtt_server.crt.S"
  "rmaker_ota_server.crt.S"
  "sablina_tamagotchi_2_0_idf.map"
  "x509_crt_bundle.S"
  )
endif()

idf_component_register(SRCS "TicTac.c" "TicTac.h" "mqtt.c" "esp_rest_main.c"
                            "rest_server.c"
                            "detector.c"
                    PRIV_REQUIRES esp_http_server esp_driver_gpio fatfs json spiffs nvs_flash esp_timer esp_adc esp_wifi mqtt
                    INCLUDE_DIRS ".")

if(CONFIG_EXAMPLE_WEB_DEPLOY_SF)
    set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../front/web-demo")
    if(EXISTS ${WEB_SRC_DIR}/dist)
        spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist FLASH_IN_PROJECT)
    else()
        message(FATAL_ERROR "${WEB_SRC_DIR}/dist doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
    endif()
endif()

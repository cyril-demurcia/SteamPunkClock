/* HTTP Restful API Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_vfs_semihost.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "sdmmc_cmd.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mdns.h"
#include "esp_wifi.h"

#include "lwip/apps/netbiosns.h"
#include "protocol_examples_common.h"
#if CONFIG_EXAMPLE_WEB_DEPLOY_SD
#include "driver/sdmmc_host.h"
#endif

#define MDNS_INSTANCE "esp home web server"

static volatile bool wifi_connected = false;

// Methode qui viennent d'autre source C
esp_err_t start_rest_server(const char *base_path);
void startTicTacProcessing();
void mqtt_start();

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ESP_LOGI("WIFI", "Got IP, disabling power save");
        esp_wifi_set_ps(WIFI_PS_NONE);   // 🔴 FIX CRUCIAL
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGW("WIFI", "WiFi disconnected, reconnecting...");
        esp_wifi_connect();
    }
}

static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set(CONFIG_EXAMPLE_MDNS_HOST_NAME);
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                           ESP_EVENT_ANY_ID,
                                           &wifi_event_handler,
                                           NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                           IP_EVENT_STA_GOT_IP,
                                           &wifi_event_handler,
                                           NULL));

    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name(CONFIG_EXAMPLE_MDNS_HOST_NAME);

    // Example connect is the example provided by espresif handling WiFi
    // Chat GPT :
    // Cette fonction :
    // initialise le Wi-Fi STA
    // se connecte à l’AP
    // retourne quand l’IP est obtenue

    // ⚠️ MAIS :

    // elle n’installe pas de gestion robuste des déconnexions

    // elle laisse le Wi-Fi en power-save par défaut

    // elle ne te donne pas de flag wifi_connected

    // 👉 C’est exactement ce qui explique tes pertes Wi-Fi → pertes MQTT.


    ESP_ERROR_CHECK(example_connect());
    ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));

    mqtt_start();

    // Warning : contains an infinite loop. Add nothing after
    startTicTacProcessing();
}

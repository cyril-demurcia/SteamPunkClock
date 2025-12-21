#include "mqtt_client.h"
#include "esp_log.h"
#include "TicTac.h"
#include "esp_log.h"
#include "cJSON.h"

static esp_mqtt_client_handle_t client = NULL;

static const char *TAG = "PIEZO_TICTAC (MQTT)";
static char currentTime[9]; // HH:MM:SS

// -----------------------------
// TOPICS
// -----------------------------
const char* DISCOVERY_TOPIC = "homeassistant/sensor/horloge/config";
const char* STATE_TOPIC     = "Horloge";

 const char* DISCOVERY_PAYLOAD = 
        "{"
          "\"name\": \"Horloge\","
          "\"unique_id\": \"mqtt_horloge_sensor\","
          "\"state_topic\": \"Horloge\","
          "\"value_template\": \"{{ value_json.time }}\","
          "\"icon\": \"mdi:clock-digital\""
        "}";

char* whatTimeIsIt() {
    // Calculer les heures, minutes et secondes actuelles
    long totalSeconds = clockTimeReference.hours * 3600 + clockTimeReference.minutes * 60 + ticTacNumbers;

    unsigned int hours   = (totalSeconds / 3600) % 24;   // modulo 24 pour l'heure
    unsigned int minutes = (totalSeconds % 3600) / 60;
    unsigned int seconds = totalSeconds % 60;

    // Allocation exacte de la chaîne "HH:MM:SS" + '\0'
    snprintf(currentTime, 9, "%02u:%02u:%02u", hours, minutes, seconds);

    return currentTime;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker !");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Déconnected from MQTT broker");
            break;

        default:
            break;
    }
}



void publishTime()
{
    
    if (client == NULL) return;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "time", currentTime);
    // exemple garde ci besoins cJSON_AddNumberToObject(root, "minutes", clockTimeReference.minutes);
    const char *timeInfo = cJSON_Print(root);

    
    // Convert time into a readable hour
    whatTimeIsIt();
    ESP_LOGI(TAG, "     publishing : %s", currentTime);
    esp_mqtt_client_publish(client,
                            STATE_TOPIC,
                            timeInfo,
                            0,
                            1,
                            0);
}

void startListeningTicTacs(void *arg) {
    // consumer loop: affiche les tic et tacs reçus
    while (true) {
        uint32_t ts;
        if (TictacQueue != NULL && xQueueReceive(TictacQueue, &ts, pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "Tick @ %u ms (queue)", ts);
            // ici: notifier MQTT
            ticTacNumbers++;
            publishTime();

            // IMPORTANT pour laisser respirer (IDLE)
            vTaskDelay(pdMS_TO_TICKS(10));   
        }
    }
}

/**
 * To declare a new device to MQTT Broker, publish for example, the following JSON declaration
 * {
 *  "name": "Horloge",
 *  "unique_id": "mqtt_horloge_sensor",
 *  "state_topic": "Horloge",
 *  "value_template": "{{ value_json.time }}"
 *  "icon": "mdi:clock-digital"
 * }
 * IMPORTANT : The message must be sent mode with retain=true
 */
void declareDeviceToMqttBroker() {
    ESP_LOGI(TAG, "     Declaring device to Broker : %s", DISCOVERY_PAYLOAD);
    esp_mqtt_client_publish(client,
                            DISCOVERY_TOPIC,
                            DISCOVERY_PAYLOAD,
                            0,
                            1,
                            true);  // retain = true
}

// Entry Point :
//   - intialize the connection with MQTT serveur
//   - start listening the tic tacs
//   - send to MQTT
void mqtt_start()
{
    // A rendre parametrable par API rest
    ESP_LOGI(TAG, "Trying to establish connection with MQTT Server ...");
    esp_mqtt_client_config_t mqtt_cfg = {
        //.broker.address.uri = "mqtt://192.168.1.102:1883",   // ton broker Mosquitto
        .broker.address.uri = "mqtt://homeassistant.local:1883",   // ton broker Mosquitto
        .credentials.username="mqtt",
        .credentials.authentication.password = "mqtt",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    // Declare Device
    declareDeviceToMqttBroker();

    // create sensor task that read pinned to core 1
    xTaskCreate(startListeningTicTacs, "mqtt_task", 4096, NULL, 5, NULL);
}

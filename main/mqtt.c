#include "mqtt_client.h"
#include "esp_log.h"
#include "TicTac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"

static esp_mqtt_client_handle_t client = NULL;

static const char *TAG = "MQTT";
static char currentTime[9]; // HH:MM:SS
static bool mqtt_connected = false;

// -----------------------------
// TOPICS
// -----------------------------
const char* CLOCK_DISCOVERY_TOPIC = "homeassistant/sensor/horloge_time/config";
const char* CLOCK_RATIO_DISCOVERY_TOPIC = "homeassistant/sensor/horloge_ratio/config";
const char* STATE_TOPIC     = "Horloge";

        // Déclaration MQTT Discovery pour le sensor "Horloge" (time)
void declareClockTimeSensor()
{
    cJSON *root = cJSON_CreateObject();

    // Propriétés du sensor
    cJSON_AddStringToObject(root, "name", "Horloge");
    cJSON_AddStringToObject(root, "unique_id", "mqtt_horloge_time_sensor");
    cJSON_AddStringToObject(root, "state_topic", STATE_TOPIC); // "Horloge"
    cJSON_AddStringToObject(root, "value_template", "{{ value_json.time }}");
    cJSON_AddStringToObject(root, "icon", "mdi:clock-digital");

    // Bloc device pour regrouper les sensors
    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "identifiers", "esp32_horloge_01");
    cJSON_AddStringToObject(device, "name", "ESP32 Horloge");
    cJSON_AddStringToObject(device, "manufacturer", "Custom");
    cJSON_AddStringToObject(device, "model", "ESP32");

    cJSON_AddItemToObject(root, "device", device);

    // Générer le payload JSON
    char *payload = cJSON_PrintUnformatted(root);

    // Publier sur le topic MQTT Discovery avec retain=true
    esp_mqtt_client_publish(client,
                            CLOCK_DISCOVERY_TOPIC, // "homeassistant/sensor/horloge_time/config"
                            payload,
                            0,
                            1,  // QoS 1
                            true);  // retain

    // Libérer la mémoire
    cJSON_Delete(root);
    free(payload);

    ESP_LOGD(TAG, "Horloge Time sensor declared via MQTT Discovery");
}

/**
 * To declare a new device to MQTT Broker, publish for example, the following JSON declaration
 * IMPORTANT : The message must be sent with mode  retain=true
 */
void declareDeviceSensor() {

    // Declaration du bloc Ratio
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "Ratio Second");
    cJSON_AddStringToObject(root, "unique_id", "mqtt_ratio_sec_sensor");
    cJSON_AddStringToObject(root, "state_topic", STATE_TOPIC);
    cJSON_AddStringToObject(root, "value_template", "{{ value_json.ratio_sec }}");
    cJSON_AddStringToObject(root, "icon", "mdi:clock-digital");
    cJSON_AddStringToObject(root, "unit_of_measurement", "%");
    cJSON_AddStringToObject(root, "state_class", "measurement");

    // Bloc device
    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "identifiers", "esp32_horloge_01");
    cJSON_AddStringToObject(device, "name", "ESP32 Horloge");
    cJSON_AddStringToObject(device, "manufacturer", "Custom");
    cJSON_AddStringToObject(device, "model", "ESP32");

    cJSON_AddItemToObject(root, "device", device);

    char *payload = cJSON_PrintUnformatted(root);
    esp_mqtt_client_publish(client, CLOCK_RATIO_DISCOVERY_TOPIC, payload, 0, 1, true);
    cJSON_Delete(root);
    free(payload);

}

void computeCurrentTime() {
    // Calculer les heures, minutes et secondes actuelles
    long totalSeconds = clockTimeReference.hours * 3600 + clockTimeReference.minutes * 60 + ticTacNumbers * 615 / 600;

    unsigned int hours   = (totalSeconds / 3600) % 24;   // modulo 24 pour l'heure
    unsigned int minutes = (totalSeconds % 3600) / 60;
    unsigned int seconds = totalSeconds % 60;

    // Allocation exacte de la chaîne "HH:MM:SS" + '\0'
    snprintf(currentTime, 9, "%02u:%02u:%02u", hours, minutes, seconds);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGD(TAG, "Connected to MQTT broker !");
            mqtt_connected = true;
            // Declare Device
            declareClockTimeSensor();
            declareDeviceSensor();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Déconnected from MQTT broker");
            mqtt_connected = false;
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "Error event from MQTT broker. Trying to reconnect ...");
            esp_mqtt_client_reconnect(client);
            break;

        default:
            break;
    }
}

void publish(float ratio)
{
    
    if (client == NULL || mqtt_connected == false) {
        return;
    }

    // Convert time into a readable hour
    computeCurrentTime(); // Compute currentTime

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "time", currentTime);
    cJSON_AddNumberToObject(root, "ratio_sec", (int)(ratio*100.0));
    // exemple garde ci besoins cJSON_AddNumberToObject(root, "minutes", clockTimeReference.minutes);
    char *json = cJSON_Print(root);

    
    ESP_LOGD(TAG, "     publishing : %s", currentTime);
    esp_mqtt_client_publish(client,
                            STATE_TOPIC,
                            json,
                            0,
                            0,   // QOS mettre 0 pour ne pas attendre l'acquittement
                            false);  // retain = false 
    cJSON_Delete(root); 
    free(json);
}

void startListeningTicTacs(void *arg) {

    int64_t lastTime = esp_timer_get_time();
    int64_t currentTime = lastTime;

    // consumer loop: affiche les tic et tacs reçus
    while (true) {
 
        uint32_t ts;
        if (TictacQueue != NULL && xQueueReceive(TictacQueue, &ts, pdMS_TO_TICKS(1000))) {
            ESP_LOGD(TAG, "Tick @ %u ms (queue)", ts);
            currentTime = esp_timer_get_time();
            float ratio = (float)(currentTime - lastTime)/1000000.0; // timer est en micro
            publish(ratio);
            lastTime = currentTime;

            // ici: notifier MQTT
            ticTacNumbers++;
            publish(ratio);

            // IMPORTANT pour laisser respirer (IDLE)
            vTaskDelay(pdMS_TO_TICKS(10));   
            //ESP_LOGD("HEAP", "free heap: %u", esp_get_free_heap_size());
        }
    }
}

// Entry Point :
//   - intialize the connection with MQTT serveur
//   - start listening the tic tacs
//   - send to MQTT
void mqtt_start()
{
    
    // A rendre parametrable par API rest
    ESP_LOGD(TAG, "Trying to establish connection with MQTT Server ...");
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://homeassistant.local:1883",   // ton broker Mosquitto
        //.broker.address.uri = "mqtt://192.168.1.102:1883",   // ton broker Mosquitto
        .credentials.username="mqtt",
        .credentials.authentication.password = "mqtt",
        .session.keepalive = 120,
        .network.disable_auto_reconnect = false,
        .buffer.size = 2048,
        .buffer.out_size = 2048
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_mqtt_client_start(client);

    // create sensor task that read pinned to core 1
    xTaskCreate(startListeningTicTacs, "mqtt_task", 4096, NULL, 5, NULL);
}

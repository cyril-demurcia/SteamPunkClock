// piezo_detector_idf5.c
// Compatible ESP-IDF v5.x (5.5.1)
// Utilise adc_oneshot et le nouveau driver adc_cali

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"     // pour adc_oneshot_*
#include "esp_adc/adc_cali.h"       // pour adc_cali_handle_t et fonctions générales
#include "esp_adc/adc_cali_scheme.h"// pour _create_scheme_line_fitting() et structs de config
#include "TicTac.h"

// CONFIG
#define SAMPLE_RATE_HZ      8000    // Hz
#define WINDOW_MS           4      // ms pour RMS
#define DEBOUNCE_MS         250     // ms
#define HP_ALPHA 0.90

// ADC mapping: ADC unit 1, channel 0 -> GPIO36 sur la plupart des ESP32
#define ADC_UNIT_ID         ADC_UNIT_1
#define ADC_IN_CHANNEL      ADC_CHANNEL_0   // ADC1_CH0 (GPIO36)
#define ADC_ATTEN           ADC_ATTEN_DB_12
#define ADC_BITWIDTH        ADC_BITWIDTH_DEFAULT

static const char *TAG = "PIEZO_TICTAC (SENSOR)";

// README
// On ne peut pas faire une tache qui lit periodiquement les valeurs de l'ADC à 8kHz =>
// Cela aurait pour effet de bloquer FreeRTOS.
// On utilise un timer à 8kHz, qui lit une valeur et la met dans une queue : ici SAMPLE_QUEUE.
// Ensuite sensor_task qui est définie comme une tache sur un coeur dédié vient lire les echantillons dans
// la queue. Il effectue ensuite le traitement et si il ya une détection l'écrit à son tour dans une queue
// Il s'agit de la queue TICTAC_QUEUE.
// Reste à lire sur cette queue et publier sur MQtt les tics tacss

// Processed after calibration
// ---------------------------
double ambient_sum = 0.0;
int ambient_count = 0;
double ambient_mean_mv = 0.0;

// handles
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;

/* Initialise ADC oneshot (ADC1) */
static esp_err_t adc_oneshot_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_ID,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };
    ret = adc_oneshot_config_channel(adc_handle, ADC_IN_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
        return ret;
    }
    return ESP_OK;
}

/* Initialise la calibration (line fitting preferred) */
static esp_err_t adc_cali_init(void)
{
    // vérifier les schemes supportés (optionnel mais utile)
    adc_cali_scheme_ver_t param; // Parametre de retour
    esp_err_t error = adc_cali_check_scheme(&param);
    ESP_LOGI(TAG, "adc_cali_check_scheme -> 0x%X", (unsigned)error);

    // essayer line fitting en priorité si supporté
    if (error == ESP_OK && param == ADC_CALI_SCHEME_VER_LINE_FITTING) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_ID,
            .atten = ADC_ATTEN,
            .bitwidth = ADC_BITWIDTH,
        };
        esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "adc_cali: line fitting scheme created");
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "adc_cali_create_scheme_line_fitting failed: %s", esp_err_to_name(ret));
        }
    }

    // si aucun scheme disponible, renvoyer erreur (on peut continuer sans calibration mais moins précis)
    ESP_LOGW(TAG, "No ADC calibration scheme available; readings will be raw (un-calibrated)");
    adc_cali_handle = NULL;
    return ESP_OK;
}

/* Convertit raw->mV via le handle de calibration si présent, sinon renvoie raw*approx */
static esp_err_t convert_raw_to_mv(int raw, int *out_mv)
{
    if (adc_cali_handle) {
        return adc_cali_raw_to_voltage(adc_cali_handle, raw, out_mv);
    } else {
        // fallback approximatif : calcul simple en supposant range 0..Vmax
        // attention : dépend de l'atténuation; pour ADC_ATTEN_DB_11, Vmax ~ 2.45V typique.
        // on renvoie une approximation en mV pour log/debug seulement.
        const float Vmax = 2450.0f; // mV approximé pour ATTEN_11
        const int max_raw = (1 << 12) - 1; // 12 bits -> 4095
        *out_mv = (int)((raw / (float)max_raw) * Vmax);
        return ESP_OK;
    }
}

/* Task capteur : échantillonne, calcule RMS et détecte tic */
static void sensor_task(void *arg)
{
    const int samplesPerWindow = (SAMPLE_RATE_HZ * WINDOW_MS) / 1000;

    double baseline_noise = 0.001;
    double prev_x = 0.0;
    double prev_y = 0.0;
    uint64_t last_detect_ms = 0;

    // Pre calibration pendant 2 seconde
    ESP_LOGI(TAG, "=============== Micro calibration ================");
    ambient_mean_mv = 0;
    for (int i = 0; i < 2*SAMPLE_RATE_HZ; i++) { // 2 s à 8 kHz
        int raw;
        xQueueReceive(SamplesQueue, &raw, portMAX_DELAY);
        int mv;
        convert_raw_to_mv(raw, &mv);
        ambient_mean_mv += mv;
    }
    ambient_mean_mv = ambient_mean_mv / (2.0*SAMPLE_RATE_HZ);
    ESP_LOGI(TAG, "=============== Ambient mean %.6e ================", ambient_mean_mv);

    ESP_LOGI(TAG, "=============== Starting Listening Tic/Tacs ================");
    while (1) {
        
        int adcSamples[samplesPerWindow];
        for (int i = 0; i < samplesPerWindow; i++) {
            xQueueReceive(SamplesQueue, &adcSamples[i], portMAX_DELAY);
        }
        
        double energy = 0.0;
        
        for (int i = 0; i < samplesPerWindow; i++) {
            int mv;
            convert_raw_to_mv(adcSamples[i], &mv);
            
            // 1️⃣ Centrage
            double x = ((double)mv - ambient_mean_mv) / 1000.0; // V
            
            // 2️⃣ Passe-haut 1er ordre
            double y = HP_ALPHA * (prev_y + x - prev_x);
            prev_x = x;
            prev_y = y;
            
            // 3️⃣ Énergie
            energy += y * y;
        }
        
        // 4️⃣ Baseline lente (seulement hors événement)
        double energy_norm = energy / samplesPerWindow;
        
        double COEF_MAGIQUE = 10.0;
        double threshold = baseline_noise * COEF_MAGIQUE;
        if (threshold < 1e-6) threshold = 1e-6;
        
        uint64_t now_ms = esp_timer_get_time() / 1000ULL;
        
        if (energy_norm < threshold) {
            baseline_noise = baseline_noise * 0.99 + energy_norm * 0.01;
        }
        
        // 5️⃣ Détection
        if (energy_norm > threshold &&
            (now_ms - last_detect_ms) > DEBOUNCE_MS) {
                
                last_detect_ms = now_ms;
                
                ESP_LOGI(TAG,
                    "TIC/TAC energy=%.6e thr=%.6e baseline=%.6e",
                    energy_norm, threshold, baseline_noise);
                    
                    uint32_t t = (uint32_t)now_ms;
                    xQueueSend(TictacQueue, &t, 0);
                }
            }
}

static void IRAM_ATTR timerCallback(void *arg)
{
    int raw;
    esp_err_t r = adc_oneshot_read(adc_handle, ADC_IN_CHANNEL, &raw);

    if (r != ESP_OK) {
    ESP_LOGI(TAG, "     Error reading sample");
    // en cas d'erreur, skip
    raw = 0;
}

    // mettre dans un buffer FIFO (ring buffer ou queue)
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(SamplesQueue, &raw, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

// Start periodic timer that read values to push them into sample Queue
void startAdcSampling()
{
    const esp_timer_create_args_t timer_args = {
        .callback = timerCallback,
        .arg = NULL,
        .name = "adc_sampler"
    };
    esp_timer_handle_t h;
    esp_timer_create(&timer_args, &h);

    // 8 kHz → période = 125 µs = sampleDelayUs
    const int sampleDelayUs = 1000000 / SAMPLE_RATE_HZ;

    esp_timer_start_periodic(h, sampleDelayUs);
}

//        MAIN
// -------------------
void startTicTacProcessing()
{
    ESP_LOGI(TAG, "Starting piezo detector (IDF v5.x style)");
    TictacQueue = xQueueCreate(16, sizeof(uint32_t));
    if (!TictacQueue) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    ESP_ERROR_CHECK(adc_oneshot_init());
    ESP_ERROR_CHECK(adc_cali_init());

    // taille de la queue : ici, on stocke 1024 échantillons max
    // Il s'agit de la queue qui va être remplie par lecture adc_one_shot dans le timer sur interruption
    SamplesQueue = xQueueCreate(1024, sizeof(int));
    if (SamplesQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create SAMPLE_QUEUE");
        return;
    }

    // Start reading from ADC
    startAdcSampling();

    // create sensor task that read pinned to core 1
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);

}

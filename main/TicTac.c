#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "TicTac.h"

// This queue is where smaples data are written from piezo sensor to be processed by algorithm to detect Tic Tac
QueueHandle_t SamplesQueue;
// This queue is where a tic tac is written after it has been detecte
QueueHandle_t TictacQueue;

// Count the global number of tictacs since the esp32 started
long ticTacNumbers = 0;

// The global time to be set through Rest API
// The value here is only used the first time. After the value is read from the Non Volatim Memory
ClockTime_t clockTimeReference = {12,34};

// Filter alpogorithm
double ALPHA = 0.65;

char* MQTT_SERVER_ADRESS = "homeassistant.local";
int MQTT_PORT_NUMBER = 1883;
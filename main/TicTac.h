#ifndef TIC_TAC_H
#define TIC_TAC_H

// Piezo sensor  => Data read at 8 khz => SAMPLE_QUEUE => Detectio Algo => TITAC_QUEUE
// ------------------------------------------------------------------------------------

// This queue is where smaples data are written from piezo sensor to be processed by algorithm to detect Tic Tac
extern QueueHandle_t SamplesQueue;
// This queur is where a tic tac is written after it has been detecte
extern QueueHandle_t TictacQueue;

// Count the global number of tictacs since the esp32 started
extern long ticTacNumbers;

typedef struct  {
    int hours;
    int minutes;
} ClockTime_t;

extern ClockTime_t clockTimeReference;

extern char* MQTT_SERVER_ADRESS;
extern int MQTT_PORT_NUMBER;

#endif
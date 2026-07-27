#ifndef SEN54_DATA_H
#define SEN54_DATA_H

typedef struct {
    char node_id[16];

    float pm1;
    float pm25;
    float pm4;
    float pm10;
    float temperature;
    float humidity;
    float voc;
    float nox;

} sen54_data_t;


extern sen54_data_t current_sensor_data;

#endif
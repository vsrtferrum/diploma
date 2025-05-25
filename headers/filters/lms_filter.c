#include <stdlib.h>
#include <string.h>
#include "lms_filter.h"

int lms_filter_init(lms_filter *filter, int length, float mu) {
    if (!filter || length <= 0 || mu <= 0.0f) {
        return -1;
    }

    filter->length = length;
    filter->mu = mu;
    
    filter->weights = (float*)calloc(length, sizeof(float));
    filter->buffer = (float*)calloc(length, sizeof(float));
    
    if (!filter->weights || !filter->buffer) {
        lms_filter_free(filter);
        return -2;
    }
    
    filter->position = 0;
    return 0;
}

void lms_filter_free(lms_filter *filter) {
    if (filter) {
        free(filter->weights);
        free(filter->buffer);
    }
}

float lms_filter_process(lms_filter *filter, float input, float desired) {
    // Обновляем буфер входных отсчетов
    filter->buffer[filter->position] = input;
    
    // Вычисляем выходной отсчет
    float output = 0.0f;
    for (int i = 0; i < filter->length; i++) {
        int idx = (filter->position - i + filter->length) % filter->length;
        output += filter->weights[i] * filter->buffer[idx];
    }
    
    // Вычисляем ошибку
    float error = desired - output;
    
    // Обновляем веса
    for (int i = 0; i < filter->length; i++) {
        int idx = (filter->position - i + filter->length) % filter->length;
        filter->weights[i] += filter->mu * error * filter->buffer[idx];
    }
    
    // Обновляем позицию
    filter->position = (filter->position + 1) % filter->length;
    
    return output;
} 
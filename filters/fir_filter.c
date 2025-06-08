#include "fir_filter.h"

int fir_filter_init(fir_filter *fir, const float *coefficients, int length) {
    if(length <= 0 || !coefficients) {
        return -1;
    }
    
    fir->length = length;
    fir->coefficients = (float*)malloc(length * sizeof(float));
    if(!fir->coefficients) {
        return -2;
    }
    
    fir->buffer = (float*)calloc(length, sizeof(float));
    if(!fir->buffer) {
        free(fir->coefficients);
        return -3;
    }
    
    fir->position = 0;
    memcpy(fir->coefficients, coefficients, length * sizeof(float));
    return 0;
}

void fir_filter_free(fir_filter *fir) {
    free(fir->coefficients);
    free(fir->buffer);
}

float fir_filter_process(fir_filter *fir, float input) {
    fir->buffer[fir->position] = input;
    fir->position = (fir->position + 1) % fir->length;

    float output = 0.0f;
    int index = fir->position;

    for (int i = 0; i < fir->length; i++) {
        index = (index == 0) ? fir->length - 1 : index - 1;
        output += fir->coefficients[i] * fir->buffer[index];
    }

    return output;
} 
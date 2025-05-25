#ifndef FIR_FILTER_H
#define FIR_FILTER_H

#include <stdlib.h>
#include <string.h>

typedef struct {
    float *coefficients;  
    float *buffer;       
    int length;           
    int position;         
} fir_filter;

// Объявления функций
int fir_filter_init(fir_filter *fir, const float *coefficients, int length);
void fir_filter_free(fir_filter *fir);
float fir_filter_process(fir_filter *fir, float input);

#endif // FIR_FILTER_H

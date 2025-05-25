#ifndef RLS_FILTER_H
#define RLS_FILTER_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef struct {
    float *weights;    // веса фильтра
    float *buffer;     // буфер входных отсчетов
    float *P;          // матрица P
    int length;        // длина фильтра
    float lambda;      // фактор забывания
    float delta;       // параметр регуляризации
    int position;      // текущая позиция в буфере
} rls_filter;

int rls_filter_init(rls_filter *filter, int length, float lambda, float delta);
void rls_filter_free(rls_filter *filter);
float rls_filter_process(rls_filter *filter, float input, float desired);

#endif // RLS_FILTER_H


#ifndef LMS_FILTER_H
#define LMS_FILTER_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    float *weights;    // веса фильтра
    float *buffer;     // буфер входных отсчетов
    int length;        // длина фильтра
    float mu;          // шаг адаптации
    int position;      // текущая позиция в буфере
} lms_filter;

int lms_filter_init(lms_filter *filter, int length, float mu);
void lms_filter_free(lms_filter *filter);
float lms_filter_process(lms_filter *filter, float input, float desired);

#endif // LMS_FILTER_H


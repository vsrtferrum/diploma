#include <stdlib.h>
#include <string.h>
#include "rls_filter.h"

int rls_filter_init(rls_filter *filter, int length, float lambda, float delta) {
    if (!filter || length <= 0 || lambda <= 0.0f || lambda > 1.0f || delta <= 0.0f) {
        return -1;
    }

    filter->length = length;
    filter->lambda = lambda;
    filter->delta = delta;
    
    filter->weights = (float*)calloc(length, sizeof(float));
    filter->buffer = (float*)calloc(length, sizeof(float));
    filter->P = (float*)malloc(length * length * sizeof(float));
    
    if (!filter->weights || !filter->buffer || !filter->P) {
        rls_filter_free(filter);
        return -2;
    }
    
    // Инициализация матрицы P
    for (int i = 0; i < length; i++) {
        for (int j = 0; j < length; j++) {
            filter->P[i * length + j] = (i == j) ? 1.0f / delta : 0.0f;
        }
    }
    
    filter->position = 0;
    return 0;
}

void rls_filter_free(rls_filter *filter) {
    if (filter) {
        free(filter->weights);
        free(filter->buffer);
        free(filter->P);
    }
}

float rls_filter_process(rls_filter *filter, float input, float desired) {
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
    
    // Вычисляем вектор k (коэффициент Калмана)
    float *k = (float*)malloc(filter->length * sizeof(float));
    if (!k) return output;
    
    // P * x
    for (int i = 0; i < filter->length; i++) {
        k[i] = 0.0f;
        for (int j = 0; j < filter->length; j++) {
            int idx = (filter->position - j + filter->length) % filter->length;
            k[i] += filter->P[i * filter->length + j] * filter->buffer[idx];
        }
    }
    
    // x^T * P * x
    float denominator = filter->lambda;
    for (int i = 0; i < filter->length; i++) {
        int idx = (filter->position - i + filter->length) % filter->length;
        denominator += filter->buffer[idx] * k[i];
    }
    
    // k = k / (lambda + x^T * P * x)
    for (int i = 0; i < filter->length; i++) {
        k[i] /= denominator;
    }
    
    // Обновляем веса
    for (int i = 0; i < filter->length; i++) {
        filter->weights[i] += k[i] * error;
    }
    
    // Обновляем матрицу P
    float *P_new = (float*)malloc(filter->length * filter->length * sizeof(float));
    if (!P_new) {
        free(k);
        return output;
    }
    
    for (int i = 0; i < filter->length; i++) {
        for (int j = 0; j < filter->length; j++) {
            P_new[i * filter->length + j] = (filter->P[i * filter->length + j] - 
                k[i] * k[j] * denominator) / filter->lambda;
        }
    }
    
    memcpy(filter->P, P_new, filter->length * filter->length * sizeof(float));
    free(P_new);
    free(k);
    
    // Обновляем позицию
    filter->position = (filter->position + 1) % filter->length;
    
    return output;
} 
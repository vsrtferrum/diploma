// iir_filter.h
#ifndef IIR_FILTER_H
#define IIR_FILTER_H

typedef struct {
    float* b_coeffs;  // Коэффициенты числителя
    float* a_coeffs;  // Коэффициенты знаменателя
    float* x_buffer;  // Буфер входных отсчетов
    float* y_buffer;  // Буфер выходных отсчетов
    int b_length;     // Длина числителя
    int a_length;     // Длина знаменателя
    int x_pos;        // Позиция в буфере входных отсчетов
    int y_pos;        // Позиция в буфере выходных отсчетов
} iir_filter;

// Инициализация IIR фильтра
int iir_filter_init(iir_filter* filter, const float* b_coeffs, int b_length, 
                   const float* a_coeffs, int a_length);

int iir_filter_set_section(iir_filter* filter, int section, 
                          float b0, float b1, float b2,
                          float a0, float a1, float a2);

// Освобождение ресурсов
void iir_filter_free(iir_filter* filter);

// Обработка одного отсчета
float iir_filter_process(iir_filter* filter, float input);

#endif
#include "iir_filter.h"
#include <stdlib.h>
#include <string.h>

int iir_filter_init(iir_filter* filter, const float* b_coeffs, int b_length, 
                   const float* a_coeffs, int a_length) {
    // Проверка корректности длин коэффициентов
    if (b_length % 3 != 0 || a_length % 3 != 0 || b_length != a_length) {
        return -1; // Некорректные длины
    }
    int num_sections = b_length / 3;

    // Выделение памяти под коэффициенты
    filter->b_coeffs = (float*)malloc(b_length * sizeof(float));
    filter->a_coeffs = (float*)malloc(a_length * sizeof(float));
    if (!filter->b_coeffs || !filter->a_coeffs) {
        iir_filter_free(filter);
        return -1;
    }

    // Копирование коэффициентов, если предоставлены
    if (b_coeffs) {
        memcpy(filter->b_coeffs, b_coeffs, b_length * sizeof(float));
    }
    if (a_coeffs) {
        memcpy(filter->a_coeffs, a_coeffs, a_length * sizeof(float));
    }

    filter->b_length = b_length;
    filter->a_length = a_length;

    // Выделение памяти под буферы состояний (инициализация нулями)
    filter->x_buffer = (float*)calloc(2 * num_sections, sizeof(float));
    filter->y_buffer = (float*)calloc(2 * num_sections, sizeof(float));
    if (!filter->x_buffer || !filter->y_buffer) {
        iir_filter_free(filter);
        return -1;
    }

    filter->x_pos = 0;
    filter->y_pos = 0;

    return 0;
}

int iir_filter_set_section(iir_filter* filter, int section, 
                          float b0, float b1, float b2,
                          float a0, float a1, float a2) {
    int num_sections = filter->b_length / 3;
    if (section < 0 || section >= num_sections) {
        return -1;
    }
    filter->b_coeffs[section * 3] = b0;
    filter->b_coeffs[section * 3 + 1] = b1;
    filter->b_coeffs[section * 3 + 2] = b2;
    filter->a_coeffs[section * 3] = a0;
    filter->a_coeffs[section * 3 + 1] = a1;
    filter->a_coeffs[section * 3 + 2] = a2;
    return 0;
}

void iir_filter_free(iir_filter* filter) {
    free(filter->b_coeffs);
    free(filter->a_coeffs);
    free(filter->x_buffer);
    free(filter->y_buffer);
    filter->b_coeffs = NULL;
    filter->a_coeffs = NULL;
    filter->x_buffer = NULL;
    filter->y_buffer = NULL;
    filter->b_length = 0;
    filter->a_length = 0;
    filter->x_pos = 0;
    filter->y_pos = 0;
}

float iir_filter_process(iir_filter* filter, float input) {
    float output = input;
    int num_sections = filter->b_length / 3;

    for (int i = 0; i < num_sections; ++i) {
        const float* b = &filter->b_coeffs[i * 3];
        const float* a = &filter->a_coeffs[i * 3];
        float* x_buf = &filter->x_buffer[i * 2];
        float* y_buf = &filter->y_buffer[i * 2];

        // Текущий вход для секции (выход предыдущей секции)
        float x = output;

        // Вычисление нового выхода
        float y = (b[0] * x + b[1] * x_buf[0] + b[2] * x_buf[1]
                 - a[1] * y_buf[0] - a[2] * y_buf[1]) / a[0];

        // Обновление состояний входа
        x_buf[1] = x_buf[0];
        x_buf[0] = x;

        // Обновление состояний выхода
        y_buf[1] = y_buf[0];
        y_buf[0] = y;

        output = y;
    }

    return output;
}
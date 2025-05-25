#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../headers/filters/fir_filter.h"
#include "../headers/filters/iir_filter.h"
#include "../headers/filters/lms_filter.h"
#include "../headers/filters/rls_filter.h"

#include "test_signals.h"
#include "performance_tests.h"

#define MAX_COEFFS 1024
#define SIGNAL_LENGTH 1000  // Длина тестового сигнала

typedef struct {
    float* a_coeffs;
    float* b_coeffs;
    int a_length;
    int b_length;
} IIRCoeffs;

typedef struct {
    float* weights;
    int length;
} AdaptiveCoeffs;

static int read_fir_coefficients(const char* filename, float** coeffs, int* num_coeffs) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Ошибка открытия файла");
        return -1;
    }

    char line[256];
    int reading_fir = 0;
    *num_coeffs = 0;
    *coeffs = malloc(MAX_COEFFS * sizeof(float));

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        
        if (strstr(line, "FIR Filter Coefficients")) {
            reading_fir = 1;
            continue;
        }
        
        if (reading_fir) {
            if (strlen(line) == 0 || strstr(line, "=====")) {
                continue;
            }
            
            float val;
            char* comma = strchr(line, ',');
            if (comma) *comma = '\0';
            
            if (sscanf(line, "%f", &val) == 1) {
                if(*num_coeffs >= MAX_COEFFS) {
                    fprintf(stderr, "Превышен максимальный размер коэффициентов\n");
                    break;
                }
                (*coeffs)[(*num_coeffs)++] = val;
            }
        }
    }

    fclose(file);
    
    if(*num_coeffs == 0) {
        fprintf(stderr, "Не найдено коэффициентов FIR в файле\n");
        free(*coeffs);
        return -2;
    }
    
    return 0;
}

static int read_iir_coefficients(iir_filter* filter, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Ошибка открытия файла %s\n", filename);
        return -1;
    }

    char line[1024];
    int reading_sos = 0;
    int section_count = 0;
    float b0, b1, b2, a0, a1, a2;

    // Сначала подсчитаем количество секций
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "SOS sections")) {
            reading_sos = 1;
            continue;
        }
        if (reading_sos && strstr(line, "---")) {
            break;
        }
        if (reading_sos && sscanf(line, "%f, %f, %f, %f, %f, %f", 
                                &b0, &b1, &b2, &a0, &a1, &a2) == 6) {
            section_count++;
        }
    }

    if (section_count == 0) {
        printf("Не найдено SOS секций\n");
        fclose(file);
        return -2;
    }

    // Выделяем память для коэффициентов
    float* b_coeffs = malloc(section_count * 3 * sizeof(float));
    float* a_coeffs = malloc(section_count * 3 * sizeof(float));
    if (!b_coeffs || !a_coeffs) {
        printf("Ошибка выделения памяти\n");
        fclose(file);
        free(b_coeffs);
        free(a_coeffs);
        return -3;
    }

    // Возвращаемся в начало файла
    rewind(file);
    reading_sos = 0;
    int current_section = 0;

    // Читаем коэффициенты
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "SOS sections")) {
            reading_sos = 1;
            continue;
        }
        if (reading_sos && strstr(line, "---")) {
            break;
        }
        if (reading_sos && sscanf(line, "%f, %f, %f, %f, %f, %f", 
                                &b0, &b1, &b2, &a0, &a1, &a2) == 6) {
            int offset = current_section * 3;
            b_coeffs[offset] = b0;
            b_coeffs[offset + 1] = b1;
            b_coeffs[offset + 2] = b2;
            a_coeffs[offset] = a0;
            a_coeffs[offset + 1] = a1;
            a_coeffs[offset + 2] = a2;
            current_section++;
        }
    }

    fclose(file);

    // Инициализируем фильтр
    int result = iir_filter_init(filter, b_coeffs, section_count * 3, 
                                a_coeffs, section_count * 3);
    
    free(b_coeffs);
    free(a_coeffs);
    
    return result;
}

static int read_adaptive_coefficients(const char* filename, AdaptiveCoeffs* coeffs, const char* filter_type) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Ошибка открытия файла");
        return -1;
    }

    char line[256];
    int reading_weights = 0;
    coeffs->weights = malloc(MAX_COEFFS * sizeof(float));
    coeffs->length = 0;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        
        if (strstr(line, filter_type)) {
            reading_weights = 1;
            continue;
        }
        
        if (reading_weights) {
            if (strlen(line) == 0 || strstr(line, "=====")) {
                continue;
            }
            
            if (strstr(line, "Real") || strstr(line, "Imag")) {
                continue;
            }
            
            // Удаляем все пробелы и табуляции
            char* p = line;
            char* q = line;
            while (*p) {
                if (*p != ' ' && *p != '\t') {
                    *q++ = *p;
                }
                p++;
            }
            *q = '\0';
            
            float real, imag;
            char* j_pos = strchr(line, 'j');
            if (j_pos) {
                *j_pos = '\0';  // Удаляем 'j' из строки
                if (sscanf(line, "%f%f", &real, &imag) == 2) {
                    if (coeffs->length >= MAX_COEFFS) {
                        fprintf(stderr, "Превышен максимальный размер весов\n");
                        break;
                    }
                    coeffs->weights[coeffs->length++] = real;
                }
            }
        }
    }

    fclose(file);
    
    if (coeffs->length == 0) {
        fprintf(stderr, "Не найдено весов %s в файле\n", filter_type);
        free(coeffs->weights);
        return -2;
    }
    
    return 0;
}

static void print_test_result(const char* test_name, const TestResult* result) {
    printf("\n=== %s ===\n", test_name);
    printf("Время обработки: %.4f сек\n", result->processing_time);
    printf("Скорость обработки: %.2f кГц\n", result->samples_per_second / 1000.0);
    printf("MSE: %.6f\n", result->mse);
}

static void test_fir_filter(float* fir_coeffs, int num_coeffs, const Signal* clean_signal, const Signal* noisy_signal) {
    printf("\n=== Тестирование FIR фильтра ===\n");
    
    fir_filter fir;
    if(fir_filter_init(&fir, fir_coeffs, num_coeffs) != 0) {
        fprintf(stderr, "Ошибка инициализации FIR фильтра\n");
        return;
    }

    TestResult perf_result = test_filter_performance(&fir, noisy_signal, FILTER_TYPE_FIR);
    print_test_result("Тест производительности", &perf_result);

    TestResult noise_result = test_noise_reduction(&fir, clean_signal, noisy_signal, FILTER_TYPE_FIR);
    print_test_result("Тест подавления шума", &noise_result);

    TestResult freq_result = test_frequency_response(&fir, 50.0f, 1.0f, FILTER_TYPE_FIR);
    print_test_result("Тест частотной характеристики", &freq_result);

    fir_filter_free(&fir);
}

static void test_iir_filter(const Signal* clean_signal, const Signal* noisy_signal) {
    printf("\n=== Тестирование IIR фильтра ===\n");
    
    iir_filter filter;
    if (read_iir_coefficients(&filter, "coeffs.txt") != 0) {
        fprintf(stderr, "Ошибка чтения коэффициентов IIR фильтра\n");
        return;
    }

    TestResult perf_result = test_filter_performance(&filter, noisy_signal, FILTER_TYPE_IIR);
    print_test_result("Тест производительности", &perf_result);

    TestResult noise_result = test_noise_reduction(&filter, clean_signal, noisy_signal, FILTER_TYPE_IIR);
    print_test_result("Тест подавления шума", &noise_result);

    TestResult freq_result = test_frequency_response(&filter, 50.0f, 1.0f, FILTER_TYPE_IIR);
    print_test_result("Тест частотной характеристики", &freq_result);

    iir_filter_free(&filter);
}

static void test_lms_filter(const Signal* clean_signal, const Signal* noisy_signal) {
    printf("\n=== Тестирование LMS фильтра ===\n");
    
    AdaptiveCoeffs coeffs;
    if (read_adaptive_coefficients("coeffs.txt", &coeffs, "LMS Filter Weights") != 0) {
        fprintf(stderr, "Ошибка чтения весов LMS\n");
        return;
    }
    
    lms_filter lms;
    if(lms_filter_init(&lms, coeffs.length, 0.01f) != 0) {
        fprintf(stderr, "Ошибка инициализации LMS фильтра\n");
        free(coeffs.weights);
        return;
    }

    // Копируем веса из файла
    memcpy(lms.weights, coeffs.weights, coeffs.length * sizeof(float));

    TestResult perf_result = test_filter_performance(&lms, noisy_signal, FILTER_TYPE_LMS);
    print_test_result("Тест производительности", &perf_result);

    TestResult noise_result = test_noise_reduction(&lms, clean_signal, noisy_signal, FILTER_TYPE_LMS);
    print_test_result("Тест подавления шума", &noise_result);

    TestResult freq_result = test_frequency_response(&lms, 50.0f, 1.0f, FILTER_TYPE_LMS);
    print_test_result("Тест частотной характеристики", &freq_result);

    lms_filter_free(&lms);
    free(coeffs.weights);
}

static void test_rls_filter(const Signal* clean_signal, const Signal* noisy_signal) {
    printf("\n=== Тестирование RLS фильтра ===\n");
    
    AdaptiveCoeffs coeffs;
    if (read_adaptive_coefficients("coeffs.txt", &coeffs, "RLS Filter Weights") != 0) {
        fprintf(stderr, "Ошибка чтения весов RLS\n");
        return;
    }
    
    rls_filter rls;
    if(rls_filter_init(&rls, coeffs.length, 0.99f, 0.1f) != 0) {
        fprintf(stderr, "Ошибка инициализации RLS фильтра\n");
        free(coeffs.weights);
        return;
    }

    // Копируем веса из файла
    memcpy(rls.weights, coeffs.weights, coeffs.length * sizeof(float));

    TestResult perf_result = test_filter_performance(&rls, noisy_signal, FILTER_TYPE_RLS);
    print_test_result("Тест производительности", &perf_result);

    TestResult noise_result = test_noise_reduction(&rls, clean_signal, noisy_signal, FILTER_TYPE_RLS);
    print_test_result("Тест подавления шума", &noise_result);

    TestResult freq_result = test_frequency_response(&rls, 50.0f, 1.0f, FILTER_TYPE_RLS);
    print_test_result("Тест частотной характеристики", &freq_result);

    rls_filter_free(&rls);
    free(coeffs.weights);
}

int main(void) {
    float* fir_coeffs = NULL;
    int num_coeffs = 0;
    
    if (read_fir_coefficients("coeffs.txt", &fir_coeffs, &num_coeffs) != 0) {
        fprintf(stderr, "Ошибка чтения коэффициентов\n");
        return 1;
    }

    // Генерация тестовых сигналов
    Signal clean_signal = generate_sine_wave(50.0f, 1.0f, 5.0f);
    Signal noisy_signal = generate_noisy_signal(50.0f, 1.0f, 0.5f, 5.0f);
    
    if (clean_signal.length == 0 || noisy_signal.length == 0) {
        fprintf(stderr, "Ошибка генерации тестовых сигналов\n");
        goto cleanup;
    }

    test_fir_filter(fir_coeffs, num_coeffs, &clean_signal, &noisy_signal);
    test_iir_filter(&clean_signal, &noisy_signal);
    test_lms_filter(&clean_signal, &noisy_signal);
    test_rls_filter(&clean_signal, &noisy_signal);

cleanup:
    free_signal(&clean_signal);
    free_signal(&noisy_signal);
    free(fir_coeffs);

    return 0;
}
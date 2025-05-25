#ifndef PERFORMANCE_TESTS_H
#define PERFORMANCE_TESTS_H

#include "../headers/filters/fir_filter.h"
#include "../headers/filters/iir_filter.h"
#include "../headers/filters/lms_filter.h"
#include "../headers/filters/rls_filter.h"
#include "test_signals.h"

typedef struct {
    double processing_time;
    double samples_per_second;
    double mse;
} TestResult;

// Общие функции тестирования для всех типов фильтров
TestResult test_filter_performance(void* filter, const Signal* signal, int filter_type);
TestResult test_noise_reduction(void* filter, const Signal* clean, const Signal* noisy, int filter_type);
TestResult test_frequency_response(void* filter, float frequency, float amplitude, int filter_type);

// Константы для типов фильтров
#define FILTER_TYPE_FIR 0
#define FILTER_TYPE_IIR 1
#define FILTER_TYPE_LMS 2
#define FILTER_TYPE_RLS 3

#endif // PERFORMANCE_TESTS_H 
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "performance_tests.h"

static float process_sample(void* filter, float input, float desired, int filter_type) {
    switch (filter_type) {
        case FILTER_TYPE_FIR:
            return fir_filter_process((fir_filter*)filter, input);
        case FILTER_TYPE_IIR:
            return iir_filter_process((iir_filter*)filter, input);
        case FILTER_TYPE_LMS:
            return lms_filter_process((lms_filter*)filter, input, desired);
        case FILTER_TYPE_RLS:
            return rls_filter_process((rls_filter*)filter, input, desired);
        default:
            return 0.0f;
    }
}

TestResult test_filter_performance(void* filter, const Signal* signal, int filter_type) {
    TestResult result = {0};
    if (!filter || !signal || !signal->data) {
        return result;
    }
    
    clock_t start = clock();
    
    for (int i = 0; i < signal->length; i++) {
        process_sample(filter, signal->data[i], 0.0f, filter_type);
    }
    
    result.processing_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    result.samples_per_second = signal->length / result.processing_time;
    
    return result;
}

TestResult test_noise_reduction(void* filter, const Signal* clean, const Signal* noisy, int filter_type) {
    TestResult result = {0};
    if (!filter || !clean || !noisy || !clean->data || !noisy->data) {
        return result;
    }
    
    if (clean->length != noisy->length) {
        return result;
    }
    
    float* filtered = (float*)malloc(clean->length * sizeof(float));
    if (!filtered) {
        return result;
    }
    
    clock_t start = clock();
    
    for (int i = 0; i < clean->length; i++) {
        filtered[i] = process_sample(filter, noisy->data[i], clean->data[i], filter_type);
    }
    
    result.processing_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    result.samples_per_second = clean->length / result.processing_time;
    
    float max_amplitude = 0.0f;
    for (int i = 0; i < clean->length; i++) {
        if (isfinite(clean->data[i]) && fabs(clean->data[i]) > max_amplitude) {
            max_amplitude = fabs(clean->data[i]);
        }
    }
    
    if (max_amplitude < 1e-10f) {
        max_amplitude = 1.0f;
    }
    
    float mse = 0.0f;
    int valid_samples = 0;
    for (int i = 0; i < clean->length; i++) {
        if (isfinite(filtered[i]) && isfinite(clean->data[i])) {
            float error = (clean->data[i] - filtered[i]) / max_amplitude;
            mse += error * error;
            valid_samples++;
        }
    }
    
    if (valid_samples > 0) {
        result.mse = mse / valid_samples;
        if (result.mse > 1e6f) {
            result.mse = 1e6f;
        }
    } else {
        result.mse = 0.0f;
    }
    
    free(filtered);
    return result;
}

TestResult test_frequency_response(void* filter, float frequency, float amplitude, int filter_type) {
    TestResult result = {0};
    if (!filter) {
        return result;
    }
    
    Signal signal = generate_sine_wave(frequency, amplitude, 1.0f);
    if (signal.length == 0) {
        return result;
    }
    
    float* filtered = (float*)malloc(signal.length * sizeof(float));
    if (!filtered) {
        free_signal(&signal);
        return result;
    }
    
    clock_t start = clock();
    
    for (int i = 0; i < signal.length; i++) {
        filtered[i] = process_sample(filter, signal.data[i], 0.0f, filter_type);
    }
    
    result.processing_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    result.samples_per_second = signal.length / result.processing_time;
    
    float max_amplitude = 0.0f;
    for (int i = 0; i < signal.length; i++) {
        if (fabs(filtered[i]) > max_amplitude) {
            max_amplitude = fabs(filtered[i]);
        }
    }
    
    result.mse = max_amplitude / amplitude;
    
    free(filtered);
    free_signal(&signal);
    return result;
} 
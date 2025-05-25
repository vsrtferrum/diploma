#define _USE_MATH_DEFINES
#define M_PI 3.14159265358979323846
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test_signals.h"

Signal generate_sine_wave(float frequency, float amplitude, float duration) {
    int num_samples = (int)(duration * SAMPLE_RATE);
    Signal signal;
    signal.length = num_samples;
    signal.data = (float*)malloc(num_samples * sizeof(float));
    
    if (!signal.data) {
        signal.length = 0;
        return signal;
    }
    
    for (int i = 0; i < num_samples; i++) {
        float t = i / SAMPLE_RATE;
        signal.data[i] = amplitude * sin(2 * M_PI * frequency * t);
    }
    
    return signal;
}

Signal generate_noisy_signal(float frequency, float amplitude, float noise_level, float duration) {
    Signal signal = generate_sine_wave(frequency, amplitude, duration);
    
    if (signal.length == 0) {
        return signal;
    }
    
    for (int i = 0; i < signal.length; i++) {
        float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        signal.data[i] += noise * noise_level;
    }
    
    return signal;
}

Signal generate_multi_tone(float* frequencies, float* amplitudes, int num_tones, float duration) {
    int num_samples = (int)(duration * SAMPLE_RATE);
    Signal signal;
    signal.length = num_samples;
    signal.data = (float*)calloc(num_samples, sizeof(float));
    
    if (!signal.data) {
        signal.length = 0;
        return signal;
    }
    
    for (int i = 0; i < num_samples; i++) {
        float t = i / SAMPLE_RATE;
        for (int j = 0; j < num_tones; j++) {
            signal.data[i] += amplitudes[j] * sin(2 * M_PI * frequencies[j] * t);
        }
    }
    
    return signal;
}

void free_signal(Signal* signal) {
    if (signal && signal->data) {
        free(signal->data);
        signal->data = NULL;
        signal->length = 0;
    }
} 
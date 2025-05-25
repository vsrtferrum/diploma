#ifndef TEST_SIGNALS_H
#define TEST_SIGNALS_H

#include <math.h>

#define SAMPLE_RATE 1000.0

typedef struct {
    float* data;
    int length;
} Signal;

// Генерация чистого синусоидального сигнала
Signal generate_sine_wave(float frequency, float amplitude, float duration);

// Генерация сигнала с шумом
Signal generate_noisy_signal(float frequency, float amplitude, float noise_level, float duration);

// Генерация многочастотного сигнала
Signal generate_multi_tone(float* frequencies, float* amplitudes, int num_tones, float duration);

// Освобождение памяти сигнала
void free_signal(Signal* signal);

#endif // TEST_SIGNALS_H 
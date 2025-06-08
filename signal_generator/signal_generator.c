#define _USE_MATH_DEFINES

#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "signal_generator.h"


uint8_t* generate_random_bits(int num_bits) {
    uint8_t* bits = malloc(num_bits * sizeof(uint8_t));
    if (!bits) return NULL;
    
    srand(time(NULL));
    for (int i = 0; i < num_bits; i++) {
        bits[i] = rand() % 2;
    }
    return bits;
}

void add_noise_and_interference(complex_float* signal, int length, float noise_power, 
                               float interference_freq, float interference_power, 
                               float fs) {
    float noise_std = sqrtf(noise_power);
    float interf_std = sqrtf(interference_power);
    float phase_inc = 2 * M_PI * interference_freq / fs;
    float phase = 0.0f;
    
    for (int i = 0; i < length; i++) {
        // Гауссов шум
        float noise_real = 0;
        float noise_imag = 0;
        for (int j = 0; j < 12; j++) {
            noise_real += (float)rand() / RAND_MAX - 0.5f;
            noise_imag += (float)rand() / RAND_MAX - 0.5f;
        }
        noise_real *= noise_std;
        noise_imag *= noise_std;
        
        // Узкополосная помеха
        float interf_real = cosf(phase) * interf_std;
        float interf_imag = sinf(phase) * interf_std;
        phase += phase_inc;
        if (phase > 2 * M_PI) phase -= 2 * M_PI;
        
        // Добавляем шум и помеху к сигналу
        signal[i].real += noise_real + interf_real;
        signal[i].imag += noise_imag + interf_imag;
    }
}
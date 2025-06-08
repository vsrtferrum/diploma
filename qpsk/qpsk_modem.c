#define _USE_MATH_DEFINES
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "qpsk_modem.h"



complex_float* qpsk_modulate(const uint8_t* bits, int num_bits, 
                            const qpsk_params* params, int* out_length) {
    int num_symbols = num_bits / 2;
    *out_length = num_symbols * params->samples_per_sym;
    
    // Выделяем память под выходной сигнал
    complex_float* tx_signal = malloc(*out_length * sizeof(complex_float));
    if (!tx_signal) return NULL;
    
    // Генерируем несущую
    float* carrier_real = malloc(*out_length * sizeof(float));
    float* carrier_imag = malloc(*out_length * sizeof(float));
    if (!carrier_real || !carrier_imag) {
        free(tx_signal);
        free(carrier_real);
        free(carrier_imag);
        return NULL;
    }
    
    float phase_inc = 2 * M_PI * params->f_center / params->fs;
    float phase = 0.0f;
    for (int i = 0; i < *out_length; i++) {
        carrier_real[i] = cosf(phase);
        carrier_imag[i] = sinf(phase);
        phase += phase_inc;
        if (phase > 2 * M_PI) phase -= 2 * M_PI;
    }
    
    // Модуляция
    for (int i = 0; i < num_symbols; i++) {
        // Чтение двух битов
        uint8_t dibit = (bits[2*i] << 1) | bits[2*i+1];
        complex_float symbol;
        
        // Сопоставление бит с символом (Gray coding)
        switch (dibit) {
            case 0: // 00
                symbol.real =  1.0f / M_SQRT2;
                symbol.imag =  1.0f / M_SQRT2;
                break;
            case 1: // 01
                symbol.real = -1.0f / M_SQRT2;
                symbol.imag =  1.0f / M_SQRT2;
                break;
            case 2: // 10
                symbol.real =  1.0f / M_SQRT2;
                symbol.imag = -1.0f / M_SQRT2;
                break;
            case 3: // 11
                symbol.real = -1.0f / M_SQRT2;
                symbol.imag = -1.0f / M_SQRT2;
                break;
        }
        
        // Формируем импульс
        int start = i * params->samples_per_sym;
        for (int j = 0; j < params->samples_per_sym; j++) {
            int idx = start + j;
            tx_signal[idx].real = symbol.real * carrier_real[idx] - symbol.imag * carrier_imag[idx];
            tx_signal[idx].imag = symbol.real * carrier_imag[idx] + symbol.imag * carrier_real[idx];
        }
    }
    
    free(carrier_real);
    free(carrier_imag);
    return tx_signal;
}

uint8_t* qpsk_demodulate(const complex_float* signal, int signal_length,
                        const qpsk_params* params, int delay, 
                        int* out_num_bits, complex_float** out_constellation) {
    // Применяем задержку
    if (delay >= signal_length) delay = signal_length - 1;
    int processed_length = signal_length - delay;
    const complex_float* processed = &signal[delay];
    
    // Перенос в базовую полосу
    complex_float* baseband = malloc(processed_length * sizeof(complex_float));
    if (!baseband) return NULL;
    
    float phase_inc = 2 * M_PI * params->f_center / params->fs;
    float phase = 0.0f;
    for (int i = 0; i < processed_length; i++) {
        float cos_val = cosf(phase);
        float sin_val = sinf(phase);
        baseband[i].real = processed[i].real * cos_val + processed[i].imag * sin_val;
        baseband[i].imag = processed[i].imag * cos_val - processed[i].real * sin_val;
        phase += phase_inc;
        if (phase > 2 * M_PI) phase -= 2 * M_PI;
    }
    
    // Выборка символов
    int num_symbols = (processed_length + params->samples_per_sym - 1) / params->samples_per_sym;
    *out_num_bits = num_symbols * 2;
    uint8_t* decoded_bits = malloc(*out_num_bits * sizeof(uint8_t));
    *out_constellation = malloc(num_symbols * sizeof(complex_float));
    
    if (!decoded_bits || !*out_constellation) {
        free(baseband);
        free(decoded_bits);
        free(*out_constellation);
        return NULL;
    }
    
    for (int i = 0; i < num_symbols; i++) {
        // Среднее значение в середине символа
        int start = i * params->samples_per_sym + params->samples_per_sym / 4;
        int end = start + params->samples_per_sym / 2;
        if (end >= processed_length) end = processed_length - 1;
        
        complex_float avg = {0, 0};
        int count = 0;
        for (int j = start; j < end; j++) {
            avg.real += baseband[j].real;
            avg.imag += baseband[j].imag;
            count++;
        }
        avg.real /= count;
        avg.imag /= count;
        
        (*out_constellation)[i] = avg;
        
        // Решающее устройство
        float angle = atan2f(avg.imag, avg.real);
        uint8_t symbol;
        
        if (angle >= -M_PI_4 && angle < M_PI_4) {
            symbol = 0; // 00
        } else if (angle >= M_PI_4 && angle < 3*M_PI_4) {
            symbol = 1; // 01
        } else if (angle >= -3*M_PI_4 && angle < -M_PI_4) {
            symbol = 2; // 10
        } else {
            symbol = 3; // 11
        }
        
        decoded_bits[2*i] = (symbol >> 1) & 1;
        decoded_bits[2*i+1] = symbol & 1;
    }
    
    free(baseband);
    return decoded_bits;
}
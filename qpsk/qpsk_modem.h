#ifndef QPSK_MODEM_H
#define QPSK_MODEM_H

#include <stdint.h>
#include "../coeffs.h"

// Параметры модуляции
typedef struct {
    float f_center;       // Центральная частота (Гц)
    float fs;             // Частота дискретизации (Гц)
    int samples_per_sym;  // Отсчетов на символ
} qpsk_params;

// QPSK модуляция
complex_float* qpsk_modulate(const uint8_t* bits, int num_bits, 
                            const qpsk_params* params, int* out_length);

// QPSK демодуляция
uint8_t* qpsk_demodulate(const complex_float* signal, int signal_length,
                        const qpsk_params* params, int delay, 
                        int* out_num_bits, complex_float** out_constellation);

#endif // QPSK_MODEM_H
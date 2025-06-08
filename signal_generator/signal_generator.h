#ifndef SIGNAL_GENERATOR_H
#define SIGNAL_GENERATOR_H

#include "../qpsk/qpsk_modem.h"

// Генерация случайных битов
uint8_t* generate_random_bits(int num_bits);

// Добавление шума и помех к сигналу
void add_noise_and_interference(complex_float* signal, int length, float noise_power, 
                               float interference_freq, float interference_power, 
                               float fs);

#endif // SIGNAL_GENERATOR_H
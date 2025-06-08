#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "../filters/fir_filter.h"
#include "../filters/iir_filter.h"
#include "../filters/lms_filter.h"
#include "../filters/rls_filter.h"
#include "../signal_generator/signal_generator.h"

// Конфигурация теста
#define NUM_BITS 10000
#define SAMPLES_PER_SYMBOL 100
#define FS 5e9f           // Частота дискретизации 5 ГГц
#define F_CENTER 2140e6f  // Центральная частота 2.14 ГГц
#define NOISE_POWER 0.1f
#define INTERFERENCE_FREQ 2150e6f
#define INTERFERENCE_POWER 0.3f
#define LMS_LENGTH 64     // Длина LMS фильтра
#define RLS_LENGTH 64     // Длина RLS фильтра
#define LMS_MU 0.01f      // Шаг адаптации для LMS
#define RLS_LAMBDA 0.99f  // Фактор забывания для RLS
#define RLS_DELTA 0.01f   // Параметр регуляризации для RLS

// Прототипы функций
float calculate_ber(const uint8_t* original, const uint8_t* decoded, int length);
void run_benchmark(const char* name, complex_float* signal, int length, 
    int filter_delay, const qpsk_params* params,
    uint8_t* original_bits, int num_bits,
    complex_float* desired_signal); 

int main() {
    // Инициализация параметров модуляции
    qpsk_params params = {
        .f_center = F_CENTER,
        .fs = FS,
        .samples_per_sym = SAMPLES_PER_SYMBOL
    };
    
    // Генерация тестовых данных
    uint8_t* original_bits = generate_random_bits(NUM_BITS);
    if (!original_bits) {
        printf("Ошибка генерации битов\n");
        return 1;
    }
    
    int tx_length;
    complex_float* tx_signal = qpsk_modulate(original_bits, NUM_BITS, &params, &tx_length);
    if (!tx_signal) {
        printf("Ошибка модуляции\n");
        free(original_bits);
        return 1;
    }
    
    // Создание зашумленного сигнала
    complex_float* clean_signal = malloc(tx_length * sizeof(complex_float));
    complex_float* noisy_signal = malloc(tx_length * sizeof(complex_float));
    
    memcpy(clean_signal, tx_signal, tx_length * sizeof(complex_float));
    memcpy(noisy_signal, tx_signal, tx_length * sizeof(complex_float));
    add_noise_and_interference(noisy_signal, tx_length, NOISE_POWER, 
                              INTERFERENCE_FREQ, INTERFERENCE_POWER, FS);
        
    // Запуск тестов для каждого фильтра
    const char* conditions[] = {"Без шума", "С шумом"};
    complex_float* signals[] = {clean_signal, noisy_signal};
    
    for (int cond = 0; cond < 2; cond++) {
        printf("\n===== Условие: %s =====\n", conditions[cond]);
        
        // Для FIR и IIR desired_signal не используется
        run_benchmark("FIR", signals[cond], tx_length, FIR_NUMTAPS/2, &params, 
                     original_bits, NUM_BITS, NULL);
        run_benchmark("IIR", signals[cond], tx_length, IIR_ORDER*10, &params, 
                     original_bits, NUM_BITS, NULL);
        
        // Для адаптивных фильтров используем чистый сигнал как reference
        run_benchmark("LMS", signals[cond], tx_length, LMS_LENGTH/2, &params, 
                     original_bits, NUM_BITS, clean_signal);
        run_benchmark("RLS", signals[cond], tx_length, RLS_LENGTH/2, &params, 
                     original_bits, NUM_BITS, clean_signal);
    }
    
    // Очистка памяти
    free(clean_signal);
    free(noisy_signal);
    free(original_bits);
    free(tx_signal);
    
    return 0;
}

float calculate_ber(const uint8_t* original, const uint8_t* decoded, int length) {
    int errors = 0;
    for (int i = 0; i < length; i++) {
        if (original[i] != decoded[i]) errors++;
    }
    return (float)errors / length;
}

void run_benchmark(const char* name, complex_float* signal, int length, 
    int filter_delay, const qpsk_params* params,
    uint8_t* original_bits, int num_bits,
    complex_float* desired_signal) {
printf("\n[%s] Тестирование фильтра\n", name);

// Создаем фильтры для I и Q компонент
fir_filter fir_i = {0}, fir_q = {0};
iir_filter iir_i = {0}, iir_q = {0};
lms_filter lms_i = {0}, lms_q = {0};
rls_filter rls_i = {0}, rls_q = {0};

if (strcmp(name, "FIR") == 0) {
fir_filter_init(&fir_i, fir_coeff, FIR_NUMTAPS);
fir_filter_init(&fir_q, fir_coeff, FIR_NUMTAPS);
} else if (strcmp(name, "IIR") == 0) {
iir_filter_init(&iir_i, iir_b, IIR_ORDER + 1, iir_a, IIR_ORDER + 1);
iir_filter_init(&iir_q, iir_b, IIR_ORDER + 1, iir_a, IIR_ORDER + 1);
} else if (strcmp(name, "LMS") == 0) {
lms_filter_init(&lms_i, LMS_LENGTH, LMS_MU);
lms_filter_init(&lms_q, LMS_LENGTH, LMS_MU);
} else if (strcmp(name, "RLS") == 0) {
rls_filter_init(&rls_i, RLS_LENGTH, RLS_LAMBDA, RLS_DELTA);
rls_filter_init(&rls_q, RLS_LENGTH, RLS_LAMBDA, RLS_DELTA);
} else {
printf("Неизвестный тип фильтра\n");
return;
}

// Фильтрация сигнала
complex_float* filtered = malloc(length * sizeof(complex_float));

clock_t start = clock();

for (int i = 0; i < length; i++) {
float real = signal[i].real;
float imag = signal[i].imag;

if (strcmp(name, "FIR") == 0) {
filtered[i].real = fir_filter_process(&fir_i, real);
filtered[i].imag = fir_filter_process(&fir_q, imag);
} else if (strcmp(name, "IIR") == 0) {
filtered[i].real = iir_filter_process(&iir_i, real);
filtered[i].imag = iir_filter_process(&iir_q, imag);
} else if (strcmp(name, "LMS") == 0 && desired_signal) {
filtered[i].real = lms_filter_process(&lms_i, real, desired_signal[i].real);
filtered[i].imag = lms_filter_process(&lms_q, imag, desired_signal[i].imag);
} else if (strcmp(name, "RLS") == 0 && desired_signal) {
filtered[i].real = rls_filter_process(&rls_i, real, desired_signal[i].real);
filtered[i].imag = rls_filter_process(&rls_q, imag, desired_signal[i].imag);
} else {
// Если нет reference-сигнала, просто копируем вход
filtered[i].real = real;
filtered[i].imag = imag;
}
}

clock_t end = clock();
double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
double samples_per_sec = length / elapsed;

printf("Время обработки: %.4f сек\n", elapsed);
printf("Скорость обработки: %.2f млн отсчетов/сек\n", samples_per_sec / 1e6);

// Демодуляция и расчет BER
int demod_bits_count;
complex_float* constellation;
int delay = filter_delay;

uint8_t* decoded_bits = qpsk_demodulate(filtered, length, params, delay, 
                           &demod_bits_count, &constellation);

if (decoded_bits) {
int compare_length = (num_bits < demod_bits_count) ? num_bits : demod_bits_count;
float ber = calculate_ber(original_bits, decoded_bits, compare_length);
printf("BER: %.6f (ошибок: %d из %d бит)\n", ber, (int)(ber * compare_length), compare_length);
free(decoded_bits);
free(constellation);
} else {
printf("Ошибка демодуляции\n");
}

// Освобождение ресурсов фильтров
if (strcmp(name, "FIR") == 0) {
fir_filter_free(&fir_i);
fir_filter_free(&fir_q);
} else if (strcmp(name, "IIR") == 0) {
iir_filter_free(&iir_i);
iir_filter_free(&iir_q);
} else if (strcmp(name, "LMS") == 0) {
lms_filter_free(&lms_i);
lms_filter_free(&lms_q);
} else if (strcmp(name, "RLS") == 0) {
rls_filter_free(&rls_i);
rls_filter_free(&rls_q);
}

free(filtered);
}
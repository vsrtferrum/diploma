import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
from scipy.signal import firwin, lfilter, butter, iirfilter, tf2zpk
from tqdm import tqdm
import os

# ================== Параметры системы ==================
fs = 5e9           # Частота дискретизации 5 ГГц
Ts = 1/fs          
num_bits = 200     # Количество битов
samples_per_symbol = 100  # Отсчетов на символ
symbols_count = num_bits // 2  # Количество символов QPSK
total_samples = symbols_count * samples_per_symbol  # Общее количество отсчетов
T = total_samples * Ts  # Общая длительность сигнала

t = np.arange(0, T, Ts)
f_center = 2140e6  # Центральная частота 2.14 ГГц
f_low = 2110e6     # Нижняя граница полосы
f_high = 2170e6    # Верхняя граница полосы
np.random.seed(42) # Для воспроизводимости

# ================== Функция проверки устойчивости IIR фильтра ==================
def check_iir_stability(b, a):
    """Проверка устойчивости IIR фильтра по полюсам"""
    # Находим полюса передаточной функции
    zeros, poles, gain = tf2zpk(b, a)  # Исправлено: получаем три значения
    
    # Проверяем, что все полюса внутри единичной окружности
    stable = True
    for pole in poles:
        if np.abs(pole) >= 1.0:
            stable = False
            break
    
    # Вычисляем максимальный модуль полюса
    max_pole_magnitude = np.max(np.abs(poles))
    
    return stable, max_pole_magnitude, poles

# ================== Улучшенная QPSK модуляция ==================
def qpsk_modulate(bits, samples_per_sym):
    """Генерация QPSK сигнала с Gray-кодированием"""
    # Разбиваем биты на символы (2 бита на символ)
    symbols = np.zeros(len(bits)//2, dtype=complex)
    for i in range(0, len(bits), 2):
        dibit = (bits[i] << 1) | bits[i+1]
        # QPSK созвездие с Gray coding
        if dibit == 0:   # 00
            symbols[i//2] = (1 + 1j) / np.sqrt(2)
        elif dibit == 1: # 01
            symbols[i//2] = (-1 + 1j) / np.sqrt(2)
        elif dibit == 2: # 10
            symbols[i//2] = (1 - 1j) / np.sqrt(2)
        else:            # 11
            symbols[i//2] = (-1 - 1j) / np.sqrt(2)
    
    # Формируем импульсы с прямоугольным окном
    tx_baseband = np.zeros(len(symbols) * samples_per_sym, dtype=complex)
    for i, sym in enumerate(symbols):
        start = i * samples_per_sym
        end = (i+1) * samples_per_sym
        tx_baseband[start:end] = sym
    
    # Перенос на несущую частоту
    carrier = np.exp(1j*2*np.pi*f_center*t[:len(tx_baseband)])
    tx_signal = tx_baseband * carrier
    return tx_signal, symbols

# Генерация битовых данных
binary_data = np.random.randint(0, 2, num_bits)
tx_signal, tx_symbols = qpsk_modulate(binary_data, samples_per_symbol)

# ================== Модель канала ==================
print("\n" + "="*50)
print("Создание модели канала")
print("="*50)

channel = 0.9 * signal.unit_impulse(len(t)) + 0.1 * signal.unit_impulse(len(t), int(0.1e-6/Ts))
interference = 0.3 * np.exp(1j*2*np.pi*2150e6*t[:len(tx_signal)])
noise = 0.1 * (np.random.randn(len(tx_signal)) + 1j*np.random.randn(len(tx_signal)))
rx_signal = lfilter(channel, 1, tx_signal) + interference + noise

print(f"Размерность сигналов:")
print(f"  tx_signal: {tx_signal.shape}")
print(f"  rx_signal: {rx_signal.shape}")

# ================== Фильтрация ==================
print("\n" + "="*50)
print("Фильтрация сигнала")
print("="*50)

# 1. FIR фильтр (всегда устойчив)
print("\n[FIR Фильтр]")
numtaps = 501
fir_coeff = firwin(numtaps, [f_low, f_high], 
                  window=('kaiser', 8), 
                  fs=fs, 
                  pass_zero='bandpass')
fir_filtered = lfilter(fir_coeff, 1.0, rx_signal)
print(f"  Порядок фильтра: {numtaps}")
print(f"  Задержка: {numtaps//2} отсчетов")

# 2. IIR фильтр (требует проверки устойчивости)
print("\n[IIR Фильтр]")
order = 4
b, a = butter(order, [f_low, f_high], 
           btype='bandpass', 
           fs=fs, 
           output='ba')

# Проверка устойчивости IIR фильтра
iir_stable, max_pole, poles = check_iir_stability(b, a)
print(f"  Стабильность: {'УСТОЙЧИВ' if iir_stable else 'НЕУСТОЙЧИВ'}")
print(f"  Максимальный модуль полюса: {max_pole:.6f}")
print(f"  Полюса: {poles}")

if not iir_stable:
    print("  Внимание: IIR фильтр неустойчив! Применяю стабилизацию...")
    # Простая стабилизация - масштабирование коэффициентов
    b = b / (max_pole + 0.1)  # Добавляем запас 0.1
    # Повторная проверка
    iir_stable, max_pole, poles = check_iir_stability(b, a)
    print(f"  После стабилизации:")
    print(f"    Стабильность: {'УСТОЙЧИВ' if iir_stable else 'НЕУСТОЙЧИВ'}")
    print(f"    Максимальный модуль полюса: {max_pole:.6f}")

iir_filtered = lfilter(b, a, rx_signal)
print(f"  Порядок фильтра: {order}")
print(f"  Задержка: {order*10} отсчетов")

# 3. LMS адаптивный фильтр
print("\n[LMS Фильтр]")
mu = 0.005
ntaps = 64
lms_weights = np.zeros(ntaps, dtype=complex)
lms_filtered = np.zeros_like(rx_signal)

# Проверка устойчивости LMS
signal_power = np.var(rx_signal)
stability_bound = 1.0 / (ntaps * signal_power)
lms_stable = mu < stability_bound

print(f"  Размер шага (mu): {mu}")
print(f"  Порядок фильтра: {ntaps}")
print(f"  Мощность сигнала: {signal_power:.4f}")
print(f"  Граница устойчивости: {stability_bound:.6f}")
print(f"  Стабильность: {'УСТОЙЧИВ' if lms_stable else 'ПОТЕНЦИАЛЬНО НЕУСТОЙЧИВ'}")

if not lms_stable:
    print("  Внимание: Параметр mu слишком большой для устойчивой работы!")

print("  Запуск адаптации LMS...")
for n in tqdm(range(ntaps, len(rx_signal)), desc='LMS Filtering'):
    x = rx_signal[n-ntaps:n][::-1]
    y = np.dot(lms_weights.conj(), x)
    e = tx_signal[n] - y
    lms_weights += mu * e.conj() * x
    lms_filtered[n] = y

# 4. RLS адаптивный фильтр
print("\n[RLS Фильтр]")
lam = 0.999
delta = 0.01
rls_weights = np.zeros(ntaps, dtype=complex)
P = np.eye(ntaps)/delta
rls_filtered = np.zeros_like(rx_signal)

# Проверка устойчивости RLS
rls_stable = (lam > 0) and (lam <= 1.0)
print(f"  Коэффициент забывания (lambda): {lam}")
print(f"  Порядок фильтра: {ntaps}")
print(f"  Стабильность: {'УСТОЙЧИВ' if rls_stable else 'НЕУСТОЙЧИВ - lambda вне диапазона (0,1]'}")

if not rls_stable:
    print("  Внимание: Некорректное значение lambda!")

print("  Запуск адаптации RLS...")
for n in tqdm(range(ntaps, len(rx_signal)), desc='RLS Filtering'):
    x = rx_signal[n-ntaps:n][::-1]
    y = np.dot(rls_weights.conj(), x)
    e = tx_signal[n] - y
    k = np.dot(P, x)/(lam + np.dot(x.conj(), np.dot(P, x)))
    rls_weights += k * e.conj()
    P = (P - np.outer(k, np.dot(x.conj(), P)))/lam
    rls_filtered[n] = y

# ================== Улучшенная QPSK демодуляция ==================
def qpsk_demodulate(signal, delay, samples_per_sym, num_symbols):
    """Демодуляция QPSK сигнала с синхронизацией и решением по созвездию"""
    # Перенос в базовую полосу
    carrier = np.exp(-1j*2*np.pi*f_center*t[:len(signal)])
    signal_bb = signal * carrier
    
    # Учет задержки фильтра
    processed = signal_bb[delay:]
    
    # Ресинхронизация и выборка символов
    sampled = np.zeros(num_symbols, dtype=complex)
    for i in range(num_symbols):
        # Берем среднее значение в середине символа
        start = i * samples_per_sym + samples_per_sym//4
        end = (i+1) * samples_per_sym - samples_per_sym//4
        if end < len(processed):
            sampled[i] = np.mean(processed[start:end])
        else:
            # Если вышли за границы, прекращаем обработку
            sampled = sampled[:i]
            break
    
    # Принятие решения по созвездию
    angles = np.angle(sampled)
    decoded_symbols = np.zeros(len(sampled), dtype=int)
    
    # Решающее устройство с Gray decoding
    for i, angle in enumerate(angles):
        if -np.pi/4 <= angle < np.pi/4:
            decoded_symbols[i] = 0  # 00
        elif np.pi/4 <= angle < 3*np.pi/4:
            decoded_symbols[i] = 1  # 01
        elif -3*np.pi/4 <= angle < -np.pi/4:
            decoded_symbols[i] = 2  # 10
        else:
            decoded_symbols[i] = 3  # 11
    
    # Преобразование символов в биты
    decoded_bits = np.zeros(2*len(decoded_symbols), dtype=int)
    for i, sym in enumerate(decoded_symbols):
        decoded_bits[2*i] = (sym >> 1) & 1
        decoded_bits[2*i+1] = sym & 1
    
    return decoded_bits[:min(2*len(decoded_symbols), num_bits)], sampled

# Задержки для разных методов фильтрации
delays = {
    'Original': 0,
    'FIR': numtaps//2,
    'IIR': order*10,  # Увеличена задержка для IIR
    'LMS': ntaps//2,
    'RLS': ntaps//2
}

# Демодуляция для всех типов сигналов
decoded = {}
constellations = {}
print("\n" + "="*50)
print("Демодуляция сигналов")
print("="*50)

for name, sig in [('Original', tx_signal),
                 ('FIR', fir_filtered),
                 ('IIR', iir_filtered),
                 ('LMS', lms_filtered),
                 ('RLS', rls_filtered)]:
    print(f"  Демодуляция: {name}...")
    decoded[name], constellations[name] = qpsk_demodulate(
        sig, delays[name], samples_per_symbol, len(tx_symbols))
    # Расчет BER
    error_count = np.sum(decoded[name] != binary_data[:len(decoded[name])])
    ber = error_count / len(decoded[name])
    print(f"    BER: {ber:.6f} ({error_count} ошибок из {len(decoded[name])} битов)")

# ================== Визуализация ==================
print("\n" + "="*50)
print("Визуализация результатов")
print("="*50)

# 1. Спектры сигналов
plt.figure(figsize=(15, 10))

f = np.fft.fftshift(np.fft.fftfreq(len(tx_signal), Ts))/1e6
psd_rx = 20*np.log10(np.abs(np.fft.fftshift(np.fft.fft(rx_signal)) + 1e-10))
psd_fir = 20*np.log10(np.abs(np.fft.fftshift(np.fft.fft(fir_filtered)) + 1e-10))
psd_iir = 20*np.log10(np.abs(np.fft.fftshift(np.fft.fft(iir_filtered)) + 1e-10))
psd_lms = 20*np.log10(np.abs(np.fft.fftshift(np.fft.fft(lms_filtered)) + 1e-10))
psd_rls = 20*np.log10(np.abs(np.fft.fftshift(np.fft.fft(rls_filtered)) + 1e-10))

plt.plot(f, psd_rx, label='Принятый (с помехами)', alpha=0.5)
plt.plot(f, psd_fir, label='FIR', linewidth=1.5)
plt.plot(f, psd_iir, label='IIR', linewidth=1.5)
plt.plot(f, psd_lms, label='LMS', linewidth=1.5)
plt.plot(f, psd_rls, label='RLS', linewidth=1.5)
plt.xlim(1500, 2400)
plt.ylim(0, 80)
plt.title('Спектральная плотность мощности')
plt.xlabel('Частота (МГц)')
plt.ylabel('Мощность (дБ)')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig('spectrum_comparison.png')
plt.show()

# 2. Сравнение битовых последовательностей
plt.figure(figsize=(15, 8))

plt.subplot(2,1,1)
plt.step(range(50), binary_data[:50], label='Оригинал', where='post', linewidth=2)
plt.title('Оригинальная битовая последовательность')
plt.xlabel('Битовый индекс')
plt.ylabel('Значение бита')
plt.ylim(-0.1, 1.1)
plt.legend()
plt.grid(True)

plt.subplot(2,1,2)
plt.step(range(50), decoded['FIR'][:50], label='FIR', where='post', alpha=0.8)
plt.step(range(50), decoded['IIR'][:50], label='IIR', where='post', alpha=0.8)
plt.step(range(50), decoded['LMS'][:50], label='LMS', where='post', alpha=0.8)
plt.step(range(50), decoded['RLS'][:50], label='RLS', where='post', alpha=0.8)
plt.title('Демодулированные битовые последовательности')
plt.xlabel('Битовый индекс')
plt.ylabel('Значение бита')
plt.ylim(-0.1, 1.1)
plt.legend()
plt.grid(True)

plt.tight_layout()
plt.savefig('bit_comparison.png')
plt.show()

# 3. Показатели качества (BER)
plt.figure(figsize=(10, 6))
ber_values = []
names = ['FIR', 'IIR', 'LMS', 'RLS']

for name in names:
    error_count = np.sum(decoded[name] != binary_data[:len(decoded[name])])
    ber_values.append(error_count / len(decoded[name]))

plt.bar(names, ber_values, alpha=0.6, color=['blue', 'orange', 'green', 'red'])
plt.yscale('log')
plt.title('Сравнение коэффициента ошибок по битам (BER)')
plt.ylabel('BER (логарифмическая шкала)')
plt.xlabel('Тип фильтра')
plt.grid(True, which="both", ls="-")

# Добавляем текстовые значения BER
for i, v in enumerate(ber_values):
    plt.text(i, v, f"{v:.4f}", ha='center', va='bottom')

plt.tight_layout()
plt.savefig('ber_comparison.png')
plt.show()

# 4. Созвездия сигналов
plt.figure(figsize=(15, 10))
for i, name in enumerate(['Original', 'FIR', 'IIR', 'LMS', 'RLS'], 1):
    plt.subplot(2, 3, i)
    if name in constellations:
        plt.scatter(constellations[name].real, constellations[name].imag, 
                   alpha=0.6, s=20, label=name)
        plt.title(f'{name} Созвездие')
        plt.xlabel('In-phase (I)')
        plt.ylabel('Quadrature (Q)')
        plt.xlim(-1.5, 1.5)
        plt.ylim(-1.5, 1.5)
        plt.grid(True)
        plt.axhline(0, color='black', lw=0.5)
        plt.axvline(0, color='black', lw=0.5)
        plt.gca().set_aspect('equal', adjustable='box')
        plt.legend()

plt.tight_layout()
plt.savefig('constellations.png')
plt.show()

# 5. Импульсные характеристики фильтров
plt.figure(figsize=(15, 10))

# FIR
plt.subplot(2, 2, 1)
plt.stem(fir_coeff, linefmt='b-', markerfmt='bo', basefmt=' ')
plt.title('Импульсная характеристика FIR фильтра')
plt.xlabel('Отсчеты')
plt.ylabel('Амплитуда')
plt.xlim(0, len(fir_coeff))
plt.grid(True)

# IIR
plt.subplot(2, 2, 2)
impulse = np.zeros(100)
impulse[0] = 1
iir_response = lfilter(b, a, impulse)
plt.stem(iir_response, linefmt='g-', markerfmt='go', basefmt=' ')
plt.title('Импульсная характеристика IIR фильтра')
plt.xlabel('Отсчеты')
plt.ylabel('Амплитуда')
plt.xlim(0, len(iir_response))
plt.grid(True)

# LMS
plt.subplot(2, 2, 3)
plt.stem(lms_weights.real, linefmt='b-', markerfmt='bo', basefmt=' ', label='Real')
plt.stem(lms_weights.imag, linefmt='r-', markerfmt='ro', basefmt=' ', label='Imag')
plt.title('Коэффициенты LMS фильтра')
plt.xlabel('Отсчеты')
plt.ylabel('Амплитуда')
plt.xlim(0, len(lms_weights))
plt.legend()
plt.grid(True)

# RLS
plt.subplot(2, 2, 4)
plt.stem(rls_weights.real, linefmt='b-', markerfmt='bo', basefmt=' ', label='Real')
plt.stem(rls_weights.imag, linefmt='r-', markerfmt='ro', basefmt=' ', label='Imag')
plt.title('Коэффициенты RLS фильтра')
plt.xlabel('Отсчеты')
plt.ylabel('Амплитуда')
plt.xlim(0, len(rls_weights))
plt.legend()
plt.grid(True)

plt.tight_layout()
plt.savefig('impulse_responses.png')
plt.show()

# 6. Диаграмма полюсов-нулей IIR фильтра
def plot_pole_zero(b, a, title):
    """Визуализация полюсов и нулей"""
    zeros, poles, _ = tf2zpk(b, a)  
    
    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)
    
    # Единичная окружность
    unit_circle = plt.Circle((0, 0), radius=1, fill=False, color='green', 
                            linestyle='--', alpha=0.7)
    ax.add_patch(unit_circle)
    
    # Полюса
    plt.plot(poles.real, poles.imag, 'rx', markersize=10, label='Полюса')
    
    # Нули
    plt.plot(zeros.real, zeros.imag, 'bo', markersize=10, label='Нули')
    
    plt.axvline(0, color='0.7')
    plt.axhline(0, color='0.7')
    plt.grid(True)
    plt.axis('equal')
    plt.title(f'{title}\nСтабильность: {"Устойчив" if iir_stable else "Неустойчив"}')
    plt.xlabel('Действительная ось')
    plt.ylabel('Мнимая ось')
    plt.legend()
    
    # Добавляем информацию о полюсах
    for i, pole in enumerate(poles):
        plt.text(pole.real, pole.imag, f"  {i+1}: {abs(pole):.3f}", 
                fontsize=9, verticalalignment='bottom')
    
    plt.tight_layout()
    plt.savefig('pole_zero_plot.png')
    plt.show()

plot_pole_zero(b, a, 'Диаграмма полюсов-нулей IIR фильтра')

# ================== Сохранение коэффициентов в заголовочный файл C ==================
with open('coeffs.h', 'w') as f:
    # Заголовочная часть файла
    f.write("/**\n")
    f.write(" * @file coeffs.h\n")
    f.write(" * @brief Коэффициенты фильтров для проекта ЦОС\n")
    f.write(" * @warning Автоматически сгенерированный файл - не изменять вручную!\n")
    f.write(" * @date " + np.datetime64('today').astype(str) + "\n")
    f.write(" */\n\n")
    f.write("#ifndef COEFFS_H\n")
    f.write("#define COEFFS_H\n\n")
    f.write("#include <stdint.h>\n\n")
    
    # Определение структуры для комплексных чисел
    f.write("// Структура для хранения комплексных чисел\n")
    f.write("typedef struct {\n")
    f.write("    float real;\n")
    f.write("    float imag;\n")
    f.write("} complex_float;\n\n")
    
    # Размеры фильтров
    f.write(f"#define FIR_NUMTAPS {numtaps}\n")
    f.write(f"#define IIR_ORDER {order*2}\n")
    f.write(f"#define LMS_NTAPS {ntaps}\n")
    f.write(f"#define RLS_NTAPS {ntaps}\n\n")
    
    # FIR коэффициенты
    f.write("// FIR filter coefficients\n")
    f.write("const float fir_coeff[FIR_NUMTAPS] = {\n")
    for i in range(numtaps):
        f.write(f"    {fir_coeff[i]:.8f}f")
        if i < numtaps - 1:
            f.write(",")
        if (i + 1) % 5 == 0:  # 5 значений на строку
            f.write("\n")
    f.write("\n};\n\n")
    
    # IIR коэффициенты (b)
    f.write("// IIR filter numerator coefficients (b)\n")
    f.write("const float iir_b[IIR_ORDER + 1] = {\n")
    for i in range(len(b)):
        f.write(f"    {b[i]:.8f}f")
        if i < len(b) - 1:
            f.write(",")
        if (i + 1) % 5 == 0:  # 5 значений на строку
            f.write("\n")
    f.write("\n};\n\n")
    
    # IIR коэффициенты (a)
    f.write("// IIR filter denominator coefficients (a)\n")
    f.write("const float iir_a[IIR_ORDER + 1] = {\n")
    for i in range(len(a)):
        f.write(f"    {a[i]:.8f}f")
        if i < len(a) - 1:
            f.write(",")
        if (i + 1) % 5 == 0:  # 5 значений на строку
            f.write("\n")
    f.write("\n};\n\n")
    
    # LMS комплексные коэффициенты
    f.write("// LMS filter complex coefficients\n")
    f.write("const complex_float lms_weights[LMS_NTAPS] = {\n")
    for i in range(ntaps):
        real = lms_weights[i].real
        imag = lms_weights[i].imag
        f.write(f"    {{{real:.8f}f, {imag:.8f}f}}")
        if i < ntaps - 1:
            f.write(",")
        if (i + 1) % 2 == 0:  # 2 комплексных значения на строку
            f.write("\n")
    f.write("\n};\n\n")
    
    # RLS комплексные коэффициенты
    f.write("// RLS filter complex coefficients\n")
    f.write("const complex_float rls_weights[RLS_NTAPS] = {\n")
    for i in range(ntaps):
        real = rls_weights[i].real
        imag = rls_weights[i].imag
        f.write(f"    {{{real:.8f}f, {imag:.8f}f}}")
        if i < ntaps - 1:
            f.write(",")
        if (i + 1) % 2 == 0:  # 2 комплексных значения на строку
            f.write("\n")
    f.write("\n};\n\n")
    
    # Завершение файла
    f.write("#endif // COEFFS_H\n")

print("\n" + "="*50)
print("Коэффициенты фильтров успешно сохранены в coeffs.h")
print("="*50)

print("\nСкрипт успешно завершен!")
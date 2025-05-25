import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
from scipy.signal import firwin, lfilter, butter, iirfilter
from tqdm import tqdm
import os

# ================== Параметры системы ==================
fs = 5e9           # Частота дискретизации 5 ГГц
Ts = 1/fs          
T = 1e-6           # Длительность сигнала 1 мкс
t = np.arange(0, T, Ts)
f_center = 2140e6  # Центральная частота 2.14 ГГц
f_low = 2110e6     # Нижняя граница полосы
f_high = 2170e6    # Верхняя граница полосы
num_bits = 200     # Количество битов
np.random.seed(42) # Для воспроизводимости

# ================== Генерация данных ==================
binary_data = np.random.randint(0, 2, num_bits)
symbols = (binary_data[::2] << 1) + binary_data[1::2]
modulated = np.exp(1j * (np.pi/2 * symbols))
t_symbol = np.linspace(0, T, len(modulated))

repeat_factor = int(np.ceil(len(t)/len(modulated)))
tx_baseband = np.tile(modulated, repeat_factor)[:len(t)]
tx_signal = tx_baseband * np.exp(1j*2*np.pi*f_center*t)

# ================== Модель канала ==================
channel = 0.9 * signal.unit_impulse(len(t)) + 0.1 * signal.unit_impulse(len(t), int(0.1e-6/Ts))
interference = 0.3 * np.exp(1j*2*np.pi*2150e6*t)
noise = 0.1 * (np.random.randn(len(t)) + 1j*np.random.randn(len(t)))
rx_signal = lfilter(channel, 1, tx_signal) + interference + noise

# ================== Фильтрация ==================
# 1. FIR фильтр
numtaps = 501
fir_coeff = firwin(numtaps, [f_low, f_high], 
                  window=('kaiser', 8), 
                  fs=fs, 
                  pass_zero='bandpass')
fir_filtered = lfilter(fir_coeff, 1.0, rx_signal)

# 2. IIR фильтр в стандартной форме (b, a)
order = 4  # Уменьшен порядок для устойчивости
b, a = butter(order, [f_low, f_high], 
           btype='bandpass', 
           fs=fs, 
           output='ba')
iir_filtered = lfilter(b, a, rx_signal)

# 3. LMS адаптивный фильтр
mu = 0.005
ntaps = 64
lms_weights = np.zeros(ntaps, dtype=complex)
lms_filtered = np.zeros_like(rx_signal)
for n in tqdm(range(ntaps, len(rx_signal)), desc='LMS Filtering'):
    x = rx_signal[n-ntaps:n][::-1]
    y = np.dot(lms_weights.conj(), x)
    e = tx_signal[n] - y
    lms_weights += mu * e.conj() * x
    lms_filtered[n] = y

# 4. RLS адаптивный фильтр
lam = 0.999
delta = 0.01
rls_weights = np.zeros(ntaps, dtype=complex)
P = np.eye(ntaps)/delta
rls_filtered = np.zeros_like(rx_signal)
for n in tqdm(range(ntaps, len(rx_signal)), desc='RLS Filtering'):
    x = rx_signal[n-ntaps:n][::-1]
    y = np.dot(rls_weights.conj(), x)
    e = tx_signal[n] - y
    k = np.dot(P, x)/(lam + np.dot(x.conj(), np.dot(P, x)))
    rls_weights += k * e.conj()
    P = (P - np.outer(k, np.dot(x.conj(), P)))/lam
    rls_filtered[n] = y

# ================== Демодуляция ==================
def demodulate(signal, delay):
    signal_bb = signal * np.exp(-1j*2*np.pi*f_center*t)
    processed = signal_bb[delay:]
    actual_repeat = len(processed) // len(modulated)
    processed = processed[:len(modulated)*actual_repeat]
    symbols_received = processed.reshape(-1, actual_repeat).mean(axis=1)
    angles = np.angle(symbols_received)
    decoded_symbols = np.round(angles/(np.pi/2)).astype(int) % 4
    decoded_bits = np.zeros(2*len(decoded_symbols), dtype=int)
    decoded_bits[::2] = (decoded_symbols >> 1) & 1
    decoded_bits[1::2] = decoded_symbols & 1
    return decoded_bits[:num_bits]

delays = {
    'Original': 0,
    'FIR': numtaps//2,
    'IIR': order*2,  
    'LMS': ntaps//2,
    'RLS': ntaps//2
}

decoded = {}
for name, sig in [('Original', tx_signal),
                 ('FIR', fir_filtered),
                 ('IIR', iir_filtered),
                 ('LMS', lms_filtered),
                 ('RLS', rls_filtered)]:
    decoded[name] = demodulate(sig, delays[name])

# ================== Визуализация ==================
# ... (остальная часть визуализации без изменений)

# ================== Вывод коэффициентов ==================
with open('coeffs.txt', 'w') as f:
    # FIR коэффициенты
    f.write("FIR Coefficients:\n")
    np.savetxt(f, fir_coeff, fmt='%.8f', delimiter=',\n')
    
    # IIR коэффициенты в формате b, a
    f.write("\n\nIIR Numerator (b):\n")
    np.savetxt(f, b, fmt='%.8f', delimiter=',\n')
    f.write("\nIIR Denominator (a):\n")
    np.savetxt(f, a, fmt='%.8f', delimiter=',\n')
    
    # LMS веса
    f.write("\n\nLMS Weights:\n")
    np.savetxt(f, lms_weights.view(float).reshape(-1, 2), 
              fmt='%.8f, %.8fj', 
              delimiter=',\n')
    
    # RLS веса
    f.write("\n\nRLS Weights:\n")
    np.savetxt(f, rls_weights.view(float).reshape(-1, 2), 
              fmt='%.8f, %.8fj', 
              delimiter=',\n')
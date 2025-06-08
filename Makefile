CC = gcc
CFLAGS = -O3 -Wall -Wextra -I. -Ifilters -Iqpsk -Isignal_generator
LDFLAGS = -lm

# Директории
SRC_DIR = .
FILTERS_DIR = filters
QPSK_DIR = qpsk
SIGNAL_DIR = signal_generator
OBJ_DIR = obj
BENCHMARK_DIR = benchmark

# Исходные файлы
FILTERS_SRC = $(wildcard $(FILTERS_DIR)/*.c)
QPSK_SRC = $(wildcard $(QPSK_DIR)/*.c)
SIGNAL_SRC = $(wildcard $(SIGNAL_DIR)/*.c)
BENCHMARK_SRC = $(wildcard $(BENCHMARK_DIR)/*.c)

SRC = $(FILTERS_SRC) $(QPSK_SRC) $(SIGNAL_SRC) $(BENCHMARK_SRC)
OBJ = $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(SRC)))

# Исполняемый файл (изменено имя, чтобы избежать конфликта)
TARGET = dsp_benchmark

.PHONY: all clean run plot

all: $(TARGET)

# Сборка исполняемого файла
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Компиляция объектных файлов
$(OBJ_DIR)/%.o: $(FILTERS_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(QPSK_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SIGNAL_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(BENCHMARK_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Создание директории для объектных файлов
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Генерация коэффициентов и графиков
plot:
	python3 filters_calculation.py

# Запуск тестов
run: $(TARGET)
	./$(TARGET)

# Очистка
clean:
	rm -rf $(OBJ_DIR) $(TARGET) \
	ber_comparison.png bit_comparison.png constellations.png \
	impulse_responses.png pole_zero_plot.png spectrum_comparison.png \
	coeffs.h
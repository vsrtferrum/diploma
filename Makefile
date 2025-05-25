CC = gcc
CFLAGS = -Wall -Wextra -I. -Iheaders -Iheaders/filters -Iheaders/infra
BENCHMARK_DIR = benchmark
HEADERS_DIR = headers

.PHONY: all clean benchmark

all: benchmark

benchmark: $(BENCHMARK_DIR)/benchmark.c $(BENCHMARK_DIR)/test_signals.c $(BENCHMARK_DIR)/performance_tests.c \
          $(HEADERS_DIR)/filters/fir_filter.c $(HEADERS_DIR)/filters/iir_filter.c \
          $(HEADERS_DIR)/filters/lms_filter.c $(HEADERS_DIR)/filters/rls_filter.c 
	cp coeffs.txt $(BENCHMARK_DIR)/
	$(CC) $(BENCHMARK_DIR)/benchmark.c $(BENCHMARK_DIR)/test_signals.c $(BENCHMARK_DIR)/performance_tests.c \
		$(HEADERS_DIR)/filters/fir_filter.c $(HEADERS_DIR)/filters/iir_filter.c \
		$(HEADERS_DIR)/filters/lms_filter.c $(HEADERS_DIR)/filters/rls_filter.c \
		-o $(BENCHMARK_DIR)/benchmark $(CFLAGS) -lm

run_benchmark: benchmark
	./$(BENCHMARK_DIR)/benchmark

clean:
	rm -f $(BENCHMARK_DIR)/benchmark
	rm -f $(BENCHMARK_DIR)/coeffs.txt

calc_coeffs:
	python3 filters_calculation.py



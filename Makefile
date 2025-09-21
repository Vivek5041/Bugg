# Makefile for Buggy Lock Testing Suite
# This directory contains intentionally buggy code for testing lock shielding mechanisms

# Compiler settings
CC = gcc
CFLAGS = -Wall -O2 -std=c11 -D_GNU_SOURCE
PTHREAD_FLAGS = -lpthread

# Source file
SOURCE = cpu_hiera_bugged.c

# Default target - build all bug variants
.PHONY: all clean help test-all

all: cpu_hiera_bugged cpu_hiera_bugged_ul cpu_hiera_bugged_uu cpu_hiera_bugged_both

# Help target
help:
	@echo "Buggy Lock Testing Suite"
	@echo "========================"
	@echo ""
	@echo "Available targets:"
	@echo "  all                    - Build all bug variants"
	@echo "  cpu_hiera_bugged       - Build normal version (no bug injection)"
	@echo "  cpu_hiera_bugged_ul    - Build with unlock bug injection (INJECT_UL_BUG)"
	@echo "  cpu_hiera_bugged_uu    - Build with lock bug injection (INJECT_UU_BUG)"
	@echo "  cpu_hiera_bugged_both  - Build with both bug types enabled"
	@echo "  test-all              - Run basic tests on all variants"
	@echo "  clean                 - Remove all built executables"
	@echo "  help                  - Show this help message"
	@echo ""
	@echo "Bug Types:"
	@echo "  INJECT_UL_BUG  - Randomly skip unlock operations (deadlock risk)"
	@echo "  INJECT_UU_BUG  - Randomly skip lock operations (unbalanced locks)"
	@echo ""
	@echo "Usage examples:"
	@echo "  ./cpu_hiera_bugged <threads> <nesting> <warmup> <iterations> [bug_prob]"
	@echo "  ./cpu_hiera_bugged_ul 2 5 1000 10000 0.001"
	@echo "  ./cpu_hiera_bugged_uu 4 3 500 5000 0.0001"

# Normal version (no bugs injected at compile time - bugs controlled by runtime probability)
cpu_hiera_bugged: $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $< $(PTHREAD_FLAGS)

# Version with unlock bug injection enabled
cpu_hiera_bugged_ul: $(SOURCE)
	$(CC) $(CFLAGS) -DINJECT_UL_BUG -o $@ $< $(PTHREAD_FLAGS)

# Version with lock bug injection enabled  
cpu_hiera_bugged_uu: $(SOURCE)
	$(CC) $(CFLAGS) -DINJECT_UU_BUG -o $@ $< $(PTHREAD_FLAGS)

# Version with both bug types enabled
cpu_hiera_bugged_both: $(SOURCE)
	$(CC) $(CFLAGS) -DINJECT_UL_BUG -DINJECT_UU_BUG -o $@ $< $(PTHREAD_FLAGS)

# Test all variants with safe parameters
test-all: all
	@echo "Testing all bug variants with safe parameters..."
	@echo ""
	@echo "1. Testing normal version (no bug injection):"
	@./cpu_hiera_bugged 2 2 100 1000 0.0
	@echo ""
	@echo "2. Testing unlock bug version (very low probability):"
	@timeout 10s ./cpu_hiera_bugged_ul 2 2 100 1000 0.0001 || echo "Test completed or timed out"
	@echo ""
	@echo "3. Testing lock bug version (very low probability):"
	@timeout 10s ./cpu_hiera_bugged_uu 2 2 100 1000 0.0001 || echo "Test completed or timed out"
	@echo ""
	@echo "4. Testing both bugs version (very low probability):"
	@timeout 10s ./cpu_hiera_bugged_both 2 2 100 1000 0.00005 || echo "Test completed or timed out"
	@echo ""
	@echo "All tests completed!"

# Quick test with very safe parameters (no bugs triggered)
test-safe: all
	@echo "Running safe tests (no bug injection)..."
	@./cpu_hiera_bugged 2 3 100 1000 0.0
	@./cpu_hiera_bugged_ul 2 3 100 1000 0.0  
	@./cpu_hiera_bugged_uu 2 3 100 1000 0.0
	@./cpu_hiera_bugged_both 2 3 100 1000 0.0
	@echo "Safe tests completed successfully!"

# Dangerous test - use with caution! May cause deadlock
test-dangerous:
	@echo "WARNING: Running dangerous tests with higher bug probability!"
	@echo "These may cause deadlocks - use Ctrl+C if needed"
	@echo ""
	@echo "Testing with moderate bug probability..."
	@timeout 5s ./cpu_hiera_bugged_ul 2 2 10 100 0.01 || echo "Test timed out (expected)"
	@timeout 5s ./cpu_hiera_bugged_uu 2 2 10 100 0.01 || echo "Test timed out (expected)"

# Development target - build with debug symbols
debug: CFLAGS += -g -DDEBUG
debug: all

# Clean target
clean:
	@echo "Cleaning built files..."
	@rm -f cpu_hiera_bugged cpu_hiera_bugged_ul cpu_hiera_bugged_uu cpu_hiera_bugged_both
	@echo "Clean complete."

# Show compilation commands for debugging
show-config:
	@echo "Compiler Configuration:"
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "PTHREAD_FLAGS = $(PTHREAD_FLAGS)"
	@echo "SOURCE = $(SOURCE)"

.PHONY: test-safe test-dangerous debug show-config
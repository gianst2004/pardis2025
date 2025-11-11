# ==================================================
# Connected Components Project, developed for:
#
# Parralel and Distributed Systems,
# Department of Electrical and Computer Engineering,
# Aristotle University of Thessaloniki.
# ==================================================

# Project Name
PROJECT := connected_components

# Compilers
export CC := gcc
export CLANG := clang

CILK_PATH := /usr/local/opencilk

# Base compiler flags
BASE_CFLAGS := -Wall -Wextra -Wpedantic -std=c11 -O3 -march=native
BASE_CFLAGS += -Isrc/core -Isrc/algorithms -Isrc/utils

# Implementation-specific flags
SEQUENTIAL_CFLAGS := $(BASE_CFLAGS) -DUSE_SEQUENTIAL
OPENMP_CFLAGS := $(BASE_CFLAGS) -fopenmp -DUSE_OPENMP
PTHREADS_CFLAGS := $(BASE_CFLAGS) -pthread -DUSE_PTHREADS
CILK_CFLAGS := $(BASE_CFLAGS) -fopencilk -DUSE_CILK -I$(CILK_PATH)/include

# Linker flags
SEQUENTIAL_LDFLAGS :=
OPENMP_LDFLAGS := -fopenmp
PTHREADS_LDFLAGS := -pthread
CILK_LDFLAGS := -fopencilk -L$(CILK_PATH)/lib

# Common libraries
LDLIBS := -lmatio -lm

# Directories
SRC_DIR   := src
BUILD_DIR := build
BIN_DIR   := bin
OBJ_DIR   := $(BUILD_DIR)/obj
DEP_DIR   := $(BUILD_DIR)/deps

# Source files
CORE_SRCS := $(wildcard $(SRC_DIR)/core/*.c)
UTILS_SRCS := $(wildcard $(SRC_DIR)/utils/*.c)
MAIN_SRC := $(SRC_DIR)/main.c

# Algorithm implementation files
SEQUENTIAL_ALGO := $(SRC_DIR)/algorithms/cc_sequential.c
OPENMP_ALGO := $(SRC_DIR)/algorithms/cc_openmp.c
PTHREADS_ALGO := $(SRC_DIR)/algorithms/cc_pthreads.c
CILK_ALGO := $(SRC_DIR)/algorithms/cc_cilk.c

# Object files for each implementation
SEQUENTIAL_OBJS := $(CORE_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/sequential/%.o) \
                   $(UTILS_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/sequential/%.o) \
                   $(MAIN_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/sequential/%.o) \
                   $(SEQUENTIAL_ALGO:$(SRC_DIR)/%.c=$(OBJ_DIR)/sequential/%.o)

OPENMP_OBJS := $(CORE_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/openmp/%.o) \
               $(UTILS_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/openmp/%.o) \
               $(MAIN_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/openmp/%.o) \
               $(OPENMP_ALGO:$(SRC_DIR)/%.c=$(OBJ_DIR)/openmp/%.o)

PTHREADS_OBJS := $(CORE_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/pthreads/%.o) \
                 $(UTILS_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/pthreads/%.o) \
                 $(MAIN_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/pthreads/%.o) \
                 $(PTHREADS_ALGO:$(SRC_DIR)/%.c=$(OBJ_DIR)/pthreads/%.o)

CILK_OBJS := $(CORE_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/cilk/%.o) \
             $(UTILS_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/cilk/%.o) \
             $(MAIN_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/cilk/%.o) \
             $(CILK_ALGO:$(SRC_DIR)/%.c=$(OBJ_DIR)/cilk/%.o)

# Benchmark runner sources
RUNNER_MAIN_SRC := $(SRC_DIR)/runner.c
RUNNER_UTILS := $(SRC_DIR)/utils/error.c $(SRC_DIR)/utils/args.c

# Runner object files
RUNNER_OBJS := $(RUNNER_MAIN_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/runner/%.o) \
               $(RUNNER_UTILS:$(SRC_DIR)/%.c=$(OBJ_DIR)/runner/%.o)

RUNNER_TARGET := $(BIN_DIR)/benchmark_runner
RUNNER_CFLAGS := $(BASE_CFLAGS)
RUNNER_LDFLAGS :=

# Target executables
SEQUENTIAL_TARGET := $(BIN_DIR)/$(PROJECT)_sequential
OPENMP_TARGET := $(BIN_DIR)/$(PROJECT)_openmp
PTHREADS_TARGET := $(BIN_DIR)/$(PROJECT)_pthreads
CILK_TARGET := $(BIN_DIR)/$(PROJECT)_cilk

ALL_TARGETS := $(SEQUENTIAL_TARGET) $(OPENMP_TARGET) $(PTHREADS_TARGET) $(CILK_TARGET) $(RUNNER_TARGET)

# Pretty Output
ECHO := /bin/echo -e
COLOR_RESET := \033[0m
COLOR_GREEN := \033[1;32m
COLOR_YELLOW := \033[1;33m
COLOR_BLUE := \033[1;34m
COLOR_MAGENTA := \033[1;35m
COLOR_CYAN := \033[1;36m

# ============================================
# Directory creation
# ============================================

$(BIN_DIR):
	@mkdir -p $@

$(OBJ_DIR)/sequential $(OBJ_DIR)/sequential/core $(OBJ_DIR)/sequential/algorithms $(OBJ_DIR)/sequential/utils:
	@mkdir -p $@

$(OBJ_DIR)/openmp $(OBJ_DIR)/openmp/core $(OBJ_DIR)/openmp/algorithms $(OBJ_DIR)/openmp/utils:
	@mkdir -p $@

$(OBJ_DIR)/pthreads $(OBJ_DIR)/pthreads/core $(OBJ_DIR)/pthreads/algorithms $(OBJ_DIR)/pthreads/utils:
	@mkdir -p $@

$(OBJ_DIR)/cilk $(OBJ_DIR)/cilk/core $(OBJ_DIR)/cilk/algorithms $(OBJ_DIR)/cilk/utils:
	@mkdir -p $@

$(OBJ_DIR)/runner $(OBJ_DIR)/runner/utils:
	@mkdir -p $@

$(DEP_DIR)/sequential $(DEP_DIR)/sequential/core $(DEP_DIR)/sequential/algorithms $(DEP_DIR)/sequential/utils:
	@mkdir -p $@

$(DEP_DIR)/openmp $(DEP_DIR)/openmp/core $(DEP_DIR)/openmp/algorithms $(DEP_DIR)/openmp/utils:
	@mkdir -p $@

$(DEP_DIR)/pthreads $(DEP_DIR)/pthreads/core $(DEP_DIR)/pthreads/algorithms $(DEP_DIR)/pthreads/utils:
	@mkdir -p $@

$(DEP_DIR)/cilk $(DEP_DIR)/cilk/core $(DEP_DIR)/cilk/algorithms $(DEP_DIR)/cilk/utils:
	@mkdir -p $@

$(DEP_DIR)/runner $(DEP_DIR)/runner/utils:
	@mkdir -p $@

# ============================================
# Main targets
# ============================================

.PHONY: all
all: $(ALL_TARGETS)

.PHONY: sequential
sequential: $(SEQUENTIAL_TARGET)

.PHONY: openmp
openmp: $(OPENMP_TARGET)

.PHONY: pthreads
pthreads: $(PTHREADS_TARGET)

.PHONY: cilk
cilk: $(CILK_TARGET)

.PHONY: runner
runner: $(RUNNER_TARGET)

# ============================================
# Sequential Implementation
# ============================================

$(SEQUENTIAL_TARGET): $(SEQUENTIAL_OBJS) | $(BIN_DIR)
	@$(ECHO) "$(COLOR_GREEN)Linking [sequential]:$(COLOR_RESET) $@"
	@$(CC) $(SEQUENTIAL_LDFLAGS) $(SEQUENTIAL_OBJS) $(LDLIBS) -o $@

$(OBJ_DIR)/sequential/core/%.o: $(SRC_DIR)/core/%.c | $(OBJ_DIR)/sequential/core $(DEP_DIR)/sequential/core
	@$(ECHO) "$(COLOR_BLUE)Compiling [sequential/core]:$(COLOR_RESET) $<"
	@$(CC) $(SEQUENTIAL_CFLAGS) -MMD -MP -MF $(DEP_DIR)/sequential/core/$*.d -c $< -o $@

$(OBJ_DIR)/sequential/algorithms/%.o: $(SRC_DIR)/algorithms/%.c | $(OBJ_DIR)/sequential/algorithms $(DEP_DIR)/sequential/algorithms
	@$(ECHO) "$(COLOR_BLUE)Compiling [sequential/algo]:$(COLOR_RESET) $<"
	@$(CC) $(SEQUENTIAL_CFLAGS) -MMD -MP -MF $(DEP_DIR)/sequential/algorithms/$*.d -c $< -o $@

$(OBJ_DIR)/sequential/utils/%.o: $(SRC_DIR)/utils/%.c | $(OBJ_DIR)/sequential/utils $(DEP_DIR)/sequential/utils
	@$(ECHO) "$(COLOR_BLUE)Compiling [sequential/utils]:$(COLOR_RESET) $<"
	@$(CC) $(SEQUENTIAL_CFLAGS) -MMD -MP -MF $(DEP_DIR)/sequential/utils/$*.d -c $< -o $@

$(OBJ_DIR)/sequential/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)/sequential $(DEP_DIR)/sequential
	@$(ECHO) "$(COLOR_BLUE)Compiling [sequential/main]:$(COLOR_RESET) $<"
	@$(CC) $(SEQUENTIAL_CFLAGS) -MMD -MP -MF $(DEP_DIR)/sequential/$*.d -c $< -o $@

# ============================================
# OpenMP Implementation
# ============================================

$(OPENMP_TARGET): $(OPENMP_OBJS) | $(BIN_DIR)
	@$(ECHO) "$(COLOR_GREEN)Linking [openmp]:$(COLOR_RESET) $@"
	@$(CC) $(OPENMP_LDFLAGS) $(OPENMP_OBJS) $(LDLIBS) -o $@

$(OBJ_DIR)/openmp/core/%.o: $(SRC_DIR)/core/%.c | $(OBJ_DIR)/openmp/core $(DEP_DIR)/openmp/core
	@$(ECHO) "$(COLOR_BLUE)Compiling [openmp/core]:$(COLOR_RESET) $<"
	@$(CC) $(OPENMP_CFLAGS) -MMD -MP -MF $(DEP_DIR)/openmp/core/$*.d -c $< -o $@

$(OBJ_DIR)/openmp/algorithms/%.o: $(SRC_DIR)/algorithms/%.c | $(OBJ_DIR)/openmp/algorithms $(DEP_DIR)/openmp/algorithms
	@$(ECHO) "$(COLOR_BLUE)Compiling [openmp/algo]:$(COLOR_RESET) $<"
	@$(CC) $(OPENMP_CFLAGS) -MMD -MP -MF $(DEP_DIR)/openmp/algorithms/$*.d -c $< -o $@

$(OBJ_DIR)/openmp/utils/%.o: $(SRC_DIR)/utils/%.c | $(OBJ_DIR)/openmp/utils $(DEP_DIR)/openmp/utils
	@$(ECHO) "$(COLOR_BLUE)Compiling [openmp/utils]:$(COLOR_RESET) $<"
	@$(CC) $(OPENMP_CFLAGS) -MMD -MP -MF $(DEP_DIR)/openmp/utils/$*.d -c $< -o $@

$(OBJ_DIR)/openmp/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)/openmp $(DEP_DIR)/openmp
	@$(ECHO) "$(COLOR_BLUE)Compiling [openmp/main]:$(COLOR_RESET) $<"
	@$(CC) $(OPENMP_CFLAGS) -MMD -MP -MF $(DEP_DIR)/openmp/$*.d -c $< -o $@

# ============================================
# Pthreads Implementation
# ============================================

$(PTHREADS_TARGET): $(PTHREADS_OBJS) | $(BIN_DIR)
	@$(ECHO) "$(COLOR_GREEN)Linking [pthreads]:$(COLOR_RESET) $@"
	@$(CC) $(PTHREADS_LDFLAGS) $(PTHREADS_OBJS) $(LDLIBS) -o $@

$(OBJ_DIR)/pthreads/core/%.o: $(SRC_DIR)/core/%.c | $(OBJ_DIR)/pthreads/core $(DEP_DIR)/pthreads/core
	@$(ECHO) "$(COLOR_BLUE)Compiling [pthreads/core]:$(COLOR_RESET) $<"
	@$(CC) $(PTHREADS_CFLAGS) -MMD -MP -MF $(DEP_DIR)/pthreads/core/$*.d -c $< -o $@

$(OBJ_DIR)/pthreads/algorithms/%.o: $(SRC_DIR)/algorithms/%.c | $(OBJ_DIR)/pthreads/algorithms $(DEP_DIR)/pthreads/algorithms
	@$(ECHO) "$(COLOR_BLUE)Compiling [pthreads/algo]:$(COLOR_RESET) $<"
	@$(CC) $(PTHREADS_CFLAGS) -MMD -MP -MF $(DEP_DIR)/pthreads/algorithms/$*.d -c $< -o $@

$(OBJ_DIR)/pthreads/utils/%.o: $(SRC_DIR)/utils/%.c | $(OBJ_DIR)/pthreads/utils $(DEP_DIR)/pthreads/utils
	@$(ECHO) "$(COLOR_BLUE)Compiling [pthreads/utils]:$(COLOR_RESET) $<"
	@$(CC) $(PTHREADS_CFLAGS) -MMD -MP -MF $(DEP_DIR)/pthreads/utils/$*.d -c $< -o $@

$(OBJ_DIR)/pthreads/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)/pthreads $(DEP_DIR)/pthreads
	@$(ECHO) "$(COLOR_BLUE)Compiling [pthreads/main]:$(COLOR_RESET) $<"
	@$(CC) $(PTHREADS_CFLAGS) -MMD -MP -MF $(DEP_DIR)/pthreads/$*.d -c $< -o $@

# ============================================
# Cilk Implementation
# ============================================

$(CILK_TARGET): $(CILK_OBJS) | $(BIN_DIR)
	@$(ECHO) "$(COLOR_GREEN)Linking [cilk]:$(COLOR_RESET) $@"
	@$(CLANG) $(CILK_LDFLAGS) $(CILK_OBJS) $(LDLIBS) -o $@

$(OBJ_DIR)/cilk/core/%.o: $(SRC_DIR)/core/%.c | $(OBJ_DIR)/cilk/core $(DEP_DIR)/cilk/core
	@$(ECHO) "$(COLOR_BLUE)Compiling [cilk/core]:$(COLOR_RESET) $<"
	@$(CLANG) $(CILK_CFLAGS) -MMD -MP -MF $(DEP_DIR)/cilk/core/$*.d -c $< -o $@

$(OBJ_DIR)/cilk/algorithms/%.o: $(SRC_DIR)/algorithms/%.c | $(OBJ_DIR)/cilk/algorithms $(DEP_DIR)/cilk/algorithms
	@$(ECHO) "$(COLOR_BLUE)Compiling [cilk/algo]:$(COLOR_RESET) $<"
	@$(CLANG) $(CILK_CFLAGS) -MMD -MP -MF $(DEP_DIR)/cilk/algorithms/$*.d -c $< -o $@

$(OBJ_DIR)/cilk/utils/%.o: $(SRC_DIR)/utils/%.c | $(OBJ_DIR)/cilk/utils $(DEP_DIR)/cilk/utils
	@$(ECHO) "$(COLOR_BLUE)Compiling [cilk/utils]:$(COLOR_RESET) $<"
	@$(CLANG) $(CILK_CFLAGS) -MMD -MP -MF $(DEP_DIR)/cilk/utils/$*.d -c $< -o $@

$(OBJ_DIR)/cilk/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)/cilk $(DEP_DIR)/cilk
	@$(ECHO) "$(COLOR_BLUE)Compiling [cilk/main]:$(COLOR_RESET) $<"
	@$(CLANG) $(CILK_CFLAGS) -MMD -MP -MF $(DEP_DIR)/cilk/$*.d -c $< -o $@

# ============================================
# Benchmark Runner
# ============================================

$(RUNNER_TARGET): $(RUNNER_OBJS) | $(BIN_DIR)
	@$(ECHO) "$(COLOR_GREEN)Linking [runner]:$(COLOR_RESET) $@"
	@$(CC) $(RUNNER_LDFLAGS) $(RUNNER_OBJS) $(LDLIBS) -o $@

$(OBJ_DIR)/runner/utils/%.o: $(SRC_DIR)/utils/%.c | $(OBJ_DIR)/runner/utils $(DEP_DIR)/runner/utils
	@$(ECHO) "$(COLOR_BLUE)Compiling [runner/utils]:$(COLOR_RESET) $<"
	@$(CC) $(RUNNER_CFLAGS) -MMD -MP -MF $(DEP_DIR)/runner/utils/$*.d -c $< -o $@

$(OBJ_DIR)/runner/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)/runner $(DEP_DIR)/runner
	@$(ECHO) "$(COLOR_BLUE)Compiling [runner]:$(COLOR_RESET) $<"
	@$(CC) $(RUNNER_CFLAGS) -MMD -MP -MF $(DEP_DIR)/runner/$*.d -c $< -o $@

# Include dependency files
-include $(SEQUENTIAL_OBJS:.o=.d)
-include $(OPENMP_OBJS:.o=.d)
-include $(PTHREADS_OBJS:.o=.d)
-include $(CILK_OBJS:.o=.d)
-include $(RUNNER_OBJS:.o=.d)

# ============================================
# Cleaning
# ============================================

.PHONY: clean
clean:
	@$(ECHO) "$(COLOR_YELLOW)Cleaning build artifacts...$(COLOR_RESET)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@$(ECHO) "$(COLOR_GREEN)✓ Clean complete$(COLOR_RESET)"

.PHONY: rebuild
rebuild: clean all

# ============================================
# Project structure
# ============================================

.PHONY: tree
tree:
	@$(ECHO) "$(COLOR_BLUE)Project structure:$(COLOR_RESET)"
	@tree -I 'build|bin' --dirsfirst || \
		($(ECHO) "$(COLOR_YELLOW)tree command not found, using find:$(COLOR_RESET)" && \
		 find . -not -path '*/build/*' -not -path '*/bin/*' -not -path '*/.git/*' | sort)

.PHONY: list-sources
list-sources:
	@$(ECHO) "$(COLOR_BLUE)Source files:$(COLOR_RESET)\n"
	@$(ECHO) "$(COLOR_MAGENTA)Core:$(COLOR_RESET)"
	@for f in $(CORE_SRCS); do [ -n "$$f" ] && echo "  $$f"; done
	@$(ECHO) "$(COLOR_MAGENTA)Utils:$(COLOR_RESET)"
	@for f in $(UTILS_SRCS); do [ -n "$$f" ] && echo "  $$f"; done
	@$(ECHO) "$(COLOR_MAGENTA)Main:$(COLOR_RESET)"
	@echo "  $(MAIN_SRC)"
	@$(ECHO) "$(COLOR_MAGENTA)Algorithms:$(COLOR_RESET)"
	@for f in $(SEQUENTIAL_ALGO) $(OPENMP_ALGO) $(PTHREADS_ALGO) $(CILK_ALGO); do \
		if [ -f "$$f" ]; then echo "  $$f"; else echo "  $$f (missing)"; fi; \
	done
	@$(ECHO) "$(COLOR_MAGENTA)Runner:$(COLOR_RESET)"
	@echo "  $(RUNNER_MAIN_SRC)"
	@for f in $(RUNNER_UTILS); do echo "  $$f"; done

# ============================================
# Information and help
# ============================================

.PHONY: info
info:
	@$(ECHO) "$(COLOR_BLUE)════════════════════════════════════════$(COLOR_RESET)"
	@$(ECHO) "$(COLOR_GREEN)Build Configuration$(COLOR_RESET)"
	@$(ECHO) "$(COLOR_BLUE)════════════════════════════════════════$(COLOR_RESET)"
	@echo "  Project:      $(PROJECT)"
	@echo "  Compilers:    $(CC), $(CLANG)"
	@echo ""
	@$(ECHO) "$(COLOR_BLUE)Compiler Flags:$(COLOR_RESET)"
	@echo "  Base:         $(BASE_CFLAGS)"
	@echo "  Sequential:   $(SEQUENTIAL_CFLAGS)"
	@echo "  OpenMP:       $(OPENMP_CFLAGS)"
	@echo "  Pthreads:     $(PTHREADS_CFLAGS)"
	@echo "  Cilk:         $(CILK_CFLAGS)"
	@echo "  Runner:       $(RUNNER_CFLAGS)"
	@echo ""
	@$(ECHO) "$(COLOR_BLUE)Linker Flags:$(COLOR_RESET)"
	@echo "  Sequential:   $(SEQUENTIAL_LDFLAGS)"
	@echo "  OpenMP:       $(OPENMP_LDFLAGS)"
	@echo "  Pthreads:     $(PTHREADS_LDFLAGS)"
	@echo "  Cilk:         $(CILK_LDFLAGS)"
	@echo "  Runner:       $(RUNNER_LDFLAGS)"
	@echo "  Libraries:    $(LDLIBS)"
	@echo ""
	@$(ECHO) "$(COLOR_BLUE)Implementations:$(COLOR_RESET)"
	@echo "  Sequential:   $(SEQUENTIAL_TARGET)"
	@echo "  OpenMP:       $(OPENMP_TARGET)"
	@echo "  Pthreads:     $(PTHREADS_TARGET)"
	@echo "  Cilk:         $(CILK_TARGET)"
	@echo "  Runner:       $(RUNNER_TARGET)"
	@echo ""
	@$(ECHO) "$(COLOR_BLUE)Source Files:$(COLOR_RESET)"
	@echo "  Core:         $(words $(CORE_SRCS)) files"
	@echo "  Utils:        $(words $(UTILS_SRCS)) files"
	@echo "  Main:         1 file"
	@echo "  Runner:       $(words $(RUNNER_MAIN_SRC) $(RUNNER_UTILS)) files"
	@echo "  Total:        $(words $(CORE_SRCS) $(UTILS_SRCS) $(MAIN_SRC) $(RUNNER_MAIN_SRC) $(RUNNER_UTILS)) common files"

.PHONY: list-binaries
list-binaries:
	@$(ECHO) "$(COLOR_BLUE)Built executables:$(COLOR_RESET)\n"
	@for bin in $(ALL_TARGETS); do \
		if [ -f "$$bin" ]; then \
			$(ECHO) "  $(COLOR_GREEN)✓$(COLOR_RESET) $$bin"; \
		else \
			$(ECHO) "  $(COLOR_YELLOW)✗$(COLOR_RESET) $$bin (not built)"; \
		fi; \
	done

.PHONY: check-deps
check-deps:
	@$(ECHO) "$(COLOR_BLUE)Checking dependencies...$(COLOR_RESET)"
	@which $(CC) > /dev/null || ($(ECHO) "$(COLOR_YELLOW)✗ gcc not found$(COLOR_RESET)" && exit 1)
	@$(ECHO) "  $(COLOR_GREEN)✓$(COLOR_RESET) gcc found: $(shell $(CC) --version | head -n1)"
	@which $(CLANG) > /dev/null || ($(ECHO) "$(COLOR_YELLOW)✗ clang not found$(COLOR_RESET)" && exit 1)
	@$(ECHO) "  $(COLOR_GREEN)✓$(COLOR_RESET) clang found: $(shell $(CLANG) --version | head -n1)"
	@gcc -pthread -E - </dev/null >/dev/null 2>&1 || ($(ECHO) "$(COLOR_YELLOW)✗ pthreads not found$(COLOR_RESET)" && exit 1)
	@$(ECHO) "  $(COLOR_GREEN)✓$(COLOR_RESET) pthreads found"
	@gcc -fopenmp -dM -E - < /dev/null | grep -i openmp > /dev/null || ($(ECHO) "$(COLOR_YELLOW)✗ openmp not found$(COLOR_RESET)" && exit 1)
	@$(ECHO) "  $(COLOR_GREEN)✓$(COLOR_RESET) openmp found"
	@echo -e '#include <cilk/cilk.h> \n int main() { cilk_spawn; return 0; }' | clang -fopencilk -xc - -o /dev/null 2> /dev/null || \
		($(ECHO) "$(COLOR_YELLOW)✗ opencilk not found$(COLOR_RESET)" && exit 1)
	@$(ECHO) "  $(COLOR_GREEN)✓$(COLOR_RESET) opencilk found"
	@pkg-config --exists matio && $(ECHO) "  $(COLOR_GREEN)✓$(COLOR_RESET) matio library found" || \
		($(ECHO) "  $(COLOR_YELLOW)✗$(COLOR_RESET) matio library not found" && exit 1)
	@which tree > /dev/null && $(ECHO) "  $(COLOR_GREEN)✓$(COLOR_RESET) tree found (optional)" || \
		$(ECHO) "  $(COLOR_YELLOW)○$(COLOR_RESET) tree not found (optional)"
	@$(ECHO) "$(COLOR_GREEN)All required dependencies found!$(COLOR_RESET)"

# ============================================
# Convenience targets for running benchmarks
# ============================================

# Run a comprehensive benchmark
.PHONY: benchmark
benchmark: all
	@$(ECHO) "$(COLOR_YELLOW)Running benchmark...$(COLOR_RESET)"
	@if [ -z "$(MATRIX)" ]; then \
		$(ECHO) "$(COLOR_RED)Error: MATRIX variable not set$(COLOR_RESET)"; \
		$(ECHO) "Usage: make bench-full MATRIX=path/to/matrix.mat [THREADS=8] [TRIALS=10]"; \
		exit 1; \
	fi
	@$(RUNNER_TARGET) -t $(if $(THREADS),$(THREADS),8) -n $(if $(TRIALS),$(TRIALS),10) $(MATRIX)

.PHONY: help
help:
	@$(ECHO) "$(COLOR_GREEN)════════════════════════════════════════$(COLOR_RESET)"
	@$(ECHO) "$(COLOR_GREEN)Connected Components - Available Targets$(COLOR_RESET)"
	@$(ECHO) "$(COLOR_GREEN)════════════════════════════════════════$(COLOR_RESET)"
	@echo ""
	@$(ECHO) "$(COLOR_BLUE)Building:$(COLOR_RESET)"
	@$(ECHO) "  $(COLOR_MAGENTA)all$(COLOR_RESET)           - Build all implementations (default)"
	@$(ECHO) "  $(COLOR_MAGENTA)sequential$(COLOR_RESET)    - Build only sequential version"
	@$(ECHO) "  $(COLOR_MAGENTA)openmp$(COLOR_RESET)        - Build only OpenMP version"
	@$(ECHO) "  $(COLOR_MAGENTA)pthreads$(COLOR_RESET)      - Build only Pthreads version"
	@$(ECHO) "  $(COLOR_MAGENTA)cilk$(COLOR_RESET)          - Build only Cilk version"
	@$(ECHO) "  $(COLOR_MAGENTA)runner$(COLOR_RESET)        - Build only benchmark runner"
	@$(ECHO) "  $(COLOR_MAGENTA)clean$(COLOR_RESET)         - Remove build artifacts"
	@$(ECHO) "  $(COLOR_MAGENTA)rebuild$(COLOR_RESET)       - Clean and build all"
	@echo ""
	@$(ECHO) "$(COLOR_BLUE)Benchmarking:$(COLOR_RESET)"
	@$(ECHO) "  $(COLOR_MAGENTA)benchmark$(COLOR_RESET)    - Full benchmark with custom settings"
	@$(ECHO) "                   Usage: make benchmark MATRIX=path/to/matrix.mat [THREADS=4] [TRIALS=10]"
	@echo ""
	@$(ECHO) "$(COLOR_BLUE)Information:$(COLOR_RESET)"
	@$(ECHO) "  $(COLOR_MAGENTA)info$(COLOR_RESET)          - Show build configuration"
	@$(ECHO) "  $(COLOR_MAGENTA)tree$(COLOR_RESET)          - Show project structure"
	@$(ECHO) "  $(COLOR_MAGENTA)list-sources$(COLOR_RESET)  - List all source files by category"
	@$(ECHO) "  $(COLOR_MAGENTA)list-binaries$(COLOR_RESET) - List all executables and their status"
	@$(ECHO) "  $(COLOR_MAGENTA)check-deps$(COLOR_RESET)    - Verify dependencies are installed"
	@$(ECHO) "  $(COLOR_MAGENTA)help$(COLOR_RESET)          - Show this message"
	@echo ""
	@$(ECHO) "$(COLOR_BLUE)Examples:$(COLOR_RESET)"
	@$(ECHO) "  make                                    # Build all versions"
	@$(ECHO) "  make benchmark MATRIX=data/test.mat THREADS=8 TRIALS=20"
	@echo ""

.DEFAULT_GOAL := all

# ============================================
# Phony targets (prevent conflicts with files)
# ============================================

.PHONY: all clean rebuild tree list-sources info check-deps help \
        sequential openmp pthreads cilk runner list-binaries \
        benchmark
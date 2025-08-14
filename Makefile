# 兼容 benchmark.sh 的转发目标
compare_bmssp: $(BIN)
baseline: bin/compare_bmssp_baseline

bin/compare_bmssp_baseline: $(SRC) $(HDR)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@
CXX := clang++
INCDIR := include
SRCDIR := src
SCRIPTDIR := scripts
CPPFLAGS := -I$(INCDIR) -I.
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers $(CPPFLAGS)
CXXFLAGS_O0 := -std=c++17 -O0 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers $(CPPFLAGS)
CXXFLAGS_DEBUG := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -DBMSSP_ENABLE_DEBUG=1 $(CPPFLAGS)
CXXFLAGS_LEMMA33 := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -DBMSSP_USE_LEMMA33=1 $(CPPFLAGS)
CXXFLAGS_DEBUG_LEMMA33 := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -DBMSSP_ENABLE_DEBUG=1 -DBMSSP_USE_LEMMA33=1 $(CPPFLAGS)
CXXFLAGS_O0_LEMMA33 := -std=c++17 -O0 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -DBMSSP_USE_LEMMA33=1 $(CPPFLAGS)

SRC := $(SRCDIR)/compare_main.cpp
HDR := $(INCDIR)/Graph.h $(INCDIR)/Dijkstra.h $(INCDIR)/BMSSP.h
BIN_DIR := bin
BIN := $(BIN_DIR)/compare_bmssp

all: $(BIN)

# 默认可执行程序改为使用 Lemma 3.3 数据结构
$(BIN): $(SRC) $(HDR)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS_LEMMA33) $(SRC) -o $@

.PHONY: debug debug_lemma33 o0 o0_lemma33 benchmark clean baseline lemma33
debug: $(BIN_DIR)/compare_bmssp_debug
debug_lemma33: $(BIN_DIR)/compare_bmssp_debug_lemma33

$(BIN_DIR)/compare_bmssp_debug: $(SRC) $(HDR)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS_DEBUG) $(SRC) -o $@

$(BIN_DIR)/compare_bmssp_debug_lemma33: $(SRC) $(HDR) opt_landings/BlockQueueLemma33.h
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS_DEBUG_LEMMA33) $(SRC) -o $@

o0: $(BIN_DIR)/compare_bmssp_o0
o0_lemma33: $(BIN_DIR)/compare_bmssp_o0_lemma33

$(BIN_DIR)/compare_bmssp_o0: $(SRC) $(HDR)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS_O0) $(SRC) -o $@

$(BIN_DIR)/compare_bmssp_o0_lemma33: $(SRC) $(HDR) opt_landings/BlockQueueLemma33.h
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS_O0_LEMMA33) $(SRC) -o $@


# Compare O0 vs O2 performance (default: both are lemma33 now)
benchmark: $(BIN) $(BIN_DIR)/compare_bmssp_o0_lemma33
	@echo "=== O0 vs O2 Performance Comparison ==="
	@echo "Running O0 version (no optimization)..."
	@time ./$(BIN_DIR)/compare_bmssp_o0_lemma33 1000 3
	@echo ""
	@echo "Running O2 version (optimized)..."
	@time ./$(BIN) 1000 3

.PHONY: bench bench-all bench-all-stats plot pip-install test-all
# Multi-scale benchmark (writes CSV). Options are passed to script.
bench: $(SCRIPTDIR)/benchmark.sh
	@chmod +x $(SCRIPTDIR)/benchmark.sh
	@$(SCRIPTDIR)/benchmark.sh

# Run all three presets (small/medium/large) and compare two main variants
bench-all: $(SCRIPTDIR)/benchmark.sh
	@chmod +x $(SCRIPTDIR)/benchmark.sh
	@$(SCRIPTDIR)/benchmark.sh -m all -v "lemma33 baseline" -o artifacts/bench_all.csv

# Same as bench-all but with BMSSP_STATS=1 to collect pulls/batches/inserts
bench-all-stats: $(SCRIPTDIR)/benchmark.sh
	@chmod +x $(SCRIPTDIR)/benchmark.sh
	@BMSSP_STATS=1 $(SCRIPTDIR)/benchmark.sh -m all -v "lemma33 baseline" -o artifacts/bench_all.csv

# Install Python plotting dependencies
pip-install: $(SCRIPTDIR)/requirements.txt
	python3 -m pip install -r $(SCRIPTDIR)/requirements.txt

# Plot helper: make plot PLOT_IN=bench_all.csv PLOT_OUT=plots
PLOT_IN ?= bench_all.csv
PLOT_OUT ?= plots
plot: $(SCRIPTDIR)/plot_bench.py
	python3 $(SCRIPTDIR)/plot_bench.py $(PLOT_IN) --out $(PLOT_OUT)

# Run a quick end-to-end test suite: build variants, run DS self-test, and two sanity runs
test-all: $(BIN) $(BIN_DIR)/compare_bmssp_baseline opt_landings/test_blockqueue
	@echo "=== [1/3] Data structure self-test (BlockQueue Lemma 3.3) ==="
	@cd opt_landings && make >/dev/null && ./test_blockqueue || { echo "BlockQueue self-test failed"; exit 1; }
	@echo "\n=== [2/3] Sanity run (lemma33) ==="
	@BMSSP_STATS=1 BMSSP_STRICT=1 ./$(BIN) 1000 5 42 || true
	@echo "\n=== [3/3] Sanity run (baseline) ==="
	@BMSSP_STATS=1 BMSSP_STRICT=1 ./$(BIN_DIR)/compare_bmssp_baseline 1000 5 42 || true
	@echo "\nDone. Review 'verify' and optional 'stats' lines above."

clean:
	rm -f $(BIN) $(BIN_DIR)/compare_bmssp_debug $(BIN_DIR)/compare_bmssp_debug_lemma33 $(BIN_DIR)/compare_bmssp_o0 $(BIN_DIR)/compare_bmssp_o0_lemma33 $(BIN_DIR)/compare_bmssp_baseline $(BIN_DIR)/compare_bmssp_lemma33

.PHONY: lemma33 baseline
lemma33: $(BIN_DIR)/compare_bmssp_lemma33
baseline: $(BIN_DIR)/compare_bmssp_baseline

$(BIN_DIR)/compare_bmssp_lemma33: $(SRC) $(HDR) opt_landings/BlockQueueLemma33.h
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS_LEMMA33) $(SRC) -o $@

$(BIN_DIR)/compare_bmssp_baseline: $(SRC) $(HDR)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

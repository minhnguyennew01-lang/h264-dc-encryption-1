# Makefile — H.264 DC Selective Encryption
# Algorithms: hybrid (PLCM+Arnold2D) | aes | des | rc4
# Run from project root (bitstream_impl_arnold2d/)

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-result

ALGO_SRCS = \
    algorithms/hybrid_encryption.cpp \
    algorithms/hybrid_algo.cpp \
    algorithms/aes_algo.cpp \
    algorithms/des_algo.cpp \
    algorithms/rc4_algo.cpp \
    algorithms/encryption_factory.cpp

PIPELINES = \
    pipelines/pipeline_hybrid_s1_h264analyze \
    pipelines/pipeline_hybrid_s2_h264analyze \
    pipelines/pipeline_hybrid_decrypt_s1_h264analyze \
    pipelines/pipeline_hybrid_decrypt_s2_h264analyze

EXTRACT = extract/extract_nalu_from_h264analyze

.PHONY: all pipelines extract clean

all: pipelines extract

pipelines: $(PIPELINES)

pipelines/pipeline_hybrid_s1_h264analyze: pipelines/pipeline_hybrid_s1_h264analyze.cpp $(ALGO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✓ Built $@"

pipelines/pipeline_hybrid_s2_h264analyze: pipelines/pipeline_hybrid_s2_h264analyze.cpp $(ALGO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✓ Built $@"

pipelines/pipeline_hybrid_decrypt_s1_h264analyze: pipelines/pipeline_hybrid_decrypt_s1_h264analyze.cpp $(ALGO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✓ Built $@"

pipelines/pipeline_hybrid_decrypt_s2_h264analyze: pipelines/pipeline_hybrid_decrypt_s2_h264analyze.cpp $(ALGO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✓ Built $@"

extract: $(EXTRACT)

$(EXTRACT): extract/extract_nalu_from_h264analyze.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<
	@echo "✓ Built $@"

TEST_BIN = test_vectors

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): test_vectors.cpp $(ALGO_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✓ Built $@"

clean:
	rm -f $(PIPELINES) $(EXTRACT) $(TEST_BIN)

# ──────────────────────────────────────────────────────────────────────────────
# Workflow:
#   make                                           # build everything
#   ./extract/extract_nalu_from_h264analyze_wrapper.sh videos/output.h264
#   ./pipelines/pipeline_hybrid_s1_h264analyze videos/output.h264 mykey --algo hybrid
#   ./pipelines/pipeline_hybrid_s1_h264analyze videos/output.h264 mykey --algo aes
#   ./pipelines/pipeline_hybrid_s1_h264analyze videos/output.h264 mykey --algo des
#   ./pipelines/pipeline_hybrid_s1_h264analyze videos/output.h264 mykey --algo rc4
#   ./pipelines/pipeline_hybrid_decrypt_s1_h264analyze \
#       results/output.h264/aes_s1/output.h264.s1_aes mykey --algo aes
# ──────────────────────────────────────────────────────────────────────────────

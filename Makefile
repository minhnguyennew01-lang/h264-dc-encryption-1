# Makefile for H.264 DC Encryption/Decryption Pipeline
# Usage:
#   make all       - Compile both encryption and decryption programs
#   make encrypt   - Compile encryption program only
#   make decrypt   - Compile decryption program only
#   make clean     - Remove all compiled binaries
#   make help      - Show this help message

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra
LDFLAGS = 

# Source files (common)
COMMON_SOURCES = cavlc_v2.cpp rbsp.cpp bitio.cpp

# Encryption with metadata program (MAIN)
ENCRYPT_TARGET = pipeline_encrypt_simple_metadata
ENCRYPT_SOURCES = pipeline_encrypt_simple_metadata.cpp encryption.cpp $(COMMON_SOURCES)
ENCRYPT_OBJECTS = $(ENCRYPT_SOURCES:.cpp=.o)

# Decryption with metadata program (MAIN)
DECRYPT_TARGET = pipeline_decrypt_simple_metadata
DECRYPT_SOURCES = pipeline_decrypt_simple_metadata.cpp encryption.cpp $(COMMON_SOURCES)
DECRYPT_OBJECTS = $(DECRYPT_SOURCES:.cpp=.o)

# DC Extraction Tool
EXTRACT_DC_TARGET = extract_dc
EXTRACT_DC_SOURCES = extract_dc.cpp $(COMMON_SOURCES)
EXTRACT_DC_OBJECTS = $(EXTRACT_DC_SOURCES:.cpp=.o)

# Default target
.PHONY: all
all: $(ENCRYPT_TARGET) $(DECRYPT_TARGET) $(EXTRACT_DC_TARGET)
	@echo ""
	@echo "╔════════════════════════════════════════════════════════╗"
	@echo "║  Build Complete!                                       ║"
	@echo "║  Encryption:        ./$(ENCRYPT_TARGET)                ║"
	@echo "║  Decryption:        ./$(DECRYPT_TARGET)                ║"
	@echo "║  DC Extraction:     ./$(EXTRACT_DC_TARGET)             ║"
	@echo "╚════════════════════════════════════════════════════════╝"
	@echo ""

# Encryption target
$(ENCRYPT_TARGET): $(ENCRYPT_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Compiled: $@"

# Decryption target
$(DECRYPT_TARGET): $(DECRYPT_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Compiled: $@"

# DC Extraction target
$(EXTRACT_DC_TARGET): $(EXTRACT_DC_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Compiled: $@"

# Encryption only
.PHONY: encrypt
encrypt: $(ENCRYPT_TARGET)

# Decryption only
.PHONY: decrypt
decrypt: $(DECRYPT_TARGET)

# DC Extraction only
.PHONY: extract-dc
extract-dc: $(EXTRACT_DC_TARGET)

# Object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo "  Compiling: $<"

# Clean build artifacts
.PHONY: clean
clean:
	rm -f *.o
	rm -f $(ENCRYPT_TARGET) $(DECRYPT_TARGET) $(EXTRACT_DC_TARGET)
	@echo "Cleaned: all build artifacts removed"

# Help
.PHONY: help
help:
	@echo "H.264 DC Encryption/Decryption Pipeline - Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make all      - Build both programs (default)"
	@echo "  make encrypt  - Build encryption program only"
	@echo "  make decrypt  - Build decryption program only"
	@echo "  make clean    - Remove all compiled files"
	@echo "  make help     - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  ./$(ENCRYPT_TARGET) <input.h264> <output.h264> \"key\""
	@echo "  ./$(DECRYPT_TARGET) <input.h264> <output.h264>"
	@echo ""
	@echo "Example:"
	@echo "  make all"
	@echo "  ./$(ENCRYPT_TARGET) video.h264 encrypted.h264 \"mykey\""
	@echo "  ./$(DECRYPT_TARGET) encrypted.h264 decrypted.h264"
	@echo "  diff video.h264 decrypted.h264  # Should be identical"
	@echo ""

# Test target (basic verification)
.PHONY: test
test: all
	@echo ""
	@echo "╔════════════════════════════════════════════════════════╗"
	@echo "║  Running Test...                                       ║"
	@echo "║  (Requires test files in current directory)            ║"
	@echo "╚════════════════════════════════════════════════════════╝"
	@echo ""
	@if [ -f output.h264 ]; then \
		echo "✓ Found test file: output.h264"; \
		echo "  - Encrypting..."; \
		./$(ENCRYPT_TARGET) output.h264 test_encrypted.h264 "testkey"; \
		echo "  - Decrypting..."; \
		./$(DECRYPT_TARGET) test_encrypted.h264 test_decrypted.h264 "testkey"; \
		echo "  - Verifying..."; \
		if cmp -s output.h264 test_decrypted.h264; then \
			echo "✓ TEST PASSED: Files are identical"; \
		else \
			echo "✗ TEST FAILED: Files differ"; \
		fi; \
		rm -f test_encrypted.h264 test_decrypted.h264; \
	else \
		echo "✗ Test file not found: output.h264"; \
		echo "  Please provide test H.264 file named 'output.h264'"; \
	fi
	@echo ""

# Display info
.PHONY: info
info:
	@echo "H.264 DC Encryption/Decryption Pipeline - Build Info"
	@echo ""
	@echo "Configuration:"
	@echo "  Compiler:    $(CXX)"
	@echo "  C++ Std:     -std=c++11"
	@echo "  Optimization: -O2"
	@echo ""
	@echo "Programs:"
	@echo "  Encryption:  $(ENCRYPT_TARGET)"
	@echo "  Decryption:  $(DECRYPT_TARGET)"
	@echo ""
	@echo "Source Files:"
	@echo "  Common:     $(COMMON_SOURCES)"
	@echo "  Encryption: pipeline_main.cpp encryption.cpp"
	@echo "  Decryption: pipeline_decrypt.cpp"
	@echo ""

# Verbose compilation (for debugging)
.PHONY: verbose
verbose: CXXFLAGS += -v
verbose: clean all

# Generate dependencies (advanced)
.PHONY: depend
depend:
	$(CXX) -MM *.cpp > .dependencies
	@echo "Dependencies generated"

# Include dependencies if they exist
-include .dependencies

# Default help on no target
.PHONY: default
default: help

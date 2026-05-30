# KEYSTREAM NON-DETERMINISM ROOT CAUSE ANALYSIS

## Problem Statement
Same I-frame (NALU Type:5) produces **different encrypted DC values** when encrypted by different strategies:
```
Original DC_S1_BEFORE:  Y: -120-124  47  -1  -2 -10 -82  -4 -53  43 116 126-107  46  29  89
Strategy 1 DC_AFTER:    Y: -107  82 -90 117  81  11 102 114  95-122  42  36 119  48   1  85
Strategy 2 DC_AFTER:    Y:  -84-102-117  61 111 -94  67  18 -65  50 -92-111-108-105  80 126
Strategy 3 DC_AFTER:    Y:  -84-102-117  61 111 -94  67  18 -65  50 -92-111-108-105  80 126
```

## Root Cause: PLCM Chaotic State Evolution

### The Issue
In `encrypt_dc_coefficients()` function (encryption.cpp, lines 240-260):

```cpp
PLCM plcm(mu, 0.37);  // x = 0.37 FIXED initial state
vector<uint8_t> keystream;
for (size_t i = 0; i < rbsp_data.size(); i++) {
    plcm.x = plcm.single_iterate(plcm.x);  // ← x EVOLVES with NALU position
    vector<uint8_t> bytes = plcm.double_to_bytes(plcm.x);
    keystream.push_back(bytes[0]);
}
```

**Problem**: PLCM state `x` is **persistent across NALUs**!

### Why Strategies Give Different Results

**Strategy 2** (Type 5 only):
1. NALU #1 (Type 7, SPS): Not encrypted, x stays at 0.37
2. NALU #2 (Type 8, PPS): Not encrypted, x stays at 0.37
3. NALU #3 (Type 5, I-frame): **x starts at 0.37**, evolves through this specific I-frame
   - This I-frame gets keystream from x₀.₃₇ → x₁ → x₂ → ... → x_N

**Strategy 3** (Type 1 & 5):
1. NALU #1 (Type 7, SPS): Not encrypted, x stays at 0.37
2. NALU #2 (Type 8, PPS): Not encrypted, x stays at 0.37
3. NALU #3 (Type 5, I-frame): **x starts at 0.37**, evolves through I-frame
   - This I-frame gets keystream from x₀.₃₇ → x₁ → x₂ → ... → x_N
4. NALU #4-22 (Type 1, P-frame): **x continues evolving!**
   - After encrypting N P-frames, x is now at x_M

**Strategy 1** (All frames):
- Uses different code path: `KeystreamGenerator::generateKeystream()`
- PLCM initialization: mu=0.37, x=0.2 (different from 0.37!)
- Each NALU pair gets different keystream

### Wait... Why Do S2 and S3 Match?

Since both use `encrypt_dc_coefficients()` with x=0.37 initial, they **should** produce same keystream for same I-frame... **BUT THEY DO IF:**

- Same video file
- Same key
- **Same processing order**
- If x doesn't evolve between their execution

Actually, **S2 and S3 matching makes sense**: Both use the exact same encryption pipeline for Type 5. The reason they match but differ from S1 is:

**S1 uses different PLCM parameters:**
```cpp
// Strategy 1: KeystreamGenerator::generateKeystream()
double x = 0.2;   // NOT 0.37!
double mu = 0.37;
// Pre-iterate 1000 times
// Generate keystream byte by byte
```

**S2 & S3 use same parameters:**
```cpp
// Strategies 2 & 3: encrypt_dc_coefficients()
PLCM plcm(mu, 0.37);  // x = 0.37, same mu from key
```

## Fundamental Cryptographic Issue

### Correct PLCM Initialization
For reproducible, deterministic encryption:
```
For each NALU:
    x = KEY_DERIVATION_FUNCTION(key, nalu_index)
    keystream = generate_keystream_from(x)
```

### Current Implementation (WRONG)
```
x = 0.37  // Fixed
For each NALU:
    x = x.evolve()  // ← Chaotic state carries over!
    keystream = generate_keystream_from(x)
```

## Solution Options

### Option A: Reset PLCM Per NALU (Recommended)
```cpp
vector<uint8_t> encrypt_dc_coefficients(...) {
    ...
    // Generate initial x from NALU index + key
    double init_x = KEY_DERIVE_INIT_X(key, nalu_index);
    
    PLCM plcm(mu, init_x);  // Reset for each NALU!
    for (size_t i = 0; i < rbsp_data.size(); i++) {
        plcm.x = plcm.single_iterate(plcm.x);
        keystream.push_back(...);
    }
}
```

### Option B: Use Global PLCM Counter
Track NALU index globally, deterministically seed x per NALU.

### Option C: Different Keystream Per NALU Size
Make keystream depend on NALU size + index, not just previous state.

## Impact Assessment

| Strategy | S2 vs S1 | S3 vs S1 | S2 vs S3 |
|----------|----------|----------|----------|
| Current behavior | Different (✓ anomaly) | Different (✓ anomaly) | Same (✓ expected) |
| Root cause | Different x₀ (0.37 vs 0.2) | Different x₀ | Same x₀ + same code |
| Determinism | ✗ Each run same keystream but non-reproducible | ✗ Each run same keystream but non-reproducible | ✓ Reproducible |

## Recommendation

**Implement Option A: PLCM state reset per NALU with key-derived seeding.**

This ensures:
- ✓ Same plaintext + same key → same ciphertext (determinism)
- ✓ Different NALUs get different keystreams (security)
- ✓ All strategies produce same encrypted result for same NALU
- ✓ Reproducible across runs and different processing strategies

## Files to Modify

1. **encryption.cpp**: `encrypt_dc_coefficients()` - reset x per NALU
2. **encryption.cpp**: `decrypt_dc_coefficients()` - must match encryption
3. **pipeline_encrypt_strategy1_with_dc.cpp**: Consider using `encrypt_dc_coefficients()` instead of `KeystreamGenerator`
4. **encryption.h**: Add NALU index parameter to encryption functions

## Verification Method

After fix, run:
```bash
./pipeline_encrypt_s1 input.h264 out_s1.h264 key
./pipeline_encrypt_s2 input.h264 out_s2.h264 key
./pipeline_encrypt_s3 input.h264 out_s3.h264 key

./extract_dc_ycbcr out_s1.h264 > dc_s1.txt
./extract_dc_ycbcr out_s2.h264 > dc_s2.txt
./extract_dc_ycbcr out_s3.h264 > dc_s3.txt

diff dc_s1.txt dc_s2.txt  # Should be identical!
diff dc_s2.txt dc_s3.txt  # Should be identical!
```

./extract/extract_nalu_from_h264analyze videos/output.h264 2>&1

2.Encrypt all 8 combinations with key testkey:
KEY="testkey"
for ALGO in hybrid aes des rc4; do
    echo "=== Encrypt S1 --algo $ALGO ==="
    ./pipelines/pipeline_hybrid_s1_h264analyze videos/output.h264 $KEY --algo $ALGO 2>&1 | grep -E "Encrypted|DC_ONLY|ERROR|error"
    echo "=== Encrypt S2 --algo $ALGO ==="
    ./pipelines/pipeline_hybrid_s2_h264analyze videos/output.h264 $KEY --algo $ALGO 2>&1 | grep -E "Encrypted|DC_ONLY|ERROR|error"
done

3.Decrypt all 8 combinations:
KEY="testkey"
declare -A ENC_S1=(
    [hybrid]="results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed"
    [aes]="results/output.h264/aes_s1/output.h264.s1_aes"
    [des]="results/output.h264/des_s1/output.h264.s1_des"
    [rc4]="results/output.h264/rc4_s1/output.h264.s1_rc4"
)
declare -A ENC_S2=(
    [hybrid]="results/output.h264/hybrid_s2/output.h264.s2_hybrid_fixed"
    [aes]="results/output.h264/aes_s2/output.h264.s2_aes"
    [des]="results/output.h264/des_s2/output.h264.s2_des"
    [rc4]="results/output.h264/rc4_s2/output.h264.s2_rc4"
)
for ALGO in hybrid aes des rc4; do
    echo "=== Decrypt S1 --algo $ALGO ==="
    ./pipelines/pipeline_hybrid_decrypt_s1_h264analyze "${ENC_S1[$ALGO]}" $KEY --algo $ALGO 2>&1 | grep -E "Decrypted|DC_ONLY|ERROR|error"
    echo "=== Decrypt S2 --algo $ALGO ==="
    ./pipelines/pipeline_hybrid_decrypt_s2_h264analyze "${ENC_S2[$ALGO]}" $KEY --algo $ALGO 2>&1 | grep -E "Decrypted|DC_ONLY|ERROR|error"
done

4.Verify byte-exact for all 8 combinations
declare -A DEC_S1=(
    [hybrid]="results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed.decrypted"
    [aes]="results/output.h264/aes_s1/output.h264.s1_aes.decrypted"
    [des]="results/output.h264/des_s1/output.h264.s1_des.decrypted"
    [rc4]="results/output.h264/rc4_s1/output.h264.s1_rc4.decrypted"
)
declare -A DEC_S2=(
    [hybrid]="results/output.h264/hybrid_s2/output.h264.s2_hybrid_fixed.decrypted"
    [aes]="results/output.h264/aes_s2/output.h264.s2_aes.decrypted"
    [des]="results/output.h264/des_s2/output.h264.s2_des.decrypted"
    [rc4]="results/output.h264/rc4_s2/output.h264.s2_rc4.decrypted"
)
for ALGO in hybrid aes des rc4; do
    for STR in S1 S2; do
        if [ "$STR" = "S1" ]; then DEC="${DEC_S1[$ALGO]}"; else DEC="${DEC_S2[$ALGO]}"; fi
        RESULT=$(cmp --silent videos/output.h264 "$DEC" && echo "BYTE-EXACT OK" || echo "MISMATCH")
        echo "$ALGO $STR: $RESULT"
    done
done

5.Run measure_metrics.py for all 4 algos S1
KEY="testkey"
declare -A ENC_S1=([hybrid]="results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed" [aes]="results/output.h264/aes_s1/output.h264.s1_aes" [des]="results/output.h264/des_s1/output.h264.s1_des" [rc4]="results/output.h264/rc4_s1/output.h264.s1_rc4")
declare -A ENC_S2=([hybrid]="results/output.h264/hybrid_s2/output.h264.s2_hybrid_fixed" [aes]="results/output.h264/aes_s2/output.h264.s2_aes" [des]="results/output.h264/des_s2/output.h264.s2_des" [rc4]="results/output.h264/rc4_s2/output.h264.s2_rc4")
declare -A DEC_S1=([hybrid]="results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed.decrypted" [aes]="results/output.h264/aes_s1/output.h264.s1_aes.decrypted" [des]="results/output.h264/des_s1/output.h264.s1_des.decrypted" [rc4]="results/output.h264/rc4_s1/output.h264.s1_rc4.decrypted")
declare -A DEC_S2=([hybrid]="results/output.h264/hybrid_s2/output.h264.s2_hybrid_fixed.decrypted" [aes]="results/output.h264/aes_s2/output.h264.s2_aes.decrypted" [des]="results/output.h264/des_s2/output.h264.s2_des.decrypted" [rc4]="results/output.h264/rc4_s2/output.h264.s2_rc4.decrypted")

for ALGO in hybrid aes des rc4; do
    echo ""
    echo "=============================="
    echo " METRICS: $ALGO S1"
    echo "=============================="
    python3 scripts/measure_metrics.py \
        --original  videos/output.h264 \
        --encrypted "${ENC_S1[$ALGO]}" \
        --decrypted "${DEC_S1[$ALGO]}" \
        --strategy S1 --algo $ALGO --key $KEY \
        --output-dir results/output.h264/metrics 2>&1 | grep -E "NPCR|UACI|Entropy|SHA256|BER|PSNR|SSIM|Encrypt|Decrypt|plaintext|Byte Corr|SKIP.*plain|Codec" | grep -v "RuntimeWarning\|numpy\|invalid"
done

6.Run metrics for hybrid S1 and show output:
python3 scripts/measure_metrics.py \
    --original  videos/output.h264 \
    --encrypted results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed \
    --decrypted results/output.h264/hybrid_s1/output.h264.s1_hybrid_fixed.decrypted \
    --strategy S1 --algo hybrid --key testkey 2>&1 | grep -v "RuntimeWarning\|_function_base\|stddev\|c /=" | head -60

7.Run metrics for AES, DES, RC4 S1:
for ALGO in aes des rc4; do
    echo ""
    echo "=============================="
    echo " METRICS: $ALGO S1"
    echo "=============================="
    python3 scripts/measure_metrics.py \
        --original  videos/output.h264 \
        --encrypted "results/output.h264/${ALGO}_s1/output.h264.s1_${ALGO}" \
        --decrypted "results/output.h264/${ALGO}_s1/output.h264.s1_${ALGO}.decrypted" \
        --strategy S1 --algo $ALGO --key testkey 2>&1 | grep -v "RuntimeWarning\|_function_base\|stddev\|c /=" | grep -E "NPCR|UACI|Entropy|SHA256|BER|PSNR|SSIM|Encrypt time|Decrypt time|plaintext|Byte Corr|SKIP.*plain|Codec|WARNING"
done

8.Run measure_metrics.py for all 4 algos S2
cd /home/minh/Documents/NCKH20262/bitstream_impl_arnold2d && for ALGO in hybrid aes des rc4; do
    ENC_FILE="results/output.h264/${ALGO}_s2/output.h264.s2_${ALGO}"
    [ "$ALGO" = "hybrid" ] && ENC_FILE="results/output.h264/hybrid_s2/output.h264.s2_hybrid_fixed"
    DEC_FILE="${ENC_FILE}.decrypted"
    echo ""
    echo "=============================="
    echo " METRICS: $ALGO S2"
    echo "=============================="
    python3 scripts/measure_metrics.py \
        --original  videos/output.h264 \
        --encrypted "$ENC_FILE" \
        --decrypted "$DEC_FILE" \
        --strategy S2 --algo $ALGO --key testkey 2>&1 | grep -v "RuntimeWarning\|_function_base\|stddev\|c /=" | grep -E "NPCR|UACI|Entropy|SHA256|BER|PSNR|SSIM|Encrypt time|Decrypt time|plaintext|Byte Corr|SKIP.*plain|WARNING"
done
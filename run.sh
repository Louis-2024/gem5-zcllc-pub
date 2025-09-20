rm -rf m5out

# kernal test

# PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.fast --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
#     configs/xyz/simple_ruby.py --wc --ncore 4 --wc --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
#     --program /gem5/Splash-3/codes/kernels/cholesky/CHOLESKY --cwd /gem5/Splash-3/codes/kernels/cholesky --args "-p4 -B32 -C65536" \
#     --input-file /gem5/Splash-3/codes/kernels/cholesky/inputs/tk14.O

# PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.fast --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
#     configs/xyz/simple_ruby.py --wc --ncore 4 --wc --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
#     --program /gem5/Splash-3/codes/kernels/lu/contiguous_blocks/LU --cwd /gem5/Splash-3/codes/kernels/lu/contiguous_blocks --args "-p4 -n256 -b16"

# app test

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.fast --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
    configs/xyz/simple_ruby.py --wc --ncore 4 --wc --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
    --program /gem5/Splash-3/codes/apps/fmm/FMM --cwd /gem5/Splash-3/codes/apps/fmm --input-file /gem5/Splash-3/codes/apps/fmm/inputs/input.4.256
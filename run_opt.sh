rm -rf m5out

# quick tests

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
    --program /gem5/Splash-3/codes/kernels/cholesky/CHOLESKY --cwd /gem5/Splash-3/codes/kernels/cholesky --args "-p4 -B32 -C65536" \
    --input-file /gem5/Splash-3/codes/kernels/cholesky/inputs/tk14.O

# PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
#     configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
#     --program /gem5/Splash-3/codes/kernels/radix/RADIX --cwd /gem5/Splash-3/codes/kernels/radix --args "-p4 -n262144 -r256"

# PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
#     configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
#     --program /gem5/Splash-3/codes/apps/ocean/contiguous_partitions/OCEAN --cwd /gem5/Splash-3/codes/apps/ocean/contiguous_partitions \
#     --args "-p4 -n130"

# moderate tests

# PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
#     configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
#     --program /gem5/Splash-3/codes/apps/raytrace/RAYTRACE \
#     --cwd /gem5/Splash-3/codes/apps/raytrace \
#     --args "-p4 /gem5/Splash-3/codes/apps/raytrace/inputs/teapot.env"

# PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
#     configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
#     --program /gem5/Splash-3/codes/apps/barnes/BARNES --cwd /gem5/Splash-3/codes/apps/barnes \
#     --input-file /gem5/Splash-3/codes/apps/barnes/inputs/n8k-p4

# slow tests

# PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
#     configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --wc --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
#     --program /gem5/Splash-3/codes/apps/fmm/FMM --cwd /gem5/Splash-3/codes/apps/fmm \
#     --input-file /gem5/Splash-3/codes/apps/fmm/inputs/input.4.16384

# PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
#     configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
#     --program /gem5/Splash-3/codes/apps/barnes/BARNES --cwd /gem5/Splash-3/codes/apps/barnes \
#     --input-file /gem5/Splash-3/codes/apps/barnes/inputs/n16384-p4

# synthetic test
 
# PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt --debug-flags=ZIVCache --debug-file=ziv.log --debug-start=0 \
#     configs/xyz/simple_ruby.py --wc --subslot-opt --ruby-test --ncore 4 --use-ziv --use-vi --l1-size 2kB --l2-size 8kB --l3-size 128kB \
#     --nreq 20000
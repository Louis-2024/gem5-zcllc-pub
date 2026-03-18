mkdir -p stats

#kernels

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/kernels/cholesky/CHOLESKY --cwd /gem5/Splash-3/codes/kernels/cholesky --args "-p4 -B32 -C65536" \
    --input-file /gem5/Splash-3/codes/kernels/cholesky/inputs/tk16.O \
    2>&1 | tee stats/cholesky_terminal.txt
sleep 1
mv m5out/stats.txt stats/cholesky_stats.txt

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/kernels/radix/RADIX --cwd /gem5/Splash-3/codes/kernels/radix --args "-p4 -n524288 -r256" \
    2>&1 | tee stats/radix_terminal.txt
sleep 1
mv m5out/stats.txt stats/radix_stats.txt

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/kernels/fft/FFT --cwd /gem5/Splash-3/codes/kernels/fft --args "-p4 -m20 -n20 -l4" \
    2>&1 | tee stats/fft_terminal.txt
sleep 1
mv m5out/stats.txt stats/fft_stats.txt

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/kernels/lu/contiguous_blocks/LU --cwd /gem5/Splash-3/codes/kernels/lu/contiguous_blocks --args "-p4 -n512 -b16" \
    2>&1 | tee stats/lu_contiguous_terminal.txt
sleep 1
mv m5out/stats.txt stats/lu_contiguous_stats.txt

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/kernels/lu/non_contiguous_blocks/LU --cwd /gem5/Splash-3/codes/kernels/lu/non_contiguous_blocks --args "-p4 -n512 -b16" \
    2>&1 | tee stats/lu_non_contig_terminal.txt
sleep 1
mv m5out/stats.txt stats/lu_non_contig_stats.txt

# apps

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/apps/ocean/contiguous_partitions/OCEAN --cwd /gem5/Splash-3/codes/apps/ocean/contiguous_partitions \
    --args "-p4 -n130" \
    2>&1 | tee stats/ocean_contiguous_terminal.txt
sleep 1
mv m5out/stats.txt stats/ocean_contiguous_stats.txt

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/apps/ocean/non_contiguous_partitions/OCEAN --cwd /gem5/Splash-3/codes/apps/ocean/non_contiguous_partitions \
    --args "-p4 -n130" \
    2>&1 | tee stats/ocean_noncontig_terminal.txt
sleep 1
mv m5out/stats.txt stats/ocean_noncontig_stats.txt

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/apps/raytrace/RAYTRACE \
    --cwd /gem5/Splash-3/codes/apps/raytrace \
    --args "-p4 /gem5/Splash-3/codes/apps/raytrace/inputs/teapot.env" \
    2>&1 | tee stats/raytrace_terminal.txt
sleep 1
mv m5out/stats.txt stats/raytrace_stats.txt

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/apps/barnes/BARNES --cwd /gem5/Splash-3/codes/apps/barnes \
    --input-file /gem5/Splash-3/codes/apps/barnes/inputs/n8k-p4 \
    2>&1 | tee stats/barnes_small_terminal.txt
sleep 1
mv m5out/stats.txt stats/barnes_small_stats.txt

PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.opt \
    configs/xyz/simple_ruby.py --wc --subslot-opt --ncore 4 --use-ziv --use-vi --l1-assoc 8 --l2-assoc 8 --l3-assoc 8 --l1-size 4kB --l2-size 16kB --l3-size 1MB \
    --program /gem5/Splash-3/codes/apps/barnes/BARNES --cwd /gem5/Splash-3/codes/apps/barnes \
    --input-file /gem5/Splash-3/codes/apps/barnes/inputs/n16384-p4 \
    2>&1 | tee stats/barnes_large_terminal.txt
sleep 1
mv m5out/stats.txt stats/barnes_large_stats.txt



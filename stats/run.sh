cd ..
rm -rf m5out
PYTHONPATH="$PWD/configs:$PWD/build/X86_LC_MSI/python" ./build/X86_LC_MSI/gem5.fast configs/xyz/simple_ruby.py  --ncore 8 --wc --use-ziv --use-vi --l1-size 8kB --l2-size 64kB --l3-size 1MB --slot-width 128 --ruby-test
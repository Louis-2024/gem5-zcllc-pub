rm -rf build/.scons_config build/sconsign build/variables/ build/variables.global
 
CC=clang-12 CXX=clang++-12 \
scons -Q --config=force \
    PYTHON_CONFIG=/usr/bin/python3.8-config \
    build/X86_LC_MSI/gem5.fast --ignore-style -j8 --default=X86_LC_MSI
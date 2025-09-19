rm -rf build/X86_LC_MSI

scons --config=force -Q \
  PYTHON_CONFIG=/usr/bin/python3.8-config \
  CC=clang-12 CXX=clang++-12 \
  build/X86_LC_MSI/gem5.opt \
  --ignore-style -j4 --default=X86_LC_MSI --gold-linker
# Build helper for hosts whose Device Guard policy blocks the RISC-V toolchain's
# riscv32-esp-elf-gcc-ar.exe wrapper. gcc-ar is the plain archiver plus the LTO
# plugin; this firmware is built without LTO, so the toolchain's own ar/ranlib
# produce the same archives and keep clean builds and `-t upload` working.
#
# This is wired up by `extra_scripts = pre:tools/build_plain_ar.py` in
# platformio.ini. Remove that line (and this file) if your policy allows the
# wrapper, or if LTO is ever enabled for this project.
Import("env")

env.Replace(ARCOM="riscv32-esp-elf-ar $ARFLAGS $TARGET $SOURCES")
env.Replace(RANLIBCOM="riscv32-esp-elf-ranlib $TARGET")

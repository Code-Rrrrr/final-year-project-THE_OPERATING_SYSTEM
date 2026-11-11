@echo off

echo Running Command 1...
nasm -f bin stage_one_bootloader.asm -o stage_one_bootloader.bin
if errorlevel 1 goto error

echo Running Command 2...
nasm -f bin stage_two_bootloader.asm -o stage_two_bootloader.bin
if errorlevel 1 goto error

echo Running Command 3...
nasm -f elf64 kernel_entry.asm -o kernel_entry.o
if errorlevel 1 goto error

echo Running Command 4...
clang --target=x86_64-elf -march=x86-64 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel -Wall -Wextra -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only -c kernel.c -o kernel.o
if errorlevel 1 goto error

echo Running Command 5...
ld.lld -Ttext 0x10000 --image-base=0x10000 -e kernel_entry -nostdlib kernel_entry.o kernel.o -o kernel.elf
if errorlevel 1 goto error

echo Running Command 6...
llvm-objcopy -O binary kernel.elf kernel.bin
if errorlevel 1 goto error

echo Running Command 7...
python pad.py
if errorlevel 1 goto error

echo Running Command 8...
python combine.py
if errorlevel 1 goto error

echo Running Command 9...
qemu-system-x86_64 -m 16M -drive format=raw,file=os.bin
if errorlevel 1 goto error

echo.
echo All commands executed successfully!

exit /b 0

:error
echo.
echo ERROR: A command failed. Stopping execution.

exit /b 1

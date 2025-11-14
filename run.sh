echo '--Starting a debug session using GDB--' 
qemu-system-i386 -drive file=fs.img,format=raw -cdrom kernel.iso -boot d -s -S &
# Connect using gef-remote (can be switch to a simple remote)
# Enter 'c' (or 'continue') to continue program execution
gdb kernel.elf -ex 'target remote localhost:1234' -ex 'set architecture i386'

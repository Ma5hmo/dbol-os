# DBol OS
## Build
Run the init.sh script

Make sure that the $HOME/opt/cross/bin is in the PATH env variable
```
$ make clean
$ make
```

## Run
To create a file system you should run 
- ```qemu-img create -f raw fs.img <size you want>```
- ``` mkfs.fat -F 16 fs.img ```


And you can run with 

``` qemu-system-i386 -cdrom kernel.iso -drive format=raw,file=fs.img,media=disk -boot d```

## Filesystem
To initialzie a disk image you can run: ``` make disk ```

We prepared a loader script for the os disk.

The loader is in the directory fs_loader, and can be compiled easily with gcc:

``` gcc fat_loader.c -o fat_loader ```

Then you can use the loader:

``` ./fat_loader fs.img <target path> <local path> ```


## Debug
To the debug the kernel with qemu and gdb you must follow these steps:
1. Install qemu and gdb
2. Build the kernel with regular ```make```
3. Build the kernel with ``make kernel.elf``
4. Run ```qemu-system-i386 -cdrom kernel.iso -drive format=raw,file=fs.img,media=disk -boot d -s -S```
5. Run gdb
6. Inside gdb enter the following commands:
- ```file kernel.elf``` (or just run ```gdb kernel.elf```)
- (put breakpoint where you want)
- ``target remote localhost:1234 `` or ``gef-remote localhost 1234``

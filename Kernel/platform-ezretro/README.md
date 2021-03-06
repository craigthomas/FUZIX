## A FUZIX target for the EZ-Retro v2 board
> see <https://docs.jeelabs.org/projects/ezr/>

(adapted from the `jeeretro`, and `z80pack`/`sbcv2` ports)

This is a first exploration to run FUZIX on the Zilog eZ80F91 processor. The
EZ-Retro board has the eZ80 µC, an SRAM of either 512 KB or 2048 KB, and a
"secondary" µC board to drive the clock (at 36 MHz), to connect to the serial
port as console, and to act as (optional) disk controller. For now, the
secondary µC board is an ARM STM32F103, but a future variant could use an ESP32
for network connectivity, or forego the extra µC altogether and directly
connect to an SD card using SPI.

EZ Retro is configured with a simple 8K fixed common and 56K fixed sized banks.

We run FUZIX with one application per bank and the memory map currently is:

### Bank 0:

``` text
0000-0080	Vectors
0081-0084	Saved CP/M command info
0100		FUZIX kernel start
????		FUZIX kernel end ~= A000
(big kernels go up to E400 or so!)
no discard area, current kernel easily fits in 56K
End of kernel:	Common >= 0xE000
                uarea
                uarea stack
                interrupt stack
                bank switching code
                user memory access copy
                interrupt vectors and glue
                [Possibly future move the buffers up here to allow for more
                 disk and inode buffer ?]
FFFF		Hard end of kernel room
```

### Bank 1 to Bank n:

``` text
0000		Vector copy
0080		free
0100		Application
DCFF		Application end
DD00-DFFF	uarea stash
```

### Booting

The `kernel/fuzix.bin` file is configured to load at 0x0100. It's converted to
a C header file using the command "`xxd -i <fuzix.bin >fuzix.h`" and then
incorporated into the F103 binary. This is done so a new kernel can quickly be
tested, without having to go through a µSD card juggle/dance/swap.

The root file system during development is the 1440K image created by the
`Standalone/` scripts. It is loaded into RAM by that same F103 binary on
startup, so that no actual disk hardware is needed for initial testing. This is
only possible with a 2048 KB ram chip, the 512 KB config would not be enough to
hold user banks as well as a RAM disk.

### eZ80 details

The eZ80F91 chip only needs a clock and external RAM to operate, with its
built-in UART as console. The eZ80 has many more peripherals, such as timers,
RTC, SPI, and even an Ethernet MAC, which can/will/shall be explored later. For
now, the first build runs without any timers or interrupts, and the disk image
in kept fully in RAM.

When the eZ80 is in Z80 mode (i.e. the original 16-bit addressing), the upper 8
bits of the memory addresses are supplied by a "memory base" register (which
can only be changed in "ADL" mode). Similarly, the upper 8 address bits of the
internal flash and RAM memory areas are defined through I/O writes to special
eZ80 I/O ports (flash is at 00:0000 after reset, RAM at FF:C000 and FF:E000).

The memory map is set up as follows by the secondary µC, before starting the
FUZIX image:

| Range (hex)       | Size      | Description                      | Purpose  |
|:-----------------:|----------:|----------------------------------|:--------:|
| FF:E000 - FF:FFFF | 8 KB      | internal RAM, moved around    | common bank |
| FF:C000 - FF:DFFF | 8 KB      | internal Ethernet RAM          | (disabled) |
| F4:0000 - FF:BFFF | 1,776 KB  | _(alias of 14:0000 - 1F:BFFF)_   | (unused) |
| F0:0000 - F3:FFFF | 256 KB    | internal flash memory            | (unused) |
| 20:0000 - EF:FFFF | 13,312 KB | _(aliased external RAM copies)_  | (unused) |
| 08:0000 - 1F:FFFF | 1,536 KB  | more external RAM (2048K only)   | RAM disk |
| 00:0000 - 07:FFFF | 512 KB    | external RAM (512/2048K)     | 8x 64K banks |

Bank 0 is for the FUZIX kernel, banks 1..7 are user processes. Each bank only
has 56K available, the upper 8K will be shadowed by the common bank, which gets remapped
during bank switches.

### Development

The hardware requirements are kept minimal for development, just to get this
stuff going. There's no real disk or SD card. Booting is completely managed by
the secondary µC, using four I/O pins:

* XDI is the 36 MHz clock signal, generated by the F103 (which runs at 72 MHz)
* RST is connected to the hardware reset pin of the eZ80 chip
* ZCL and ZDA act like JTAG, or rather SWD, i.e. the "Zilog Debugging
  Interface"

And of course RX/TX are connected as interface to the console UART.

In addition, 4 SPI pins and a "BUSY" line are also wired between the eZ80 and
the F103. These could be used later to implement a disk drive for the eZ80.

Once the pins, clock, and uarts have been set up, the FUZIX kernel is saved to
00:0100-up, and the disk image is saved to 08:0000-up. As copying 1.5 MB takes
over 10 seconds, this is only done if there's no valid disk image in RAM. The
eZ80 can always be power-cycled to force copying.

The F103's firmware contains a copy of the FUZIX kernel, converted to C via
this command:

    xxd -i <fuzix.bin >src/fuzix.h

As last step, the F103 lets the eZ80 start up at 00:0100 in Z80 mode, and turns
itself into a serial pass-through.

### F103 details

The eZ80 is managed by a secondary µC, which provides a clock signal, as well
as initializing the eZ80's state and its RAM contents. The eZ80's built-in
flash memory is not used. I currently use an STM32F103, the code for which is
available at
[git.jeelabs.org/jcw/retro](https://git.jeelabs.org/jcw/retro/src/branch/master/ez80/fuzix-arm).
This is a PlatformIO project.

Once the proper hardware (a cheap board off eBay) and the required six I/O
wires have been connected (see notes at start of
[main.cpp](https://git.jeelabs.org/jcw/retro/src/branch/master/ez80/fuzix-arm/src/main.cpp)),
the build + upload can be done with a single command:

    pio run -t upload

You will have to change the `upload_protocol`, `upload_port`, and
`monitor_port` to match your development setup. I use a Black Magic Probe,
which is highly recommended, as it supports uploading, gdb-debugging, and
serial console, all through a single USB cable.

### Thoughts

* There are many ways to implement disk storage: either connect an SD card
  directly and let the eZ80 handle the SPI/SD transactions, or define a simple
  protocol to use the secondary µC as  peripheral processor, for storage,
  networking, comms, clock/sleep control, etc. By adding a clock crystal and SD
  card socket, the former would remove the need for a secondary µC (but it
  still leaves the puzzle of initial flash setup). The latter is more of a
  "retro'ish hybrid", but with a lot of potential with an ESP32, for example.

* Since the eZ80 can switch from Z80 to ADL mode through special instructions
  (based on illegal or useless opcodes in the original Z80), there is not even
  a need to maintain a common bank for FUZIX. Each user process could have the
  full 64K, with a special "system call" for kernel requests. The kernel itself
  can easily access all of memory, even if it stays (mostly) in Z80 mode.

* With a more advanced approach, the eZ80 can also run in "mixed" mode, where
  the user app is Z80 code, but all interrupts are handled in ADL mode, which
  has its own 24-bit stack pointer. That would offer extra overwrite/crash
  protection and allow the kernel itself to grow beyond 64K.

## Programming the Series1 (G1) Pano Logic device

The first generation device (Series 1 or G1) has a SPI flash chip which is 
slightly larger than the bitstream, but it is not big enough for two 
bitstreams so there is only one bitstream to program.  

This makes things easier, but it also means an unexpected error or power 
outage can **BRICK** a device.

There are no known bitstreams other than the original Pano Logic bitstreams that
implement Pano's protocol so this is a one way update.  

There is no way to recover a "bricked" G1 Pano without a JTAG or SPI 
programmer after flashing a new bitstreams.

## Patching progfpga

* Checkout the pano_progfpga repository.
* CD into pano_progfpga.
* Build the patch program.
* Run patch_progfpga giving agrements specifying type of Pano which will 
be updated and the path to the binary Xilinx bitstream file to embed into 
progfpga (note: bitstream must be a raw binary file, not a .bit file).  

For example to flash a revision G1 with a new bitstream:

```
skip@dell-790:~/pano$ git clone https://github.com/skiphansen/pano_progfpga.git
Cloning into 'pano_progfpga'...
remote: Enumerating objects: 80, done.
remote: Counting objects: 100% (80/80), done.
remote: Compressing objects: 100% (61/61), done.
remote: Total 80 (delta 13), reused 78 (delta 11), pack-reused 0
Unpacking objects: 100% (80/80), done.
skip@dell-790:~/pano$ cd pano_progfpga/
skip@dell-790:~/pano/pano_progfpga$ make
skip@dell-790:~/pano/pano_progfpga$ make
mkdir patched
cp -a series1 patched
cp -a series2 patched
cc     patch_progfpga.c   -o patch_progfpga
skip@dell-790:~/pano/pano_progfpga$ ./patch_progfpga -1 ../pano_man/xilinx/Pano.bin
patched/series1/progfpga updated successfully with ../pano_man/xilinx/Pano.bin
progfpga script created successfully.

To flash series1:
   ./progfpga -d
   ./progfpga <ip address>
skip@dell-790:~/pano/pano_progfpga$
```

## Flashing the image over the network

Flashing a Pano device with the patched progfpga utility is exactly the 
same is as explained in Pano video, namely: 

* Plug the Pano into your local LAN.
* Turn it on.
* Wait for it to obtain an IP address via DHCP.
* Discover it's IP address.
* Update it.
* Cross your fingers and wait as the Pano is updated.  (Go get coffee, 
this will take a good amount of time).  
* Power cycle the Pano and enjoy your new image.

For example:

```
skip@dell-790:~/pano/pano_progfpga$ ./progfpga -d
Running Test = discover
IP ADDRESS        SUBNET MASK       GATEWAY ADDRESS   MAC ADDRESS       REVISION
192.168.123.166   255.255.255.0     192.168.123.1     00-1c-02-63-71-62 4.14
192.168.123.166   255.255.255.0     192.168.123.1     00-1c-02-63-71-62 4.14
192.168.123.166   255.255.255.0     192.168.123.1     00-1c-02-63-71-62 4.14
192.168.123.166   255.255.255.0     192.168.123.1     00-1c-02-63-71-62 4.14
192.168.123.166   255.255.255.0     192.168.123.1     00-1c-02-63-71-62 4.14
Test has PASSED
skip@dell-790:~/pano/pano_progfpga$ ./progfpga 192.168.123.166
Running Test = flashlatestfpga
Client connected : IP Addr = 192.168.123.166:8321
READ CFG reg 0: 0x00050000
TESTING with board Type = RevC
FPGA Major Rev = 0004, Minor Rev = 000e
S3E(RevC) board
Initializing DDR...
READ DATA @ 0x2009: 0x00000032
READ DATA @ 0x2005: 0x00000000
READ DATA @ 0x200a: 0x00000000
READ DATA @ 0x200b: 0x00000032
READ DATA @ 0x200c: 0x00000000
READ DATA @ 0x200d: 0x00000000
READ DATA @ 0x200e: 0x00000000
READ DATA @ 0x200f: 0x00000000
READ DATA @ 0x2004: 0x00000000
READ DATA @ 0x2008: 0x00000000
DDR init done
Detected FPGA with new SPI flash controller.SPI ERASE SECTOR 00000000
SPI ERASE SECTOR 00004000
SPI ERASE SECTOR 00008000
SPI ERASE SECTOR 0000c000
SPI ERASE SECTOR 00010000
SPI ERASE SECTOR 00014000
SPI ERASE SECTOR 00018000
SPI ERASE SECTOR 0001c000
SPI ERASE SECTOR 00020000
SPI ERASE SECTOR 00024000
SPI ERASE SECTOR 00028000
SPI ERASE SECTOR 0002c000
SPI ERASE SECTOR 00030000
SPI ERASE SECTOR 00034000
Quick Erase Check: ..............PASSED
Image Programing
Image had 186553 words, last address written 0x0002d8b8
reading flash & verifing
Image Verified
Done Programming - power cycle to take effect
Test has PASSED
Disconnecting audio...
Disconnected
skip@dell-790:~/pano/pano_progfpga$
```



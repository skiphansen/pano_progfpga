## Series2 (G2) Pano Logic devices

The second generation device has a SPI flash chip that is large enough to 
hold two complete bitstreams, the main bitstream and a backup.  This allows 
the device to automatically recover from unexpected errors or power 
failures while updating the main image.  (Note: Xilinx documentation 
refers to the main and backup bitstream are referred as the multiboot 
golden bitstreams)

## Differences between series 2 revisions

The first two revisions (A and B) of the G2 used the Spartan-6 LX150 
which is the largest chip in the family while the last revision (C) used 
the smaller LX100 chip.  Correspondingly the A and B revisions used a 16 megabyte 
SPI chip (M25P128) while the revisions C used a 8 megabyte (M25P64) chip.

Since the bitstreams are embedded into progfpga there are 4 different 
versions of progfpga depending on the desired bitstream and the revision of 
the target Pano.  Even though there are separate programs for each 
bitstream type progfpga still requires a command line argument to specify
the bitstream type.  To simplify usage patch_progfpga creates an script 
to execute the proper executable with the correct arguments.

In order for patch_progfpga to select the proper progfpga to 
patch it is necessary to specify both the device revision and bitstream type on 
the command line.  

When programming a revision A or B G2 specify '-2' on 
then command line, for a revision C G2 specify '-c'.  

To program the main bitstream specify '-m' on then command line,
for the backup bitstream specify '-g'.  

| Revision |    Bitstream     |   Command line     |
|----------|------------------|--------------------|
| A or B   | main (multiboot) | ./patch_fpga -2 -m |
|   C      | main (multiboot) | ./patch_fpga -c -m |
| A or B   | backup (golden)  | ./patch_fpga -2 -g |
|   C      | backup (golden)  | ./patch_fpga -c -g |


## Patching progfpga

* Checkout the pano_progfpga repository.
* CD into pano_progfpga.
* Build the patch program.
* Run patch_progfpga giving agrements specifying type of Pano which will 
be updated and the path to the binary Xilinx image file to embed into 
progfpga.  

For example to flash a revision B G2 with a new main image:

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
skip@dell-790:~/pano/pano_progfpga$ ./patch_progfpga -2 -m ../panologic-g2/spinal/ise/Pano.bin
patched/series2/lx150/progfpga_multiboot updated successfully with ../panologic-g2/spinal/ise/Pano.bin
progfpga script created successfully.

To flash series2 Rev A/B with the default bitstream:
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
192.168.123.134   255.255.255.0     192.168.123.1     00-1c-02-70-41-72 2049.24           
192.168.123.134   255.255.255.0     192.168.123.1     00-1c-02-70-41-72 2049.24           
192.168.123.134   255.255.255.0     192.168.123.1     00-1c-02-70-41-72 2049.24           
192.168.123.134   255.255.255.0     192.168.123.1     00-1c-02-70-41-72 2049.24           
192.168.123.134   255.255.255.0     192.168.123.1     00-1c-02-70-41-72 2049.24           
Test has PASSED 
skip@dell-790:~/pano/pano_progfpga$ ./progfpga 192.168.123.134
Program series2 Rev A/B with the default image y/n ?
y
Running Test = flashFPGA_Series2_Multiboot
Client connected : IP Addr = 192.168.123.134:8321
READ CFG reg 0: 0x08010000
TESTING with board Type = SERIES_II
FPGA Major Rev = 0801, Minor Rev = 0018 
SPI ERASE SECTOR 00120000 
Flash type : LX150look for lx150/multiboot.9nimgSPI ERASE SECTOR 00124000 
SPI ERASE SECTOR 00128000 
SPI ERASE SECTOR 0012c000 
SPI ERASE SECTOR 00130000 
SPI ERASE SECTOR 00134000 
SPI ERASE SECTOR 00138000 
SPI ERASE SECTOR 0013c000 
SPI ERASE SECTOR 00140000 
SPI ERASE SECTOR 00144000 
SPI ERASE SECTOR 00148000 
SPI ERASE SECTOR 0014c000 
SPI ERASE SECTOR 00150000 
SPI ERASE SECTOR 00154000 
SPI ERASE SECTOR 00158000 
SPI ERASE SECTOR 0015c000 
SPI ERASE SECTOR 00160000 
SPI ERASE SECTOR 00164000 
SPI ERASE SECTOR 00168000 
SPI ERASE SECTOR 0016c000 
SPI ERASE SECTOR 00170000 
SPI ERASE SECTOR 00174000 
SPI ERASE SECTOR 00178000 
SPI ERASE SECTOR 0017c000 
SPI ERASE SECTOR 00180000 
SPI ERASE SECTOR 00184000 
SPI ERASE SECTOR 00188000 
SPI ERASE SECTOR 0018c000 
SPI ERASE SECTOR 00190000 
SPI ERASE SECTOR 00194000 
SPI ERASE SECTOR 00198000 
SPI ERASE SECTOR 0019c000 
SPI ERASE SECTOR 001a0000 
SPI ERASE SECTOR 001a4000 
SPI ERASE SECTOR 001a8000 
SPI ERASE SECTOR 001ac000 
SPI ERASE SECTOR 001b0000 
SPI ERASE SECTOR 001b4000 
SPI ERASE SECTOR 001b8000 
SPI ERASE SECTOR 001bc000 
SPI ERASE SECTOR 001c0000 
SPI ERASE SECTOR 001c4000 
SPI ERASE SECTOR 001c8000 
SPI ERASE SECTOR 001cc000 
SPI ERASE SECTOR 001d0000 
SPI ERASE SECTOR 001d4000 
SPI ERASE SECTOR 001d8000 
SPI ERASE SECTOR 001dc000 
SPI ERASE SECTOR 001e0000 
SPI ERASE SECTOR 001e4000 
SPI ERASE SECTOR 001e8000 
SPI ERASE SECTOR 001ec000 
SPI ERASE SECTOR 001f0000 
SPI ERASE SECTOR 001f4000 
SPI ERASE SECTOR 001f8000 
SPI ERASE SECTOR 001fc000 
SPI ERASE SECTOR 00200000 
SPI ERASE SECTOR 00204000 
SPI ERASE SECTOR 00208000 
SPI ERASE SECTOR 0020c000 
SPI ERASE SECTOR 00210000 
SPI ERASE SECTOR 00214000 
SPI ERASE SECTOR 00218000 
SPI ERASE SECTOR 0021c000 
SPI ERASE SECTOR 00220000 
SPI ERASE SECTOR 00224000 
SPI ERASE SECTOR 00228000 
SPI ERASE SECTOR 0022c000 
Erase took 204.651474 seconds
finish writing addr=0x00123fc0, 0x00004000 words and 1 sectors
finish writing addr=0x00127fc0, 0x00008000 words and 2 sectors
finish writing addr=0x0012bfc0, 0x0000c000 words and 3 sectors
finish writing addr=0x0012ffc0, 0x00010000 words and 4 sectors
finish writing addr=0x00133fc0, 0x00014000 words and 5 sectors
finish writing addr=0x00137fc0, 0x00018000 words and 6 sectors
finish writing addr=0x0013bfc0, 0x0001c000 words and 7 sectors
finish writing addr=0x0013ffc0, 0x00020000 words and 8 sectors
finish writing addr=0x00143fc0, 0x00024000 words and 9 sectors
finish writing addr=0x00147fc0, 0x00028000 words and 10 sectors
finish writing addr=0x0014bfc0, 0x0002c000 words and 11 sectors
finish writing addr=0x0014ffc0, 0x00030000 words and 12 sectors
finish writing addr=0x00153fc0, 0x00034000 words and 13 sectors
finish writing addr=0x00157fc0, 0x00038000 words and 14 sectors
finish writing addr=0x0015bfc0, 0x0003c000 words and 15 sectors
finish writing addr=0x0015ffc0, 0x00040000 words and 16 sectors
finish writing addr=0x00163fc0, 0x00044000 words and 17 sectors
finish writing addr=0x00167fc0, 0x00048000 words and 18 sectors
finish writing addr=0x0016bfc0, 0x0004c000 words and 19 sectors
finish writing addr=0x0016ffc0, 0x00050000 words and 20 sectors
finish writing addr=0x00173fc0, 0x00054000 words and 21 sectors
finish writing addr=0x00177fc0, 0x00058000 words and 22 sectors
finish writing addr=0x0017bfc0, 0x0005c000 words and 23 sectors
finish writing addr=0x0017ffc0, 0x00060000 words and 24 sectors
finish writing addr=0x00183fc0, 0x00064000 words and 25 sectors
finish writing addr=0x00187fc0, 0x00068000 words and 26 sectors
finish writing addr=0x0018bfc0, 0x0006c000 words and 27 sectors
finish writing addr=0x0018ffc0, 0x00070000 words and 28 sectors
finish writing addr=0x00193fc0, 0x00074000 words and 29 sectors
finish writing addr=0x00197fc0, 0x00078000 words and 30 sectors
finish writing addr=0x0019bfc0, 0x0007c000 words and 31 sectors
finish writing addr=0x0019ffc0, 0x00080000 words and 32 sectors
finish writing addr=0x001a3fc0, 0x00084000 words and 33 sectors
finish writing addr=0x001a7fc0, 0x00088000 words and 34 sectors
finish writing addr=0x001abfc0, 0x0008c000 words and 35 sectors
finish writing addr=0x001affc0, 0x00090000 words and 36 sectors
finish writing addr=0x001b3fc0, 0x00094000 words and 37 sectors
finish writing addr=0x001b7fc0, 0x00098000 words and 38 sectors
finish writing addr=0x001bbfc0, 0x0009c000 words and 39 sectors
finish writing addr=0x001bffc0, 0x000a0000 words and 40 sectors
finish writing addr=0x001c3fc0, 0x000a4000 words and 41 sectors
finish writing addr=0x001c7fc0, 0x000a8000 words and 42 sectors
finish writing addr=0x001cbfc0, 0x000ac000 words and 43 sectors
finish writing addr=0x001cffc0, 0x000b0000 words and 44 sectors
finish writing addr=0x001d3fc0, 0x000b4000 words and 45 sectors
finish writing addr=0x001d7fc0, 0x000b8000 words and 46 sectors
finish writing addr=0x001dbfc0, 0x000bc000 words and 47 sectors
finish writing addr=0x001dffc0, 0x000c0000 words and 48 sectors
finish writing addr=0x001e3fc0, 0x000c4000 words and 49 sectors
finish writing addr=0x001e7fc0, 0x000c8000 words and 50 sectors
finish writing addr=0x001ebfc0, 0x000cc000 words and 51 sectors
finish writing addr=0x001effc0, 0x000d0000 words and 52 sectors
finish writing addr=0x001f3fc0, 0x000d4000 words and 53 sectors
finish writing addr=0x001f7fc0, 0x000d8000 words and 54 sectors
finish writing addr=0x001fbfc0, 0x000dc000 words and 55 sectors
finish writing addr=0x001fffc0, 0x000e0000 words and 56 sectors
finish writing addr=0x00203fc0, 0x000e4000 words and 57 sectors
finish writing addr=0x00207fc0, 0x000e8000 words and 58 sectors
finish writing addr=0x0020bfc0, 0x000ec000 words and 59 sectors
finish writing addr=0x0020ffc0, 0x000f0000 words and 60 sectors
finish writing addr=0x00213fc0, 0x000f4000 words and 61 sectors
finish writing addr=0x00217fc0, 0x000f8000 words and 62 sectors
finish writing addr=0x0021bfc0, 0x000fc000 words and 63 sectors
finish writing addr=0x0021ffc0, 0x00100000 words and 64 sectors
Writing the Start writing multiboot image! consumed 85.662704 seconds
reading flash & verifing 
Test has PASSED 
Disconnecting audio...
Start writing multiboot image! Image Verified, validation consumed 232.459595 seconds
Disconnected
^C
skip@dell-790:~/pano/pano_progfpga$
```

## FAQ

* Q: Which bitstream should I program?
* A: In almost all cases you'll want to program the main/multiboot bitstream.
     This is the bitstream that is loaded on power up.  I recommend that you
     **NOT** program the backup/golden image.

* Q: Is is possible to "brick" my Pano?

* A: That depends on your definition of a "brick".  It should always be 
     possible to recover a Pano that has been flashed with an incorrect or with 
     an undesiredable bitstream using a JTAG programmer, but it will not be 
     possible to reflash a Pano without a JTAG programmer unless the flashed 
     bitstream provides a way to execute the stock Pano backup (golden) image 
     **and** the backup image is still present in flash.  
  
* Q: Why is it so slow?
  A: The Pano code only programs one word a time and the flash isn't all that
     fast to start with. 

* Q: How can I tell what generation my devices is?
  A: If your device has a VGA connector then it is a Series 1 (G1).
     If your device has a DVI connector then it is a Series 2 (G2).
     
* Q: How can I tell what revision my devices is?
  A: The device revision should be printed on the same stick on label as the 
     MAC address.  

* Q: How do I update the Pano after I flash my bitstream?
  A: See [g2_multiboot](./g2_multiboot) for an example of how to reconfigure
     the device with the golden bitstream.  If JTAG is available the example
     can be loaded via JTAG, it will reconfigure the device with the golden
     bitstream when the Pano button is pressed.

* Q: I programmed the golden bitstream but when I power cycle it's still the Pano
     bitstream.
  A: Correct!  The "multiboot" bitstream image is loaded at power up the golden
     bitstream is only loaded if the "mutiboot" bitstream fails to load.

## Series 2 rev A/B SPI memory map

The minimum erase size is 128k bytes.

The size of an uncompressed xc6ls150 bitstream size is 4220212 (0x406534) 
bytes or 33 erase sectors.  

|     Byte address     |        usage        | size     | notes |
|----------------------|---------------------|----------|-------|
| 0x000000 -> 0x000034 | multiboot header    | 52 bytes |       |
| 0x000035 -> 0x01ffff | unused              | 127k     |   1   |
| 0x020000 -> 0x03ffff | unused              | 128k     |       |
| 0x040000 -> 0x446534 | golden bitstream    | 4122k    |       |
| 0x446535 -> 0x45ffff | unused              | 102k     |   2   |
| 0x460000 -> 0x47ffff | unused              | 128k     |       |
| 0x480000 -> 0x886534 | multiboot bitstream | 4122k    |       |
| 0x886535 -> 0x89ffff | unused              | 102k     |   3   |
| 0x8a0000 -> 0xffffff | unused/unknown      | 7552     |   4   |

Notes:
1. This is located within the multiboot header erase sector.
2. This is located within a golden bitstream erase sector.
3. This is located within a multiboot bitstream erase sector.
4. This region MAY contain non bitstream information such as the MAC address.


## Series 2 rev C SPI memory map

The minimum erase size is 64k bytes.

The size of an uncompressed xc6ls150 bitstream size is 3317908 (0x32A094) bytes
bytes or 51 erase sectors.  

|     Byte address     |        usage        | size     | notes |
|----------------------|---------------------|----------|-------|
| 0x000000 -> 0x000034 | multiboot header    | 52 bytes |       |
| 0x000035 -> 0x00ffff | unused              | 63k      |   1   |
| 0x020000 -> 0x03ffff | unused              | 128k     |       |
| 0x040000 -> 0x36a094 | golden bitstream    | 3240k    |       |
| 0x36a095 -> 0x36ffff | unused              | 23k      |   2   |
| 0x370000 -> 0x37ffff | unused              | 64k      |       |
| 0x380000 -> 0x6aa094 | multiboot bitstream | 3240k    |       |
| 0x6aa094 -> 0x6affff | unused              | 23k      |   3   |
| 0x6b0000 -> 0x7fffff | unused/unknown      | 1334k    |   4   |

Notes:
1. This is located within the multiboot header erase sector.
2. This is located within a golden bitstream erase sector.
3. This is located within a multiboot bitstream erase sector.
4. This region MAY contain non bitstream information such as the MAC address.

## Random thoughts about a future bootloader for G2 devices

As can be seen from the memory map above there is enough room for 3 complete 
bitstreams in the G2 flash.  This makes it possible for an application to 
allow a user to select between two completely different applications as 
well a the orignal Pano bitstream without reflashing.  

However if we're more clever it might be possible to store considerably 
more than 3 bitstreams if we consider the following: 

The uncompressed xc6ls150 bitstream with trivial complexity size of 
4220212 is reduced to 1152184 bytes when bitstream compression is enabled 
in ISE.  This is essentially free, since such a compressed bitstream can 
be loaded by the Xilinx without any additional processing.  

The size of the same bitstream size is only 66029 when gziped.  If we were 
to store gzip'ed bitstream in flash it would be possible to store a lot 
more bitstreams but code running on a softcore would have to decompress 
the bitstream and write it into flash in order for the Xilinx to be able 
to load it.  



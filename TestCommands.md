# Test Commands Available Progfpga

The progfpga_multiboot and progfpga_golden versions of progfpga also include a 
large number of test command which can be run with a command line in the format of 
```
./series2/lx150//progfpga_golden <Pano IP address> <test name>.
```


A version of the source from some/most/all of these test appear to be in [tnptest.cc](https://github.com/skiphansen/pano_progfpga/blob/master/archaeology/tnptest.cc)

| Test | Function |
| - | - | 
|ddrDbgTest||
|avgAccessTimeTest||
|ddrDbgWritePattern||
|ddrWriteTest||
|ddrImgStressTest||
|usbBasic||
|usbMultiWord||
|usbMfgTest||
|usbDevBasic||
|usbIntrBasic||
|i2cBasic||
|i2cEdidTest| Dump EDID table of connected monitor |
|setMacAddr||
|setSpiLock||
|spiBasicRd||
|spiEraseChk||
|getMacAddr||
|audioBasic||
|audioCont||
|audioMax||
|audioContInit||
|audioWolfLpbk||
|audioTnpLpbk||
|audioExtLpbk||
|audioInOnly||
|vgaBasic||
|vgaTestDdr||
|vgaScanout||
|vgaBorderTest||
|vgaPatternTest||
|dhcpStart||
|discover||
|attoButton||
|oobBasic||
|resetOutTest||
|randnumgen||
|chipIdRd| Read and display Board type, FPGA Major Major and Minor Rev |
|flashfpga||
|flash_fpga_series_2||
|flashlatestfpga||
|flashFPGA_Series2_Golden||
|flashFPGA_Series2_Multiboot||
|fpgaverify||
|spiPStat||
|ledTest||
|ledGreenBlue| Start LED alternating beteen green and blue |
|spiledburn||
|buttontest||
|audioButtonTest||
|spiUserErase||
|spiledbutt||

## test chipIdRd

The _chipIdRd_ command is extremely useful allows the board generation and revision to be determined interactively.

For example:

This is a rev B Pano G2:
```
Running Test = chipIdRd
Client connected : IP Addr = 192.168.123.118:8321
READ CFG reg 0: 0x08010000
TESTING with board Type = SERIES_II
FPGA Major Rev = 0801, Minor Rev = 0014
READ @ 00000001: 0x08010014
READ @ 00000010: 0x0000001c
...
```

This is a rev B Pano G2:
```
Running Test = chipIdRd
Client connected : IP Addr = 192.168.123.206:8321
READ CFG reg 0: 0x08010002
TESTING with board Type = SERIES_II
FPGA Major Rev = 0881, Minor Rev = 0016
READ @ 00000001: 0x08810016
READ @ 00000010: 0x0000001c
...
```

And this is a DZ22-2:
```
Running Test = chipIdRd
Client connected : IP Addr = 192.168.123.158:8321
READ CFG reg 0: 0x08011000
TESTING with board Type = SERIES_II
FPGA Major Rev = 0801, Minor Rev = 0014
READ @ 00000001: 0x08010014
READ @ 00000010: 0x00000019
...
```

## test i2cEdidTest

The test reads the EIDD information of an attached monitor via I2C and dumps
the contents.

For example this is the results a DZ22-2

```
Running Test = i2cEdidTest
Client connected : IP Addr = 192.168.123.158:8321
READ CFG reg 0: 0x08011000
TESTING with board Type = SERIES_II
FPGA Major Rev = 0801, Minor Rev = 0014
I2C Clean Up complete
I2C READ EDID Table @ offset 00000000: 0xffffff00
I2C READ EDID Table @ offset 00000004: 0x00ffffff
I2C READ EDID Table @ offset 00000008: 0x0783b31a
I2C READ EDID Table @ offset 0000000c: 0x00000001
I2C READ EDID Table @ offset 00000010: 0x0301151b
I2C READ EDID Table @ offset 00000014: 0x781e2f80
I2C READ EDID Table @ offset 00000018: 0xa065c62a
I2C READ EDID Table @ offset 0000001c: 0x279d5859
I2C READ EDID Table @ offset 00000020: 0x0154500e
I2C READ EDID Table @ offset 00000024: 0x00810008
I2C READ EDID Table @ offset 00000028: 0x01010095
I2C READ EDID Table @ offset 0000002c: 0x01010101
I2C READ EDID Table @ offset 00000030: 0x01010101
I2C READ EDID Table @ offset 00000034: 0x39210101
I2C READ EDID Table @ offset 00000038: 0x1a623090
I2C READ EDID Table @ offset 0000003c: 0xb0684027
I2C READ EDID Table @ offset 00000040: 0x28da0036
I2C READ EDID Table @ offset 00000044: 0x1c000011
I2C READ EDID Table @ offset 00000048: 0xfd000000
I2C READ EDID Table @ offset 0000004c: 0x1f3d3b00
I2C READ EDID Table @ offset 00000050: 0x0a001051
I2C READ EDID Table @ offset 00000054: 0x20202020
I2C READ EDID Table @ offset 00000058: 0x00002020
I2C READ EDID Table @ offset 0000005c: 0x4400fc00
I2C READ EDID Table @ offset 00000060: 0x2d32325a
I2C READ EDID Table @ offset 00000064: 0x20200a32
I2C READ EDID Table @ offset 00000068: 0x20202020
I2C READ EDID Table @ offset 0000006c: 0xff000000
I2C READ EDID Table @ offset 00000070: 0x34565900
I2C READ EDID Table @ offset 00000074: 0x3030304b
I2C READ EDID Table @ offset 00000078: 0x0a313030
I2C READ EDID Table @ offset 0000007c: 0xf6002020
Test has PASSED
```


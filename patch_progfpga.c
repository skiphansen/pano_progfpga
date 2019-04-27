/*
 *  patch_progfpga
 *
 *  Copyright (C) 2019  Skip Hansen
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms and conditions of the GNU General Public License,
 *  version 2, as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define OPTION_STRING      "12cgm"

/* 
 
Decompiled flashFPGA_Series2_Golden routine extracted by Ghidra: 
 
RtnStatus_t flashFPGA_Series2_Golden(void *param_1)
{
  uint32_t uVar1;
  uint uVar2;
  RtnStatus_t RVar3;
  RtnStatus_t local_28;
  uint32_t sectorSize_golden;
  uint32_t goldenSize;
  uint32_t spi_addr_golden;
  uint32_t sectorSize_header;
  uint32_t headerSize;
  uint32_t spi_addr_header;
  uint32_t flashType;
  uint32_t Rev;
  
  uVar1 = configRead(ep0Client,1);
  uVar2 = (uVar1 & 0x3800000) >> 0x17;
  if (uVar2 == 0) {
    log_severe("Flash type : LX150\n");
    log_severe("look for lx150/header.9nimg");
    goldenSize = 0x10ffff;
    sectorSize_golden = 0x43;
    log_severe("look for lx150/golden.9nimg\n");
  }
  else {
    if (uVar2 != 1) {
      log_severe("Flash type : UNKNOWN\n");
      return TEST_FAIL;
    }
    log_severe("Flash type : LX100\n");
    log_severe("look for lx100/header.9nimg\n");
    goldenSize = 0xcffff;
    sectorSize_golden = 0x33;
    log_severe("look for lx100/golden.9nimg\n");
  }
  RVar3 = writeAndValidateImage
                    ("Start writing golden header!",(uint32_t *)latestFpgaHeader,0,0xffff,3);
  if (RVar3 == TEST_FAIL) {
    local_28 = TEST_FAIL;
  }
  else {
    RVar3 = writeAndValidateImage
                      ("Start writing golden image!",(uint32_t *)latestFpgaImage,0x10000,goldenSize,
                       sectorSize_golden);
    if (RVar3 == TEST_FAIL) {
      local_28 = TEST_FAIL;
    }
    else {
      log_severe("Done Programming - power cycle to take effect\n");
      write(ep0Client,0x8086,0x7ff);
      write(ep0Client,0x8087,0x1ff800);
      write(ep0Client,0x8013,0xc);
      local_28 = TEST_OK;
    }
  }
  return local_28;
} 
 
Decompiled flashFPGA_Series2_Golden routine extracted by Ghidra: 
 
RtnStatus_t flashFPGA_Series2_Multiboot(void *param_1)
{
  uint32_t uVar1;
  uint uVar2;
  RtnStatus_t RVar3;
  RtnStatus_t local_20;
  uint32_t sectorSize;
  uint32_t imageSize;
  uint32_t spi_addr;
  uint32_t spi_dat;
  uint32_t flashType;
  uint32_t Rev;
  
  uVar1 = configRead(ep0Client,1);
  uVar2 = (uVar1 & 0x3800000) >> 0x17;
  if (uVar2 == 0) {
    log_severe("Flash type : LX150");
    spi_addr = 0x120000;
    imageSize = 0x10ffff;
    sectorSize = 0x43;
    log_severe("look for lx150/multiboot.9nimg");
  }
  else {
    if (uVar2 != 1) {
      log_severe("Flash type : UNKNOWN");
      return TEST_FAIL;
    }
    log_severe("Flash type : LX100");
    spi_addr = 0xe0000;
    imageSize = 0xcffff;
    sectorSize = 0x33;
    log_severe("look for lx100/multiboot.9nimg");
  }
  RVar3 = writeAndValidateImage
                    ("Start writing multiboot image!",(uint32_t *)latestFpgaMultiBoot,spi_addr,
                     imageSize,sectorSize);
  if (RVar3 != TEST_FAIL) {
    write(ep0Client,0x8086,0x7ff);
    write(ep0Client,0x8087,0x1ff800);
    write(ep0Client,0x8013,0xc);
  }
  local_20 = (RtnStatus_t)(RVar3 == TEST_FAIL);
  return local_20;
}
 
 
-rwxrwxr-x 1 skip skip  9180426 Apr 10 18:11 series1/progfpga 
skip@dell-790:~/pano/fpgaImage$ objdump -h series1/progfpga | grep '\.data '
 24 .data         001796d4  08143c00  08143c00  000fbc00  2**5
skip@dell-790:~/pano/fpgaImage$ objdump -t series1/progfpga | grep latestFpgaImage
08143c40 g     O .data  0016c5d0              latestFpgaImage
 
-rwxrwxr-x 1 skip skip 16144893 Apr 10 18:11 series2/lx150/progfpga_golden
skip@dell-790:~/pano/fpgaImage$ objdump -h series2/lx150/progfpga_golden | grep '\.data '
 24 .data         00819ab4  08146c00  08146c00  000fec00  2**5
skip@dell-790:~/pano/fpgaImage$ objdump -t series2/lx150/progfpga_golden | grep latestFpgaImage
08146ca0 g     O .data  0080ca68              latestFpgaImage
 
-rwxrwxr-x 1 skip skip 16142365 Apr 10 18:11 series2/lx150/progfpga_multiboot 
skip@dell-790:~/pano/fpgaImage$ objdump -h series2/lx150/progfpga_multiboot | grep '\.data '
 24 .data         00819a54  08147280  08147280  000fe280  2**5
skip@dell-790:~/pano/fpgaImage$ objdump -t series2/lx150/progfpga_multiboot | grep latestFpgaMultiBoot
081472c0 g     O .data  0080ca68              latestFpgaMultiBoot
 
-rwxr-xr-x 1 skip skip 14337853 Apr 10 18:11 series2/lx100/progfpga_golden
skip@dell-790:~/pano/fpgaImage$ objdump -h series2/lx100/progfpga_golden | grep '\.data '
 24 .data         00661174  08147280  08147280  000fe280  2**5
skip@dell-790:~/pano/fpgaImage$ objdump -t series2/lx100/progfpga_golden | grep latestFpgaImage
08147320 g     O .data  00654128              latestFpgaImage
 
-rwxr-xr-x 1 skip skip 14340189 Apr 10 18:11 series2/lx100/progfpga_multiboot 
skip@dell-790:~/pano/fpgaImage$ objdump -h series2/lx100/progfpga_multiboot | grep '\.data '
 24 .data         00661114  08146c00  08146c00  000fec00  2**5
skip@dell-790:~/pano/fpgaImage$ objdump -t series2/lx100/progfpga_multiboot | grep latestFpgaMultiBoot
08146c40 g     O .data  00654128              latestFpgaMultiBoot 
 
Type        .bin size 
XC6SLX100   3317908 
XC6SLX150   4220212
*/ 

struct {
   const char *Desc;
   const char *ProgFpgaPath;
   const char *ProgCmd;
   int ExecutableSize;
   int ImageOffset;
   int ImageSize;
   int StartAdr;
} PatchData[] = 
{ 
   {  "series1",
      "patched/series1/progfpga",
      "",
      9180426,
      (0x8143c40 - 0x08143c00 + 0x000fbc00),
      (0x0016c5d0 / 2) - 4,
      0
   },
   {  "series2 Rev A/B with the default bitstream",
      "patched/series2/lx150/progfpga_multiboot",
      "flashFPGA_Series2_Multiboot",
      16142365,
      (0x081472c0 - 0x08147280+ 0x000fe280),
      0x0080ca68 / 2
   },
   {  "series2 Rev A/B with the golden bitstream",
      "patched/series2/lx150/progfpga_golden",
      "flashFPGA_Series2_Golden",
      16144893,
      (0x08146ca0 - 0x08146c00 + 0x000fec00),
      0x0080ca68 / 2
   },
   {  "series2 Rev C with the default bitstream",
      "patched/series2/lx100/progfpga_multiboot",
      "flashFPGA_Series2_Multiboot",
      14340189,
      (0x08146c40 - 0x08146c00 + 0x000fec00),
      0x00654128 / 2
   },
   {  "series2 Rev C with the golden bitstream",
      "patched/series2/lx100/progfpga_golden",
      "flashFPGA_Series2_Golden",
      14337853,
      (0x08147320 - 0x08147280 + 0x000fe280),
      0x00654128 / 2
   }
};

enum {
   TYPE_SERIES_1,
   TYPE_SERIES_2,
   TYPE_SERIES_2_GOLDEN,
   TYPE_SERIES_2C,
   TYPE_SERIES_2C_GOLDEN
};

int CreateScript(int BurnType);
int WriteStrings2Script(FILE *fp,const char **Array);
void Usage(void);

int main(int argc, char **argv)
{
   uint32_t Value;
   uint32_t Value1;
   uint32_t Adr = 0;
   uint8_t *cp = (uint8_t *) &Value;
   uint8_t *cp1 = (uint8_t *) &Value1;
   FILE *fp = NULL;
   FILE *fOut = NULL;
   int BurnType = -1;
   int bDisplayUsage = 1;
   int bGolden = 0;
   int bMultiboot = 0;
   struct stat Stat;
   int Ret = 0;
   int Option;
   int FilenameArg = 1;
   const char *ProgFpgaPath;
   const char *ImageFilePath;

   while(Ret == 0 && (Option = getopt(argc, argv,OPTION_STRING)) != -1) {
      FilenameArg++;
      switch(Option) {
         case '1':
            if(BurnType != -1) {
               Ret = EINVAL;
            }
            else {
               BurnType = TYPE_SERIES_1;
            }
            break;

         case '2':
            if(BurnType != -1) {
               Ret = EINVAL;
            }
            else {
               BurnType = TYPE_SERIES_2;
            }
            break;

         case 'c':
            if(BurnType != -1) {
               Ret = EINVAL;
            }
            else {
               BurnType = TYPE_SERIES_2C;
            }
            break;

         case 'g':
            bGolden = 1;
            break;

         case 'm':
            bMultiboot = 1;
            break;
      }
   }

   if(Ret == 0) do {
      if(FilenameArg != argc - 1) {
         break;
      }
      if(BurnType == -1) {
         printf("Error: Pano device type not specified.\n");
         Ret = EINVAL;
      }

      if(bGolden) {
         if(BurnType == TYPE_SERIES_1) {
            printf("Error: series 1 devices don't have a golden bitstream.\n");
            Ret = EINVAL;
            break;
         }
         BurnType++;
      }
      else if(!bMultiboot && BurnType != TYPE_SERIES_1) {
         printf("Error: bitstream type not specified.\n");
         Ret = EINVAL;
         break;
      }
      bDisplayUsage = 0;
      ProgFpgaPath = PatchData[BurnType].ProgFpgaPath;
      ImageFilePath = argv[FilenameArg];

      if(stat(ImageFilePath,&Stat)) {
         printf("Error: %s not found\n",ImageFilePath);
         Ret = errno;
         break;
      }

      if(PatchData[BurnType].ImageSize < Stat.st_size) {
         printf("Error incorrect FPGA bin file size (%lu), "
                "it must be %u bytes or smaller.\n",Stat.st_size,
                PatchData[BurnType].ImageSize);
         printf("NOTE: A .bit file is NOT the right format for this tool.\n");
         Ret = EINVAL;
         break;
      }

      if((fp = fopen(ImageFilePath,"r")) == NULL) {
         printf("Error: couldn't open %s - %s\n",ImageFilePath,
                strerror(errno));
         Ret = errno;
         break;
      }

      if(stat(ProgFpgaPath,&Stat)) {
         printf("Error: %s not found\n",ProgFpgaPath);
         Ret = errno;
         break;
      }

      if(PatchData[BurnType].ExecutableSize != Stat.st_size) {
         printf("Error: %s is not the expected size (%u != %lu)\n",
                ProgFpgaPath,PatchData[BurnType].ExecutableSize,Stat.st_size);
         break;
      }

      if((fOut = fopen(ProgFpgaPath,"r+")) == NULL) {
         printf("Error: couldn't open %s - %s\n",ProgFpgaPath,strerror(errno));
         break;
      }

      if(fseek(fOut,PatchData[BurnType].ImageOffset,SEEK_SET) != 0) {
         printf("fseek failed: %s\n",strerror(errno));
         break;
      }

      while(!feof(fp)) {
         if(fread(&Value,sizeof(Value),1,fp) != 1) {
            if(!feof(fp)) {
               printf("fread failed: %s\n",strerror(errno));
               Ret = errno;
            }
            else {
               printf("%s updated successfully with %s\n",ProgFpgaPath,
                      argv[FilenameArg]);
            }
            break;
         }
      // Reverse the byte order
         cp1[0] = cp[3];
         cp1[1] = cp[2];
         cp1[2] = cp[1];
         cp1[3] = cp[0];
         if(fwrite(&Adr,sizeof(Adr),1,fOut) != 1) {
            printf("fwrite failed: %s\n",strerror(errno));
            Ret = errno;
            break;
         }
         if(fwrite(&Value1,sizeof(Value1),1,fOut) != 1) {
            printf("fwrite failed: %s\n",strerror(errno));
            Ret = errno;
            break;
         }
         Adr++;
      }

      if(Ret != 0) {
         break;
      }

   // Create a script to execute the patched progfpga
      Ret = CreateScript(BurnType);
   } while(0);

   if(bDisplayUsage) {
      Usage();
   }

   if(fp != NULL) {
      fclose(fp);
   }
   if(fOut != NULL) {
      fclose(fOut);
   }
   return errno;
}

const char *BoilerPlate1[] = {
   "#!/bin/sh\n",
   "# Warning: This file was generated by patch_progfpga, any edits will be lost\n",
   "# the next time patch_progfpga is run\n\n",
   NULL
};

const char *BoilerPlate2[] = {
   "if [ $# -ne 1 ]; then\n",
   "   echo \"Usage:\"\n",
   "   echo \"  ./progfpga -d\"\n",
   "   echo \"  ./progfpga <Pano device ip address>\"\n",
   "   exit 1\n",
   "fi\n",
   "if [ $1 = \"-d\" ]; then\n",
   "   series1/progfpga -d\n",
   "   exit 0\n",
   "fi\n",
   "echo \"Program ${prog_type} y/n ?\"\n",
   "read answer\n",
   "if [ x$answer != \"xy\" ]; then\n",
   "   exit 0\n",
   "fi\n",
   NULL
};

const char *BoilerPlate3[] = {
   "sleep 1\n",
   "tail -f BurninLowLevel${1}.log\n",
   NULL
};

// Create a script to execute the patched progfpga
int CreateScript(int BurnType)
{
   FILE *fp;
   int Ret = 0;
   int i;
   struct stat Stat;
   const char *DeleteOldLog = "";

   do {
      if((fp = fopen("progfpga","w")) == NULL) {
         printf("Error: couldn't open ./progfpga for write - %s\n",
                strerror(errno));
         Ret = errno;
         break;
      }

      if((Ret = WriteStrings2Script(fp,BoilerPlate1))!= 0) {
         break;
      }
      if(fprintf(fp,"prog_type=\"%s\"\n",PatchData[BurnType].Desc) <= 0) {
         Ret = errno;
         break;
      }

      if((Ret = WriteStrings2Script(fp,BoilerPlate2))!= 0) {
         break;
      }

      if(BurnType != TYPE_SERIES_1) {
         DeleteOldLog = "rm BurninLowLevel${1}.log\n";
      }
      if(fprintf(fp,"%s./%s $1 %s%s\n",
                 DeleteOldLog,
                 PatchData[BurnType].ProgFpgaPath,
                 PatchData[BurnType].ProgCmd,
                 BurnType == TYPE_SERIES_1 ? "" : " &")  <= 0) 
      {
         Ret = errno;
         break;
      }
      if(BurnType != TYPE_SERIES_1) {
         if((Ret = WriteStrings2Script(fp,BoilerPlate3))!= 0) {
            break;
         }
      }
   } while(0);

   if(fp != NULL && Ret != 0) {
      printf("Error: fprintf failed - %s\n",strerror(errno));
   }

   if(fp != NULL) {
      fclose(fp);
      if(stat("progfpga",&Stat)) {
         printf("stat failed - %s\n",strerror(errno));
         Ret = errno;
      }
      else {
         Stat.st_mode |= S_IXUSR;
         if(chmod("progfpga",Stat.st_mode) != 0) {
            printf("chmod failed - %s\n",strerror(errno));
            Ret = errno;
         }
      }
      printf("progfpga script created successfully.\n\n");
      printf("To flash %s:\n",PatchData[BurnType].Desc);
      printf("   ./progfpga -d\n");
      printf("   ./progfpga <ip address>\n");
   }

   return Ret;
}

int WriteStrings2Script(FILE *fp,const char **Array)
{
   int i;
   int Ret = 0;
   for(i = 0; Array[i] != NULL; i++) {
      if(fprintf(fp,"%s",Array[i]) <= 0) {
         Ret = errno;
         break;
      }
   }

   return Ret;
}

void Usage()
{
   printf("usage: patch_progfpg [options] <path to Pano .bin file>\n");
   printf("\t-1\tPano Series 1 (Spartan-3E XCS3S1600E)\n");
   printf("\t-2\tPano Series 2 revision A or B (Spartan-6 XC6SLX150)\n");
   printf("\t-c\tPano Series 2 revision C (Spartan-6 XC6SLX100)\n");
   printf("\t-m\tburn multiboot (main) bitstream (series 2 devices only)\n");
   printf("\t-g\tburn golden (backup) bitstream (series 2 devices only)\n");
}


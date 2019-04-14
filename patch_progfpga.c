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

// Ghidra tells us that latestFpgaImage is located @ 0x8143c40 in the .data 
// segment
// 
// Objdump tells us that the data segment starts @ 08143c00 and is located
// @ offset 000fbc00 in the elf file
// 
// so:
#define IMAGE_OFFSET    (0x8143c40 - 0x08143c00 + 0x000fbc00)
#define IMAGE_SIZE      1492432

#define BIN_FILE_SIZE   746212
#define PROG_FPGA_SIZE  9180426

int TestImages(FILE *fOut,FILE *fIn);

int main(int argc, char **argv)
{
   uint32_t Value;
   uint32_t Value1;
   uint32_t Adr = 0;
   uint8_t *cp = (uint8_t *) &Value;
   uint8_t *cp1 = (uint8_t *) &Value1;
   FILE *fp = NULL;
   FILE *fOut = NULL;
   int bDisplayUsage = 1;

   if(argc != 2) {
   }
   else if((fp = fopen(argv[1],"r")) == NULL) {
      printf("Error: couldn't open %s - %s\n",argv[1],strerror(errno));
   }
   else if((fOut = fopen("progfpga","r+")) == NULL) {
      printf("Error: couldn't open progfpga - %s\n",strerror(errno));
   }
   else if(TestImages(fOut,fp)) {
      printf("Error: wrong version of progfpga\n");
   }
   else if(fseek(fOut,IMAGE_OFFSET,SEEK_SET) != 0) {
      printf("fseek failed: %s\n",strerror(errno));
   }
   else {
      bDisplayUsage = 0;
      while(!feof(fp)) {
         if(fread(&Value,sizeof(Value),1,fp) != 1) {
            if(!feof(fp)) {
               printf("fread failed: %s\n",strerror(errno));
            }
            else {
               printf("progfpga updated successfully with %s\n",argv[1]);
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
            break;
         }
         if(fwrite(&Value1,sizeof(Value1),1,fOut) != 1) {
            printf("fwrite failed: %s\n",strerror(errno));
            break;
         }
         Adr++;
      }
   }

   if(bDisplayUsage) {
      printf("usage: patch_progfpg <path to Pano G1 .bin file>\n");
   }

   if(fp != NULL) {
      fclose(fp);
   }
   if(fOut != NULL) {
      fclose(fOut);
   }
   return errno;
}

int TestImages(FILE *fOut,FILE *fIn)
{
   int Ret = 1;
   int Err;
   uint32_t TestBuf[3];
   struct stat Stat;

   do {
      if(fstat(fileno(fIn),&Stat) != 0) {
         printf("fstat failed: %s\n",strerror(errno));
         break;
      }

      if(Stat.st_size > BIN_FILE_SIZE) {
         printf("Error incorrect FPGA bin file size (%lu), "
                "it must be %u bytes or smaller.\n",Stat.st_size,BIN_FILE_SIZE);
         printf("NOTE: A .bit file is NOT the right format for this tool.\n");
         break;
      }
      if(fstat(fileno(fOut),&Stat) != 0) {
         printf("fstat failed: %s\n",strerror(errno));
         break;
      }

      if(Stat.st_size != PROG_FPGA_SIZE) {
         printf("Error incorrect progfpga executable file size (%lu)\n",
                Stat.st_size);
         break;
      }
      if(fseek(fOut,IMAGE_OFFSET,SEEK_SET) != 0) {
         printf("fseek failed: %s\n",strerror(errno));
         break;
      }
      if((Err = fread(TestBuf,sizeof(TestBuf),1,fOut)) != 1) {
         printf("fread failed: %s\n",strerror(errno));
         break;
      }
      if(TestBuf[0] != 0) {
         printf("Unexpected value @ offset 0x%x - Expected 0x0, is 0x%x\n",
                IMAGE_OFFSET,TestBuf[0]);
         break;
      }
      if(TestBuf[2] != 1) {
         printf("Unexpected value @ offset 0x%x - Expected 0x1, is 0x%x\n",
                IMAGE_OFFSET+8,TestBuf[2]);
         break;
      }
      if(fseek(fOut,IMAGE_OFFSET + IMAGE_SIZE - 4,SEEK_SET) != 0) {
         printf("fseek failed: %s\n",strerror(errno));
         break;
      }
      if(fread(TestBuf,sizeof(TestBuf),1,fOut) != 1) {
         printf("fread failed: %s\n",strerror(errno));
         break;
      }
      if(TestBuf[0] != 0xffffffff) {
         printf("Unexpected value @ offset 0x%x - Expected 0xffffffff, is 0x%x\n",
                IMAGE_OFFSET + IMAGE_SIZE - 4,TestBuf[0]);
      }
      Ret = 0;
   } while(0);

   return Ret;
}


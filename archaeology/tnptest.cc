
#include <unistd.h>

//#include "TnpPacket.h"
//#include "TnpClient.h"
#include "Util.h"

#include "UsbTest.h"
#include "tnptest.h"

#include "BringupVideoSettings.h" // Local Vesa/Pll Settings

#include <map>
#include <string>
#include <iostream>
#include <vector>

#include "common/ISP1760Driver.h"
#include "common/Ep0Client.h"
#include "common/VideoDisplayDriver.h"
#include "common/TnpStreamSocket.h"
#include "common/TnpDiscover.h"
#include "common/Workpool.h"
#include "TnpDiscoverTest.h"
#include "AttoButtonTest.h"

static TestList* BringupTests;
static const uint32_t AudioTnpEndpoint = 3;
Ep0Client *ep0Client;
Ep0Client *ep0ClientOob;
ISP1760Driver *ispDrv;
VideoDisplayDriver *videoDrv;
TnpStreamSocket *sStream;
//TnpDgramSocket *sDgram;
string* boardName;
RtnStatus_t gStatus = TEST_OK;
bool gDone = false;

bool debugModeUsb = false;
uint32_t audioCSRRead(uint32_t addr);

//#define TNP_DEBUG

#include "image.h"
#include "rletile.h"

// onEvent for audio_in
class EndpointInput : public TnpSocket::EndpointInput {
 public:
  EndpointInput(RtnStatus_t *status, bool *done, uint32_t num_iter = 200){status_ = status; done_ = done; num_iter_ = num_iter;}
  ~EndpointInput() {}
  
  void onEvent( const TnpSocket::SocketEvent& e);
  EndpointInput* clone() const {
    return new EndpointInput(status_, done_);
  }
  protected : 
  RtnStatus_t *status_;
  bool *done_;
  uint32_t num_iter_;

};

RtnStatus_t (*localAudioProc)(char *, uint32_t);
uint32_t audio_pkt_num = 0;

void spiWriteNF (uint32_t addr, uint32_t data) {
    EP0WRITE(0x5001, 0x80000000 | addr);
    
    EP0WRITE(0x5000, data);

    EP0FLUSH;
    usleep(2000);
}


void spiWrite (uint32_t addr, uint32_t data) {
    EP0WRITE(0x5001, 0x80000000 | addr);
    
    EP0WRITE(0x5000, data);

    EP0FLUSH;
     //  sleep(1);//(50/1000);
  
}


void spiErase (uint32_t addr) {
    EP0WRITE(0x5001, 0x80000000 | addr);
    EP0FLUSH;
    EP0WRITE(0x5002, 0x00000000); //erase
    EP0FLUSH;

    printf("SPI ERASE SECTOR %08x \n",addr);    
    sleep(3);
}



uint32_t spiRead (uint32_t addr) {

    uint32_t data;
    EP0WRITE(0x5001, 0x80000000 | addr);
    data = EP0READ(0x5000);

    return data;
}

void clkBitWrite(uint32_t bitData, int32_t i) {
    assert (bitData == 0 || bitData == 1);

    // Set the data out
    uint32_t wrData = bitData;
    EP0WRITE(0x10, wrData);
    //printf("CLK Synthesizer Data Write @ %08x: 0x%08x\n",0x10, wrData);
    
    // Toggle the clk or the strobe 
    if (i != 0) {
        wrData = bitData | (1 << 8);
        EP0WRITE(0x10, wrData);
    }
    else {
        wrData = bitData | (1 << 8);
        EP0WRITE(0x10, wrData);
        wrData = bitData | (1 << 16); // Toggle the Strobe bit.
        EP0WRITE(0x10, wrData);
    }
    printf("CLK Synthesizer Clock and Strobe Write @ %08x: 0x%08x\n",0x10, wrData);

    // Clear the clk or strobe signal
    wrData = bitData ;
    EP0WRITE(0x10, wrData);
}

void clkWordWrite (uint32_t wdData) {
    assert (!(wdData & 0xfe000000));
    for (int32_t i=24; i >= 0; i--) {
        clkBitWrite( (wdData & (1 << i)) >> i, i);
    }
}


uint32_t axtoi(const char *hexStg) {
  uint32_t n = 0;         // position in string
  uint32_t m = 0;         // position in digit[] to shift
  uint32_t count;         // loop index
  uint32_t intValue = 0;  // integer value of hex string
  uint32_t digit[5];      // hold values to convert
  while (n < 6) {
     if (hexStg[n]=='\0')
        break;
     if (hexStg[n] > 0x29 && hexStg[n] < 0x40 ) //if 0 to 9
        digit[n] = hexStg[n] & 0x0f;            //convert to int
     else if (hexStg[n] >='a' && hexStg[n] <= 'f') //if a to f
        digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
     else if (hexStg[n] >='A' && hexStg[n] <= 'F') //if A to F
        digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
     else break;
    n++;
  }
  count = n;
  m = n - 1;
  n = 0;
  while(n < count) {
     // digit[n] is value of hex digit at position n
     // (m << 2) is the number of positions to shift
     // OR the bits into return value
     intValue = intValue | (digit[n] << (m << 2));
     m--;   // adjust the position to set
     n++;   // next digit to process
  }
  return (intValue);
}
        
void spiDump() {
    uint32_t spi_addr = 0x37FFF;
    uint32_t testdat = 0x0;
    uint32_t spi_data = 0x0;    

    while(testdat != 0xffffffff){
    spi_addr++;
    testdat = spiRead(spi_addr);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, testdat);
    }


    spi_addr++;
    testdat = spiRead(spi_addr);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, testdat);

//
//    uint32_t spi_addr = 0x38000;
//    uint32_t TEST0,TEST1,TEST2,TEST3,TEST4;    
//
//    TEST0 = spiRead(spi_addr);
//    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, TEST0);
//    spi_addr++;
//    TEST1 = spiRead(spi_addr);
//    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, TEST1);
//    spi_addr++;
//    TEST2 = spiRead(spi_addr);
//    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, TEST2);
//    spi_addr++;
//    TEST3 = spiRead(spi_addr);
//    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, TEST3);
//    spi_addr++;
//    TEST4 = spiRead(spi_addr);
//    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, TEST4);
}

void chipIDrd() {
    uint32_t chipID_addr = 0x1;
    uint32_t ethLow_addr = 0x10;
    printf("READ @ %08x: 0x%08x\n",chipID_addr, EP0CFGREAD(chipID_addr));
    EP0FLUSH;
    printf("READ @ %08x: 0x%08x\n",ethLow_addr, EP0CFGREAD(ethLow_addr));
}

RtnStatus_t i2cEdidSetup() {
    
    // Cleanup register info from last transfer.
    EP0WRITE(0x6010, 0x00000000);
    EP0WRITE(0x600c, 0x00000000);
    printf("I2C Clean Up complete\n");

    //printf("Read configuration registers before writing them ...\n");
    //printf("I2C READ Address Register @ %08x: 0x%08x\n",0x6000, EP0READ(0x6000));
    //printf("I2C READ Offset Register @ %08x: 0x%08x\n",0x6004, EP0READ(0x6004));
    //printf("I2C READ Status Register @ %08x: 0x%08x\n",0x6010, EP0READ(0x6010));
    //printf("I2C READ Control Register @ %08x: 0x%08x\n",0x600C, EP0READ(0x600c));
    //printf("Configuration read of register complete!\n");

    //printf("-------------------------------------------------------\n");
    //printf("\n");
    //printf("I2C WRITE Address Register @ %08x: 0x%08x\n",0x6000, 0x00a1a060);
    EP0WRITE(0x6000, 0x00a1a060);
    //printf("I2C WRITE Offset Register @ %08x: 0x%08x\n",0x6004, 0x00000000);
    //EP0WRITE(0x6004, 0x00001300);
    //printf("I2C WRITE Status Register @ %08x: 0x%08x\n",0x6010, 0x00000000);
    //EP0WRITE(0x6010, 0x00000000);
    //printf("I2C WRITE Control Register @ %08x: 0x%08x\n",0x600c, 0x0040ff1f);
    //EP0WRITE(0x600c, 0x0043ff1f);

    /*
    printf("I2C READ Address Register @ %08x: 0x%08x\n",0x6000, EP0READ(0x6000));
    printf("I2C READ Offset Register @ %08x: 0x%08x\n",0x6004, EP0READ(0x6004));
    printf("I2C READ Status Register @ %08x: 0x%08x\n",0x6010, EP0READ(0x6010));
    printf("I2C READ Control Register @ %08x: 0x%08x\n",0x600C, EP0READ(0x600c));
    printf("I2C READ Data Register @ %08x: 0x%08x\n",0x6008, EP0READ(0x6008));
    */
    uint32_t offset = 0;
    uint32_t nBytes = 128;
    while ( offset < nBytes ) {
        EP0WRITE(0x6004, (( offset ) & 0xff ) << 8 );
        EP0WRITE( 0x600C, ( 0x4 << 20 ) | ( 0x3ff << 8 ) | ( 0x1f ));
        uint32_t rd_data = EP0READ(0x6008);
        printf("I2C READ EDID Table @ offset %08x: 0x%08x\n",offset, rd_data);
        if (offset == 0) {
            if (rd_data != 0xffffff00) {
                return TEST_FAIL;
            }
        }
        if (offset == 4) {
            if (rd_data != 0x00ffffff) {
                return TEST_FAIL;
            }
        }
        
        //printf("I2C READ EDID Table @ offset %08x: 0x%08x\n",offset, EP0READ(0x6008));
        EP0WRITE( 0x600c, 0 );
        EP0WRITE( 0x6010, 0 );
        offset += 4;
    }
    return TEST_OK;

}


#define LED_RGB(re,r,ge,g,be,b) \
                          (((b & 0xff) << 0) \
                        | ((be & 0x7) << 8) \
                        | ((g & 0x7f) << 11) \
                        | ((ge & 0x7) << 18) \
                        | ((r & 0xff) << 21) \
                        | ((re & 0x7) << 29)) 

RtnStatus_t ledGreenBlue(void* ptr) {
    EP0WRITE(0x8086,LED_RGB(0,0, 0,0, 7,255)); // Color (OFF)
    EP0WRITE(0x8087,LED_RGB(0,0, 7,255, 0,0)); // Color (ON)
    EP0WRITE(0x8013,0x0000000C); // Pattern
    return TEST_OK;
}

RtnStatus_t ledRed(void* ptr) {
    EP0WRITE(0x8086,LED_RGB(7,255, 0,0, 0,0)); // Color (OFF)
    EP0WRITE(0x8087,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8013,0x00000000); // Pattern
    return TEST_OK;
}

RtnStatus_t ledAmber(void* ptr) {
    EP0WRITE(0x8086,LED_RGB(7,99, 7,255, 0,0)); // Color (OFF)
    EP0WRITE(0x8087,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8013,0x00000000); // Pattern
    return TEST_OK;
}

RtnStatus_t ledGreen(void* ptr) {
    EP0WRITE(0x8086,LED_RGB(0,0, 7,155, 0,0)); // Color (OFF)
    EP0WRITE(0x8087,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8013,0x00000000); // Pattern
    return TEST_OK;
}

RtnStatus_t ledBlue(void* ptr) {
    EP0WRITE(0x8086,LED_RGB(0,0, 0,0, 7,255)); // Color (OFF)
    EP0WRITE(0x8087,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8013,0x00000000); // Pattern
    return TEST_OK;
}






// indicate unit is bad, failed test
// changed behavior of LED
// RED in CONNECT state
// RED in hasIP state
void ledIndicateFail() {

    // Code 0 - Startup
    EP0WRITE(0x8080,LED_RGB(0,0, 0,0, 0,0)); // Color (OFF)
    EP0WRITE(0x8081,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8010,0x00000000); // Pattern

    // Code 1 - No Link
    EP0WRITE(0x8082,LED_RGB(0,0, 0,0, 0,0)); // Color (OFF)
    EP0WRITE(0x8083,LED_RGB(7,255, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8011,0x00000009); // Pattern

    // Code 2 - No IP
    EP0WRITE(0x8084,LED_RGB(7,99, 7,255, 0,0)); // Color (OFF)
    EP0WRITE(0x8085,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8012,0x00000009); // Pattern

    // Code 3 - Has IP
    EP0WRITE(0x8086,LED_RGB(7,255, 0,0, 0,0)); // Color (OFF)
    EP0WRITE(0x8087,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8013,0x00000000); // Pattern

    // Code 3 - Button Pend
    EP0WRITE(0x8088,LED_RGB(0,0, 0,0, 7,255)); // Color (OFF)
    EP0WRITE(0x8089,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8014,0x00000000); // Pattern

    // Code 4 - Connect // Changed to RED to indicate failing unit
    EP0WRITE(0x808a,LED_RGB(7,255, 0,0, 0,0)); // Color (OFF)
    EP0WRITE(0x808b,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8015,0x00000000); // Pattern

    EP0WRITE(0x8000,0x00000001); // Enable

    sleep(1);

}

RtnStatus_t ledTest(void* ptr) {

    // Code 0 - Startup
    EP0WRITE(0x8080,LED_RGB(0,0, 0,0, 0,0)); // Color (OFF)
    EP0WRITE(0x8081,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8010,0x00000000); // Pattern

    // Code 1 - No Link
    EP0WRITE(0x8082,LED_RGB(0,0, 0,0, 0,0)); // Color (OFF)
    EP0WRITE(0x8083,LED_RGB(7,255, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8011,0x00000009); // Pattern

    // Code 2 - No IP
    EP0WRITE(0x8084,LED_RGB(7,99, 7,255, 0,0)); // Color (OFF)
    EP0WRITE(0x8085,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8012,0x00000009); // Pattern

    // Code 3 - Has IP
    EP0WRITE(0x8086,LED_RGB(7,99, 7,255, 0,0)); // Color (OFF)
    EP0WRITE(0x8087,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8013,0x00000000); // Pattern

    // Code 3 - Button Pend
    EP0WRITE(0x8088,LED_RGB(0,0, 0,0, 7,255)); // Color (OFF)
    EP0WRITE(0x8089,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8014,0x00000000); // Pattern

    // Code 4 - Connect
    EP0WRITE(0x808a,LED_RGB(0,0, 0,0, 7,255)); // Color (OFF)
    EP0WRITE(0x808b,LED_RGB(0,0, 0,0, 0,0)); // Color (ON)
    EP0WRITE(0x8015,0x00000000); // Pattern

    EP0WRITE(0x8000,0x00000001); // Enable

    sleep(1);

    return TEST_OK;
}    





RtnStatus_t spiUserErase(void *ptr) 
{
    uint32_t spi_addr = 0x38000;

    spiErase(spi_addr); // ERASE takes 5 secs
    printf("SPI Erased User Data \n");
    return TEST_OK;
}





void spiInit(uint32_t mac_addr, bool locken) {

    uint32_t spi_addr = 0x38000;
    uint32_t spi_data;
    

    spiErase (spi_addr); // ERASE takes 5 secs

    printf("Mac_addr requested  = %08x\n", mac_addr);
    
    // First Cmd in Flash
    spi_data = 0x1 << 28 | // CFG WR OPCODE
	0x00; // "BOARD ID ADDR"
    spiWrite(spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

    // Determine the Board ID from the MAC Addr provided
    spi_data = ((mac_addr & 0xf00) == 0x500) ? 0x00040100 : ((mac_addr & 0xf00) == 0x400) ? 0x00040001 : 0x00020001;
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

    spi_data = 0x1 << 28 | // CFG WR OPCODE
        0x11; // "LSB MAC ADDR" csr reg addr within cfg space
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

    spi_data = (0x02000000 | (mac_addr & 0x00ffffff)); // LSB of MAC ADDR
    printf("SPI Write Data  = %08x\n", spi_data);
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));
    
    spi_data = 0x1 << 28 | // CFG WR OPCODE
        0x10; // "LSB MAC ADDR" csr reg addr within cfg space
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

    spi_data = 0x0000001c; // LSB of MAC ADDR
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));


    if(locken) {
	printf("***SPI WRITING SPI PROTECT LOCK***");
    spi_data = 0x1 << 28 | // CFG WR OPCODE
        0xFF; // protect register
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

    spi_data = 0x00000001; // LSB of MAC ADDR
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));
       
    }



    //    spi_data = 0xffffffff; // LSB of MAC ADDR
    // spiWrite(++spi_addr, spi_data);
    ++spi_addr;
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));
    ++spi_addr;
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));


}

//--------------------------------------------------
void spiInitAjit(uint32_t mac_addr, bool locken) {

    uint32_t spi_addr = 0x38000;
    uint32_t spi_data;
    

    spiErase (spi_addr); // ERASE takes 5 secs
    printf("Mac_addr requested  = %08x\n", mac_addr);
    
    // First Cmd in Flash
    spi_data = 0x1 << 28 | // CFG WR OPCODE
	0x00; // "BOARD ID ADDR"
    spiWrite(spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

    // Determine the Board ID from the MAC Addr provided
    //spi_data = ((mac_addr & 0xf00) == 0x500) ? 0x00040100 : ((mac_addr & 0xf00) == 0x400) ? 0x00040001 : 0x00020001;
    
    spi_data = 0x00040200;  //P6
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

    spi_data = 0x1 << 28 | // CFG WR OPCODE
        0x11; // "LSB MAC ADDR" csr reg addr within cfg space
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

     spi_data = (0x02000000 | (mac_addr & 0x00ffffff)); // LSB of MAC ADDR
    //spi_data =  | mac_addr; //0x02200018;
    printf("SPI Write Data  = %08x\n", spi_data);
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));
    
    spi_data = 0x1 << 28 | // CFG WR OPCODE
        0x10; // "LSB MAC ADDR" csr reg addr within cfg space
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

    spi_data = 0x0000001c; // LSB of MAC ADDR
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));


    if(locken) {
	printf("***SPI WRITING SPI PROTECT LOCK***");
    spi_data = 0x1 << 28 | // CFG WR OPCODE
        0xFF; // protect register
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

    spi_data = 0x00000001; // LSB of MAC ADDR
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));
       
    }



    //    spi_data = 0xffffffff; // LSB of MAC ADDR
    // spiWrite(++spi_addr, spi_data);
    ++spi_addr;
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));
    ++spi_addr;
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));


}




uint32_t audioCSRRead(uint32_t addr) {

  bool rdValid = false; // read data has been returned
  bool csrReady = false; // EP0 ready to accept a new CSR Rd/Wr yet
  uint32_t status;
  uint32_t rdData;
  uint32_t rdAddr;

  status = EP0READ(0x4002);
  csrReady = !(status & 0x80000000);
  
  while (!csrReady) {// while not ready, keep polling
    printf ("Previous CSR to LM4550 still pending, status=%x...\n",status);
    csrReady = !(EP0READ(0x4002) & 0x80000000);
    status = EP0READ(0x4002);
  }

  // setup Rd Cmd
  // 0x4001 ADCCSRCMD
  // [31] Rd=1/Wr=0
  // [22:16] addr
  // [15:0] wrData

  EP0WRITE(0x4001, ((addr & 0xff) << 16) | 0x80000000);

  // Poll for Rd Valid
  // 0x4002 ADCCSRSTAT
  // [31] adcCsrReg still busy
  // [30] RdData Valid
  // [22:16] RdAddr
  // [15:0] RdData

  csrReady = !(EP0READ(0x4002) & 0x80000000);
  while (!csrReady) {// while not ready, keep polling
    printf ("CSR Read Return from LM4550 @ addr=%x is not ready yet...\n", addr);
    csrReady = !(EP0READ(0x4002) & 0x80000000);
  }

  printf ("CSR Read to LM4550 @ addr=%x has been transmitted...\n", addr);

  uint32_t i=0;

  while (!rdValid) {
    i++;
    status = EP0READ(0x4002);
    rdValid = status & 0x40000000;
    rdData = status & 0x0000ffff;
    rdAddr = (status & 0x007f0000) >> 16;
    //printf ("CSR Read Data Return = %x for try #%d \n", status, i);
  }

  //printf("ADCCSRSTAT : valid=%x Rd Addr=%x Rd Data=%x\n", rdValid, rdAddr, rdData);

  return rdData;

}

void audioCSRWrite(uint32_t addr, uint32_t data)
{
  bool csrReady=false;
  uint32_t status;

  // 0x4001 ADCCSRCMD
  // [31] Rd=1/Wr=0
  // [22:16] addr
  // [15:0] wrData

  status = EP0READ(0x4002);
  csrReady = !(status & 0x80000000);

  while (!csrReady) {// while not ready, keep polling
    printf ("Previous CSR to LM4550 still pending, status=%x...\n",status);
    csrReady = !(EP0READ(0x4002) & 0x80000000);
    
  }

  EP0WRITE(0x4001, ((addr & 0x7f) << 16) | (data & 0xffff));

  csrReady = !(EP0READ(0x4002) & 0x80000000);
  while (!csrReady) {
    csrReady = !(EP0READ(0x4002) & 0x80000000);
    printf ("CSR Write still pending transmission...\n");
  }

  printf("Audio write @ addr=%x and data=%x has been sent out\n", addr, data);
  
}

//---------------------------------------------------------------------
//*********************************************************************
//**********  AUDIO INIT STUFF            *****************************
//*********************************************************************
//--------------------------------------------------------------------

void audioInit() {
  
  EP0WRITE(0x4000, 0x1); // assert cold reset
  EP0FLUSH;
  sleep(1); //  > 1us
  EP0WRITE(0x4000, 0x0); // deassert cold reset
  EP0FLUSH;
  sleep(1);
  EP0WRITE(0x4000, 0x80000000); // assert RxEn
  EP0FLUSH;

  // check for status register to see if LM4550 is ready
  // [0] : ADC is ready for transmit data
  // [1] : DAC is ready for receiving data
  // [2] : Analog mixers ready
  // [3] : Vref is up to nominal level

  uint32_t audio_status;
  audio_status = audioCSRRead(0x26);
  while ((audio_status & 0xf) != 0xf) {
    printf ("LM4550 chip is not ready yet...\n");
  }

  printf ("LM4550 chip is up and running... \n");

  audioCSRWrite(0x06, 0x0000); // Mono volume
  audioCSRWrite(0x04, 0x0f0f); // Headphone volume
  audioCSRWrite(0x18, 0x0008); // PCM out volume

  printf ("Audio Init done... \n");

}






//-------------------------------
//--- I2C WRITE ---------------------------
//-------------------------------
//  the address is the address for the device


void i2cwrite(uint32_t addr, uint32_t data)
{

    uint32_t tmp;
   //-------------------------------- 
   //i2cwrite

    EP0WRITE(0x6010, 0x0);
    EP0WRITE(0x6010, 0x0);
    EP0WRITE(0x600c, 0x0);
    EP0WRITE(0x600c, 0x0);
    EP0FLUSH;  
    
    EP0WRITE(0x6000, addr); 
    EP0WRITE(0x6000, addr); 
    EP0WRITE(0x6008,data);
    EP0WRITE(0x6008,data);
    
   //// 2 bytes of data, speed 0xff, enable i2c adc clock and data,
   //// addr1 valid, addr0 invalid, segment valid, write=0, start xfer
   //3'b0,
   //9'h2(num bytes of data),
   //12'hff (3ff is default for slowest speed)
   //3'b1 (Enable the I2C ADC clock and datapath),
   //1'b1, (addr1_valid)
   //1'b0, (addr0_valid)
   //1'b0, (segment_valid)
   //1'b0, (read=1/write=0)
   //1'b1; (start transfer)
   EP0WRITE(0x600c,0x0020ff31);
   EP0WRITE(0x600c,0x0020ff31);
   EP0FLUSH;

   printf ("INFO: ...I2C Write Addr::0x%08x - Data::0x%08x \n",addr,data);

    for(uint32_t i=0; i<64; i++) {
	tmp = EP0READ(0x6010);
	tmp = EP0READ(0x6010);
	printf("INFO: ...Polling I2C Status: 0x%08x\n",tmp );
	if ((tmp & 0x1)) break;
        EP0FLUSH;

	    }

   //-------------------------------
}





//-------------------------------
//--- wolfson WRITE ---------------------------
//-------------------------------
//  the address is for the internal register
//  the data and address has an interesting arrangement
//  the i2c address for the wolfson is 0x34 

void wolfwrite(uint32_t addr, uint32_t data)
{
    uint32_t i2cdata;

     // 16'b0,data[7:0],addr[6:0],data[8]
    i2cdata =              (0x0000FF00 & (data << 8));
    i2cdata = i2cdata    | (0x00000001 & (data >> 8));
    i2cdata = i2cdata    | (0x000000FE & (addr << 1));
    
   printf ("INFO: ...Wolfson Write RegAdr=0x%02x data=0x%03x\n",addr,data);
 

    
   i2cwrite(0x00340000,i2cdata); // i2c wolfson part address
 
}



void audioInitP4(bool switchover = true, bool max_vol = false) {
  // Initializing Wolfson part
  wolfwrite(0x0f,0x1); // reset register values to default
  
  // enable everything
  // Pwr Mgmt1
  wolfwrite(0x19,0x0);
  sleep(1);
  wolfwrite(0x19,0x1FE);
  // Pwr Mgmt2
  wolfwrite(0x1A,0x1FE);
  // enable on-board speaker by default
  // switchover when headphone is detected
  wolfwrite(0x18,0x40);

  printf (">>> Enable everything...<<< \n");
  //addr = 8  data 9'b0_0011_0111 sample rate 22.05khz
  //addr = 8  data 9'b0_0010_0011 sample rate 44.118khz
  //addr = 8  data 9'b0_0010_0111 sample rate 8.01khz
  printf (">>> Audio Sample rate set up 22.05khz...<<<\n");
  wolfwrite(0x08,0x37);  //sample rate 22.05khz
  //wolfwrite(0x08,0x23);  //sample rate 44.11khz
  //addr = 7  data 9'b0_0100_0010 enable master mode 
  printf (">>> Audio Enable Master Mode... <<< \n");
  wolfwrite(0x07,0x042);  //enable master mode

  // Enable Left Mixer
  // left mixer
  // addr = 0x22 data 9'b1_0000_0000
  wolfwrite(0x22, 0x100);

  // Enable Right Mixer
  // right mixer
  // addr = 0x25 data 9'b1_0000_0000
  wolfwrite(0x25, 0x100);

  // Update gains
  // default values at the moment
  // addr = 0x0a, data 9'b1_1111_1111
  // addr = 0x0b, data 9'b1_1111_1111
  // addr = 0x02, data 9'b1_0111_1001
  // addr = 0x03, data 9'b1_0111_1001
  if(max_vol) {
    wolfwrite(0x0a, 0x0ff);
    wolfwrite(0x0b, 0x1ff);
    // Choon newly added to beef up volume further
    // turn up max volume
    // remove for now, so that we don't overdrive the output stage
    wolfwrite(0x2, 0x07f);
    wolfwrite(0x3, 0x17f);
    wolfwrite(0x28, 0x07f);
    wolfwrite(0x29, 0x17f);
  } else { // normal vol
    wolfwrite(0x0a, 0x0ef);
    wolfwrite(0x0b, 0x1ef);
  }

  wolfwrite(0x02, 0x079);
  wolfwrite(0x03, 0x179);

  // Tom feedback to add 9/8/07
  if(switchover) {
    wolfwrite(0x18, 0x50);
  } else {
    wolfwrite(0x18, 0x10);
  }
  
  wolfwrite(0x17, 0x1c0);

  // remove digital soft mute
  wolfwrite(0x05, 0x000);

  // for audio_in path
  wolfwrite(0x11, 0x17b); // ALC for left channel
  wolfwrite(0x14, 0x043); // noise gate to reduce hissing sound

  EP0WRITE(0x4000, 0x00000001); // reset assertiion in BCLK domain
  EP0FLUSH;
  sleep(1); //  > 1us
  //EP0WRITE(0x4003, 0x0); // set trig value
  //EP0WRITE(0x4000, 0x40000000); // enable mono and enable Tx/Rx
  EP0FLUSH;
  sleep(1); //  > 1us
  printf ("Set up for Mono and enabling Tx/Rx...\n");
  
  printf ("DONE AUDIO INIT!!!...\n");
  
}
 

// loops back mic-in to speaker out in Wolfson
void audioInitLpbk() {
  
  //wolfwrite 
  // LMIXSEL = LINPUT1
  // LI2LO = enable LMIXSEL to LOUT
  wolfwrite(0x22, 0x180);

  // LI2R0 = enable LMIXSEL to ROUT
  wolfwrite(0x24, 0x080);
	    
}

// disconnecting audio
void audioDisconnect() {

  printf("Disconnecting audio...\n");
  EP0WRITE(0x4000, 0x00000000); // enable mono and enable Tx/Rx
  EP0FLUSH;
  sleep(1);
  EP0WRITE(0x4003, 0x0); // set trig value
  

}

//---------------------------------------------------------------------
//*********************************************************************
//**********  AUDIO INIT STUFF  END       *****************************
//*********************************************************************
//--------------------------------------------------------------------


bool cvtSupport() {
  uint32_t offset;
  uint32_t rd_data;
  
  offset = 0x18;
  EP0WRITE(0x6004, (( offset ) & 0xff ) << 8 );
  EP0WRITE( 0x600C, ( 0x4 << 20 ) | ( 0x3ff << 8 ) | ( 0x1f ));
  rd_data = EP0READ(0x6008);
  printf("I2C READ EDID Table @ offset %08x: 0x%08x\n",offset, rd_data);

  return (((rd_data & 0x1)) == 1);
}

uint32_t calcClkSynthSettings(uint32_t freq) {
  
  uint32_t fin = 25; // 25MHz
  uint32_t fout;
  uint32_t RDW;
  uint32_t VDW;
  uint32_t RDW_max = 123; // max divider value for fin
  float VDW_min;
  float VDW_max;
  uint32_t OD;
  float fout_calc;
  bool converge=false;
  float ppm;
  uint32_t S;

  for(RDW=1;RDW < RDW_max;RDW++) {
    // use commercial temperature range
    VDW_min = ((55/(2*(float)fin))*(RDW+2))-8;
    VDW_max = ((400/(2*(float)fin))*(RDW+2))-8;
    if(VDW_min < 4) VDW_min=3; // VDW range 4-511
    //printf("RDW=%0d, VDW_min=%0f, VDW_max=%0f\n",RDW, VDW_min,VDW_max);
    for(VDW=(int)VDW_max;VDW>(int)VDW_min;VDW--) {
      for(OD=2;OD<10;OD++) {
	if(converge) break;
	if(OD==9) {OD=OD+1;} // no OD=9
	fout_calc = (float)fin * 2 * ((float)VDW+8)/(((float)RDW+2)*(float)OD);
	ppm = ((fout_calc - (float)freq) / (float)freq) * 1000000;
	if(ppm < 0) ppm = (ppm * -1); // ppm was negative
	//printf("VDW=%0d, RDW=%0d, OD=%0d, fout_calc = %0f, ppm=%0f\n",VDW, RDW, OD, fout_calc, ppm);

	if(ppm < 500) { // if < 500ppm
	  converge = true;
	  break;
	}
      }
      if(converge) break;
    }
    if(converge) break;

  }
  
  printf("VDW=%0d, RDW=%0d, OD=%0d, fout_calc = %0f\n",VDW,RDW,OD,fout_calc);
  
  switch (OD)
    {
    case 2 : 
      S = 0x1;
      break;
    case 3 :
      S = 0x6;
      break;
    case 4 : 
      S = 0x3;
      break;
    case 5 :
      S = 0x4;
      break;
    case 6 : 
      S = 0x7;
      break;
    case 7 :
      S = 0x5;
      break;
    case 8 : 
      S = 0x2;
      break;
    case 10 :
      S = 0x0;
      break;
    }
  
  uint32_t F = 0x0;
  uint32_t TTL = 0x1;
  uint32_t C = 0x0;

  uint32_t settings = ((RDW & 0x7f) |
		  ((VDW & 0x001) << 7) |
		  ((VDW & 0x1fe) >> 1 << 8) |
		  ((S & 0x7) << 16) |
		  ((F & 0x3) << 19) |
		  ((TTL & 0x1) << 21) |
		  ((C & 0x3) << 22));
  
  printf("clk synth settings = 0x%0x\n", settings);

  return settings;
  
}

bool setPrefTiming(uint32_t width, uint32_t height) {
  
  uint8_t edid_data[ 20 ];

  ep0Client->edidRead(0x34, edid_data, 20);
  
  // for debug
  //for(uint32_t i=0;i<20;i++) {
  //  printf("i=%0d, edid_data = %0x\n",i, edid_data[i]);
  //}

  uint32_t freq = (edid_data[3] << 8 | edid_data[2])/100;
  uint32_t h_videopix = (((edid_data[6] & 0xf0) << 4) | edid_data[4]);
  uint32_t v_videopix = (((edid_data[9] & 0xf0) << 4) | edid_data[7]);
  
  if (!width || !height) {
    width = h_videopix;
    height = v_videopix;
  }
  
  if (edid_data[2] == 0x0) { // no preferred timing info
    printf("No preferred timing info...\n");
    return false;
  }
  
  // HW constraints from VGA_CLK
  if (freq > 166) {
    printf("Pixel Clock Freq is too high...\n");
    return false;
  }
  
  // HW constraints from DDR BW
  if (width > 1920 && height > 1200) {
    printf("Monitor resolution not supported due to DDR bandwidth\n");
    return false;
  }
  
  if (*boardName == "AttoP2") {
    printf("Old board, preferred timing info not supported... \n");
    return false;
  }

  if(width == h_videopix && height == v_videopix) {
    uint32_t h_blankpix = (((edid_data[6] & 0x0f) << 8) | edid_data[5]);
    uint32_t v_blankpix = (((edid_data[9] & 0x0f) << 8) | edid_data[8]);
    uint32_t h_frontporchpix = (((edid_data[13] & 0xc0) << 2) | edid_data[10]);
    // hack for Sony G220
    //uint32_t h_frontporchpix = (((edid_data[13] & 0x00) << 2) | edid_data[10]);
    uint32_t h_syncpix = (((edid_data[13] & 0x30) << 4) | edid_data[11]);
    // hack for Sony G220
    //uint32_t h_syncpix = (((edid_data[13] & 0x00) << 4) | edid_data[11]);
    uint32_t v_frontporchpix = (((edid_data[13] & 0x0c) << 2) | 
    				(edid_data[12] & 0xf0) >> 4);
    uint32_t v_syncpix = (((edid_data[13] & 0x03) << 4) | 
    			  (edid_data[12] & 0x0f));
    // hack for Sony G220
    //uint32_t v_syncpix = (((edid_data[13] & 0x00) << 4) | 
    //			  (edid_data[12] & 0x0f));
    uint32_t h_borderpix = edid_data[17];
    uint32_t v_borderpix = edid_data[18];
    
    // new to handle sync polarity
    uint32_t sync_def = edid_data[19] & 0x1e;
    // hack for Sony G220
    //h_borderpix = 0;

    uint32_t wdSetting = calcClkSynthSettings(freq);
    clkWordWrite(wdSetting);
    
    // VGA Registers
    uint32_t vgaLinePels = h_videopix + h_blankpix + 2*h_borderpix - 1;
    uint32_t hvisPels = h_videopix - 1;
    uint32_t vvisPels = v_videopix - 1;
    uint32_t hsyncStart = 2*h_borderpix + h_videopix + h_frontporchpix +2;
    uint32_t hsyncEnd = 2*h_borderpix + h_videopix + h_frontporchpix + h_syncpix +2;
    uint32_t vsyncStart = 2*v_borderpix + v_videopix + v_frontporchpix +2;
    uint32_t vsyncEnd = 2*v_borderpix + v_videopix + v_frontporchpix + v_syncpix +2;
    uint32_t hvisStart = h_borderpix + 2;
    uint32_t vvisStart = v_borderpix + 2;
    uint32_t frameLines =  v_videopix + v_blankpix + 2*v_borderpix - 1;
    // when sync_def[4:0] == 4'b1_xx1x, hsyncPol=1
    uint32_t hsyncPol = ( (sync_def & 0x12) == 0x12 ) ? 1 : 0;
    // when sync_def[4:0] == 4'b1_11xx, vsyncPol=1
    uint32_t vsyncPol = ( (sync_def & 0x1c) == 0x1c ) ? 1 : 0;
    uint32_t vgaPol = hsyncPol | (vsyncPol << 1);

    EP0WRITE(0x3002, vgaLinePels);
    EP0WRITE(0x3003, hvisPels);
    EP0WRITE(0x3004, vvisPels);
    EP0WRITE(0x3005, hsyncStart);
    EP0WRITE(0x3006, hsyncEnd);
    EP0WRITE(0x3007, vsyncStart);
    EP0WRITE(0x3008, vsyncEnd);
    EP0WRITE(0x3009, frameLines);
    EP0WRITE(0x300a, hvisStart);
    EP0WRITE(0x300b, vvisStart);
    EP0WRITE(0x300c, vgaPol);
    EP0FLUSH;

    // For debugging
    printf("Preferred Timing Mode Selected...\n");
    printf("freq = %0d \n", freq);
    printf("h_videopix = %0d \n", h_videopix);
    printf("h_blankpix = %0d \n", h_blankpix);
    printf("v_videopix = %0d \n", v_videopix);
    printf("v_blankpix = %0d \n", v_blankpix);
    printf("h_frontporchpix = %0d \n", h_frontporchpix);
    printf("h_syncpix = %0d \n", h_syncpix);
    printf("v_frontporchpix = %0d \n", v_frontporchpix);
    printf("v_syncpix = %0d \n", v_syncpix);
    printf("h_borderpix = %0d \n", h_borderpix);
    printf("v_borderpix = %0d \n", v_borderpix); 
    printf("sync_def = %0x \n", sync_def);
    printf("cfgVgaLinePels = %0d\n",EP0READ(0x3002));
    printf("cfgVgaHvisPels = %0d\n",EP0READ(0x3003));
    printf("cfgVgaVvisPels = %0d\n",EP0READ(0x3004));
    printf("cfgVgaHsyncStart = %0d\n",EP0READ(0x3005));
    printf("cfgVgaHsyncEnd = %0d\n",EP0READ(0x3006));
    printf("cfgVgaVsyncStart = %0d\n",EP0READ(0x3007));
    printf("cfgVgaVsyncEnd = %0d\n",EP0READ(0x3008));
    printf("cfgVgaFrameLines = %0d\n",EP0READ(0x3009));
    printf("cfgVgaHVisStart = %0d\n",EP0READ(0x300a));
    printf("cfgVgaVVisStart = %0d\n",EP0READ(0x300b));
    printf("cfgVgaPolarity = %0d\n",EP0READ(0x300c));
   
    return true;
  }

  // requested resolution don't match preferred timing
  return false;

}

RtnStatus_t testDualMonitor(void*) {

  // Program VGA at 2560x1024 dual monitor RB resolution

  // VGA Registers
  uint32_t vgaLinePels = 2719;
  uint32_t hvisPels = 2559;
  uint32_t vvisPels = 1023;
  uint32_t hsyncStart = 2610;
  uint32_t hsyncEnd = 2642;
  uint32_t vsyncStart = 1029;
  uint32_t vsyncEnd = 1039;
  uint32_t hvisStart = 2;
  uint32_t vvisStart = 2;
  uint32_t frameLines =  1053;
  uint32_t vgaPol = 1;
  
  EP0WRITE(0x3002, vgaLinePels);
  EP0WRITE(0x3003, hvisPels);
  EP0WRITE(0x3004, vvisPels);
  EP0WRITE(0x3005, hsyncStart);
  EP0WRITE(0x3006, hsyncEnd);
  EP0WRITE(0x3007, vsyncStart);
  EP0WRITE(0x3008, vsyncEnd);
  EP0WRITE(0x3009, frameLines);
  EP0WRITE(0x300a, hvisStart);
  EP0WRITE(0x300b, vvisStart);
  EP0WRITE(0x300c, vgaPol);
  EP0FLUSH;

  uint32_t freq = 172;

  uint32_t wdSetting = calcClkSynthSettings(freq);
  clkWordWrite(wdSetting);

  EP0WRITE(0x3001,0x1); // VGA Enable pattern
  EP0WRITE(0x1001,0x21); // enable VDC, use color_swap mode
  
  // Display dual image
  Image *img = new Image;
  Image *img2 = new Image;
  assert (img != NULL);
  assert (img2 != NULL);
  img->readBmp("../util/bmps/sunsetstar_1920x1200.bmp");
  img2->readBmp("../util/bmps/newjersey_1920x1200.bmp");


  printf ("Done w/ Read BMP\n");
  
  uint32_t totlen = 0;
  
  
  printf ("Starting VDC Image\n");
  
  // Now go thru raw tiles, convert them to RLE, and send them to the HW
  uint32_t iter=0;
  while (iter<32) {
    iter++;
    printf ("display iter %0d\n", iter);
    for (uint16_t tileX = 0; tileX < img->getNumXTiles(); tileX++) {
      for (uint16_t tileY = 0; tileY < img->getNumYTiles(); tileY++) {
	RleTile rleTile;
	rleTile.rleEncode(img->getRawTileData(tileX, tileY), tileX, tileY);
	sStream->epWrite(videoDrv->Endpoint,(char*)rleTile.getRleTileData(),rleTile.getRleTileDataLength());
	totlen += rleTile.getRleTileDataLength();
      }
    }
    FLUSH;
    // send second image to second fram buffer (at vertical tile idx + 128)
    for (uint16_t tileY = 0; tileY < img2->getNumYTiles(); tileY++) {
      for (uint16_t tileX = 0; tileX < img2->getNumXTiles(); tileX++)  {
	RleTile rleTile;
	rleTile.rleEncode(img2->getRawTileData(tileX,tileY), tileX, tileY);
	sStream->epWrite(videoDrv->Endpoint,(char*)rleTile.getRleTileData(),rleTile.getRleTileDataLength());
	totlen += rleTile.getRleTileDataLength();
      }
      FLUSH;
    }
  }
  FLUSH;

  delete img, img2;

  return TEST_OK;

}

void vgaInit (ScreenRes_t *res) {

    // configure VGA registers
    // - configure the VGA Hsync because of the latency in DAC
    //   increased from 2 to 7.5 cycles
    // - but the original hsync numbers are not good enough anyway
  // Choon : remove the _test_ when we sync up with new software tree again
    printf ("Initializing VGA...\n");
    uint32_t tableSize;
    bool cvt_support;

    EP0WRITE(0x3001,0x0); // disable Vga
    EP0FLUSH;

    if(setPrefTiming(res->width, res->height)) {
      printf ("VGA init with Preferred Timing for %0dx%0d\n", res->width, res->height);
    } else { // non-preferred timing mode
      if((res->width == 1600) 
	 && (res->height == 1200) 
	 && (res->refresh == 60)) {

        setClkSynthFreq(res->pixClk);
        sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

        tableSize = VesaSettingSize;
        for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_1600_60_cvt_settings[i].addr, vesa_1600_60_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_1600_60_settings[i].addr, vesa_1600_60_settings[i].data);    
	    }
	  }
        EP0FLUSH;
        printf ("VGA init done for 1600x1200\n");
      } else if((res->width == 1280) 
		&& (res->height == 1024) 
		&& (res->refresh == 60)) {

	setClkSynthFreq(res->pixClk);
	sleep(1);
      
	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_1280_60_cvt_settings[i].addr, vesa_1280_60_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_1280_60_settings[i].addr, vesa_1280_60_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 1280x1024 @ 60Hz\n");
      } else if((res->width == 1920) 
		&& (res->height == 1200) 
		&& (res->refresh == 60)) {

	setClkSynthFreq(res->pixClk);
	sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_1920_60_cvt_settings[i].addr, vesa_1920_60_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_1920_60_test_settings[i].addr, vesa_1920_60_test_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 1920x1200 @ 60Hz\n");
      } else if((res->width == 1280) 
		&& (res->height == 1024) 
		&& (res->refresh == 75)) {

	setClkSynthFreq(res->pixClk);
	sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_1280_75_cvt_settings[i].addr, vesa_1280_75_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_1280_75_settings[i].addr, vesa_1280_75_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 1280x1024 @ 75Hz\n");
	/*  } else if((res->width == 1024) 
	    && (res->height == 768) 
	    && (res->refresh == 60)) {

	    setClkSynthFreq(res->pixClk);
	    sleep(1);

	    tableSize = VesaSettingSize;
	    for (uint32_t i=0;i<tableSize;i++)
	    {
	    EP0WRITE(vesa_1024_60_settings[i].addr, vesa_1024_60_settings[i].data);    
	    }
	    EP0FLUSH;
	    printf ("VGA init done for 1024x768 @ 60Hz\n");
	*/
      } else if((res->width == 1024) 
		&& (res->height == 768) 
		&& (res->refresh == 60)) {

	setClkSynthFreq(res->pixClk);
	sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_1024_60_cvt_settings[i].addr, vesa_1024_60_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_1024_60_settings[i].addr, vesa_1024_60_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 1024x768 @ 60Hz\n");
      } else if((res->width == 1024) 
		&& (res->height == 768) 
		&& (res->refresh == 75)) {

	setClkSynthFreq(res->pixClk);
	sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_1024_75_cvt_settings[i].addr, vesa_1024_75_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_1024_75_settings[i].addr, vesa_1024_75_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 1024x768 @ 75Hz\n");
      } else if((res->width == 800) 
		&& (res->height == 600) 
		&& (res->refresh == 60)) {

	setClkSynthFreq(res->pixClk);
	sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_800_60_cvt_settings[i].addr, vesa_800_60_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_800_60_settings[i].addr, vesa_800_60_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 800x600 @ 60Hz\n");
      } else if((res->width == 800) 
		&& (res->height == 600) 
		&& (res->refresh == 75)) {

	setClkSynthFreq(res->pixClk);
	sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_800_75_cvt_settings[i].addr, vesa_800_75_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_800_75_settings[i].addr, vesa_800_75_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 800x600 @ 75Hz\n");
      } else if((res->width == 640) 
		&& (res->height == 480) 
		&& (res->refresh == 60)) {

	setClkSynthFreq(res->pixClk);
	sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_640_60_cvt_settings[i].addr, vesa_640_60_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_640_60_settings[i].addr, vesa_640_60_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 640x480 @ 60Hz\n");
      } else if((res->width == 640) 
		&& (res->height == 480) 
		&& (res->refresh == 75)) {

	setClkSynthFreq(res->pixClk);
	sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_640_75_cvt_settings[i].addr, vesa_640_75_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_640_75_settings[i].addr, vesa_640_75_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 640x480 @ 75Hz\n");
      } else { // default is 1024
	setClkSynthFreq(res->pixClk);
	sleep(1);

	cvt_support = cvtSupport();
	printf ("CVT Support = %x\n", cvt_support);

	tableSize = VesaSettingSize;
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    if(cvt_support) {
	      EP0WRITE(vesa_1024_60_cvt_settings[i].addr, vesa_1024_60_cvt_settings[i].data);    
	    } else {
	      EP0WRITE(vesa_1024_60_settings[i].addr, vesa_1024_60_settings[i].data);    
	    }
	  }
	EP0FLUSH;
	printf ("VGA init done for 1024x768 @ 60Hz\n");
	printf ("VGA Reg Dump\n");
	for (uint32_t i=0;i<tableSize;i++)
	  {
	    printf("Reg @ %08x = %08x\n",
		   vesa_1024_60_settings[i].addr, 
		   EP0READ(vesa_1024_60_settings[i].addr)
		   );    
	  }
	EP0FLUSH;

      }
    }
}

void ddrInit (const char *boardName) {
  ///////////////////////////////////////////////
  // DDR initialization

  bool board_ml40x=false, 
    board_attoP1 = false, 
    board_attoP4 = false,
    board_attoP5 = false;
  
    if (strcmp (boardName, "ml40x") == 0) {
        printf ("V4(ML40x) board\n");
        board_ml40x = true;
    } else if (strcmp (boardName, "AttoP1") == 0) {
        printf ("S3E(AttoP1) board\n");
        board_attoP1 = true;
    } else if (strcmp (boardName, "AttoP2") == 0) {
        printf ("S3E(%s) board\n",boardName);
        board_attoP1 = true; 
    } else if (strcmp (boardName, "AttoP4") == 0) { // Choon added for P4
        printf ("S3E(%s) board\n",boardName);
        board_attoP4 = true; 
    } else if (strcmp (boardName, "AttoP5") == 0) { // Choon added for P5
        printf ("S3E(%s) board\n",boardName);
        board_attoP5 = true; 
    } else if (strcmp (boardName, "AttoP6") == 0) { // Choon added for P6
        printf ("S3E(%s) board\n",boardName);
        board_attoP5 = true; // use the same DDR init as P5
    } else if (strcmp (boardName, "AttoP7") == 0) { // Choon added for P7
        printf ("S3E(%s) board\n",boardName);
    } else if (strcmp (boardName, "AttoP8") == 0) { // Choon added for P8
      printf ("S3E(%s) board\n",boardName);
        board_attoP5 = true;  // use the same DDR init as P5
    } else if (strcmp (boardName, "AttoP8") == 0) { 
        printf ("S3E(%s) board\n",boardName);
        board_attoP5 = true;  // use the same DDR init as P5
    } else if (strcmp (boardName, "Un-Programmed") == 0) {
        printf ("Board is in the process of being programmed\n"); // No need to do any DDR Init
        return;
    } else {
        printf ("ERROR: HUH??? unknown board name!\n");
	board_attoP5 = true;  // use the same DDR init as P5
        //exit (1);
    }

    printf ("Initializing DDR...\n");
    // StdModeReg of DDR
    // {19'b0,Mode(6),CL(3),BT(1),BL(3)}
    // {19'b0,6'd0   ,3'd3 ,1'd0 ,3'd3 }
    if (board_ml40x) {
        EP0WRITE(0x2009,0x0153);
    } else {
      EP0WRITE(0x2040,0x060); // extended mode register
      EP0WRITE(0x2009,0x033); // std mode register
      EP0WRITE(0x2010,0x0); // enable init sequence to DDR DRAM chip
    }

    // configure DDR logic
    // originally RdTo*WrTo* = 0x1e0
    // refresh has been 0x0
    if (board_ml40x) {
        EP0WRITE(0x2005,0x2); // RdToData default 
    } else {
      if(board_attoP4 || board_attoP5) { // Choon : P4 board, DQS collides with clkDdr
        EP0WRITE(0x2005,0x4); // RdToData default 4
      }
      else {
        EP0WRITE(0x2005,0x3); // RdToData default 3
      }
    }
    EP0WRITE(0x200a,0x0);  // cfgIssueBitmap RdToRd
    EP0WRITE(0x200b,0x0);  // cfgIssueBitmap RdToWR
    EP0WRITE(0x200c,0x0);  // cfgIssueBitmap RdToRef
    EP0WRITE(0x200d,0x0);  // cfgIssueBitmap WrToWr
    EP0WRITE(0x200e,0x0);  // cfgIssueBitmap WrToRd
    EP0WRITE(0x200f,0x0);  // cfgIssueBitmap WrToRef
    //EP0WRITE(0x2004,0x1); // WrToData default 1
    if(board_attoP4 || board_attoP5) { // Choon : P4 board, DQS collides with clkDdr
      EP0WRITE(0x2030,0x80000000); // cfgSelDdrClk
    } 
    else {
      EP0WRITE(0x2030,0x00000000); // cfgSelDdrClk
    }
    if (board_ml40x) {
        EP0WRITE(0x2008,0x9); // num of col bits
    } else {
        EP0WRITE(0x2008,0x8); // num of col bits
    }

    EP0FLUSH;
    printf("READ DATA @ 0x2009: 0x%08x\n",EP0READ(0x2009));
    /*
    printf("READ DATA @ 0x2005: 0x%08x\n",EP0READ(0x2005));
    printf("READ DATA @ 0x200a: 0x%08x\n",EP0READ(0x200a));
    printf("READ DATA @ 0x200b: 0x%08x\n",EP0READ(0x200b));
    printf("READ DATA @ 0x200c: 0x%08x\n",EP0READ(0x200c));
    printf("READ DATA @ 0x200d: 0x%08x\n",EP0READ(0x200d));
    printf("READ DATA @ 0x200e: 0x%08x\n",EP0READ(0x200e));
    printf("READ DATA @ 0x200f: 0x%08x\n",EP0READ(0x200f));
    printf("READ DATA @ 0x2004: 0x%08x\n",EP0READ(0x2004));
    printf("READ DATA @ 0x2008: 0x%08x\n",EP0READ(0x2008));
    */

    EP0WRITE(0x2010,0x1); // enable init sequence to DDR DRAM chip
    EP0FLUSH;
    sleep(1);
    printf ("DDR init done\n");
}

void ddrWrite(uint32_t addr, uint32_t bank, uint32_t* data)
{
    uint32_t i;

    EP0WRITE(0x1012, (bank<<28) | addr);

    // write burst of 8 32-bit data
    // data[7] contains the high order number
    // data[0] contains the low order number
    for (i=0;i<4;i++) {
        EP0WRITE(0x1010, data[2*i]);
        EP0WRITE(0x1011, data[2*i+1]); 
    }

    //printf("DDR Status Reg after data write: %x\n", EP0READ(0x201c));

    // read DDRWRCMD
    //  printf("DDRWRCMD =%x\n",EP0READ(0x1012));

}

// for Read
// csrRead to 0x201d DDRCPURD {busy,1b,bank[1:0],4b,addr[23:0]}
// csrRead to 0x3010 DDRRDDATA (low 32-bit)
// csrRead to 0x3011 DDRRDDATA (high 32-bit)

void ddrRead(uint32_t addr, uint32_t bank, uint32_t* data) {
  // read a line of memory through the debug port.  Requires VGA to be disabled!
  //printf("DDR Status Reg before read cmd issued: %x\n", EP0READ(0x201c));
  EP0WRITE(0x201d, (bank << 28) | addr);
  printf("DDR Status Reg after 1st read cmd issued: %x\n", EP0READ(0x201c));
  // poll for busy status [31] deasserted to indicate 
  bool busy = true;
  uint32_t i;
  
  // could add timer to reduce network traffic
  while (busy) {
    busy = (EP0READ(0x201d) & 0x80000000);
    printf("Busy reading... \n");
  }
  
  uint32_t result = EP0READ(0x201d);
  printf("DDRCPURD Reg : %x\n", result);
  
  for (i=0;i<4;i++) {
    data[2*i] = EP0READ(0x3010);
    //printf("DDR Status Reg after reading 0x3010 for i=%x : %x\n", i, EP0READ(0x201c));
    //printf("i=%x, vgaErr = %x\n", i, EP0READ(0x3020));
    data[2*i+1] = EP0READ(0x3011);
    //printf("DDR Status Reg after reading 0x3011 for i=%x : %x\n", i, EP0READ(0x201c));
    //printf("i=%x, vgaErr = %x\n", i, EP0READ(0x3020));
  }
  
}

bool wrRdDdr(uint32_t addr, uint32_t bank, uint32_t* wrData){

  uint32_t rdData[8];
  uint32_t i=0;
  bool fail_status=false;

  ddrWrite(addr, bank, wrData);
  ddrRead(addr, bank, rdData);
  
  // self checker for the burst of reads
  for(i=0;i<8;i++) {
    if(rdData[i] != wrData[i]) {
      printf("MISMATCH : Write Data[%x] = %x, DDR Read Data[%x] = %x \n", i, wrData[i], i, rdData[i]);
      fail_status = true;
    }
  }
  
  if(fail_status) {
      return false;
  }
  else {
    return true;
  }

}

/*
   void testSeqNums() {
   assert(SEQNO_GTE(10,10));
   assert(SEQNO_GTE(10,9));
   assert(!SEQNO_GTE(9,10));
   assert(SEQNO_GTE(10,4090));
   assert(!SEQNO_GTE(4090,10));

   assert(!SEQNO_GT(10,10));
   assert(SEQNO_GT(10,9));
   assert(!SEQNO_GT(9,10));
   assert(SEQNO_GT(10,4090));
   assert(!SEQNO_GT(4090,10));

   assert(SEQNO_LTE(10,10));
   assert(SEQNO_LTE(9,10));
   assert(!SEQNO_LTE(10,9));
   assert(SEQNO_LTE(4090,10));
   assert(!SEQNO_LTE(10,4090));

   assert(!SEQNO_LT(10,10));
   assert(SEQNO_LT(9,10));
   assert(!SEQNO_LT(10,9));
   assert(!SEQNO_LT(10,4090));
   assert(SEQNO_LT(4090,10));
   }
 */
RtnStatus_t avgAccessTimeTest(void*) {
    // measure avg read latency
    timeus avg = 0;
    uint32_t count = 0;

    for(uint32_t i=0;i<32;i++) {
        EP0CFGWRITE(2,1<<i);
        timeus start = gettimeus();
        //printf("READ DATA: 0x%08x\n", EP0CFGREAD(2));
        avg += gettimeus() - start;
        count++;
    }

    avg /= count;

    printf("Average Access Time: %lld us\n", avg);
    return TEST_OK;
}

// choon added function to do burst of ddr access
// for Write
// csrWrite to 0x1012 DDRCPUWRCMD {2b,bank[1:0],4b,addr[23:0]}
// csrWrite to 0x1010 DDRWRDATA(low 32-bit)
// csrWrite to 0x1011 DDRWRDATA(high 32-bit)

void ddrDbgClearScreen() {
    // Try to clear the screen using debug insertion path in the VDC
    printf("Disabling VDC\n");
    EP0WRITE(0x1001,0x0); // disable VDC
    EP0FLUSH;
    printf ("Clearing screen through VDC debug\n");

    uint32_t wrData[8] = {0,0,0,0 ,0,0,0,0};
    //uint32_t wrData[8] = {0xffffffff};
    // for 1024x768, need to write 64*1024 = 65536 lines in all 4 banks
    for(uint32_t addr=0; addr<16384; addr++) {
        for(uint32_t bank=0; bank<4; bank++) {
            ddrWrite(addr, bank, wrData);
            EP0FLUSH;
        }
        if ((addr % 4096) == 0) {
            // this is purely present to slow down the clear screen so that TNP doesn't get
            // overwhelmed
            uint32_t data = EP0READ(0x0000);
            printf ("addr=0x%0x;  chipid=0x%0x\n", addr, data);
        } 
    }
}

void ddrWrite_2(uint32_t addr, uint32_t *data) {
    EP0WRITE(0x2013,addr);
    EP0WRITE(0x2013,0xffff);
    EP0WRITE(0x2013,data[0]);
    EP0WRITE(0x2013,data[1]);
    EP0WRITE(0x2013,data[2]);
    EP0WRITE(0x2013,data[3]);
}

void vdcWriteRect(int x, int y, int w, int h, char *color) {
  EP0WRITE(0x1001, 0x23);
  printf("Enabling VDC 1.5...\n");
  EP0FLUSH;

  char data[256];
  uint32_t i;
  uint32_t idx; // current byte count
  
  /////////////////////////////
  // Configure here
  bool xy_present = true;
  bool rpt_tile = true;
  x /= 8;
  y /= 8;
  uint32_t rpt_in_x = x + w/8 - 1;
  uint32_t rpt_in_y = y + h/8 - 1;
  
  // 2-color palette
  // opcode
  // [0] x,y present
  // [1] repeat
  // [2] palette present
  data[0] = (0x14 | // 2-color tile mode
	     (xy_present ? 0x01 : 0x00) |
	     (rpt_tile ? 0x02 : 0x00)
	     );
  idx = 0;

  // x/y coordinates
  if(xy_present) {
    data[1] = x; // x 
    data[2] = y; // y
    idx = 2;
  }
  
  // repeat tile
  if(rpt_tile) {
    data[idx+1] = rpt_in_x; // x repeat
    data[idx+2] = rpt_in_y; // y repeat
    idx=idx+2;
  }

  // setup palette
  printf ("idx is = %d, when setting up palette\n",idx);
  data[idx+1] = color[0] ;// palette color0 B
  data[idx+2] = color[1] ;// palette color0 G
  data[idx+3] = color[2] ;// palette color0 R
  data[idx+4] = color[0] ;// palette color1 B
  data[idx+5] = color[1] ;// palette color1 G
  data[idx+6] = color[2] ;// palette color1 R
  idx = idx + 6;

  // 64-pixel data
  // 8 bytes worth
  for(i=0;i<8;i++) {
    data[idx+i+1] = 0xc3;
  }
  idx = idx + 8;

  for(i=0;i<=idx;i++) {
    printf("data of %d = 0x%x\n",i, data[i]);
  }
  
  sStream->epWrite(videoDrv->Endpoint, data, idx + 1);
  FLUSH;

  // 4-color palette

}

void ddrDbgClearScreen_2() {
    char color_black[3] = {0,0,0};
    vdcWriteRect(0,0,1920,1200,color_black);
}

void vdcWriteTiles() {
    // VGA Write through normal VDC path
    uint32_t x,y;
    uint8_t red,green,blue;
    uint32_t data;
    bool color_swap;
    bool mode16b;

    // for debug csr
    //EP0WRITE(0x1021,0x7); // VDC debug reg

    EP0WRITE(0x1001,0x31); // enable VDC
    //printf("READ CSR VDC enable reg: 0x%08x\n", EP0READ(0x1001));
    printf("Enabling VDC...\n");
    EP0FLUSH;

    // read if doing 16-bit mode or color swap mode
    data = EP0READ(0x1001);
    color_swap =(((data & 0x20) >> 5) == 1);
    mode16b = (((data & 0x10) >> 4) == 1);

    // don't matter if enable VGA or not
    EP0WRITE(0x3001,0x1); // enable Vga
    EP0FLUSH;

    printf ("VGA write\n");
    for (x=0;x<32;x+=2) {
        for (y=0;y<32;y+=2) {
            red = 0x0;
            green = 0xff;
            blue = 0xff;

            // assemble message for VDC to process
            char data[1024];
            data[0] = x & 0xff;
            data[1] = y & 0xff;
            data[2] = (((x & 0xf00) >> 4) | 
                    (y & 0xf00) >> 8) ;
            data[3] = 0x3f;         // [7] = Repeat;  [5:0] rpt length - 1
	    if(mode16b) {
	      for (uint32_t i=0;i<64;i++) {
		if(color_swap) {
		  data[4+i*2] = ((blue & 0xf8) >> 3) | ((green & 0x1c) << 3);
		  data[5+i*2] = ((green & 0xe0) >> 5) | ((red & 0xf8) );
		}
		else {
		  data[4+i*2] = ((green & 0xe0) >> 5) | ((red & 0xf8) );
		  data[5+i*2] = ((blue & 0xf8) >> 3) | ((green & 0x1c) << 3);
		}
	      }
	      sStream->epWrite(videoDrv->Endpoint, data, 64*2 + 4);
	    } else {
	      for (uint32_t i=0;i<64;i++) {
		if(color_swap) {
		  data[4+i*3] = blue;
		  data[5+i*3] = green;
		  data[6+i*3] = red;
		}
		else {
		  data[4+i*3] = red;
		  data[5+i*3] = green;
		  data[6+i*3] = blue;
		}
	      }
	      sStream->epWrite(videoDrv->Endpoint, data, 64*3 + 4);
	    }

            //printf ("Sent VDC msg to x=%0d,y=%0d\n",x,y);
            FLUSH;

        }
    }


    printf ("16-bit color mode = %x, color_swap mode = %x \n", mode16b, color_swap);

    printf ("sleeping before VGA enable\n");
    sleep (2);

}

void vdcWriteTiles2() {
    // VGA Write through normal VDC path
    uint32_t x,y;
    uint8_t red,green,blue;
    uint32_t data;
    bool color_swap;
    bool mode16b;

    // for debug csr
    //EP0WRITE(0x1021,0x7); // VDC debug reg

    EP0WRITE(0x1001,0x31); // enable VDC
    //printf("READ CSR VDC enable reg: 0x%08x\n", EP0READ(0x1001));
    printf("Enabling VDC...\n");
    EP0FLUSH;

    // read if doing 16-bit mode or color swap mode
    data = EP0READ(0x1001);
    color_swap =(((data & 0x20) >> 5) == 1);
    mode16b = (((data & 0x10) >> 4) == 1);

    // don't matter if enable VGA or not
    EP0WRITE(0x3001,0x1); // enable Vga
    EP0FLUSH;

    printf ("VGA write\n");
    for (x=0;x<8;x+=2) {
        for (y=0;y<8;y+=2) {
            red = 0x00;
            green = 0xff;
            blue = 0xff;

            // assemble message for VDC to process
            char data[1024];
            data[0] = x & 0xff;
            data[1] = y & 0xff;
            data[2] = (((x & 0xf00) >> 4) | 
		       (y & 0xf00) >> 8) ;
            data[3] = 0xbf;         // [7] = Repeat;  [5:0] rpt length - 1
	    if(mode16b) {
	      for (uint32_t i=0;i<1;i++) {
		if(color_swap) {
		  data[4+i*2] = ((blue & 0xf8) >> 3) | ((green & 0x1c) << 3);
		  data[5+i*2] = ((green & 0xe0) >> 5) | ((red & 0xf8) );
		}
		else {
		  data[4+i*2] = ((green & 0xe0) >> 5) | ((red & 0xf8) );
		  data[5+i*2] = ((blue & 0xf8) >> 3) | ((green & 0x1c) << 3);
		}
	      }
	      sStream->epWrite(videoDrv->Endpoint, data, 1*2 + 4);
	    } else {
	      for (uint32_t i=0;i<1;i++) {
		if(color_swap) {
		  data[4+i*3] = blue;
		  data[5+i*3] = green;
		  data[6+i*3] = red;
		}
		else {
		  data[4+i*3] = red;
		  data[5+i*3] = green;
		  data[6+i*3] = blue;
		}
	      }
	      sStream->epWrite(videoDrv->Endpoint, data, 1*3 + 4);
	    }

           // printf ("Sent VDC msg to x=%0d,y=%0d\n",x,y);
            FLUSH;

        }
    }


    printf ("16-bit color mode = %x, color_swap mode = %x \n", mode16b, color_swap);

    printf ("sleeping before VGA enable\n");
    sleep (2);

}

void vdcWriteTilesNonRpt() {
    // VGA Write through normal VDC path
    uint32_t x,y;
    uint8_t red,green,blue;
    uint32_t data;
    bool color_swap;
    bool mode16b;

    // for debug csr
    //EP0WRITE(0x1021,0x7); // VDC debug reg

    EP0WRITE(0x1001,0x31); // enable VDC
    //printf("READ CSR VDC enable reg: 0x%08x\n", EP0READ(0x1001));
    printf("Enabling VDC...\n");
    EP0FLUSH;

    // read if doing 16-bit mode or color swap mode
    data = EP0READ(0x1001);
    color_swap =(((data & 0x20) >> 5) == 1);
    mode16b = (((data & 0x10) >> 4) == 1);
    uint32_t scale_factor = 0;

    // don't matter if enable VGA or not
    EP0WRITE(0x3001,0x1); // enable Vga
    EP0FLUSH;

    printf ("VGA write with non-repeating pixels\n");
    for (x=0;x<32;x+=2) {
        for (y=0;y<32;y+=2) {
            red = 0x00;
            green = 0xbf; //0xbf;
            blue = 0x00; //0x8f;

            // assemble message for VDC to process
            char data[1024];
            data[0] = x & 0xff;
            data[1] = y & 0xff;
            data[2] = (((x & 0xf00) >> 4) | 
		       (y & 0xf00) >> 8) ;
            data[3] = 0x3f;         // [7] = Repeat;  [5:0] rpt length - 1
	    if(mode16b) {
	      for (uint32_t i=0;i<64;i++) {
		if(color_swap) {
		  data[4+i*2] = (((blue - scale_factor *i) & 0xf8) >> 3) | (((green - scale_factor*i) & 0x1c) << 3);
		  data[5+i*2] = (((green - scale_factor*i) & 0xe0) >> 5) | (((red - scale_factor*i) & 0xf8) );
		}
		else {
		  data[4+i*2] = (((green - scale_factor*i) & 0xe0) >> 5) | (((red - scale_factor*i) & 0xf8) );
		  data[5+i*2] = (((blue - scale_factor*i) & 0xf8) >> 3) | (((green - scale_factor*i) & 0x1c) << 3);
		}
	      }
	      sStream->epWrite(videoDrv->Endpoint, data, 64*2 + 4);
	    } else {
	      for (uint32_t i=0;i<64;i++) {
		if(color_swap) {
		  data[4+i*3] = blue - scale_factor*i;
		  data[5+i*3] = green - scale_factor*i;
		  data[6+i*3] = red - scale_factor*i;
		}
		else {
		  data[4+i*3] = red - scale_factor*i;
		  data[5+i*3] = green - scale_factor*i;
		  data[6+i*3] = blue - scale_factor*i;
		}
	      }
	      sStream->epWrite(videoDrv->Endpoint, data, 64*3 + 4);
	    }

            printf ("Sent VDC msg to x=%0d,y=%0d\n",x,y);
            FLUSH;

        }
    }

    printf ("16-bit color mode = %x, color_swap mode = %x \n", mode16b, color_swap);

    printf ("sleeping before VGA enable\n");
    sleep (2);

}

vector<char> net2vec(uint32_t word, int n) 
{
    vector<char> ret;
    for (int i=0; i<n; i++) {
        ret.push_back((word>>((n-i-1)*8)) & 0xff);
    }
    return ret;
}


void vdcSetDest(int origin, int stride) {

    vector<char> data;
    vector<char> v;

    data.push_back(0x90); 
    v = net2vec(origin,4);
    copy(v.begin(),v.end(),back_inserter(data));
    v = net2vec(stride,2);
    copy(v.begin(),v.end(),back_inserter(data));

    char *cdata;
    cdata = new char [data.size()];
    copy(data.begin(), data.end(), cdata);
    sStream->epWrite(videoDrv->Endpoint, cdata, data.size());
    FLUSH;
    delete [] cdata;
}

void vdcMemBlit(int origin, int stride, int src_x, int src_y, int w, int h, int dst_x, int dst_y, int mix_mask) {

    vector<char> data;
    vector<char> v;

    data.push_back(0xa0);
    v = net2vec(origin,4);
    copy(v.begin(),v.end(),back_inserter(data));

    v = net2vec(stride, 2);
    copy(v.begin(),v.end(),back_inserter(data));

    v = net2vec(src_x, 2);
    copy(v.begin(),v.end(),back_inserter(data));

    v = net2vec(src_y, 2);
    copy(v.begin(),v.end(),back_inserter(data));

    v = net2vec(w,2);
    copy(v.begin(),v.end(),back_inserter(data));

    v = net2vec(h,2);
    copy(v.begin(),v.end(),back_inserter(data));

    v = net2vec(dst_x,2);
    copy(v.begin(),v.end(),back_inserter(data));

    v = net2vec(dst_y,2);
    copy(v.begin(),v.end(),back_inserter(data));

//    v = net2vec(mix_mask,1);
//    copy(v.begin(),v.end(),back_inserter(data));

    char *cdata;
    cdata = new char [data.size()];
    copy(data.begin(), data.end(), cdata);
    sStream->epWrite(videoDrv->Endpoint, cdata, data.size());
    delete [] cdata;
}

void vdcCursorBlit(int origin) {

    vector<char> data;
    vector<char> v;

    data.push_back(0xb0);
    v = net2vec(origin, 4);
    copy(v.begin(),v.end(),back_inserter(data));

    char *cdata;
    cdata = new char [data.size()];
    copy(data.begin(), data.end(), cdata);
    sStream->epWrite(videoDrv->Endpoint, cdata, data.size());
    delete [] cdata;
}

void vdcFillRect(int x, int y, int w, int h, vector<char> &color) {

    vector<char> data;
    vector<char> v;

    data.push_back(0xc0 | (color.size()/3 == 2 ? 0x8 : 0x0));

    v = net2vec(x,2);
    copy(v.begin(),v.end(),back_inserter(data));
    v = net2vec(y,2);
    copy(v.begin(),v.end(),back_inserter(data));
    v = net2vec(w,2);
    copy(v.begin(),v.end(),back_inserter(data));
    v = net2vec(h,2);
    copy(v.begin(),v.end(),back_inserter(data));


    for (int i=0; i<color.size(); i++) {
        data.push_back(color[i]);
    }

    char *cdata;
    cdata = new char [data.size()];
    copy(data.begin(), data.end(), cdata);
    sStream->epWrite(videoDrv->Endpoint, cdata, data.size());
    delete [] cdata;
}

void vdc15Write2ColorTileWithColor(int x, int y, int mask, vector<int> color_pal) {

  EP0WRITE(0x1001, 0x23);
  printf("Enabling VDC 1.5...\n");
  EP0FLUSH;

  char data[256];
  uint32_t i;
  uint32_t idx; // current byte count
  
  /////////////////////////////
  // Configure here
  bool xy_present = true;
  bool rpt_tile = true;
  uint32_t rpt_in_x = 20;
  uint32_t rpt_in_y = 20;
  
  // 2-color palette
  // opcode
  // [0] x,y present
  // [1] repeat
  // [2] palette present
  data[0] = (0x14 | // 2-color tile mode
	     (xy_present ? 0x01 : 0x00) |
	     (rpt_tile ? 0x02 : 0x00)
	     );
  idx = 0;

  // x/y coordinates
  if(xy_present) {
    data[1] = x; // x 
    data[2] = y; // y
    idx = 2;
  }
  
  // repeat tile
  if(rpt_tile) {
    data[idx+1] = x+rpt_in_x; // x repeat
    data[idx+2] = y+rpt_in_y; // y repeat
    idx=idx+2;
  }

  // setup palette
  printf ("idx is = %d, when setting up palette\n",idx);
  for (int i=0; i<6; i++) 
      data[idx+1+i] = color_pal[i];

  idx = idx + 6;

  // 64-pixel data
  // 8 bytes worth
  for(i=0;i<8;i++) {
    data[idx+i+1] = mask;
  }
  idx = idx + 8;

  for(i=0;i<=idx;i++) {
    printf("data of %d = 0x%x\n",i, data[i]);
  }
  
  for (int i=0; i<idx+1; i++) {
      printf("%x ", data[i] & 0xff);
  }
  printf("\n");
  sStream->epWrite(videoDrv->Endpoint, data, idx + 1);
  FLUSH;

  // 4-color palette
}

vector<int> green_blue_pal() {
    vector<int> color_pal(6);
    color_pal[0] = 0xff ;// palette color0 B
    color_pal[1] = 0x04 ;// palette color0 G
    color_pal[2] = 0x00 ;// palette color0 R
    color_pal[3] = 0x00 ;// palette color1 B
    color_pal[4] = 0xff ;// palette color1 G
    color_pal[5] = 0x00 ;// palette color1 R
    return color_pal;
}
void vdc15Write2ColorTile(int x=0, int y=0, int mask=0x33) {
    vector<int> color_pal(6);
    color_pal[0] = 0xff ;// palette color0 B
    color_pal[1] = 0x00 ;// palette color0 G
    color_pal[2] = 0x00 ;// palette color0 R
    color_pal[3] = 0x00 ;// palette color1 B
    color_pal[4] = 0x00 ;// palette color1 G
    color_pal[5] = 0xff ;// palette color1 R

    vdc15Write2ColorTileWithColor(x, y, mask, color_pal);
}




void vdcWriteTestDdr() {
    // VGA Write through normal VDC path
    uint32_t x,y;
    uint8_t red,green,blue;
    uint32_t tmp;
    uint32_t rightTileNum;
    uint32_t bottomTileNum;
    uint32_t hvispels;
    uint32_t vvispels;

    hvispels = (EP0READ(0x3003) & 0xfff) + 1;
    vvispels = (EP0READ(0x3004) & 0xfff) + 1;

    rightTileNum = (hvispels / 8)- 1;
    bottomTileNum = (vvispels / 8) - 1;

    // for debug csr
    //EP0WRITE(0x1021,0x7); // VDC debug reg


    EP0WRITE(0x1001,0x1); // enable VDC
    //printf("READ CSR VDC enable reg: 0x%08x\n", EP0READ(0x1001));
    //printf("Enabling VDC...\n");
    EP0FLUSH;

    // don't matter if enable VGA or not
    EP0WRITE(0x3001,0x1); // enable Vga
    EP0FLUSH;

    //printf ("VGA write, write 0 to clear\n");
    for (x=0;x<=rightTileNum;x++) {
        for (y=0;y<=bottomTileNum;y++) {
            red = 0x00;
            green = 0xff;
            blue = 0xff;

            // assemble message for VDC to process
            char data[1024];
            data[0] = x & 0xff;
            data[1] = y & 0xff;
            data[2] = (((x & 0xf00) >> 4) | 
                    (y & 0xf00) >> 8) ;
            data[3] = 0x3f;         // [7] = Repeat;  [5:0] rpt length - 1
            for (uint32_t i=0;i<8;i++) {
	      data[4+i*24] =  0x00; // R pix0
	      data[5+i*24] =  0x00; // G
	      data[6+i*24] =  0x00; // B
	      data[7+i*24] =  0x00; // R pix1
	      data[8+i*24] =  0x00; // G
	      data[9+i*24] =  0x00; // B
	      data[10+i*24] = 0x00; // R pix2
	      data[11+i*24] = 0x00; // G
	      data[12+i*24] = 0x00; // B
	      data[13+i*24] = 0x00; // R pix3
	      data[14+i*24] = 0x00; // G
	      data[15+i*24] = 0x00; // B
	      data[16+i*24] = 0x00; // R pix4
	      data[17+i*24] = 0x00; // G
	      data[18+i*24] = 0x00; // B
	      data[19+i*24] = 0x00; // R pix5
	      data[20+i*24] = 0x00; // G
	      data[21+i*24] = 0x00; // B
	      data[22+i*24] = 0x00; // R pix6
	      data[23+i*24] = 0x00; // G
	      data[24+i*24] = 0x00; // B
	      data[25+i*24] = 0x00; // R pix7
	      data[26+i*24] = 0x00; // G
	      data[27+i*24] = 0x00; // B
            }

            // Ideally, we should be using a higher level API in VideoDisplayDriver to write to the Frame Buffer on the Client
            sStream->epWrite(videoDrv->Endpoint, data, 64*3 + 4);
            //cli->sendMessage(1, data, 64*3 + 4);
            //printf ("Sent VDC msg to x=%0d,y=%0d\n",x,y);
	    // congestion control works
	    tmp = EP0READ(0x3001); // VGA Enable pattern
            FLUSH;
        }
    }

    //printf ("VGA write, write a block\n");
    
    //while(1) {
    for (x=0;x<=rightTileNum;x++) {
        for (y=0;y<=bottomTileNum;y++) {
            // assemble message for VDC to process
            char data[1024];
            data[0] = x & 0xff;
            data[1] = y & 0xff;
            data[2] = (((x & 0xf00) >> 4) | 
                    (y & 0xf00) >> 8) ;
            data[3] = 0x3f;         // [7] = Repeat;  [5:0] rpt length - 1
            for (uint32_t i=0;i<8;i++) {
	      data[4+i*24] =  0xaa; // R pix0
	      data[5+i*24] =  0xaa; // G
	      data[6+i*24] =  0xaa; // B
	      data[7+i*24] =  0xaa; // R pix1
	      data[8+i*24] =  0x55; // G
	      data[9+i*24] =  0x55; // B
	      data[10+i*24] = 0x55; // R pix2
	      data[11+i*24] = 0x55; // G
	      data[12+i*24] = 0xaa; // B
	      data[13+i*24] = 0xaa; // R pix3
	      data[14+i*24] = 0xaa; // G
	      data[15+i*24] = 0xaa; // B
	      data[16+i*24] = 0x55; // R pix4
	      data[17+i*24] = 0x55; // G
	      data[18+i*24] = 0x55; // B
	      data[19+i*24] = 0x55; // R pix5
	      data[20+i*24] = 0xaa; // G
	      data[21+i*24] = 0xaa; // B
	      data[22+i*24] = 0xaa; // R pix6
	      data[23+i*24] = 0xaa; // G
	      data[24+i*24] = 0x55; // B
	      data[25+i*24] = 0x55; // R pix7
	      data[26+i*24] = 0x55; // G
	      data[27+i*24] = 0x55; // B
            }

            // Ideally, we should be using a higher level API in VideoDisplayDriver to write to the Frame Buffer on the Client
            sStream->epWrite(videoDrv->Endpoint, data, 64*3 + 4);
            //cli->sendMessage(1, data, 64*3 + 4);
         //   printf ("Sent VDC msg to x=%0d,y=%0d\n",x,y);
	    // Choonc : added the read to slow down test so that
	    // manufacturing diags will pass. To be removed when 
	    // congestion control works
	    tmp = EP0READ(0x3001); // VGA Enable pattern
            FLUSH;
        }
    }
    //}
    

    //printf ("sleeping before VGA enable\n");
    //sleep (2);

}

void vdcWriteTestDdr2() {
    // VGA Write through normal VDC path
    uint32_t x,y;
    uint8_t red,green,blue;
    uint32_t tmp;
    uint32_t rightTileNum;
    uint32_t bottomTileNum;
    uint32_t hvispels;
    uint32_t vvispels;

    hvispels = (EP0READ(0x3003) & 0xfff) + 1;
    vvispels = (EP0READ(0x3004) & 0xfff) + 1;

    rightTileNum = (hvispels / 8)- 1;
    bottomTileNum = (vvispels / 8) - 1;

    // for debug csr
    //EP0WRITE(0x1021,0x7); // VDC debug reg


    EP0WRITE(0x1001,0x1); // enable VDC
    //printf("READ CSR VDC enable reg: 0x%08x\n", EP0READ(0x1001));
    //printf("Enabling VDC...\n");
    EP0FLUSH;

    // don't matter if enable VGA or not
    EP0WRITE(0x3001,0x1); // enable Vga
    EP0FLUSH;

    //printf ("VGA write, write 0 to clear\n");
    for (x=0;x<=rightTileNum;x++) {
        for (y=0;y<=bottomTileNum;y++) {
            red = 0x00;
            green = 0xff;
            blue = 0xff;

            // assemble message for VDC to process
            char data[1024];
            data[0] = x & 0xff;
            data[1] = y & 0xff;
            data[2] = (((x & 0xf00) >> 4) | 
                    (y & 0xf00) >> 8) ;
            data[3] = 0x3f;         // [7] = Repeat;  [5:0] rpt length - 1
            for (uint32_t i=0;i<8;i++) {
	      data[4+i*24] =  0x00; // R pix0
	      data[5+i*24] =  0x00; // G
	      data[6+i*24] =  0x00; // B
	      data[7+i*24] =  0x00; // R pix1
	      data[8+i*24] =  0x00; // G
	      data[9+i*24] =  0x00; // B
	      data[10+i*24] = 0x00; // R pix2
	      data[11+i*24] = 0x00; // G
	      data[12+i*24] = 0x00; // B
	      data[13+i*24] = 0x00; // R pix3
	      data[14+i*24] = 0x00; // G
	      data[15+i*24] = 0x00; // B
	      data[16+i*24] = 0x00; // R pix4
	      data[17+i*24] = 0x00; // G
	      data[18+i*24] = 0x00; // B
	      data[19+i*24] = 0x00; // R pix5
	      data[20+i*24] = 0x00; // G
	      data[21+i*24] = 0x00; // B
	      data[22+i*24] = 0x00; // R pix6
	      data[23+i*24] = 0x00; // G
	      data[24+i*24] = 0x00; // B
	      data[25+i*24] = 0x00; // R pix7
	      data[26+i*24] = 0x00; // G
	      data[27+i*24] = 0x00; // B
            }

            // Ideally, we should be using a higher level API in VideoDisplayDriver to write to the Frame Buffer on the Client
            sStream->epWrite(videoDrv->Endpoint, data, 64*3 + 4);
            //cli->sendMessage(1, data, 64*3 + 4);
            //printf ("Sent VDC msg to x=%0d,y=%0d\n",x,y);
	    // congestion control works
	    tmp = EP0READ(0x3001); // VGA Enable pattern
            FLUSH;
        }
    }

    //printf ("VGA write, write a block\n");
    
    //while(1) {
    for (x=0;x<=rightTileNum;x++) {
        for (y=0;y<=bottomTileNum;y++) {
            // assemble message for VDC to process
            char data[1024];
            data[0] = x & 0xff;
            data[1] = y & 0xff;
            data[2] = (((x & 0xf00) >> 4) | 
                    (y & 0xf00) >> 8) ;
            data[3] = 0x3f;         // [7] = Repeat;  [5:0] rpt length - 1
            for (uint32_t i=0;i<8;i++) {
	      data[4+i*24] =  0x55; // R pix0
	      data[5+i*24] =  0x55; // G
	      data[6+i*24] =  0x55; // B
	      data[7+i*24] =  0x55; // R pix1
	      data[8+i*24] =  0xaa; // G
	      data[9+i*24] =  0xaa; // B
	      data[10+i*24] = 0xaa; // R pix2
	      data[11+i*24] = 0xaa; // G
	      data[12+i*24] = 0x55; // B
	      data[13+i*24] = 0x55; // R pix3
	      data[14+i*24] = 0x55; // G
	      data[15+i*24] = 0x55; // B
	      data[16+i*24] = 0xaa; // R pix4
	      data[17+i*24] = 0xaa; // G
	      data[18+i*24] = 0xaa; // B
	      data[19+i*24] = 0xaa; // R pix5
	      data[20+i*24] = 0x55; // G
	      data[21+i*24] = 0x55; // B
	      data[22+i*24] = 0x55; // R pix6
	      data[23+i*24] = 0x55; // G
	      data[24+i*24] = 0xaa; // B
	      data[25+i*24] = 0xaa; // R pix7
	      data[26+i*24] = 0xaa; // G
	      data[27+i*24] = 0xaa; // B
            }

            // Ideally, we should be using a higher level API in VideoDisplayDriver to write to the Frame Buffer on the Client
            sStream->epWrite(videoDrv->Endpoint, data, 64*3 + 4);
            //cli->sendMessage(1, data, 64*3 + 4);
          //  printf ("Sent VDC msg to x=%0d,y=%0d\n",x,y);
	    // Choonc : added the read to slow down test so that
	    // manufacturing diags will pass. To be removed when 
	    // congestion control works
	    tmp = EP0READ(0x3001); // VGA Enable pattern
            FLUSH;
        }
    }
    //}
    

    //printf ("sleeping before VGA enable\n");
    //sleep (2);

}

void vdcWriteTestVga() {
    // VGA Write through normal VDC path
    uint32_t x,y;
    uint32_t i;
    uint8_t red,green,blue;

    EP0WRITE(0x1001,0x1); // enable VDC
    //printf("READ CSR VDC enable reg: 0x%08x\n", EP0READ(0x1001));
    printf("Enabling VDC...\n");
    EP0FLUSH;

    // don't matter if enable VGA or not
    EP0WRITE(0x3001,0x1); // enable Vga
    EP0FLUSH;


    for (x=0;x<200;x++) {
        for (y=0;y<150;y++) {
            // assemble message for VDC to process
            char data[1024];
            data[0] = x & 0xff;
            data[1] = y & 0xff;
            data[2] = (((x & 0xf00) >> 4) | 
                    (y & 0xf00) >> 8) ;
            data[3] = 0x3f;         // [7] = Repeat;  [5:0] rpt length - 1
	    if(x==0 && y==0) { // upper left
	      data[4] =  0xff; // R pix0 word0 rising byte0
	      data[5] =  0xff; // G
	      data[6] =  0xff; // B

	      data[7] =  0xff; // R pix1
	      data[8] =  0xff; // G falling byte0
	      data[9] =  0xff; // B

	      data[10] = 0xff; // R pix2
	      data[11] = 0xff; // G
	      data[12] = 0xff; // B word1 rising byte0

	      data[13] = 0xff; // R pix3
	      data[14] = 0xff; // G
	      data[15] = 0xff; // B

	      data[16] = 0xff; // R pix4 falling byte0
	      data[17] = 0xff; // G
	      data[18] = 0xff; // B

	      data[19] = 0xff; // R pix5
	      data[20] = 0xff; // G word2 rising byte0
	      data[21] = 0xff; // B

	      data[22] = 0xff; // R pix6
	      data[23] = 0xff; // G
	      data[24] = 0xff; // B falling byte0

	      data[25] = 0xff; // R pix7
	      data[26] = 0xff; // G
	      data[27] = 0xff; // B

	      data[28] = 0xff; // R pix8 word3 rising byte0
	      data[29] = 0xff; // G
	      data[30] = 0xff; // B

	      data[31] = 0x00; // R pix9
	      data[32] = 0x00; // G falling byte0
	      data[33] = 0x00; // B

	      data[34] = 0x00; // R pix10
	      data[35] = 0x00; // G
	      data[36] = 0x00; // B word4 rising byte0

	      data[37] = 0x00; // R pix11
	      data[38] = 0x00; // G
	      data[39] = 0x00; // B

	      data[40] = 0x00; // R pix12 falling byte0
	      data[41] = 0x00; // G
	      data[42] = 0x00; // B

	      data[43] = 0x00; // R pix13
	      data[44] = 0x00; // G word5 rising byte0
	      data[45] = 0x00; // B

	      data[46] = 0x00; // R pix14
	      data[47] = 0x00; // G 
	      data[48] = 0x00; // B falling byte0

	      data[49] = 0x00; // R pix15
	      data[50] = 0x00; // G
	      data[51] = 0x00; // B

	      data[52] = 0xff; // R pix16 word3 rising byte0
	      data[53] = 0xff; // G
	      data[54] = 0xff; // B

	      data[55] = 0x00; // R pix17
	      data[56] = 0x00; // G falling byte0
	      data[57] = 0x00; // B

	      data[58] = 0xff; // R pix18
	      data[59] = 0xff; // G
	      data[60] = 0xff; // B word4 rising byte0

	      data[61] = 0xff; // R pix19
	      data[62] = 0xff; // G
	      data[63] = 0xff; // B

	      data[64] = 0xff; // R pix20 falling byte0
	      data[65] = 0xff; // G
	      data[66] = 0xff; // B

	      data[67] = 0xff; // R pix21
	      data[68] = 0xff; // G word5 rising byte0
	      data[69] = 0xff; // B

	      data[70] = 0xff; // R pix22
	      data[71] = 0xff; // G 
	      data[72] = 0xff; // B falling byte0

	      data[73] = 0xff; // R pix23
	      data[74] = 0xff; // G
	      data[75] = 0xff; // B

	      for(i=0;i<5;i++) { // all black
		data[76+i*24] = 0xff; // R pix0 word0 rising byte0
		data[77+i*24] = 0xff; // G
		data[78+i*24] = 0xff; // B

		data[79+i*24] = 0x00; // R pix1
		data[80+i*24] = 0x00; // G falling byte0
		data[81+i*24] = 0x00; // B

		data[82+i*24] = 0xff; // R pix2
		data[83+i*24] = 0xff; // G
		data[84+i*24] = 0xff; // B word1 rising byte0

		data[85+i*24] = 0x00; // R pix3
		data[86+i*24] = 0x00; // G
		data[87+i*24] = 0x00; // B

		data[88+i*24] = 0x00; // R pix4 falling byte0
		data[89+i*24] = 0x00; // G
		data[90+i*24] = 0x00; // B

		data[91+i*24] = 0x00; // R pix5
		data[92+i*24] = 0x00; // G word2 rising byte0
		data[93+i*24] = 0x00; // B

		data[94+i*24] = 0x00; // R pix6
		data[95+i*24] = 0x00; // G
		data[96+i*24] = 0x00; // B falling byte0

		data[97+i*24] = 0x00; // R pix7
		data[98+i*24] = 0x00; // G
		data[99+i*24] = 0x00; // B
	      }
	    }
	    else if(x==199 && y==0) { // upper right
	      data[4] =  0xff; // R pix0 word0 rising byte0
	      data[5] =  0xff; // G
	      data[6] =  0xff; // B

	      data[7] =  0xff; // R pix1
	      data[8] =  0xff; // G falling byte0
	      data[9] =  0xff; // B

	      data[10] = 0xff; // R pix2
	      data[11] = 0xff; // G
	      data[12] = 0xff; // B word1 rising byte0

	      data[13] = 0xff; // R pix3
	      data[14] = 0xff; // G
	      data[15] = 0xff; // B

	      data[16] = 0xff; // R pix4 falling byte0
	      data[17] = 0xff; // G
	      data[18] = 0xff; // B

	      data[19] = 0xff; // R pix5
	      data[20] = 0xff; // G word2 rising byte0
	      data[21] = 0xff; // B

	      data[22] = 0xff; // R pix6
	      data[23] = 0xff; // G
	      data[24] = 0xff; // B falling byte0

	      data[25] = 0xff; // R pix7
	      data[26] = 0xff; // G
	      data[27] = 0xff; // B

	      data[28] = 0x00; // R pix8 word3 rising byte0
	      data[29] = 0x00; // G
	      data[30] = 0x00; // B

	      data[31] = 0x00; // R pix9
	      data[32] = 0x00; // G falling byte0
	      data[33] = 0x00; // B

	      data[34] = 0x00; // R pix10
	      data[35] = 0x00; // G
	      data[36] = 0x00; // B word4 rising byte0

	      data[37] = 0x00; // R pix11
	      data[38] = 0x00; // G
	      data[39] = 0x00; // B

	      data[40] = 0x00; // R pix12 falling byte0
	      data[41] = 0x00; // G
	      data[42] = 0x00; // B

	      data[43] = 0x00; // R pix13
	      data[44] = 0x00; // G word5 rising byte0
	      data[45] = 0x00; // B

	      data[46] = 0x00; // R pix14
	      data[47] = 0x00; // G 
	      data[48] = 0x00; // B falling byte0

	      data[49] = 0xff; // R pix15
	      data[50] = 0xff; // G
	      data[51] = 0xff; // B

	      data[52] = 0xff; // R pix16 word3 rising byte0
	      data[53] = 0xff; // G
	      data[54] = 0xff; // B

	      data[55] = 0xff; // R pix17
	      data[56] = 0xff; // G falling byte0
	      data[57] = 0xff; // B

	      data[58] = 0xff; // R pix18
	      data[59] = 0xff; // G
	      data[60] = 0xff; // B word4 rising byte0

	      data[61] = 0xff; // R pix19
	      data[62] = 0xff; // G
	      data[63] = 0xff; // B

	      data[64] = 0xff; // R pix20 falling byte0
	      data[65] = 0xff; // G
	      data[66] = 0xff; // B

	      data[67] = 0xff; // R pix21
	      data[68] = 0xff; // G word5 rising byte0
	      data[69] = 0xff; // B

	      data[70] = 0x00; // R pix22
	      data[71] = 0x00; // G 
	      data[72] = 0x00; // B falling byte0

	      data[73] = 0xff; // R pix23
	      data[74] = 0xff; // G
	      data[75] = 0xff; // B

	      for(i=0;i<5;i++) { // all black
		data[76+i*24] = 0x00; // R pix0 word0 rising byte0
		data[77+i*24] = 0x00; // G
		data[78+i*24] = 0x00; // B

		data[79+i*24] = 0x00; // R pix1
		data[80+i*24] = 0x00; // G falling byte0
		data[81+i*24] = 0x00; // B

		data[82+i*24] = 0x00; // R pix2
		data[83+i*24] = 0x00; // G
		data[84+i*24] = 0x00; // B word1 rising byte0

		data[85+i*24] = 0x00; // R pix3
		data[86+i*24] = 0x00; // G
		data[87+i*24] = 0x00; // B

		data[88+i*24] = 0x00; // R pix4 falling byte0
		data[89+i*24] = 0x00; // G
		data[90+i*24] = 0x00; // B

		data[91+i*24] = 0xff; // R pix5
		data[92+i*24] = 0xff; // G word2 rising byte0
		data[93+i*24] = 0xff; // B

		data[94+i*24] = 0x00; // R pix6
		data[95+i*24] = 0x00; // G
		data[96+i*24] = 0x00; // B falling byte0

		data[97+i*24] = 0xff; // R pix7
		data[98+i*24] = 0xff; // G
		data[99+i*24] = 0xff; // B
	      }
	    }
	    else if(y==0) { // top row
	      data[4] =  0xff; // R pix0 word0 rising byte0
	      data[5] =  0xff; // G
	      data[6] =  0xff; // B

	      data[7] =  0xff; // R pix1
	      data[8] =  0xff; // G falling byte0
	      data[9] =  0xff; // B

	      data[10] = 0xff; // R pix2
	      data[11] = 0xff; // G
	      data[12] = 0xff; // B word1 rising byte0

	      data[13] = 0xff; // R pix3
	      data[14] = 0xff; // G
	      data[15] = 0xff; // B

	      data[16] = 0xff; // R pix4 falling byte0
	      data[17] = 0xff; // G
	      data[18] = 0xff; // B

	      data[19] = 0xff; // R pix5
	      data[20] = 0xff; // G word2 rising byte0
	      data[21] = 0xff; // B

	      data[22] = 0xff; // R pix6
	      data[23] = 0xff; // G
	      data[24] = 0xff; // B falling byte0

	      data[25] = 0xff; // R pix7
	      data[26] = 0xff; // G
	      data[27] = 0xff; // B

	      data[28] = 0x00; // R pix8 word3 rising byte0
	      data[29] = 0x00; // G
	      data[30] = 0x00; // B

	      data[31] = 0x00; // R pix9
	      data[32] = 0x00; // G falling byte0
	      data[33] = 0x00; // B

	      data[34] = 0x00; // R pix10
	      data[35] = 0x00; // G
	      data[36] = 0x00; // B word4 rising byte0

	      data[37] = 0x00; // R pix11
	      data[38] = 0x00; // G
	      data[39] = 0x00; // B

	      data[40] = 0x00; // R pix12 falling byte0
	      data[41] = 0x00; // G
	      data[42] = 0x00; // B

	      data[43] = 0x00; // R pix13
	      data[44] = 0x00; // G word5 rising byte0
	      data[45] = 0x00; // B

	      data[46] = 0x00; // R pix14
	      data[47] = 0x00; // G 
	      data[48] = 0x00; // B falling byte0

	      data[49] = 0x00; // R pix15
	      data[50] = 0x00; // G
	      data[51] = 0x00; // B

	      data[52] = 0xff; // R pix16 word3 rising byte0
	      data[53] = 0xff; // G
	      data[54] = 0xff; // B

	      data[55] = 0xff; // R pix17
	      data[56] = 0xff; // G falling byte0
	      data[57] = 0xff; // B

	      data[58] = 0xff; // R pix18
	      data[59] = 0xff; // G
	      data[60] = 0xff; // B word4 rising byte0

	      data[61] = 0xff; // R pix19
	      data[62] = 0xff; // G
	      data[63] = 0xff; // B

	      data[64] = 0xff; // R pix20 falling byte0
	      data[65] = 0xff; // G
	      data[66] = 0xff; // B

	      data[67] = 0xff; // R pix21
	      data[68] = 0xff; // G word5 rising byte0
	      data[69] = 0xff; // B

	      data[70] = 0xff; // R pix22
	      data[71] = 0xff; // G 
	      data[72] = 0xff; // B falling byte0

	      data[73] = 0xff; // R pix23
	      data[74] = 0xff; // G
	      data[75] = 0xff; // B

	      for(i=0;i<5;i++) { // all black
		data[76+i*24] = 0x00; // R pix0 word0 rising byte0
		data[77+i*24] = 0x00; // G
		data[78+i*24] = 0x00; // B

		data[79+i*24] = 0x00; // R pix1
		data[80+i*24] = 0x00; // G falling byte0
		data[81+i*24] = 0x00; // B

		data[82+i*24] = 0x00; // R pix2
		data[83+i*24] = 0x00; // G
		data[84+i*24] = 0x00; // B word1 rising byte0

		data[85+i*24] = 0x00; // R pix3
		data[86+i*24] = 0x00; // G
		data[87+i*24] = 0x00; // B

		data[88+i*24] = 0x00; // R pix4 falling byte0
		data[89+i*24] = 0x00; // G
		data[90+i*24] = 0x00; // B

		data[91+i*24] = 0x00; // R pix5
		data[92+i*24] = 0x00; // G word2 rising byte0
		data[93+i*24] = 0x00; // B

		data[94+i*24] = 0x00; // R pix6
		data[95+i*24] = 0x00; // G
		data[96+i*24] = 0x00; // B falling byte0

		data[97+i*24] = 0x00; // R pix7
		data[98+i*24] = 0x00; // G
		data[99+i*24] = 0x00; // B
	      }
	    }
	    else if(x==0 && y==149) { // lower left
	      for(i=0;i<5;i++) { // all black
		data[ 4+i*24] = 0xff; // R pix0 word0 rising byte0
		data[ 5+i*24] = 0xff; // G
		data[ 6+i*24] = 0xff; // B

		data[ 7+i*24] = 0x00; // R pix1
		data[ 8+i*24] = 0x00; // G falling byte0
		data[ 9+i*24] = 0x00; // B

		data[10+i*24] = 0xff; // R pix2
		data[11+i*24] = 0xff; // G
		data[12+i*24] = 0xff; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0x00; // R pix5
		data[20+i*24] = 0x00; // G word2 rising byte0
		data[21+i*24] = 0x00; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0x00; // R pix7
		data[26+i*24] = 0x00; // G
		data[27+i*24] = 0x00; // B
	      }
	      data[124] =  0xff; // R pix0 word0 rising byte0
	      data[125] =  0xff; // G
	      data[126] =  0xff; // B

	      data[127] =  0x00; // R pix1
	      data[128] =  0x00; // G falling byte0
	      data[129] =  0x00; // B

	      data[130] = 0xff; // R pix2
	      data[131] = 0xff; // G
	      data[132] = 0xff; // B word1 rising byte0

	      data[133] = 0xff; // R pix3
	      data[134] = 0xff; // G
	      data[135] = 0xff; // B

	      data[136] = 0xff; // R pix4 falling byte0
	      data[137] = 0xff; // G
	      data[138] = 0xff; // B

	      data[139] = 0xff; // R pix5
	      data[140] = 0xff; // G word2 rising byte0
	      data[141] = 0xff; // B

	      data[142] = 0xff; // R pix6
	      data[143] = 0xff; // G
	      data[144] = 0xff; // B falling byte0

	      data[145] = 0xff; // R pix7
	      data[146] = 0xff; // G
	      data[147] = 0xff; // B

	      data[148] = 0xff; // R pix8 word3 rising byte0
	      data[149] = 0xff; // G
	      data[150] = 0xff; // B

	      data[151] = 0x00; // R pix9
	      data[152] = 0x00; // G falling byte0
	      data[153] = 0x00; // B

	      data[154] = 0x00; // R pix10
	      data[155] = 0x00; // G
	      data[156] = 0x00; // B word4 rising byte0

	      data[157] = 0x00; // R pix11
	      data[158] = 0x00; // G
	      data[159] = 0x00; // B

	      data[160] = 0x00; // R pix12 falling byte0
	      data[161] = 0x00; // G
	      data[162] = 0x00; // B

	      data[163] = 0x00; // R pix13
	      data[164] = 0x00; // G word5 rising byte0
	      data[165] = 0x00; // B

	      data[166] = 0x00; // R pix14
	      data[167] = 0x00; // G 
	      data[168] = 0x00; // B falling byte0

	      data[169] = 0x00; // R pix15
	      data[170] = 0x00; // G
	      data[171] = 0x00; // B

	      data[172] = 0xff; // R pix16 word3 rising byte0
	      data[173] = 0xff; // G
	      data[174] = 0xff; // B

	      data[175] = 0xff; // R pix17
	      data[176] = 0xff; // G falling byte0
	      data[177] = 0xff; // B

	      data[178] = 0xff; // R pix18
	      data[179] = 0xff; // G
	      data[180] = 0xff; // B word4 rising byte0

	      data[181] = 0xff; // R pix19
	      data[182] = 0xff; // G
	      data[183] = 0xff; // B

	      data[184] = 0xff; // R pix20 falling byte0
	      data[185] = 0xff; // G
	      data[186] = 0xff; // B

	      data[187] = 0xff; // R pix21
	      data[188] = 0xff; // G word5 rising byte0
	      data[189] = 0xff; // B

	      data[190] = 0xff; // R pix22
	      data[191] = 0xff; // G 
	      data[192] = 0xff; // B falling byte0

	      data[193] = 0xff; // R pix23
	      data[194] = 0xff; // G
	      data[195] = 0xff; // B

	    }
	    else if(x==199 && y==149) { // lower right
	      for(i=0;i<5;i++) { // all black
		data[ 4+i*24] = 0x00; // R pix0 word0 rising byte0
		data[ 5+i*24] = 0x00; // G
		data[ 6+i*24] = 0x00; // B

		data[ 7+i*24] = 0x00; // R pix1
		data[ 8+i*24] = 0x00; // G falling byte0
		data[ 9+i*24] = 0x00; // B

		data[10+i*24] = 0x00; // R pix2
		data[11+i*24] = 0x00; // G
		data[12+i*24] = 0x00; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0xff; // R pix5
		data[20+i*24] = 0xff; // G word2 rising byte0
		data[21+i*24] = 0xff; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0xff; // R pix7
		data[26+i*24] = 0xff; // G
		data[27+i*24] = 0xff; // B
	      }
	      data[124] =  0xff; // R pix0 word0 rising byte0
	      data[125] =  0xff; // G
	      data[126] =  0xff; // B

	      data[127] =  0xff; // R pix1
	      data[128] =  0xff; // G falling byte0
	      data[129] =  0xff; // B

	      data[130] = 0xff; // R pix2
	      data[131] = 0xff; // G
	      data[132] = 0xff; // B word1 rising byte0

	      data[133] = 0xff; // R pix3
	      data[134] = 0xff; // G
	      data[135] = 0xff; // B

	      data[136] = 0xff; // R pix4 falling byte0
	      data[137] = 0xff; // G
	      data[138] = 0xff; // B

	      data[139] = 0xff; // R pix5
	      data[140] = 0xff; // G word2 rising byte0
	      data[141] = 0xff; // B

	      data[142] = 0x00; // R pix6
	      data[143] = 0x00; // G
	      data[144] = 0x00; // B falling byte0

	      data[145] = 0xff; // R pix7
	      data[146] = 0xff; // G
	      data[147] = 0xff; // B

	      data[148] = 0x00; // R pix8 word3 rising byte0
	      data[149] = 0x00; // G
	      data[150] = 0x00; // B

	      data[151] = 0x00; // R pix9
	      data[152] = 0x00; // G falling byte0
	      data[153] = 0x00; // B

	      data[154] = 0x00; // R pix10
	      data[155] = 0x00; // G
	      data[156] = 0x00; // B word4 rising byte0

	      data[157] = 0x00; // R pix11
	      data[158] = 0x00; // G
	      data[159] = 0x00; // B

	      data[160] = 0x00; // R pix12 falling byte0
	      data[161] = 0x00; // G
	      data[162] = 0x00; // B

	      data[163] = 0x00; // R pix13
	      data[164] = 0x00; // G word5 rising byte0
	      data[165] = 0x00; // B

	      data[166] = 0x00; // R pix14
	      data[167] = 0x00; // G 
	      data[168] = 0x00; // B falling byte0

	      data[169] = 0xff; // R pix15
	      data[170] = 0xff; // G
	      data[171] = 0xff; // B

	      data[172] = 0xff; // R pix16 word3 rising byte0
	      data[173] = 0xff; // G
	      data[174] = 0xff; // B

	      data[175] = 0xff; // R pix17
	      data[176] = 0xff; // G falling byte0
	      data[177] = 0xff; // B

	      data[178] = 0xff; // R pix18
	      data[179] = 0xff; // G
	      data[180] = 0xff; // B word4 rising byte0

	      data[181] = 0xff; // R pix19
	      data[182] = 0xff; // G
	      data[183] = 0xff; // B

	      data[184] = 0xff; // R pix20 falling byte0
	      data[185] = 0xff; // G
	      data[186] = 0xff; // B

	      data[187] = 0xff; // R pix21
	      data[188] = 0xff; // G word5 rising byte0
	      data[189] = 0xff; // B

	      data[190] = 0xff; // R pix22
	      data[191] = 0xff; // G 
	      data[192] = 0xff; // B falling byte0

	      data[193] = 0xff; // R pix23
	      data[194] = 0xff; // G
	      data[195] = 0xff; // B
	    }
	    else if(y==149) { // bottom row
	      for(i=0;i<5;i++) { // all black
		data[ 4+i*24] = 0x00; // R pix0 word0 rising byte0
		data[ 5+i*24] = 0x00; // G
		data[ 6+i*24] = 0x00; // B

		data[ 7+i*24] = 0x00; // R pix1
		data[ 8+i*24] = 0x00; // G falling byte0
		data[ 9+i*24] = 0x00; // B

		data[10+i*24] = 0x00; // R pix2
		data[11+i*24] = 0x00; // G
		data[12+i*24] = 0x00; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0x00; // R pix5
		data[20+i*24] = 0x00; // G word2 rising byte0
		data[21+i*24] = 0x00; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0x00; // R pix7
		data[26+i*24] = 0x00; // G
		data[27+i*24] = 0x00; // B
	      }
	      data[124] =  0xff; // R pix0 word0 rising byte0
	      data[125] =  0xff; // G
	      data[126] =  0xff; // B

	      data[127] =  0xff; // R pix1
	      data[128] =  0xff; // G falling byte0
	      data[129] =  0xff; // B

	      data[130] = 0xff; // R pix2
	      data[131] = 0xff; // G
	      data[132] = 0xff; // B word1 rising byte0

	      data[133] = 0xff; // R pix3
	      data[134] = 0xff; // G
	      data[135] = 0xff; // B

	      data[136] = 0xff; // R pix4 falling byte0
	      data[137] = 0xff; // G
	      data[138] = 0xff; // B

	      data[139] = 0xff; // R pix5
	      data[140] = 0xff; // G word2 rising byte0
	      data[141] = 0xff; // B

	      data[142] = 0xff; // R pix6
	      data[143] = 0xff; // G
	      data[144] = 0xff; // B falling byte0

	      data[145] = 0xff; // R pix7
	      data[146] = 0xff; // G
	      data[147] = 0xff; // B

	      data[148] = 0x00; // R pix8 word3 rising byte0
	      data[149] = 0x00; // G
	      data[150] = 0x00; // B

	      data[151] = 0x00; // R pix9
	      data[152] = 0x00; // G falling byte0
	      data[153] = 0x00; // B

	      data[154] = 0x00; // R pix10
	      data[155] = 0x00; // G
	      data[156] = 0x00; // B word4 rising byte0

	      data[157] = 0x00; // R pix11
	      data[158] = 0x00; // G
	      data[159] = 0x00; // B

	      data[160] = 0x00; // R pix12 falling byte0
	      data[161] = 0x00; // G
	      data[162] = 0x00; // B

	      data[163] = 0x00; // R pix13
	      data[164] = 0x00; // G word5 rising byte0
	      data[165] = 0x00; // B

	      data[166] = 0x00; // R pix14
	      data[167] = 0x00; // G 
	      data[168] = 0x00; // B falling byte0

	      data[169] = 0x00; // R pix15
	      data[170] = 0x00; // G
	      data[171] = 0x00; // B

	      data[172] = 0xff; // R pix16 word3 rising byte0
	      data[173] = 0xff; // G
	      data[174] = 0xff; // B

	      data[175] = 0xff; // R pix17
	      data[176] = 0xff; // G falling byte0
	      data[177] = 0xff; // B

	      data[178] = 0xff; // R pix18
	      data[179] = 0xff; // G
	      data[180] = 0xff; // B word4 rising byte0

	      data[181] = 0xff; // R pix19
	      data[182] = 0xff; // G
	      data[183] = 0xff; // B

	      data[184] = 0xff; // R pix20 falling byte0
	      data[185] = 0xff; // G
	      data[186] = 0xff; // B

	      data[187] = 0xff; // R pix21
	      data[188] = 0xff; // G word5 rising byte0
	      data[189] = 0xff; // B

	      data[190] = 0xff; // R pix22
	      data[191] = 0xff; // G 
	      data[192] = 0xff; // B falling byte0

	      data[193] = 0xff; // R pix23
	      data[194] = 0xff; // G
	      data[195] = 0xff; // B
	    }
	    else if(x==0) { // left column
	      for(i=0;i<8;i++) {
		data[4+i*24] = 0xff; // R pix0
		data[5+i*24] = 0xff; // G
		data[6+i*24] = 0xff; // B

		data[7+i*24] = 0x00; // R pix1
		data[8+i*24] = 0x00; // G
		data[9+i*24] = 0x00; // B

		data[10+i*24] = 0xff; // R pix2
		data[11+i*24] = 0xff; // G
		data[12+i*24] = 0xff; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0x00; // R pix5
		data[20+i*24] = 0x00; // G word2 rising byte0
		data[21+i*24] = 0x00; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0x00; // R pix7
		data[26+i*24] = 0x00; // G
		data[27+i*24] = 0x00; // B
	      }
	    }
	    else if(x==199) { // right column
	      for(i=0;i<8;i++) {
		data[4+i*24] = 0x00; // R pix0
		data[5+i*24] = 0x00; // G
		data[6+i*24] = 0x00; // B

		data[7+i*24] = 0x00; // R pix1
		data[8+i*24] = 0x00; // G
		data[9+i*24] = 0x00; // B

		data[10+i*24] = 0x00; // R pix2
		data[11+i*24] = 0x00; // G
		data[12+i*24] = 0x00; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0xff; // R pix5
		data[20+i*24] = 0xff; // G word2 rising byte0
		data[21+i*24] = 0xff; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0xff; // R pix7
		data[26+i*24] = 0xff; // G
		data[27+i*24] = 0xff; // B
	      }
	    }
	    else { // other blocks
	      for(i=0;i<8;i++) { // all black
		data[4+i*24] = 0xff; // R
		data[5+i*24] = 0x00; // G
		data[6+i*24] = 0x00; // B

		data[7+i*24] = 0x00; // R
		data[8+i*24] = 0x00; // G
		data[9+i*24] = 0xff; // B

		data[10+i*24] = 0x00; // R
		data[11+i*24] = 0x00; // G
		data[12+i*24] = 0x00; // B

		data[13+i*24] = 0x00; // R
		data[14+i*24] = 0xff; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0xff; // R
		data[20+i*24] = 0x00; // G
		data[21+i*24] = 0x00; // B

		data[22+i*24] = 0xff; // R
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B

		data[25+i*24] = 0xff; // R
		data[26+i*24] = 0x00; // G
		data[27+i*24] = 0x00; // B
	      }
	    }

            // Ideally, we should be using a higher level API in VideoDisplayDriver to write to the Frame Buffer on the Client
            sStream->epWrite(videoDrv->Endpoint, data, 64*3 + 4);
            //cli->sendMessage(1, data, 64*3 + 4);
        //    printf ("Sent VDC msg to x=%0d,y=%0d\n",x,y);
            FLUSH;
        }
    }
    //}
    

    printf ("sleeping before VGA enable\n");
    sleep (2);
}

void vdcWriteBorder() {
    // VGA Write through normal VDC path
    uint32_t x,y;
    uint32_t i;
    uint8_t red,green,blue;
    uint32_t rightTileNum;
    uint32_t bottomTileNum;
    uint32_t hvispels;
    uint32_t vvispels;

    EP0WRITE(0x1001,0x1); // enable VDC
    //printf("READ CSR VDC enable reg: 0x%08x\n", EP0READ(0x1001));
    printf("Enabling VDC...\n");
    EP0FLUSH;

    // read back to see resolution
    hvispels = (EP0READ(0x3003) & 0xfff) + 1;
    vvispels = (EP0READ(0x3004) & 0xfff) + 1;

    rightTileNum = (hvispels / 8)- 1;
    bottomTileNum = (vvispels / 8) - 1;

    // don't matter if enable VGA or not
    EP0WRITE(0x3001,0x1); // enable Vga
    EP0FLUSH;


    for (x=0;x<=rightTileNum;x++) {
        for (y=0;y<=bottomTileNum;y++) {
            // assemble message for VDC to process
            char data[1024];
            data[0] = x & 0xff;
            data[1] = y & 0xff;
            data[2] = (((x & 0xf00) >> 4) | 
                    (y & 0xf00) >> 8) ;
            data[3] = 0x3f;         // [7] = Repeat;  [5:0] rpt length - 1
	    if(x==0 && y==0) { // upper left
	      data[4] =  0xff; // R pix0 word0 rising byte0
	      data[5] =  0xff; // G
	      data[6] =  0xff; // B

	      data[7] =  0xff; // R pix1
	      data[8] =  0xff; // G falling byte0
	      data[9] =  0xff; // B

	      data[10] = 0xff; // R pix2
	      data[11] = 0xff; // G
	      data[12] = 0xff; // B word1 rising byte0

	      data[13] = 0xff; // R pix3
	      data[14] = 0xff; // G
	      data[15] = 0xff; // B

	      data[16] = 0xff; // R pix4 falling byte0
	      data[17] = 0xff; // G
	      data[18] = 0xff; // B

	      data[19] = 0xff; // R pix5
	      data[20] = 0xff; // G word2 rising byte0
	      data[21] = 0xff; // B

	      data[22] = 0xff; // R pix6
	      data[23] = 0xff; // G
	      data[24] = 0xff; // B falling byte0

	      data[25] = 0xff; // R pix7
	      data[26] = 0xff; // G
	      data[27] = 0xff; // B

	      data[28] = 0xff; // R pix8 word3 rising byte0
	      data[29] = 0xff; // G
	      data[30] = 0xff; // B

	      data[31] = 0x00; // R pix9
	      data[32] = 0x00; // G falling byte0
	      data[33] = 0x00; // B

	      data[34] = 0x00; // R pix10
	      data[35] = 0x00; // G
	      data[36] = 0x00; // B word4 rising byte0

	      data[37] = 0x00; // R pix11
	      data[38] = 0x00; // G
	      data[39] = 0x00; // B

	      data[40] = 0x00; // R pix12 falling byte0
	      data[41] = 0x00; // G
	      data[42] = 0x00; // B

	      data[43] = 0x00; // R pix13
	      data[44] = 0x00; // G word5 rising byte0
	      data[45] = 0x00; // B

	      data[46] = 0x00; // R pix14
	      data[47] = 0x00; // G 
	      data[48] = 0x00; // B falling byte0

	      data[49] = 0x00; // R pix15
	      data[50] = 0x00; // G
	      data[51] = 0x00; // B

	      data[52] = 0xff; // R pix16 word3 rising byte0
	      data[53] = 0xff; // G
	      data[54] = 0xff; // B

	      data[55] = 0x00; // R pix17
	      data[56] = 0x00; // G falling byte0
	      data[57] = 0x00; // B

	      data[58] = 0xff; // R pix18
	      data[59] = 0xff; // G
	      data[60] = 0xff; // B word4 rising byte0

	      data[61] = 0xff; // R pix19
	      data[62] = 0xff; // G
	      data[63] = 0xff; // B

	      data[64] = 0xff; // R pix20 falling byte0
	      data[65] = 0xff; // G
	      data[66] = 0xff; // B

	      data[67] = 0xff; // R pix21
	      data[68] = 0xff; // G word5 rising byte0
	      data[69] = 0xff; // B

	      data[70] = 0xff; // R pix22
	      data[71] = 0xff; // G 
	      data[72] = 0xff; // B falling byte0

	      data[73] = 0xff; // R pix23
	      data[74] = 0xff; // G
	      data[75] = 0xff; // B

	      for(i=0;i<5;i++) { // all black
		data[76+i*24] = 0xff; // R pix0 word0 rising byte0
		data[77+i*24] = 0xff; // G
		data[78+i*24] = 0xff; // B

		data[79+i*24] = 0x00; // R pix1
		data[80+i*24] = 0x00; // G falling byte0
		data[81+i*24] = 0x00; // B

		data[82+i*24] = 0xff; // R pix2
		data[83+i*24] = 0xff; // G
		data[84+i*24] = 0xff; // B word1 rising byte0

		data[85+i*24] = 0x00; // R pix3
		data[86+i*24] = 0x00; // G
		data[87+i*24] = 0x00; // B

		data[88+i*24] = 0x00; // R pix4 falling byte0
		data[89+i*24] = 0x00; // G
		data[90+i*24] = 0x00; // B

		data[91+i*24] = 0x00; // R pix5
		data[92+i*24] = 0x00; // G word2 rising byte0
		data[93+i*24] = 0x00; // B

		data[94+i*24] = 0x00; // R pix6
		data[95+i*24] = 0x00; // G
		data[96+i*24] = 0x00; // B falling byte0

		data[97+i*24] = 0x00; // R pix7
		data[98+i*24] = 0x00; // G
		data[99+i*24] = 0x00; // B
	      }
	    }
	    else if(x==rightTileNum && y==0) { // upper right
	      data[4] =  0xff; // R pix0 word0 rising byte0
	      data[5] =  0xff; // G
	      data[6] =  0xff; // B

	      data[7] =  0xff; // R pix1
	      data[8] =  0xff; // G falling byte0
	      data[9] =  0xff; // B

	      data[10] = 0xff; // R pix2
	      data[11] = 0xff; // G
	      data[12] = 0xff; // B word1 rising byte0

	      data[13] = 0xff; // R pix3
	      data[14] = 0xff; // G
	      data[15] = 0xff; // B

	      data[16] = 0xff; // R pix4 falling byte0
	      data[17] = 0xff; // G
	      data[18] = 0xff; // B

	      data[19] = 0xff; // R pix5
	      data[20] = 0xff; // G word2 rising byte0
	      data[21] = 0xff; // B

	      data[22] = 0xff; // R pix6
	      data[23] = 0xff; // G
	      data[24] = 0xff; // B falling byte0

	      data[25] = 0xff; // R pix7
	      data[26] = 0xff; // G
	      data[27] = 0xff; // B

	      data[28] = 0x00; // R pix8 word3 rising byte0
	      data[29] = 0x00; // G
	      data[30] = 0x00; // B

	      data[31] = 0x00; // R pix9
	      data[32] = 0x00; // G falling byte0
	      data[33] = 0x00; // B

	      data[34] = 0x00; // R pix10
	      data[35] = 0x00; // G
	      data[36] = 0x00; // B word4 rising byte0

	      data[37] = 0x00; // R pix11
	      data[38] = 0x00; // G
	      data[39] = 0x00; // B

	      data[40] = 0x00; // R pix12 falling byte0
	      data[41] = 0x00; // G
	      data[42] = 0x00; // B

	      data[43] = 0x00; // R pix13
	      data[44] = 0x00; // G word5 rising byte0
	      data[45] = 0x00; // B

	      data[46] = 0x00; // R pix14
	      data[47] = 0x00; // G 
	      data[48] = 0x00; // B falling byte0

	      data[49] = 0xff; // R pix15
	      data[50] = 0xff; // G
	      data[51] = 0xff; // B

	      data[52] = 0xff; // R pix16 word3 rising byte0
	      data[53] = 0xff; // G
	      data[54] = 0xff; // B

	      data[55] = 0xff; // R pix17
	      data[56] = 0xff; // G falling byte0
	      data[57] = 0xff; // B

	      data[58] = 0xff; // R pix18
	      data[59] = 0xff; // G
	      data[60] = 0xff; // B word4 rising byte0

	      data[61] = 0xff; // R pix19
	      data[62] = 0xff; // G
	      data[63] = 0xff; // B

	      data[64] = 0xff; // R pix20 falling byte0
	      data[65] = 0xff; // G
	      data[66] = 0xff; // B

	      data[67] = 0xff; // R pix21
	      data[68] = 0xff; // G word5 rising byte0
	      data[69] = 0xff; // B

	      data[70] = 0x00; // R pix22
	      data[71] = 0x00; // G 
	      data[72] = 0x00; // B falling byte0

	      data[73] = 0xff; // R pix23
	      data[74] = 0xff; // G
	      data[75] = 0xff; // B

	      for(i=0;i<5;i++) { // all black
		data[76+i*24] = 0x00; // R pix0 word0 rising byte0
		data[77+i*24] = 0x00; // G
		data[78+i*24] = 0x00; // B

		data[79+i*24] = 0x00; // R pix1
		data[80+i*24] = 0x00; // G falling byte0
		data[81+i*24] = 0x00; // B

		data[82+i*24] = 0x00; // R pix2
		data[83+i*24] = 0x00; // G
		data[84+i*24] = 0x00; // B word1 rising byte0

		data[85+i*24] = 0x00; // R pix3
		data[86+i*24] = 0x00; // G
		data[87+i*24] = 0x00; // B

		data[88+i*24] = 0x00; // R pix4 falling byte0
		data[89+i*24] = 0x00; // G
		data[90+i*24] = 0x00; // B

		data[91+i*24] = 0xff; // R pix5
		data[92+i*24] = 0xff; // G word2 rising byte0
		data[93+i*24] = 0xff; // B

		data[94+i*24] = 0x00; // R pix6
		data[95+i*24] = 0x00; // G
		data[96+i*24] = 0x00; // B falling byte0

		data[97+i*24] = 0xff; // R pix7
		data[98+i*24] = 0xff; // G
		data[99+i*24] = 0xff; // B
	      }
	    }
	    else if(y==0) { // top row
	      data[4] =  0xff; // R pix0 word0 rising byte0
	      data[5] =  0xff; // G
	      data[6] =  0xff; // B

	      data[7] =  0xff; // R pix1
	      data[8] =  0xff; // G falling byte0
	      data[9] =  0xff; // B

	      data[10] = 0xff; // R pix2
	      data[11] = 0xff; // G
	      data[12] = 0xff; // B word1 rising byte0

	      data[13] = 0xff; // R pix3
	      data[14] = 0xff; // G
	      data[15] = 0xff; // B

	      data[16] = 0xff; // R pix4 falling byte0
	      data[17] = 0xff; // G
	      data[18] = 0xff; // B

	      data[19] = 0xff; // R pix5
	      data[20] = 0xff; // G word2 rising byte0
	      data[21] = 0xff; // B

	      data[22] = 0xff; // R pix6
	      data[23] = 0xff; // G
	      data[24] = 0xff; // B falling byte0

	      data[25] = 0xff; // R pix7
	      data[26] = 0xff; // G
	      data[27] = 0xff; // B

	      data[28] = 0x00; // R pix8 word3 rising byte0
	      data[29] = 0x00; // G
	      data[30] = 0x00; // B

	      data[31] = 0x00; // R pix9
	      data[32] = 0x00; // G falling byte0
	      data[33] = 0x00; // B

	      data[34] = 0x00; // R pix10
	      data[35] = 0x00; // G
	      data[36] = 0x00; // B word4 rising byte0

	      data[37] = 0x00; // R pix11
	      data[38] = 0x00; // G
	      data[39] = 0x00; // B

	      data[40] = 0x00; // R pix12 falling byte0
	      data[41] = 0x00; // G
	      data[42] = 0x00; // B

	      data[43] = 0x00; // R pix13
	      data[44] = 0x00; // G word5 rising byte0
	      data[45] = 0x00; // B

	      data[46] = 0x00; // R pix14
	      data[47] = 0x00; // G 
	      data[48] = 0x00; // B falling byte0

	      data[49] = 0x00; // R pix15
	      data[50] = 0x00; // G
	      data[51] = 0x00; // B

	      data[52] = 0xff; // R pix16 word3 rising byte0
	      data[53] = 0xff; // G
	      data[54] = 0xff; // B

	      data[55] = 0xff; // R pix17
	      data[56] = 0xff; // G falling byte0
	      data[57] = 0xff; // B

	      data[58] = 0xff; // R pix18
	      data[59] = 0xff; // G
	      data[60] = 0xff; // B word4 rising byte0

	      data[61] = 0xff; // R pix19
	      data[62] = 0xff; // G
	      data[63] = 0xff; // B

	      data[64] = 0xff; // R pix20 falling byte0
	      data[65] = 0xff; // G
	      data[66] = 0xff; // B

	      data[67] = 0xff; // R pix21
	      data[68] = 0xff; // G word5 rising byte0
	      data[69] = 0xff; // B

	      data[70] = 0xff; // R pix22
	      data[71] = 0xff; // G 
	      data[72] = 0xff; // B falling byte0

	      data[73] = 0xff; // R pix23
	      data[74] = 0xff; // G
	      data[75] = 0xff; // B

	      for(i=0;i<5;i++) { // all black
		data[76+i*24] = 0x00; // R pix0 word0 rising byte0
		data[77+i*24] = 0x00; // G
		data[78+i*24] = 0x00; // B

		data[79+i*24] = 0x00; // R pix1
		data[80+i*24] = 0x00; // G falling byte0
		data[81+i*24] = 0x00; // B

		data[82+i*24] = 0x00; // R pix2
		data[83+i*24] = 0x00; // G
		data[84+i*24] = 0x00; // B word1 rising byte0

		data[85+i*24] = 0x00; // R pix3
		data[86+i*24] = 0x00; // G
		data[87+i*24] = 0x00; // B

		data[88+i*24] = 0x00; // R pix4 falling byte0
		data[89+i*24] = 0x00; // G
		data[90+i*24] = 0x00; // B

		data[91+i*24] = 0x00; // R pix5
		data[92+i*24] = 0x00; // G word2 rising byte0
		data[93+i*24] = 0x00; // B

		data[94+i*24] = 0x00; // R pix6
		data[95+i*24] = 0x00; // G
		data[96+i*24] = 0x00; // B falling byte0

		data[97+i*24] = 0x00; // R pix7
		data[98+i*24] = 0x00; // G
		data[99+i*24] = 0x00; // B
	      }
	    }
	    else if(x==0 && y==bottomTileNum) { // lower left
	      for(i=0;i<5;i++) { // all black
		data[ 4+i*24] = 0xff; // R pix0 word0 rising byte0
		data[ 5+i*24] = 0xff; // G
		data[ 6+i*24] = 0xff; // B

		data[ 7+i*24] = 0x00; // R pix1
		data[ 8+i*24] = 0x00; // G falling byte0
		data[ 9+i*24] = 0x00; // B

		data[10+i*24] = 0xff; // R pix2
		data[11+i*24] = 0xff; // G
		data[12+i*24] = 0xff; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0x00; // R pix5
		data[20+i*24] = 0x00; // G word2 rising byte0
		data[21+i*24] = 0x00; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0x00; // R pix7
		data[26+i*24] = 0x00; // G
		data[27+i*24] = 0x00; // B
	      }
	      data[124] =  0xff; // R pix0 word0 rising byte0
	      data[125] =  0xff; // G
	      data[126] =  0xff; // B

	      data[127] =  0x00; // R pix1
	      data[128] =  0x00; // G falling byte0
	      data[129] =  0x00; // B

	      data[130] = 0xff; // R pix2
	      data[131] = 0xff; // G
	      data[132] = 0xff; // B word1 rising byte0

	      data[133] = 0xff; // R pix3
	      data[134] = 0xff; // G
	      data[135] = 0xff; // B

	      data[136] = 0xff; // R pix4 falling byte0
	      data[137] = 0xff; // G
	      data[138] = 0xff; // B

	      data[139] = 0xff; // R pix5
	      data[140] = 0xff; // G word2 rising byte0
	      data[141] = 0xff; // B

	      data[142] = 0xff; // R pix6
	      data[143] = 0xff; // G
	      data[144] = 0xff; // B falling byte0

	      data[145] = 0xff; // R pix7
	      data[146] = 0xff; // G
	      data[147] = 0xff; // B

	      data[148] = 0xff; // R pix8 word3 rising byte0
	      data[149] = 0xff; // G
	      data[150] = 0xff; // B

	      data[151] = 0x00; // R pix9
	      data[152] = 0x00; // G falling byte0
	      data[153] = 0x00; // B

	      data[154] = 0x00; // R pix10
	      data[155] = 0x00; // G
	      data[156] = 0x00; // B word4 rising byte0

	      data[157] = 0x00; // R pix11
	      data[158] = 0x00; // G
	      data[159] = 0x00; // B

	      data[160] = 0x00; // R pix12 falling byte0
	      data[161] = 0x00; // G
	      data[162] = 0x00; // B

	      data[163] = 0x00; // R pix13
	      data[164] = 0x00; // G word5 rising byte0
	      data[165] = 0x00; // B

	      data[166] = 0x00; // R pix14
	      data[167] = 0x00; // G 
	      data[168] = 0x00; // B falling byte0

	      data[169] = 0x00; // R pix15
	      data[170] = 0x00; // G
	      data[171] = 0x00; // B

	      data[172] = 0xff; // R pix16 word3 rising byte0
	      data[173] = 0xff; // G
	      data[174] = 0xff; // B

	      data[175] = 0xff; // R pix17
	      data[176] = 0xff; // G falling byte0
	      data[177] = 0xff; // B

	      data[178] = 0xff; // R pix18
	      data[179] = 0xff; // G
	      data[180] = 0xff; // B word4 rising byte0

	      data[181] = 0xff; // R pix19
	      data[182] = 0xff; // G
	      data[183] = 0xff; // B

	      data[184] = 0xff; // R pix20 falling byte0
	      data[185] = 0xff; // G
	      data[186] = 0xff; // B

	      data[187] = 0xff; // R pix21
	      data[188] = 0xff; // G word5 rising byte0
	      data[189] = 0xff; // B

	      data[190] = 0xff; // R pix22
	      data[191] = 0xff; // G 
	      data[192] = 0xff; // B falling byte0

	      data[193] = 0xff; // R pix23
	      data[194] = 0xff; // G
	      data[195] = 0xff; // B

	    }
	    else if(x==rightTileNum && y==bottomTileNum) { // lower right
	      for(i=0;i<5;i++) { // all black
		data[ 4+i*24] = 0x00; // R pix0 word0 rising byte0
		data[ 5+i*24] = 0x00; // G
		data[ 6+i*24] = 0x00; // B

		data[ 7+i*24] = 0x00; // R pix1
		data[ 8+i*24] = 0x00; // G falling byte0
		data[ 9+i*24] = 0x00; // B

		data[10+i*24] = 0x00; // R pix2
		data[11+i*24] = 0x00; // G
		data[12+i*24] = 0x00; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0xff; // R pix5
		data[20+i*24] = 0xff; // G word2 rising byte0
		data[21+i*24] = 0xff; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0xff; // R pix7
		data[26+i*24] = 0xff; // G
		data[27+i*24] = 0xff; // B
	      }
	      data[124] =  0xff; // R pix0 word0 rising byte0
	      data[125] =  0xff; // G
	      data[126] =  0xff; // B

	      data[127] =  0xff; // R pix1
	      data[128] =  0xff; // G falling byte0
	      data[129] =  0xff; // B

	      data[130] = 0xff; // R pix2
	      data[131] = 0xff; // G
	      data[132] = 0xff; // B word1 rising byte0

	      data[133] = 0xff; // R pix3
	      data[134] = 0xff; // G
	      data[135] = 0xff; // B

	      data[136] = 0xff; // R pix4 falling byte0
	      data[137] = 0xff; // G
	      data[138] = 0xff; // B

	      data[139] = 0xff; // R pix5
	      data[140] = 0xff; // G word2 rising byte0
	      data[141] = 0xff; // B

	      data[142] = 0x00; // R pix6
	      data[143] = 0x00; // G
	      data[144] = 0x00; // B falling byte0

	      data[145] = 0xff; // R pix7
	      data[146] = 0xff; // G
	      data[147] = 0xff; // B

	      data[148] = 0x00; // R pix8 word3 rising byte0
	      data[149] = 0x00; // G
	      data[150] = 0x00; // B

	      data[151] = 0x00; // R pix9
	      data[152] = 0x00; // G falling byte0
	      data[153] = 0x00; // B

	      data[154] = 0x00; // R pix10
	      data[155] = 0x00; // G
	      data[156] = 0x00; // B word4 rising byte0

	      data[157] = 0x00; // R pix11
	      data[158] = 0x00; // G
	      data[159] = 0x00; // B

	      data[160] = 0x00; // R pix12 falling byte0
	      data[161] = 0x00; // G
	      data[162] = 0x00; // B

	      data[163] = 0x00; // R pix13
	      data[164] = 0x00; // G word5 rising byte0
	      data[165] = 0x00; // B

	      data[166] = 0x00; // R pix14
	      data[167] = 0x00; // G 
	      data[168] = 0x00; // B falling byte0

	      data[169] = 0xff; // R pix15
	      data[170] = 0xff; // G
	      data[171] = 0xff; // B

	      data[172] = 0xff; // R pix16 word3 rising byte0
	      data[173] = 0xff; // G
	      data[174] = 0xff; // B

	      data[175] = 0xff; // R pix17
	      data[176] = 0xff; // G falling byte0
	      data[177] = 0xff; // B

	      data[178] = 0xff; // R pix18
	      data[179] = 0xff; // G
	      data[180] = 0xff; // B word4 rising byte0

	      data[181] = 0xff; // R pix19
	      data[182] = 0xff; // G
	      data[183] = 0xff; // B

	      data[184] = 0xff; // R pix20 falling byte0
	      data[185] = 0xff; // G
	      data[186] = 0xff; // B

	      data[187] = 0xff; // R pix21
	      data[188] = 0xff; // G word5 rising byte0
	      data[189] = 0xff; // B

	      data[190] = 0xff; // R pix22
	      data[191] = 0xff; // G 
	      data[192] = 0xff; // B falling byte0

	      data[193] = 0xff; // R pix23
	      data[194] = 0xff; // G
	      data[195] = 0xff; // B
	    }
	    else if(y==bottomTileNum) { // bottom row
	      for(i=0;i<5;i++) { // all black
		data[ 4+i*24] = 0x00; // R pix0 word0 rising byte0
		data[ 5+i*24] = 0x00; // G
		data[ 6+i*24] = 0x00; // B

		data[ 7+i*24] = 0x00; // R pix1
		data[ 8+i*24] = 0x00; // G falling byte0
		data[ 9+i*24] = 0x00; // B

		data[10+i*24] = 0x00; // R pix2
		data[11+i*24] = 0x00; // G
		data[12+i*24] = 0x00; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0x00; // R pix5
		data[20+i*24] = 0x00; // G word2 rising byte0
		data[21+i*24] = 0x00; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0x00; // R pix7
		data[26+i*24] = 0x00; // G
		data[27+i*24] = 0x00; // B
	      }
	      data[124] =  0xff; // R pix0 word0 rising byte0
	      data[125] =  0xff; // G
	      data[126] =  0xff; // B

	      data[127] =  0xff; // R pix1
	      data[128] =  0xff; // G falling byte0
	      data[129] =  0xff; // B

	      data[130] = 0xff; // R pix2
	      data[131] = 0xff; // G
	      data[132] = 0xff; // B word1 rising byte0

	      data[133] = 0xff; // R pix3
	      data[134] = 0xff; // G
	      data[135] = 0xff; // B

	      data[136] = 0xff; // R pix4 falling byte0
	      data[137] = 0xff; // G
	      data[138] = 0xff; // B

	      data[139] = 0xff; // R pix5
	      data[140] = 0xff; // G word2 rising byte0
	      data[141] = 0xff; // B

	      data[142] = 0xff; // R pix6
	      data[143] = 0xff; // G
	      data[144] = 0xff; // B falling byte0

	      data[145] = 0xff; // R pix7
	      data[146] = 0xff; // G
	      data[147] = 0xff; // B

	      data[148] = 0x00; // R pix8 word3 rising byte0
	      data[149] = 0x00; // G
	      data[150] = 0x00; // B

	      data[151] = 0x00; // R pix9
	      data[152] = 0x00; // G falling byte0
	      data[153] = 0x00; // B

	      data[154] = 0x00; // R pix10
	      data[155] = 0x00; // G
	      data[156] = 0x00; // B word4 rising byte0

	      data[157] = 0x00; // R pix11
	      data[158] = 0x00; // G
	      data[159] = 0x00; // B

	      data[160] = 0x00; // R pix12 falling byte0
	      data[161] = 0x00; // G
	      data[162] = 0x00; // B

	      data[163] = 0x00; // R pix13
	      data[164] = 0x00; // G word5 rising byte0
	      data[165] = 0x00; // B

	      data[166] = 0x00; // R pix14
	      data[167] = 0x00; // G 
	      data[168] = 0x00; // B falling byte0

	      data[169] = 0x00; // R pix15
	      data[170] = 0x00; // G
	      data[171] = 0x00; // B

	      data[172] = 0xff; // R pix16 word3 rising byte0
	      data[173] = 0xff; // G
	      data[174] = 0xff; // B

	      data[175] = 0xff; // R pix17
	      data[176] = 0xff; // G falling byte0
	      data[177] = 0xff; // B

	      data[178] = 0xff; // R pix18
	      data[179] = 0xff; // G
	      data[180] = 0xff; // B word4 rising byte0

	      data[181] = 0xff; // R pix19
	      data[182] = 0xff; // G
	      data[183] = 0xff; // B

	      data[184] = 0xff; // R pix20 falling byte0
	      data[185] = 0xff; // G
	      data[186] = 0xff; // B

	      data[187] = 0xff; // R pix21
	      data[188] = 0xff; // G word5 rising byte0
	      data[189] = 0xff; // B

	      data[190] = 0xff; // R pix22
	      data[191] = 0xff; // G 
	      data[192] = 0xff; // B falling byte0

	      data[193] = 0xff; // R pix23
	      data[194] = 0xff; // G
	      data[195] = 0xff; // B
	    }
	    else if(x==0) { // left column
	      for(i=0;i<8;i++) {
		data[4+i*24] = 0xff; // R pix0
		data[5+i*24] = 0xff; // G
		data[6+i*24] = 0xff; // B

		data[7+i*24] = 0x00; // R pix1
		data[8+i*24] = 0x00; // G
		data[9+i*24] = 0x00; // B

		data[10+i*24] = 0xff; // R pix2
		data[11+i*24] = 0xff; // G
		data[12+i*24] = 0xff; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0x00; // R pix5
		data[20+i*24] = 0x00; // G word2 rising byte0
		data[21+i*24] = 0x00; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0x00; // R pix7
		data[26+i*24] = 0x00; // G
		data[27+i*24] = 0x00; // B
	      }
	    }
	    else if(x==rightTileNum) { // right column
	      for(i=0;i<8;i++) {
		data[4+i*24] = 0x00; // R pix0
		data[5+i*24] = 0x00; // G
		data[6+i*24] = 0x00; // B

		data[7+i*24] = 0x00; // R pix1
		data[8+i*24] = 0x00; // G
		data[9+i*24] = 0x00; // B

		data[10+i*24] = 0x00; // R pix2
		data[11+i*24] = 0x00; // G
		data[12+i*24] = 0x00; // B word1 rising byte0

		data[13+i*24] = 0x00; // R pix3
		data[14+i*24] = 0x00; // G
		data[15+i*24] = 0x00; // B

		data[16+i*24] = 0x00; // R pix4 falling byte0
		data[17+i*24] = 0x00; // G
		data[18+i*24] = 0x00; // B

		data[19+i*24] = 0xff; // R pix5
		data[20+i*24] = 0xff; // G word2 rising byte0
		data[21+i*24] = 0xff; // B

		data[22+i*24] = 0x00; // R pix6
		data[23+i*24] = 0x00; // G
		data[24+i*24] = 0x00; // B falling byte0

		data[25+i*24] = 0xff; // R pix7
		data[26+i*24] = 0xff; // G
		data[27+i*24] = 0xff; // B
	      }
	    }
	    else { // other blocks
	      for(i=0;i<64;i++) { // all black
		data[4+i*3] = 0x00;
		data[5+i*3] = 0x00;
		data[6+i*3] = 0x00;
	      }
	    }

            // Ideally, we should be using a higher level API in VideoDisplayDriver to write to the Frame Buffer on the Client
            sStream->epWrite(videoDrv->Endpoint, data, 64*3 + 4);
            //cli->sendMessage(1, data, 64*3 + 4);
          //  printf ("Sent VDC msg to x=%0d,y=%0d\n",x,y);
            FLUSH;
        }
    }
    //}
    

    printf ("sleeping before VGA enable\n");
    sleep (2);
}

RtnStatus_t ddrImgStressTest(void*) {

  // setup worst case operating condition
  ScreenRes_t res_640_60 = {640,480,60, 25};
  ScreenRes_t res_800_60 = {800,600,60, 40};
  ScreenRes_t res_1024_60 = {1024,768,60, 65};
  ScreenRes_t res_1280_60 = {1280,1024,60, 108};
  ScreenRes_t res_1600_60 = {1600,1200,60, 162};
  ScreenRes_t res_1920_60 = {1920,1200,60, 154};
  ScreenRes_t res_1680_60 = {1680,1050,60, 154};
  ScreenRes_t res_1280_75 = {1280,1024,75, 135};

  //vgaInit(&res_640_60);
  //vgaInit(&res_800_60);
  //vgaInit(&res_1024_60);
  //vgaInit(&res_1280_60);
  vgaInit(&res_1600_60);
  //vgaInit(&res_1280_75);
  //vgaInit(&res_1680_60);
  //vgaInit(&res_1920_60);
  EP0WRITE(0x3001,0x1); // VGA Enable pattern
  EP0WRITE(0x1001,0x21); // enable VDC, use color_swap mode
  
  Image *img = new Image;
  Image *img2 = new Image;
  assert (img != NULL);
  assert (img2 != NULL);
  //img->readBmp("../util/bmps/eye.bmp");
  //img2->readBmp("../util/bmps/magnolia.bmp");
  //img->readBmp("../util/bmps/fullscreen_1024_768.bmp");
  //img2->readBmp("../util/bmps/desktop_800_600.bmp");
  img->readBmp("../util/bmps/sunsetstar_1920x1200.bmp");
  img2->readBmp("../util/bmps/newjersey_1920x1200.bmp");
  //img->readBmp("../util/bmps/bars_1920x1200.bmp");
  //img2->readBmp("../util/bmps/bars_1920x1200.bmp");


  printf ("Done w/ Read BMP\n");
  
  uint32_t totlen = 0;
  
  
  printf ("Starting VDC Image\n");
  
  // Now go thru raw tiles, convert them to RLE, and send them to the HW
  uint32_t iter=0;
  while (iter<32) {
    iter++;
    printf ("display iter %0d\n", iter);
    for (uint16_t tileX = 0; tileX < img->getNumXTiles(); tileX++) {
      for (uint16_t tileY = 0; tileY < img->getNumYTiles(); tileY++) {
	RleTile rleTile;
	rleTile.rleEncode(img->getRawTileData(tileX, tileY), tileX, tileY);
	sStream->epWrite(videoDrv->Endpoint,(char*)rleTile.getRleTileData(),rleTile.getRleTileDataLength());
	totlen += rleTile.getRleTileDataLength();
      }
    }
    FLUSH;
    // send second image to second fram buffer (at vertical tile idx + 128)
    for (uint16_t tileY = 0; tileY < img2->getNumYTiles(); tileY++) {
      for (uint16_t tileX = 0; tileX < img2->getNumXTiles(); tileX++)  {
	RleTile rleTile;
	rleTile.rleEncode(img2->getRawTileData(tileX,tileY), tileX, tileY);
	sStream->epWrite(videoDrv->Endpoint,(char*)rleTile.getRleTileData(),rleTile.getRleTileDataLength());
	totlen += rleTile.getRleTileDataLength();
      }
      FLUSH;
    }
  }
  FLUSH;

  delete img, img2;

  return TEST_OK;

}

void audioSend(uint32_t num = 4) {

    uint32_t x,i;
    char data[1024];
    char audioData[2048];
    uint32_t num_bytes;
    uint32_t rem_bytes;
    uint32_t pkt_size;
    uint32_t sent_bytes;
    uint64_t time1;
    uint64_t time2;
    uint32_t timeElapsed;
    float num_bytes_float;
    
    printf ("Audio Sending Packet ...\n");
    /*
    // prepare data
    for (i=0;i<1024;i++) {
      if(i%256 < 128) {
	data[i] = i%256;
      }
      if(i%256 >= 128) {
	data[i] = 255-(i%256);
      }
    }
    */

    // Increasing the amplitude 
    
    // prepare data
    for (i=0;i<1024;i++) {
      if((2*i)%512 < 256) {
	data[i] = (2*i)%512;
      }
      if((2*i)%512 >= 256) {
	data[i] = 511-((2*i)%512);
      }
    }    

 
    // 12MHz / 544 = 22.05KHz * 2B/s => 44.1 KB/s
    float rate = ((12 * 1000000) / 1496) * 1;
    printf("rate = %f\n",rate);
    
    // send 1st & 2nd audio packet
    sStream->epWrite(AudioTnpEndpoint, data, 1024);
    sStream->epWrite(AudioTnpEndpoint, data, 1024);
    time1 = gettimeus();
    
    FLUSH;
    sent_bytes = 0;
    
    for (x=0;x<num;x++) {
      usleep(200000);
      time2 = gettimeus();
      timeElapsed = time2-time1;
      num_bytes_float = rate * ((float)timeElapsed / 1000000);
      num_bytes = (int)num_bytes_float;
      printf("timeElapsed = %d, num_bytes = %d, sent_bytes = %d\n",timeElapsed, num_bytes, sent_bytes);

      // create the audio packets
      // make sure largest packet is 1024B
      // if num_bytes > 1024, split up into multiple packets
      rem_bytes = num_bytes;
      pkt_size = (num_bytes > 1024) ? 1024 : num_bytes;
      while(rem_bytes != 0) {
	for(i=0;i<pkt_size;i++) {
	  audioData[i] = data[(i+sent_bytes)%1024];
	}
	printf("Sending a audio packet of size %d\n",pkt_size);
	sStream->epWrite(AudioTnpEndpoint, audioData, pkt_size);
	rem_bytes = rem_bytes - pkt_size;
	pkt_size = (rem_bytes > 1024) ? 1024 : rem_bytes;
      }

      FLUSH;
      sent_bytes = (num_bytes + sent_bytes)%1024;
      time1 = time2;
    }
}

RtnStatus_t doAudioLpbk(char* buff, uint32_t len) {
  
  char *data = new char[len-1];

  for(uint32_t i=0;i<len-1;i++) {
    data[i] = buff[i];
    //printf("Loopback mic data byte %0d = %0d\n", i, buff[i] & 0xff);
  }
  printf("Audio Packet Loopback in TNP, audio_pkt_num = %0d\n", audio_pkt_num);
  sStream->epWrite(AudioTnpEndpoint, data, len-1);
  FLUSH;

  delete [] data;

  return TEST_OK;
}

RtnStatus_t analyzeAudioRcv(char* buff, uint32_t len) {

  // throw away packets
  // - early-on packets to let ALC settle to steady state  
  // - received audio packets after audio_out has been turned off
  if (audio_pkt_num < 4 || audio_pkt_num > 200) {
    //printf("Discarding audio_in packets, num = %0d\n", audio_pkt_num);
    return TEST_OK;
  }
  
  // audio_in is being turned off
  if (len < 10) 
    return TEST_OK;
  

  char *data = new char[len-1];

  int32_t maxData = -100;
  int32_t minData = 100;
  uint32_t diff;
  uint32_t maxDiff = 0;

  for(uint32_t i=0;i<len-1;i++) {
    if(maxData < buff[i]) {
      maxData = buff[i];
    }
    if(minData > buff[i]) {
      minData = buff[i];
    }

    if(i==0) { // 1st element, nothing to diff against
      diff = 0;
    } else {
      diff = abs(buff[i] - buff[i-1]);
    }

    if(maxDiff < diff) {
      maxDiff = diff;
    }
    
    //printf("Audio receive data  = %0d, diff = %0d\n", buff[i], diff);
    
  }
  
  
  if(maxDiff < 1 || (maxData < minData) ) { // failed if maxData < minData
    printf("Audio data : max = %0d min = %0d, maxDiff = %0d, length = %0d\n", maxData, minData, maxDiff, len);
    return TEST_FAIL;
  }

  delete [] data;

  return TEST_OK;
  
}

void audioSendMax() {

    uint32_t x,i;

    EP0WRITE(0x4000, 0xc0000000); // assert RxEn/TxEn and mono mode
    //printf("Enabling Rx/Tx of audio...\n");
    EP0FLUSH;

    //printf ("Audio Sending Packet ...\n");
    for (x=0;x<4;x++) {
      // assemble message for ADC to process
      char data[1024];
      
      for (i=0;i<128;i++) {
	data[i] = 128+i;
      }
      for (i=128;i<256;i++) {
	data[i] = i;
      }
      
      // Ideally, we should be using a higher level API in VideoDisplayDriver to write to the Frame Buffer on the Client
      sStream->epWrite(3, data, 256);
      //cli->sendMessage(1, data, 64*3 + 4);
      //printf ("Sending Audio msg... \n");
      FLUSH;
    }


    //printf ("done sending audio packet\n");
    //sleep (2);
    //usleep(40000);

}

void EndpointInput::onEvent(const TnpSocket::SocketEvent& e) 
{
  char* buff = e.receivedBuffer();
  uint32_t len=e.bufferLength();

  //printf("Audio packet with length(in bytes) = %0d\n", len);

  if (localAudioProc != NULL) { 
    audio_pkt_num++;
    num_iter_--;
    if((*localAudioProc)(buff,len) == TEST_FAIL) {
	*status_ = TEST_FAIL;
	*done_ = true;
	} 
    if (num_iter_ == 0) {
	*done_ = true;
	}
    
  }

}

RtnStatus_t ddrDbgWritePattern(void*) {
    // Write a pattern to the screen using debug insertion path in the VDC
    uint32_t addr, bank;
    uint32_t wrData[8];
    printf("Disabling VDC\n");
    EP0WRITE(0x1001,0x0); // disable VDC
    EP0FLUSH;

    printf ("Writing pattern into DRAM through VDC debug path\n");
    for(addr=0; addr<1024; addr++) { // write 4 lines on screen
        for(bank=0; bank<4; bank++) {
            // wrData[0] = (addr << 4) | (bank << 28) | 0x0;
            // wrData[1] = (addr << 4) | (bank << 28) | 0x1;
            // wrData[2] = (addr << 4) | (bank << 28) | 0x2;
            // wrData[3] = (addr << 4) | (bank << 28) | 0x3;
            // wrData[4] = (addr << 4) | (bank << 28) | 0x4;
            // wrData[5] = (addr << 4) | (bank << 28) | 0x5;
            // wrData[6] = (addr << 4) | (bank << 28) | 0x6;
            // wrData[7] = (addr << 4) | (bank << 28) | 0x7;
            wrData[0] = 0xff0000ff;
            wrData[1] = 0x00ff0000;
            wrData[2] = 0x0000ff00;
            wrData[3] = 0xff0000ff;
            wrData[4] = 0x00ff0000;
            wrData[5] = 0x0000ff00;
            wrData[6] = 0x0;
            wrData[7] = 0x0;
            ddrWrite(addr, bank, wrData);
            //cli->service(0); // XXX - Is this the right thing to do here?
            FLUSH;
            //printf("DDR Write Data[0] = %x for addr=%x bank=%x\n", wrData[0],addr,bank);
            //printf("DDR Write Data[1] = %x for addr=%x bank=%x\n", wrData[1],addr,bank);
            //printf("DDR Write Data[2] = %x for addr=%x bank=%x\n", wrData[2],addr,bank);
            //printf("DDR Write Data[3] = %x for addr=%x bank=%x\n", wrData[3],addr,bank);
        }
        FLUSH;
    }
    printf ("Done writing pattern into DRAM through VDC debug path\n");
    uint32_t data;
    ddrRead(addr, bank, &data);

    printf ("Enabling VGA\n");
    ScreenRes_t res = {1024,768,60, 50};
    vgaInit(&res);

    EP0WRITE(0x3001,0x1); // VGA Enable pattern
    return TEST_OK;
}

RtnStatus_t ddrDbg3Test(void *) {
    int pass = 1;

    ScreenRes_t res_800_60 = {800,600,60, 40};

    printf ("Enabling VGA\n");
    vgaInit(&res_800_60);
    ddrDbgClearScreen_2();

    for (uint32_t addr = 0x0, iter = 0; iter < 1024; addr += 0x10, iter++) {
        EP0WRITE(0x2013,addr);
        EP0WRITE(0x2013,0xffff);
        EP0WRITE(0x2013,0x00ff0000);
        EP0WRITE(0x2013,0x00ff0000);
        EP0WRITE(0x2013,0x00ff0000);
        EP0WRITE(0x2013,0x00ff0000);
    }
   //vdc15Write2ColorTile();


    EP0WRITE(0x3001,0x1); // VGA Enable pattern
    EP0FLUSH;

    return pass == 1 ? TEST_OK : TEST_FAIL;
}

RtnStatus_t ddrDbg2Test(void *) {
    int pass = 1;
    std::vector<uint32_t> vc(4);
    std::vector<uint32_t> vd(4);
    std::vector<uint32_t> last(4);

    uint32_t addr = 0x10;

    uint32_t num_tr = 100000;

    for (uint32_t addr = 0x10, iter = 0; iter < num_tr; addr += 0x10, iter++) {
        for (int i=0; i<4; i++) {
            vc[i] = rand();
        }

        EP0WRITE(0x2013,addr);
        EP0WRITE(0x2013,0xffff);
        EP0WRITE(0x2013,vc[0]);
        EP0WRITE(0x2013,vc[1]);
        EP0WRITE(0x2013,vc[2]);
        EP0WRITE(0x2013,vc[3]);

        EP0WRITE(0x2012, addr);

        for (int i=0; i<4; i++) {
            vd[i] = EP0READ(0x2012);
        }
        for (int i=0; i<4; i++) {
            if (vc[i] != vd[i]) {
                pass = 0;
                printf("[%d] ddrDbg2Test failed: %x (addr) %x (exp) != %x (act) %x(last)\n", iter, addr, vc[i], vd[i], last[i]);
            }
        }
        if (pass == 0) return TEST_FAIL;
        copy(vd.begin(),vd.end(),last.begin());
    }
    printf("ddrDbg2Test: %d transactions passed\n", num_tr);

    return pass == 1 ? TEST_OK : TEST_FAIL;
}

RtnStatus_t ddrDbgTest(void*) {
    // test the DDR interface through writes/reads of the debug port

    //////////////////////////////////////////////////////////////
    // DDR Rd/Wr test
    // colNum = {2'b0,addr[5:0],3'b0}
    // rowNum = addr[19:6]
    // => addr goes to 2^20-1
    uint32_t addr;
    uint32_t bank;
    uint32_t wrData[8];
    uint32_t rdData[8];
    uint32_t i=0;
    bool fail_status=false;
    bool local_status = true; // true indicates pass
    // for debug path, make sure VDC/VGA is disabled
    EP0WRITE(0x1001,0x0); // disable VDC
    EP0WRITE(0x3001,0x0); // disable Vga
    EP0WRITE(0x2010,0x4); // don't optimize for burst length of 6
    EP0FLUSH;

    ////   uint32_t stopAddr = 2^20-1;
    //// if simple read/write works, then can do rate control to prevent
    //// buffer overflow
    //uint32_t stopAddr = 2^1;
    
    // tests for addr stuck bits
    
    // tests for bank stuck bits

    // tests for data stuck bits
    // walking 1's
    wrData[0] = 0x84218421;
    wrData[1] = 0x12481248;
    wrData[2] = 0xacefacef;
    wrData[3] = 0xfecafeca;
    wrData[4] = 0x84218421;
    wrData[5] = 0x12481248;
    wrData[6] = 0xacefacef;
    wrData[7] = 0xfecafeca;

    local_status = wrRdDdr(0xb2, 0x1, wrData);    

    if(!local_status) {fail_status = true;};

    wrData[0] = 0x11111111;
    wrData[1] = 0x22222222;
    wrData[2] = 0x44444444;
    wrData[3] = 0x88888888;
    wrData[4] = 0xaaaaaaaa;
    wrData[5] = 0xcccccccc;
    wrData[6] = 0xeeeeeeee;
    wrData[7] = 0xffffffff;

    local_status = wrRdDdr(0xb2, 0x2, wrData);    

    if(!local_status) {fail_status = true;};

    if(fail_status) {
      return TEST_FAIL;
    }
    else {
      return TEST_OK;
    }
}



/*
   RtnStatus_t epLoopbackTest(void*) {
   Image *img = new Image;
   assert (img != NULL);
   if ((img->readBmp("bmps/desktop_800_600.bmp") == -1)) {
   printf ("ERROR: couldn't read BMP\n");
   exit(1);
   }
   printf ("Done w/ Read BMP\n");

//EP0WRITE(0x3001,0x1);

uint32_t totlen = 0;

queue<TnpClient::RxPayload> eq;

// Now go thru raw tiles, convert them to RLE, and send them to the HW
TnpClient::RxPayload e;

uint32_t tot=0;

for (uint16_t tileX = 0; tileX < img->getNumXTiles(); tileX++) {
for (uint16_t tileY = 0; tileY < img->getNumYTiles(); tileY++) {
RleTile rleTile;
rleTile.rleEncode(img->getRawTileData(tileX, tileY), tileX, tileY);
cli->sendMessage(13,rleTile.getRleTileData(),rleTile.getRleTileDataLength());
cli->service(0);
//EP0CFGREAD(0x0);
totlen += rleTile.getRleTileDataLength();

e.len = rleTile.getRleTileDataLength();
e.data = new uint8_t[e.len];
memcpy(e.data,rleTile.getRleTileData(),e.len);
eq.push(e);

while(cli->getMessage(e)) {
if(eq.empty()) {
printf("ERROR: Got a message while expected queue was empty\n");
delete [] e.data;
fflush(stdout);
return TEST_FAIL;
} else {
if(eq.front().len != e.len || memcmp(eq.front().data,e.data,e.len)) {
printf("ERROR: Message miss-match\n");
printCompare(e, eq.front());
delete [] eq.front().data;
delete [] e.data;
fflush(stdout);
return TEST_FAIL;
}
else {
printf ("MATCHED %0d bytes (X,Y)=(%0d,%0d)\n", e.len, tileX, tileY);
}
eq.pop();
}
}
tot++;

//if(tot>10) break;
}

//if(tot>10) break;
}

printf("Waiting for remaining messages (%d/%d)...\n",eq.size(),tot); fflush(stdout);
//printf("READ OOB DATA: 0x%08x\n", EP0OOBCFGREAD(0));

uint32_t giveup = 0;

while(!eq.empty()) {
cli->service(0);

while(cli->getMessage(e)) {
    if(eq.empty()) {
        printf("ERROR: Got a message while expected queue was empty\n");
        delete [] e.data;
        fflush(stdout);
        return TEST_FAIL;
    } else {
        if(eq.front().len != e.len || memcmp(eq.front().data,e.data,e.len)) {
            printf("ERROR: Message miss-match\n");
            printCompare(e, eq.front());
            delete [] eq.front().data;
            delete [] e.data;
            fflush(stdout);
            return TEST_FAIL;
        }
        eq.pop();
    }
}

giveup++;
if(giveup > 50) {
    printf("GIVING UP\n");
    break;
    return TEST_FAIL;
}
}
*/
void dumpClkSynthRegs () {

    uint32_t data =0;

    for (uint32_t i=0; i<0x70;i++) {
        data = CLKSYNREAD (i);
        printf ("Reg @ %08x = %x \n", i, data);
    }
}

void setClkSynthFreq_p4 (uint32_t freq) {
	p4ClkSynth_t ClkSynthSettings[] = {
	{25, 0x22d3e9},
	{31, 0x201b88},
	{40, 0x200801},
	{49, 0x225f17},
	{50, 0x220801},
	{65, 0x270f83},
	{78, 0x231b88},
	{82, 0x234e17},
	{108, 0x213217},
	{135, 0x210983},
	{139, 0x213392},
	{140, 0x210a03},
	{154, 0x214917},
	{162, 0x214d17}
	};

	uint32_t p4ClkSynthSettingsSize = sizeof(ClkSynthSettings)/sizeof(p4ClkSynth_t);
	uint32_t wdSetting = 0;
	
	for (uint32_t i=0; i<p4ClkSynthSettingsSize; i++ ) {
		if (ClkSynthSettings[i].freq == freq) {
			wdSetting = ClkSynthSettings[i].wdSetting;
			break;
		}
	}

	if (wdSetting == 0) {
		printf ("Can't find the requested Frequency setting for the Clk Synth (P4)\n");
		return;
	}

	clkWordWrite(wdSetting);
}

void setClkSynthFreq_pre_p4(uint32_t freq)
{
    uint32_t data = 1 << 16 | 1<< 8 | 1;
    EP0WRITE(0x10, data);

    CLKSYNWRITE (0x1c, 0x00); // Set the 'OE' setting for PLL0 to 1 

    RegEntry8 **pll0Settings;
    uint32_t tableSize;

    switch (freq)
    {
        case 25 : 
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_25_175M_settings[j].addr;
                pll0Settings[j]->data = Pll0_25_175M_settings[j].data;
            }
            break;

        case 31 : 
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_31_5M_settings[j].addr;
                pll0Settings[j]->data = Pll0_31_5M_settings[j].data;
            }
            break;

        case 38 : 
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_38_25M_settings[j].addr;
                pll0Settings[j]->data = Pll0_38_25M_settings[j].data;
            }
            break;
        case 40 : 
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_40M_settings[j].addr;
                pll0Settings[j]->data = Pll0_40M_settings[j].data;
            }
            break;
        case 49 : 
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_49_5M_settings[j].addr;
                pll0Settings[j]->data = Pll0_49_5M_settings[j].data;
            }
            break;
        
        case 50 : 
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_50M_settings[j].addr;
                pll0Settings[j]->data = Pll0_50M_settings[j].data;
            }
            break;

        case 65 : 
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_65M_settings[j].addr;
                pll0Settings[j]->data = Pll0_65M_settings[j].data;
            }
            break;

        case 78 : 
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_78_75M_settings[j].addr;
                pll0Settings[j]->data = Pll0_78_75M_settings[j].data;
            }
            break;
        
        case 82 : // use 81MHz 
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_81M_settings[j].addr;
                pll0Settings[j]->data = Pll0_81M_settings[j].data;
            }
            break;
        
        case 108 :
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_108M_settings[j].addr;
                pll0Settings[j]->data = Pll0_108M_settings[j].data;
            }
            break;

        case 135 :
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_135M_settings[j].addr;
                pll0Settings[j]->data = Pll0_135M_settings[j].data;
            }
            break;

        case 139 :
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_139M_settings[j].addr;
                pll0Settings[j]->data = Pll0_139M_settings[j].data;
            }
            break;

        case 154 :
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_154M_settings[j].addr;
                pll0Settings[j]->data = Pll0_154M_settings[j].data;
            }
            break;

        case 162 :
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_162M_settings[j].addr;
                pll0Settings[j]->data = Pll0_162M_settings[j].data;
            }
            break;

        default :
            tableSize       = PllSettingSize;
            pll0Settings    = (RegEntry8**)malloc(tableSize*sizeof(RegEntry8*));
            for (uint32_t j=0;j<tableSize;j++) {
                pll0Settings[j] = (RegEntry8*)malloc(sizeof(RegEntry8));
                pll0Settings[j]->addr = Pll0_50M_settings[j].addr;
                pll0Settings[j]->data = Pll0_50M_settings[j].data;
            }
            printf ("[setClkSynthFreq] : Trying to set an unrecognized frequency -- Reverting to 50 Mhz\n");
    }

    // PLL 0 Programming from IDT's programming tool
    for (uint32_t i=0; i < tableSize; i++) {
      CLKSYNWRITE (pll0Settings[i]->addr, pll0Settings[i]->data);
      //printf ("Writing to CLK_SYNTH, %02x @ addr %02x\n",
      //	      pll0Settings[i]->data, pll0Settings[i]->addr);
    } 

    for (uint32_t i=0; i < tableSize; i++) {
      //uint32_t readData = CLKSYNREAD (pll0Settings[i]->addr);
      ////if(readData != pll0Settings[i]->data) {
      //printf ("Clk Synth Read data = %02x, Write data = %02x @ addr %02x\n", 
      //	      readData, pll0Settings[i]->data, pll0Settings[i]->addr);
      ////}
    } 

    for (uint32_t j=0;j<tableSize;j++) {
        delete pll0Settings[j];
    }
    delete pll0Settings;

    printf ("Clk Synth read back = %08x\n", CLKSYNREAD(0x7));
}

void setClkSynthFreq (int freq) {
    if (*boardName == "AttoP2") {
	setClkSynthFreq_pre_p4 (freq);
    } else {
	setClkSynthFreq_p4 (freq);
    }

}

/*
// Moved this functionality to sub routine SetClkSynth -- vgaBasic now runs with both P3 and P4
RtnStatus_t newVgaBasic(void* ptr) {
    // 135 Mhz setting (0x210983)
    // uint32_t wdSetting = 0x2244a2; // 25.174 MHz Setting. Lowest Jitter.
    // uint32_t wdSetting = 0x22d3e9; // 25.175 MHz Setting. Best Accuracy.
     uint32_t wdSetting = 0x201b88; // 31.5 MHz Setting. 
    // uint32_t wdSetting = 0x200801; // 40 MHz Setting. Best Accuracy.
    // uint32_t wdSetting = 0x225f17; // 49.5 MHz Setting. Best Accuracy.
    // uint32_t wdSetting = 0x220801; // 50 MHz Setting. Best Accuracy.
    // uint32_t wdSetting = 0x270f83; // 65 MHz Setting. Best Accuracy.
    // uint32_t wdSetting = 0x231b88; // 78.75 MHz Setting. Best Accuracy.
    // uint32_t wdSetting = 0x213217; // 108 MHz Setting. Best Accuracy.
  //uint32_t wdSetting = 0x210983; // 135 MHz Setting.
    // uint32_t wdSetting = 0x212917; // 154 MHz Setting. Best Accuracy.
    // uint32_t wdSetting = 0x214917; // 154 MHz Setting. (Choon found typo)
    // uint32_t wdSetting = 0x214d17; // 162 MHz Setting. Best Accuracy.

    // 65 Mhz setting
    //uint32_t wdSetting = 0x0 << 22 | // C1C0 : Capacitor value of 22.1 - 0.094 * 27 (Mhz)
    //                0x1 << 21 | // TTL  : Duty Cycle measured at VDD/2
    //                0x0 << 19 | // F1F0 : CLK2 Output set to REF
    //                0x7 << 16 | // S2S1S0 : Output Divide set to '6'
    //                0x039 << 7| // VDW : VCO Divider set to '57'
    //                0x9       ; // RDW : Reference/Input Divider set to '9'
                    
    clkWordWrite(wdSetting);

    return TEST_OK;
}    
*/

RtnStatus_t vgaBasic(void* ptr) {

    ScreenRes_t res = {1024,768,60, 50};
    ScreenRes_t res_640_60 = {640,480,60, 25};
    ScreenRes_t res_640_75 = {640,480,75, 31};
    ScreenRes_t res_800_60 = {800,600,60, 40};
    ScreenRes_t res_800_75 = {800,600,75, 49};
    ScreenRes_t res_1024_60 = {1024,768,60, 65};
    ScreenRes_t res_1024_75 = {1024,768,75, 78};
    ScreenRes_t res_1280_60 = {1280,1024,60, 108};
    ScreenRes_t res_1280_75 = {1280,1024,75, 135};
    ScreenRes_t res_1600_60 = {1600,1200,60, 162};

    if (ptr) {
        res = ScreenRes_t(*(ScreenRes_t*)ptr);
    }

    // Start by disabling VGA
    EP0WRITE(0x3001,0x0); // VGA Enable pattern
    
    //vgaInit(&res_1280_75);
    vgaInit(&res_1600_60);

    EP0WRITE(0x3001,0x2); // VGA Test pattern
    EP0FLUSH;

    uint32_t data = EP0READ(0x3001); // VGA Test pattern
    printf ("VGA Pattern Test Read back = %08x \n", data);

    EP0WRITE(0x3001,0x3); // VGA Enable pattern
    data = EP0READ(0x3001); // VGA Enable pattern
    printf ("VGA Enable Read back = %08x \n", data);
    EP0FLUSH;

    return TEST_OK;
}    

void dump_vga_regs() {
   printf("cfgVgaDramLineIncrAddr = %x\n", EP0READ(0x3000));
   printf("vgaEnableAddr = %x\n", EP0READ(0x3001));
   printf("cfgVgaLinePelsAddr = %x\n", EP0READ(0x3002));
   printf("cfgVgaHVisPelsAddr = %x\n", EP0READ(0x3003));
   printf("cfgVgaVVisPelsAddr = %x\n", EP0READ(0x3004));
   printf("cfgVgaHsyncStartAddr = %x\n", EP0READ(0x3005)); 
   printf("cfgVgaHsyncEndAddr = %x\n", EP0READ(0x3006));
   printf("cfgVgaVsyncStartAddr = %x\n", EP0READ(0x3007)); 
   printf("cfgVgaVsyncEndAddr = %x\n", EP0READ(0x3008)); 
   printf("cfgVgaFrameLinesAddr = %x\n", EP0READ(0x3009));  
   printf("cfgVgaHVisStartAddr = %x\n", EP0READ(0x300a));  
   printf("cfgVgaVVisStartAddr = %x\n", EP0READ(0x300b));  
   printf("cfgVgaPolaritiesAddr = %x\n", EP0READ(0x300c));
   printf("vgaPageAddr = %x\n", EP0READ(0x300d));
   printf("ddrRdDataAddr = %x\n", EP0READ(0x3010));
   printf("ddrErrCntAddr = %x\n", EP0READ(0x3012));
   printf("ddrErrLaneAddr = %x\n", EP0READ(0x3013));
   printf("ddrExpDataLoAddr = %x\n", EP0READ(0x3014));
   printf("ddrExpDataHiAddr = %x\n", EP0READ(0x3015));
   printf("vgaErrAddr = %x\n", EP0READ(0x3020));
   printf("vgaCursorPosAddr = %x\n", EP0READ(0x3021));
   printf("vgaCrcAddr = %x\n", EP0READ(0x3022));
   printf("vgaCrcCfgAddr = %x\n", EP0READ(0x3023));
}

RtnStatus_t vgaScanout(void* ptr) {
   
   ScreenRes_t res = {1024,768,60, 50};
   ScreenRes_t res_640_60 = {640,480,60, 25};
   ScreenRes_t res_640_75 = {640,480,75, 31};
   ScreenRes_t res_800_60 = {800,600,60, 40};
   ScreenRes_t res_800_75 = {800,600,75, 49};
   ScreenRes_t res_1024_60 = {1024,768,60, 65};
   ScreenRes_t res_1024_75 = {1024,768,75, 78};
   ScreenRes_t res_1280_60 = {1280,1024,60, 108};
   ScreenRes_t res_1280_75 = {1280,1024,75, 135};
   ScreenRes_t res_1600_60 = {1600,1200,60, 162};
   ScreenRes_t res_1920_60 = {1920,1200,60, 154};

   if (ptr) {
       res = ScreenRes_t(*(ScreenRes_t*)ptr);
   }
   
   //vgaInit(&res_640_60);
   vgaInit(&res_800_60);
   //vgaInit(&res_1024_60);
   //vgaInit(&res_1280_60);
   //vgaInit(&res_1600_60);
   //vgaInit(&res_640_75);
   //vgaInit(&res_800_75);
   //vgaInit(&res_1024_75);
   //vgaInit(&res_1280_75);
   //vgaInit(&res_1920_60);

   ddrDbgClearScreen();
   //vdcWriteTiles();
   //vdcWriteTilesNonRpt();
   vdc15Write2ColorTile();

   dump_vga_regs();

   EP0WRITE(0x3001,0x1); // VGA Enable pattern
   //EP0WRITE(0x3001,0x3); // VGA Enable pattern
   uint32_t data = EP0READ(0x3001); // VGA Enable pattern
   printf ("VGA Enable Read back = %08x \n", data);
   printf ("DDR Error Status Reg = %08x \n", EP0READ(0x2020));
   printf ("VDC Ctrl Reg = %08x \n", EP0READ(0x1001));
   EP0FLUSH;

   sleep(10);

   return TEST_OK;
}    

RtnStatus_t vdcRLETile(int x=0, int y=0, int sixteeen_color=0, char *color=0) {
    vector<char> data;

    data.push_back(0x01 | (sixteeen_color ? 0x8 : 0x0));
    data.push_back(x);
    data.push_back(y);
    data.push_back(0x80 | 0x3f);
    data.push_back(color[0]);
    data.push_back(color[1]);
    data.push_back(color[2]);

    char *cdata;
    cdata = new char [data.size()];
    copy(data.begin(), data.end(), cdata);
    sStream->epWrite(videoDrv->Endpoint, cdata, data.size());
    delete [] cdata;

    return TEST_OK;
}
RtnStatus_t vdcRLETileUnique(int x=0, int y=0, int sixteeen_color=0, char *color=0) {
    vector<char> data;

    data.push_back(0x01 | (sixteeen_color ? 0x8 : 0x0));
    data.push_back(x);
    data.push_back(y);


    for (int i=0; i < 4; i++) {
    data.push_back(0x80 | 0x1f);
    data.push_back(color[0]);
    data.push_back(color[1]);
    data.push_back(color[2]);
    }

    char *cdata;
    cdata = new char [data.size()];
    copy(data.begin(), data.end(), cdata);
    sStream->epWrite(videoDrv->Endpoint, cdata, data.size());
    delete [] cdata;

    return TEST_OK;
}

void vdcRedrawTile() {
   char green[] = {0,0xff,0};
   char red[] = {0,0,0xff};
   char blue[] = {0xff,0,0};
   char color[][3] = {{0,0xff,0},{0,0,0xff},{0xff,0,0}};
   for (int i=0; i<800/8; i++) {
       for (int j=0; j<600/8; j++) {
           vdcRLETile(i,j,0,color[i%3]);
       }
   }
   FLUSH;
   sleep(7);

}

void vdcRedrawTile2() {
   char green[] = {0,0xff,0};
   char red[] = {0,0,0xff};
   char blue[] = {0xff,0,0};
   char color[][3] = {{0,0x8f,0},{0,0,0x8f},{0x8f,0,0}};
   for (int i=0; i<800/8; i++) {
       for (int j=0; j<600/8; j++) {
           vdcRLETile(i,j,0,color[i%3]);
       }
   }
   FLUSH;
   sleep(7);

}

void vdcRedrawTile_shiftright(char * color) {
  
  char newcolor[3];
   for (int i=0; i<800/8; i++) {
     for (int j=0; j<600/8; j++) {
       
       vdcRLETileUnique(i,j,0,color);
       newcolor[0] = ((color[0] >> 1) & 0x7f) | ((color[2] << 7) & 0x80);
       newcolor[1] = ((color[1] >> 1) & 0x7f) | ((color[0] << 7) & 0x80);
       newcolor[2] = ((color[2] >> 1) & 0x7f) | ((color[1] << 7) & 0x80);
       
       for (int a=0; a<3; a++)
	 color[a] = newcolor[a];
       
     }
   }
   FLUSH;
   sleep(2);
}

void vdcRedrawTile_64color(char * orig_color) {
 
  char newcolor[3];
  char color[3];
  char data[300];
  int datasize = 0;
  int sixteen_color = 0;
  int z;

  color[0] = orig_color[0];
  color[1] = orig_color[1];
  color[2] = orig_color[2];
  
  for (int j=74; j<600/8; j++) {   
    for (int i=0; i<800/8; i++) {
    data[0] = (0x01 | (sixteen_color ? 0x8 : 0x0));
      data[1] = (i);
       data[2] = (j);

       datasize = 3;
       
       for (int k=0; k<64; k++) {
	 data[3+(4*k)] = 0x00;  //unique
	 data[4+(4*k)] = color[0];
	 data[5+(4*k)] = color[1];
	 data[6+(4*k)] = color[2];
	 datasize += 4;

     	 newcolor[0] = ((color[0] >> 1) & 0x7f) | ((color[2] << 7) & 0x80);
	 newcolor[1] = ((color[1] >> 1) & 0x7f) | ((color[0] << 7) & 0x80);
	 newcolor[2] = ((color[2] >> 1) & 0x7f) | ((color[1] << 7) & 0x80);
	 
	 for (int a=0; a<3; a++)
	   color[a] = newcolor[a];
       }
       sStream->epWrite(videoDrv->Endpoint, data, datasize);   
     }
   }
   FLUSH;
   sleep(5);
}


RtnStatus_t vdcTileRepeat(int x=0, int y=0, int w=800, int h=600, int sixteeen_color=0) {
    vector<char> data;

    data.push_back(0x27 | (sixteeen_color ? 0x8 : 0x0));
    data.push_back(x);
    data.push_back(y);
    data.push_back((x+w-1)/8);
    data.push_back((y+h-1)/8);
    data.push_back(0xff);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0xff);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0xff);
    data.push_back(0xff);
    data.push_back(0xff);
    data.push_back(0xff);
    for (int i=0; i<8; i++) {
        data.push_back(0xe4);
        data.push_back(0xe4);
    }

    char *cdata;
    cdata = new char [data.size()];
    copy(data.begin(), data.end(), cdata);
    sStream->epWrite(videoDrv->Endpoint, cdata, data.size());
    delete [] cdata;

    return TEST_OK;
}

RtnStatus_t vdc4Color(int x=0, int y=0, int sixteeen_color=0) {
    vector<char> data;

    data.push_back(0x25 | (sixteeen_color ? 0x8 : 0x0));
    data.push_back(x);
    data.push_back(y);
    data.push_back(0xff);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0xff);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0xff);
    data.push_back(0xff);
    data.push_back(0xff);
    data.push_back(0xff);
    for (int i=0; i<8; i++) {
        data.push_back(0x50);
        data.push_back(0xfa);
    }

    char *cdata;
    cdata = new char [data.size()];
    copy(data.begin(), data.end(), cdata);
    sStream->epWrite(videoDrv->Endpoint, cdata, data.size());
    delete [] cdata;

    return TEST_OK;
}

RtnStatus_t vdc2Color(int x=0, int y=0, int sixteeen_color=0) {
    vector<char> data;

    data.push_back(0x15 | (sixteeen_color ? 0x8 : 0x0));
    data.push_back(x);
    data.push_back(y);
    data.push_back(0xff);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0xff);
    for (int i=0; i<8; i++) {
        data.push_back(0xc3);
    }

    char *cdata;
    cdata = new char [data.size()];
    copy(data.begin(), data.end(), cdata);
    sStream->epWrite(videoDrv->Endpoint, cdata, data.size());
    delete [] cdata;

    return TEST_OK;
}

RtnStatus_t vdc15Test(void* ptr) {
   ScreenRes_t res = {1024,768,60, 50};
   ScreenRes_t res_640_60 = {640,480,60, 25};
   ScreenRes_t res_640_75 = {640,480,75, 31};
   ScreenRes_t res_800_60 = {800,600,60, 40};
   ScreenRes_t res_800_75 = {800,600,75, 49};
   ScreenRes_t res_1024_60 = {1024,768,60, 65};
   ScreenRes_t res_1024_75 = {1024,768,75, 78};
   ScreenRes_t res_1280_60 = {1280,1024,60, 108};
   ScreenRes_t res_1280_75 = {1280,1024,75, 135};
   ScreenRes_t res_1600_60 = {1600,1200,60, 162};
   ScreenRes_t res_1920_60 = {1920,1200,60, 154};

   if (ptr) {
       res = ScreenRes_t(*(ScreenRes_t*)ptr);
   }
   
   vgaInit(&res_800_60);

   ddrDbgClearScreen_2();

   EP0WRITE(0x1001, 0x23);
   printf("Enabling VDC 1.5...\n");
   EP0FLUSH;
   
   // Enable VGA
   EP0WRITE(0x3001,0x1);
   uint32_t data = EP0READ(0x3001);
   printf ("VGA Enable Read back = %08x \n", data);
   printf ("VDC Ctrl Reg = %08x \n", EP0READ(0x1001));
   EP0FLUSH;
   sleep(5);

   // 2 color pallette
   printf("vdc2Color\n");
   for (int i=0; i<800/8; i++) {
       for (int j=0; j<600/8; j++) {
           vdc2Color(i,j);
        }
   }
   FLUSH;

   sleep(5);
   
   // 4 color pallette
   printf("vdc4Color\n");
   for (int i=0; i<800/8; i++) {
       for (int j=0; j<600/8; j++) {
           vdc4Color(i,j);
        }
   }
   FLUSH;

   sleep(5);
   
   // RLE
   printf("vdcRLETile\n");
   char green[] = {0,0xff,0};
   char red[] = {0,0,0xff};
   char blue[] = {0xff,0,0};
   char color[][3] = {{0,0xff,0},{0,0,0xff},{0xff,0,0}};
   for (int i=0; i<800/8; i++) {
       for (int j=0; j<600/8; j++) {
           vdcRLETile(i,j,0,color[i%3]);
       }
   }
   FLUSH;
   sleep(5);


   // Tile repeat
   printf("vdcTileRepeat\n");
   vdcTileRepeat(0,0, 800, 600);
   FLUSH;

   return TEST_OK;
}

RtnStatus_t vdcCheckerBoardTest(void* ptr) {

    ScreenRes_t res_800_60 = {800,600,60, 40};
    vgaInit(&res_800_60);
    EP0WRITE(0x3001,0x1); // VGA Enable pattern
    EP0FLUSH;

    ddrDbgClearScreen_2();
    uint32_t FrameBufferSize = 2048*2048*4;
    uint8_t checkerboardStoreBytes[] =
        { 0x90, 
          ( FrameBufferSize >> 24 ) & 0xff, 
          ( FrameBufferSize >> 16 ) & 0xff, 
          ( FrameBufferSize >> 8 ) & 0xff, 
          (( FrameBufferSize >> 0 ) & 0xff ) + 0x60, 0, 0x60,
          0x9, 0x0, 0x0, 0x80 | 63, 0xff, 0xff,
          0x9, 0x0, 0x0, 0x80 | 63, 0x00, 0x00 };

    sStream->epWrite(videoDrv->Endpoint, reinterpret_cast<char *>( checkerboardStoreBytes ), 7);

    for ( int i = 0; i < 81; ++i ) {
        checkerboardStoreBytes[ 7 + ( i % 2 ? 6 : 0 ) + 1 ] = i % 9;
        checkerboardStoreBytes[ 7 + ( i % 2 ? 6 : 0 ) + 2 ] = i / 9;
        sStream->epWrite(videoDrv->Endpoint, reinterpret_cast<char *>
                         ( checkerboardStoreBytes + 7 + ( i % 2 ? 6 : 0 )),
                         6);
    }

    uint8_t checkerboardRenderBytes[] = {
        0x90, 0x0, 0x0, 0x0, 0x0, 0x8, 0x0,
        0xA0, 0x1, 0x00, 0x0, 0x60, 0, 0x60, 0, 0x05, 0x0, 0x3, 0x0, 0x28, 0, 0x23, 0, 0x20, 0, 0x20, 0x0 }; 

    sStream->epWrite(videoDrv->Endpoint, reinterpret_cast<char *>( checkerboardRenderBytes ), 7);
    sStream->epWrite(videoDrv->Endpoint, reinterpret_cast<char *>( checkerboardRenderBytes ) + 7, 20);
    FLUSH;


    int src_x = 5;
    int src_y = 3;
    int w = 0x28;
    int h = 0x24;

    for (h=1; h<=0x28; h+=1) {
        cout << "hit any key to continue... " << h << endl;
        cin.ignore(1000,'\n');
        ddrDbgClearScreen_2();
        checkerboardRenderBytes[7+8] = src_x;
        checkerboardRenderBytes[7+10] = src_y;
        checkerboardRenderBytes[7+12] = w;
        checkerboardRenderBytes[7+14] = h;
        sStream->epWrite(videoDrv->Endpoint, reinterpret_cast<char *>( checkerboardRenderBytes ) + 7, 20);
        FLUSH;
    }


    return TEST_OK;
}

RtnStatus_t vdcFillRectTest(void *ptr) {
    ScreenRes_t res = {1024,768,60, 50};
    ScreenRes_t res_640_60 = {640,480,60, 25};
    ScreenRes_t res_640_75 = {640,480,75, 31};
    ScreenRes_t res_800_60 = {800,600,60, 40};
    ScreenRes_t res_800_75 = {800,600,75, 49};
    ScreenRes_t res_1024_60 = {1024,768,60, 65};
    ScreenRes_t res_1024_75 = {1024,768,75, 78};
    ScreenRes_t res_1280_60 = {1280,1024,60, 108};
    ScreenRes_t res_1280_75 = {1280,1024,75, 135};
    ScreenRes_t res_1600_60 = {1600,1200,60, 162};
    ScreenRes_t res_1920_60 = {1920,1200,60, 154};

    if (ptr) {
        res = ScreenRes_t(*(ScreenRes_t*)ptr);
    }

    vgaInit(&res_800_60);

    ddrDbgClearScreen_2();

    vector<char> color(3);
    color[0] = 0xff;
    color[1] = 0x00;
    color[2] = 0x00;
    vdcFillRect(0,0,32,32,color);
    color[0] = 0x00;
    color[1] = 0xff;
    color[2] = 0x00;
    vdcFillRect(32,32,32,32,color);
    color[0] = 0x00;
    color[1] = 0x00;
    color[2] = 0xff;
    vdcFillRect(32,0,32,32,color);
    color[0] = 0xff;
    color[1] = 0x00;
    color[2] = 0x00;
    vdcFillRect(0,32,32,32,color);

    EP0WRITE(0x3001,0x1); // VGA Enable pattern
    EP0FLUSH;
    return TEST_OK;
}

RtnStatus_t vdcCursorBlitTest(void *ptr) {
    ScreenRes_t res = {1024,768,60, 50};
    ScreenRes_t res_640_60 = {640,480,60, 25};
    ScreenRes_t res_640_75 = {640,480,75, 31};
    ScreenRes_t res_800_60 = {800,600,60, 40};
    ScreenRes_t res_800_75 = {800,600,75, 49};
    ScreenRes_t res_1024_60 = {1024,768,60, 65};
    ScreenRes_t res_1024_75 = {1024,768,75, 78};
    ScreenRes_t res_1280_60 = {1280,1024,60, 108};
    ScreenRes_t res_1280_75 = {1280,1024,75, 135};
    ScreenRes_t res_1600_60 = {1600,1200,60, 162};
    ScreenRes_t res_1920_60 = {1920,1200,60, 154};

    if (ptr) {
        res = ScreenRes_t(*(ScreenRes_t*)ptr);
    }

    vgaInit(&res_800_60);

    ddrDbgClearScreen_2();
    vdc15Write2ColorTile(0,0,0xc3);

    vdcSetDest(0x01000060, 0x20);
    for (int i=0; i<4; i++) for (int j=0; j<4; j++) {
        vdc15Write2ColorTileWithColor(i,j,0x3c,green_blue_pal());
    }

    vdcSetDest(0,2048);
    //vdcMemBlit(0x01000060,32,0,0,32,32,0,0,0);
    vdcCursorBlit(0x01000060);
    vdcMemBlit(0x01000060,32,0,0,32,32,32,32,0);

    EP0WRITE(0x3001,0x1); // VGA Enable pattern
    EP0FLUSH;
    return TEST_OK;
}

RtnStatus_t vdcMemBlitTest(void* ptr) {
   
   ScreenRes_t res = {1024,768,60, 50};
   ScreenRes_t res_640_60 = {640,480,60, 25};
   ScreenRes_t res_640_75 = {640,480,75, 31};
   ScreenRes_t res_800_60 = {800,600,60, 40};
   ScreenRes_t res_800_75 = {800,600,75, 49};
   ScreenRes_t res_1024_60 = {1024,768,60, 65};
   ScreenRes_t res_1024_75 = {1024,768,75, 78};
   ScreenRes_t res_1280_60 = {1280,1024,60, 108};
   ScreenRes_t res_1280_75 = {1280,1024,75, 135};
   ScreenRes_t res_1600_60 = {1600,1200,60, 162};
   ScreenRes_t res_1920_60 = {1920,1200,60, 154};

   if (ptr) {
       res = ScreenRes_t(*(ScreenRes_t*)ptr);
   }
   
   vgaInit(&res_1920_60);

   ddrDbgClearScreen_2();

   vdc15Write2ColorTile(0,0,0xc3);
   EP0WRITE(0x3001,0x1); // VGA Enable pattern
   uint32_t data = EP0READ(0x3001); // VGA Enable pattern
   printf ("VGA Enable Read back = %08x \n", data);
   printf ("VDC Ctrl Reg = %08x \n", EP0READ(0x1001));
   EP0FLUSH;

   vdcSetDest(0, 2048);
   int pos_x[] = {0, 452, 272, 0, 0, 272};
   int pos_y[] = {0, 0, 0, 272, 452, 272};

   sleep(5);

   for (int j=0; j<8; j++) {
       for (int i=0; i<10; i++) {
            vdcMemBlit(0, 2048, 176*i, 176*j, 168, 168, 176*(i+1), 176*j, 0);
       }
       if (j != 7) {
            vdcMemBlit(0, 2048, 0, 176*j, 168, 168, 0, 176*(j+1), 0);
       }
   }

   return TEST_OK;
}    

RtnStatus_t vgaPatternTest(void* ptr) {
   
   ScreenRes_t res = {1024,768,60, 50};
   ScreenRes_t res_640_60 = {640,480,60, 25};
   ScreenRes_t res_640_75 = {640,480,75, 31};
   ScreenRes_t res_800_60 = {800,600,60, 40};
   ScreenRes_t res_800_75 = {800,600,75, 49};
   ScreenRes_t res_1024_60 = {1024,768,60, 65};
   ScreenRes_t res_1024_75 = {1024,768,75, 78};
   ScreenRes_t res_1280_60 = {1280,1024,60, 108};
   ScreenRes_t res_1280_75 = {1280,1024,75, 135};
   ScreenRes_t res_1600_60 = {1600,1200,60, 162};
   ScreenRes_t res_1920_60 = {1920,1200,60, 154};
   

   if (ptr) {
       res = ScreenRes_t(*(ScreenRes_t*)ptr);
   }
   
   //vgaInit(&res_640_60);
   // vgaInit(&res_800_60);
   // vgaInit(&res_1024_60);
   //vgaInit(&res_1280_60);
   vgaInit(&res_1600_60);
   //   vgaInit(&res_640_75);
   //vgaInit(&res_800_75);
   //   vgaInit(&res_1024_75);
   //vgaInit(&res_1280_75);
   //vgaInit(&res_1920_60);

   vdcWriteTestVga();
   
   EP0WRITE(0x3001,0x1); // VGA Enable pattern
   //EP0WRITE(0x3001,0x3); // VGA Enable pattern
   uint32_t data = EP0READ(0x3001); // VGA Enable pattern
   printf ("VGA Enable Read back = %08x \n", data);
   EP0FLUSH;

    return TEST_OK;
}    

RtnStatus_t vgaCrcTest(void* ptr) {
   uint32_t data;
   uint32_t crc_data;

   ScreenRes_t res = {1024,768,60, 50};
   ScreenRes_t res_640_60 = {640,480,60, 25};
   ScreenRes_t res_640_75 = {640,480,75, 31};
   ScreenRes_t res_800_60 = {800,600,60, 40};
   ScreenRes_t res_800_75 = {800,600,75, 49};
   ScreenRes_t res_1024_60 = {1024,768,60, 65};
   ScreenRes_t res_1024_75 = {1024,768,75, 78};
   ScreenRes_t res_1280_60 = {1280,1024,60, 108};
   ScreenRes_t res_1280_75 = {1280,1024,75, 135};
   ScreenRes_t res_1600_60 = {1600,1200,60, 162};
   ScreenRes_t res_1920_60 = {1920,1200,60, 154};
   
   // Starting with the pattern (0x6b, 0x57, 0x29) (randomly chosen), and shifting over by one every 8 pixels.
   char boxcolor[3] = {0x6b,0x57,0x29};

   
   if (ptr) {
       res = ScreenRes_t(*(ScreenRes_t*)ptr);
   }
   
   vgaInit(&res_800_60);

   ddrDbgClearScreen_2();

   EP0WRITE(0x1001, 0x23);
   printf("Enabling VDC 1.5...\n");
   EP0FLUSH;
   
   // Enable VGA
   EP0WRITE(0x3001,0x1);
   data = EP0READ(0x3001);
   printf ("VGA Enable Read back = %08x \n", data);
   printf ("VDC Ctrl Reg = %08x \n", EP0READ(0x1001));
   EP0FLUSH;
   sleep(5);

   printf("Start of vgaCrcTest, Running Six Tests.\n");
   uint32_t crc_red_A = 0x00006cbb;
   uint32_t crc_grn_A = 0x0000632d;
   uint32_t crc_blu_A = 0x00000a50;
   uint32_t crc_red_B = 0x0000fe46;
   uint32_t crc_grn_B = 0x0000fcd7;
   uint32_t crc_blu_B = 0x0000a840;
   uint32_t crc_correct;
   bool errDet = false;
   string* color;
   string* p_alt;
   color = new string("Green");
   p_alt = new string("A");

  
   vdcRedrawTile_64color(boxcolor);
   for(int i=0;i<6;i++) {
     if (i == 0) {
       crc_correct = crc_red_A;
       *color = string("Red");
       *p_alt = string("A");
     } else if (i == 1) {
       crc_correct = crc_grn_A;
       *color = string("Green");
       *p_alt = string("A");
     } else if (i == 2) {
       crc_correct = crc_blu_A;
       *color = string("Blue");
       *p_alt = string("A");
     } else if (i == 3) {
       crc_correct = crc_red_B;
       *color = string("Red");
       *p_alt = string("B");
     } else if (i == 4) {
       crc_correct = crc_grn_B;
       *color = string("Green");
       *p_alt = string("B");
     } else if (i == 5) {
       crc_correct = crc_blu_B;
       *color = string("Blue");
       *p_alt = string("B");
     }

     EP0WRITE(0x3023,i); // VGA Crc Cfg
     data = EP0READ(0x3023);
     printf ("Test(%0d) %0s : CRC Cfg Value  = %08x \n",i+1,color->c_str(), data);

     sleep(3);
     
     //vdcTileRepeat(0,0, 800, 600);
     FLUSH;

     crc_data = EP0READ(0x3022);
     if (crc_data == crc_correct) {
       printf ("PASS : VGA %0s \"%0s\" CRC Value  = %08x \n",color->c_str(),p_alt->c_str(),crc_data);
     } else {
       printf ("FAIL : VGA %0s \"%0s\" CRC Value  = %08x : Expected = %08x \n",color->c_str(),p_alt->c_str(),crc_data,crc_correct);
       errDet = true || errDet;
     }
   }// end for
  
   EP0FLUSH;

   if(errDet == false) { // no error observed
     return TEST_OK;
   }
   else {
     return TEST_FAIL;
   }
}    

RtnStatus_t vgaProjectorTest(void* ptr) {
   
   ScreenRes_t res_1024_60 = {1024,768,60, 65};
   ScreenRes_t res_1024_75 = {1024,768,75, 78};
   ScreenRes_t res_1280_60 = {1280,1024,60, 108};
   ScreenRes_t res_1280_75 = {1280,1024,75, 135};
   ScreenRes_t res_1600_60 = {1600,1200,60, 162};
   ScreenRes_t res_1920_60 = {1920,1200,60, 154};

   // mimicking 
   vgaInit(&res_1280_60); 
   EP0WRITE(0x3001,0x3); // VGA Enable pattern
   EP0FLUSH;
   sleep(5);
   // switching from broker to das
   EP0WRITE(0x3001,0x0); // disable Vga
   //sleep(1);
   vgaInit(&res_1024_60);
   vdcWriteTiles();
   EP0WRITE(0x3001,0x1); // VGA Enable pattern

   uint32_t data = EP0READ(0x3001); // VGA Enable pattern
   printf ("VGA Enable Read back = %08x \n", data);
   EP0FLUSH;
   sleep(5);

   return TEST_OK;
}    

RtnStatus_t vgaBorderTest(void* ptr) {
   
   ScreenRes_t res = {1024,768,60, 50};
   ScreenRes_t res_640_60 = {640,480,60, 25};
   ScreenRes_t res_640_75 = {640,480,75, 31};
   ScreenRes_t res_800_60 = {800,600,60, 40};
   ScreenRes_t res_800_75 = {800,600,75, 49};
   ScreenRes_t res_1024_60 = {1024,768,60, 65};
   ScreenRes_t res_1024_75 = {1024,768,75, 78};
   ScreenRes_t res_1024_75_cvt = {1024,768,75, 82};
   ScreenRes_t res_1280_60 = {1280,1024,60, 108};
   ScreenRes_t res_1280_75 = {1280,1024,75, 135};
   ScreenRes_t res_1280_75_cvt = {1280,1024,75, 139};
   ScreenRes_t res_1600_60 = {1600,1200,60, 162};
   ScreenRes_t res_1920_60 = {1920,1200,60, 154};
   ScreenRes_t res_1680_60 = {1680,1050,60, 154};
   
   if (ptr) {
       res = ScreenRes_t(*(ScreenRes_t*)ptr);
   }
   
   //vgaInit(&res_640_60);
   //vgaInit(&res_800_60);
   //vgaInit(&res_1024_60);
   //vgaInit(&res_1280_60); // !! observed once still shifted, cannot reproduce 
   //vgaInit(&res_1600_60);
   //vgaInit(&res_640_75);
   //vgaInit(&res_800_75);
   //vgaInit(&res_1024_75);
   //vgaInit(&res_1024_75_cvt);
   //vgaInit(&res_1280_75);
   //vgaInit(&res_1280_75_cvt);
   //vgaInit(&res_1920_60);
   vgaInit(&res_1680_60);

   vdcWriteBorder();
   
   EP0WRITE(0x3001,0x1); // VGA Enable pattern
   //EP0WRITE(0x3001,0x3); // VGA Enable pattern
   uint32_t data = EP0READ(0x3001); // VGA Enable pattern
   printf ("VGA Enable Read back = %08x \n", data);
   EP0FLUSH;

    return TEST_OK;
}    

RtnStatus_t vgaTestDdr(void* ptr) {
   
   ScreenRes_t res = {1024,768,60, 50};
   ScreenRes_t res_640_60 = {640,480,60, 25};
   ScreenRes_t res_640_75 = {640,480,75, 31};
   ScreenRes_t res_800_60 = {800,600,60, 40};
   ScreenRes_t res_800_75 = {800,600,75, 49};
   ScreenRes_t res_1024_60 = {1024,768,60, 65};
   ScreenRes_t res_1024_75 = {1024,768,75, 78};
   ScreenRes_t res_1280_60 = {1280,1024,60, 108};
   ScreenRes_t res_1280_75 = {1280,1024,75, 135};
   ScreenRes_t res_1280_75_cvt = {1280,1024,75, 139};
   ScreenRes_t res_1600_60 = {1600,1200,60, 162};
   ScreenRes_t res_1680_60 = {1680,1050,60, 162};
   ScreenRes_t res_1920_60 = {1920,1200,60, 154};
   
   if (ptr) {
       res = ScreenRes_t(*(ScreenRes_t*)ptr);
   }
   
   //vgaInit(&res_640_60);
   //vgaInit(&res_800_60);
   //   vgaInit(&res_1024_60);
   //vgaInit(&res_1280_60);
   //vgaInit(&res_1680_60);
   //   vgaInit(&res_640_75);
   //   vgaInit(&res_800_75);
   //vgaInit(&res_1024_75);
   //vgaInit(&res_1024_75);
   vgaInit(&res_1280_75);
   //vgaInit(&res_1600_60);
   //vgaInit(&res_1920_60);
   
   uint32_t i;
   uint32_t lanes;
   bool errDet = false;
   uint32_t data;

   // 0->1 transition test
   vdcWriteTestDdr();
   
   EP0WRITE(0x3001,0x1); // VGA Enable pattern
   data = EP0READ(0x3001); // VGA Enable pattern
//   printf ("VGA Enable Read back = %08x \n", data);
   EP0FLUSH;

   // disable DDR error counter, clears counter
   // sets up expected data
//   printf ("Monitoring error on DDR interface... \n");
   EP0WRITE(0x3012,0x00000000); // disable error counter
   EP0WRITE(0x3014,0xaaaaaaaa); // low data 
   EP0WRITE(0x3015,0x55555555); // high data
   EP0FLUSH;
   // enable DDR error counter
   EP0WRITE(0x3012,0x80000000); // enable error counter
   EP0FLUSH;

   // poll the error counter
   //while(!errDet) {
   for(i=0;i<8;i++) {
     sleep(1);
     data = EP0READ(0x3012) & 0x7fffffff; // error counter read
     printf("i=%d, DDR Error Counter = %08x\n", i, data);
     if(data > 0x0) {
       lanes = EP0READ(0x3013) & 0xffffffff; // error on lanes read
       printf("i=%d, DDR Lane Errors = %08x\n", i, lanes);
       errDet = true || errDet;
     }
   }
   //}

   // 1->0 transition test
   vdcWriteTestDdr2();
   
   EP0WRITE(0x3001,0x1); // VGA Enable pattern
   data = EP0READ(0x3001); // VGA Enable pattern
//   printf ("VGA Enable Read back = %08x \n", data);
   EP0FLUSH;

   // disable DDR error counter, clears counter
   // sets up expected data
//   printf ("Monitoring error on DDR interface... \n");
   EP0WRITE(0x3012,0x00000000); // disable error counter
   EP0WRITE(0x3014,0x55555555); // low data 
   EP0WRITE(0x3015,0xaaaaaaaa); // high data
   EP0FLUSH;
   // enable DDR error counter
   EP0WRITE(0x3012,0x80000000); // enable error counter
   EP0FLUSH;

   // poll the error counter
   //while(!errDet) {
   for(i=0;i<8;i++) {
     sleep(1);
     data = EP0READ(0x3012) & 0x7fffffff; // error counter read
//     printf("i=%d, DDR Error Counter = %08x\n", i, data);
     if(data > 0x0) {
       lanes = EP0READ(0x3013) & 0xffffffff; // error on lanes read
//       printf("i=%d, DDR Lane Errors = %08x\n", i, lanes);
       errDet = true || errDet;
     }
   }
   //}
   
   if(errDet == false) { // no error observed
     return TEST_OK;
   }
   else {
     return TEST_FAIL;
   }

}    

RtnStatus_t ddrWriteTest(void* ptr) {
  // only send writes to DDR interface
   
   EP0WRITE(0x3001,0x0); // Disable VGA
   uint32_t data = EP0READ(0x3001); // 
   printf ("VGA Enable Read back = %08x \n", data);
   EP0FLUSH;

   // continuous write
   uint32_t i=0;
   while(1) {
     i++;
     if(i%50 == 0) {
       printf("Written %d blocks\n",2*i);
       sleep(1);
     }
     vdcWriteTestDdr();
     EP0FLUSH;
   }
   
}

RtnStatus_t i2cBasic (void*) {

    //printf ("Clk Synth Reg Dump after Power Up\n");
    //dumpClkSynthRegs();

    setClkSynthFreq(50);

    uint32_t data = EP0CFGREAD(0x10);
    printf ("Upper MAC Addres is : %08x\n", data);
    data = EP0CFGREAD(0x11);
    printf ("Lower MAC Addres is : %08x\n", data);

    data = EP0CFGREAD(0x18);
    printf ("Peer IP Address : %d.%d.%d.%d\n", 
		(data & 0xff000000)>>24,
		(data & 0xff0000)>>16,
		(data & 0xff00)>>8,
		(data & 0xff)
		);

    return TEST_OK;
}

RtnStatus_t audioBasic (void*) {

  uint32_t rdData;
  uint32_t i;

  if (*boardName == "AttoP2") {
    printf("Initializing audio based on P3 or older hardware\n");
    audioInit();
  } else {
    printf("Initializing audio based on P4 or newer hardware\n");
    audioInitP4();
  }

  // send audio packets 
  EP0WRITE(0x4003, 0x5); // set trig value
  EP0WRITE(0x4000, 0x40000000); // enable mono and enable Tx/Rx
  EP0FLUSH;

  //  if(i==0) { // clears the status in the middle of a song
  //    EP0FLUSH;
  //  }
  //  if(i%10 == 9) { // report status
  //    printf("audio pkt #%d, Audio Status = %x\n",i, EP0READ(0x4004));
  //  }
  //}

  // Clear audio status
  printf("Initial Audio Status = %x, clearing...\n",EP0READ(0x4004));
  EP0WRITE(0x4004, 0x1);
  audioSend(256);
  printf("Audio Status = %x\n",EP0READ(0x4004));

  return TEST_OK;

}

RtnStatus_t audioWolfLpbk(void*) {

  audioInitP4(); 
  printf("Initializing audio loopback...\n");
  audioInitLpbk();
  
  EP0WRITE(0x4003, 0x5); // set trig value
  EP0WRITE(0x4000, 0x80000000); // enable Rx
  EP0FLUSH;

  sleep(60);

  return TEST_OK;

}

RtnStatus_t audioTnpLpbk(void*) {
  // loops audio_in to audio_out(mono) via TNP software in server
  EndpointInput ep(&gStatus, &gDone);
  sStream->setEndpointInput(AudioTnpEndpoint, &ep);

  // rate is self regulated by the audio_in
  // turnaround and resend the audio packet out
  localAudioProc = &doAudioLpbk;

  audioInitP4();

  EP0WRITE(0x4003, 0x9); // set trig value
  EP0WRITE(0x4000, 0xc8000000); // enable mono and enable Tx/Rx
  printf (">>> Audio Sample rate set up 8khz...<<<\n");
  wolfwrite(0x08,0x23);  //sample rate 8khz
  EP0FLUSH;
  
  for(uint32_t i=0;i<(3600*3);i++) {
    sleep(1);
  }

  return TEST_OK;
}

RtnStatus_t audioExtLpbk(void*) {
  // loops audio_out to audio_in external to the client
  EndpointInput ep(&gStatus, &gDone);
  sStream->setEndpointInput(AudioTnpEndpoint, &ep);

  audioInitP4(false); // switchover false
  EP0WRITE(0x4003, 0x9);
  EP0WRITE(0x4000, 0xc8000000);
  printf (">>> Audio Sample rate set up 8khz...<<<\n");
  wolfwrite(0x08,0x27);  //sample rate 8khz
  EP0FLUSH;
  
  printf("Initializing loopback...\n");

  localAudioProc = &analyzeAudioRcv;

  uint32_t iter = 128;
  
  audioSend(iter);
  uint32_t timeout = 100;

  // poll for done for audioRcv
  while (!gDone) {
    usleep(1000);
    timeout --;
  }
  if (!timeout) {
    return TEST_FAIL;
  }

  return gStatus;

}

RtnStatus_t audioInOnly(void*) {
  // turns on audioInput only

  audioInitP4();
  EP0WRITE(0x4003, 0x9);
  EP0WRITE(0x4000, 0x80000000);
  EP0FLUSH;
  
  sleep(5);

  return TEST_OK;

}

RtnStatus_t audioCont (void*) {

  uint32_t rdData;
  uint32_t i;

  if (*boardName == "AttoP2") {
    printf("Initializing audio based on P3 or older hardware\n");
    audioInit();
  } else {
    printf("Initializing audio based on P4 or newer hardware\n");
    audioInitP4();
  }

  // send audio packets 
  while(1) {
    audioSend(128);
  }

  return TEST_OK;

}

RtnStatus_t audioMax (void*) {

  uint32_t rdData;
  uint32_t i;

  if (*boardName == "AttoP2") {
    printf("Initializing audio based on P3 or older hardware\n");
    audioInit();
  } else {
    printf("Initializing audio based on P4 or newer hardware\n");
    audioInitP4();
  }

  // send audio packets 
  while(1) {
    for(i=0;i<256;i++) {
      audioSendMax();
      usleep(20000);
    }
  }


  return TEST_OK;

}

RtnStatus_t audioContInit (void*) {

  uint32_t i;

  while(1) {
    if (*boardName == "AttoP2") {
      printf("Initializing audio based on P3 or older hardware\n");
      audioInit();
    } else {
      printf("Initializing audio based on P4 or newer hardware\n");
      audioInitP4();
    }
    
  }

  return TEST_OK;
}

RtnStatus_t setMacAddr (void* ptr) {

    uint32_t mac_addr = (ptr) ? *static_cast<uint32_t*>(ptr) : 0;
    spiInitAjit(mac_addr,0);
    return TEST_OK;

}


RtnStatus_t setSpiLock (void* ptr) {

    uint32_t spi_addr = 0x37FFF;
    uint32_t testdat = 0x0;
    uint32_t spi_data = 0x0;    

    while(testdat != 0xffffffff){
    spi_addr++;
    testdat = spiRead(spi_addr);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, testdat);
    }
    printf("***SPI WRITING SPI PROTECT LOCK @ %08x***\n",spi_addr);
       spi_data = 0x4 << 28 | // WR OPCODE
        0xFF; // protect register
     spiWrite(spi_addr, spi_data);
     printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

     spi_data = 0x00000001; // Write lock
    spiWrite(++spi_addr, spi_data);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));


    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(++spi_addr));
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(++spi_addr));

    return TEST_OK;

}



void spiSeqCsrWr(uint32_t addr, uint32_t data , uint32_t &spi_addr)
{ 
    uint32_t spi_data;

        spi_data = 0x4 << 28 | // WR OPCODE
            addr; // protect register

        //printf("debug write spi_addr=0x%08x spi_data=0x%08x\n",spi_addr, spi_data);
        spiWrite(spi_addr, spi_data);
        printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

        spi_data = data;
        //printf("debug write spi_addr=0x%08x spi_data=0x%08x\n",++spi_addr, spi_data);
        spiWrite(++spi_addr, spi_data);
        printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(spi_addr));

	  spiRead(spi_addr);
}


RtnStatus_t spiledburn(void* ptr) {


    uint32_t spi_addr = 0x37FFF;
    uint32_t testdat = 0x0;
    uint32_t spi_data = 0x0;    

    while(testdat != 0xffffffff){
    spi_addr++;
    testdat = spiRead(spi_addr);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, testdat);
    }
    printf("***SPI WRITING LED IMAGE DATA @ %08x***\n",spi_addr);
 


    // Code 0 - Startup
    spiSeqCsrWr(0x8080,LED_RGB(0,0, 0,0, 0,0),spi_addr); // Color (OFF)
    spiSeqCsrWr(0x8081,LED_RGB(0,0, 0,0, 0,0),++spi_addr); // Color (ON)
    spiSeqCsrWr(0x8010,0x00000000,++spi_addr); // Pattern
    
    // Code 1 - No Link
    spiSeqCsrWr(0x8082,LED_RGB(0,0, 0,0, 0,0),++spi_addr); // Color (OFF)
    spiSeqCsrWr(0x8083,LED_RGB(7,255, 0,0, 0,0),++spi_addr); // Color (ON)
    spiSeqCsrWr(0x8011,0x00000009,++spi_addr); // Pattern
    
    // Code 2 - No IP
    spiSeqCsrWr(0x8084,LED_RGB(7,99, 7,255, 0,0),++spi_addr); // Color (OFF)
    spiSeqCsrWr(0x8085,LED_RGB(0,0, 0,0, 0,0),++spi_addr); // Color (ON)
    spiSeqCsrWr(0x8012,0x00000009,++spi_addr); // Pattern
    
     // Code 3 - Has IP
    spiSeqCsrWr(0x8086,LED_RGB(7,99, 7,255, 0,0),++spi_addr); // Color (OFF)
    spiSeqCsrWr(0x8087,LED_RGB(0,0, 0,0, 0,0),++spi_addr); // Color (ON)
    spiSeqCsrWr(0x8013,0x00000000,++spi_addr); // Pattern
    
     // Code 4 - Button Pend
    spiSeqCsrWr(0x8088,LED_RGB(0,0, 0,0, 7,255),++spi_addr); // Color (OFF)
    spiSeqCsrWr(0x8089,LED_RGB(0,0, 0,0, 0,0),++spi_addr); // Color (ON)
    spiSeqCsrWr(0x8014,0x00000000,++spi_addr); // Pattern
    
     // Code 5 - Connect
    spiSeqCsrWr(0x808a,LED_RGB(0,0, 0,0, 7,255),++spi_addr); // Color (OFF)
    spiSeqCsrWr(0x808b,LED_RGB(0,0, 0,0, 0,0),++spi_addr); // Color (ON)
    spiSeqCsrWr(0x8015,0x00000000,++spi_addr); // Pattern
    
    // Code 6 - Button Down
    spiSeqCsrWr(0x808c,LED_RGB(7,75, 7,255, 7,75),++spi_addr); // Color (OFF)
    spiSeqCsrWr(0x808d,LED_RGB(0,0, 0,0, 0,0),++spi_addr); // Color (ON)
    spiSeqCsrWr(0x8016,0x00000000,++spi_addr); // Pattern



    spiSeqCsrWr(0x8000,0x00000001,++spi_addr); // Enable


    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(++spi_addr));
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, spiRead(++spi_addr));

    return TEST_OK;


}    


RtnStatus_t spiledbutt(void* ptr) {


    uint32_t spi_addr = 0x37FFF;
    uint32_t testdat = 0x0;
    uint32_t spi_data = 0x0;    

    while(testdat != 0xffffffff){
    spi_addr++;
    testdat = spiRead(spi_addr);
    printf("SPI READ DATA @ %08x: 0x%08x\n",spi_addr, testdat);
    }
    printf("***SPI WRITING LED IMAGE DATA @ %08x***\n",spi_addr);
 
     // Code 6 - Button Down
    spiSeqCsrWr(0x808c,LED_RGB(7,75, 7,255, 7,75),spi_addr); // Color (OFF)
    spiSeqCsrWr(0x808d,LED_RGB(0,0, 0,0, 0,0),++spi_addr); // Color (ON)
    spiSeqCsrWr(0x8016,0x00000000,++spi_addr); // Pattern


    return TEST_OK;


}    












RtnStatus_t spiBasicRd (void*) {

    spiDump();
    return TEST_OK;

}


RtnStatus_t spiPStat (void*) {
    uint32_t i;

  
    for (i = 0; i<5 ; i++)
	{   
	    printf("SPI PROTECT STATUS 0x%08x \n", EP0READ(0x00FF));
	}
    return TEST_OK;
}

RtnStatus_t spiEraseChk (void*) {

    uint32_t spi_addr;
    uint32_t start_addr = 0x3C000;
    uint32_t start_data = 0xABCD0123;
    uint32_t tstdat,spi_data;    
    uint32_t i;    

    //  EP0WRITE(0x00FF, 0x3);
    //  EP0FLUSH;
    //  printf("SPI ERASE PROTECT \n");

   
    printf("SPI PROTECT STATUS 0x%08x \n", EP0READ(0x00FF));
    spi_addr = start_addr;
    spiErase (spi_addr); // ERASE takes 5 secs


    spi_addr = start_addr;
    spi_data = start_data;

    for(i=0;i<5;i++)
	{
	    spiWrite (spi_addr,spi_data);
	    printf("SPI PRE ERASE WRITE DATA @ %08x: 0x%08x\n",spi_addr,spi_data);
	    spi_addr++;
	    spi_data++;
	}

   spi_addr = start_addr;
   spi_data = start_data;
    for(i=0;i<5;i++)
	{
	    tstdat = spiRead(spi_addr);
	    printf("SPI PRE ERASE READ DATA @ %08x: 0x%08x\n",spi_addr,tstdat);
	    if(tstdat != spi_data) 
		{ printf("PRE ERASE READ MISMATCH !!!");
		      return TEST_FAIL;
		}
	    spi_data++;
	    spi_addr++;
	} 
    

    spi_addr = start_addr;
    spiErase (spi_addr); // ERASE takes 5 secs

   spi_addr = start_addr;
    for(i=0;i<5;i++)
	{
	    tstdat = spiRead(spi_addr);
	    printf("SPI POST ERASE READ DATA @ %08x: 0x%08x\n",spi_addr, tstdat);
	    if(tstdat != 0xFFFFFFFF) 
		{ printf("ERASE FAILED!!!");
		return TEST_FAIL;
		}
	    spi_addr++;
	}


    spi_addr = start_addr; 
    spi_data = start_data;
    for(i=0;i<5;i++)
	{
	    spiWrite (spi_addr,spi_data);
	    printf("SPI POST ERASE WRITE DATA @ %08x: 0x%08x\n",spi_addr,spi_data);
	    spi_addr++;
	    spi_data++;
	}

    spi_addr = start_addr;
    spi_data = start_data;
   for(i=0;i<5;i++)
       {   tstdat = spiRead(spi_addr);
	    printf("SPI POST ERASE WRITTEN  READ DATA @ %08x: 0x%08x\n",spi_addr, tstdat);
	    if(tstdat != spi_data) 
		{ printf("POST ERASE READ DATA MISMATCH !!!");
		return TEST_FAIL;
		}
	       spi_addr++;
	       spi_data++;
	}


  return TEST_OK;

}

RtnStatus_t chipIdRd (void*) {

    chipIDrd();
    chipIDrd();
    chipIDrd();
    chipIDrd();
    chipIDrd();
    return TEST_OK;

}

RtnStatus_t i2cEdidTest (void*) {

    return i2cEdidSetup();

}

RtnStatus_t getMacAddr (void*) {

    uint32_t macLsb = EP0CFGREAD(0x11);
    uint32_t macMsb = EP0CFGREAD(0x10);
    printf("MAC ADDRESS = %02x:%02x:%02x:%02x:%02x:%02x\n", 
                (macMsb & 0xff00) >> 8,
                (macMsb & 0xff),
                (macLsb & 0xff000000) >> 24,
                (macLsb & 0xff0000) >> 16,
                (macLsb & 0xff00) >> 8,
                (macLsb & 0xff)
                ); 
    return TEST_OK;

}
RtnStatus_t dhcpStart (void*) {

  uint32_t readData = EP0CFGREAD(0x30);
  printf ("npc_ctrl = %08x\n", readData);

  readData ^= 0x1;
  printf ("Writing new npc_ctrl : %08x\n", readData);
  EP0CFGWRITE(0x30, readData);

  return TEST_OK;

}


RtnStatus_t attoButton (void* args) {

    try {
      
      uint32_t *count;
      *count = 0;
    	
	AttoButtonPressHandler ah(count);
	ep0Client->setEp0Handler(&ah);

	/*
	 * Main processing loop.
	 */
        sleep(15);
        
    }
    catch (std::exception& e) {
	std::cerr << e.what() << std::endl;
	return TEST_FAIL;
    }

    return TEST_OK;

}

RtnStatus_t oobBasic (void* args) {

    uint32_t rdData = EP0OOBCFGREAD(0x0);
    printf ("OOB READ : Board ID = %08x\n", rdData);
    return TEST_OK;

}

RtnStatus_t discover (void* args) {

    using std::string;
    
    string dest = *(string*)(args);

    string tnp_port = dest.substr(dest.find(":"));
    
    //WSADATA wsa;
    //WSAStartup(WINSOCK_VERSION, &wsa);

    try {
	Workpool* workpool = Workpool::instance( 0 );


	TnpDiscover* disc = new TnpDiscover(tnp_port, ":8320");
	
	DiscoveryHandler dh;
	disc->setDiscoveryHandler(&dh);

	disc->discover();

	/*
	 * Main processing loop.
	 */
    sleep(5);
        
    disc->setDiscoveryHandler((TnpDiscover::ClientDiscoveryHandler*)0);
	disc->shutdown();
    }
    catch (std::exception& e) {
	    std::cerr << e.what() << std::endl;
	return TEST_FAIL;
    }


    return TEST_OK;

}

RtnStatus_t resetOutTest(void*) {

  EP0WRITE(0x00f0, 0x0);
  EP0FLUSH;
  printf ("Keeping USB at reset!\n");
  sleep(1);
  EP0WRITE(0x00f0, 0x100);
  printf ("Asserting board reset, and loosing the client...\n");

  return TEST_OK;
}
  
RtnStatus_t randnumgen(void*) {
  
    uint32_t i;
  printf ("Enabling the randnum generator...\n"); 
  EP0WRITE(0x7084, 0x1); // enable the randnum gen
  EP0FLUSH;
  // sleep(1); //  > 1us

  printf(" Check to see randomness below\n");
  for (i=0;i < 100; i++)
      {
  printf("randnum = 0x%08x %08x %08x %08x \n", EP0READ(0x7080),EP0READ(0x7081),EP0READ(0x7082),EP0READ(0x7083));
  EP0FLUSH;
      }

  printf ("Disabling the randnum generator...\n"); 
  EP0WRITE(0x7084, 0x0); // enable the randnum gen
  EP0FLUSH;
  sleep(2); //  > 1us
   printf("randnum = 0x%08x %08x %08x %08x \n", EP0READ(0x7080),EP0READ(0x7081),EP0READ(0x7082),EP0READ(0x7083));
  EP0FLUSH;
  sleep(3); //  > 1us
  printf("randnum = 0x%08x %08x %08x %08x \n", EP0READ(0x7080),EP0READ(0x7081),EP0READ(0x7082),EP0READ(0x7083));
  sleep(2); //  > 1us
  printf("randnum = 0x%08x %08x %08x %08x \n", EP0READ(0x7080),EP0READ(0x7081),EP0READ(0x7082),EP0READ(0x7083));
  EP0FLUSH;
  sleep(1); //  > 1us

  printf ("Enabling the randnum generator...\n"); 
  EP0WRITE(0x7084, 0x1); // enable the randnum gen
  EP0FLUSH;
  sleep(1); //  > 1us

 printf(" Check to see randomness below\n");
  for (i=0;i < 100; i++)
      {
  printf("randnum = 0x%08x %08x %08x %08x \n", EP0READ(0x7080),EP0READ(0x7081),EP0READ(0x7082),EP0READ(0x7083));

  EP0FLUSH;
      }



  printf ("Disabling the randnum generator...\n"); 
  EP0WRITE(0x7084, 0x0); // enable the randnum gen
  EP0FLUSH;
  sleep(1); //  > 1us




  
  return TEST_OK;
}

RtnStatus_t audioButtonTest(void*) {

  if (*boardName == "AttoP2") {
    printf("Initializing audio based on P3 or older hardware\n");
    audioInit();
  } else {
    printf("Initializing audio based on P4 or newer hardware\n");
    audioInitP4(false, true);
  }

  EP0WRITE(0x4000, 0x40000000); // enable mono and enable Tx/Rx
  EP0FLUSH;
  
  uint32_t startbut;
  uint32_t buttonval = 0;
  uint32_t buttondiff = 0;

  startbut =  EP0READ(0x00FF);
  buttonval = startbut;
  buttondiff = buttonval - startbut;
  //printf ("current button state 0x%08x \n",startbut);
  
  printf("Before while loop\n");
  while(buttondiff <= 0x0001) {
    audioSend(20);  
    buttonval =  EP0READ(0x00FF);
    printf("Please press the Pano Button\n");
    buttondiff = buttonval - startbut;
    sleep(1);
    printf("After sleep\n");
  }

  printf("Returning test status...\n");
  return TEST_OK;

}

RtnStatus_t buttontest(void*) {

 
    uint32_t buttonval;
    uint32_t startbut;
    uint32_t buttondiff;
    uint32_t loopcnt;
 
  printf ("press da button 3 times...\n");
  startbut =  EP0READ(0x00FF);
  printf ("current button state 0x%08x \n",startbut);
  
  buttonval = startbut;
  buttondiff = buttonval - startbut;
  loopcnt = 0;
  
  while (buttondiff <= 0x0005)
      {
	  buttonval = EP0READ(0x00FF);
	  printf("button state = 0x%08x \r", buttonval);
	  EP0FLUSH;
	  buttondiff = buttonval - startbut;
	  loopcnt++;
	  if(loopcnt > 50000) { return TEST_FAIL;}
	  usleep(250000);
          //sleep(.25); //  > 1us

      }


  return TEST_OK;

}

RtnStatus_t flasholdfpga(void*) {
 
    uint32_t spi_addr = 0x000000;
    uint32_t spi_dat = 0x00000000;
    uint32_t spi_rdat,spi_flushrd;
    uint32_t spi_secerase = 0x000000;
    uint32_t i,errcnt,j;    

    FILE *fin;
    while(spi_secerase < 0x38000)
    { 
	  spiErase(spi_secerase);
	  spi_secerase = spi_secerase + 0x4000;
    }
    printf("Quick Erase Check: ");
    for(i=0;i<14;i++){//14
	spi_dat = spiRead(spi_addr);
	//printf("ERASE CHECK addr =0x%08x data=0x%08x\n",spi_addr,spi_dat);
	if(spi_dat != 0xFFFFFFFF) { printf("ERASE FAILED!!!!\n"); return TEST_FAIL;}
	printf(".");
	spi_addr=spi_addr + 0x4000;
    }
    printf("PASSED\n");
 
    printf("htc.9nimg Programing\n");
    fin = fopen("htc.9nimg","r");
    //   fflush;

    if(fin == NULL) { 
	printf("could not find htc.9nimg \n");
	return TEST_FAIL;
    }
      i = 0;
      //  printf("Found htc.9nimg Writting image \n");
      while(fscanf(fin,"%x %x", &spi_addr,&spi_dat) != EOF)
	 {
	     //  printf("SPI WRITE DAT @ 0x%08x: 0x%08x \n",spi_addr,spi_dat);
	    spiWriteNF(spi_addr, spi_dat);
	    // spi_rdat = spiRead(spi_addr);
            // printf("SPI READ DATA @ 0x%08x: 0x%08x \n",spi_addr, );
		printf("%d \r",i);
		fflush(stdout);
		i++;
                if(i > 50) spi_flushrd = spiRead(spi_addr);
                //if(i > 25) break;
	}
    printf("Image had %d words, last address written 0x%08x\n",i,spi_addr);
    fclose(fin);


    /* reopen file and verify */
    // printf("reopening file to verify image\n");
    fin = fopen("htc.9nimg","r");
    if(fin == NULL) { 
	printf("could not find htc.9nimg \n");
	return TEST_FAIL;
    }

     errcnt = 0;
     i=0;
    printf("reading flash & verifing \n");
     while(fscanf(fin,"%x %x", &spi_addr,&spi_dat) != EOF)
	 {
	    spi_rdat = spiRead(spi_addr);
	    if(spi_rdat != spi_dat) 
		{
		printf("SPI DATA MISMATCH @ addr 0x%08x rddat=0x%08x fildat=0x%08x \n",spi_addr,spi_rdat,spi_dat);
		errcnt++;
		if(errcnt >= 1) {return TEST_FAIL;}
		}
		printf("%d \r",i); 
		i++;
	 }
     printf("Image Verified\n");
    fclose(fin);
    printf("Done Programming - power cycle to take effect\n");

    // SUCCESS: BLINK GREEN-BLUE  
    //EP0WRITE(0x8086,LED_RGB(7,99, 7,255, 0,0)); // Color (OFF)
    EP0WRITE(0x8086,LED_RGB(0,0, 0,0, 7,255)); // Color (OFF)
    EP0WRITE(0x8087,LED_RGB(0,0, 7,255, 0,0)); // Color (ON)
    EP0WRITE(0x8013,0x0000000C); // Pattern

  return TEST_OK;
}

RtnStatus_t flashfpga(void*) {
    uint32_t Rev      = EP0CFGREAD(1);

    // FPGA's older than 3.13 need the slower flash code
    if(Rev < 0x0003000D) {
        printf("Detected FPGA with old SPI flash controller.");
        return flasholdfpga(NULL); 
    }
 
    uint32_t spi_addr = 0x000000;
    uint32_t spi_dat = 0x00000000;
    uint32_t spi_rdat,spi_flushrd;
    uint32_t spi_secerase = 0x000000;
    uint32_t i,errcnt,j;    

    EP0WRITE(0xfc, 1);
    FILE *fin;
    while(spi_secerase < 0x38000)
    { 
	  spiErase(spi_secerase);
	  spi_secerase = spi_secerase + 0x4000;
    }
    printf("Quick Erase Check: ");
    for(i=0;i<14;i++){
	spi_dat = spiRead(spi_addr);
	if(spi_dat != 0xFFFFFFFF) { printf("ERASE FAILED!!!!\n"); return TEST_FAIL;}
	printf(".");
	spi_addr=spi_addr + 0x4000;
    }
    printf("PASSED\n");
 
    printf("htc.9nimg Programing\n");
    fin = fopen("htc.9nimg","r");

    if(fin == NULL) { 
	printf("could not find htc.9nimg \n");
	return TEST_FAIL;
    }

    i = 0;
    while(fscanf(fin,"%x %x", &spi_addr, &spi_dat) != EOF)
    {
	    spiWrite(spi_addr, spi_dat);
        printf("%d \r",i); 
        i++;
        if (i % 64 == 0) {
            usleep(10000);
        }
	}
    printf("Write Done\n");
    fclose(fin);

    printf("Image had %d words, last address written 0x%08x\n",i,spi_addr);
    EP0WRITE(0xfc, 0);
    EP0FLUSH;


    /* reopen file and verify */
    // printf("reopening file to verify image\n");
    fin = fopen("htc.9nimg","r");
    if(fin == NULL) { 
	printf("could not find htc.9nimg \n");
	return TEST_FAIL;
    }

     errcnt = 0;
     i=0;
     printf("reading flash & verifing \n");
     while(fscanf(fin,"%x %x", &spi_addr,&spi_dat) != EOF)
	 {
	    spi_rdat = spiRead(spi_addr);
	    if(spi_rdat != spi_dat) 
		{
		printf("SPI DATA MISMATCH @ addr 0x%08x rddat=0x%08x fildat=0x%08x \n",spi_addr,spi_rdat,spi_dat);
		errcnt++;
		if(errcnt >= 1) {return TEST_FAIL;}
		}
		printf("%d \r",i); 
		i++;
	 }
     printf("Image Verified\n");
    fclose(fin);
    printf("Done Programming - power cycle to take effect\n");

    // SUCCESS: BLINK GREEN-BLUE  
    //EP0WRITE(0x8086,LED_RGB(7,99, 7,255, 0,0)); // Color (OFF)
    EP0WRITE(0x8086,LED_RGB(0,0, 0,0, 7,255)); // Color (OFF)
    EP0WRITE(0x8087,LED_RGB(0,0, 7,255, 0,0)); // Color (ON)
    EP0WRITE(0x8013,0x0000000C); // Pattern

  return TEST_OK;
}


RtnStatus_t fpgaverify(void*) {
 
    uint32_t spi_addr = 0x000000;
    uint32_t spi_dat = 0x00000000;
    uint32_t spi_rdat;
    uint32_t spi_secerase = 0x000000;
    uint32_t i,errcnt,j;    

    FILE *fin;
    printf("opening file\n");
    fin = fopen("htc.9nimg","r");
    if(fin == NULL) { 
	printf("could not find htc.9nimg \n");
	return TEST_FAIL;
    }

     errcnt = 0;
     i=0;
     while(fscanf(fin,"%x %x", &spi_addr,&spi_dat) != EOF)
	 {
	    spi_rdat = spiRead(spi_addr);
	    if(spi_rdat != spi_dat) {
		printf("SPI DATA MISMATCH @ addr 0x%08x rddat=0x%08x fildat=0x%08x \n",spi_addr,spi_rdat,spi_dat);
		errcnt++;
		if(errcnt > 15) {return TEST_FAIL;}
	    }
	    else  {
		if(i>= 512) { i = 0;  j++; 
		printf("%d \r",j);
		fflush(stdout);
		}  //384
		i++;
	    }
	}
    fclose(fin);

  return TEST_OK;

}
    




void addTestToBringUpList ( const char * testname, ptrTest testFunction) {
    if (!BringupTests) { 
        printf ("You should initialize the BringupTests Map before trying to add any tests! \n");
        return;
    }

    BringupTests->insert(pair<string,ptrTest>(string(testname),testFunction));

}

int main( uint32_t argc, char * argv[])
{
    boardName = new string("unknown");
    BringupTests = new TestList();
    sStream = new TnpStreamSocket();

    addTestToBringUpList ( "ddrDbg3Test",&ddrDbg3Test);
    addTestToBringUpList ( "ddrDbg2Test",&ddrDbg2Test);
    addTestToBringUpList ( "ddrDbgTest",&ddrDbgTest);
    addTestToBringUpList ( "avgAccessTimeTest",&avgAccessTimeTest);
    addTestToBringUpList ( "ddrDbgWritePattern",&ddrDbgWritePattern);
    addTestToBringUpList ( "ddrWriteTest",&ddrWriteTest);
    addTestToBringUpList ( "ddrImgStressTest",&ddrImgStressTest);
    addTestToBringUpList ( "testDualMonitor",&testDualMonitor);
    //addTestToBringUpList ( "epLoopbackTest",&epLoopbackTest);
    addTestToBringUpList ( "usbBasic",&usbBasic);
    addTestToBringUpList ( "usbMultiWord",&usbMultiWord);
    addTestToBringUpList ( "usbMfgTest",&usbMfgTest);
    addTestToBringUpList ( "usbMfgTest1",&usbMfgTest1);
    addTestToBringUpList ( "usbMfgTest2",&usbMfgTest2);
    addTestToBringUpList ( "usbMfgTest3",&usbMfgTest3);
    addTestToBringUpList ( "usbDevBasic",&usbDevBasic);
    addTestToBringUpList ( "usbIntrBasic",&usbIntrBasic);
    addTestToBringUpList ( "i2cBasic",&i2cBasic);
    addTestToBringUpList ( "i2cEdidTest",&i2cEdidTest);
    addTestToBringUpList ( "setMacAddr",&setMacAddr);
    addTestToBringUpList ( "setSpiLock",&setSpiLock);
    addTestToBringUpList ( "spiBasicRd",&spiBasicRd);
    addTestToBringUpList ( "spiEraseChk",&spiEraseChk);
    addTestToBringUpList ( "getMacAddr",&getMacAddr);
    addTestToBringUpList ( "audioBasic",&audioBasic);
    addTestToBringUpList ( "audioCont",&audioCont);
    addTestToBringUpList ( "audioMax",&audioMax);
    addTestToBringUpList ( "audioContInit",&audioContInit);
    addTestToBringUpList ( "audioWolfLpbk",&audioWolfLpbk);
    addTestToBringUpList ( "audioTnpLpbk",&audioTnpLpbk);
    addTestToBringUpList ( "audioExtLpbk",&audioExtLpbk);
    addTestToBringUpList ( "audioInOnly",&audioInOnly);
    addTestToBringUpList ( "vgaBasic",&vgaBasic);
    addTestToBringUpList ( "vgaTestDdr",&vgaTestDdr);
    addTestToBringUpList ( "vgaCrcTest",&vgaCrcTest);
    addTestToBringUpList ( "vgaScanout",&vgaScanout);
    addTestToBringUpList ( "vdcMemBlitTest",&vdcMemBlitTest);
    addTestToBringUpList ( "vdcFillRectTest",&vdcFillRectTest);
    addTestToBringUpList ( "vdcCursorBlitTest",&vdcCursorBlitTest);
    addTestToBringUpList ( "vdcCheckerBoardTest",&vdcCheckerBoardTest);
    addTestToBringUpList ( "vdc15Test",&vdc15Test);
    addTestToBringUpList ( "vgaBorderTest",&vgaBorderTest);
    addTestToBringUpList ( "vgaPatternTest",&vgaPatternTest);
    addTestToBringUpList ( "vgaProjectorTest",&vgaProjectorTest);
    addTestToBringUpList ( "dhcpStart",&dhcpStart);
    addTestToBringUpList ( "discover",&discover);
    addTestToBringUpList ( "attoButton",&attoButton);
    addTestToBringUpList ( "oobBasic",&oobBasic);
    addTestToBringUpList ( "resetOutTest",&resetOutTest);
    addTestToBringUpList ( "randnumgen",&randnumgen);
    addTestToBringUpList ( "chipIdRd",&chipIdRd);
    addTestToBringUpList ( "flashfpga",&flashfpga);
    addTestToBringUpList ( "fpgaverify",&fpgaverify);
    addTestToBringUpList ( "spiPStat",&spiPStat);
    addTestToBringUpList ( "ledTest",&ledTest);
    addTestToBringUpList ( "ledGreenBlue",&ledGreenBlue);
    addTestToBringUpList ( "ledRed",&ledRed);
    addTestToBringUpList ( "ledAmber",&ledAmber);
    addTestToBringUpList ( "ledGreen",&ledGreen);
    addTestToBringUpList ( "ledBlue",&ledBlue);
    addTestToBringUpList ( "spiledburn",&spiledburn);
    addTestToBringUpList ( "buttontest",&buttontest);
    addTestToBringUpList ( "audioButtonTest",&audioButtonTest);
    addTestToBringUpList ( "spiUserErase",&spiUserErase);
    addTestToBringUpList ( "spiledbutt", &spiledbutt);
   
   
    ptrTest ptrTestToRun;
    bool norun = false;
    bool noconnect = false;
    bool debugModeAll = false;
    uint32_t macAddr = 0; // Bottom 3 bytes of mac address
    string testname;

    string dest(argc > 1 ? argv[1] : "");

    // Args processing
    if (argc <= 1) {
        printf ("Usage is : %s ip_addr --test=testname \n", argv[0]);
        return 0;
    } else if (argc == 2) {
        printf ("Usage is : %s ip_addr --test=testname \n", argv[0]);
        printf ("Total Tests available = %d\n", BringupTests->size());
        if (BringupTests->size()) {
            printf ("List of Tests available : \n");
            TestList::iterator it = BringupTests->begin();
            while (it != BringupTests->end()) {
                printf ("%s \n", it->first.c_str());
                it++;
            }
        }
        return 0;
    } else {
        uint32_t i=2;
        while (i<argc) {
            if (!strncmp(argv[i],"--test=",7)) {
                string tmp_string = string(argv[i]);
                testname = tmp_string.substr(7);

                // Check if the testname is part of the bringup tests
                TestList::iterator it;
                if ((it = BringupTests->find(testname)) != BringupTests->end()) {
                    printf ("Running Test = %s\n", testname.c_str());
                    ptrTestToRun = it->second;
                } else {
                    printf ("[Error] : Can't Find requested test (%s) \n", testname.c_str());
                    printf ("[Error] : Rerun without --test and you'll get the full list \n");
                    return 1;
                }

                if (testname == "discover") {
                    noconnect = true;
                }

            } else if (!strncmp(argv[i],"--macAddr=",10)) {
                string tmp_string = string(argv[i]);
                string macAddrStr = tmp_string.substr(10);
                if (macAddrStr.find("0x") != string::npos) {
                    tmp_string 	= macAddrStr.substr(2);
                    macAddr 	= axtoi(tmp_string.c_str());
                } else {
                    macAddr = atoi(macAddrStr.c_str());
                }
                printf ("macAddr = 00-1C-02-%02x-%02x-%02x \n", (macAddr & 0x00ff0000) >> 16, (macAddr & 0x0000ff00) >> 8, macAddr & 0x000000ff);
            } else if (!strncmp(argv[i],"--noconnect",11)) {
                noconnect = true;
            } else if (!strncmp(argv[i],"--norun",7)) {
                norun = true;
            } else if (!strncmp(argv[i],"--debug_all",11)) {
                debugModeAll = true;
            } else if (!strncmp(argv[i],"--debug_usb",11)) {
                debugModeUsb = true;
            } else {
                printf ("unrecognized Argument (%s) \n", argv[i]);
                return 1;
            }

            // Keep parsing the args
            i++;
        }

    }

    // Check first if this is just a "norun"
    if (norun) {
        printf ("Not running any test -- Finish\n");
        return 0;
    }

    if (debugModeAll) {
        printf ("Enabling Debug Mode \n");
        //sStream->setDebugLevel(TNP_DEBUG_ALL);
        debugModeUsb = true;
    }
    
    if (dest.find(':') == string::npos)
	dest += ":8321";

    if (!noconnect) {
        // Connect to client (note hardcoded port number)
        try {
        sStream->connect(dest);
        //sDgram->connect(dest);
        } catch (...){return 1;}

        // 'Connected' here means that we established a pairing not
        // necessarily that we have connection oriented traffic
        printf ("Client connected : IP Addr = %s\n", dest.c_str());

        ep0Client   = new Ep0Client(sStream);
        //ep0ClientOob= new Ep0Client(sDgram);
        ispDrv      = new ISP1760Driver(sStream);
	//        videoDrv    = new VideoDisplayDriver(sStream, ep0Client,0,0);

//         // couple of test reads
         uint32_t boardId  = EP0CFGREAD(0);
         printf("READ CFG reg 0: 0x%08x\n", boardId);
         if (boardId == 0x54447020) {
             *boardName = string("AttoP1");
             printf ("TESTING with board Type = %0s\n", boardName->c_str());
         } else if (boardId == 0x54435032 || boardId == 0x00020001) {
             *boardName = string("AttoP2");
             printf ("TESTING with board Type = %0s\n", boardName->c_str());
         } else if (boardId == 0x00040001 ) {
             *boardName = string("AttoP4");
             printf ("TESTING with board Type = %0s\n", boardName->c_str());
         } else if (boardId == 0x00040100) {
             *boardName = string("AttoP5");
             printf ("TESTING with board Type = %0s\n", boardName->c_str());
         } else if (boardId == 0x00040200 ) {
             *boardName = string("AttoP6");
             printf ("TESTING with board Type = %0s\n", boardName->c_str());
         } else if (boardId == 0x00040400 ) {
             *boardName = string("AttoP7");
             printf ("TESTING with board Type = %0s\n", boardName->c_str());
         } else if (boardId == 0x00040401 ) {
             *boardName = string("AttoP8");
             printf ("TESTING with board Type = %0s\n", boardName->c_str());
         }   else if (boardId == 0xffffffff ) {
             *boardName = string("Un-Programmed");
             printf ("Board SPI Flash has not been programmed yet\n");
         } else {
             printf ("This is an UNKOWN board, please verify the FPGA integrity\n");
         }

	 // Choon added this or else the pano button related test
	 // will cause next test to fail.
	 // Disable watchdog timer
	 EP0CFGWRITE(0x27, 0x0);
	 
         // Comment out the access to the Rev Register since it's not yet implemeneted
         uint32_t Rev      = EP0CFGREAD(1);
         printf ("FPGA Major Rev = %04x, Minor Rev = %04x \n", (Rev & 0xffff0000) >> 16, Rev & 0xffff);

        // init the rest of the chip
	 ddrInit(boardName->c_str());
    }

    // No Tests are looking at this right now so let's not bother making it
    // test dependant
    void * args = 0;
    
    if (testname.find("setMacAddr") != string::npos) {
        args = (void*)(&macAddr);
    } else if (testname.find("discover") != string::npos) {
        args = (void*)(&dest);
    }
    
    // Now Run the test that we identified earlier
    RtnStatus_t test_status = TEST_OK;
    try {
        test_status = (*(ptrTestToRun))(args);
    } catch (std::exception& e){
        cout << "Exception occured : " << e.what() << std::endl;
        test_status = TEST_FAIL;
    }

    uint32_t rtn_val = 0;
    
    if (test_status == TEST_OK) {
        printf ("Test has PASSED \n");
    } else {
        ledIndicateFail();
        printf ("Test has FAILED \n");
        rtn_val = 1;
    }


    //printf("READ CSR VDC enable reg: 0x%08x\n", EP0READ(0x1001));
    if (!noconnect) {
        audioDisconnect();
        FLUSH;
        sStream->close();
        //sDgram->close();
        printf("Disconnected\n");
    }

    sStream->setEndpointInput(AudioTnpEndpoint, 0);
    //delete sDgram;
    
    delete ep0Client;
    //delete ep0ClientOob;
    delete ispDrv;
    delete videoDrv;

    delete sStream;
    delete BringupTests;
    delete boardName;
    return rtn_val;
}










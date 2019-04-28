//  -------------------------------------------------
// Modified by Skip Hansen for Pano Logic G2
//
// This code orginally from ZX-UNO project http://zxuno.speccy.org/
// http://svn.zxuno.com/svn/zxuno/cores
//
// License from website:
// 
// What is the license? Creative Commons Share Alike, that briefly means: 
// 
// You are free to:
// Share — copy and redistribute the material in any medium or format 
// Adapt — remix, transform, and build upon the material 
// for any purpose, even commercially. 
// The licensor cannot revoke these freedoms as long as you follow the 
// license terms. 
// 
// Under the following terms:
// 
// Attribution — You must give appropriate credit, provide a link to the 
// license, and indicate if changes were made.  You may do so in any 
// reasonable manner, but not in any way that suggests the licensor endorses 
// you or your use.  
// 
// ShareAlike — If you remix, transform, or build upon the material, you must 
// distribute your contributions under the same license as the original.  
// 
// No additional restrictions — You may not apply legal terms or 
// technological measures that legally restrict others from doing anything 
// the license permits.  
// 
// Notices: You do not have to comply with the license for elements of the 
// material in the public domain or where your use is permitted by an 
// applicable exception or limitation.  
// 
// No warranties are given.  The license may not give you all of the 
// permissions necessary for your intended use.  For example, other rights 
// such as publicity, privacy, or moral rights may limit how you use the 
// material.  
// ------------------------------------------------- 


//  Pano Logic G2 info
//
//  NB: Pano uses *WORD* addressing so addresses mined from the executable
//  must be multiplied by 4 to get the byte address
//
//  Golden bitstream address (both): 0x40000 (i.e. word address 0x10000)
//  Golden bitstream size LX150: 0x10ffff
//  Golden bitstream size LX100: 0x0cffff 
//
//  Multiboot bitstream address LX150: 0x480000
//  Multiboot bitstream size LX150:    0x10ffff
//
//  Multiboot bitstream address LX100: 0x380000
//  Multiboot bitstream size LX100:    0xcffff
//
//  Original Pano Logic LX150 multi boot header:
//  00000000 ffff ffff dummy, dummy
//  01000000 6655 99aa sync word1, sync word2
//  02000000 ffff e131 0xffff -> CWDT (set watchdog to maximum)
//  03000000 0000 6132 0x0000 -> GENERAL1 (multi boot start address 15:0)
//  04000000 4803 8132 0x0348 -> GENERAL2 (opcode and multi boot start address 23:16)
//  05000000 0000 a132 0x0000 -> GENERAL3 (fallback start address 15:0) 
//  06000000 0403 c132 0x0304 -> GENERAL4 (opcode and fallback start address 23:16)
//  07000000 0000 e132 0x0000 -> GENERAL5 (user defined scratch register)
//  08000000 0000 a130 0x0000 -> CMD (Null command)
//  09000000 0021 0133 0x2100 -> MODE_REG (bit stream, by 1 SPI, bootmode: 0)
//  0a000000 1f00 0132 0x001f -> HC_OPT_REG (do not skip initialization & reserved)
//  0b000000 0e00 a130 IPROG -> CMD (type 1 write 1 word)
//  0c000000 0020 0020 nop, nop
//  0d000000 0020 0020 nop, nop
//  ffffffff ffff ffff 

module multiboot (
    input wire clk_icap,   // WARNING: this clock must not be greater than 20MHz (50ns period)
    input wire boot
    );
    
    reg [4:0] q = 5'b00000;
    reg reboot_ff = 1'b0;
    always @(posedge clk_icap) begin
      q[0] <= boot;
      q[1] <= q[0];
      q[2] <= q[1];
      q[3] <= q[2];
      q[4] <= q[3];
      reboot_ff <= (q[4] && (!q[3]) && (!q[2]) && (!q[1]) );
    end

    multiboot_spartan6 hacer_multiboot (
        .CLK(clk_icap),
        .MBT_RESET(1'b0),
        .MBT_REBOOT(reboot_ff),
        .spi_addr(24'h40000)    // golden image
    );
endmodule            
    
module multiboot_spartan6 (
    input wire CLK,
    input wire MBT_RESET,
    input wire MBT_REBOOT,
    input wire [23:0] spi_addr
  );

reg  [15:0] icap_din;
reg         icap_ce;
reg         icap_wr;

reg  [15:0] ff_icap_din_reversed;
reg         ff_icap_ce;
reg         ff_icap_wr;


  ICAP_SPARTAN6 ICAP_SPARTAN6_inst (
  
    .CE        (ff_icap_ce),   // Clock enable input
    .CLK       (CLK),         // Clock input
    .I         (ff_icap_din_reversed),  // 16-bit data input
    .WRITE     (ff_icap_wr)    // Write input
  );


//  -------------------------------------------------
//  --  State Machine for ICAP_SPARTAN6 MultiBoot  --
//  -------------------------------------------------

parameter         IDLE     = 0, 
                  SYNC_H   = 1, 
                  SYNC_L   = 2, 
                  
                  CWD_H    = 3,  
                  CWD_L    = 4,  
                                 
                  GEN1_H   = 5,  
                  GEN1_L   = 6,  
                                 
                  GEN2_H   = 7,  
                  GEN2_L   = 8,  
                                 
                  GEN3_H   = 9,  
                  GEN3_L   = 10, 
                                 
                  GEN4_H   = 11, 
                  GEN4_L   = 12, 
                                 
                  GEN5_H   = 13, 
                  GEN5_L   = 14, 
                                 
                  NUL_H    = 15, 
                  NUL_L    = 16, 
                                 
                  MOD_H    = 17, 
                  MOD_L    = 18, 
                                 
                  HCO_H    = 19, 
                  HCO_L    = 20, 
                                 
                  RBT_H    = 21, 
                  RBT_L    = 22, 
                  
                  NOOP_0   = 23, 
                  NOOP_1   = 24,
                  NOOP_2   = 25,
                  NOOP_3   = 26;
                  
                   
reg [4:0]     state;
reg [4:0]     next_state;


always @*
   begin: COMB

      case (state)
      
         IDLE:
            begin
               if (MBT_REBOOT)
                  begin
                     next_state  = SYNC_H;
                     icap_ce     = 0;
                     icap_wr     = 0;
                     icap_din    = 16'hAA99;  // Sync word 1 
                  end
               else
                  begin
                     next_state  = IDLE;
                     icap_ce     = 1;
                     icap_wr     = 1;
                     icap_din    = 16'hFFFF;  // Null 
                  end
            end
            
         SYNC_H:
            begin
               next_state  = SYNC_L;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h5566;    // Sync word 2
            end

         SYNC_L:
            begin
               next_state  = NUL_H;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h30A1;    //  Write to Command Register....
            end

        NUL_H:
            begin
              // next_state  = NUL_L;
           next_state  = GEN1_H;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h0000;   //  Null Command issued....  value = 0x0000
            end

//Q

         GEN1_H:
            begin
               next_state  = GEN1_L;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h3261;    //  Escritura a reg GENERAL_1 (bit boot en caliente)
            end

        GEN1_L:
            begin
               next_state  = GEN2_H;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = spi_addr[15:0]; //16'hC000;   //  dreccion SPI BAJA
            end

         GEN2_H:
            begin
               next_state  = GEN2_L;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h3281;   //  Escritura a reg GENERAL_2
            end

        GEN2_L:
            begin
               next_state  = MOD_H;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = {3'h6B, spi_addr[23:16]}; //  03 lectura SPI opcode + direccion SPI ALTA (03 = 1x, 6B = 4x)
            end
    
/////// Registro MODE (para carga a 4x tras reboot)

        MOD_H:
            begin
               next_state  = MOD_L;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h3301;   //  Escritura a reg MODE
            end

        MOD_L:
            begin
               next_state  = NUL_L;
               icap_ce     = 0;
               icap_wr     = 0;
//             icap_din    = 16'h3100; // Activamos bit de lectura a modo 4x en el proceso de Config
               icap_din    = 16'h2100; // MODE_REG (bit stream, by 1 SPI, bootmode: 0)
            end             
/////

        NUL_L:
            begin
               next_state  = RBT_H;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h30A1;    //  Write to Command Register....
            end

        RBT_H:
            begin
               next_state  = RBT_L;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h000E;    // REBOOT Command 0x000E
            end

//--------------------

        RBT_L:
            begin
               next_state  = NOOP_0;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h2000;    //  NOOP
            end

        NOOP_0:
            begin
               next_state  = NOOP_1;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h2000;    // NOOP
            end

        NOOP_1:
            begin
               next_state  = NOOP_2;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h2000;    // NOOP
            end

        NOOP_2:
            begin
               next_state  = NOOP_3;
               icap_ce     = 0;
               icap_wr     = 0;
               icap_din    = 16'h2000;    // NOOP
            end

//--------------------

        NOOP_3:
            begin
               next_state  = IDLE;
               icap_ce     = 1;
               icap_wr     = 1;
               icap_din    = 16'h1111;    // NULL value
            end
          
        default:
            begin
               next_state  = IDLE;
               icap_ce     = 1;
               icap_wr     = 1;
               icap_din    = 16'h1111;    //  16'h1111"
            end

      endcase
   end

always @(posedge CLK)

   begin:   SEQ
      if (MBT_RESET)
         state <= IDLE;
      else
         state <= next_state;
   end


always @(posedge CLK)

   begin:   ICAP_FF
   
        ff_icap_din_reversed[0]  <= icap_din[7];   //need to reverse bits to ICAP module since D0 bit is read first
        ff_icap_din_reversed[1]  <= icap_din[6]; 
        ff_icap_din_reversed[2]  <= icap_din[5]; 
        ff_icap_din_reversed[3]  <= icap_din[4]; 
        ff_icap_din_reversed[4]  <= icap_din[3]; 
        ff_icap_din_reversed[5]  <= icap_din[2]; 
        ff_icap_din_reversed[6]  <= icap_din[1]; 
        ff_icap_din_reversed[7]  <= icap_din[0]; 
        ff_icap_din_reversed[8]  <= icap_din[15];
        ff_icap_din_reversed[9]  <= icap_din[14];
        ff_icap_din_reversed[10] <= icap_din[13];
        ff_icap_din_reversed[11] <= icap_din[12];
        ff_icap_din_reversed[12] <= icap_din[11];
        ff_icap_din_reversed[13] <= icap_din[10];
        ff_icap_din_reversed[14] <= icap_din[9]; 
        ff_icap_din_reversed[15] <= icap_din[8]; 
        
        ff_icap_ce  <= icap_ce;
        ff_icap_wr  <= icap_wr;
   end  
        
        
endmodule

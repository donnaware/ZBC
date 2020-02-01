//------------------------------------------------------------------------------
// Library for an SST25V DataFlash                             
//
// init_STFlash() - Initializes the pins that control the flash device. This must  
//                  be called before any other flash function is used.             
//
// BYTE STFlash_getByte() - Gets a byte of data from the flash device                     
//                          Use after calling STFlash_startContinuousRead()              
//
// void STFlash_getBytes(a, n) - Read n bytes and store in array a                              
//                               Use after calling STFlash_startContinuousRead()              
//
// void STFlash_readBuffer(b, i, a, n) - Read n bytes from buffer b at index i
//                                       and store in array a
//
// BYTE STFlash_readStatus() - Return the status of the flash device:  
//                             Rdy/Busy Comp 0101XX
//
// void STFlash_writeToBuffer(b, i, a, n) - Write n bytes from array a to 
//                                          buffer b at index i
//
// void STFlash_eraseBlock(b) - Erase all bytes in block b to 0xFF. A block is 256.    
// 
// The main program may define FLASH_SELECT, FLASH_CLOCK,   
// FLASH_DI, and FLASH_DO to override the defaults below.  
//                                      
//                       Pin Layout                         
//   ---------------------------------------------------    
//   |    __                                           | 
//   | 1: CS    FLASH_SELECT   | 8: VCC  +2.7V - +3.6V | 
//   |                         |    ____               |  
//   | 2: SO   FLASH_DO        | 7: HOLD  Hold         |   
//   |    ___                  |                       |    
//   | 3: WP    Write Protect  | 6: SCK   FLASH_CLOCK  |              
//   |                         |    __                 |              
//   | 4: Vss   Ground         | 5: SI     FLASH_DI    |              
//   ---------------------------------------------------              
//                                                                    
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//     * * * USER CONFIGURATION Section, set these per Hardware set up * * *
//------------------------------------------------------------------------------
// FLASH_SIZE   4,194,304  bytes 
//------------------------------------------------------------------------------
#define SCLKDELAY    2  // Delay in cycles
#define SELSDELAY    1  // Delay in us

//------------------------------------------------------------------------------
// Purpose:       Initialize the pins that control the flash device.
//                This must be called before any other flash function is used.
// Inputs:        None
// Outputs:       None
// Dependencies:  None
//------------------------------------------------------------------------------
void Init_STFlash(void)
{
    Set_Tris_B(TRISB_Master);     // Flash Disabled, turn on output pins
    Set_Tris_C(TRISC_Master);     // Set up for PIC being Master 
    output_high(FLASH_SELECT);    // FLASH_SELECT high
    output_low(FLASH_CLOCK);     // Clock High
}

//------------------------------------------------------------------------------
// Purpose:       Disables Flash Functions to yeild to FPGA by tri-stating 
//                FLASH_SELECT Line
// Inputs:        None
// Outputs:       None
// Dependencies:  None
//------------------------------------------------------------------------------
void Disable_STFlash(void)
{
    Set_Tris_C(TRISC_Disable);     // Set up for PIC being Master 
    Set_Tris_B(TRISB_Disable);     // Flash Disabled, output pins Tristated
}

//------------------------------------------------------------------------------
// Purpose:       Select the flash device
// Inputs:        None
// Outputs:       None
//------------------------------------------------------------------------------
void chip_select(void)
{
   output_low(FLASH_CLOCK);
   output_low(FLASH_SELECT);            // Enable select line
   delay_us(SELSDELAY);                 // Settling time
}

//------------------------------------------------------------------------------
// Purpose:       Deselect the flash device
// Inputs:        None
// Outputs:       None
//------------------------------------------------------------------------------
void chip_deselect(void)
{
   output_high(FLASH_SELECT);           // Disable select line
   output_low(FLASH_CLOCK);            // Pulse the clock
   delay_us(SELSDELAY);                 // Settling time
}

//------------------------------------------------------------------------------
// Purpose:       Send data Byte to the flash device
// Inputs:        1 byte of data
// Outputs:       None
//------------------------------------------------------------------------------
void STFlash_SendByte(int8 data)
{
    int8 i;
    for(i=0; i<8; i++) {
        output_bit(FLASH_DI, shift_left(&data,1,0));    // Send a data bit
        output_high(FLASH_CLOCK);                       // Pulse the clock
        delay_cycles(SCLKDELAY);                        // Same as a NOP
        output_low(FLASH_CLOCK);
        delay_cycles(SCLKDELAY);                        // Same as a NOP
   }
}

//------------------------------------------------------------------------------
// Purpose:       Receive data Byte from the flash device
// Inputs:        None
// Outputs:       1 byte of data
// Dependencies:  Must enter with Clock high (preceded by a send)
//------------------------------------------------------------------------------
int8 STFlash_GetByte(void)
{
   int8 i, flashData;
   for(i=0; i<8; i++) {
       output_high(FLASH_CLOCK);                       // Pulse the clock
       delay_cycles(SCLKDELAY);                        // Same as a NOP
       shift_left(&flashData, 1, input(FLASH_DO));      
       output_low(FLASH_CLOCK);
       delay_cycles(SCLKDELAY);                        // Same as a NOP
   }
   return(flashData);
}

//------------------------------------------------------------------------------
// Purpose:       Get a byte of data from the flash device. This function is
//                meant to be used after STFlash_startContinuousRead() has
//                been called to initiate a continuous read. This function is
//                also used by STFlash_readPage() and STFlash_readBuffer().
// Inputs:        1) A pointer to an array to fill
//                2) The number of bytes of data to read
// Outputs:       None
// Dependencies:  None
//------------------------------------------------------------------------------
void STFlash_getBytes(int8 *data, int16 size)
{
   int16 i;
   int8  j;
   for(i=0; i<size; i++) {
      for(j=0; j<8; j++) {
         output_high(FLASH_CLOCK);
         delay_cycles(SCLKDELAY);                   // Small delay NOP
         shift_left(data+i, 1, input(FLASH_DO));
         output_low(FLASH_CLOCK);
         delay_cycles(SCLKDELAY);                   // Same as a NOP
      }
   }
}

//------------------------------------------------------------------------------
// Purpose:       Return the Read status Register of the flash device
// Inputs:        None            ____
// Outputs:       The Read status
// Dependencies:  STFlash_sendData(), STFlash_getByte()
//------------------------------------------------------------------------------
int8 STFlash_readStatus()
{
   int8 status;
   chip_select();                  // Enable select line
   STFlash_SendByte(0x05);         // Send status command
   status = STFlash_GetByte();     // Get the status
   chip_deselect();                // Disable select line
   return(status);                 // Return the status
}

//------------------------------------------------------------------------------
// Purpose:       Wait until the flash device is ready to accept commands
// Inputs:        None
// Outputs:       None
// Dependencies:  STFlash_sendData()
//------------------------------------------------------------------------------
void STFlash_waitUntilReady(void)
{
   chip_select();                  // Enable select line
   STFlash_sendByte(0x05);         // Send status command
   while(input(FLASH_DO));         // Wait until ready
   STFlash_GetByte();              // Get byte 
   chip_deselect();                // Disable select line
}

//----------------------------------------------------------------------------
// Purpose:       Enable Page Program write
// Inputs:        None.
// Outputs:       None.
// Dependencies:  STFlash_sendData(), STFlash_waitUntilReady()
//----------------------------------------------------------------------------
void STFlash_WriteEnable(void)
{
   chip_select();                  // Enable select line
   STFlash_sendByte(0x06);         // Send opcode
   chip_deselect();                // Disable select line
}

//----------------------------------------------------------------------------
// Purpose:       Disable Page Program write
// Inputs:        None.
// Outputs:       None.
// Dependencies:  STFlash_sendData(), STFlash_waitUntilReady()
//----------------------------------------------------------------------------
void STFlash_WriteDisable(void)
{
   chip_select();                  // Enable select line
   STFlash_sendByte(0x04);         // Send opcode
   chip_deselect();                // Disable select line
}

//------------------------------------------------------------------------------
// Purpose:       Write a byte to the status register of the flash device
// Inputs:        None
// Outputs:       None
// Dependencies:  STFlash_sendData(), STFlash_getByte()
//------------------------------------------------------------------------------
void STFlash_writeStatus(int8 value)
{
   STFlash_WriteEnable();          // Enable b yte writting
   chip_select();                  // Enable select line
   STFlash_sendByte(0x01);         // Send status command
   STFlash_sendByte(value);        // Send status value
   chip_deselect();                // Disable select line
   STFlash_WriteDisable();         // Disable writting
}

//------------------------------------------------------------------------------
// Purpose:       Reads a block of data (usually 256 bytes) from the ST
//                Flash and into a buffer pointed to by int Buffer;
//
// Inputs:        1) Address of block to read from
//                2) A pointer to an array to fill
//                3) The number of bytes of data to read
// Outputs:       None
// Dependencies:  STFlash_sendData(), STFlash_getByte()
//------------------------------------------------------------------------------
void STFlash_ReadBlock(int32 Address, int8 *buffer, int16 size)
{
   chip_select();                         // Enable select line
   STFlash_sendByte(0x03);                // Send opcode
   STFlash_sendByte(Make8(Address, 2));   // Send address 
   STFlash_sendByte(Make8(Address, 1));   // Send address
   STFlash_sendByte(Make8(Address, 0));   // Send address
   STFlash_getBytes(buffer, size);        // Get bytes into buffer
   chip_deselect();                       // Disable select line
}

//------------------------------------------------------------------------------
// Purpose:       Send some bytes of data to the flash device
// Inputs:        1) A pointer to an array of data to send
//                2) The number of bytes to send
// Outputs:       None
// Dependencies:  None
//------------------------------------------------------------------------------
void STFlash_Write1Byte(int32 Address, int8 data)
{
   STFlash_WriteEnable();                 // Write 1 byte to specified address
   chip_select();                         // Enable select line
   STFlash_sendByte(0x02);                // Send Opcode
   STFlash_sendByte(Make8(Address, 2));   // Send Address 
   STFlash_sendByte(Make8(Address, 1));   // Send Address
   STFlash_sendByte(Make8(Address, 0));   // Send Address
   STFlash_SendByte(data);                // Send Data
   chip_deselect();                       // Disable select line
}

//------------------------------------------------------------------------------
// Purpose:       Writes a block of data (usually 256 bytes) to the ST
//                Flash from a buffer pointed to by int Buffer;
//
// Inputs:        1) Address of block to read from
//                2) A pointer to an array to fill
//                3) The number of bytes of data to read
// Outputs:       None
//------------------------------------------------------------------------------
void STFlash_WriteBlock(int32 Address, int8 *buffer, int16 size)
{
    int16 i;
    for(i = 0; i < size; i++) {
       STFlash_Write1Byte(Address+i, buffer[i]);
       delay_us(10);
       while(STFlash_readStatus() & 0x01);
    }
    STFlash_WriteDisable();
}

//------------------------------------------------------------------------------
// Purpose:       Erase a block of data (usually 256 bytes)
//
// Inputs:        1) Address of block to erase
// Outputs:       None
//------------------------------------------------------------------------------
void STFlash_EraseBlock(int32 Address)
{

   STFlash_WriteEnable();                // Enable b yte writting
   chip_select();                        // Enable select line
   STFlash_sendByte(0xD8);               // Send opcode
   STFlash_sendByte(Make8(Address, 2));  // Send address 
   STFlash_sendByte(Make8(Address, 1));  // Send address
   STFlash_sendByte(Make8(Address, 0));  // Send address
   output_high(FLASH_SELECT);            // Disable select line
   chip_deselect();                      // Disable select line
   STFlash_WriteDisable();               // Disable writting
}

//----------------------------------------------------------------------------
//  End .h
//----------------------------------------------------------------------------

# This is the data sheet for my theoretical 8 bit computer

## Bus:
 - Address  (16 Bit)
 - Data     (8 Bit)
 - Control  (8 Bit)

## Clock:
 - Read data rising edge
 - Write data falling edge

## Opcodes:  
0000: nop  
0001: li  [reg] [int8_t]  
0010: la  [reg] [addr]     (loads addr / int16_t into reg and reg+1)  
0011: hlt  
(Uses a bit after the instrctions to determine int16_t and reg) (Can only access first 8 of them up to R6) (Set for reg)  
0100: lb  [reg] [addr] / [Reg] [Reg]  
0101: wb  [reg] [addr] / [Reg] [Reg]  
0110: bz  [reg] [addr] / [Reg] [Reg] (branch if reg zero)  
0111: bc  [addr] / [Reg] [Reg] (branch if carry)  

ALU: 3 bits (1000 - 1111)
 - ADD [reg] [reg] [reg]
 - SUB [reg] [reg] [reg]
 - MULT[reg] [reg] [reg]
 - and [reg] [reg] [reg]
 - or  [reg] [reg] [reg]
 - nor [reg] [reg] [reg]
 - sr  [reg] [reg] [reg]
 - sl  [reg] [reg] [reg]

For each instruction microcode is executed

(Uses memory mapped io (0xFF00-0xFFFF))

Registers: (control sets out for reg index into registers ram array)  
0
R0 
R1
R2
R3
R4
R5
R6
R7
R8
R9
R10
R11
R12
R13
R14

Two's complement
 - Regs store 255 - 0 (Unsigned 8 bit int)

Non general purpose:  
IR  
PC  
MAR

## Memory: 16 bit addressing (65536)
 - Program      : 0x0000 - 0x3FFF
 - Data         : 0x4000 - 0x7FFF
 - Stack        : 0xDFFF - 0x8000
 - Video(64x64) : 0xE000 - 0xEFFF
 - XtraReg      : 0xF000 - 0xFEFF
 - Hardware     : 0xFF00 - 0xFFFF

@INPUTDATA = 0xFF00  
@CONTROL = 0xFF01  
@OUTPUTDATA = 0xFF02  
@STATUS = 0xFF04


EXTRA:

sub r3, r1, r2
bcs label      ; Branch if Carry Set (no borrow → r1 >= r2)
bcc label      ; Branch if Carry Clear (borrow → r1 < r2)

To print string put address in hardware 0XFF02 and 0xFF03 then write 4 to 0xFF01

If i need extra instructions:
 - Rem sl -> can be done with mult and a power of 2

Hadrware IO structured like:
 - INPUT data = 0xFF00 (Only need one byte for input of an int8_t)
 - CONTROL = 0xFF01 (Set to A certain value to either print value or get input)
 - OUTPUT data = 0xFF02 - 0xFF03 (1 for printing char or number, 2 for getting address or bigger number) 
 - STATUS = 0xFF04 (Bit 0: input ready, Bit 1: output ready, Bit 7: error flag)

Able to add at end of op with () but must separate finishing )'s with spaces.
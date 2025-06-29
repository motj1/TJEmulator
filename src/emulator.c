#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#define VERBOSE 1
#define RAMOUT ram[mar]
#define RAMIN ram[mar]
#define INPUTDATA 0xFF00
#define CONTROL 0xFF01
#define OUTPUTDATA 0xFF02
#define STATUS 0xFF04

uint8_t registers[16];
uint16_t pc, mar;
uint8_t mdr, mdrl, mdrh, ir;
uint8_t ALUt_1, ALUt_2, carry;

char *ram;

void runProgram();

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Use: %s [filename]\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");

    ram = (char *)calloc(65536, sizeof(char));

    fread(ram, 65536, 1, fp);
    fclose(fp);

    runProgram();

    return 0;
}

void hardware();
int execute();
void decode();
void fetch();

void runProgram() {
    for (int i = 0; i < 16; i++) registers[i] = 0;
    pc = mar = 0; mdr = ir = mdrl = mdrh = 0;
    ALUt_1 = ALUt_2 = carry = 0;

    do {
        if (VERBOSE) printf("New instruction:\n");
        fetch();
        decode();
    } while (!execute());
}


void fetch() {
    if (VERBOSE) printf("Fetch\n");
    // Micro instructions
    mar = pc;       // T0
    ir = RAMOUT;    // T1
    pc ++;
    if (VERBOSE) printf("\t1Read: %X\n", ir);
}

void decode() {
    if (VERBOSE) printf("Decode\n");
    if (ir == 0b11 || ir == 0) return;
    
    mar = pc;       // T2
    mdr = RAMOUT;   // T3
    pc ++;
    if (VERBOSE) printf("\t2Read: %X\n", mdr);

    if (    (ir & 0b1111) == 0b0010 ||
            ((ir & 0b1111) == 0b0100 ||
            (ir & 0b1111) == 0b0101 ||
            (ir & 0b1111) == 0b0110 ||
            (ir & 0b1111) == 0b0111) && !(ir & 0b10000)) {
        mar = pc;       // T4
        mdrh = RAMOUT;  // T5
        pc ++;
        if (VERBOSE) printf("\t3Read: %X\n", mdrh);
    }
}

int execute() {
    if (VERBOSE) printf("Execute: %X\n", ir);
    switch ((ir & 0b1111)) {
    case 0b0001:    // li
        registers[ir >> 4] = mdr; // reg (ir >> 4) in, mdr out, T6
        break;
    case 0b0010:    // la
        registers[ir >> 4] = mdr; // reg (ir >> 4) in, mdr out, T6
        registers[(ir >> 4) + 1] = mdrh; // reg (ir >> 4 + 1) in, mdrh out, T7
        break;
    case 0b0011:    // hlt
        return 1; // T6 (Halts clock)
    case 0b0100:    // lb
        if ((ir & 0b10000)) {
            mdrl = registers[mdr & 0b1111];   // T6
            mdrh = registers[mdr >> 4];      // T7
        } else {
            mdrl = mdr; // T6
        }
        mar = mdrl + (mdrh << 8); // mar in, mdr, mdrh out, T8
        registers[ir >> 5] = RAMOUT; // reg[ir & 0b11110000] in RAMOUT T9
        break;
    case 0b0101:    // wb
        if ((ir & 0b10000)) {
            mdrl = registers[mdr & 0b1111];   // T6
            mdrh = registers[mdr >> 4];      // T7
        } else {
            mdrl = mdr; // T6
        }
        mar = mdrl + (mdrh << 8); // mar in, mdr, mdrh out, T8
        RAMIN = registers[ir >> 5]; // reg[ir & 0b11110000] out RAMIN T9
        if ((mar & 0XFFFF) == 0xFF01) {
            hardware();
        }
        break;
    case 0b0110:    // bz
        if (registers[ir >> 5] == 0) {
            if ((ir & 0b10000)) {
                mdrl = registers[mdr & 0b1111];   // T6
                mdrh = registers[mdr >> 4];      // T7
            } else {
                mdrl = mdr; // T6
            }
            pc = mdrl + (mdrh << 8); // mar in, mdr, mdrh out, T8
        }
        break;
    case 0b0111:    // bc
        if (carry) {
            if ((ir & 0b10000)) {
                mdrl = registers[mdr & 0b1111];   // T6
                mdrh = registers[mdr >> 4];      // T7
            } else {
                mdrl = mdr; // T6
            }
            pc = mdrl + (mdrh << 8); // mar in, mdr, mdrh out, T8
        }
        break;
    case 0b1000:    // add
        ALUt_1 = registers[mdr & 0b1111];           // T6
        ALUt_2 = registers[(mdr >> 4) & 0b1111];    // T7
        registers[ir >> 4] = ALUt_1 + ALUt_2;       // T8
        break;
    case 0b1001:    // sub
        ALUt_1 = registers[mdr & 0b1111];           // T6
        ALUt_2 = registers[(mdr >> 4) & 0b1111];    // T7
        if (ALUt_1 <= ALUt_2) carry = 1;
        else carry = 0;
        registers[ir >> 4] = ALUt_1 - ALUt_2;       // T8
        break;
    case 0b1010:    // mult
        ALUt_1 = registers[mdr & 0b1111];           // T6
        ALUt_2 = registers[(mdr >> 4) & 0b1111];    // T7
        if ((int)ALUt_1 * (int)ALUt_2 >= 255) carry = 1;
        else carry = 0;
        registers[ir >> 4] = ALUt_1 * ALUt_2;       // T8
        break;
    case 0b1011:    // and
        ALUt_1 = registers[mdr & 0b1111];           // T6
        ALUt_2 = registers[(mdr >> 4) & 0b1111];    // T7
        registers[ir >> 4] = ALUt_1 & ALUt_2;       // T8
        break;
    case 0b1100:    // or
        ALUt_1 = registers[mdr & 0b1111];           // T6
        ALUt_2 = registers[(mdr >> 4) & 0b1111];    // T7
        registers[ir >> 4] = ALUt_1 | ALUt_2;       // T8
        break;
    case 0b1101:    // nor
        ALUt_1 = registers[mdr & 0b1111];           // T6
        ALUt_2 = registers[(mdr >> 4) & 0b1111];    // T7
        registers[ir >> 4] = !(ALUt_1 | ALUt_2);    // T8
        break;
    case 0b1110:    // sr
        ALUt_1 = registers[mdr & 0b1111];           // T6
        ALUt_2 = registers[(mdr >> 4) & 0b1111];    // T7
        if ((ALUt_1 >> ALUt_2) << ALUt_2 != ALUt_1) carry = 1;
        else carry = 0;
        registers[ir >> 4] = ALUt_1 >> ALUt_2;      // T8
        break;
    case 0b1111:    // sl
        ALUt_1 = registers[mdr & 0b1111];           // T6
        ALUt_2 = registers[(mdr >> 4) & 0b1111];    // T7
        if ((ALUt_1 >> ALUt_2) << ALUt_2 != ALUt_1) carry = 1;
        else carry = 0;
        registers[ir >> 4] = ALUt_1 << ALUt_2;      // T8
        break;
    default:
        break;
    }

    registers[0] = 0;

    if (VERBOSE) {
        printf("Data:\t");
        for (int i=0; i < 16; i ++) {
            printf("%X, ", ram[i+pc]);
        } printf("\n");
        printf("\tRegisters = [");
        for (int i=0; i<16; i++) {
            printf("%d, ", registers[i]);
        }
        printf("]\n\tPC = %d, MDR = %d, MAR = %d, ir = %d\n", pc, mdr, mar, ir);
        usleep(1000000);
    }
    return 0;
}

void hardware() {
    switch (ram[CONTROL]) {
    case 1:
        printf("%d", ram[OUTPUTDATA]);
        break;
    case 4:
        printf("%s", &ram[(ram[OUTPUTDATA] + (ram[OUTPUTDATA + 1] << 8))]);
        break;
    case 5:
        scanf("%"SCNd8, &ram[INPUTDATA]);
        break;
    case 11:
        printf("%c", ram[OUTPUTDATA]);
        break;
    }
    // printf("HARDWARE\n");
}
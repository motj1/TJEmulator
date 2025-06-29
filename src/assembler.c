#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#define GETREG(in) \
            ((!strcmp(in, "r14") || !strcmp(in, "H"))? 15 : \
            !strcmp(in, "0")?  0 : \
            !strcmp(in, "r0")? 1 : \
            !strcmp(in, "r1")? 2 : \
            !strcmp(in, "r2")? 3 : \
            !strcmp(in, "r3")? 4 : \
            !strcmp(in, "r4")? 5 : \
            !strcmp(in, "r5")? 6 : \
            !strcmp(in, "r6")? 7 : \
            !strcmp(in, "r7")? 8 : \
            !strcmp(in, "r8")? 9 : \
            !strcmp(in, "r9")? 10: \
            !strcmp(in, "r10")? 11: \
            !strcmp(in, "r11")? 12: \
            !strcmp(in, "r12")? 12: \
            (!strcmp(in, "r13") || !strcmp(in, "L"))? 14: -1)
#define CHECKOP(in) !strcmp(pch, in)
#define VERBOSE 1
#define VERBOSECALC 0
#define ZERO registers[0]

char *data;
int labelcount;
uint16_t textptr, dataptr;

int8_t registers[16];

struct label {
    char *text;
    uint16_t address;
    struct label *next;
};
struct label *head, *tail;

struct label *createLabel(char *text, uint16_t address) {
    struct label *new = (struct label *)malloc(sizeof(struct label));
    
    new->text = text;
    new->next = NULL;
    new->address = address;

    if (VERBOSE) printf("Label %s = %d\n", text, address);

    return new;
}

struct constant {
    char *text;
    uint16_t value;
    struct constant *next;
};
struct constant *consthead, *consttail;

struct constant *createConstant(char *text, char *value) {
    struct constant *new = (struct constant *)malloc(sizeof(struct constant));
    
    new->text = text;
    new->next = NULL;

    if (value[0] == '0' && value[1] == 'x')
        new->value = (int)strtoul(value, NULL, 16);
    else
        new->value = atoi(value);

    if (VERBOSE) printf("New Constant %s = %d from %s\n", text, new->value, value);

    return new;
}

int parseDataLine(char *line);
int parseTextLine(char *line);
void preParse(FILE *fp);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Useage: %s [filename].tj\n", argv[0]);
        return 1;
    }

    data = (char *)calloc(65536, sizeof(char));

    textptr = labelcount = 0; 
    dataptr = 0x4000;
    head = tail = NULL;

    FILE *fp = fopen(argv[1], "r");

    preParse(fp);
    textptr = 0;
    dataptr = 0x4000;

    fclose(fp);

    fp = fopen("/tmp/tmp__tj.tj", "r");

    int segment = 2; // Starts in .text segment
    ssize_t linelen;
    size_t linecap = 0;
    char *line = NULL; // Read in line

    while ((linelen = getline(&line, &linecap, fp)) > 0) {
        char *end = line + strlen(line) - 1;
        if (end[0] == '\n')
            end[0] = 0;

        if (!strcmp(line, ".text:")) {
            segment = 1;
        } else if (!strcmp(line, ".data:")) {
            segment = 0;
        } else {
            if ((segment)? parseTextLine(line) : parseDataLine(line)) {
                fclose(fp);
                return 1;
            }
        }
        line = NULL;
    }

    fclose(fp);

    int check = 0; char *rem = NULL;
    for (char *pointer = argv[1]; *pointer; pointer ++) {
        if (*pointer == '.') {
            check = 1;
            rem = pointer;
        } else if (*pointer == 't' && check == 1) {
            check = 2;
        } else if (*pointer == 'j' && check == 2) {
            *rem = 0;
        } else {
            rem = NULL;
            check = 0;
        }
    }

    fp = fopen(argv[1], "wb");

    fwrite(data, 65536, 1, fp);

    fclose(fp);

    struct label *delete = NULL;
    for (struct label *i = head; i != NULL; ) {
        delete = i;
        i = i->next;
        free(delete);
    }
    free(data);

    return 0;
}


int calculator(char *pch);

int parseTextLine(char *line) {
    uint8_t command = 0, data_line = 0, address_line = 0, tmp;
    int dataflag = 0, addressflag = 0;

    char *pch;
    pch = strtok(line, " ,");

    if (CHECKOP("nop") || CHECKOP("hlt")) { // Type 0
        command = CHECKOP("nop")? 0b0000 : 0b0011;
    } else if ( CHECKOP("add")  || CHECKOP("sub") || 
                CHECKOP("mult") || CHECKOP("and") || 
                CHECKOP("or")   || CHECKOP("nor") || 
                CHECKOP("sr")   || CHECKOP("sl")) {
        command =   CHECKOP("add")?  0b1000 : 
                    CHECKOP("sub")?  0b1001 :
                    CHECKOP("mult")? 0b1010 :
                    CHECKOP("and")?  0b1011 :
                    CHECKOP("or")?   0b1100 :
                    CHECKOP("nor")?  0b1101 :
                    CHECKOP("sr")?   0b1110 : 0b1111;
        
        pch = strtok(NULL, " ,");
        command += GETREG(pch) << 4;

        pch = strtok(NULL, " ,");
        data_line = GETREG(pch);

        pch = strtok(NULL, " ,");
        data_line += GETREG(pch) << 4;

        dataflag = 1;
    } else if (CHECKOP("li")) {
        command = 0b0001;
        pch = strtok(NULL, " ,");
        command += GETREG(pch) << 4;

        pch = strtok(NULL, " ,");
        if (pch[0] == '(') {
            int res = calculator(pch);
            data_line = res & 0b11111111;
        } else {
            struct constant *i;
            for (i = consthead; i != NULL; i = i->next) {
                if (!strcmp(i->text, pch)) {
                    data_line = i->value & 0b11111111;
                    break;
                }
            }
            if (i == NULL) {
                if (pch[0] == '\'' && pch[1] != '\\') {
                    data_line = (uint8_t)pch[1];
                } else if (pch[0] == '\'') {
                    if (pch[2] == '\'') {
                        data_line = (uint8_t)pch[1];
                    } else if (pch[2] == 'n') {
                        data_line = (uint8_t)10;
                    } else {
                        data_line = (uint8_t)pch[2];
                    }
                } else if (pch[0] == '0' && pch[1] == 'x') {
                    data_line = (int)strtoul(pch, NULL, 16);
                } else {
                    data_line = atoi(pch);
                }
            }
        }
        dataflag = 1;
    } else if (CHECKOP("la")) {
        command = 0b0010;
        pch = strtok(NULL, " ,");
        command += GETREG(pch) << 4;

        pch = strtok(NULL, " ,");
        if (pch[0] == '(') {
            int res = calculator(pch);
            data_line = res & 0b11111111;
            address_line = (res >> 8) & 0b11111111;
        } else {
            struct label *j;
            for (j = head; j != NULL; j = j->next) {
                if (!strcmp(pch, j->text)) {
                    data_line = j->address & 0b11111111;
                    address_line = j->address >> 8;
                    break;
                }
            }
            struct constant *i;
            for (i = consthead; i != NULL; i = i->next) {
                if (!strcmp(i->text, pch)) {
                    data_line = i->value & 0b11111111;
                    address_line = i->value >> 8;
                    break;
                }
            }
            if (i == NULL && j == NULL) {
                if (pch[0] == '0' && pch[1] == 'x') {
                    data_line = strtoul(pch, NULL, 16) & 0b11111111;
                    address_line = strtoul(pch, NULL, 16) >> 8;
                } else {
                    data_line = atoi(pch) & 0b11111111;
                    address_line = atoi(pch) >> 8;
                }
            }
        }
        command &= ~(1 << 4);
        addressflag = 1;
    } else if ( CHECKOP("bz") || CHECKOP("bc") ||
                CHECKOP("lb") || CHECKOP("wb")) {
        command =   CHECKOP("bz")? 0b0110 : 
                    CHECKOP("bc")? 0b0111 :
                    CHECKOP("lb")? 0b0100 : 0b0101;
        if (!CHECKOP("bc")) {
            pch = strtok(NULL, " ,");
            if (GETREG(pch) > 7) 
                printf("Register too large for write / jump instruction truncating\n");
            command += GETREG(pch) << 5;
        }
        pch = strtok(NULL, " ,");
        // char *pch2 = strtok(NULL, " ,");
        if (GETREG(pch) != -1) {
            data_line = GETREG(pch);
            pch = strtok(NULL, " ,");
            data_line += GETREG(pch) << 4;
            command |= 1 << 4;
            dataflag = 1;
        } else {
            if (pch[0] == '(') {
                int res = calculator(pch);
                data_line = res & 0b11111111;
                address_line = (res >> 8) & 0b11111111;
            } else {
                struct label *i;
                for (i = head; i != NULL; i = i->next) {
                    if (!strcmp(pch, i->text)) {
                        data_line = i->address & 0b11111111;
                        address_line = i->address >> 8;
                        if (VERBOSE) printf("Matched label %s for %X\n", i->text, i->address);
                        break;
                    }
                }
                struct constant *j;
                for (j = consthead; j != NULL; j = j->next) {
                    if (!strcmp(j->text, pch)) {
                        data_line = j->value & 0b11111111;
                        address_line = j->value >> 8;
                        break;
                    }
                }
                if (i == NULL && j == NULL) {
                    if (pch[0] == '0' && pch[1] == 'x') {
                        data_line = strtoul(pch, NULL, 16) & 0b11111111;
                        address_line = strtoul(pch, NULL, 16) >> 8;
                    } else {
                        data_line = atoi(pch) & 0b11111111;
                        address_line = atoi(pch) >> 8;
                    }
                }
            }
            command &= ~(1 << 4);
            addressflag = 1;
        }
    } else if (pch[strlen(pch)-1] == ':') {
        return 0;
    } else {
        printf("Unknown command \"%s\" skipping ...\n", pch);
    }

    data[textptr ++] = command;
    if (dataflag) {
        data[textptr ++] = data_line;
    } else if (addressflag) {
        data[textptr ++] = data_line;
        data[textptr ++] = address_line;
    }

    return 0;
}

int parseDataLine(char *line) {
    char *pch;
    pch = strtok(line, " ,");

    if (pch[strlen(pch)-1] == ':') {
        pch = strtok(NULL, " ,");
        if (pch == NULL) {
            return 0;
        }
    }

    if (CHECKOP(".ascii") || CHECKOP(".asciiz")) {
        pch = strtok(NULL, "\"");
        int escape = 0;
        for (int i = 0; pch[i] != 0; i ++) {
            if (pch[i] == '\\' && escape != 1) {
                escape = 1;
                continue;
            } else if (escape == 1) {
                if (pch[i] == '\\') {
                    data[dataptr] = pch[i];
                    if (VERBOSE) printf("data[%d] = %c\n", dataptr, data[dataptr]);
                    dataptr ++;
                } else if (pch[i] == 'n') {
                    data[dataptr] = 10;
                    if (VERBOSE) printf("data[%d] = NEWLINE\n", dataptr);
                    dataptr ++;
                }

                escape = 0;
            } else {
                data[dataptr] = pch[i];
                if (VERBOSE) printf("data[%d] = %c\n", dataptr, data[dataptr]);
                dataptr ++;
            }
        }
        if (CHECKOP(".asciiz")) {
            data[dataptr ++] = 0;
        }
    } else if (CHECKOP(".space")) {
        pch = strtok(NULL, " ,");
        dataptr += atoi(pch);
        if (VERBOSE) printf("dataptr += %d\n", atoi(pch));
    } else if (CHECKOP(".byte")) {
        pch = strtok(NULL, " ,");
        uint8_t val;
        if (pch[0] == '0' && pch[1] == 'x') {
            val = (uint8_t)strtoul(pch, NULL, 16);
        } else {
            val = (uint8_t)atoi(pch);
        }
        
        data[dataptr ++] = val;
        if ((pch = strtok(NULL, " ,")) != NULL) {
            if (pch[0] == ':') {
                char *tmp = strtok(NULL, " ,");
                dataptr --;
                for (int i = atoi(tmp); i > 0; i --) {
                    data[dataptr ++] = val;
                }
            } else {
                while ((pch = strtok(NULL, " ,")) != NULL) {
                    if (pch[0] == '0' && pch[1] == 'x') {
                        val = (uint8_t)strtoul(pch, NULL, 16);
                    } else {
                        val = (uint8_t)atoi(pch);
                    }
                    data[dataptr ++] = val;
                }
            }
        }
    } else if (pch[strlen(pch)-1] != ':') {
        printf("Unknown data operator \"%s\" skipping ...\n", pch);
    }

    return 0;
}


int preParseDataLine(char *line);
int preParseTextLine(char *pch);

void preParse(FILE *fp) {
    FILE *interFP = fopen("/tmp/tmp__tj.tj", "w");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    int segment = 2; // Starts in .text segment
    char *start = NULL;

    int iterator = 0;
    while ((linelen = getline(&line, &linecap, fp)) > 0) {
        if (VERBOSE) printf("line got: %s\n", line);
        if (start != NULL) start = (char *)1;
        char prev = ' ';
        for (char *pointer = line; *pointer; pointer ++) {
            if (*pointer == '#' && prev != '\\' && start == NULL) {
                *pointer = 0;
                break;
            } else if (*pointer == '*' && prev == '\\') {
                start = pointer - 1;
                *(pointer - 1) = 0;
            } else if (*pointer == '\\' && prev == '*' && start == (char *)1) {
                start = NULL;
                line = pointer + 1;
            } else if (*pointer == '\\' && prev == '*' && start != NULL) {
                for (int i = 1; pointer[i] != 0; i ++) {
                    *start = pointer[i];
                    start ++;
                }
                start = NULL;
                line = pointer + 1;
            }
            prev = *pointer;
        }
        if (start == (char *)1) {
            line = NULL;
            continue;
        }
        // Remove whitespace
        while(isspace((unsigned char)*line)) line++;
        if (*line == 0) {
            line = NULL;
            continue;
        }
        char *end = line + strlen(line) - 1;
        while(end > line && isspace((unsigned char)*end)) end--;
        end[1] = 0;
        // Make lowercase
        int stringflag = 0;
        for (int i = 0; line[i]; i++) {
            if (line[i] == '"') stringflag ^= 1;
            if (!stringflag) line[i] = tolower(line[i]);
        }
        if (VERBOSE) printf("line manipulated: %s\n", line);

        if (line[0] != '@') {
            fprintf(interFP, "%s\n", line);
        } else {
            line ++;
        }

        if (!strcmp(line, ".text:")) {
            segment = 1;
        } else if (!strcmp(line, ".data:")) {
            segment = 0;
        } else {
            if (segment) {
                char *pch;
                pch = strtok(line, " ,");
                if (segment == 2 && !strcmp(strtok(NULL, " "), "=")) {
                    struct constant *new = createConstant(pch, strtok(NULL, " "));
                    if (consthead == NULL) {
                        consthead = new;
                        consttail = new;
                    } else {
                        consttail->next = new;
                        consttail = consttail->next;
                    }
                    if (VERBOSE) printf("New constant: %s = %d\n", new->text, new->value);
                } else {
                    pch = strtok(line, " ,");
                    preParseTextLine(pch);
                }
            } else {
                preParseDataLine(line);
            }
        }
        if (VERBOSE) printf("line parsed\n");

        line = NULL;
    }

    fclose(interFP);
}

int preParseDataLine(char *line) {
    int dataflag = 0;
    char *pch;
    pch = strtok(line, " ,");
    
    if (pch[strlen(pch)-1] == ':') {
        pch[strlen(pch)-1] = 0;
        struct label *new = createLabel(pch, dataptr);
        if (head == NULL) {
            head = new;
            tail = new;
        } else {
            tail->next = new;
            tail = tail->next;
        }
        if (VERBOSE) printf("New label: %s at %d\n", new->text, new->address);
        pch = strtok(NULL, " ,");
        if (pch == NULL) {
            return 1;
        }
    }
    if (CHECKOP(".ascii")) {
        pch = strtok(NULL, " ,");
        *(pch + strlen(pch) - 1) = 0;
        pch ++;
        dataflag = strlen(pch);
    } else if (CHECKOP(".asciiz")) {
        pch = strtok(NULL, " ,");
        *(pch + strlen(pch) - 1) = 0;
        pch ++;
        dataflag = strlen(pch) + 1;
    } else if (CHECKOP(".space")) {
        pch = strtok(NULL, " ,");
        dataflag = atoi(pch);
    } else if (CHECKOP(".byte")) {
        pch = strtok(NULL, " ,");
        dataflag ++;
        while ((pch = strtok(NULL, " ,")) != NULL) {
            if (pch[0] == ':') {
                char *tmp = strtok(NULL, " ,");
                dataflag = atoi(tmp);
                break;
            } else {
                dataflag ++;
            }
        }
    }

    dataptr += dataflag;
    return 0;
}

int preParseTextLine(char *pch) {
    int dataflag = 0;

    if (pch[strlen(pch)-1] == ':') {
        pch[strlen(pch)-1] = 0;
        struct label *new = createLabel(pch, textptr);
        if (head == NULL) {
            head = new;
            tail = new;
        } else {
            tail->next = new;
            tail = tail->next;
        }
        if (VERBOSE) printf("New label: %s at %d\n", new->text, new->address);
        pch = strtok(NULL, " ,");
        if (pch == NULL) {
            return 1;
        }
    }
    
    if (CHECKOP("nop") || CHECKOP("hlt")) { // Type 0
        dataflag = 1;
    } else if ( CHECKOP("add")  || CHECKOP("sub") || 
                CHECKOP("mult") || CHECKOP("and") || 
                CHECKOP("or")   || CHECKOP("nor") || 
                CHECKOP("sr")   || CHECKOP("sl")  ||
                CHECKOP("li")) {
        dataflag = 2;
    } else if (CHECKOP("la")) {
        dataflag = 3;
    } else if ( CHECKOP("bz") || CHECKOP("bc") ||
                CHECKOP("lb") || CHECKOP("wb")) {
        pch = strtok(NULL, " ,");
        if (strtok(NULL, " ,") == NULL) {
            dataflag = 3;   
        } else {
            dataflag = 2;
        }
    } 

    textptr += dataflag;
    return 0;
}


int calculator(char *pch) { // Will be like (Label + num - ... * Constant / ...)
    if (VERBOSECALC) printf("Calc start\n");
    int result = 0, operator = 0; // 0 = +, 1 = -, 2 = *, 3 = /
    int stopflag = 1, operatorclock = 0;
    // char *pch = *inp;
    pch ++;
    while (pch != NULL && stopflag) {
        if (operatorclock) {
            switch (pch[0]) {
            case '+':
                operator = 0;
                break;
            case '-':
                operator = 1;
                break;
            case '*':
                operator = 2;
                break;
            case '/':
                operator = 3;
                break;
            default:
                operator = 0;
                break;
            }
            operatorclock = 0;
            pch = strtok(NULL, " ");
            if (VERBOSECALC) printf("\tEnd op got %s\n", pch);
            continue;
        } else {
            operatorclock = 1;
        }
        if (pch[0] == '(') {
            int tmp = calculator(pch);
            result = (operator == 1)? result - tmp :
                        (operator == 2)? result * tmp : 
                        (operator == 3)? result / tmp : tmp + result;
            if (VERBOSECALC) printf("\tPCH = %s\n", pch);
            pch = strtok(NULL, " ");
            if (VERBOSECALC) printf("\tEnd call got %s\n", pch);

            if (pch[0] == ')') {
                if (VERBOSECALC) printf("Stopping\n");
                break;
            }

            continue;
        }
        if (pch[strlen(pch) - 1] == ')') {
            if (VERBOSECALC) printf("Stopping\n");
            pch[strlen(pch) - 1] = 0;
            stopflag = 0;
        }
        struct label *i;
        for (i = head; i != NULL; i = i->next) {
            if (!strcmp(pch, i->text)) {
                result = (operator == 1)? result - i->address :
                           (operator == 2)? result * i->address : 
                           (operator == 3)? result / i->address : i->address + result;
                if (VERBOSE) printf("\tMatched label %s for %X\n", i->text, i->address);
                break;
            }
        }
        struct constant *j;
        for (j = consthead; j != NULL; j = j->next) {
            if (!strcmp(j->text, pch)) {
                result = (operator == 1)? result - j->value :
                           (operator == 2)? result * j->value : 
                           (operator == 3)? result / j->value : j->value + result;
                break;
            }
        }
        if (i == NULL && j == NULL) {
            int tmp = 0;
            if (pch[0] == '\'' && pch[1] != '\\') {
                tmp = (uint8_t)pch[1];
            } else if (pch[0] == '\'') {
                if (pch[2] == '\'') {
                    tmp = (uint8_t)pch[1];
                } else if (pch[2] == 'n') {
                    tmp = (uint8_t)10;
                } else {
                    tmp = (uint8_t)pch[2];
                }
            } else if (pch[0] == '0' && pch[1] == 'x') {
                tmp = (int)strtoul(pch, NULL, 16);
            } else {
                tmp = atoi(pch);
            }
            result = (operator == 1)? result - tmp :
                        (operator == 2)? result * tmp : 
                        (operator == 3)? result / tmp : tmp + result;
        }
        if (stopflag) {
            pch = strtok(NULL, " ");
            if (VERBOSECALC) printf("\tGot %s\n", pch);
        }
    }
    if (VERBOSECALC) printf("Calc end\n");
    return result;
}


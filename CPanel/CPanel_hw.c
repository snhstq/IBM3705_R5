/* Copyright (c) 2024, Edwin Freekenhorst

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   HENK STEGEMAN AND EDWIN FREEKENHORST BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
   ---------------------------------------------------------------------------

   3705_panel.c: IBM 3705 Interfaced Operator Panel

   This module emulates several founctions of the 3705 front panel.
   To access the panel connect access port 37050 with a TN3270 emulator

   This module includes an interval timer that tiggers every 100msec a L3 interrupt.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "../I3705/i3705_defs.h"
//#include "i3705_Eregs.h"               /* Exernal regs defs */
#include <ncurses.h>

#define RED_BLACK    1
#define GREEN_BLACK  2
#define YELLOW_BLACK 3
#define WHITE_BLACK  4
#define BLUE_BLACK   5
#define BLACK_RED    6
#define BLACK_GREEN  7
#define BLACK_YELLOW 8
#define BLACK_WHITE  9
#define BLACK_BLACK 10

//extern int32_t msize;
//extern int32_t PC;
//extern int32_t saved_PC;
//extern int32_t opcode;
//extern int32_t Eregs_Out[];
//extern int32_t Eregs_Inp[];
//extern uint8_t  timer_req_L3;
//extern uint8_t  inter_req_L3;
//extern uint8_t  load_prsd;
int32_t msize;
int32_t PC;
int32_t saved_PC;
int32_t opcode;
int32_t Eregs_Out[20];
int32_t Eregs_Inp[20];
uint8_t  timer_req_L3;
uint8_t  inter_req_L3;

// CCU status flags
//extern uint8_t  test_mode;
//extern uint8_t  load_state;
//extern uint8_t  wait_state;
//extern uint8_t  pgm_stop;
uint8_t  test_mode;
uint8_t  load_state;
uint8_t  wait_state;
uint8_t  pgm_stop;

int rc, inp, i;
int32_t hex_sw, rot_sw;

#define DOWN 0
#define UP 1
#define OFF 0
#define ON 1
#define SHMEM_ID "/SHM_PANEL"

int key = KEY_F0;
int rc, len;                           /* Return code from various rtns */
uint8_t wrkbyte;                       /* Work byte */
uint8_t mbyte;
uint8_t ABsw1 = DOWN;                  /* AB switch CA1 direction               */
uint8_t ABsw2 = DOWN;                  /* AB switch CA2 direction               */

uint8_t buf[8192];
uint8_t hexsw[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t  hexswpos = 0;
int8_t   fsswpos  = 1;
char fssw[10][17] = {"TAR&OP REGISTER", "STATUS",
                     "FUNCTION 6", "STORAGE ADDRESS",
                     "FUNCTION 5", "REGISTER ADDRESS",
                     "FUNCTION 4", "FUNCTION 1",
                     "FUNCTION 3", "FUNCTION 2"};


struct CP3705 *cp3705;
int Ireg_bit(int reg, int bit_mask);

int row, col;

extern void wait();

// ******************************************************************
// Function to write coloured text on the front panel at row x, col y
// ******************************************************************

void stringAtXY (int x, int y, char *buf, int colour) {

   wmove(stdscr, x, y);
   attron(COLOR_PAIR(colour));
   //attron(A_BOLD);
   printw("%s",buf);
   attroff(COLOR_PAIR(colour));
   return;
}

// ******************************************************************
// Function to write coloured integer  on the front panel at row x, col y
// ******************************************************************

void integerAtXY (int x, int y, int value, int colour) {

   wmove(stdscr,x,y);
   attron(COLOR_PAIR(colour));
   printw("%d", value);
   attroff(COLOR_PAIR(colour));
   return;
}

// ******************************************************************
// Function to write coloured floating point value on the front panel at row x, col y
// ******************************************************************

void floatAtXY (int x, int y, float value, int colour) {

   wmove(stdscr,x,y);
   attron(COLOR_PAIR(colour));
   printw("%.4f", value);
   attroff(COLOR_PAIR(colour));
   return;
}

// ******************************************************************
// Function to write a nibble on the front panel at row x, col y
// ******************************************************************

void nibbleAtXY (int x, int y, int value, int colour) {

   wmove(stdscr, x, y);
   attron(COLOR_PAIR(colour));
   printw("%01X", value);
   attroff(COLOR_PAIR(colour));
   return;
}

// ******************************************************************
// Function to write a byte on the front panel at row x, col y
// ******************************************************************

void byteAtXY (int x, int y, int value, int colour) {

   wmove(stdscr,x,y);
   attron(COLOR_PAIR(colour));
   printw("%02X", value);
   attroff(COLOR_PAIR(colour));
   return;
}

// **********************************************************
// Function to build the front panel.
// **********************************************************
void FrontPanel() {

    stringAtXY(0, 29, "IBM 3705 Control Panel", RED_BLACK);
    stringAtXY(1,  0, "----------------------------------------", GREEN_BLACK);
    stringAtXY(1, 40, "----------------------------------------", GREEN_BLACK);
    stringAtXY(3, 1,  ".----CCU CHECKS-----.", GREEN_BLACK);
    stringAtXY(3, 24, ".--DISPLAY A--.--DISPLAY B--.", GREEN_BLACK);
    stringAtXY(3, 59, "MEMORY SIZE :", BLUE_BLACK);
    stringAtXY(4, 1,  "|", GREEN_BLACK);
    stringAtXY(4, 3,  "ADAPTER CHECK", BLUE_BLACK);
    stringAtXY(4, 21, "|", GREEN_BLACK);
    stringAtXY(4, 24, "|   |    |    |   |    |    |", GREEN_BLACK);
    stringAtXY(4, 26, " ", BLACK_YELLOW);
    stringAtXY(4, 30, "  ", BLACK_YELLOW);
    stringAtXY(4, 35, "  ", BLACK_YELLOW);
    stringAtXY(4, 40, " ", BLACK_YELLOW);
    stringAtXY(4, 44, "  ", BLACK_YELLOW);
    stringAtXY(4, 49, "  ", BLACK_YELLOW);
    stringAtXY(4, 59, "IPL PHASE   :", BLUE_BLACK);
    stringAtXY(5, 1,  "|", GREEN_BLACK);
    stringAtXY(5, 3,  "I/O CHECK", BLUE_BLACK);
    stringAtXY(5, 21, "|", GREEN_BLACK);
    stringAtXY(5, 24, "'-X----0----1-'-X----0----1-'", GREEN_BLACK);
    stringAtXY(5, 59, "FREE BUFFERS:",BLUE_BLACK);
    stringAtXY(6, 1,  "|", GREEN_BLACK);
    stringAtXY(6, 3,  "ADDRESS EXCEPT", BLUE_BLACK);
    stringAtXY(6, 21, "|", GREEN_BLACK);
    stringAtXY(6, 59, "CYCLCE COUNT:", BLUE_BLACK);
    stringAtXY(7, 1,  "|", GREEN_BLACK);
    stringAtXY(7, 3,  "PROTECT CHECK", BLUE_BLACK);
    stringAtXY(7, 21, "|", GREEN_BLACK);
    stringAtXY(8, 1,  "|", GREEN_BLACK);
    stringAtXY(8, 3,  "INVALID OP",BLUE_BLACK);
    stringAtXY(8, 21, "|", GREEN_BLACK);
    stringAtXY(8, 30, "+ A  B  C  D  E +", GREEN_BLACK);
    stringAtXY(9, 1,  "'-------------------'", GREEN_BLACK);
    stringAtXY(12, 28, "DISPLAY FUNCTION SELECT", BLUE_BLACK);
    stringAtXY(14, 1,  ".-Channel Adapter 1-.", GREEN_BLACK);
    stringAtXY(15, 1,  "| A:", GREEN_BLACK);
    stringAtXY(15, 21, "|", GREEN_BLACK);
    stringAtXY(16, 1,  "| B:", GREEN_BLACK);
    stringAtXY(16, 21, "|", GREEN_BLACK);
    stringAtXY(17, 1,  "|-Channel Adapter 2-|", GREEN_BLACK);
    stringAtXY(18, 1,  "| A:", GREEN_BLACK);
    stringAtXY(18, 21, "|", GREEN_BLACK);
    stringAtXY(19, 1,  "| B:", GREEN_BLACK);
    stringAtXY(19, 21, "|", GREEN_BLACK);
    stringAtXY(20, 1,  "'-------------------'", GREEN_BLACK);
    stringAtXY(22, 1,  "PF1=CA1 A/B  PF2=CA2 A/B  PF3=D/F SELECT PF4=LOAD PF7=INTERRUPT", GREEN_BLACK);
    stringAtXY(22,70,  "HOME=exit", GREEN_BLACK);
    stringAtXY(23, 1,  " ", GREEN_BLACK);
    return;
}

// **********************************************************
// The panel adaptor handling thread starts here...
// **********************************************************
/* Function to be run as a thread always must have the same
   signature: it has one void* parameter and returns void    */

int main(void *arg) {

   int disp_regA, disp_regB;
   int inp_h, inp_l, count, row, col, rc;
   int five_lt = 0x00;
   int fdshmem;

   // get shared memory file descriptor (NOT a file)
   fdshmem = shm_open(SHMEM_ID, O_RDWR, S_IRUSR | S_IWUSR);
   if (fdshmem == -1) {
      printf("PNL: Shared Memory failed to open");
      exit(-1);
   }
   // map shared memory to process address space
    cp3705 = mmap(NULL, sizeof(struct CP3705), PROT_WRITE, MAP_SHARED, fdshmem, 0);
    if (cp3705 == MAP_FAILED) {
      printf("PNL: Mapping failed for Shared Memory\n");
      exit(-1);
   }


    // *******************************************************
    // Build the main screen
    // *******************************************************
    FILE *f = fopen("/dev/tty", "r+");
    SCREEN *pnlwin = newterm(NULL, f, f);
    set_term(pnlwin);
    refresh();
    curs_set(0);

    if (has_colors() == FALSE) {
       endwin();                              // End window
       printf("\nPNL: No colour suipport for your terminal\r");
       exit(-1);
    }       // End if (has_colors)

    start_color();
    init_color(COLOR_YELLOW, 1000, 1000, 0);
    init_color(COLOR_RED, 1000, 0, 0);
    init_color(COLOR_BLUE, 0, 1000, 1000);
    init_color(COLOR_GREEN, 0, 1000, 0);
    init_pair(RED_BLACK, COLOR_RED, COLOR_BLACK);
    init_pair(GREEN_BLACK, COLOR_GREEN, COLOR_BLACK);
    init_pair(YELLOW_BLACK, COLOR_YELLOW, COLOR_BLACK);
    init_pair(WHITE_BLACK, COLOR_WHITE, COLOR_BLACK);
    init_pair(BLUE_BLACK, COLOR_BLUE, COLOR_BLACK);
    init_pair(BLACK_RED, COLOR_BLACK, COLOR_RED);
    init_pair(BLACK_GREEN, COLOR_BLACK, COLOR_GREEN);
    init_pair(BLACK_YELLOW, COLOR_BLACK, COLOR_YELLOW);
    init_pair(BLACK_WHITE, COLOR_BLACK, COLOR_WHITE);
    init_pair(BLACK_BLACK, COLOR_BLACK, COLOR_BLACK);

    noecho();
    keypad(stdscr, TRUE);
    // Display the front panel
    FrontPanel();
    refresh();

    // Init hex switches
    for (int i = 0; i < 5; i++) {
       if (i == hexswpos)
          nibbleAtXY(10, 32+(i*3), hexsw[i], BLACK_WHITE);
       else
          nibbleAtXY(10, 32+(i*3), hexsw[i], WHITE_BLACK);
    }       // End for int i

    // Init Function Select switch
    for (int i = 0; i < 10; i++) {
       if (i%2 == 0) {
           col = 26;
           row = 13 + (i/2);
       } else {
           col = 44;
           row = 13 + ((i-1) / 2);
       } // End if i%2

       if (i == fsswpos)
          stringAtXY(row, col, fssw[i], BLACK_GREEN);
       else
          stringAtXY(row, col, fssw[i], GREEN_BLACK);
    }       // End for int i
    key = KEY_F0;
    while ((key != KEY_HOME) && (key != 0x007E)) {          /* 0x007E needed for putty      */
       /* Show Memory Size */
       integerAtXY(3, 73, cp3705->memsize, BLUE_BLACK); /* Show memory size                 */
       attron(COLOR_PAIR(BLUE_BLACK));
       printw("K");                                          /* Kilobytes...                */
       /* Pick up IPL Phase */
       wrkbyte = ((cp3705->reg72_Out & 0x3000) >> 12);       /* Get IPL Phase...            */
       integerAtXY(4, 73, wrkbyte, BLUE_BLACK);              /* ...and display it           */
       /* Pick up free buffer count */
       integerAtXY(5, 73, cp3705->freebuf, BLUE_BLACK); /* Display free buffer count        */
       /* Pick up cycle counter */
       count = cp3705->reg7A_In & 0x7FFF;                    /* get cycle counter...        */
       integerAtXY(6, 73, count, BLUE_BLACK);                /* ...and display it           */

       /********************************/
       /* check channel adapter states       */
       /********************************/
       /* Channel Adapter 1 */

       if (cp3705->ABswitch[0] == 0) {
             stringAtXY(16, 6, "DISABLED", RED_BLACK);
          if (cp3705->CA1_bus_socket[0] > 0) {
             stringAtXY(15, 6, "ACTIVE  ", BLACK_YELLOW);
          } else {
             stringAtXY(15, 6, "ENABLED ", GREEN_BLACK);
          }
       } else if (cp3705->ABswitch[0] == 1) {
          stringAtXY(15, 6, "DISABLED", RED_BLACK);
          if (cp3705->CA1_bus_socket[1] > 0) {
             stringAtXY(16, 6, "ACTIVE  ", YELLOW_BLACK);
          } else {
             stringAtXY(16, 6, "ENABLED ", GREEN_BLACK);
          }
       } else {
          stringAtXY(15, 6, "DISABLED", RED_BLACK);
          stringAtXY(16, 6, "DISABLED", RED_BLACK);
       }

       /* Channel Adapter 2 */

       if (cp3705->ABswitch[1] == 0) {
             stringAtXY(19, 6, "DISABLED", RED_BLACK);
          if (cp3705->CA2_bus_socket[0] > 0) {
             stringAtXY(18, 6, "ACTIVE  ", BLACK_YELLOW);
          } else {
             stringAtXY(18, 6, "ENABLED ", GREEN_BLACK);
          }
       } else if (cp3705->ABswitch[1] == 1) {
          stringAtXY(18, 6, "DISABLED", RED_BLACK);
          if (cp3705->CA2_bus_socket[1] > 0) {
             stringAtXY(19, 6, "ACTIVE  ", BLACK_YELLOW);
          } else {
             stringAtXY(19, 6, "ENABLED ", GREEN_BLACK);
          }
       } else {
          stringAtXY(18, 6, "DISABLED", RED_BLACK);
          stringAtXY(19, 6, "DISABLED", RED_BLACK);
       }

       if (cp3705->ABswitch[0] == -1)                                    /* if CA disabled.                            */
           stringAtXY(14, 3,       "Channel Adapter 1", GREEN_BLACK);    /* display dim                                */
       else                                                              /* else                                       */
           stringAtXY(14, 3,       "Channel Adapter 1", BLACK_GREEN);    /* Display bright                             */

       if (cp3705->ABswitch[1] == -1)                                    /* if CA disabled.                            */
           stringAtXY(17, 3,       "Channel Adapter 2", GREEN_BLACK);    /* display dim                                */
       else                                                              /* else                                       */
           stringAtXY(17, 3,       "Channel Adapter 2", BLACK_GREEN);    /* Display bright                             */


       /********************************/
       /* Wait for a key to be pressed */
       /********************************/
       key = getch();
       switch (key) {
          case KEY_F(1):
             if (cp3705->ABswitch[0] == 0) cp3705->ABswitch[0] = -1;            /* if switch in upper position, set to centre */
             else if (cp3705->ABswitch[0] == -1) {                             /* if switch in centre position               */
                if (ABsw1 == DOWN) {                                           /* and we cam from upper                      */
                   cp3705->ABswitch[0] = 1;                                    /* move to lower position                     */
                   ABsw1 = UP;                                                 /* Reverse switch sequence                    */
                 } else {                                                      /* if we came from lower position             */
                   cp3705->ABswitch[0] = 0;                                    /* move to upper position                     */
                   ABsw1 = DOWN;                                               /* Reverse switch sequence                    */
                }
             } else {                                                          /* switch in lower position                   */
                cp3705->ABswitch[0] = -1;                                      /* move to centre                             */
             }
             if (cp3705->ABswitch[0] == -1)                                    /* if CA disabled.                            */
                 stringAtXY(14, 3,       "Channel Adapter 1", GREEN_BLACK);    /* display dim                                */
             else                                                              /* else                                       */
                 stringAtXY(14, 3,       "Channel Adapter 1", BLACK_GREEN);    /* Display bright                             */
             refresh();
             break;

          case KEY_F(2):
             if (cp3705->ABswitch[1] == 0) cp3705->ABswitch[1] = -1;           /* if switch in upper position, set to centre */
             else if (cp3705->ABswitch[1] == -1) {                             /* if switch in centre position               */
                if (ABsw2 == DOWN) {                                           /* and we cam from upper                      */
                   cp3705->ABswitch[1] = 1;                                    /* move to lower position                     */
                   ABsw2 = UP;                                                 /* Reverse switch sequence                    */
                 } else {                                                      /* if we came from lower position             */
                   cp3705->ABswitch[1] = 0;                                    /* move to upper position                     */
                   ABsw2 = DOWN;                                               /* Reverse switch sequence                    */
                }
             } else {                                                          /* switch in lower position                   */
                cp3705->ABswitch[1] = -1;                                      /* move to centre                             */
             }
             if (cp3705->ABswitch[1] == -1)                                    /* if CA disabled....                         */
                 stringAtXY(17, 3,       "Channel Adapter 2", GREEN_BLACK);    /* display dim                                */
             else                                                              /* else                                       */
                 stringAtXY(17, 3,       "Channel Adapter 2", BLACK_GREEN);    /* Display bright                             */
             refresh();
             break;

          case KEY_F(3):
             if (fsswpos%2 == 0) {
                col = 26;
                row = 13 + (fsswpos/2);
             } else {
                col = 44;
                row = 13 + ((fsswpos-1) / 2);
             } // End if fsswpos%2
             stringAtXY(row, col, fssw[fsswpos], GREEN_BLACK);
             if (fsswpos%2 == 0) fsswpos=fsswpos - 2;
             else fsswpos = fsswpos + 2;
             if (fsswpos > 9) fsswpos = 8;
             if (fsswpos < 0) fsswpos = 1;
             if (fsswpos%2 == 0) {
                col = 26;
                row = 13 + (fsswpos / 2);
             } else {
                col = 44;
                row = 13 + ((fsswpos-1) / 2);
             } // End if fsswpos%2
             stringAtXY(row, col, fssw[fsswpos], BLACK_GREEN);
             break;

          case KEY_F(4):
             cp3705->pnlkey = LOAD;                                            /* Load Key presst                            */
             while (cp3705->pnlkey != NOKEY)                                     /* Wait until function executed              */
                usleep(10);
             break;

          case KEY_F(7):
             cp3705->pnlkey = INTERRUPT;                                       /* Interrupt Key presst                       */
             while (cp3705->pnlkey != NOKEY)                                     /* Wait until function executed              */
                usleep(10);
             break;

          case KEY_RIGHT:
             nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], WHITE_BLACK);
             if (fsswpos == 5) {
                if (hexswpos == 1) hexswpos = 3;
                else hexswpos = 1;
             } else {
                hexswpos++;
                if (hexswpos > 4) hexswpos = 0;
             }
             nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], BLACK_WHITE);
             break;

          case KEY_LEFT:
             nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], WHITE_BLACK);
             if (fsswpos == 5) {
                if (hexswpos == 3) hexswpos = 1;
                else hexswpos = 3;
             } else {
                hexswpos--;
                if (hexswpos < 0) hexswpos = 4;
             }
             nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], BLACK_WHITE);
             break;

          case KEY_UP:
             hexsw[hexswpos] = (hexsw[hexswpos] + 0x01) & 0x0F;
             if ((fsswpos == 3) && (hexswpos == 0) && (hexsw[hexswpos] > 0x03))
                hexsw[hexswpos]=0x00;
             nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], BLACK_WHITE);
             break;

          case KEY_DOWN:
             hexsw[hexswpos] = (hexsw[hexswpos] - 0x01) & 0x0F;
             if ((fsswpos == 3) && (hexswpos == 0) && (hexsw[hexswpos] > 0x03))
                hexsw[hexswpos]=0x03;
             nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], BLACK_WHITE);
             break;
       }       // End switch (key)

       //****************************************************
       //*             Execute the switch function
       //****************************************************
       switch (fsswpos) {
          case 0: // TAR & OP Register
             cp3705->pnlkey = TAROP;                                             /* TAR&OP Register                            */
             while (cp3705->pnlkey != NOKEY)                                     /* Wait until function executed              */
                usleep(10);
             break;

          case 1: // Status
             cp3705->pnlkey = STATUS;                                            /* STATUS Function                           */
             while (cp3705->pnlkey != NOKEY)                                     /* Wait until function executed              */
                usleep(10);
             /* Display status from Display A en Display B */
             nibbleAtXY(4, 26, (cp3705->reg71_Out >> 16), BLACK_YELLOW);
             byteAtXY(4, 30, ((cp3705->reg71_Out >> 8) & 0x00FF), BLACK_YELLOW);
             byteAtXY(4, 35, (cp3705->reg71_Out             & 0x000FF), BLACK_YELLOW);
             nibbleAtXY(4, 40, (cp3705->reg72_Out >> 16), BLACK_YELLOW);
             byteAtXY(4, 44, ((cp3705->reg72_Out >> 8) & 0x00FF), BLACK_YELLOW);
             byteAtXY(4, 49, (cp3705->reg72_Out             & 0x000FF), BLACK_YELLOW);

             /* Decode display register 72*/
             // Adapter Check
             if (cp3705->reg72_Out & 0x0800)
                stringAtXY(4, 19, " ", BLACK_RED);         // Red light !
             else
                stringAtXY(4, 19, " ", BLACK_BLACK);       // No light
             // In/Out Check
             if (cp3705->reg72_Out & 0x0400)
                stringAtXY(5, 19, " ", BLACK_RED);         // Red light !
             else
                stringAtXY(5, 19, " ", BLACK_BLACK);       // No light
             // Address Exception
             if (cp3705->reg72_Out & 0x0200)
                stringAtXY(6, 19, " ", BLACK_RED);         // Red light !
             else
                stringAtXY(6, 19, " ", BLACK_BLACK);       // No light
             // Protect Check
             if (cp3705->reg72_Out & 0x0100)
                stringAtXY(7, 19, " ", BLACK_RED);         // Red light !
             else
                stringAtXY(7, 19, " ", BLACK_BLACK);       // No light
             // Invalid Operation
             if (cp3705->reg72_Out & 0x0080)
                stringAtXY(8, 19, " ", BLACK_RED);         // Red light !
             else
                stringAtXY(8, 19, " ", BLACK_BLACK);       // No light
             break;

          case 2: // Function Select 6
             cp3705->pnlkey = FUNCTION6;                                            /* FUNCTION6 Function                           */
             while (cp3705->pnlkey != NOKEY)                                     /* Wait until function executed              */
                usleep(10);
             break;

          case 3: // Storage Address
             /* Set Hex Switch A can be eiter 0,       1,  2 or 3. If not, set to 0  */
             if (hexsw[0] > 3) {
                nibbleAtXY(10, 32+(0), hexsw[hexswpos], WHITE_BLACK);
                hexsw[0] = 0;
                nibbleAtXY(10, 32+(0), hexsw[hexswpos], BLACK_WHITE);
             }
             // Display hex switch setting
             wrkbyte=hexsw[0];                                              // Get switch A
             nibbleAtXY(4, 26, wrkbyte, BLACK_YELLOW);                      // Show hex switch A
             wrkbyte=(hexsw[1] << 4) + hexsw[2];                            // Get switch B and C
             nibbleAtXY(4, 30, wrkbyte, BLACK_YELLOW);                      // Show hex switch B and C
             wrkbyte=(hexsw[3] << 4) + hexsw[4];                            // Get switch D and E
             nibbleAtXY(4, 35, wrkbyte, BLACK_YELLOW);                      // Show hex switch D and E
             // Get and Display memeory content
             cp3705->maddr = (hexsw[0] & 0x0003) << 18;
             cp3705->maddr = cp3705->maddr + (((hexsw[1] << 4) + hexsw[2]) << 8);
             cp3705->maddr = cp3705->maddr + ((hexsw[3] << 4) + hexsw[4]);
             // Check if address is within specified memory size
             // If not, display all zero's and switch-on address excpetion light
             // else display memory content
             if (cp3705->maddr > (msize * 1024)-1) {
                stringAtXY(6, 19, " ", BLACK_RED);                          // Address Exception
                nibbleAtXY(4, 40, 0x00, BLACK_YELLOW);                      // Write 0 in byte X
                byteAtXY(4, 44, 0x00, BLACK_YELLOW);                        // Write 00 in byte 0
                byteAtXY(4, 49, 0x00, BLACK_YELLOW);                        // Write 00 in byte 1
             } else {
                cp3705->pnlkey = STORADDR;                                 /* Storage Address Function                           */
                while (cp3705->pnlkey != NOKEY)
                   usleep(10);
                wrkbyte = cp3705->membyte;
                stringAtXY(6, 19, " ", BLACK_BLACK);                        // Make sure Address Exception is off
                nibbleAtXY(4, 40, 0x00, BLACK_YELLOW);                      // Write 0 in byte X
                byteAtXY(4, 44, 0x00, BLACK_YELLOW);                        // Write 00 in byte 0
                byteAtXY(4, 49, wrkbyte, BLACK_YELLOW);                     // Write byte from memory location in byte 1
             }
             break;

          case 4: // Function Select 5
             cp3705->pnlkey = FUNCTION5;                                    /* FUNCTION5 Function                           */
             while (cp3705->pnlkey != NOKEY)                                     /* Wait until function executed              */
                usleep(10);
             break;

          case 5: // Register Address
             /* Hilight Hex switch B and D position */
             stringAtXY(8, 35, "B", BLACK_GREEN);
             stringAtXY(8, 41, "D", BLACK_GREEN);
             if ((hexswpos != 1) && (hexswpos != 3)) {
                nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], WHITE_BLACK);
                hexswpos = 1;
                nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], BLACK_WHITE);
             }
             // Reset previous function select and set current
             stringAtXY(6, 19, " ", BLACK_BLACK);                              // Reset Address Exception (in case it was on)

             // Display hex switch setting
             stringAtXY(4, 26, " ", BLACK_YELLOW);                             // Clear byte X
             wrkbyte=hexsw[1] << 4;
             nibbleAtXY(4, 30, wrkbyte, BLACK_YELLOW);                         // Show hex switch B in bits 0-3
             stringAtXY(4, 31, " ", BLACK_YELLOW);                             // Clear bits 4-7
             wrkbyte=hexsw[3] << 4;
             nibbleAtXY(4, 35, wrkbyte, BLACK_YELLOW);                         // Show hex switch D in bits 0-3
             stringAtXY(4, 36, " ", BLACK_YELLOW);                             // Clear bits 4-7

             cp3705->hexswitch=(hexsw[1] << 4) + hexsw[3];
             cp3705->pnlkey = REGADDR;
              while (cp3705->pnlkey != NOKEY)
                 usleep(10);
             // Display register content
             byteAtXY(4, 44, ((cp3705->regvalue >> 8) & 0x000FF), BLACK_YELLOW);
             byteAtXY(4, 49, (cp3705->regvalue & 0x000FF), BLACK_YELLOW);
             break;

          case 6: // Function Select 4
             // First reset highlited B and D switches
             hexswpos=1;
             nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], WHITE_BLACK);
             hexswpos=3;
             nibbleAtXY(10, 32+(hexswpos*3), hexsw[hexswpos], WHITE_BLACK);
             cp3705->pnlkey = FUNCTION4;                                        /* FUNCTION1 Function                        */
             while (cp3705->pnlkey != NOKEY)                                    /* Wait until function executed              */
                usleep(10);
             break;

          case 7: // Function Select 1
             // Reset highlited B and D switches from previous function
             stringAtXY(8, 35, "B", GREEN_BLACK);
             stringAtXY(8, 41, "D", GREEN_BLACK);
             cp3705->pnlkey = FUNCTION1;                                      /* FUNCTION1 Function                        */
             while (cp3705->pnlkey != NOKEY)                                  /* Wait until function executed              */
                usleep(10);
             break;

          case 8: // Function Select 3
             cp3705->pnlkey = FUNCTION3;                                      /* FUNCTION3 Function                       */
             while (cp3705->pnlkey != NOKEY)                                   /* Wait until function executed            */
                usleep(10);
             break;

          case 9: // Function Select 2
             cp3705->pnlkey = FUNCTION2;                                      /* FUNCTION2 Function                        */
             while (cp3705->pnlkey != NOKEY)                                  /* Wait until function executed              */
                usleep(10);
             break;

       }       // End switch fsspos
    }       // End while key != exit

    endwin();                         /* end window                      */
    usleep(1000);                     /* relax a bit                     */
}


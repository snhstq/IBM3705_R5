/* Copyright (c) 2020, Henk Stegeman and Edwin Freekenhorst

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

   3705_cpu.c: IBM 3705 CCU simulator (18 bit EA)

   cpu         IBM 3705 central processor (with 18 bit Extended Addressin)

   The IBM 3705 Communications Controller is a transmission control unit
   designed to assume many of the line-control and processing functions
   for the teleprocessing subsystem. The IBM 3705 performs all the usual
   functions of a transmission control unit and, in addition, takes over
   many of the capabilities of an access method. In this way, the 3705
   removes much of the control of the teleprocessing subsystem from the CPU.

   The 3705 communications controller consists cf 4 major components:
   - The central control unit (CCU) contains most of the arithmetic and logic
     circuitry necessary for the operation of the 3705.
   - The core memory serves its usual function of providing a storage area for
     both machine instructions and data.
   - The channel adapter (CA) controls the interface between the 3705 and the
     host computer.
   - The communication scanner (CS) serves as the interface between the 3705
     and the communications network.

   Programs in the 3705 can execute at any of 5 priority levels which
   are controlled by hardware.  Levels 1 through 4 are interrupt driven;
   that is, they are entered only on the occurrence of specific hardware
   interrupt conditions.
   Level 1, the highest priority level, is used mainly for handling error
            conditions. It is entered when either a hardware failure or
            programming error occurs.
   Level 2, deals with the communication network, and is entered whenever a
            communication line must be serviced by the software.
   Level 3, is used to handle processing of a less critical nature,
            including communication with the host computer, timer maintenance,
            and operator intevention.
   Level 4, is the lowest level of the supervisor, and is entered only upon
            request of one of the other program levels.
   Level 5, the lowest priority level, is unique in several respects. It is
            not interrupt driven, and is executed only when there are no
            outstanding requests for any of the other program levels.
            It is intended for non-critical background processing, and
            hence is not allowed to execute the privileged instructions
            available to the other 4 levels.

   The 3705 core storage is organized in bytes of 8 bits each; these may
   be grouped into halfwords (2 bytes) and fullwords (4 bytes). Storage
   addressing is by byte; the 1st byte of memory is designated Byte 0,
   and successive bytes are numbered sequentially.

   At each program level, the first general register (register 0) of the
   associated group serves as the instruction address register (IAR).
   It always contains the address of the next machine instruction to be
   executed. It is incremented sequentially as processing proceeds, unless
   it is modified by the executing program.
   When an interrupt occurs, the IAR of the appropriate level is loaded
   with the starting address for that level.

   Each program level has a pair of condition latches known as the C and
   Z latches. These are used to record the results of certain arithmetic,
   comparative, and logical operations.

   The 3705 recognizes 51 machine instructions.  All arithmetic and
   logical operations operate on registers only; the only instructions
   which directly reference storage are of the load and store variety.
   There are a number of branching instructions available, although
   branching may be accomplished by any instruction which modifies regis-
   ter 0 (the-IAR).

        ********      Instruction Set       ********
        Mnem   Description                    Format
        B      Branch                           RT
        BCL    Branch on C Latch                RT
        BZL    Branch on Z Latch                RT
        BB     Branch on Bit                    RT
        BCT    Branch on Count                  RT
        BAL    Branch and Link                  RA
        BALR   Branch and Link Register         RR
        AR     Add Register                     RR
        AHR    Add Halfword Register            RR
        ACR    Add Character Register           RR
        ARI    Add Register Immediate           RI
        SR     Subtract Register                RR
        SHR    Subtract Halfword Register       RR
        SCR    Subtract Character Register      RR
        SRI    Subtract Register Immediate      RI
        IC     Insert Character                 RS
        ICT    Insert Character and Count       RSA
        L      Load                             RS
        LH     Load Halfword                    RS
        LR     Load Register                    RR
        LHR    Load Halfword Register           RR
        LCR    Load Character Register          RR
        LRI    Load Register Immediate          RI
        LA     Load Address                     RA
        LOR    Load with Offset Register        RS
        LHOR   Load Halfword with Offset Reg.   RR
        LCOR   Load Char with Offset Reg.       RR
        ST     Store                            RS
        STH    Store Halfword                   RS
        STC    Store Character                  RS
        STCT   Store Character and Count        RSA
        CR     Compare Register                 RR
        CHR    Compare Halfword Register        RR
        CCR    Compare Character Register       RR
        CRI    Compare Register Immediate       RI
        NR     AND Register                     RR
        NHR    AND Halfword Register            RR
        NCR    AND Character Register           RR
        NRI    AND Register Immediate           RI
        OR     OR Register                      RR
        OHR    OR Halfword Register             RR
        OCR    OR Character Register            RR
        ORI    OR Register Immediate            RI
        XR     Exclusive OR Register            RR
        XHR    Exclusive OR Halfword Reg.       RR
        XRI    Exclusive OR Reg. Immediate      RI
        XCR    Exclusive OR Character Reg.      RR
        TRM    Test Register Under Mask         RI
        IN     Input                            RE
        OUT    Output                           RE
        EXIT   Exit                             EXIT

   Notes:
   Instruction formats:
        RA     Reg Addr
        RE     Reg External I/O reg
        RI     Reg Immediate
        RR     Reg Register
        RS     Reg Storage
        RSA    Reg Storage Address
        RT     Branch
        EXIT   Exit
*/

#include <sched.h>
#include "i3705_defs.h"
#include "i3705_Eregs.h"                                /* Exernal regs defs */
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <sys/syscall.h>

#define UNIT_V_MSIZE (UNIT_V_UF+3)                      /* dummy mask */
#define UNIT_MSIZE   (1 << UNIT_V_MSIZE)

extern void Get_ICW(int abar);                          /* CS2: ICW ===> Inp_Eregs 44, 45, 46, 47 rtn */
extern int8 icw_scf[];                                  /* CS2: sdf */
extern int8 icw_pdf[];                                  /* CS2: pdf */
extern int8 icw_lcd[];                                  /* CS2: lcd */
extern int8 icw_pcf[];                                  /* CS2: pcf */
extern int8 icw_sdf[];                                  /* CS2: sdf */
extern int8 icw_Rflags[];                               /* CS2: Rflags */
extern int8 icw_pdf_reg[];                              /* CS2: pdf is filled or empty state */
extern int8 icw_pcf_nxt[];                              /* CS2: new pdf */
extern int8 shwlib;                                     /* Show LIB Panel */
extern uint16_t Sdbg_reg;                               /* SCANNER debug flags register */
extern uint16_t Adbg_reg;                               /* Channel Adapter debug flags register */
extern struct IO3705*  iobs[MAXCHAN];                   /* IBM 3705 I/O Block pointer array */
extern struct CP3705*  cp3705;                          /* IBM 3705 Control Panel settings and values  */
extern int CAready;                                     /* Channel Adaper code fully initialized       */
extern int abar;                                        /* Attachment Buffer Addr Reg (020-1FF) to CS2 */
extern int abar_int;                                    /* ABAR of line interrupt (020-1FF) from CS2   */
extern pthread_mutex_t icw_lock;                        /* CS2: ICW update lock */
pthread_mutex_t r77_lock;                               /* CA2/CS2: Reg77 update lock */
pthread_mutex_t r7f_lock;                               /* CCU: Reg7F update lock */

uint8 M[MAXMEMSIZE] = { 0 };                            /* Memory 3705 */
int32 msize = 0;                                        /* specifed memory size */

int32 GR[8][4] = { 0x00 };                              /* General Registers Group 0-3 */
int32 opcode;                                           /* Operation Code 16 bits */
int32 opcode0, opcode1;                                 /* OpCode byte0(H) & Byte1(L) */
int8  CL_C[4] = { OFF };                                /* Condition Latches 'C' */
int8  CL_Z[4] = { OFF };                                /* Condition Latches 'Z' */
int32 Eregs_Inp[128] = { 0xEFEF };                      /* External regs X'00 -> X'7F' inp */
int32 Eregs_Out[128] = { 0x0000 };                      /* External regs X'00 -> X'7F' out */

int8  int_lvl_req[1+5]  = {0, OFF, OFF, OFF, OFF, OFF}; /* Requested Program Levels */
int8  int_lvl_ent[1+5]  = {0, OFF, OFF, OFF, OFF, OFF}; /* Entered Program Levels */
int8  int_lvl_mask[1+5] = {0, ON,  ON,  ON,  ON,  ON }; /* Masked Program Levels */

int8  ipl_req_L1 = OFF;                                 /* IPL L1 request flag */
int8  diag_req_L2 = OFF;                                /* Diagnostic L2 (in test mode only) */
int8  svc_req_L2 = OFF;                                 /* SVC L2 request flag */
int8  pci_req_L3 = OFF;                                 /* PCI L3 request flag */
int8  timer_req_L3 = OFF;                               /* Interval timer L3 request flag */
int8  inter_req_L3 = OFF;                               /* Panel interrupt L3 request flag */
int8  pci_req_L4 = OFF;                                 /* PCI L4 request flag */
int8  svc_req_L4 = OFF;                                 /* SVC L4 request flag */
// These flags below belong in chan.c
int8  CA1_DS_req_L3 = OFF;                              /* Chan Adap Data/Status request flag */
int8  CA1_IS_req_L3 = OFF;                              /* Chan Adap Initial Sel request flag */
int8  CA1_NSC_end_seq = OFF;                            /* NSC channel end xfer seq flag */
int8  CA1_NSC_final_seq = OFF;                          /* NSC channel final xfer seq flag */
int8  CA1_NSC_SB_clred = OFF;                           /* NSC status byte cleared flag */

int8  last_P_Ns;                                        /* Last frame Ns received from prim station */
int8  last_S_Ns;                                        /* Last frame Ns send to prim station */
int8  last_lu;

int8  cycle_eight = 0;                                  /* Eight cycle counter */
int8  load_state = OFF;                                 /* Load state flag (IPL loadTest mode flag */
int8  test_mode  = ON;                                  /* Test mode flag */
int8  bypass_CCU_check = OFF;                           /* CCU check bypass */
int8  FET_stor_diag = OFF;                              /* FET storage diagnostics */
int8  wait_state = OFF;                                 /* Wait state flag */
int8  pgm_stop   = OFF;                                 /* Program STOP flag */
int8  OP_reg_chk = OFF;                                 /* Invalid instruction */
int8  adr_ex_chk = OFF;                                 /* Address exception check */
int8  IO_L5_chk  = OFF;                                 /* I/O instruction in L5 */
int8  CS1_L1_chk = OFF;                                 /* Scanner 1 adaptor L1 int req */
int8  CS2_L1_chk = OFF;                                 /* Scanner 2 adaptor L1 int req */
int8  CS3_L1_chk = OFF;                                 /* Scanner 3 adaptor L1 int req */
int8  CS4_L1_chk = OFF;                                 /* Scanner 4 adaptor L1 int req */
int8  IO_par_chk = OFF;                                 /* I/O bus parity check */
int8  rpl        = OFF;                                 /* Remote Program Loader        */
int8  CAdisable  = FALSE;                               /* Disable all CA's             */
int8  load_prsd  = FALSE;                               /* Load button pressed          */
int32 lvl;                                              /* Active Program Level (1...5) */
int32 Grp;                                              /* Active Register Group (0...3) */
int32 PC;                                               /* Program Counter */
int32 LAR;                                              /* Lagging Address Register */
int32 saved_PC;                                         /* Previous (saved) PC */
int32 debug_reg = 0x00;                                 /* Bit flags for debug/trace */
int32 debug_flag = OFF;                                 /* 1 when trace.log open */
FILE  *trace;
int32 cc = 1;
int32 val[4] = { 0x00, 0x00, 0x00, 0x00 };              /* Used for printing mnem */

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_reset (DEVICE *dptr);
t_stat cpu_set_size (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat cpu_boot (int32 unitno, DEVICE *dptr);

int32 RegGrp(int32 level);
int32 GetMem(int32 addr);
int32 PutMem(int32 addr, int32 data);
static char * TimeStamp();
int16 reason;

/* CPU data structures
   cpu_unit     CPU unit descriptor
   cpu_reg      CPU register list
   cpu_dev      CPU device descriptor
   cpu_mod      CPU modifiers list
*/

unsigned short old_crc;
unsigned char crc_data;
UNIT cpu_unit = { UDATA (NULL, UNIT_FIX + UNIT_BINK, MAXMEMSIZE) };

REG cpu_reg[] = {
    { HRDATA (IAR, PC, 20), REG_RO },
    { HRDATA (LAR, LAR,20), REG_RO },
    { HRDATA (LVL, lvl, 8), REG_RO },
    { HRDATA (GRP, Grp, 8), REG_RO },

    /* Interrupt requests flags */
    { FLDATA (IREQ1, int_lvl_req[0], 8) },  // No idea why
    { FLDATA (IREQ2, int_lvl_req[1], 8) },  // 1 => 0
    { FLDATA (IREQ3, int_lvl_req[2], 8) },
    { FLDATA (IREQ4, int_lvl_req[3], 8) },
    { FLDATA (IREQ5, int_lvl_req[4], 8) },

    /* Group 0 registers */
    { HRDATA (GR0G0, GR[0][0], 18) },
    { HRDATA (GR1G0, GR[1][0], 18) },
    { HRDATA (GR2G0, GR[2][0], 18) },
    { HRDATA (GR3G0, GR[3][0], 18) },
    { HRDATA (GR4G0, GR[4][0], 18) },
    { HRDATA (GR5G0, GR[5][0], 18) },
    { HRDATA (GR6G0, GR[6][0], 18) },
    { HRDATA (GR7G0, GR[7][0], 18) },
    { FLDATA (CLCG0, CL_C[0],   8) },
    { FLDATA (CLZG0, CL_Z[0],   8) },

    /* Group 1 registers */
    { HRDATA (GR0G1, GR[0][1], 18) },
    { HRDATA (GR1G1, GR[1][1], 18) },
    { HRDATA (GR2G1, GR[2][1], 18) },
    { HRDATA (GR3G1, GR[3][1], 18) },
    { HRDATA (GR4G1, GR[4][1], 18) },
    { HRDATA (GR5G1, GR[5][1], 18) },
    { HRDATA (GR6G1, GR[6][1], 18) },
    { HRDATA (GR7G1, GR[7][1], 18) },
    { FLDATA (CLCG1, CL_C[1],   8) },
    { FLDATA (CLZG1, CL_Z[1],   8) },

    /* Group 2 registers */
    { HRDATA (GR0G2, GR[0][2], 18) },
    { HRDATA (GR1G2, GR[1][2], 18) },
    { HRDATA (GR2G2, GR[2][2], 18) },
    { HRDATA (GR3G2, GR[3][2], 18) },
    { HRDATA (GR4G2, GR[4][2], 18) },
    { HRDATA (GR5G2, GR[5][2], 18) },
    { HRDATA (GR6G2, GR[6][2], 18) },
    { HRDATA (GR7G2, GR[7][2], 18) },
    { FLDATA (CLCG2, CL_C[2],   8) },
    { FLDATA (CLZG2, CL_Z[2],   8) },

    /* Group 3 registers */
    { HRDATA (GR0G3, GR[0][3], 18) },
    { HRDATA (GR1G3, GR[1][3], 18) },
    { HRDATA (GR2G3, GR[2][3], 18) },
    { HRDATA (GR3G3, GR[3][3], 18) },
    { HRDATA (GR4G3, GR[4][3], 18) },
    { HRDATA (GR5G3, GR[5][3], 18) },
    { HRDATA (GR6G3, GR[6][3], 18) },
    { HRDATA (GR7G3, GR[7][3], 18) },
    { FLDATA (CLCG3, CL_C[3],   8) },
    { FLDATA (CLZG3, CL_Z[3],   8) },

    /* External registers CS2 */
    { HRDATA (CS40I, Eregs_Inp[0x40], 16) },
    { HRDATA (CS41I, Eregs_Inp[0x41], 16) },
    { HRDATA (CS42I, Eregs_Inp[0x42], 16) },
    { HRDATA (CS43I, Eregs_Inp[0x43], 16) },
    { HRDATA (CS44I, Eregs_Inp[0x44], 16) },
    { HRDATA (CS45I, Eregs_Inp[0x45], 16) },
    { HRDATA (CS46I, Eregs_Inp[0x46], 16) },
    { HRDATA (CS47I, Eregs_Inp[0x47], 16) },

    /* External registers CA1 */
    { HRDATA (CA60I, Eregs_Inp[0x60], 16) },
    { HRDATA (CA61I, Eregs_Inp[0x61], 16) },
    { HRDATA (CA62I, Eregs_Inp[0x62], 16) },
    { HRDATA (CA63I, Eregs_Inp[0x63], 16) },
    { HRDATA (CA64I, Eregs_Inp[0x64], 16) },
    { HRDATA (CA65I, Eregs_Inp[0x65], 16) },
    { HRDATA (CA66I, Eregs_Inp[0x66], 16) },
    { HRDATA (CA67I, Eregs_Inp[0x67], 16) },

    /* External registers CCU */
    { HRDATA (CU70I, Eregs_Inp[0x70], 16) },
    { HRDATA (CU71I, Eregs_Inp[0x71], 16) },
    { HRDATA (CU72I, Eregs_Inp[0x72], 16) },
    { HRDATA (CU73I, Eregs_Inp[0x73], 16) },
    { HRDATA (CU74I, Eregs_Inp[0x74], 16) },
    { HRDATA (CU75I, Eregs_Inp[0x75], 16) },
    { HRDATA (CU76I, Eregs_Inp[0x76], 16) },
    { HRDATA (CU77I, Eregs_Inp[0x77], 16) },

    { HRDATA (CU78I, Eregs_Inp[0x78], 16) },
    { HRDATA (CU79I, Eregs_Inp[0x79], 16) },
    { HRDATA (CU7AI, Eregs_Inp[0x7A], 16) },
    { HRDATA (CU7BI, Eregs_Inp[0x7B], 16) },
    { HRDATA (CU7CI, Eregs_Inp[0x7C], 16) },
    { HRDATA (CU7DI, Eregs_Inp[0x7D], 16) },
    { HRDATA (CU7EI, Eregs_Inp[0x7E], 16) },
    { HRDATA (CU7FI, Eregs_Inp[0x7F], 16) },

    /* External registers CS2 */
    { HRDATA (CS40O, Eregs_Out[0x40], 16) },
    { HRDATA (CS41O, Eregs_Out[0x41], 16) },
    { HRDATA (CS42O, Eregs_Out[0x42], 16) },
    { HRDATA (CS43O, Eregs_Out[0x43], 16) },
    { HRDATA (CS44O, Eregs_Out[0x44], 16) },
    { HRDATA (CS45O, Eregs_Out[0x45], 16) },
    { HRDATA (CS46O, Eregs_Out[0x46], 16) },
    { HRDATA (CS47O, Eregs_Out[0x47], 16) },

    /* External registers CA1 */
    { HRDATA (CA60O, Eregs_Out[0x60], 16) },
    { HRDATA (CA61O, Eregs_Out[0x61], 16) },
    { HRDATA (CA62O, Eregs_Out[0x62], 16) },
    { HRDATA (CA63O, Eregs_Out[0x63], 16) },
    { HRDATA (CA64O, Eregs_Out[0x64], 16) },
    { HRDATA (CA65O, Eregs_Out[0x65], 16) },
    { HRDATA (CA66O, Eregs_Out[0x66], 16) },
    { HRDATA (CA67O, Eregs_Out[0x67], 16) },

    /* External registers CCU */
    { HRDATA (CU70O, Eregs_Out[0x70], 16) },
    { HRDATA (CU71O, Eregs_Out[0x71], 16) },
    { HRDATA (CU72O, Eregs_Out[0x72], 16) },
    { HRDATA (CU73O, Eregs_Out[0x73], 16) },
    { HRDATA (CU74O, Eregs_Out[0x74], 16) },
    { HRDATA (CU75O, Eregs_Out[0x75], 16) },
    { HRDATA (CU76O, Eregs_Out[0x76], 16) },
    { HRDATA (CU77O, Eregs_Out[0x77], 16) },

    { HRDATA (CU78O, Eregs_Out[0x78], 16) },
    { HRDATA (CU79O, Eregs_Out[0x79], 16) },
    { HRDATA (CU7AO, Eregs_Out[0x7A], 16) },
    { HRDATA (CU7BO, Eregs_Out[0x7B], 16) },
    { HRDATA (CU7CO, Eregs_Out[0x7C], 16) },
    { HRDATA (CU7DO, Eregs_Out[0x7D], 16) },
    { HRDATA (CU7EO, Eregs_Out[0x7E], 16) },
    { HRDATA (CU7FO, Eregs_Out[0x7F], 16) },

    /* Misc */
    { HRDATA (RPL, rpl, 8) },
    { HRDATA (CADISABLE, CAdisable, 16) },
    { HRDATA (WRU, sim_int_char, 8) },
    { HRDATA (SHWLIB, shwlib, 8) },
    { HRDATA (DEBUG, debug_reg, 16) },
    { HRDATA (DEBUGC, debug_reg, 16) },
    { HRDATA (DEBUGS, Sdbg_reg, 16) },
    { HRDATA (DEBUGA, Adbg_reg, 16) },
    { NULL }
};

MTAB cpu_mod[] = {    // <=== for future 3705 model selection
    { UNIT_MSIZE, 32768,  NULL, "32K",  &cpu_set_size },
    { UNIT_MSIZE, 65536,  NULL, "64K",  &cpu_set_size },
    { UNIT_MSIZE, 98304,  NULL, "96K",  &cpu_set_size },
    { UNIT_MSIZE, 131072, NULL, "128K", &cpu_set_size },
    { UNIT_MSIZE, 163840, NULL, "160K", &cpu_set_size },
    { UNIT_MSIZE, 196608, NULL, "192K", &cpu_set_size },
    { UNIT_MSIZE, 229376, NULL, "224K", &cpu_set_size },
    { UNIT_MSIZE, 262144, NULL, "256K", &cpu_set_size },
    { 0 }
};

DEVICE cpu_dev = {
    "CPU", &cpu_unit, cpu_reg, cpu_mod,
    1, 16, 18, 1, 16, 8,
    &cpu_ex, &cpu_dep, &cpu_reset, &cpu_boot,
    NULL, NULL
};

//********************************************************
// Instruction simulator starts here...
//********************************************************

t_stat sim_instr (void) {

// Assign this thread to a core
// core_id = 1 (CPU), 2 (SCAN), 3 (SDLC)
int core_id = 1;
int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

//if ((core_id > 0) && (core_id <= num_cores)) {
//   cpu_set_t cpuset;
//   CPU_ZERO(&cpuset);
//   CPU_SET(core_id, &cpuset);
//   pthread_t current_thread = pthread_self();
//   pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
//   fprintf(stderr, "\rCPU: Thread assigned to core #%1d.\n", core_id);
//}

// BSC CRC calculation

unsigned short calculateBSCCrcChar (unsigned short crc, unsigned char data_p) {
   unsigned char i;
   unsigned int data;
   for (int i=0, data=(unsigned int)0xff & data_p;
      i < 8;
      i++, data >>= 1)
      {
      if ((crc & 0x0001) ^ (data & 0x0001))
         crc = (crc >> 1) ^ 0xa001;
      else crc >>= 1;
   }
   return crc;
}

int32 i, j, w_byte, addr;
int32 R1fld, R2fld, Rfld;
int32 N1fld, N2fld, Nfld;
int32 Afld, Bfld, Dfld, Efld, Ifld, Mfld, Tfld;

Grp = RegGrp(lvl);
saved_PC = PC;
PC = GR[0][Grp];
reason = 0;

//********************************************************
// Main instruction fetch/decode loop                    *
//********************************************************

//##################### START OF SIMULATOR WHILE LOOP ######################
// Scroll down 1900+ lines to find the end of this while loop

while (reason == 0) {                          /* Loop until halted */
   if (sim_interval <= 0) {                    /* Check clock queue */
      if (reason = sim_process_event())
         break;                                /* Stop simulation   */
   }
   sim_interval = sim_interval - 1;            /* Tick the clock    */

   if (sim_brk_summ && sim_brk_test(PC, SWMASK('E')) ) {  /* Any execution BP ? */
      reason = STOP_IBKPT;                     /* Stop simulation   */
      break;
   }
   if (load_prsd) {                            /* Load button pressed ? */
      cpu_reset(&cpu_dev);                         /* Reset CPU             */
      cpu_boot(0,&cpu_dev);                       /* (re)boot              */
      load_prsd = FALSE;                       /* Reset load button     */
   }

//********************************************************
// Uncomment this code to start CCU execution trace
// after NCP load completion.
//********************************************************
//   Update ----- vvvv
//   if (ipl_req_L1 == OFF) debug_reg = 0x60;
//   if (svc_req_L2 || lvl == 2) {
//      debug_reg = 0x43;
//   } else
//      debug_reg = 0x00;

//   if ((debug_reg == 0x00) && (debug_flag == ON)) {  /* Close log file ? */
//      fclose(trace);
//      debug_flag = OFF;
//   }

//********************************************************
//  Debug trace facility
//********************************************************
   if (debug_flag == OFF) {
      trace = fopen("trace.log", "w");
      time_t tme = time(NULL);
      struct tm *currentTime = localtime(&tme);
      fprintf(trace, "\r     ****** 3705 Executed instructions log file ******\n"
                     "     ******           Date: %02d/%02d/%04d          ******\n"
                     "\r     sim> d debug 01 - trace IAR, mnem, C & Z & lvl\n"
                     "                  02 - trace all enter/leave/wait interrupts\n"
                     "                  04 - trace scanner ext input regs\n"
                     "                  08 - trace channel adap ext input regs\n"
                     "                  10 - trace CCU ext input regs\n"
                     "                  20 - trace PIU's\n"
                     "                  40 - trace ICW PCF\n"
                     "                  80 - trace channel activity\n"
                     "\r        AA55 = Unused external register \n\n",
                      currentTime->tm_mday, currentTime->tm_mon + 1, currentTime->tm_year + 1900);
      debug_flag = ON;
   }

//********************************************************
//  IBM 3705 CCU trace print statements
//********************************************************
   if (wait_state != ON) {
      if (debug_reg & 0x01) {  /* Trace instruction + mnem. */
         fprintf(trace, "%s [%06d] exec IAR=%05X - %04X        ", TimeStamp(), cc++,
            saved_PC, opcode );
         fprint_sym(trace, PC, (uint32 *) val, &cpu_unit, SWMASK('M') );
         fprintf(trace, "\n");
      }
      if (debug_reg & 0x04) {  /* Trace external scanner registers */
         fprintf(trace, "%s         CS2:  %04X  %04X  %04X  %04X   %04X  %04X  %04X  %04X (X'40-47') ",TimeStamp(),
            Eregs_Inp[CMBARIN], NOTUSED, NOTUSED, Eregs_Inp[CMERREG],
            Eregs_Inp[CMICWB0F], Eregs_Inp[CMICWLPS], Eregs_Inp[CMICWDPS], Eregs_Inp[CMICWB32]);
         fprintf(trace, "\n");
      }
      if (debug_reg & 0x08) {  /* Trace external chan adaptor registers */
         fprintf(trace, "%s         CA1:  %04X  %04X  %04X  %04X   %04X  %04X  %04X  %04X (X'60-67') ",TimeStamp(),
            Eregs_Inp[CAISC], Eregs_Inp[CAISD], Eregs_Inp[CASSC], Eregs_Inp[CASSA],
            Eregs_Inp[CASD12], Eregs_Inp[CASD34], Eregs_Inp[CARNSTAT], Eregs_Inp[CAECR]);
         fprintf(trace, "\n");
      }
      if (debug_reg & 0x10) {  /* Trace CCU external registers */
         fprintf(trace, "%s         CCU:  %04X  %04X  %04X  %04X   %04X  %04X  %04X  %04X (X'70-77') ",TimeStamp(),
            Eregs_Inp[SYSSTSZ], Eregs_Inp[SYSADRDT], Eregs_Inp[SYSFNINS], Eregs_Inp[SYSINKEY],
            Eregs_Inp[0x74], Eregs_Inp[0x75], Eregs_Inp[SYSADPG1], Eregs_Inp[SYSADPG2]);
         fprintf(trace, "\n");
         fprintf(trace, "%s         CCU:  %04X  %04X  %04X  %04X   %04X  %04X  %04X  %04X (X'78-7F') ",TimeStamp(),
            NOTUSED, Eregs_Inp[SYSUTILI], Eregs_Inp[SYSCUCI],  Eregs_Inp[SYSBSCRC],
            NOTUSED, Eregs_Inp[SYSMCHK],  Eregs_Inp[SYSCCUG1], Eregs_Inp[SYSCCUG2]);
         fprintf(trace, "\n");
      }
   }

//********************************************************
//  Check for any program level requests ?
//********************************************************

   /* Check for any L1 requests ? */
   if (ipl_req_L1 || OP_reg_chk || IO_par_chk || IO_L5_chk || adr_ex_chk)
      int_lvl_req[1] = ON;                     // Set L1 interrupt request
   else int_lvl_req[1] = OFF;

   /* Check for any L2 requests ? */
   if (diag_req_L2 || svc_req_L2)
      int_lvl_req[2] = ON;                     // Set L2 interrupt request
   else int_lvl_req[2] = OFF;

   /* Check for any L3 requests ? */
   if (inter_req_L3 || timer_req_L3 || pci_req_L3 || CA1_DS_req_L3 || CA1_IS_req_L3)
      int_lvl_req[3] = ON;                     // Set L3 interrupt request
   else int_lvl_req[3] = OFF;

   /* Check for any L4 requests ? */
   if (pci_req_L4 || svc_req_L4)
      int_lvl_req[4] = ON;                     // Set L4 interrupt request
   else int_lvl_req[4] = OFF;

   if (debug_reg & 0x02) {                     // Trace interrupt flags
      if (wait_state != ON) {
         fprintf(trace, "%s >>>  REQ[1-5] = %d %d %d %d %d   ENT[1-5] = %d %d %d %d %d   MSK[1-5] = %d %d %d %d %d\n" , TimeStamp(),
               int_lvl_req[1],  int_lvl_req[2],  int_lvl_req[3],  int_lvl_req[4],  int_lvl_req[5],
               int_lvl_ent[1],  int_lvl_ent[2],  int_lvl_ent[3],  int_lvl_ent[4],  int_lvl_ent[5],
               int_lvl_mask[1], int_lvl_mask[2], int_lvl_mask[3], int_lvl_mask[4], int_lvl_mask[5]);
   }  }

//********************************************************
// Check all 5 program levels for any work...
//********************************************************
   for (int i = 1; i < 6; i++) {               // 1, 2, 3, 4...5
      if (int_lvl_ent[i] == OFF) {             // Lvl already running ? => continue
         if ((int_lvl_req[i] == ON) || (i == 5)) {   // Lvl request pending ? => enter if not masked
            if (int_lvl_mask[i] == OFF) {      // Lvl mask on ? => skip this level
               /* Start higher prio pgm level ! */
               int_lvl_ent[i] = ON;
               lvl = i;                        // Set new pgm level
               Grp = RegGrp(lvl);              // Set new reg group
               if (debug_reg & 0x02) {         // Trace CCU interrupt levels
                  if (lvl == 1)
                     fprintf(trace, "%s >>> Entering lvl=1 -- IPL=%d; OPchk=%d; IOchk=%d; AEchk=%d \n",
                             TimeStamp(),ipl_req_L1, OP_reg_chk, (IO_par_chk + IO_L5_chk), adr_ex_chk);
                  if (lvl == 2)
                     fprintf(trace, "%s >>> Entering lvl=2 -- Diag=%d; SVCL2=%d \n",
                             TimeStamp(),diag_req_L2, svc_req_L2);
                  if (lvl == 3)
                     fprintf(trace, "%s >>> Entering lvl=3 -- Int=%d; Timer=%d; PCIL3=%d; CA1_IS=%d; CA1_D/S=%d \n",
                             TimeStamp(),inter_req_L3, timer_req_L3, pci_req_L3, CA1_IS_req_L3, CA1_DS_req_L3);
                  if (lvl == 4)
                     fprintf(trace, "%s >>> Entering lvl=4 -- PCIL4=%d; SVCL4=%d \n",
                             TimeStamp(),pci_req_L4, svc_req_L4);
                  if (lvl == 5)
                     fprintf(trace, "%s >>> Entering lvl=5 -- MSKL5=0 \n",TimeStamp());
               }
               if (debug_reg & 0x02) {
                  if (lvl == 1)                   // Display CCU interrupt levels
                     printf("%s >>> Entering lvl 1 -- IPL=%d; OPchk=%d; IOchk=%d; AEchk=%d \n\r",
                             TimeStamp(), ipl_req_L1, OP_reg_chk, IO_L5_chk, adr_ex_chk);
                  if (lvl == 2)
                     printf("%s >>> Entering lvl 2 -- Diag=%d; SVCL2=%d \n\r",
                             TimeStamp(), diag_req_L2, svc_req_L2);
                  if (lvl == 3)
                     printf("%s >>> Entering lvl 3 -- Int=%d; Timer=%d; PCIL3=%d; CA1_IS=%d; CA1_D/S=%d \n\r",
                             TimeStamp(), inter_req_L3, timer_req_L3, pci_req_L3, CA1_IS_req_L3, CA1_DS_req_L3);
                  if (lvl == 4)
                     printf("%s >>> Entering lvl 4 -- PCIL4=%d; SVCL4=%d \n\r",
                             TimeStamp(), pci_req_L4, svc_req_L4);
                  if (lvl == 5)
                     printf("%s >>> Entering lvl 5 -- MSKL5=0 \n\r",TimeStamp());
               }
               wait_state = OFF;               // Exiting wait state. Lets do some work...

               switch (lvl) {
                  case 1:
                     GR[0][0]   = 0x0010;      // Start addr level 1
                     break;
                  case 2:
                     GR[0][Grp] = 0x0080;      // Start addr level 2
                     break;
                  case 3:
                     GR[0][Grp] = 0x0100;      // Start addr level 3
                     break;
                  case 4:
                     GR[0][Grp] = 0x0180;      // Start addr level 4
                     break;
                  case 5:                      // Continue with GR0G3
                     break;
                  default:
                     reason = SCPE_IERR;       // We got a problem !
                     break;
               }
               break;                          // Go for it...
            }
            /* If level 5 and mask is ON go into WAIT state */
            if (i == 5) {
               if (int_lvl_mask[5] == ON) {
                  /* Looks like we have nothing to do, so let's wait...  */
                  if ((debug_reg & 0x02) && (wait_state == OFF)) {
                     fprintf(trace, "%s >>> Entering wait state in lvl=5, GR0G3=%05X \n",
                                    TimeStamp(),GR[0][RegGrp(i)]);
                     fprintf(trace, "%s >>> Waiting... \n",TimeStamp());
                  }
                  wait_state = ON;             // Enter wait state
               }
               lvl = i;                        // Set pgm level 5
               Grp = RegGrp(lvl);              // Set reg group 5
               break;                          // Out of inner 'for' loop
            }
            continue;                          // Check next lower pgm lvl
         }
         continue;                             // Check next lower pgm lvl
      }

      lvl = i;                                 // Set current pgm level
      Grp = RegGrp(lvl);                       // Set current reg group
      break;                                   // Continue with current pgm lvl
   }

   if (wait_state == ON) {
      usleep(1000);                            // Get some rest...
      continue;
   }

//=======================================================================================
// Read instruction and execute it starts here...
//=======================================================================================

   if (lvl != 1) LAR = saved_PC;               /* Update LAR if lvl 2, 3, 4 or 5 */
   PC = GR[0][Grp];                            /* Update PC with IAR */
   saved_PC = PC;

   val[0] = opcode0 = GetMem(PC);              /* Instruction byte 0(H) */
   PC = (PC + 1) & AMASK;
   val[1] = opcode1 = GetMem(PC);              /* Instruction byte 1(L) */
   PC = (PC + 1) & AMASK;
   opcode = (opcode0 << 8) | (opcode1);        /* Instr to be executed. */
   val[2] = GetMem(PC);                        /* Needed for possible LA */
   val[3] = GetMem(PC + 1);                    /* and BAL instructions. */

   if (((opcode0 & 0x88) == 0x00) &&           /* Invalid instruction ? */
       (test_mode == OFF) &&
      ((opcode1 == 0x00) ||
       (opcode1 == 0x20) ||
       (opcode1 == 0x50) ||
       (opcode1 == 0x60) ||
       (opcode1 == 0x70))) {
      OP_reg_chk = ON;
      if (lvl == 1)
         reason = STOP_INVOP;                  /* SIMH stop */
      continue;
   }
   GR[0][Grp] = PC;                            /* Update IAR before execution */

   // CCU Cycle Utilization counter
   cycle_eight++;                              /* Count 8 cycles               */
   if (cycle_eight == 8) {                     /* If eight cycles...           */
      cycle_eight = 0;                         /* ...reset 8 cycle counter...  */
      if (Eregs_Inp[0x7A] == 0xFFFF)           /* ...If cycle counter at max...*/
         Eregs_Inp[0x7A] = 0x8000;             /* ...reset cycle counter       */
      else                                     /* ...else...                   */
         Eregs_Inp[0x7A]++;                    /* ...Incr Cycle Utilization Reg*/
   } // End if cycle_eight

   switch (opcode & 0xF800) {
      case (0xA800):
         /* B    T              [RT]  */
         /* 01234567 89012345
            10101T<- ------>#         */
         Grp = RegGrp(lvl);
         Tfld = opcode & 0x07FE;

         if (opcode & 0x0001)                  /* Check displacement sign */
            GR[0][Grp] = GR[0][Grp] - Tfld;
         else
            GR[0][Grp] = GR[0][Grp] + Tfld;
         PC = GR[0][Grp];                      /* Update PC with new IAR */
         break;

      case (0x9800):
         /* BCL  T              [RT]  */
         /* 01234567 89012345
            10011T<- ------>#         */
         Grp = RegGrp(lvl);
         Tfld = (opcode & 0x07FE);

         if (CL_C[Grp] == ON) {
            if (opcode & 0x0001)
               GR[0][Grp] = GR[0][Grp] - Tfld;
            else
               GR[0][Grp] = GR[0][Grp] + Tfld;
            PC = GR[0][Grp];                   /* Update PC with new IAR */
         }
         break;

      case (0x8800):
         /* BZL  T              [RT]  */
         /* 01234567 89012345
            10001T<- ------>#         */
         Grp = RegGrp(lvl);
         Tfld = (opcode & 0x07FE);

         if (CL_Z[Grp] == ON) {
            if (opcode & 0x0001)
               GR[0][Grp] = GR[0][Grp] - Tfld;
            else
               GR[0][Grp] = GR[0][Grp] + Tfld;
            PC = GR[0][Grp];                   /* Update PC with new IAR */
         }
         break;

      case (0xB800):
         /* BCT  R(N),T         [RT]  */
         /* 01234567 89012345
            10111RRN 1T<-->T#         */
         if (opcode1 & 0x80) {                 /* Must be a 1, else it is BAL or LA instr */
            Grp = RegGrp(lvl);
            Rfld = (opcode0 & 0x06) + 1;       /* Extract odd register nr */
            Nfld = (opcode0 & 0x01);
            Tfld =  opcode1 & 0x7E;

            if (Nfld == 0) {                   /* Count is contained in byte 0 only */
               w_byte = (GR[Rfld][Grp] - 0x00100) & 0x0FF00;
               GR[Rfld][Grp] = (GR[Rfld][Grp] & 0x300FF) | w_byte;
            } else {                           /* Count is contained in byte 0 & 1 */
               w_byte = (GR[Rfld][Grp] - 0x00001) & 0x0FFFF;
               GR[Rfld][Grp] = (GR[Rfld][Grp] & 0x30000) | w_byte;
            }
            if ((w_byte & 0xFFFF) == 0x0000)   /* Next instr if result = 0 */
               break;
            if (opcode1 & 0x01)                /* Check displacement sign */
               GR[0][Grp] = GR[0][Grp] - Tfld;
            else
               GR[0][Grp] = GR[0][Grp] + Tfld;
            PC = GR[0][Grp];                   /* Update PC with new IAR */
         }
         break;

      case (0xC800):
      case (0xD800):
      case (0xE800):
      case (0xF800):
         /* BB   R(N),T         [RT]  */
         /* 01234567 89012345
            11MM1RRN MT<-->T#         */
         Mfld = ((opcode0 & 0x30) >> 3) + ((opcode1 & 0x80) >> 7);
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = opcode0 & 0x01;
         Tfld = opcode1 & 0x7E;

         if (Nfld == 0)                        /* Test for byte 0 or 1 */
            Mfld = 0x8000 >> Mfld;             /* Shift test mask to byte 0(H) */
         else
            Mfld = 0x0080 >> Mfld;             /* Create bit test mask */

         if ((GR[Rfld][Grp] & Mfld) != 0x0000) {  /* Test with mask */
            /* Selected bit is ON, continue at branch addr. */
            if (opcode1 & 0x01)                /* Check displacement sign */
               GR[0][Grp] = GR[0][Grp] - Tfld;
            else
               GR[0][Grp] = GR[0][Grp] + Tfld;
            PC = GR[0][Grp];                   /* Update PC with new IAR */
         }
         break;

      case (0x8000):
         /* LRI  R(N),I         [RI]  */
         /* 01234567 89012345
            10000RRN I<---->I         */
         Grp = RegGrp(lvl);
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);
         /* Reset C&Z latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         if (Nfld == 0) {                      /* Byte 0(H) */
            GR[Rfld][Grp] = (GR[Rfld][Grp] & 0x300FF) | (opcode1 << 8);
         } else {                              /* Byte 1(L) */
            GR[Rfld][Grp] = (GR[Rfld][Grp] & 0x3FF00) | opcode1;
         }
         /* Test selected byte for zero */
         if (opcode1 == 0x00) {
            CL_Z[Grp] = ON;
         } else {
            CL_C[Grp] = ON;
         }
         break;

      case (0x9000):
         /* ARI  R(N),I         [RI]  */
         /* 01234567 89012345
            10010RRN I<---->I         */
         Grp = RegGrp(lvl);
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);
         Ifld =  opcode1;
         /* Reset C&Z latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         if (Nfld == 0) {                      /* Byte 0(H) */
            w_byte = GR[Rfld][Grp] + (Ifld << 8);
            if (((GR[Rfld][Grp] & 0xFFFF) +    /* Overflow from byte 0(H) ? */
                 (Ifld << 8)) > 0xFFFF)
               CL_C[Grp] = ON;
            if ((w_byte & 0xFF00) == 0x0000)   /* Result zero ? */
               CL_Z[Grp] = ON;
            w_byte &= 0x3FFFF;                 /* Remove possible overflow bit */
            /* Store result back in register */
            GR[Rfld][Grp] = w_byte;
         } else {                              /* Byte X, 0(H) & 1(L) */
            w_byte = GR[Rfld][Grp] + Ifld;
            if (((GR[Rfld][Grp] & 0x0FFFF) +   /* Overflow from byte 1(L) ? */
                 (Ifld)) > 0xFFFF)
               CL_C[Grp] = ON;
            if ((w_byte & 0xFFFF) == 0x0000)   /* Result zero ? (X-byte not include) */
               CL_Z[Grp] = ON;
            w_byte &= 0x3FFFF;                 /* Remove possible overflow bit */
            /* Store result back in register */
            GR[Rfld][Grp] = w_byte;
         }
         break;

      case (0xA000):
         /* SRI  R(N),I         [RI]  */
         /* 01234567 89012345
            10100RRN I<---->I         */
         Grp = RegGrp(lvl);
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);
         Ifld =  opcode1;
         /* Reset C&Z latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         w_byte = Ifld;                        /* Get second operand */

         /* Perform SUB with operand 1 */
         if (Nfld == 0) {                      /* Byte 0(H) result */
            w_byte = GR[Rfld][Grp] + (~(w_byte << 8)) + 1;
            if (((GR[Rfld][Grp] & 0x0FF00) +   /* Overflow from byte 0(H) ? */
                 (~(Ifld << 8) & 0x3FF00) + 0x0100) & 0x10000)
               CL_C[Grp] = ON;
            if ((w_byte & 0x0FF00) == 0x0000)  /* Result zero ? (X-byte not include) */
               CL_Z[Grp] = ON;
            w_byte &= 0x3FFFF;                 /* Remove possible overflow bit */
         } else {                              /* Byte 0 & 1 result */
            w_byte = (GR[Rfld][Grp] + (~w_byte) + 1);
            if (((GR[Rfld][Grp] & 0x0FFFF) +   /* Overflow from byte 0 & 1 ? */
                 (~Ifld) + 1) & 0x10000)
               CL_C[Grp] = ON;
            if ((w_byte & 0xFFFF) == 0x0000)   /* Result zero ? (X-byte not include) */
               CL_Z[Grp] = ON;
            w_byte &= 0x3FFFF;                 /* Remove possible overflow bit */
         }
         /* Store result back in register */
         GR[Rfld][Grp] = w_byte;
         break;

      case (0xB000):
         /* CRI  R(N),I         [RI]  */
         /* 01234567 89012345
            10110RRN I<---->I         */
         Grp = RegGrp(lvl);
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);
         Ifld = opcode1;
         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;

         if (Nfld == 0)                        /* Byte 0(H) */
            w_byte = (GR[Rfld][Grp] >> 8) & 0x000FF;
         else                                  /* Byte 1(L) */
            w_byte = GR[Rfld][Grp] & 0x000FF;
         /* Update C&Z latches */
         if (w_byte < Ifld)                    /* R < Ifld ? */
            CL_C[Grp] = ON;
         if (w_byte == Ifld)                   /* Equal ? */
            CL_Z[Grp] = ON;
         break;

      case (0xC000):
         /* XRI  R(N),I         [RI]  */
         /* 01234567 89012345
            11000RRN I<---->I         */
         Grp = RegGrp(lvl);
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);
         Ifld =  opcode1;
         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;

         if (Nfld == 0) {                      /* Byte 0(H) */
            GR[Rfld][Grp] = GR[Rfld][Grp] ^ (Ifld << 8);  /* XR */
            /* Update C&Z latches */
            if ((GR[Rfld][Grp] & 0x0FF00) == 0x00000)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         } else {                              /* Byte 1(L) */
            GR[Rfld][Grp] = GR[Rfld][Grp] ^ (Ifld);   /* XR */
            /* Update C&Z latches */
            if ((GR[Rfld][Grp] & 0x000FF) == 0x00000)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         }
         break;

      case (0xD000):
         /* ORI  R(N),I         [RI]  */
         /* 01234567 89012345
            11010RRN I<---->I         */
         Grp = RegGrp(lvl);
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);
         Ifld =  opcode1;
         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;

         if (Nfld == 0) {                      /* Byte 0(H) */
            GR[Rfld][Grp] = GR[Rfld][Grp] | (Ifld << 8);  /* OR */
            /* Update C&Z latches */
            if ((GR[Rfld][Grp] & 0x0FF00) == 0x00000)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         } else {                              /* Byte 1(L) */
            GR[Rfld][Grp] = GR[Rfld][Grp] | (Ifld);   /* OR */
            /* Update C&Z latches */
            if ((GR[Rfld][Grp] & 0x000FF) == 0x00000)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         }
         break;

      case (0xE000):
         /* NRI  R(N),I         [RI]  */
         /* 01234567 89012345
            11100RRN I<---->I         */
         Grp = RegGrp(lvl);
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);
         Ifld =  opcode1;
         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;

         if (Nfld == 0) {                      /* Byte 0(H) */
            Ifld = (Ifld << 8) | 0x300FF;
            GR[Rfld][Grp] = GR[Rfld][Grp] & Ifld;     /* AND */
            /* Update C&Z latches */
            if ((GR[Rfld][Grp] & 0x0FF00) == 0x00000)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         } else {                              /* Byte 1(L) */
            Ifld = Ifld | 0x3FF00;
            GR[Rfld][Grp] = GR[Rfld][Grp] & Ifld;    /* AND */
            /* Update C&Z latches */
            if ((GR[Rfld][Grp] & 0x000FF) == 0x00000)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         }
         break;

      case (0xF000):
         /* TRM  R(N),I         [RI]  */
         /* 01234567 89012345
            11110RRN I<---->I         */
         Grp = RegGrp(lvl);
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);
         Ifld =  opcode1;
         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;

         if (Nfld == 0)                        /* Byte 0(H) */
            w_byte = (GR[Rfld][Grp] >> 8) & 0x000FF;
         else                                  /* Byte 1(L) */
            w_byte = GR[Rfld][Grp] & 0x000FF;
         /* Update C&Z latches */
         if ((w_byte & Ifld) == 0x00)
            CL_Z[Grp] = ON;
         else
            CL_C[Grp] = ON;
         break;
   }

   switch (opcode & 0x88FF) {
      case (0x0008):
         /* LCR  R1(N1),R2(N2)  [RR]  */
         /* 01234567 89012345
            0R2N0R1N 00001000         */
         Grp = RegGrp(lvl);
         R1fld = ( opcode0 & 0x06) + 1;        /* Extract reg 1 nr */
         N1fld = ( opcode0 & 0x01);
         R2fld = ((opcode0 & 0x60) >> 4) + 1;  /* Extract reg 2 nr */
         N2fld = ((opcode0 & 0x10) >> 4);
         /* Reset Z&C latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;

         /* Fetch the selected byte from R2 */
         if (N2fld == 0)                       /* Byte 0(H) */
            w_byte = (GR[R2fld][Grp] >> 8) & 0x000FF;
         else
            w_byte = GR[R2fld][Grp] & 0x000FF; /* Byte 1(L) */

         /* Store it the selected byte of R1 */
         if (N1fld == 0)                       /* Byte 0(H) */
            GR[R1fld][Grp] = (GR[R1fld][Grp] & 0x000FF) | (w_byte << 8);
         else                                  /* Byte 1(L) */
            GR[R1fld][Grp] = (GR[R1fld][Grp] & 0x0FF00) | w_byte;
         /* Set Z Latch if selected byte == 0x00 */
         if (w_byte == 0x00)
            CL_Z[Grp] = ON;
         /* Determine if nr of bits is odd or even and set C latch accordingly */
         j = 0;
         for (int i = 8; i != 0; i--) {
            j += w_byte & 0x01;                /* Count the one bits */
            w_byte >>= 1;
         }
         if (j == 0 | j == 2 | j == 4 | j == 6 | j == 8)
            CL_C[Grp] = ON;                    /* Update C latch */
         else
            CL_C[Grp] = OFF;
         break;

      case (0x0018):
         /* ACR  R1(N1),R2(N2)  [RR]  */
         /* 01234567 89012345
            0R2N0R1N 00011000         */
         Grp = RegGrp(lvl);
         R1fld = ( opcode0 & 0x06) + 1;        /* Extract reg 1 nr */
         R2fld = ((opcode0 & 0x60) >> 4) + 1;  /* Extract reg 2 nr */
         N1fld = ( opcode0 & 0x01);
         N2fld = ((opcode0 & 0x10) >> 4);
         /* Reset Z&C latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         /* Fetch the selected byte from R2 */
         if (N2fld == 0)                       /* Byte 0(H) */
            w_byte = (GR[R2fld][Grp] >> 8) & 0x000FF;
         else
            w_byte = GR[R2fld][Grp] & 0x000FF; /* Byte 1(L) */

         /* Perform ADD with the selected byte from R1 */
         if (N1fld == 0) {                     /* Byte 0(H) result */
            w_byte = GR[R1fld][Grp] + (w_byte << 8);
            if ((w_byte & 0x0FF00) == 0x00)    /* Zero ? */
               CL_Z[Grp] = ON;
         } else {                              /* Byte 0 & 1 result */
            w_byte = GR[R1fld][Grp] + w_byte;
            if ((w_byte & 0x0FFFF) == 0x00000) /* Zero ? */
               CL_Z[Grp] = ON;
         }
         /* Byte 0 overflow ? */
         if ((w_byte & 0x7F0000) > (GR[R1fld][Grp] & 0x7F0000))
            CL_C[Grp] = ON;
         /* Remove possible X byte overflow bit and save the result */
         GR[R1fld][Grp] = w_byte & 0x3FFFF;
         break;

      case (0x0028):
         /* SCR  R1(N1),R2(N2)  [RR]  */
         /* 01234567 89012345
            0R2N0R1N 00101000         */
         Grp = RegGrp(lvl);
         R1fld = ( opcode0 & 0x06) + 1;        /* Extract reg 1 nr */
         N1fld = ( opcode0 & 0x01);
         R2fld = ((opcode0 & 0x60) >> 4) + 1;  /* Extract reg 2 nr */
         N2fld = ((opcode0 & 0x10) >> 4);
         /* Reset Z&C latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         int32 R2H, R2L;

         if (N1fld == 0) {                     /* R1 = Byte 0(H) only */
            if (N2fld == 0) {                  /* R2 = Byte 0(H)  */
                                     /* SCR: R1(H.) = R1(H.) - R2(H.) */
               w_byte = (GR[R2fld][Grp]) & 0x0FF00;
            } else {                           /* R2 = Byte 1(L) */
                                     /* SCR: R1(H.) = R1(H.) - R2(.L) */
               w_byte = (GR[R2fld][Grp] << 8) & 0x0FF00;
            }
            if (w_byte > (GR[R1fld][Grp] & 0x0FF00))  /* Result < 0 ? */
               CL_C[Grp] = ON;
            R2H = ~(w_byte);
            R2H = (R2H + 0x00100) & 0x3FF00;   /* 2-complement */
            GR[R1fld][Grp] = (GR[R1fld][Grp] + R2H) & 0x3FFFF;
            if ((GR[R1fld][Grp] & 0x0FF00) == 0x00)   /* Result zero ?*/
               CL_Z[Grp] = ON;

         } else {     /* N1fld == 1 */         /* R1 = Byte H & L */

            if (N2fld == 0) {                  /* R2 = Byte 0(H)  */
                                     /* SCR: R1(HL) = R1(HL) - R2(H.) */
               w_byte = (GR[R2fld][Grp] >> 8) & 0x000FF;
            } else {                           /* R2 = Byte 1(L)  */
                                     /* SCR: R1(HL) = R1(HL) - R2(.L) */
               w_byte = (GR[R2fld][Grp]) & 0x000FF;
            }
            if (w_byte > (GR[R1fld][Grp] & 0x0FFFF))  /* Result < 0 ? */
               CL_C[Grp] = ON;
            R2L = ~(w_byte);
            R2L = (R2L + 1) & 0x3FFFF;         /* 2-complement */
            GR[R1fld][Grp] = (GR[R1fld][Grp] + R2L) & 0x3FFFF;
            if ((GR[R1fld][Grp] & 0x0FFFF) == 0x0000) /* Result zero ?*/
               CL_Z[Grp] = ON;
         }
         break;

      case (0x0038):
         /* CCR  R1(N1),R2(N2)  [RR]  */
         /* 01234567 89012345
            0R2N0R1N 00111000         */
         Grp = RegGrp(lvl);
         R1fld = ( opcode0 & 0x06) + 1;        /* Extract reg 1 nr */
         R2fld = ((opcode0 & 0x60) >> 4) + 1;  /* Extract reg 2 nr */
         N1fld = ( opcode0 & 0x01);
         N2fld = ((opcode0 & 0x10) >> 4);
         /* Reset Z&C latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         /* Fetch the required byte from R2 */
         if (N2fld == 0)
            w_byte = (GR[R2fld][Grp] >> 8);    /* Byte 0(H) */
         else
            w_byte = GR[R2fld][Grp];           /* Byte 1(L) */
         w_byte = w_byte & 0x000FF;

         /* Perform a compare between the selected regs */
         if (N1fld == 0) {                     /* Byte 0(H) */
            if ((( GR[R1fld][Grp] >> 8) & 0x000FF) < w_byte)
               CL_C[Grp] = ON;
            if ((( GR[R1fld][Grp] >> 8) & 0x000FF) == w_byte)
               CL_Z[Grp] = ON;
         } else {                              /* Byte 1(L) */
            if (( GR[R1fld][Grp] & 0x000FF) < w_byte)
               CL_C[Grp] = ON;
            if (( GR[R1fld][Grp] & 0x000FF) == w_byte)
               CL_Z[Grp] = ON;
         }
         break;

      case (0x0048):
         /* XCR  R1(N1),R2(N2)  [RR]  */
         /* 01234567 89012345
            0R2N0R1N 01001000         */
         Grp = RegGrp(lvl);
         R1fld = ( opcode0 & 0x06) + 1;        /* Extract odd reg 1 nr */
         R2fld = ((opcode0 & 0x60) >> 4) + 1;  /* Extract odd reg 2 nr */
         N1fld = ( opcode0 & 0x01);
         N2fld = ((opcode0 & 0x10) >> 4);
         /* Reset Z&C latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         /* Fetch the selected byte from R2 */
         if (N2fld == 0)                       /* Byte 0(H) */
            w_byte = (GR[R2fld][Grp] >> 8) & 0x000FF;
         else
            w_byte = GR[R2fld][Grp] & 0x000FF; /* Byte 1(L) */

         /* Perform XOR with the selected byte from R1 */
         if (N1fld == 0) {                     /* Byte 0(H) */
            GR[R1fld][Grp] ^= (w_byte << 8);
            if ((GR[R1fld][Grp] & 0xFF00) == 0x00)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         } else {                              /* Byte 1(L) */
            GR[R1fld][Grp] ^= w_byte;
            if ((GR[R1fld][Grp] & 0x00FF) == 0x00)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         }
         break;

      case (0x0058):
         /* OCR  R1(N1),R2(N2)  [RR]  */
         /* 01234567 89012345
            0R2N0R1N 01011000         */
         Grp = RegGrp(lvl);
         R1fld = ( opcode0 & 0x06) + 1;        /* Extract odd reg 1 nr */
         N1fld = ( opcode0 & 0x01);
         R2fld = ((opcode0 & 0x60) >> 4) + 1;  /* Extract odd reg 2 nr */
         N2fld = ((opcode0 & 0x10) >> 4);
         /* Reset Z&C latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         /* Fetch the selected byte from R2 */
         if (N2fld == 0)                       /* Byte 0(H) */
            w_byte = (GR[R2fld][Grp] >> 8) & 0x000FF;
         else
            w_byte = GR[R2fld][Grp] & 0x000FF; /* Byte 1(L) */

         /* Perform OR with the selected byte from R1 */
         if (N1fld == 0) {                     /* Byte 0(H) */
            GR[R1fld][Grp] |= (w_byte << 8);
            if ((GR[R1fld][Grp] & 0x0FF00) == 0x00)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         } else {                              /* Byte 1(L) */
            GR[R1fld][Grp] |= w_byte;
            if ((GR[R1fld][Grp] & 0x000FF) == 0x00)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         }
         break;

      case (0x0068):
         /* NCR  R1(N1),R2(N2)  [RR]  */
         /* 01234567 89012345
            0R2N0R1N 01101000         */
         Grp = RegGrp(lvl);
         R1fld = ( opcode0 & 0x06) + 1;        /* Extract odd reg 1 nr */
         R2fld = ((opcode0 & 0x60) >> 4) + 1;  /* Extract odd reg 2 nr */
         N1fld = ( opcode0 & 0x01);
         N2fld = ((opcode0 & 0x10) >> 4);
         /* Reset Z&C latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         /* Fetch the selected byte from R2 */
         if (N2fld == 0)                       /* Byte 0(H) */
            w_byte = (GR[R2fld][Grp] >> 8) & 0x000FF;
         else
            w_byte = GR[R2fld][Grp] & 0x000FF; /* Byte 1(L) */

         /* Perform AND with the selected byte from R1 */
         if (N1fld == 0) {
            GR[R1fld][Grp] &= ((w_byte << 8) | 0x300FF);
            if ((GR[R1fld][Grp] & 0x0FF00) == 0x00)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         } else {
            GR[R1fld][Grp] &= (w_byte | 0x3FF00);
            if ((GR[R1fld][Grp] & 0x000FF) == 0x00)
               CL_Z[Grp] = ON;
            else
               CL_C[Grp] = ON;
         }
         break;

      case (0x0078):
         /* LCOR R1(N1),R2(N2)  [RR]  */
         /* 01234567 89012345
            0R2N0R1N 01111000         */
         Grp = RegGrp(lvl);
         R1fld = ( opcode0 & 0x06) + 1;        /* Extract odd reg 1 nr */
         R2fld = ((opcode0 & 0x60) >> 4) + 1;  /* Extract odd reg 2 nr */
         N1fld = ( opcode0 & 0x01);
         N2fld = ((opcode0 & 0x10) >> 4);
         /* Reset Z&C latches */
         CL_Z[Grp] = OFF;
         CL_C[Grp] = OFF;

         /* Fetch the selected byte from R2 */
         if (N2fld == 0)                       /* Byte 0(H) */
            w_byte = (GR[R2fld][Grp] >> 8) & 0x000FF;
         else
            w_byte = GR[R2fld][Grp] & 0x000FF; /* Byte 1(L) */

         /* Determine C latch and Shift one byte to the right */
         if ((w_byte & 0x00001) == 0x0001)     /* Will we loose a one bit? */
            CL_C[Grp] = ON;
         w_byte = w_byte >> 1;                 /* Shift byte 1 bit right */

         /* Store it the selected byte of R1 */
         if (N1fld == 0)                       /* Byte 0(H) */
            GR[R1fld][Grp] = (GR[R1fld][Grp] & 0x000FF) | (w_byte << 8);
         else                                  /* Byte 1(L) */
            GR[R1fld][Grp] = (GR[R1fld][Grp] & 0x0FF00) | w_byte;
         /* Set Z Latch */
         if (w_byte == 0x00)
            CL_Z[Grp] = ON;
         break;

      case (0x0010):
         /* ICT  R(N),B         [RSA] */
         /* 01234567 89012345
            0BBB0RRN 00010000         */
         Grp = RegGrp(lvl);
         Bfld = (opcode0 >> 4) & 0x007;
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);

         addr = GR[Bfld][Grp];                 /* See PoO 4-9 */
         w_byte = GetMem(addr);
         GR[Bfld][Grp] = GR[Bfld][Grp] + 1;
         if (Nfld == 0) {                      /* Byte 0(H) */
            GR[Rfld][Grp] = (GR[Rfld][Grp] & 0x300FF) | (w_byte << 8);
         } else {                              /* Byte 1(L) */
            GR[Rfld][Grp] = (GR[Rfld][Grp] & 0x3FF00) | w_byte;
         }
         break;

      case (0x0030):
         /* STCT R(N),B         [RSA] */
         /* 01234567 89012345
            0BBB0RRN 00110000         */
         Grp = RegGrp(lvl);
         Bfld = (opcode0 >> 4) & 0x007;
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);

         addr = GR[Bfld][Grp];                 /* See PoO 4-13 */
         GR[Bfld][Grp] = GR[Bfld][Grp] + 1;
         if (Nfld == 0)                        /* Byte 0(H) */
            w_byte = (GR[Rfld][Grp] >> 8) & 0x000FF;
         else
            w_byte = GR[Rfld][Grp] & 0x000FF;  /* Byte 1(L) */
         PutMem(addr, w_byte);
         break;
   }

   switch (opcode & 0x8880) {
      case (0x0800):
         /* IC   R(N),D(B)      [RS]  */
         /* 01234567 89012345
            0BBB1RRN 0D<--->D         */
         Grp = RegGrp(lvl);
         Bfld = (opcode0 >> 4) & 0x007;
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);
         Dfld = (opcode1) & 0x7F;

         if (Bfld == 0)
            addr = 0x00680 + Dfld;             /* See PoO 4-9 */
         else
            addr = GR[Bfld][Grp] + Dfld;
         w_byte = GetMem(addr);

         if (Nfld == 0)                        /* Byte 0(H) */
            GR[Rfld][Grp] = (GR[Rfld][Grp] & 0x300FF) | (w_byte << 8);
         else                                  /* Byte 1(L) */
            GR[Rfld][Grp] = (GR[Rfld][Grp] & 0x3FF00) | w_byte;

         /* Test the selected byte (w_byte) */
         if (w_byte == 0x00)
            CL_Z[Grp] = ON;
         else
            CL_Z[Grp] = OFF;

         /* Determine if nr of bits is odd or even and set C latch accordingly */
         j = 0;
         for (int i = 8; i != 0; i--) {
            j += w_byte & 0x01;                /* Count the one bits */
            w_byte >>= 1;
         }
         if (j == 0 | j == 2 | j == 4 | j == 6 | j == 8)
            CL_C[Grp] = ON;
         else
            CL_C[Grp] = OFF;
         break;

      case (0x0880):
         /* STC  R(N),D(B)      [RS]  */
         /* 01234567 89012345
            0BBB1RRN 1D<--->D         */
         Grp = RegGrp(lvl);
         Bfld = (opcode0 >> 4) & 0x07;
         Dfld = (opcode1) & 0x7F;
         Rfld = (opcode0 & 0x06) + 1;          /* Extract odd register nr */
         Nfld = (opcode0 & 0x01);

         if (Bfld == 0)
            addr = 0x00680 + Dfld;             /* See PoO 4-13 */
         else
            addr = GR[Bfld][Grp] + Dfld;

         if (Nfld == 0)
            w_byte = (GR[Rfld][Grp] >> 8) & 0x000FF;
         else
            w_byte = (GR[Rfld][Grp] & 0x000FF);
         PutMem(addr, w_byte);
         break;
   }

   switch (opcode & 0x8881) {
      case (0x0001):
         /* LH   R,D(B)         [RS]  */
         /* 01234567 89012345
            0BBB0RRR 0D<-->D1         */
         Grp = RegGrp(lvl);
         Bfld = (opcode0 >> 4) & 0x007;
         Dfld = (opcode1) & 0x7E;
         Rfld = (opcode0) & 0x007;             /* Extract register nr */

         if (Bfld == 0)
            addr = 0x00700 + Dfld;             /* See PoO 4-10 */
         else
            addr = (GR[Bfld][Grp] + Dfld);
         addr &= 0x3FFFE;                      /* Force HW boundary */

         w_byte = GetMem(addr) << 8;
         addr++;
         w_byte = w_byte | GetMem(addr);
         old_crc = w_byte;
         GR[Rfld][Grp] = w_byte;               /* X-byte = 0 */
         if (Rfld == 0) break;                 /* New IAR ! */

         /* Update C&Z latches */
         if (w_byte == 0x0000) {
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x0081):
         /* STH  R,D(B)         [RS]  */
         /* 01234567 89012345
            0BBB0RRR 1D<-->D1         */
         Grp = RegGrp(lvl);
         Bfld = (opcode0 >> 4) & 0x007;
         Rfld = (opcode0) & 0x007;             /* Extract register nr */
         Dfld = (opcode1) & 0x7E;

         if (Bfld == 0)
            addr = 0x00700 + Dfld;             /* See PoO 4-4 */
         else
            addr = (GR[Bfld][Grp] + Dfld);
         addr &= 0x3FFFE;                      /* Force HW boundary */

         if (Rfld > 0) {
            PutMem(addr, (GR[Rfld][Grp] >> 8) & 0x000FF);
            addr++;
            PutMem(addr, GR[Rfld][Grp] & 0x000FF);
         } else {
            PutMem(addr,   0x00);
            PutMem(addr+1, 0x00);
         }
         break;
   }

   switch (opcode & 0x8883) {
      case (0x0002):
         /* L    R,D(B)         [RS]  */
         /* 01234567 89012345
            0BBB0RRR 0D<->D10         */
         Grp = RegGrp(lvl);
         Bfld = (opcode0 >> 4) & 0x07;
         Dfld = (opcode1) & 0x7C;              /* Dfld at fullword boundary */
         Rfld = (opcode0) & 0x07;              /* Extract register nr */

         if (Bfld == 0)
            addr = 0x00780 + Dfld;             /* See PoO 4-10 */
         else
            addr = (GR[Bfld][Grp] + Dfld);
         addr &= 0x3FFFE;                      /* Force HW boundary */

         w_byte = (GetMem(addr+1) & 0x03) << 16; /* Load X-byte */
         w_byte |= GetMem(addr+2) << 8;        /* Byte 0(H) */
         w_byte |= GetMem(addr+3);             /* Byte 1(L) */
         GR[Rfld][Grp] = w_byte;
         if (Rfld == 0) break;                 /* New IAR ! */

         /* Update C&Z latches */
         if (w_byte == 0x00000) {              /* Test includes X-byte */
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x0082):
         /* ST   R,D(B)         [RS]  */
         /* 01234567 89012345
            0BBB0RRR 1D<->D10         */
         Grp = RegGrp(lvl);
         Bfld = (opcode0 >> 4) & 0x07;
         Dfld = (opcode1) & 0x7C;              /* Dfld at fullword boundary */
         Rfld = (opcode0) & 0x07;              /* Extract register nr */

         if (Bfld == 0)
            addr = 0x00780 + Dfld;             /* See PoO 4-12 */
         else
            addr = (GR[Bfld][Grp] + Dfld);
         addr &= 0x3FFFE;                      /* Force HW boundary */

         if (Rfld > 0) {
            PutMem(addr+3,  GR[Rfld][Grp] & 0xFF);
            PutMem(addr+2, (GR[Rfld][Grp] >> 8) & 0xFF);
            w_byte = GetMem(addr+1) & 0xFC;    /* Keep the high 6 bits */
            PutMem(addr+1, w_byte | ((GR[Rfld][Grp] >> 16) & 0x03));
         } else {
            PutMem(addr+3, 0x00);              /* Clear mem locations */
            PutMem(addr+2, 0x00);
            w_byte = GetMem(addr+1) & 0xFC;    /* Keep the high 6 bits */
            PutMem(addr+1, w_byte);            /* Store X-byte bits */
         }
         // NOTE: special condition ST inst at loc 0x0010 to be implemented !!
         break;
   }

   switch (opcode & 0x88FF) {
      case (0x0080):
         /* LHR  R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 10000000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x07);            /* Extract register 1 */

         w_byte = GR[R2fld][Grp] & 0x0FFFF;    /* Load R1 with contents of R2 */
         GR[R1fld][Grp] = w_byte;
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Update C&Z latches */
         if (GR[R1fld][Grp] == 0x0000) {
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x0090):
         /* AHR  R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 10001000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = (GR[R1fld][Grp] & 0xFFFF) + (GR[R2fld][Grp] & 0xFFFF);
         GR[R1fld][Grp] = (GR[R1fld][Grp] & 0x30000) | (w_byte & 0xFFFF);
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;
         /* Update C&Z latches */
         if (w_byte & 0x10000)                 /* Overflow ? */
            CL_C[Grp] = ON;
         if ((w_byte & 0xFFFF) == 0x0000)      /* Result 0 ? */
            CL_Z[Grp] = ON;
         break;

      case (0x00A0):
         /* SHR  R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 10011000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = GR[R1fld][Grp] + ~(GR[R2fld][Grp]) + 1;
         GR[R1fld][Grp] = w_byte & 0xFFFF;     /* Remove possible overflow bit */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;
         /* Update C&Z latches */
         if (w_byte & 0x10000)                 /* Result < 0 ? */
            CL_C[Grp] = ON;
         if (GR[R1fld][Grp] == 0x0000)         /* Result == 0 ? */
            CL_Z[Grp] = ON;
         break;

      case (0x00B0):
         /* CHR  R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 10110000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */
         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;

         /* Test if R1 is < R2 */
         if ((GR[R1fld][Grp] & 0xFFFF) ==      /* Compare for equal */
             (GR[R2fld][Grp] & 0xFFFF))
            CL_Z[Grp] = ON;
         if ((GR[R1fld][Grp] & 0xFFFF) <       /* Compare for less */
             (GR[R2fld][Grp] & 0xFFFF))
            CL_C[Grp] = ON;
         break;

      case (0x00C0):
         /* XHR  R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 11000000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = (GR[R1fld][Grp] & 0x0FFFF) ^ (GR[R2fld][Grp] & 0x0FFFF);
         GR[R1fld][Grp] = (GR[R1fld][Grp] & 0xF0000) | w_byte;  /* XHR */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Update C&Z latches */
         if (w_byte == 0x0000) {
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x00D0):
         /* OHR  R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 11010000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = (GR[R1fld][Grp] & 0x0FFFF) | (GR[R2fld][Grp] & 0x0FFFF);
         GR[R1fld][Grp] = (GR[R1fld][Grp] & 0x30000) | w_byte;  /* OHR */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Update C&Z latches */
         if ((GR[R1fld][Grp] & 0xFFFF) == 0x0000) {
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x00E0):
         /* NHR  R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 11100000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = (GR[R1fld][Grp] & 0x0FFFF) & (GR[R2fld][Grp] & 0x0FFFF);
         GR[R1fld][Grp] = (GR[R1fld][Grp] & 0x30000) | w_byte;  /* NHR */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Update C&Z latches */
         if (w_byte == 0x0000) {
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x00F0):
         /* LHOR R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 11110000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = GR[R2fld][Grp];
         GR[R1fld][Grp] = (GR[R2fld][Grp] & 0x0FFFF) >> 1; /* Shift 1 bit to the right */
         if (Rfld == 0) break;                 /* New IAR ! */

         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;
         /* Update C&Z latches */
         /* If a 1 bit will be shifted out, set C latch */
         if (w_byte & 0x00001)
            CL_C[Grp] = ON;
         if (GR[R1fld][Grp] == 0x00000)
            CL_Z[Grp] = ON;
         break;

      case (0x0088):
         /* LR   R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 10001000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         GR[R1fld][Grp] = GR[R2fld][Grp];      /* Load R1 with contents of R2 */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Update C&Z latches */
         if (GR[R1fld][Grp] == 0x00000) {
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x0098):
         /* AR   R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 10011000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = GR[R1fld][Grp] + GR[R2fld][Grp];
         GR[R1fld][Grp] = w_byte & 0x3FFFF;    /* Remove possible overflow bit */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;                /* No update C&Z latches */

         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;
         /* Update C&Z latches */
         if (w_byte & 0x40000)                 /* Bit 21 overflow ? */
            CL_C[Grp] = ON;
         if (GR[R1fld][Grp] == 0x00000)
            CL_Z[Grp] = ON;
         break;

      case (0x00A8):
         /* SR   R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 10101000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = GR[R1fld][Grp] + ~(GR[R2fld][Grp]) + 1;   /* SR */
         GR[R1fld][Grp] = w_byte & 0x3FFFF;    /* Remove possible overflow bit */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;
         /* Update C&Z latches */
         if (w_byte & 0x40000)                 /* X-byte included */
            CL_C[Grp] = ON;
         if (GR[R1fld][Grp] == 0x00000)
            CL_Z[Grp] = ON;
         break;

      case (0x00B8):
         /* CR   R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 10110000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */
         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;

         /* Test if R1 is < R2 */                           /* CR */
         if (GR[R1fld][Grp] == GR[R2fld][Grp]) /* Compare for equal */
            CL_Z[Grp] = ON;
         if (GR[R1fld][Grp] < GR[R2fld][Grp])  /* Compare for less */
            CL_C[Grp] = ON;
         break;

         if (GR[R1fld][Grp] < GR[R2fld][Grp])  /* Compare for less */
            CL_C[Grp] = ON;
         break;

      case (0x00C8):
         /* XR   R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 11001000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         GR[R1fld][Grp] = GR[R1fld][Grp] ^ GR[R2fld][Grp];  /* XR */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Update C&Z latches */
         if (GR[R1fld][Grp] == 0x00000) {      /* Result zero ? */
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x00D8):
         /* OR   R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 11011000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         GR[R1fld][Grp] = GR[R1fld][Grp] | GR[R2fld][Grp];  /* OR */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Update C&Z latches */
         if (GR[R1fld][Grp] == 0x00000) {      /* Result zero ? */
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x00E8):
         /* NR   R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 11101000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         GR[R1fld][Grp] = GR[R1fld][Grp] & GR[R2fld][Grp];  /* NR */
         /* If R1 = Register 0, a branch to newly formed address occurs */
         if (R1fld == 0) break;

         /* Update C&Z latches */
         if (GR[R1fld][Grp] == 0x00000) {      /* Result zero ? */
            CL_Z[Grp] = ON;
            CL_C[Grp] = OFF;
         } else {
            CL_Z[Grp] = OFF;
            CL_C[Grp] = ON;
         }
         break;

      case (0x00F8):
         /* LOR  R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 11111000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = GR[R2fld][Grp];
         GR[R1fld][Grp] = GR[R2fld][Grp] >> 1; /* Shift 1 bit to the right */
         GR[R1fld][Grp] &= 0x1FFFF;            /* Make sure a 0 is inserted */
         if (Rfld == 0) break;                 /* New IAR ! */

         /* Reset C&Z latches */
         CL_C[Grp] = OFF;
         CL_Z[Grp] = OFF;
         /* Update C&Z latches */
         /* If a 1 bit will be shifted out, set C latch */
         if (w_byte & 0x00001)
            CL_C[Grp] = ON;
         if (GR[R1fld][Grp] == 0x00000)        /* Result zero ? */
            CL_Z[Grp] = ON;
         break;

      case (0x0040):
         /* BALR R1,R2          [RR]  */
         /* 01234567 89012345
            0R2R0R1R 01000000         */
         Grp = RegGrp(lvl);
         R2fld = ((opcode0 & 0x70) >> 4);      /* Extract register 2 */
         R1fld = ( opcode0 & 0x007);           /* Extract register 1 */

         w_byte = GR[R2fld][Grp];              /* See PoO 4-7 */
         if (R1fld > 0)
            GR[R1fld][Grp] = GR[0][Grp];       /* Save addr next seq instr. */
         if (R2fld > 0)
            GR[0][Grp] = w_byte;               /* New IAR */
         break;
   }

   switch (opcode & 0x880F) {
      case (0x000C):
         /* IN   R,E            [RE]  */
         /* 01234567 89012345
            0EEE0RRR EEEE1100         */
         Grp = RegGrp(lvl);
         Efld = (opcode0  & 0x70) | (opcode1 >> 4);
         Rfld = (opcode0) & 0x007;             /* Extract register nr */

         if (lvl == 5) {                       // && (test_mode == OFF)) {
            IO_L5_chk = ON;                    /* Check: I/O instr in level 5 ! */
            break;
         }
         if (Efld < 0x20) {                    /* Input from GR's ? */
            GR[Rfld][Grp] = GR[Efld & 0x007][Efld >> 3];
         } else {
            // An Input x'40' will reset L2 req
            if ((Efld == 0x40) && (lvl == 2)) {
               abar = abar_int;                /* Get abar the of L2 line interrupt */
               Eregs_Inp[0x40] = 0x0800 + (abar << 1);  /* Get vector @ of L2 interrupt */
               pthread_mutex_lock(&r77_lock);
               Eregs_Inp[0x77] &= ~0x4000;     /* Reset L2 flag */
               // An Input x'40' will reset L2 req
               svc_req_L2 = OFF;               /* Reset L2 request flag */
               pthread_mutex_unlock(&r77_lock);
            } else {
               // Read ABAR when executing in L3 or L4.
               Eregs_Inp[0x40] = 0x0800 + (abar << 1);  /* Echo abar */
            }

            if ((Efld >= 0x40) && Efld <= 0x47) {   // Addressing CS2 ICW regs ?
               // ICW Input register ===> Eregs_Out 44, 45, 46, 47
               Get_ICW(abar-0x020);                 // Update ICW input regs (0x44...0x47)
            }

            if (Efld == 0x44) {                     // NCP has read received byte
               if ((icw_pcf[abar-0x020] == 0x05) ||     // TEMP PDF is now empty for next rx
                  (icw_pcf[abar-0x020] == 0x06)  ||
                  (icw_pcf[abar-0x020] == 0x07))
                     icw_pdf_reg[abar-0x020] = EMPTY;
            }

            //********************************************************
            //          Channel Adaptor Type 2 updates
            //********************************************************
            if (Efld == 0x50) {                     // Get INCWAR ?
               Eregs_Inp[0x50] = Eregs_Out[0x50];   // Load INCWAR as used by CA
            }
            if (Efld == 0x51) {                     // Get OUTCWAR ?
               Eregs_Inp[0x51] = Eregs_Out[0x51];   // Load OUTCWAR as used by CA
            }
            if (Efld == 0x59) {                     // Get Cycle Steal Address Register ?
               if (Eregs_Inp[0x59] & 0x10000) {     // if X-bit 7 on ...
                  Eregs_Inp[0x58] |= 0x0001;        // ... set this in Channel Bus Out Register
               } else {                             // if X-bit 6 not on ...
                  Eregs_Inp[0x58] &= ~0x0001;       // ... reset this in Channel Bus Out Register
               }
               if (Eregs_Inp[0x59] & 0x20000) {     // if X-bit 6 on ...
                  Eregs_Inp[0x58] |= 0x0002;        // ... set this in Channel Bus Out Register
               } else {                             // if X-bit 6 not on ...
                  Eregs_Inp[0x58] &= ~0x0002;       // ... reset this in Channel Bus Out Register
               }
            }

            //********************************************************
            //          General Register updates
            //********************************************************
            Eregs_Inp[0x74]  = LAR;                 // Update LAR

            Eregs_Inp[0x76]  = 0x0000;              // Reset all bits in reg 0x76
            if (CS2_L1_chk)  Eregs_Inp[0x76] |= 0x2000;  // Scanner 2 bus parity check
            if (CS3_L1_chk)  Eregs_Inp[0x76] |= 0x1000;  // Scanner 3 bus parity check
            if (CS4_L1_chk)  Eregs_Inp[0x76] |= 0x0800;  // Scanner 4 bus parity check

            Eregs_Inp[0x79]  = 0x0000;              // Reset all bits in reg 0x79
            if (CL_C[3] == ON) Eregs_Inp[0x79] |= 0x0200;  // L5 C & Z flags
            if (CL_Z[3] == ON) Eregs_Inp[0x79] |= 0x0100;
            if (int_lvl_ent[2] == ON) Eregs_Inp[0x79] |= 0x0080;  // L2 interrupted
            if (int_lvl_ent[3] == ON) Eregs_Inp[0x79] |= 0x0040;  // L3 interrupted
            if (int_lvl_ent[4] == ON) Eregs_Inp[0x79] |= 0x0020;  // L4 interrupted
            if (int_lvl_ent[5] == ON) Eregs_Inp[0x79] |= 0x0010;  // L5 interrupted
            Eregs_Inp[0x79] |= 0x0008;              // Fet storage installed
            Eregs_Inp[0x79] |= 0x0001;              // CE IPL escape jumper NOT installed

            Eregs_Inp[0x7B] = calculateBSCCrcChar(old_crc, crc_data);
            Eregs_Inp[0x7C] = 0xF0B8;               // Good SDLC CRC.

            Eregs_Inp[0x7D] = 0x0000;               // Reset all bits in reg 0x7D
            if (FET_stor_diag)                      // If FET storage diagnostics...
                             Eregs_Inp[0x7D]  |= 0x0C00;   // ...set SAR & SDR storage parity chks
            if (IO_par_chk)  Eregs_Inp[0x7D]  |= 0x1000;   // Program check in L1

            Eregs_Inp[0x7E] = 0x0000;               // Reset all bits in reg 0x7E
            if (adr_ex_chk)  Eregs_Inp[0x7E]  |= 0x0040;   // Address exception check
            if (IO_L5_chk)   Eregs_Inp[0x7E]  |= 0x0020;   // I/O instr in L5
            if (IO_par_chk)  Eregs_Inp[0x7E]  |= 0x0020;   // I/O bus parity check
            if (OP_reg_chk)  Eregs_Inp[0x7E]  |= 0x0008;   // OPC check
            if (ipl_req_L1)  Eregs_Inp[0x7E]  |= 0x0002;   // IPL L1 request

            Eregs_Inp[0x7F] &= 0x0204;              // Reset bits in reg 0x7F
            if (diag_req_L2) Eregs_Inp[0x7F]  |= 0x8000;   // Diagnostic L2 request
            if (pci_req_L4)  Eregs_Inp[0x7F]  |= 0x0100;   // PCI L4 request
            if (pci_req_L3)  Eregs_Inp[0x7F]  |= 0x0002;   // PCI L3 request
            if (svc_req_L4)  Eregs_Inp[0x7F]  |= 0x0001;   // SVC L4 request

            GR[Rfld][Grp] = Eregs_Inp[Efld];  // <<=== !!!
         }
         break;

      case (0x0004):
         /* OUT  R,E            [RE]  */
         /* 01234567 89012345
            0EEE0RRR EEEE0100         */
         Grp = RegGrp(lvl);
         Efld = (opcode0  & 0x70) | (opcode1 >> 4);
         Rfld = (opcode0) & 0x07;              // Extract register nr

         if (lvl == 5) {
            IO_L5_chk = ON;                    // I/O instr in L5
            break;
         }
         if ((lvl == 2) || (lvl == 3) || (lvl == 4)) {
            crc_data = 0xFF & GR[Rfld][Grp];   // store crc data on all OUT with level 2,3,4
         }
         if (Efld < 0x20) {                    // Output to GR's ?
            if (Rfld == 0) break;              // Only regen of regs parity
            GR[Efld & 0x007][Efld >> 3] = GR[Rfld][Grp];
         } else {
            Eregs_Out[Efld] = GR[Rfld][Grp];   // <<=== !!! Finally update I/O reg.

            //********************************************************
            //          Communication Scanner Type 2 ICW updates
            //********************************************************
            if ((Efld >= 0x40) && (Efld <= 0x47)) {  // Addressing CS2 ICW regs ?
               // Obtain ICW update lock
               pthread_mutex_lock(&icw_lock);
               if (Efld == 0x40) {                   // Update abar
                  abar = (Eregs_Out[0x40] - 0x0800) >> 1;
               }
               if ((Efld == 0x40) && ((lvl == 3) || (lvl == 4))) {
                  // Update ICW input regs only when in L3 or L4.
                  Get_ICW(abar-0x020);
               }
               /* Only scanner 1 installed; 2, 3, 4: bus parity check */
               if ((abar >= 0x0A0) && (abar <= 0x0FF)) {
                  IO_par_chk = CS2_L1_chk = ON; }
               if ((abar >= 0x120) && (abar <= 0x17F)) {
                  IO_par_chk = CS3_L1_chk = ON; }
               if ((abar >= 0x1A0) && (abar <= 0x1FF)) {
                  IO_par_chk = CS4_L1_chk = ON; }

               if (Efld == 0x44) {                   // ICW SCF & PDF
                  if (Eregs_Out[0x44] & 0x8000) {
                     icw_scf[abar-0x020] &= 0x7F;    // Abort RESET
                  }
                  if (Eregs_Out[0x44] & 0x4000) {
                     icw_scf[abar-0x020] &= 0xBF;    // Service Interlock RESET
                  }
                  if (Eregs_Out[0x44] & 0x2000) {
                     icw_scf[abar-0x020] &= 0xDF;    // Char Overrun/Underrun flag RESET
                  }
                  if (Eregs_Out[0x44] & 0x1000) {
                     icw_scf[abar-0x020] &= 0xEF;    // Modem Check RESET
                  }
                  //if (Eregs_Out[0x44] & 0x0800) {
                  //   icw_scf[abar-0x020] &= 0xF7;    // Unknown flag RESET
                  //}
                  if (Eregs_Out[0x44] & 0x0400) {
                     icw_scf[abar-0x020] &= 0xFB;    // Zero-insert remembrance flag RESET
                  }
                  if (Eregs_Out[0x44] & 0x0200) {
                     icw_scf[abar-0x020] |= 0x02;    // Set program flag
                  } else {
                     icw_scf[abar-0x020] &= ~0x02;   // Reset program flag
                  }
                  if (Eregs_Out[0x44] & 0x0100) {
                     icw_scf[abar-0x020] |= 0x01;    // Set pad flag / SDLC disable zero insert control
                  } else {
                     icw_scf[abar-0x020] &= ~0x01;   // Reset pad flag / SDLC disable zero insert control
                  }
                  icw_scf[abar-0x020] |= (Eregs_Out[0x44] >> 8) & 0x03;   // Only Serv Req, DCD & Pgm Flag
                  icw_pdf[abar-0x020]  =  Eregs_Out[0x44] & 0x00FF;       // Update PDF

                  if (icw_pcf[abar-0x020] != 0x07)      // TEMP !!!
                     icw_pdf_reg[abar-0x020] = FILLED;  // PDF is filled for tx
               }
               if (Efld == 0x45) {                   // ICW LCD & PCF
                  icw_lcd[abar-0x020] = (Eregs_Out[0x45] >> 4) & 0x0F;
                  icw_pcf_nxt[abar-0x020] = Eregs_Out[0x45] & 0x0F;
               }
                                                     // ICW SDF
               if (Efld == 0x46) icw_sdf[abar-0x020]    = (Eregs_Out[0x46] >> 2) & 0x00FF;
                                                     // ICW 34 - 45
               if (Efld == 0x47) icw_Rflags[abar-0x020] = (Eregs_Out[0x47] << 4) & 0x0070;
               // Release ICW update lock.
               pthread_mutex_unlock(&icw_lock);
            }

            //********************************************************
            //          Channel Adaptor Type 2 updates
            //********************************************************
            if (Efld == 0x53) {                // Channel Adapter Sense
               if (Eregs_Out[0x53] & 0xFFFF)   // If any bit set...
                  Eregs_Out[0x54] |= 0x0100;   // ...set Unit Check
            }
            if (Efld == 0x56) {                // Channel Adapter Mode
               if (Eregs_Out[0x56] & 0x2000) {
                  Eregs_Inp[0x55] &= ~0x2000;  // Reset INCWAR valid
                  Eregs_Out[0x55] &= ~0x2000;  // Reset INCWAR valid
               }
               if (Eregs_Out[0x56] & 0x1000) {
                  Eregs_Inp[0x55] &= ~0x1000;  // Reset OUTCWAR valid
                  Eregs_Out[0x55] &= ~0x1000;  // Reset OUTCWAR valid
               }
            }

            if (Efld == 0x57) {                // Channel Adapter Mode
               if (Eregs_Out[0x57] & 0x0010) { // Reset CA L3 interrupt
                  pthread_mutex_lock(&r77_lock);
                  Eregs_Inp[0x77] &= ~0x0028;  // Reset CA L3  interrupt
                  pthread_mutex_unlock(&r77_lock);
                  CA1_IS_req_L3 = OFF;
                  CA1_DS_req_L3 = OFF;
               }
               if (Eregs_Out[0x57] & 0x0020) { // Reset CA L1 interrupt
                  Eregs_Inp[0x76] &= ~0x0400;  // Reset CA L1  interrupt
               }
               if (Eregs_Out[0x57] & 0x0008) { // Test for CA select
                  Eregs_Inp[0x55] |= 0x0001;   // Select CA1
                  Eregs_Inp[0x55] &= ~0x0002;  // deselect CA2
               } else {
                  Eregs_Inp[0x55] |= 0x0002;   // Select CA2
                  Eregs_Inp[0x55] &= ~0x0001;  // deselect CA1
               }
               if (Eregs_Out[0x57] & 0x0100) { // Test for IPL required
                  Eregs_Inp[0x53] |= 0x0200;   // Set not initialized sense
                  Eregs_Out[0x53] |= 0x0200;   // Set not initialized sense
               }
               if (Eregs_Out[0x57] & 0x0200) { // Test for IPL unit exception
                  if (Eregs_Out[0x57] & 0x0008)
                     iobs[0]->IPL_exception = ON;
                  else
                     iobs[1]->IPL_exception = ON;
               }
               if (!(Eregs_Out[0x57] & 0x0200)) {    // Test for reset IPL unit exception
                  if (Eregs_Out[0x57] & 0x0008)
                     iobs[0]->IPL_exception = OFF;
                  else
                     iobs[1]->IPL_exception = OFF;
               }
               if (Eregs_Out[0x57] & 0x0004) {
                  Eregs_Inp[0x55] &= ~0x0010;        // Reset reset flag
               }
               if (Eregs_Out[0x57] & 0x0002) {
                  Eregs_Inp[0x55] &= ~0x0020;        // Reset channel stop
               }
               if ((Eregs_Out[0x57] & 0x0800) &&     // If Unit Exception latch on and ...
                  ((Eregs_Out[0x57] & 0x0100) ||     //  not initialized or...
                   (Eregs_Out[0x57] & 0x0001))) {    // in diagnostic mode
                  Eregs_Inp[0x54] |= 0x0200;         // Set Unit Check latch
               }
               if ((Eregs_Out[0x57] & 0x0001) &&
                  !(Eregs_Inp[0x55] & 0x8000))  {
                  Eregs_Inp[0x55] |= 0x8000;         // Diagnostic wrap mode on
                  Eregs_Inp[0x55] &= ~0x0100;        // CA not active
               }
               if (!(Eregs_Out[0x57] & 0x0001) &&
                  (Eregs_Inp[0x55] & 0x8000))  {
                  Eregs_Inp[0x55] &= ~0x8000;        // Diagnostic wrap mode off
                  Eregs_Inp[0x55] |= 0x0100;         // CA active
               }
            }

            //********************************************************
            //          Channel Adaptor Type 1 updates
            //********************************************************
            if (Efld == 0x62) {
               Eregs_Inp[0x62] &= ~0x0100;     // Reset PCI interrupt

               if (Eregs_Out[0x62] & 0x0400) { // Reset CA1 L3 interrupts
                  pthread_mutex_lock(&r77_lock);
                  Eregs_Inp[0x77] &= ~0x0008;  // Reset L3 initial selection
                  pthread_mutex_unlock(&r77_lock);
                  CA1_IS_req_L3 = OFF;
                  Eregs_Inp[0x60] &= ~0x8200;  // Reset NSC status bits
               }
               if (Eregs_Out[0x62] & 0x0200) { // Reset CA1 L3 data service
                  pthread_mutex_lock(&r77_lock);
                  Eregs_Inp[0x77] &= ~0x0010;  // Reset L3 data service
                  pthread_mutex_unlock(&r77_lock);
                  CA1_DS_req_L3 = OFF;
               }
               if (Eregs_Out[0x62] & 0x1000)
                  Eregs_Inp[0x62] |= 0x1000;   // Set NSC Channel end
               else
                  Eregs_Inp[0x62] &= ~0x1000;  // Reset NSC Channel end

               if (Eregs_Out[0x62] & 0x0800)
                  Eregs_Inp[0x62] |= 0x0800;   // Set NSC Final status
               else
                  Eregs_Inp[0x62] &= ~0x0800;  // Reset NSC Final status
            }

            //********************************************************
            //          Remote program Loader updates
            //********************************************************
            if (Efld == 0x6B) {                // RPL - Cntl load pgm reg
               Eregs_Inp[0x6B] = Eregs_Out[Efld] & 0xFFFF;  //
            }

            //********************************************************
            //          CCU updates
            //********************************************************
            if ((Efld == 0x70) && (bypass_CCU_check == OFF)) {      // HARD STOP
               printf("\nDisplay Reg 1: %05X\n\r", Eregs_Out[0x71]);
               printf(  "Display Reg 2: %05X\n\r", Eregs_Out[0x72]);
               pgm_stop = ON;
               reason = SCPE_STOP;
               continue;
            }
            if (Efld == 0x77) {                // Miscellaneous Control
               w_byte = Eregs_Out[Efld];
               if (w_byte & 0x8000)  {         // Reset IPL L1 ?
                  Eregs_Inp[0x53] &= ~0x0200;  // Reset not-initialized flag
                  ipl_req_L1 = OFF;
               }
               if (w_byte & 0x4000)            // Reset CCU Checks
                  IO_par_chk = CS2_L1_chk = CS3_L1_chk = CS4_L1_chk = OFF;
               if (w_byte & 0x0004)            // Reset all L1 prgm checks
                  IO_par_chk = IO_L5_chk = OP_reg_chk = adr_ex_chk = OFF;
               if (w_byte & 0x2000)  {         // Reset Panel Interrupt L3 ?
                  pthread_mutex_lock(&r7f_lock);
                  Eregs_Inp[0x7F] &= ~0x0200;  // Reset L3 Interval Timer
                  pthread_mutex_unlock(&r7f_lock);
                  timer_req_L3 = OFF;
               }
               inter_req_L3 = OFF;
               if ((w_byte &0x0200) && (test_mode))  // Set Diagnostic mode L2 ?
                  diag_req_L2 = ON;
               if ((w_byte &0x0100) && (test_mode))  // Reset Diagnostic mode L2 ?
                  diag_req_L2 = OFF;
               if (w_byte & 0x0040)  {         // Reset Interval Timer L3 ?
                  pthread_mutex_lock(&r7f_lock);
                  Eregs_Inp[0x7F] &= ~0x0004;  // Reset L3 Interval Timer
                  pthread_mutex_unlock(&r7f_lock);
                  timer_req_L3 = OFF;
               }
               if (w_byte & 0x0020)            // Reset PCI L3 ?
                  pci_req_L3 = OFF;
               if (w_byte & 0x0002)            // Reset PCI L4 ?
                  pci_req_L4 = OFF;
               if (w_byte & 0x0001)            // Reset SVC L4 ?
                  svc_req_L4 = OFF;
            }
            if (Efld == 0x79) {                // Utility Control
               if (!(Eregs_Out[Efld] & 0x0400)) { // Inhibit bit PL5 C&Z flag off ?
                  if (Eregs_Out[Efld] & 0x0200)   // Prog L5 C flag
                     CL_C[3] = ON;
                  else
                     CL_C[3] = OFF;
                  if (Eregs_Out[Efld] & 0x0100)   // Prog L5 Z flag
                     CL_Z[3] = ON;
                  else
                     CL_Z[3] = OFF;
               }
               if (Eregs_Out[Efld] & 0x0040)   // Reset load state
                  load_state = OFF;
               if (Eregs_Out[Efld] & 0x0020)   // Set test mode
                  test_mode = ON;
               if (Eregs_Out[Efld] & 0x0002)   // Set test mode
                  test_mode = ON;
               if (Eregs_Out[Efld] & 0x0010)   // Reset test mode
                  test_mode = OFF;
               if (Eregs_Out[Efld] & 0x0008) { // Set bypass CCU Check
                  if (test_mode == ON)
                     bypass_CCU_check = ON;
               }
               if (Eregs_Out[Efld] & 0x0004)   // Reset bypass CCU Check
                  bypass_CCU_check = OFF;
               if (Eregs_Out[Efld] & 0x1000)   // Set FET Storage Diagnostocs
                  FET_stor_diag = ON;
               else
                  FET_stor_diag = OFF;         // Reset FET storage Diagnostics
            }

            if (Efld == 0x7A) {                // CUCR reset
               Eregs_Inp[0x7A] = 0x8000;
            }
            if (Efld == 0x7C) {                // Program Call Interrupt L3
               pci_req_L3     = ON;
            }
            if (Efld == 0x7D) {                // Program Call Interrupt L4
               pci_req_L4     = ON;
            }
            if (Efld == 0x7E) {                // Set interrupt mask bits
               w_byte = Eregs_Out[Efld];
               if (w_byte & 0x0020)            // Level 2 ?
                  int_lvl_mask[2] = ON;
               if (w_byte & 0x0010)            // Level 3 ?
                  int_lvl_mask[3] = ON;
               if (w_byte & 0x0008)            // Level 4 ?
                  int_lvl_mask[4] = ON;
               if (w_byte & 0x0004)            // Level 5 ?
                  int_lvl_mask[5] = ON;
            }
            if (Efld == 0x7F) {                // Reset interrupt mask bits
               w_byte = Eregs_Out[Efld];
               if (w_byte & 0x0020)            // Level 2 ?
                  int_lvl_mask[2] = OFF;
               if (w_byte & 0x0010)            // Level 3 ?
                  int_lvl_mask[3] = OFF;
               if (w_byte & 0x0008)            // Level 4 ?
                  int_lvl_mask[4] = OFF;
               if (w_byte & 0x0004)            // Level 5 ?
                  int_lvl_mask[5] = OFF;
            }
         }
         break;
   }

   switch (opcode & 0xF8F0) {
      case (0xB800):
         /* BAL  R,A            [RA]  */
         /* 01234567 89012345 ... 901
            10111RRR 0000A<-- // -->A */
         Grp = RegGrp(lvl);
         Rfld = (opcode0) & 0x07;              /* Extract register nr */
                                               /* Get branch addr from memory */
         Afld = (opcode1 & 0x03) << 16;        /* Xbyte EA18 */
         Afld = Afld | (GetMem(PC) << 8);      /* Read 3rd & 4th byte */
         PC = (PC + 1) & AMASK;
         Afld = Afld |  GetMem(PC);
         PC = (PC + 1) & AMASK;

         if (Rfld > 0)                         /* No link addr if R=0 */
            GR[Rfld][Grp] = PC;                /* Store link address */
         GR[0][Grp] = Afld;                    /* Unconditional branch */
         break;

      case (0xB820):
         /* LA   R,A            [RA]  */
         /* 01234567 89012345 ... 901
            10111RRR 0010A<-- // -->A */
         Grp = RegGrp(lvl);
         Rfld = (opcode0) & 0x07;              /* Extract register nr */
                                               /* Get load address from memory */
         Afld = (opcode1 & 0x03) << 16;        /* Xbyte EA18 */
         Afld = Afld | (GetMem(PC) << 8);      /* Read 3rd & 4th byte */
         PC = (PC + 1) & AMASK;
         Afld = Afld |  GetMem(PC);
         PC = (PC + 1) & AMASK;
         GR[0][Grp] = PC;                      /* Update IAR */
         GR[Rfld][Grp] = Afld;                 /* Load R with 16 bit address */
         break;
   }

   if (opcode == 0xB840) {
      /* EXIT                EXIT  */
      /* 01234567 89012345
         10111000 01000000         */

      int_lvl_ent[lvl] = OFF;                  /* Reset current active PGM level */
      if (lvl == 5) {                          /* An EXIT while in L5 triggers SVC L4 */
         svc_req_L4 = ON;
      }
      if (debug_reg & 0x02)
         fprintf(trace, "%s >>> Leaving lvl=%d \n", TimeStamp(),lvl);
   }
}  // end while (reason == 0)

//###################### END OF SIMULATOR WHILE LOOP ######################

PC = saved_PC;
/* Simulation halted */
return (reason);
}

//********************************************************
// Sub-routines used by the simulator
//********************************************************

/*** Select register group ***/

int32 RegGrp(int32 level)
{                            // Lvl 1 => Reg Grp 0
   if (level == 1)           // Lvl 2 => Reg Grp 0
      return(0);             // Lvl 3 => Reg Grp 1
   else                      // Lvl 4 => Reg Grp 2
      return(level - 2);     // Lvl 5 => Reg Grp 3
}

/*** Fetch a byte from memory ***/

int32 GetMem(int32 addr)
{
   if (addr > MEMSIZE) {
      adr_ex_chk = ON;      // Addressing Exception ?
      printf("AE at addr %d, MEMSIZE %d \n\r", addr, MEMSIZE);
   }
   else
      return(M[addr] & 0xFF);
}

/*** Place a byte in memory ***/

int32 PutMem(int32 addr, int32 data)
{
   if (addr > MEMSIZE) {
      adr_ex_chk = ON;       // Addressing Exception ?
      printf("AE at addr %d, MEMSIZE %d \n\r", addr, MEMSIZE);
   }
   else
      M[addr] = data & 0xFF;
   return 0;
}

/*** Memory examine ***/

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw) {
   if (addr >= MEMSIZE) return SCPE_NXM;
   if (vptr != NULL) *vptr = M[addr] & 0xff;
   return SCPE_OK;
}

/*** Memory deposit ***/

t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw) {
   if (addr >= MEMSIZE) return SCPE_NXM;
   M[addr] = val & 0xFF;
   return SCPE_OK;
}

t_stat cpu_set_size (UNIT *uptr, int32 val, char *cptr, void *desc) {
   int32 mc = 0;
   uint32 i;
   if ((val <= 0) || (val > MAXMEMSIZE) || ((val & 0x0FFF) != 0))
      return SCPE_ARG;
   for (int i = val; i < MEMSIZE; i++) mc = mc | M[i];
   if ((mc != 0) && (!get_yn ("Really truncate memory [N]?", FALSE)))
      return SCPE_OK;
   MEMSIZE = val;
   for (int i = MEMSIZE; i < MAXMEMSIZE; i++) M[i] = 0x00;
   return SCPE_OK;
}

/*** BOOT/LOAD procedure ***/

t_stat cpu_boot (int32 unitno, DEVICE *dptr) {    /* LOAD pressed */
   //******************************************************************
   // IPL phase 2 - Transfer miniROS to core storage
   //******************************************************************
   int32 addr, temp;

   load_state = ON;

char miniROS[] = {
   /*000   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x70\x04\xF6\xFF\x98\xB8\x80\xCE\x81\x0B\x86\xFF\x87\xFF\xA8\xA2"
   "\x00\x82\x00\x14\x01\x86\x00\x24\x02\x8A\x00\x34\x03\x8E\x00\x44"
   "\x04\x92\x00\x54\x05\x96\x00\x64\x06\x9A\x00\x74\x07\x9E\xA8\x04"
   "\x70\x04\x00\x00\x80\x00\x98\x09\x81\x00\x98\x0D\xD1\x00\x91\x00"
   "\xF1\xFF\x98\x15\xD1\xFF\x91\xFF\xF1\x01\x98\x1D\xF0\x01\x88\x21"
   "\xD0\xFF\x90\xFF\x98\x02\xFF\xFF\x80\x00\xD0\x00\x90\x00\xF0\xFF"
   "\x98\x33\x77\xC8\x71\x57\x05\x1C\x01\x34\x53\xC8\x98\x3F\x73\xC8"
   "\x98\x43\x17\xC8\x03\x54\x01\x3C\x15\xC8\x98\x4D\x80\x07\x17\x85"
   "\x13\x05\x73\xC8\x98\x57\x15\x85\x13\x05\x53\xC8\x98\x5F\x17\x86"
   "\x13\x05\x53\xC8\x98\x67\x15\x07\x75\xC8\x98\x6D\x15\x86\x07\x07"
   "\x75\xC8\x98\x75\x80\xCE\x81\x06\x83\xB4\xA8\x06\x91\x80\x98\xAF"
   "\xD0\x0E\x31\x81\xCE\x06\xF6\xFF\x98\x02\x88\x11\x70\x04\x81\x74"
   "\x83\xC8\x91\x10\xF8\x86\x31\x81\x00\x84\xA8\x0B\xD8\x84\x80\x10"
   "\xA8\x0D\x71\x9C\xF9\x82\xAE\x24\x71\xDC\x01\x85\x71\x6C\x01\x83"
   "\x71\xEC\x01\x87\xF9\x02\x70\x04\x33\xC8\x81\x10\x71\x94\x81\x08"
   "\x61\x74\x61\x7C\xE9\x02\xA8\x11\x86\x02\xE9\xB4\x87\x06\x81\x60"
   /*100   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x61\x74\x82\x0F\x71\x6C\xE8\x88\x71\x7C\xD9\x90\xE9\x36\xA8\x0D"
   "\x86\x12\x61\x7C\xE9\x9E\x81\x20\x61\x74\xA8\x08\x67\x64\x61\x7C"
   "\x61\x34\x63\x24\x71\x6C\xE8\x99\x71\x7C\xE9\x18\xD9\xFC\xA8\x0D"
   "\x71\x7C\xE9\x04\x87\x0E\xA8\x3B\x61\x1C\x65\x7C\x81\x00\x85\x00"
   "\x15\xC8\x98\x11\x82\x06\x61\x0C\xF8\xD8\xC8\xCC\xF8\x31\xE8\xB6"
   "\xD8\xAA\xC8\x08\x86\x12\x87\x0E\xD2\x08\xA8\x41\x61\x1C\x80\xFF"
   "\x91\xF6\x98\x14\x91\x01\x98\x0C\x91\x03\x98\x0C\x91\x01\x98\x58"
   "\x91\x01\x98\x1C\x86\x02\xA8\x23\x86\x82\xA8\x27\x86\x22\xA8\x63"
   "\x86\x22\x87\x02\xA8\x2F\xD8\x89\x61\x1C\xF1\xFF\x88\x2E\xA8\x28"
   "\x67\x44\xD2\x80\x83\x01\xA8\x7B\x61\x1C\x91\xFB\xF1\xFF\x88\x44"
   "\xA8\x2F\x86\x02\x81\x10\x61\x74\xA8\x87\x82\x02\x61\x2C\xD9\x8C"
   "\xE8\x92\xC9\x0C\xE8\x21\xC8\x9C\x87\x0C\xA8\x65\xD2\x08\xA8\xA3"
   "\x86\x22\xA8\x6F\xC8\x9E\xA8\x55\x84\x04\x85\x00\x55\x83\xD2\x40"
   "\x83\x02\xA8\xB7\xEC\x08\x61\x4C\x51\x81\x95\x02\xA8\x11\x86\x02"
   "\x87\x0F\xA8\x8D\x80\x04\x81\x02\x11\x01\x90\x04\x51\xC8\x98\x13"
   "\x80\xC0\x71\x74\x80\x12\x61\x24\x80\x01\x81\xFE\x10\x01\x04\x04"
   };

char maxiROS[] = {
   /*000   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x70\x04\xF6\xFF\x98\xB8\x80\xCE\x81\x0B\x86\xFF\x87\xFF\xA8\xA2"
   "\x00\x82\x00\x14\x01\x86\x00\x24\x02\x8A\x00\x34\x03\x8E\x00\x44"
   "\x04\x92\x00\x54\x05\x96\x00\x64\x06\x9A\x00\x74\x07\x9E\xA8\x04"
   "\x70\x04\x00\x00\x80\x00\x98\x09\x81\x00\x98\x0D\xD1\x00\x91\x00"
   "\xF1\xFF\x98\x15\xD1\xFF\x91\xFF\xF1\x01\x98\x1D\xF0\x01\x88\x21"
   "\xD0\xFF\x90\xFF\x98\x02\xFF\xFF\x80\x00\xD0\x00\x90\x00\xF0\xFF"
   "\x98\x33\x77\xC8\x71\x57\x05\x1C\x01\x34\x53\xC8\x98\x3F\x73\xC8"
   "\x98\x43\x17\xC8\x03\x54\x01\x3C\x15\xC8\x98\x4D\x80\x07\x17\x85"
   "\x13\x05\x73\xC8\x98\x57\x15\x85\x13\x05\x53\xC8\x98\x5F\x17\x86"
   "\x13\x05\x53\xC8\x98\x67\x15\x07\x75\xC8\x98\x6D\x15\x86\x07\x07"
   "\x75\xC8\x98\x75\x80\xCE\x81\x06\x83\xB4\xA8\x06\x91\x80\x98\xAF"
   "\xD0\x0E\x31\x81\xCE\x06\xF6\xFF\x98\x02\x88\x11\x70\x04\x81\x74"
   "\x83\xC8\x91\x10\xF8\x86\x31\x81\x00\x84\xA8\x0B\xD8\x84\x80\x10"
   "\xA8\x0D\x71\x9C\xF9\x82\xAE\x24\x71\xDC\x01\x85\x71\x6C\x01\x83"
   "\x71\xEC\x01\x87\xF9\x02\x70\x04\x11\xC8\x80\x40\x71\x74\x75\xC8"
   "\x35\xC8\x33\xC8\x73\x14\x73\x24\x83\x10\x73\x94\x77\xC8\x87\x88"
   /*000   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x57\x74\x77\xC8\x71\x6C\xE8\xA0\x71\x7C\xE9\x06\x83\x04\x73\x24"
   "\xA8\x17\x51\x5C\xF9\x86\x83\x01\x73\x24\xA8\x21\x51\x8C\xE9\x0E"
   "\xE9\x8C\x83\x02\x73\x24\xA8\x0D\x87\x28\x57\x74\xA8\x33\x51\x5C"
   "\xF8\x84\x86\x19\xA8\x1E\x51\xCC\xF9\x84\x86\x59\xA8\x16\x01\x01"
   "\xF8\x89\x51\x5C\xD9\x84\xD9\x0F\xA8\x10\xD7\x3E\xD6\x01\xA8\x0A"
   "\xD7\x3A\xA8\x06\x51\x5C\xD9\x8F\xD9\x0B\x11\xC8\x01\x81\xD7\x38"
   "\xF6\xFF\x88\x04\x57\x74\x77\xC8\x83\x08\x73\x24\x71\x6C\xE8\xA5"
   "\x71\x7C\xE9\x02\xA8\x0F\x51\x5C\xD9\xB1\xD9\x4C\x51\xCC\xF9\x82"
   "\xA8\x37\x80\x01\x81\xFA\x01\x81\x51\x04\x82\x04\x83\x02\x33\x81"
   "\x87\x18\x57\x74\x73\x24\x71\x6C\xE8\xCF\x71\x7C\xE9\x02\xA8\x0D"
   "\x51\x5C\xD9\xDB\xD9\x02\xA8\x48\x11\xC8\x71\x24\x51\x2C\x82\xFF"
   "\x83\xFF\x90\xFC\x31\xC8\x82\x04\x83\x02\x33\x01\x93\x01\x31\xC8"
   "\x88\x0A\x11\xC8\x80\x01\x51\x44\x86\x59\xA8\x7D\x51\x9C\x91\x01"
   "\x92\x04\x31\xC8\x98\x15\x01\x81\x75\xC8\x15\xC8\x80\x01\x81\xF8"
   "\x13\x01\x35\xC8\x88\x04\x70\x04\x00\x00\x84\x80\x75\x74\x10\x07"
   "\xD6\x58\xA8\xA9\xA8\x00\xA8\x00\xFF\x2F\x8F\xF8\x04\x00\x04\x04"
   };

char LPG1[] = {
  // Dummy ROS to start LPG1 after a POR.
  /*0000   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x70\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\xB8\x20\x04\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  //       Track 1 content of the RIPL diskette
  /*0040   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x08\x40\x08\x42\x11\xC1\x80\x28"   // CDS entries 1-8
   "\x00\x00\x00\x00\x00\x20\xC0\x28"   // See SY27-0208_3705-80_MLM_R01
   "\x00\x00\x00\x00\x00\x20\xC0\x28"   // Page RPL DIAG 200
   "\x00\x00\x00\x00\x00\x20\xC0\x28"
   "\x00\x00\x00\x00\x00\x20\xC0\x28"
   "\x00\x00\x00\x00\x00\x20\xC0\x28"
   "\x00\x00\x00\x00\x00\x20\xC0\x28"
   "\x00\x00\x00\x00\x00\x20\xC0\x28"
  /*0080   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\xAB\xB6\xA8\x2A\x70\x04\x70\x04\xB9\x20\x00\x04\x61\x84\x27\x0D"
   "\x97\x02\x00\x7C\xB9\x20\x00\x04\x21\xB1\x81\x11\x07\x0C\xA8\xF8"
   "\xA8\x02\xAB\x5C\xB9\x20\x00\x04\x21\xB3\x21\x0B\x21\x89\xB9\x20"
   "\x00\x01\x21\xB1\x21\x02\x21\x86\xB9\x20\x00\x06\x07\x0C\xA8\xCC"
   "\xA8\x96\xBF\x20\x00\x10\x77\x94\xBB\x20\x80\x00\x73\x74\x61\xBC"
   "\xC8\x02\xA8\x14\x21\x13\xC0\xF6\x88\x0E\x03\x05\x25\x11\x88\x04"
   "\xD3\x08\xA8\x02\xD3\x10\x03\x85\x80\x08\xBF\x20\x00\x40\x74\x88"
   "\x73\x07\xCA\xC0\x81\x00\x75\x01\x43\x05\xFC\xC6\xFC\x4C\xDA\xB4"
  /*0100   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\xF2\x01\x88\x50\xBB\x20\x00\x28\x73\x94\xA8\x00\x45\x04\xBB\x20"
   "\x40\x06\x43\x34\xBD\x20\xFF\x00\x73\xDC\xDA\x9E\x45\xE4\x84\x04"
   "\x33\xC8\x43\xD4\xBC\x85\xBB\x20\x00\x14\x73\x94\xC9\x06\x81\x80"
   "\x75\x03\xA8\x3D\x97\x08\xB8\xC9\x20\x02\xBB\x20\x40\x04\x73\x74"
   "\xA8\x1D\xFC\x0C\xDA\x13\xFA\x45\xA8\x0A\xCA\x99\xEA\xCB\xA8\x04"
   "\xCA\x1F\xEA\x51\x70\x04\xA8\x05\x21\x13\xC0\x00\x98\x0C\xB9\x20"
   "\x22\x22\x71\x14\x71\x24\x70\x04\xA8\x05\x23\x0B\x23\xAF\x81\x14"
   "\x07\x0C\xAA\x76\x21\x2F\xF1\x08\x98\xC3\xB9\x20\x00\x0C\xA8\xC5"
   "\x00\x00\x00\x00\x00\x00\xAC\x18\x00\x00\x00\x00\xBB\x20\x03\xF8"
   "\xBD\x20\x21\x06\x35\x81\x20\xA7\x27\xA5\x21\x8B\x80\xAA\x71\x14"
   "\x07\x0C\xAA\x4E\xA8\x38\x25\x63\xFC\x9C\x27\x25\x97\x04\x27\xAD"
   "\x23\x27\xFB\x90\xBD\x20\x03\xF8\xBB\x20\x21\x6B\x53\x81\xB9\x20"
   "\x00\x01\x21\xA7\x20\x2D\x23\x27\xFB\x8E\x23\x6D\x93\x01\x25\x15"
   "\x53\xC8\x21\x02\x13\x98\x23\x86\x21\x0B\x91\x01\xA8\x45\x21\x13"
   "\x90\x01\x21\x93\x21\x31\x80\xEE\x71\x24\x27\x25\x97\x02\xA8\x43"
   "\x00\x00\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\xF2\x62"
  /*0200   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  /*0300   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xB8\x20\x01\x74\x00\x00"
   "\x00\x00\xB8\x20\x01\xA6\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  /*0400   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x11\xC8\x71\x14\x71\x24\x20\xB1\xA8\x00\x20\xB3\xB9\x20\x20\x00"
   "\x71\x74\xB9\x20\x03\xE8\xBB\x20\xAB\x63\x13\x81\xB9\x20\x00\x01"
   "\x21\xA7\xB9\x20\x00\x20\x10\x81\xBB\x20\x21\x05\x13\x83\x73\x9C"
   "\x13\x8B\x73\x4C\x13\x8E\x03\x02\x13\x92\x61\xBC\x17\x88\x33\xC8"
   "\x63\xB4\x65\xBC\x35\xC8\x88\x0C\xB9\x20\x6B\x6B\x71\x14\x71\x24"
   "\x70\x04\xA8\x05\xCA\x04\x82\xF8\xA8\x1B\x67\xB4\x86\x06\x71\x0C"
   "\x81\x00\x11\x98\xBE\x85\xA0\x20\x21\x82\x21\x86\x67\xBC\xCE\x0E"
   "\x71\x1C\xBB\x20\xBB\xBB\x31\xC8\x9B\xCD\xD6\x80\x67\xB4\xBB\x20"
   "\x00\x18\x73\x34\x71\x0C\x82\x06\xBD\x20\x00\x20\xF9\x86\x82\x02"
   "\xBD\x20\x02\x00\x25\xA9\x21\x02\x11\xF8\xBA\x85\x21\xAB\x26\x29"
   "\x23\x2B\xD3\x10\xB9\x20\x01\x80\x84\x04\x73\x34\x77\x3C\x17\x30"
   "\x63\x90\xBC\x8B\x84\x04\x23\x2B\xD3\x18\x73\x34\x63\x98\xBC\x87"
   "\xBF\x20\x08\x00\x77\x34\x77\x3C\x17\x30\xBF\x20\x08\x08\x77\x34"
   "\xAC\x3F\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
  /*0500   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\xBA\x20\x06\x00\xB9\x20\x00\x02\x21\xB1\x27\x33\xEF\x88\xB9\x20"
   "\x00\x0F\x07\x0C\xAC\x1F\x22\xC8\xBF\x20\x01\x88\x70\x02\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04\x70\x04"
   "\xE5\xD3\x7E\xF2\xF0\x40\xC6\xD9\xD7\xD3\xD3\xD7\xC7\xF1\x48\x80"
   };

char LPG2[] = {
  //       Track 6 content of the RIPL diskette
  /*0000   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\xB9\x20\xFC\x00\x71\x14\x11\xA8\x71\x24\xBD\x20\x06\x28\x50\x98"
   "\xD9\xD6\xC7\xD9\xC1\xD4\x40\xF2\x40\xD4\xD6\xC4\x40\xC9\xC9\x40"
   "\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\xC1\xC3\xC6"
   "\xD9\xF3\x5C\x5C\x5C\x40\x40\x40\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  /*0100   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  /*0200   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  /*0300   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\xBD\x20\x14\xD4\x05\x98\x25\xC6\x25\xB2"
   "\x95\x0D\x45\x92\xA5\x0D\x61\x0E\x21\xC2\x50\x81\x50\x83\x71\x0C"
   "\xA8\x00\x73\x9C\xF3\x08\x98\x06\xB9\x20\x3F\x06\x60\x02\x11\xF8"
   "\x11\xF8\x58\x81\x59\x82\xBB\x20\x07\x80\x80\x08\x95\x04\x32\x02"
   "\x50\x81\x52\x82\x93\x04\x95\x04\xB8\x8D\xBB\x20\x01\xB8\x03\x98"
   "\x80\x18\x32\x02\x50\x81\x52\x82\x93\x04\x95\x04\xB8\x8D\xB9\x20"
   "\x01\x16\x41\xA1\x47\x8E\x80\x00\x81\x10\x77\x0C\xF7\x01\x98\x12"
   "\xBF\x20\x02\x00\x71\x34\x73\x3C\x53\x30\x71\x90\x98\x02\xA8\x0D"
   "\xA8\x16\xBF\x20\x00\x20\x71\x34\x73\x3C\x53\x30\x71\x98\xD8\x02"
  /*0400   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\xA8\x0D\x41\x21\x91\x80\x41\xA1\x47\x0E\x45\x8E\xA5\x04\xB9\x20"
   "\x01\x80\x13\x01\x53\x81\x13\x03\x53\x83\x45\x0E\x80\x00\x81\x00"
   "\x71\x34\x73\x3C\x53\x30\x90\x02\xD8\x82\xA8\x0D\xBB\x20\x08\x08"
   "\x73\x34\x61\xBC\x61\x58\x51\x81\x62\x66\xBD\x20\x07\xD8\x50\x98"
   "\x4C\x27\xB4\x07\x88\x0C\xB4\x08\x89\x16\xA9\x0A\xBD\x20\x07\xC6"
   "\x50\x98\x4C\x10\x88\x0E\xB9\x20\x10\x02\x41\xB1\x40\xB3\x61\x42"
   "\x15\x40\xA8\xF0\xBB\x20\x13\xE6\x03\x98\xB9\x20\x00\x0C\x45\x21"
   "\x51\xB8\x88\x02\xA8\x06\xBD\x20\x02\x00\xA8\x1A\xB9\x20\x00\x0E"
   "\x51\xB8\x88\x02\xA8\x04\x35\x05\xA8\x0C\xB9\x20\x10\x02\x41\xB1"
   "\x40\xB3\x61\x42\x15\x40\x45\xAD\x95\x06\x45\xA1\x31\x02\x41\x8A"
   "\x21\xB2\x03\x88\x83\x00\xE2\xE0\x45\x2D\x15\x98\xA5\x01\x53\xB8"
   "\x98\x02\xA8\x9C\x92\x20\xA3\x01\x53\xB8\x98\x02\xA8\x0E\xB9\x20"
   "\x08\x0A\x41\xB1\x40\xB3\x61\x42\x15\x40\xA8\x84\xA2\x20\x93\x01"
   "\xFE\x64\x41\x8E\x81\x4A\xBA\x20\x19\xAC\x02\x98\x29\x81\x41\x0E"
   "\x42\x8E\xBA\x20\x00\x16\x02\x98\x21\x82\x23\x86\x24\x8A\x25\x8E"
   "\x26\x92\x27\x96\xB9\x20\x01\x88\x12\x82\xB8\x20\x01\x86\xB9\x20"
   "\x00\x00\xBB\x20\x00\x00\xBC\x20\x00\x00\xBD\x20\x00\x00\xBE\x20"
   "\x00\x00\xBF\x20\x00\x00\xD6\x02\x47\xB9\x22\xE8\x98\x0E\x62\x66"
   "\xB9\x20\x08\x1C\x41\xB1\x40\xB3\x61\x42\x15\x40\x42\x0E\x87\x00"
   "\x2F\x81\x47\x39\x62\x66\x31\xB8\x98\x02\xA8\x0C\x31\xA8\x21\xB9"
   "\x29\x00\xD1\x08\x29\x80\xA8\x08\xE0\x3F\xA0\x10\x21\xB2\x20\xB1"
   "\x95\x01\x45\x8A\xA9\x0B\x41\x08\xBD\x20\x10\xEC\x50\x98\xA9\x15"
   "\xDE\x82\xA8\x08\xBD\x20\x11\x14\x50\x98\xA8\x00\xD6\x10\xB9\x20"
   "\x00\x06\x41\xA1\xA9\x2B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\xBE\x20\x10\xF6\x06\x98\xB9\x20"
   "\x00\x10\xBA\x20\x10\x04\x12\x81\x91\x02\xBA\x20\x18\x38\x02\x98"
   "\xBB\x20\xB8\x20\x13\x81\x12\x82\xB9\x20\x00\x80\xBA\x20\x41\x0C"
   "\x12\x81\x91\x02\xBA\x20\x07\x96\x02\x98\x13\x81\x12\x82\xB9\x20"
   "\x01\x00\xBA\x20\x73\xFC\x12\x81\x91\x02\xBA\x20\x14\xE4\x02\x98"
   "\x13\x81\x12\x82\xB9\x20\x8C\x00\xBD\x2F\xFE\xEA\x05\x98\xBB\x20"
   "\x00\x02\x03\x98\x38\x81\x02\x0C\x52\x82\x95\x04\x90\x10\x98\x02"
   "\xA8\x0F\xD9\x86\x81\x12\x39\x80\xA8\x17\x84\x20\x01\x88\xE0\xE0"
   "\x81\x00\x63\x88\x32\x02\x12\x98\x32\x82\x93\x04\xBC\x8B\xB9\x20"
   "\x00\x40\x64\x7E\x65\x7E\x95\x40\x45\xA8\x16\x10\x46\x30\xBD\x87"
   "\x34\x88\x61\x66\x82\xC0\x10\x81\x91\x02\xBA\x87\x4A\x81\x33\xA8"
   "\x61\x7E\x18\x14\xE0\x0F\x88\x02\xA8\x04\x40\xAF\xA8\x0A\xBD\x20"
   "\x02\x58\x53\x98\xB8\x85\x43\xAF\xB9\x20\x00\x10\x71\xF4\x77\xA8"
   "\x4E\x88\x65\x7E\x61\xBC\xC8\x82\xA8\x04\x03\x61\xA8\x12\xC8\x02"
   "\xA8\x0C\x03\x1E\xFA\x02\xA8\x02\xD6\x02\x03\x9C\xA8\x02\x33\xA8"
   "\x43\xB7\xD8\x82\xD6\x80\xB9\x20\xFC\x80\x71\x14\xB9\x20\x00\x28"
   "\x71\x94\xCE\x02\xA9\xB2\x47\xB9\x67\x7E\x97\x1C\xB9\x20\x08\x40"
   "\x41\x04\x5B\x14\xE3\xF0\x33\x78\x33\x78\x32\x88\x43\x14\x5A\x04"
   "\xE2\x0F\x89\x76\x5B\x0C\xD2\x80\x22\x78\x98\x0E\xF2\xF0\x88\x2E"
   "\x33\x78\x33\x78\x90\x01\x97\x08\xA8\x13\x41\x04\xBD\x20\x40\x06"
   "\x45\x34\x75\xDC\xDC\x90\x4D\x00\x25\x58\x4D\x80\x43\x24\x25\x88"
   "\x7C\x00\x45\x14\xA8\x27\xBD\x20\x40\x04\x75\x74\xA8\x2F\x47\x39"
   "\xBD\x20\x00\x10\x75\x94\x65\x7E\x49\x00\xE1\x78\x88\x79\xA8\x00"
   "\x51\x07\xC8\xCA\x51\x01\x89\x22\xE8\x02\xA9\x1E\xF9\x85\xBA\x20"
   "\x0B\xFF\x21\xB0\x98\x02\xA9\x12\xBB\x20\x00\x48\x03\x98\x43\x8E"
   "\x32\x40\x88\x2A\x51\x03\x89\x02\x43\x0E\x32\x40\x88\x20\x51\x03"
   "\xE8\x02\xA8\xF6\xF9\x85\xBA\x20\x0B\xFF\x21\xB0\x98\x02\xA8\xEA"
   "\xBB\x20\x00\x20\x73\xE4\xBB\x20\x00\x32\x03\x98\x32\x40\x95\x08"
   "\x61\x7E\x91\x40\x4B\x01\x15\xB0\x98\x02\xA8\x02\xA8\x5F\x33\x08"
   "\x88\xBC\x4B\x84\xB8\x40\x4B\x00\xF8\x0A\xF8\x84\xF3\x40\x20\x40"
   "\xF3\x20\x20\x40\xF8\x84\xF3\x10\x20\x40\xF3\x08\x20\x40\x42\x8E"
   "\x62\x66\x4A\x01\x88\x0A\xB9\x20\x00\x2C\x12\x98\xBA\x85\x4A\x01"
   "\x92\x01\x4A\x81\x5B\x05\x2B\x9E\xBB\x20\x01\x64\x03\x98\x23\x96"
   "\x63\x1E\x23\x82\x53\x07\x23\x8D\x83\x9D\x2B\xA4\x53\x01\x23\x87"
   "\x51\x03\x21\x99\x31\xB0\x88\x04\x80\x00\xA8\x02\x80\x01\x28\x80"
   "\x28\x94\x20\x8B\xB9\x20\x00\x1E\x21\x9D\x61\x4A\x21\xA2\x81\x80"
   "\x63\x7E\x35\xB8\x88\x06\x11\x78\x93\x08\xA8\x0B\x29\x8E\x29\xA0"
   "\xA8\x00\x51\x03\x41\x04\xBB\x20\x80\x90\x43\x54\x82\xF4\x43\x44"
   "\x23\x0D\xDA\xC0\x43\x64\xBB\x20\x80\x91\x43\x54\xBB\x20\x00\x20"
   "\x73\xF4\xBB\x20\x02\x00\xBB\x83\x53\x01\x31\xB8\x88\x08\x43\x04"
   "\xBB\x20\x80\x90\x43\x54\xBB\x20\x00\x10\x73\x94\x40\x0E\xB9\x20"
   "\x30\xF0\xA8\x0A\xB9\x20\x30\xF1\xA8\x04\xB9\x20\x30\xF2\x71\x24"
   "\x70\x04\xA8\x07\xEA\x02\xA8\x45\x20\x9D\x20\xA2\xBB\x20\x00\x9A"
   "\x03\x98\x23\x96\x83\x93\xA8\x4F\x01\xEC\x65\x7E\x82\x08\x52\x03"
   "\x12\xB0\x88\x06\x95\x08\xBA\x8B\xA8\x5E\x51\x07\xC8\xDA\x82\x02"
   "\xB9\x20\x00\x28\x71\x94\x45\x8E\x42\x04\xB9\x20\x40\x06\x41\x34"
   "\x71\xDC\xD8\xB8\x21\x88\x32\x88\x65\x7E\x5B\x14\x33\x78\x33\x78"
   "\x59\x0C\xE0\x03\x88\x08\x11\x78\x11\x78\x95\x08\xB8\x89\x41\x24"
   "\x5A\x1C\x43\x14\x23\x88\x45\x0E\x52\x01\xBA\xB5\x63\x7E\xB9\x20"
   "\x08\x40\x41\x04\xB9\x20\x00\x10\x71\x94\xA8\x12\xB9\x20\x40\x04"
   "\x71\x74\xB9\x20\x00\x10\x71\x94\x65\x7E\xD6\x80\xAA\x39\x45\x0E"
   "\x40\x96\x51\x03\xBB\x20\x00\x20\x73\xE4\xBB\x2F\xFE\xAE\x03\x98"
   "\x32\x40\x41\x2F\x41\xB5\x81\x01\x49\x84\xB8\x40\xBB\x20\xFF\x20"
   "\x43\xE4\xBB\x20\x06\x82\xA9\x8E\xB9\x20\x00\x1E\x21\x9D\x61\x4A"
   "\x21\xA2\xBB\x20\x00\x0A\x03\x98\x23\x96\xB9\x20\xF4\x00\x41\x44"
   "\xB8\x40\x82\xF4\x43\x44\x21\x19\x41\x04\xF7\x01\x88\x02\xA8\xE0"
   "\x21\x07\x41\x04\xB9\x20\x00\x44\x01\x98\x21\x82\x20\x9D\x20\xA2"
   "\xB9\x20\x00\x1E\x21\x8B\x61\x4A\x21\x92\x63\x7E\x3C\x04\x21\x07"
   "\xF8\x86\xF8\x0A\xDC\x96\xA8\x0E\xF8\x08\xDC\x10\xA8\x08\xCC\x8C"
   "\xA8\x04\xCC\x08\xA8\x00\x33\xC0\x82\x5B\x43\xF4\x23\x0D\x43\x64"
   "\xBB\x20\x80\x91\x43\x54\xBB\x20\xF4\x00\x43\x44\xAB\xF4\xB9\x20"
   "\x00\x1E\x21\x8B\x61\x4A\x21\x92\xB9\x20\x00\x10\x01\x98\x21\x82"
   "\xBD\x20\x00\x02\xD5\x90\x45\x54\xA8\x00\x43\x44\xAB\xD4\xB9\x20"
   "\x00\x1E\x21\x8B\x61\x4A\x21\x92\xB9\x20\x00\x54\x01\x98\x21\x82"
   "\x63\x7E\x3C\x04\x21\x07\xF8\x86\xF8\x0A\xDC\x90\xA8\x24\xF8\x08"
   "\xDC\x0A\xA8\x1E\xCC\x86\xA8\x1A\xCC\x02\xA8\x16\xBB\x20\xF4\x00"
   "\xBD\x20\x80\x98\x43\x44\xBB\x20\x00\x03\x45\x54\xA8\x00\x43\x64"
   "\xA8\x1E\xBD\x20\xFF\x20\x45\xE4\x55\xC8\x45\x54\xBD\x20\x00\xFF"
   "\x45\xD4\x55\xC8\x45\x84\xBD\x20\x00\x98\x45\x54\x82\xF4\x43\x44"
   "\xAB\x70\xBD\x20\x00\x00\x43\x44\xA8\x00\x45\x54\x63\x1E\x23\x82"
   "\x20\x8B\x20\x92\x23\x0D\xDA\xA6\xB9\x20\x00\x64\x21\x9D\x61\x4A"
   "\x21\xA2\xB9\x20\x00\x1E\x01\x98\x21\x96\xBD\x20\x00\x02\x21\x19"
   "\x41\x04\xD5\x90\x45\x54\xBD\x20\xF4\x00\x45\x44\xAB\x34\xEA\x29"
   "\x20\x9D\x20\xA2\xA8\x25\x20\x9D\x20\xA2\xD7\x80\x29\x20\x48\x3D"
   "\x10\x58\x48\xBD\x61\x06\x21\x96\x63\x7E\x3C\x04\x21\x07\xF8\x86"
   "\xF8\x0A\xDC\x90\xA8\x1E\xF8\x08\xDC\x0A\xA8\x18\xCC\x86\xA8\x14"
   "\xCC\x02\xA8\x10\xBD\x20\x80\x84\xBB\x20\xF4\x00\x43\x44\xA8\x00"
   "\x45\x54\xAA\xEE\xBB\x20\xF4\x00\x43\x44\xBB\x20\x80\x95\x43\x54"
   "\x21\x19\x41\x04\xA9\x9B\xA8\x00\x03\x98\x23\x96\xA8\x00\xBB\x20"
   "\x5B\x00\x43\xF4\x63\x32\x43\x94\x33\xC0\x33\xF8\x33\xF8\x33\xF8"
   "\x33\xF8\xD2\x04\x83\x14\x43\x84\xBB\x20\xF4\x00\x43\x44\xA8\x00"
   "\xAA\xB0\x48\x04\xA0\x01\x88\x02\xA8\x06\xB9\x20\x3F\x01\x60\x02"
   "\x48\x84\x29\x20\x48\x3D\xC1\xFF\x10\x68\x48\xBD\x2F\x14\xE7\x7F"
   "\x2F\x94\x2F\x00\xE7\x7F\x2F\x80\xB9\x20\x00\x20\x71\xE4\x21\x19"
   "\x41\x04\xB9\x20\x00\x00\x41\x54\x29\x14\xF9\x88\x21\x07\x41\x04"
   "\x81\x00\x41\x54\x41\x2F\x41\xB5\xB9\x20\x00\x20\x71\xF4\x50\x88"
   "\x2F\x14\xE7\x81\xE6\x03\x81\x00\x49\x88\xB9\x20\x00\x2E\x01\x98"
   "\x21\xC2\x20\xAE\xB9\x20\x00\x04\x21\xC6\x91\x03\x41\x92\xFF\x9C"
   "\x63\x7E\x3C\x04\x21\x07\xF8\x86\xF8\x0A\xDC\x90\xA8\xAF\xF8\x08"
   "\xDC\x0A\xA8\xB5\xCC\x86\xA8\xB9\xCC\x02\xA8\xBD\xAA\x24\x4B\x10"
   "\x88\x0C\xB9\x20\x10\x02\x41\xB1\x40\xB3\x61\x42\x15\x40\x4D\x27"
   "\xB5\x03\x88\x10\xB5\x06\x88\x2C\xB5\x09\x88\x0C\x51\x08\xBD\x20"
   "\x0A\x56\x50\x98\xD6\x40\x60\x12\xB9\x20\x08\x00\x71\x94\xBB\x20"
   "\xFF\xFE\xA8\x00\xA8\x00\xBB\x87\xB9\x20\x10\x03\x41\xB1\x40\xB3"
   "\x61\x42\x15\x40\xD6\x20\x60\x16\x61\x3E\x15\x40\xB5\x00\x88\x02"
   "\xE6\xF7\xEE\x02\xA8\x0E\x4D\x08\xD5\x10\x2D\x84\x4D\x0C\xD5\x80"
   "\x4D\x8C\xA8\x04\xB8\x00\x44\x3A\x49\x5F\xD9\xD0\x63\x7E\x3C\x04"
   "\x21\x07\xF8\x86\xF8\x0A\xDC\xC2\xA8\x0E\xF8\x08\xDC\x3C\xA8\x08"
   "\xCC\xB8\xA8\x04\xCC\x34\xA8\x00\x23\x19\x43\x04\xBB\x20\xF4\x00"
   "\x43\x44\xBB\x20\x5B\x00\x43\xF4\xB9\x20\x05\x2E\x01\x98\x21\x96"
   "\x63\x32\x43\x94\x33\xC0\x33\xF8\x33\xF8\x33\xF8\x33\xF8\xD2\x04"
   "\x83\x20\x43\x84\xBB\x20\xF4\x00\x43\x44\xB8\x40\xE7\x89\xD7\x40"
   "\x2F\x9F\x21\x07\x41\x04\x63\x7E\x3C\x04\xF8\x86\xF8\x0A\xDC\x90"
   "\xAA\x3C\xF8\x08\xDC\x0A\xAA\x36\xCC\x86\xAA\x32\xCC\x02\xAA\x2E"
   "\xA8\x00\xA8\x00\xCE\x16\xFF\x94\xBB\x2F\xFF\xF2\x03\x98\xBD\x20"
   "\xB8\x20\x35\x81\xBD\x20\x00\x36\x05\x98\x35\x82\x25\x0D\xE4\x80"
   "\xED\x14\xBB\x20\x00\x3C\x03\x98\x23\x82\x85\x00\xD4\x01\x45\x54"
   "\xA8\x00\x45\x64\xA8\x10\x63\x4E\x23\x82\x83\x00\x43\x54\x85\x03"
   "\x45\x64\xA8\x00\x85\x7E\x84\xE5\x45\x44\x85\x98\xA8\x08\xA8\x00"
   "\x63\x4E\x23\x82\x85\x99\x45\x54\xB9\x20\x00\x1E\x21\x8B\x61\x4A"
   "\x21\x92\xA8\xDE\x83\x7E\xD2\x01\x61\x4E\xA8\xCE\x20\x8B\x20\x92"
   "\x81\xFF\x80\xFF\x21\x89\xE2\xFA\x2B\x1E\x61\x52\x21\x82\xA8\xA6"
   "\x2B\x04\xB3\xF3\x88\x08\xFB\x98\xD7\x20\x61\x56\xA8\x0E\xB9\x20"
   "\x03\x20\x01\x98\x15\x01\xB5\x00\x88\x56\x61\x56\x21\x82\xA8\x86"
   "\xB3\x97\x88\x04\x61\x5E\xA8\x0D\xB9\x20\x00\x02\x01\x98\xA8\x15"
   "\xBD\x20\x00\x03\xB9\x20\x0A\x84\x01\x98\xA8\x1A\xB9\x20\x02\xF2"
   "\x01\x98\x15\x01\x29\x04\xB1\xF3\x88\x0A\xBD\x20\x00\x10\x61\x36"
   "\xD7\x10\xA8\x02\x61\x3A\x21\xB6\x61\x5A\x21\x82\x21\x36\x13\x10"
   "\x21\xB6\xBD\xC2\x2D\x04\xB5\xF3\x88\x06\xDF\x96\xEF\x28\xA8\x0C"
   "\xB9\x20\x02\xBC\x01\x98\x15\x01\x95\x01\x15\x81\x61\x5E\x21\x82"
   "\xA8\x24\x45\x21\xA5\x06\x88\x0D\x21\x32\x21\xB6\xE7\xEF\xEF\x02"
   "\xA8\x14\x25\x39\xA8\x10\x21\x39\x45\x21\xA5\x06\x15\xA0\xB9\x20"
   "\x10\x00\xE7\xF7\xA8\x1D\x21\x09\x43\x44\x71\xCC\x21\x89\xA8\x12"
   "\x21\x1B\x43\x44\x71\xCC\x21\x9B\xA8\x08\x21\x82\xA8\x02\x21\x96"
   "\x43\x44\xCF\x86\x25\xA9\x2F\x94\xA8\x04\x25\xBB\x2F\x80\xB8\x40"
   "\x62\x66\xBB\x20\x0A\x02\x03\x98\x31\x81\x4C\x01\x23\x19\x31\xB0"
   "\x88\x14\x23\x07\x31\xB0\x88\x1E\xBB\x20\x00\x2C\x32\x98\xBC\x95"
   "\x43\x44\xA8\x00\xB8\x40\x2F\x14\xFF\x88\x25\x29\x21\x16\x43\x4C"
   "\x10\x88\x29\x1F\x88\x0D\x2F\x00\xD7\x40\x25\x3B\x21\x02\x43\x4C"
   "\xCA\x86\xDA\x84\x43\x44\xA8\x02\x10\x88\xB8\x40\x2B\x09\xC3\xFF"
   "\x61\x62\xA8\x6B\x2B\x08\xC3\xFF\x61\x6A\xA8\x73\x83\x7E\xD2\x01"
   "\x61\x72\xA8\x7B\x83\x7E\xE2\xFB\x21\x0D\xE9\x04\x61\x76\xA8\x87"
   "\xFF\x86\xCE\x04\x29\x24\xA8\x02\x81\x9C\xE2\xFB\x41\x54\xF7\x01"
   "\x88\x06\x83\xFF\x61\x7A\xA8\x9F\x43\x44\x61\x1E\x21\x82\xE7\xBF"
   "\xDF\x02\xA8\x0C\xE6\xF7\x48\x08\x90\x02\xE0\xEF\x48\x88\xE7\xDF"
   "\x2F\x80\xCE\x06\x21\x2E\x88\x02\x10\x88\xFF\x9C\x63\x7E\x3C\x04"
   "\x21\x07\xF8\x86\xF8\x0A\xDC\x90\xAB\xAB\xF8\x08\xDC\x0A\xAB\xB1"
   "\xCC\x86\xAB\xB5\xCC\x02\xAB\xB9\xA8\xCD\x81\x00\x29\x9F\x61\x06"
   "\x21\x96\x43\x44\xB9\x20\x80\x85\x41\x54\xA8\x51\xA8\xEF\x21\x07"
   "\x41\x04\xA8\x00\x45\x5C\xEC\x0A\xBD\x20\x80\x00\x45\x54\xA8\x00"
   "\x45\x64\x2C\x1E\x2D\x04\x45\xD4\x55\xC0\xD5\x01\xCE\x04\xFF\x82"
   "\x85\x00\x84\x5B\x45\xF4\xBD\x20\xF4\x00\x45\x44\xA8\x00\x45\x5C"
   "\xEC\x06\xBD\x20\x00\x98\x45\x54\x2D\x04\xB5\xF3\x88\x54\xF5\x01"
   "\x99\x00\xD7\x20\xBD\x20\x00\x10\xBB\x20\x01\x30\x03\x98\x35\x81"
   "\xE7\xEF\x23\x39\x23\xBF\x43\x21\x23\xBD\x63\x36\x43\x94\x33\xC0"
   "\x33\xF8\x33\xF8\x33\xF8\x33\xF8\xBD\x20\x01\x10\x05\x98\x5B\x01"
   "\x25\x3D\xD2\x04\xA5\x06\x88\xE8\xD2\x02\x25\xBD\xB9\x20\x00\x3C"
   "\x01\x98\x25\x07\x45\x04\x21\x82\x43\x84\xBD\x20\xF4\x00\x45\x44"
   "\xA9\x81\xB9\x20\x00\xEA\x01\x98\x13\x01\x93\x01\x13\x81\x63\x32"
   "\xB9\x20\x00\x02\x13\x98\x43\x94\x33\xC0\x33\xF8\x33\xF8\x33\xF8"
   "\x33\xF8\xB9\x20\x00\xCC\x01\x98\x1B\x00\xD2\x04\xA8\xA2\x23\x32"
   "\x43\x94\x33\xC0\x33\xF8\x33\xF8\x33\xF8\x33\xF8\xA8\x02\x43\x8C"
   "\x11\xC8\xEF\x24\x21\x3D\xF0\xFF\x88\x14\xBD\x20\x00\xFF\x51\xA0"
   "\x21\xBD\x83\xFF\xD2\x06\xB9\x2F\xFF\xE2\x01\x98\xA8\x6D\x13\xD0"
   "\xD2\x04\xE2\x74\xA8\x6A\x43\x8C\x21\x3F\xF0\xFF\x88\x14\xBD\x20"
   "\x00\xFF\x51\xA0\x21\xBF\x83\xFF\xD2\x06\xB9\x2F\xFF\xE6\x01\x98"
   "\xA8\x91\x13\xD0\xD2\x06\xEF\x04\xE2\x74\xA8\x44\xB9\x20\x00\x02"
   //      2 bytes CRC at the end of track 6 are removed ---^

   //      Track 7 content of the RIPL diskette
  /*1000   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x01\x98\xA8\xA3\xBB\x20\x10\x00\x43\x94\x33\xC0\x33\xF8\x33\xF8"
   "\x33\xF8\x33\xF8\x21\x39\x45\x21\xA5\x06\x15\xA0\x25\xBF\xE7\xF7"
   "\xA8\x4B\xBB\x20\x04\x00\xC5\x97\x98\x16\xBB\x20\x07\xBE\x03\x98"
   "\x43\x94\x33\xC0\x33\xF8\x33\xF8\x33\xF8\x33\xF8\xD2\x04\x83\x03"
   "\x21\x07\x41\x04\xB9\x2F\xFA\xBA\x01\x98\x21\x96\x43\x84\xBB\x20"
   "\xF4\x00\x43\x44\xFF\x82\xA9\xD1\x83\x00\x2B\x9F\xA9\xD3\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x61\x26\xAA\x7D\xE7\xCF\xCA\x8A\xB9\x20"
   "\x80\x85\x41\x54\x61\x06\xAA\x8B\x4B\xDE\x29\x1E\x31\x38\x88\x06"
   "\xB3\xFF\x88\x02\xA8\x19\x81\xFF\x80\xFF\x21\x9B\x61\x2A\x21\x96"
   "\x47\xB9\xAA\xB5\xF2\xA0\x98\x2B\xEA\xAD\xCA\x82\xA8\x31\x4B\xDF"
   "\x61\x46\x21\x96\xBD\x20\x00\x02\xFB\x82\xA8\x04\xE7\xDF\xA8\x02"
   "\xD7\x30\x81\x00\x49\x90\x61\x3A\x21\xA6\xAA\xDD\xF2\xA0\x98\x53"
   "\xEA\xD5\xCA\x82\xA8\x59\x21\x26\x13\x30\x21\xA6\xBD\x94\x48\x5F"
   "\xB0\xBF\x88\x06\xBD\x20\x00\x10\xA8\x04\xBD\x20\x00\x30\x61\x2E"
   "\x21\x96\xAB\x05\xF2\xA0\x98\x5E\xEA\xE0\xCA\x82\xA8\x58\x48\x5F"
   "\xB0\xF3\x88\x26\xCE\x02\xDF\x22\xAB\x1B\x48\x5F\xB0\xF3\x88\x09"
   "\xDF\x8B\xEF\x0D\x43\x8E\x43\x12\x21\x26\x31\xB8\x98\x08\x83\xFF"
   "\x4B\x90\xA1\x01\x21\xA6\x43\x0E\xAB\x3B\x21\x26\x13\x30\x21\xA6"
   "\xBD\xA9\x48\x5F\xB0\xF3\x88\x1E\xDF\x84\xEF\x10\xAB\x4F\x21\x46"
   "\x21\xA6\xE7\xEF\xEF\x02\xAB\x59\x45\x03\xAB\x5D\xE7\xF7\xB9\x20"
   "\x01\x10\x21\xA6\xAB\x67\x47\x39\xA8\xDD\x21\x1B\xBD\x20\xF0\xB8"
   "\x51\xB0\x88\x04\x47\x39\x60\x06\x41\x2F\x41\xB5\x61\x26\x21\x96"
   "\x43\x44\x49\x08\x48\x5F\xB0\xF3\x88\x10\xE0\xEF\xDF\x06\xF8\x06"
   "\xCE\x06\xAA\x94\xA9\xC2\xAA\xDA\xAB\x56\xB9\x2F\xFE\xE4\x01\x98"
   "\x63\x3A\x93\x02\x25\x26\x35\xA8\x15\x81\x83\xF3\x2B\x84\xB9\x2F"
   "\xFE\xCC\x01\x98\x13\x01\x93\x01\x13\x81\x60\x1A\xE7\xCF\x29\x1E"
   "\x4B\x5E\x31\x38\x88\x06\xB3\xFF\x88\x02\xA8\x06\x4A\x5F\xFA\x84"
   "\xA8\x06\xAE\xA5\xE7\xDF\xA8\xFC\xD7\x20\x21\x19\x41\x04\xEF\x02"
   "\xA8\x7E\xB9\x20\x00\x02\xBB\x2F\xFE\x98\x03\x98\x31\x81\x21\x46"
   "\x41\x94\x25\x19\x45\x04\xA8\x00\x41\x4C\xF0\xA0\x98\xCA\xF0\x04"
   "\x98\xD2\x21\x46\x11\xF8\x11\xF8\x11\xF8\x11\xF8\xE0\xF0\x81\x88"
   "\xD0\x04\x41\x84\xB9\x2F\xFE\x6A\x01\x98\x13\x01\xBB\x8A\xB9\x20"
   "\x00\x10\x01\x98\x21\x96\xA8\x98\x13\x81\xB9\x2F\xFF\xC2\x01\x98"
   "\x21\x96\xA8\x8C\xBB\x20\x01\x10\x43\x94\xE7\xF7\x21\x19\x41\x04"
   "\xA8\x00\x41\x4C\xF0\xA0\x98\x80\xF0\x04\x98\x88\xBB\x20\x04\xFF"
   "\x43\x84\xB9\x2F\xFF\xE4\x01\x98\x21\x96\xA8\x64\xA8\x00\xA8\x00"
   "\x21\x19\x41\x04\xA8\x00\x41\x4C\xF0\xA0\x98\x5C\xF0\x04\x98\x64"
   "\x21\x46\x41\x94\x11\xC0\x11\xF8\x11\xF8\x11\xF8\x11\xF8\x83\xFF"
   "\xA8\x30\x25\x19\x45\x04\xA8\x00\x45\x4C\xF4\xA0\x98\x3A\xF4\x04"
   "\x98\x42\x41\x9C\x43\x12\x45\x8C\x55\x98\x55\x98\x55\x98\x55\x98"
   "\x55\xC0\x15\xD8\x53\xA8\xBD\x20\x00\xFF\x53\xB8\x98\x02\x53\x88"
   "\x41\x8C\xD0\x04\x31\x08\x41\x84\xB9\x2F\xFF\xC4\x01\x98\x21\x96"
   "\xB9\x20\xF4\x00\x41\x44\xAC\xB7\xB9\x20\xF4\x00\x41\x44\x47\xB9"
   "\xAF\xAF\xA8\x00\x47\xB9\x43\x4C\xF2\xA0\x98\x15\xEA\xA0\x48\x5F"
   "\xB0\xBF\x88\x04\xA8\x1F\xA8\x12\xB9\x20\x05\x58\x01\x98\x41\x94"
   "\x41\x8C\xD0\x04\x81\x22\x41\x84\xA8\x3B\xA8\x00\xA8\x00\x41\xFC"
   "\xF0\x19\x98\x3D\xB9\x20\xF4\x00\x41\x44\x41\x2F\x41\xB5\xB9\x2F"
   "\xF8\x10\x01\x98\x21\x96\x49\x08\x48\x5F\xB0\xF3\x88\x10\xE0\xEF"
   "\xDF\x06\xF8\x06\xCE\x06\xA9\x00\xA8\x2E\xA9\x46\xA9\xC2\x43\x8C"
   "\x82\x00\xB3\x12\x88\x08\xB3\x00\x88\x73\x82\x10\x32\x28\xB9\x2F"
   "\xFD\x40\x01\x98\x1A\x80\x83\xF3\x2B\x84\xB9\x2F\xFD\x30\x01\x98"
   "\x13\x01\x93\x01\x13\x81\x60\x1A\xCE\x12\xEE\x12\x00\x78\x00\x78"
   "\x00\x78\x00\x78\xE1\x0E\x10\x28\x88\x16\xA8\x2C\xA9\xD7\x49\x08"
   "\xE1\xE0\xD1\x15\xD1\x10\x48\x5F\xD8\x82\xB8\x40\x29\x84\x60\x1A"
   "\x4D\x0C\xE5\x7F\x4D\x8C\x48\x08\x49\x5F\x00\x78\x00\x78\x00\x78"
   "\x00\x78\xE1\x0E\x10\x28\x88\x06\x47\x39\xB8\x40\xA8\xBC\xDF\x85"
   "\x49\x08\x91\x20\x49\x88\x84\x08\x61\x3A\x63\x36\x12\x01\x32\x81"
   "\x91\x02\x93\x02\xBC\x8B\x62\x66\xE7\xDF\xD6\x08\x48\x18\x01\x08"
   "\xE1\xF0\xB1\x10\x88\x02\xA8\x2C\xE0\x0C\xB0\x0C\x88\x02\xA8\x30"
   "\x41\x21\xBD\x20\x00\x08\x51\xB8\x98\x32\x49\x22\xC9\x46\xF1\x60"
   "\x98\x36\x49\x25\xB1\x01\x88\x02\xA8\x2E\x49\x26\xB1\x02\x88\x02"
   "\xA8\x26\x20\x42\xB9\x20\x80\x06\x41\xB1\x40\xB3\x61\x42\x15\x40"
   "\xB9\x20\x80\x07\x41\xB1\x40\xB3\x61\x42\x15\x40\xB9\x20\x80\x0C"
   "\x41\xB1\x40\xB3\x61\x42\x15\x40\xB9\x20\x10\x07\x41\xB1\x40\xB3"
   "\x61\x42\x15\x40\xE6\xF7\xAF\xE7\xE8\x20\x65\x6E\x55\x40\x88\x1E"
   "\xEE\x0C\xE0\x0E\xB0\x0E\x88\x2C\x83\x04\xA8\x02\x83\x80\x48\x5F"
   "\x49\x08\x41\xBB\x4B\xBC\x81\x97\xA8\xD7\xE8\x11\xB8\x40\x4D\x0C"
   "\xE5\x7F\x4D\x8C\x48\x5F\xE8\x82\xEE\x08\x49\x08\xE1\xE0\xD1\x11"
   "\xA8\xEF\x60\x22\x49\x08\x91\x0E\xE1\xEF\x49\x88\x47\x39\xD6\x08"
   "\x60\x22\xE0\xEC\xB0\x04\x88\x50\xB0\x40\x88\x22\xB0\x80\x88\x2E"
   "\xB0\x20\x88\x08\xB0\xAC\x88\x26\xA8\x4F\xA9\x19\xEE\x02\xA8\x02"
   "\xA8\x6D\xCE\x02\xA8\x04\xA8\x58\xA8\x02\xA8\x50\xA8\x15\xCE\x02"
   "\xA8\x04\xA8\x48\xA8\x06\xB9\x20\x3F\x02\x60\x02\xA8\x25\xCE\x02"
   "\xA8\x02\xA8\x3C\xDE\x82\xA8\x08\xCE\x82\xA8\x04\xA9\x78\xA8\x06"
   "\xB9\x20\x3F\x03\x60\x02\xA8\x3F\xE6\xF7\xCE\x02\xA8\x04\xA8\x24"
   "\xA8\x00\xF6\x60\x88\x0C\x26\x19\x06\xE4\xB9\x20\x3F\x10\xA8\x9A"
   "\xA8\x0A\xBB\x2F\xF6\xC8\x03\x98\x23\xAE\xE6\x7F\x81\x73\xA9\x7D"
   "\x81\x07\xA9\x81\x47\xB9\x48\x01\x65\x66\x52\xB0\x88\x1A\x53\x19"
   "\x47\x39\x43\x04\xBF\x20\x80\x90\x47\x54\x86\xF4\x47\x44\x57\x07"
   "\x37\xB0\x88\x04\x73\x88\xA8\x19\x95\x2C\xB8\xA3\x47\x39\x65\x66"
   "\x21\x88\x82\x16\x12\x01\x52\x81\x91\x02\x95\x02\xBA\x8B\x62\x66"
   "\x81\x01\x49\x81\xBB\x2F\xF6\x76\x03\x98\x23\xAE\xE6\x7F\xA8\x55"
   "\xA8\xB9\x00\x78\x00\x78\x00\x78\x00\x78\xE1\x0E\x10\x28\x50\x40"
   "\x63\xBC\xE2\xAF\xD2\x80\x63\xB4\x01\x94\x84\xFC\x65\x08\x75\x14"
   "\x71\x24\x73\x2C\xFB\x02\xA8\x02\x70\x04\xA8\x00\xA8\x00\xA8\x00"
   "\xA8\x00\xBB\x20\x20\x00\x73\x94\x73\x04\x63\xBC\xE2\xBF\xD2\x10"
   "\xA8\x2F\x61\x0A\x21\xC2\xB9\x2F\xF6\xAC\x01\x98\x41\x92\xB9\x20"
   "\x01\x10\x41\x83\x29\x14\xD1\x08\x29\x94\xBD\x20\x02\xD0\x05\x98"
   "\x25\xC6\x40\x8A\xA8\x40\x48\x10\xB0\xFF\x88\xB0\x63\x36\x49\x27"
   "\xB1\x04\x88\x0C\xB1\x05\x88\x40\xA8\xB0\xBD\x2F\xF6\x78\x50\x98"
   "\x41\x21\xA1\x08\x45\x0A\x15\x98\x45\x8A\xB9\x20\x01\x10\x51\xA8"
   "\x98\x12\x88\x10\x41\x83\xD7\x08\xB9\x20\x02\x92\x01\x98\x51\x98"
   "\x21\xC6\xA8\x02\x25\xC6\x49\x08\xD1\x10\x29\x84\x20\xAE\x20\xBA"
   "\x80\x00\x81\x06\x41\xA1\xA8\x3F\xDE\x82\xA8\x02\xA8\xA0\x45\x0A"
   "\xB9\x20\x01\x10\x15\xB8\x98\x02\xA8\x08\xBB\x20\x02\x60\x03\x98"
   "\x35\x98\x59\x01\x49\x85\x58\x02\x59\x03\x41\x87\x41\x06\x45\x0A"
   "\x51\xB8\x98\x0E\x88\x0C\xB9\x20\x08\x0A\x41\xB1\x40\xB3\x61\x42"
   "\x15\x40\xD6\x10\xA8\x51\xB9\x20\x80\x90\x41\x54\xBB\x20\x02\x2E"
   "\x03\x98\xBE\x20\x01\xE0\x06\x98\x61\x01\x88\x08\x41\x04\xB9\x20"
   "\x60\x00\x41\x34\x87\xFF\x46\x06\x00\x00\xA8\xA3\xB9\x20\x10\x02"
   "\x41\xB1\x40\xB3\x61\x42\x15\x40\xA8\xB1\xB1\x03\x88\x02\xA8\x02"
   "\xDE\x2C\xB1\x03\x88\x34\xB1\x06\x88\x02\xA8\x02\xCE\xA0\xB1\x06"
   "\x88\x28\xB1\x04\x88\x12\xB1\x05\x88\x0E\xB1\x07\x88\x0A\xB1\x08"
   "\x88\x06\xB1\x09\x88\x08\xA8\x02\xA8\x04\xA8\x1A\xA8\xE5\xB9\x20"
   "\x08\x1A\x41\xB1\x40\xB3\x61\x42\x15\x40\xB9\x20\x08\x15\x41\xB1"
   "\x40\xB3\x61\x42\x15\x40\xB9\x20\x10\x03\x41\xB1\x40\xB3\x61\x42"
   "\x15\x40\x00\x00\x15\x86\xA8\x04\xB8\x20\x00\x00\x4D\x22\xCD\x02"
   "\xA8\x02\xA8\x0D\x45\x1B\x41\x1D\x41\x9B\x45\x9D\x4D\x22\xD5\x80"
   "\x4D\xA2\x4D\x23\xE5\xF6\x4D\xA3\xF5\xE0\x88\x0C\xDD\x82\xA8\x04"
   "\x85\x20\xA8\x02\x85\x00\xA8\x04\x85\x10\xE6\xF7\xA8\x37\x00\x00"
   "\x15\x86\xA8\x04\xB8\x20\x00\x00\x45\x23\xCC\x02\xA8\x04\xE6\xF7"
   "\xA8\x3E\x61\x3E\x15\x40\xEE\x02\xA8\x34\x4D\x23\xD5\x10\x4D\xA3"
   "\x4D\x22\xD5\x04\x4D\xA2\x4C\x25\x4C\xA9\x4C\x26\x4C\xAA\x4C\x27"
   "\x4C\xAB\x45\x31\x4C\xA5\x4D\xA6\x45\x33\x4C\xA7\x4D\xA8\xBD\x20"
   "\x00\x0A\x45\xA1\xBD\x20\x00\xB2\x05\x98\x25\xB2\xA8\x02\xE6\xF7"
   "\x60\x22\x00\x00\x00\x00\x15\x30\x00\x00\x10\x66\x00\x00\x15\x86"
   "\x00\x00\x04\x40\x00\x00\x15\x62\x00\x00\x03\x76\x00\x00\x0C\x38"
   "\x00\x00\x0E\xDC\x00\x00\x0C\x22\x00\x00\x10\x6A\x00\x00\x10\x94"
   "\x00\x00\x10\xE4\x00\x00\x18\x12\x00\x00\x17\xCC\x00\x00\x18\x14"
   "\x00\x00\x16\xA4\x00\x00\x16\xE0\x00\x00\x10\xBC\x00\x00\x0B\x42"
   "\x00\x00\x0D\x1C\x00\x00\x0D\x30\x00\x00\x0D\x6C\x00\x00\x0D\x8C"
   "\x00\x00\x0E\x4C\x00\x00\x0E\x54\x00\x00\x19\xE0\x00\x00\x0E\x5C"
   "\x00\x00\x15\x22\x00\x00\x0E\x64\x00\x00\x0E\x70\x00\x00\x0E\xCA"
   "\x00\x00\x1F\x96\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x17\x70\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\xFD\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   //        01B64 ---v
   "\x00\x00\x00\x00\x04\x4C\x06\x6C\x07\x7C\xFB\x24\xEB\xD6\xFA\x08"
   "\x71\x7C\xC9\x94\xA8\xCE\xB8\x40\x75\x2C\x45\xC5\x75\x1C\x45\xC2"
   "\xBD\x20\x20\x00\x75\x74\xA8\x13\xBD\x20\x00\x40\x65\x04\xA8\x1B"
   "\xBC\x2F\xFC\xB2\x04\x98\xB9\x20\x00\x20\x71\xE4\x41\x02\x88\x02"
   "\xA8\x08\xB9\x20\x00\x20\x71\xF4\xA8\x12\x45\x06\x45\x82\x40\x86"
   "\xB9\x20\x00\x20\x71\xF4\x51\x88\x15\x40\xA8\x27\xBD\x20\x00\x20"
   "\x75\x74\xA8\x4F\x48\x01\x62\x66\x68\x80\xB9\x20\x00\x20\x71\xE4"
   "\x21\x0B\x88\x18\xB9\x94\x20\x8B\x21\x12\xBD\x20\x00\x20\x75\xF4"
   "\x15\x40\xB9\x20\x00\x20\x71\xE4\xA8\x02\x21\x8B\x21\x1D\x88\x18"
   "\xB9\x94\x20\x9D\x21\x22\xBD\x20\x00\x20\x75\xF4\x15\x40\xB9\x20"
  /*1C00   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x00\x20\x71\xE4\xA8\x02\x21\x9D\xB9\x20\x00\x20\x71\xF4\xB9\x20"
   "\x00\x2C\x12\x98\x68\x00\xB8\xD1\xB9\x20\x00\x20\x71\xE4\x41\x35"
   "\x88\x0C\xB9\x88\x40\xB5\xB9\x20\x3F\x04\x60\x02\x41\xB5\xB9\x20"
   "\x00\x20\x71\xF4\xB9\x20\x00\x0E\x01\x98\x15\x40\xBD\x20\x00\x40"
   "\x75\x74\xA8\xCF\xA8\xD1\x00\x00\x15\x86\xA8\x04\xB8\x20\x00\x00"
   "\x62\x66\x55\xA8\x84\xFC\x65\x08\x45\xCA\x45\x45\x88\x2C\xF4\x10"
   "\x98\x52\xF4\x08\x98\x30\xF5\x40\x98\x7A\xF5\x20\x98\x98\xF5\x10"
   "\x99\x12\xF5\x08\x99\x8E\xF5\x04\x99\xAC\xF5\x02\x99\xD2\xA8\x0A"
   "\x45\x4A\x75\x14\x45\x4E\x75\x24\xA8\x3F\xB9\x2F\xFB\x7A\x01\x98"
   "\x15\x01\x45\xCE\xA8\x17\x45\x42\xE4\x10\xE5\xF0\x45\xCA\xBB\x20"
   "\x01\x0C\x53\xD8\xBD\x20\x00\x00\x05\x98\x53\x85\x11\xC8\xA8\x00"
   "\x41\xCE\xA8\x35\x73\x0C\xA8\x00\x75\x9C\xF5\x08\x98\x02\xA8\x08"
   "\x33\x98\x33\x98\x33\x98\x33\x98\x33\x98\x33\x98\xA3\x01\x45\x42"
   "\x53\xB8\x98\x0A\xE5\xFE\x45\xCA\x53\x01\x43\xCE\xA8\x04\x45\xCA"
   "\x40\xCE\xA8\x65\x4D\x5F\xB5\xF3\x88\x0C\x65\xBC\x4D\x3D\x45\xCA"
   "\x45\x37\x45\xCE\xA8\x0E\xBB\x2F\xF3\x64\x03\x98\x35\x01\x45\xCA"
  /*1D00   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x35\x03\x45\xCE\xA8\x87\x45\x42\xB9\x20\x01\x4A\x01\x98\x13\x40"
   "\x55\x88\x88\x0C\xB9\x2F\xFA\xEE\x01\x98\x13\x01\xA8\x08\xA8\x04"
   "\x40\xCA\x40\xCE\xA8\xA7\x53\xB8\x88\x02\xA8\x1E\xB9\x20\x00\x20"
   "\x71\xE4\x43\x04\xA8\x00\x41\x4C\x41\xCA\x41\x5C\x45\x6C\xBA\x20"
   "\x00\x20\x72\xF4\x41\x08\x41\xCE\xA8\x38\xB9\x2F\xFA\xB8\x01\x98"
   "\x13\x01\xB9\x20\x00\x20\x71\xE4\xB9\x20\x60\x00\x88\x10\x43\x04"
   "\xA8\x00\x41\x34\xB9\x2F\xFA\x9E\x01\x98\x10\x81\xA8\x0E\x45\x04"
   "\xC0\xC0\x41\x34\xB9\x2F\xFA\x8E\x01\x98\x15\x81\xB9\x20\x00\x20"
   "\x71\xF4\xA8\x67\x4C\x27\x65\x08\x45\xCA\x62\x66\xDF\x02\xA8\x02"
   "\xCE\x8C\xEE\x02\xA8\x02\xDE\x2A\x45\x0A\x45\xCE\xA9\x1F\x45\x0A"
   "\x23\x26\x53\xB8\x98\x1A\xB9\x2F\xFA\xA4\x01\x98\x13\xB8\x98\x08"
   "\x88\x06\x13\xA8\x35\x88\xA8\x08\x41\x12\x31\xB8\x98\x02\x35\x88"
   "\xA8\x29\x45\x0A\x23\x01\xB9\x2F\xEF\xC0\x01\x98\x13\xB8\x98\x06"
   "\x2F\x00\xDF\x82\xA8\x04\x23\x32\xA8\x02\x23\x36\x23\x36\x53\xB8"
   "\x98\x02\xA8\x08\xEF\x02\xFE\x04\x35\x88\xA8\x16\xFE\x02\xA8\x12"
   "\xB9\x20\x30\x00\x13\xB8\x98\x02\xA8\x08\x85\x00\xE4\xC0\x35\x98"
  /*1E00   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\x94\x10\xA8\x6B\x45\x42\xB9\x20\x00\x4C\x01\x98\x13\x40\x55\x88"
   "\x88\x0E\x4C\x5E\x4D\x5F\x45\xCA\x2C\x1E\x2D\x04\x45\xCE\xA8\x04"
   "\x40\xCA\x40\xCE\xA9\xA7\x45\x42\xB4\x00\x88\x02\xA8\x04\x63\x36"
   "\xA8\x04\xD4\xFF\x63\x3A\xB5\x12\x98\x04\x88\x02\xA8\x0C\xE5\xFE"
   "\x45\xCA\x53\x18\x35\x01\x45\xCE\xA8\x04\x40\xCA\x40\xCE\xA9\xD1"
   "\x40\xCA\x40\xCE\x40\xC5\xA9\xD9\x13\x86\xA8\x04\xB8\x20\x00\x00"
   "\xBB\x20\x0B\xFF\x35\xE8\xEC\x04\x55\x98\x94\x08\x62\x66\x48\x01"
   "\x23\x19\x53\xB8\x88\x10\x23\x07\x53\xB8\x88\x0A\xBB\x20\x00\x2C"
   "\x32\x98\xB8\x95\x55\xA8\xA8\x2D\xA8\x00\xB7\xFF\x88\x8A\x17\x74"
   "\x11\x14\x13\x34\x77\x9C\xCF\x02\xA8\x0E\x13\x0C\xB9\x20\x00\x2E"
   "\x01\x98\x31\xB8\x88\x02\x13\x82\x71\xEC\x73\x6C\x20\x08\xF0\x06"
   "\x98\x32\xF1\x80\x98\x2E\xF1\x48\x98\x32\xF9\x32\xF0\x60\x98\x2E"
   "\xF0\x18\x98\x2A\xD9\x30\xA8\x1C\x17\x7C\x11\x1C\x13\x3C\xB8\x40"
   "\xB8\x00\x00\x00\x22\xC8\xBB\x20\x40\x0C\x73\x74\xB8\x40\xBB\x20"
   "\x01\x88\x30\x02\xBB\x20\x40\x08\x73\x74\xA8\x25\x70\x04\xB9\x20"
   "\x3F\x05\x60\x02\xA8\x2F\x71\x9C\xC9\x82\xA8\x18\x03\x8C\xA3\x02"
  /*1F00   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   "\xB9\x2F\xFD\xA6\x01\x98\x31\xA8\x88\x02\xA8\x04\x01\x94\xA8\x02"
   "\xA8\x27\xA8\x02\xA8\x2B\xA8\x51\xBD\x20\x01\x10\x77\xA8\x31\x10"
   "\x71\x30\xBD\x87\xB9\x20\x00\x20\x71\xE4\xB9\x20\x00\x04\x71\x74"
   "\xB9\x20\x00\xD0\x71\x94\x80\x80\x61\xB4\x01\x88\x11\xF8\x11\xF8"
   "\x77\x0C\xF7\x01\x98\x14\x81\x00\xE0\xFE\xD1\x18\x71\x34\x80\x00"
   "\x71\x34\x90\x02\x98\x1E\xA8\x09\xA8\x1A\x11\xF8\x11\xF8\x11\xF8"
   "\x11\xF8\xE1\xE0\xD1\x18\x71\x34\x80\x00\xE1\x18\x71\x34\x91\x20"
   "\xD8\x02\xA8\x09\x21\x19\x77\xA8\x43\x0A\xE3\xFE\x05\x88\x35\xA8"
   "\x55\xF8\x37\x81\x93\x02\xBD\x87\xBD\x20\x20\x60\x75\x74\x33\xA8"
   "\x62\x88\x20\x88\xA8\xCF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
   "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x72\x00\x00\x00"
   "\x00\x00\x00\x00\x43\x54\x03\x90\xC3\xE7\xC2\xC4\xD9\xD3\xD7\xF2"
   "\xE7\xF3\xF7\xF0\xF5\xD5\xC5\xC1\x40\x40\xE5\x61\xD3\x7E\xF7\xF2"
   "\xB9\x94\x20\x9D\x21\x22\xBD\x20\x00\x20\x75\xF4\x15\x40\x58\x8D"
   };

   //******************************************************************
   // IPL phase 3 - Bootstrap load
   //******************************************************************
   /* If At least one Channel Adapter is enabled, load mini- or maxiROS else boot from diskette */
   while (CAready != TRUE)
      usleep(100);
   if (CAdisable) {
      for (int i=0; i < MAXCHAN; i++) {
         cp3705->ABswitch[i] = -1;
         printf("CPU: Disabling CA%1d\r\n",i+1);
      }
   }

   for (int i=0; i < MAXCHAN;i++) {
      printf("CPU: checking CA%1d: status = %d \r\n",i+1,cp3705->ABswitch[i] );
      if (cp3705->ABswitch[i] != -1) {
      printf("CPU: CA%1d  enabled\r\n",i+1);
      /* At least one AB switch enabled: this is not a remote 3705       */
         if (Eregs_Inp[0x79] & 0x0002) {
            /* If CA Type 1 or 4 Load miniROS at location 0x0000 */
            printf("CPU: Loading MiniROS at X'00000' \n\r");
            for (addr = 0x0000; addr < 0x0200; addr++) {
               temp = miniROS[addr];
               PutMem(addr, temp);
            }
         } else {
            /* If CA Type 2 or 3 Load maxiROS at location 0x0000 */
            printf("CPU: Loading MaxiROS at X'00000'\n\r");
            for (addr = 0x0000; addr < 0x0200; addr++) {
               temp = maxiROS[addr];
               PutMem(addr, temp);
            }
         }
         break;
      } else if (i == MAXCHAN-1) {
         printf("CPU: Booting 3705 from diskette... \n\r");
         /* RPL switch set:
            load LPG1 from 'diskette' track 0 at location 0x0040 and 0x0400
            and load LPG2 at location max memory - 0x02000 */
         printf("CPU: Loading LPG1 at X'00400'\n\r");
         for (addr = 0x0000; addr < 0x0600; addr++) {
            temp = LPG1[addr];
            PutMem(addr, temp);
         }
         /* Load LPG2 from 'diskette' track 6&7 at location max memory-0x2000 */
         printf("CPU: Loading LPG2 at X'%05X'\n\r", MEMSIZE-0x2000);
         for (addr = 0x00000; addr < 0x02000; addr++) {
            temp = LPG2[addr];
            PutMem(MEMSIZE - 0x02000 + addr, temp);
         }
      } // End if (cp3705->abswitch[i] != -1
   } // End for (int i=0; i < MAXCHAN;i++)
      Eregs_Inp[0x6B] |= 0x0800;               /* RPL ROS selected   */
      int_lvl_mask[1] = OFF;                   /* Allow pgm level 1  */
      ipl_req_L1 = ON;                         /* Request L1 for IPL */
      lvl = 1;                                 /* Switch to level 1  */
      GR[0][0] = 0x00010;                      /* IPL start address  */
   return SCPE_OK;
}

/*** RESET pressed procedure ***/

t_stat cpu_reset (DEVICE *dptr) {              /* RESET pressed */
   //******************************************************************
   // IPL phase 1 - CCU reset procedure
   //******************************************************************
   int32 i;
   sim_brk_types = sim_brk_dflt = SWMASK ('E');  /* Clear all BP's */

   /* Clear level 1 GP registers */
   GR[0][0] = 0x00000;  GR[1][0] = 0x00000;  GR[2][0] = 0x00000;  GR[3][0] = 0x00000;
   GR[4][0] = 0x00000;  GR[5][0] = 0x00000;  GR[6][0] = 0x00000;  GR[7][0] = 0x00000;
   /* Clear RPL input registers */
   Eregs_Inp[0x68] = 0x0000;
   Eregs_Inp[0x69] = 0x0000;
   Eregs_Inp[0x6A] = 0x0000;
   Eregs_Inp[0x6B] = 0x0000;
   /* Clear CCU input registers */
   Eregs_Inp[0x70] = 0x0000;
   Eregs_Inp[0x71] = 0x0000;
   Eregs_Inp[0x72] = 0x0000;
   Eregs_Inp[0x73] = 0x0000;
   Eregs_Inp[0x74] = 0x0000;
   Eregs_Inp[0x76] = 0x0000;
   Eregs_Inp[0x77] = 0x0000;
   Eregs_Inp[0x79] = 0x0000;
   Eregs_Inp[0x7A] = 0x0000;
   Eregs_Inp[0x7B] = 0x0000;
   Eregs_Inp[0x7C] = 0x0000;
   Eregs_Inp[0x7D] = 0x0000;
   Eregs_Inp[0x7E] = 0x0000;
   Eregs_Inp[0x7F] = 0x0000;

   /* Set/reset HARD STOP, PGM STOP, IPL LATCHES 1 & 2, TEST MODE */
   test_mode  = ON;                            /* See PoO 5-11 */
   pgm_stop   = OFF;
   load_state = OFF;
   wait_state = OFF;
   OP_reg_chk = OFF;
   IO_L5_chk  = OFF;

   /* Reset all interrupt level flags */
   for (int i = 0; i < 6; i++) {
      int_lvl_req[i]  = OFF;                   /* Reset all Pgm Level request */
      int_lvl_ent[i]  = OFF;                   /* Reset all "Interrupt Entered" */
      int_lvl_mask[i] = ON;                    /* Set all Pgm Level masks */
   }
   lvl = 5;
   /* Set cycle count register */
   Eregs_Inp[0x7A] = 0x8000;                   /* CUCR RPQ install        */
   cycle_eight = 0;                            /* 8 cycle counter to zero */

   printf("CPU: Reset... \n\r");
   msize = MEMSIZE / 1024;
   switch (msize) {
      case 32:
         Eregs_Inp[0x70]  = 0x0200;            // 32 kbyte storage size (3705-II)
      break;
      case 64:
         Eregs_Inp[0x70]  = 0x0400;            // 64 kbyte storage size (3705-II)
      break;
      case 96:
         Eregs_Inp[0x70]  = 0x0600;            // 96 kbyte storage size (3705-II)
      break;
      case 128:
         Eregs_Inp[0x70]  = 0x0800;            // 128 kbyte storage size (3705-II)
      break;
      case 160:
         Eregs_Inp[0x70]  = 0x0A00;            // 160 kbyte storage size (3705-II)
      break;
      case 192:
         Eregs_Inp[0x70]  = 0x0C00;            // 192 kbyte storage size (3705-II)
      break;
      case 224:
         Eregs_Inp[0x70]  = 0x0E00;            // 224 kbyte storage size (3705-II)
      break;
      case 256:
         Eregs_Inp[0x70]  = 0x1000;            // 256 kbyte storage size (3705-II)
      break;
      default:
         Eregs_Inp[0x70]  = 0x1000;            // 256 kbyte storage size (3705-II)
      break;
   }
   printf("CPU: MEMORYSIZE %dK bytes \n\r", msize);

   return SCPE_OK;
}

/*-------------------------------------------------------------------*/
/* Return Timestamp hh.mm.ss.msec                                    */
/*-------------------------------------------------------------------*/
static char * TimeStamp() {
    static char Timebuff[17+1];
    struct timeval tv;
    struct timezone tz;
    struct tm *timestamp;
    gettimeofday(&tv,&tz);
    timestamp = localtime(&tv.tv_sec);
    time_t now = time (0);
    snprintf(Timebuff,18,"[%02d:%02d:%02d.%06d]", timestamp->tm_hour,timestamp->tm_min,timestamp->tm_sec,tv.tv_usec);
    return Timebuff;
}

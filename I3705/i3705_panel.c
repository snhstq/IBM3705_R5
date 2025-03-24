/* Copyright (c) 2023, Edwin Freekenhorst and Henk Stegeman

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
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "i3705_defs.h"
#include "i3705_Eregs.h"               /* Exernal regs defs */


extern int32 msize;
extern int32 PC;
extern int32 Eregs_Out[];
extern int32 Eregs_Inp[];
extern int8  timer_req_L3;
extern int8  inter_req_L3;
extern int8  load_prsd;

// CCU status flags
extern int8  test_mode;
extern int8  load_state;
extern int8  wait_state;
extern int8  pgm_stop;

void sig_handler (int signo);
void timer_msec (long int msec);

int rc, i;

#define SHMEM_ID "/SHM_PANEL"

struct CP3705 *cp3705;


extern uint8 M[MAXMEMSIZE];
extern pthread_mutex_t r7f_lock;
extern struct IO3705* iobs[MAXCHAN];
extern int CAready;                                     /* Channel Adaper code fully initialized       */
extern int Ireg_bit(int reg, int bit_mask);
extern void wait();


// **********************************************************
// The panel adaptor handling thread starts here...
// **********************************************************
/* Function to be run as a thread always must have the same
   signature: it has one void* parameter and returns void    */

void *PNL_thread(void *arg) {
   fprintf(stderr, "PNL: Thread %ld started succesfully... \n\r", syscall(SYS_gettid));

   // We can set one or more bits here, each one representing a single CPU
   //cpu_set_t cpuset;
   // Select the CPU core we want to use
   //int cpu = 2;

   //CPU_ZERO(&cpuset);                  // Clears the cpuset
   //CPU_SET( cpu , &cpuset);            // Set CPU on cpuset

   /*
    * cpu affinity for the calling thread
    * first parameter is the pid, 0 = calling thread
    * second parameter is the size of your cpuset
    * third param is the cpuset in which your thread
    * will be placed. Each bit represents a CPU.
    */
   //sched_setaffinity(0, sizeof(cpuset), &cpuset);


   signal (SIGALRM, sig_handler);      // Interval timer //
   timer_msec(100);                    // <=== sets the 3705 interval timer

   int rc;
   int fdshmem;
   void *shmaddr;

   // set shared memory file descriptor
   fdshmem = shm_open(SHMEM_ID, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
   if (fdshmem == -1) {
      printf("\nPNL: Failed to allocate shared memory\r");
      exit(-1);
   }
   // extend shared memory object as by default it's initialized with size 0
   rc = ftruncate(fdshmem, sizeof(struct CP3705));
   if (rc == -1)  {
      printf("\nPNL: Failed to extend shared memory size\r");
      exit(-1);
   }

   // map shared memory to process address space
   cp3705 = mmap(NULL, sizeof(struct CP3705), PROT_WRITE, MAP_SHARED, fdshmem, 0);
   if (cp3705 == MAP_FAILED)  {
      printf("\nPNL: Failed to map memory to address space\r");;
      exit(-1);
   }

   while (CAready != TRUE)                                   /* Wait until channel IO blocks have been initialized */
      usleep(100);
   while(1) {
      cp3705->memsize = msize;                                 /* get memory size             */
      cp3705->reg71_Out = Eregs_Out[0x71];                     /* Get display register A      */
      cp3705->reg72_Out = Eregs_Out[0x72];                     /* Get display register B      */
      cp3705->reg7A_In =  Eregs_Inp[0x7A];
      memcpy(cp3705->CA1_bus_socket,iobs[0]->bus_socket,2);    /* Get CA1 file descriptor */
      memcpy(cp3705->CA2_bus_socket,iobs[1]->bus_socket,2);    /* Get CA file descriptor */
      for (int i = 0; i < MAXCHAN; i++) {
         if (cp3705->ABswitch[i] != iobs[i]->abswitch)         /* If CAx switch position changed on Control Panel */
           iobs[i]->abswitch = cp3705->ABswitch[i];            /* Update  CAx                                     */
      }
      // The below two lines are for 'just in case' as the values always should be accurate
      for (int i=0; i < MAXCHAN; i++) {
        iobs[i]->abswitch = cp3705->ABswitch[i];
      }
      cp3705->freebuf = (M[0x0754] << 8) + M[0x0755];        /* get free buffer count... */

      switch (cp3705->pnlkey) {                              /* Check for  a key pressed..... */
         case NOKEY:                                         /* Nothing Pressed   */
            break;
         case LOAD:                                          /* Load key pressed     */
            load_prsd = TRUE;                                /* Indicate a load is required */
      printf("PNL: LOAD pressed, rebooting....\n\r");
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed           */
            break;
         case INTERRUPT:                                     /* Interrupt key pressed     */
            pthread_mutex_lock(&r7f_lock);
            Eregs_Inp[0x7F] |= 0x0200;
            pthread_mutex_unlock(&r7f_lock);
            inter_req_L3 = ON;                               /* Panel L3 request flag */
            while (Ireg_bit(0x7F, 0x0200) == ON)
               wait();
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case TAROP:                                         /* TAR & OP function selected        */
            Eregs_Inp[0x72] &= 0x0002;                       /* Reset previous switch setting     */
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case STATUS:                                        /* STATUS function selected          */
            Eregs_Inp[0x72] &= ~0x1877;                      /* Reset all previous switch setting */
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case FUNCTION1:                                     /* FUNCTION 1 selected               */
            Eregs_Inp[0x72] &= ~0x0800;
            Eregs_Inp[0x72] |= 0x0040;
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case FUNCTION2:                                     /* FUNCTION 2 selected               */
            Eregs_Inp[0x72] &= ~0x0040;
            Eregs_Inp[0x72] |= 0x0020;
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case FUNCTION3:                                     /* FUNCTION 3 selected               */
            Eregs_Inp[0x72] &= ~0x0020;
            Eregs_Inp[0x72] |= 0x0010;
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case FUNCTION4:                                     /* FUNCTION 3 selected               */
             Eregs_Inp[0x72] &= ~0x0010;                     /* Reset previous function select    */
             Eregs_Inp[0x72] |= 0x0008;                      /* set current function              */
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case FUNCTION5:                                     /* FUNCTION 5 selected               */
            Eregs_Inp[0x72] &= ~0x0008;
            Eregs_Inp[0x72] |= 0x0004;
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case FUNCTION6:                                     /* FUNCTION 6 selected               */
            Eregs_Inp[0x72] &= ~0x0004;
            Eregs_Inp[0x72] |= 0x0002;
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case STORADDR:                                      /* Storage Address selected          */
            // Set current function
            Eregs_Inp[0x72] |= 0x1000;
            cp3705->membyte = M[cp3705->maddr];              /* Get byte from memory location     */
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
         case REGADDR:                                       /* Register Address selected         */
            // Set current function
             Eregs_Inp[0x72] &= ~0x1000;
             Eregs_Inp[0x72] |= 0x0800;
            Eregs_Inp[0x72] |= 0x1000;
            cp3705->regvalue = Eregs_Inp[cp3705->hexswitch]; /* Get bytes from Input Register     */
            cp3705->pnlkey = NOKEY;                          /* Reset key pressed                 */
            break;
      } // End switch (cp3705->pnlkey)
      usleep(1000);                                          /* relax a bit                       */
   }  // End while(1)
}

// *****************************************************
// Interval timer definition
// *****************************************************
void timer_msec (long int msec) {
   struct itimerval timer1;

   timer1.it_interval.tv_usec = (1000 * msec);
   timer1.it_interval.tv_sec = 0;
   timer1.it_value.tv_usec = (1000 * msec);
   timer1.it_value.tv_sec = 0;

   setitimer (ITIMER_REAL, &timer1, NULL);
}

// Kick the 3705 100msec timer...
void sig_handler (int signo) {
   float millis = 0.0;
   if ((test_mode == OFF) && !(Eregs_Inp[0x7F] &= 0x0004)) {
      pthread_mutex_lock(&r7f_lock);
      Eregs_Inp[0x7F] |= 0x0004;
      pthread_mutex_unlock(&r7f_lock);
      timer_req_L3 = ON;
   }
}


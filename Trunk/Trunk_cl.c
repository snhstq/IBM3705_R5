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

   Trunk_cl.C   (c) Copyright  Edwin Freekenhost

   This module emulates a trunk (local/remote) communication line.
   It is intended to connect two 3705s via half or full duplex SDLC lines,
*/

#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define RDY 1
#define NRDY 0
#define ON 1
#define OFF 0

#define LINEBASE   37500

uint16_t Tdbg_flag = OFF;          /* 1 when Ttrace.log open */
FILE *T_trace;

uint8_t bfr[256];


/* Variablers used */
uint8_t        LINE_rbuf[65536];    /* Line Read Buffer                      */
uint16_t       LINErlen;            /* Buffer size of received data          */
uint8_t        Fdx1 = OFF;          /* Default Full Duplex off for line 1    */
uint8_t        Fdx2 = OFF;          /* Default Full Duplex off for line 2    */

int SocketReadAct (int fd);
static char * TimeStamp();

//*********************************************************************
// Function to check if socket is (still) connected                   *
//*********************************************************************
static bool IsSocketConnected(int sockfd) {
   int rc;
   struct sockaddr_in peer_addr;
   int addrlen = sizeof(peer_addr);

   if (sockfd < 1) {
      return false;
   }
   rc = getpeername(sockfd, (struct sockaddr*)&peer_addr, &addrlen);
   if (rc != 0) {
      return false;
   }
   return true;
}


/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/* Main section - establish and manage TCP connections                        */
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void main(int argc, char *argv[]) {
   int      sockopt;                     /* Used for setsocketoption          */
   int      pendingrcv;                  /* pending data on the socket        */
   struct   addrinfo *line1ahost;
   struct   addrinfo *line1bhost;
   struct   addrinfo *line2ahost;
   struct   addrinfo *line2bhost;
   int      line1num = 20;               /* Line number (default 20)          */
   int      line2num = 20;               /* Line number (default 20)          */
   struct   sockaddr_in host1addr;       /* host1 address details             */
   struct   sockaddr_in host2addr;       /* host2 address details             */
   //struct   sockaddr_in line1addr;       /* Line-1 connection details         */
   //struct   sockaddr_in line2addr;       /* Line-2 connection details         */
   int      line1a_fd;                   /* Line-1 socket for half and full duplex */
   int      line1b_fd;                   /* Line-1 socket for full duplex only     */
   int      line2a_fd;                   /* Line-2 socket for half and full duplex */
   int      line2b_fd;                   /* Line-2 socket for full duplex only     */
   int      line1state;                  /* state of line 1                        */
   int      line2state;                  /* state of line 2                        */
   int      i, rc, rc1, rc2;
   char     *host1name;
   char     *host2name;
   char     host1[NI_MAXHOST];
   char     host2[NI_MAXHOST];
   char     portl1a[6];
   char     portl1b[6];
   char     portl2a[6];
   char     portl2b[6];

   char ipv4addr[sizeof(struct in_addr)];

   /* Read command line arguments */
   if (argc == 1) {
      printf("\rDLSw: Error - Arguments missing\n");
      printf("\r   Valid arguments are:\n");
      printf("\r   -cchn1 {hostname}  : hostname of host running the first 3705\n");
      printf("\r   -ccip1 {ipaddress} : ipaddress of host running the first 3705 \n");
      printf("\r   -cchn2 {hostname}  : hostname of host running the second 3705\n");
      printf("\r   -ccip2 {ipaddress} : ipaddress of host running the second 3705 \n");
      printf("\r   -line1 {line number} : Line number on first 3705 to connect to\n");
      printf("\r   -line2 {line number} : Line number on second 3705 to connect to\n");
      printf("\r   -fdx : Enable Full Duplex mode\n");
      printf("\r   -fdx1 : Enable Full Duplex mode on line 1 only\n");
      printf("\r   -fdx2 : Enable Full Duplex mode on line 2 only\n");
      printf("\r   -d : switch debug on  \n");
      return;
   }
   Tdbg_flag = OFF;
   i = 1;

   while (i < argc) {
      if (strcmp(argv[i], "-d") == 0) {
         Tdbg_flag = ON;
         printf("\rTrunk: Debug on. Trace file is trace_Trunk.log\n");
         i++;
         continue;
      } else if (strcmp(argv[i], "-fdx") == 0) {
         Fdx1 = ON;
         Fdx2 = ON;
         printf("\rTrunk: Full Duplex mode\n");
         i++;
         continue;
      } else if (strcmp(argv[i], "-fdx1") == 0) {
         Fdx1 = ON;
         printf("\rTrunk: Full Duplex mode for line 1\n");
         i++;
         continue;
      } else if (strcmp(argv[i], "-fdx2") == 0) {
         Fdx2 = ON;
         printf("\rTrunk: Full Duplex mode for line 2\n");
         i++;
         continue;
      } else if (strcmp(argv[i], "-cchn1") == 0) {
         if ( (rc = getaddrinfo(argv[i+1],NULL,NULL,&line1ahost )) != 0 ) {
            printf("\rNULL: Cannot resolve 3705 hostname 1 %s\n", argv[i+1]);
            return;                /* error */
         }  // End if line1ent
         printf("\rTrunk: Connection to be established with line-1 at 3705 on host %s\n", argv[i+1]);
         host1name = argv[i+1];
         i = i+2;
         continue;
      } else if (strcmp(argv[i], "-cchn2") == 0) {
         if ( (rc = getaddrinfo(argv[i+1],NULL,NULL,&line2ahost )) != 0 ) {
            printf("\rNULL: Cannot resolve 3705 hostname 2 %s\n", argv[i+1]);
            return;                /* error */
         }  // End if line2ent
         printf("\rTrunk: Connection to be established with line-2 at 3705 on host %s\n", argv[i+1]);
         host2name = argv[i+1];
         i = i+2;
         continue;
      } else if (strcmp(argv[i], "-ccip1") == 0) {
         if ( inet_pton(AF_INET, argv[i+1], &host1addr.sin_addr) != 1) {
            printf("\rTrunk: Cannot convert 3705 1 ip address %s, error: %s\n", argv[i+1],strerror(errno));
            return; /* error */
          }
          host1addr.sin_family=AF_INET;
          if (rc =(getnameinfo((struct sockaddr*)&host1addr,sizeof(host1addr), host1,sizeof(host1),NULL,0, NI_NOFQDN | NI_NAMEREQD )) != 0 ) {
             printf("\rTrunk: Cannot resolve 3705 1 ip address %s, error: %s, rc: %s\n", argv[i+1],strerror(errno), gai_strerror(rc));
             return; /* error */
          } // End if lineent
          printf("\rTrunk: Connection to be established with line-1 at 3705 on host %s\n",host1);
          host1name = host1;
          i = i + 2;
          continue;
      } else if (strcmp(argv[i], "-ccip2") == 0) {
         if ( inet_pton(AF_INET, argv[i+1], &host2addr.sin_addr) != 1) {
            printf("\rTrunk: Cannot convert 3705 2 ip address %s, error: %s\n", argv[i+1],strerror(errno));
            return; /* error */
         }
         host2addr.sin_family=AF_INET;
         if (rc =(getnameinfo((struct sockaddr*)&host2addr,sizeof(host2addr), host2,sizeof(host2),NULL,0, NI_NOFQDN | NI_NAMEREQD )) != 0 ) {
            printf("\rTrunk: Cannot resolve 3705 2 ip address %s, error: %s, rc: %s\n", argv[i+1],strerror(errno), gai_strerror(rc));
            return; /* error */
         }  // End if lineent
         printf("\rTrunk: Connection to be established with line-2 at 3705 on host %s\n",host2);
         host2name = host2;
         i = i + 2;
         continue;
      } else if (strcmp(argv[i], "-line1") == 0) {
         sscanf(argv[i+1], "%d", &line1num);
         printf("\rTrunk: Connection to be established with line-1 %d\n", line1num);
         i = i + 2;
         continue;
      } else if (strcmp(argv[i], "-line2") == 0) {
         sscanf(argv[i+1], "%d", &line2num);
         printf("\rTrunk: Connection to be established with line-2 %d\n", line2num);
         i = i + 2;
         continue;
      } else {
         printf("\rNULL: invalid argument %s\n", argv[i]);
         printf("\r     Valid arguments are:\n");
         printf("\r     -cchn1 {hostname}    : hostname of host running the first 3705\n");
         printf("\r     -ccip1 {ipaddress}   : ipaddress of host running the first 3705 \n");
         printf("\r     -cchn2 {hostname}    : hostname of host running the second 3705\n");
         printf("\r     -ccip2 {ipaddress}   : ipaddress of host running the second 3705 \n");
         printf("\r     -line1 {line number} : Line number on the first 3705 to connect to\n");
         printf("\r     -line2 {line number} : Line number on the second 3705 to connect to\n");
         printf("\r     -fdx : Enable Full Duplex mode\n");
         printf("\r     -fdx1 : Enable Full Duplex mode on line 1 only\n");
         printf("\r     -fdx2 : Enable Full Duplex mode on line 2 only\n");
         printf("\r     -d : switch debug on  \n");
         return;
      }  // End else
   }  // End while

   //********************************************************************
   // Null modem debug trace facility
   //********************************************************************
   if (Tdbg_flag == ON) {
      T_trace = fopen("trace_Trunk.log", "w");
      time_t tme = time(NULL);
      struct tm *currentTime = localtime(&tme);
      fprintf(T_trace, "     ****** Trunk line log file ******"
                       "\r     ******  Date: %02d/%02d/%04d   ******"
                       "\r     Trunk_cl -d : trace all Trunk line activities\n",
                        currentTime->tm_mday, currentTime->tm_mon + 1, currentTime->tm_year + 1900);
   }

   //*******************************************************************************
   //* Prepare the line connections
   //* A parallel connection will be established to send RS232 signals to the LIB
   //* these signals are used to steer the action of the 3705 scanner
   //*******************************************************************************
   // Assign IP addr and PORT numbers.

   snprintf(portl1a, 6, "%d", LINEBASE + line1num);
   if ( (rc = getaddrinfo(host1name,portl1a,NULL,&line1ahost )) != 0 ) {
      printf("\rTrunk: Cannot resolve first 3705 address for %s\n", host1name);
      return; /* error */
    }  // End if ( (rc = getaddrinfo

   // Line-1 additional port for full duplex
   if (Fdx1) {
      snprintf(portl1b, 6, "%d", LINEBASE + line1num + 1);
      if ( (rc = getaddrinfo(host1name,portl1b,NULL,&line1bhost )) != 0 ) {
         printf("\rTrunk: Cannot resolve first 3705 address for %s\n", host1name);
         return; /* error */
       } // End if ( (rc = getaddrinfo
    } // End  if (Fdx)

   snprintf(portl2a, 6, "%d", LINEBASE + line2num);
   if ( (rc = getaddrinfo(host2name,portl2a,NULL,&line2ahost )) != 0 ) {
      printf("\rTrunk: Cannot resolve second 3705 address for %s\n", host2name);
      return; /* error */
    }  // End if ( (rc = getaddrinfo

   // Line-2 additional port for full duplex
   if (Fdx2) {
      snprintf(portl2b, 6, "%d", LINEBASE + line2num + 1);
      if ( (rc = getaddrinfo(host2name,portl2b,NULL,&line2bhost )) != 0 ) {
         printf("\rTrunk: Cannot resolve second 3705 address for %s\n", host2name);
         return; /* error */
       } // End if ( (rc = getaddrinfo
    } // End  if (Fdx)

   // Line-1 socket creation
   line1a_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (line1a_fd <= 0) {
      printf("\rTrunk: Cannot create socket socket for line 1\n");
      return;
   }
   // Line-1 additional socket creation for full duplex
   if (Fdx1) {
      line1b_fd = socket(AF_INET, SOCK_STREAM, 0);
      if (line1b_fd <= 0) {
         printf("\rTrunk: Cannot create socket socket for line 1 (Full Duplex)\n");
         return;
      } // End if (line1b_fd <= 0)
   } // End if (Fdx)

   // Line-2 socket creation
   line2a_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (line2a_fd <= 0) {
      printf("\rTrunk: Cannot create socket socket for line 2\n");
      return;
   }
   // Line-2 additional socket creation for full duplex
   if (Fdx2) {
      line2b_fd = socket(AF_INET, SOCK_STREAM, 0);
      if (line2b_fd <= 0) {
         printf("\rTrunk: Cannot create socket socket for line 2 (Full Duplex)\n");
         return;
      } // End if (line2b_fd <= 0)
   } // End if (Fdx)


   // Initialize state values
   line1state = NRDY;   // Line initially not ready
   line2state = NRDY;   // Line initially not ready
   //raiseRTS = ON;       // RTS should be raised after lines are connected

   while (1) {
      if (line1state == NRDY) {
         // Line and signal sockets have been created. The connection to the LIBs will be done next
         if (!(IsSocketConnected(line1a_fd))) {
            rc1 = connect(line1a_fd, line1ahost->ai_addr,line1ahost->ai_addrlen);
         }  // End if (!(IsSocketConnected(line1a_fd)))
         rc2 = 0;
         if (Fdx1) {
            if (!(IsSocketConnected(line1b_fd))) {
               rc2 = connect(line1b_fd, line1bhost->ai_addr,line1bhost->ai_addrlen);
            } // End if (!(IsSocketConnected(line1a_fd)))
         } // End if (Fdx)
         if ((rc1 == 0) && (rc2 == 0)) {
            printf("\rTrunk: Line 1 connection has been established\n");
            line1state = RDY;    // Line is now ready
         }  // End if ((rc1 == 0) && (rc2 == 0))
      }  // End  if (line1state == NRDY)

      if (line2state == NRDY) {
         if (!(IsSocketConnected(line2a_fd))) {
            rc1 = connect(line2a_fd, line2ahost->ai_addr,line2ahost->ai_addrlen);
         }  // End if (!(IsSocketConnected(line2a_fd)))
         rc2 = 0;
         if (Fdx2) {
            if (!(IsSocketConnected(line2b_fd))) {
               rc2 = connect(line2b_fd, line2bhost->ai_addr,line2bhost->ai_addrlen);
            } // End if (!(IsSocketConnected(line1a_fd)))
         } // End if (Fdx)
         if ((rc1 == 0) && (rc2 == 0)) {
            printf("\rTrunk: Line 2 connection has been established\n");
            line2state = RDY; // Line is now ready
         }  // End if ((rc1 == 0) && (rc2 == 0))
      }  // End  if (line12state == NRDY)

      if ((line1state == RDY) && (line2state == RDY)) {
         //*****************************************************************************************
         // Check if there is data to be received from line 1
         // If the connection was lost, try to re-establish
         //*****************************************************************************************
         pendingrcv = 0;

         if (IsSocketConnected(line1a_fd)) {
            rc = ioctl(line1a_fd, FIONREAD, &pendingrcv);
            // *** Keep the below for now; under investgation ***
         } else {
            printf("\rTrunk: Line 1 connection dropped, trying to re-establish\n");
            // Line a socket re-creation. First close the sockets
            close(line1a_fd);                      // Close line connection
            if (Fdx1)
               close(line1b_fd);                  // Close Full Duplex line connection
            line1state = NRDY;                    // Set line to Not Ready
            line1a_fd = 0;                         // Reset file descriptor for line connection
            if (Fdx1)
               line1b_fd = 0;                      // Reset file descriptor for Full Duplex line connection
            line1a_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (line1a_fd <= 0) {
               printf("\rTrunk: Cannot create line-1 socket\n");
               return;
            }  // End if (line_fd <= 0)
            if (Fdx1) {
               line1b_fd = socket(AF_INET, SOCK_STREAM, 0);
               if (line1b_fd <= 0) {
                  printf("\rTrunk: Cannot create line-1 Full Duplex socket\n");
                  return;
               }  // End if (line_fd <= 0)
            } // End if (Fdx)
         }  // End if (IsSocketConnected(line1a_fd))

         if (pendingrcv > 0) {
            LINErlen = read(line1a_fd, LINE_rbuf, sizeof(LINE_rbuf));
            if (Tdbg_flag == ON) {
               fprintf(T_trace, "%s Line 1 -> Line 2 Transmit Buffer: ",TimeStamp());
               for (int i = 0; i < LINErlen; i ++) {
                  fprintf(T_trace, "%02X ", LINE_rbuf[i]);
               }  // End for (int i = 0;
               fprintf(T_trace, "\n");
               fflush(T_trace);
            }  // End if debug
            // Forward the received data to the other line
            if (Fdx2)
               rc = send(line2b_fd, LINE_rbuf, LINErlen, 0);
            else
               rc = send(line2a_fd, LINE_rbuf, LINErlen, 0);
            if (LINErlen != rc) {
               if (Tdbg_flag == ON)
                  fprintf(T_trace, "%s Trunk: Line 2 Transmit buffer size %d bytes, actual transmitted %d bytes\n",TimeStamp(),LINErlen,rc);
            }  // End  if (LINErlen != rc
         }  // End if (pendingrcv > 0)

         //*****************************************************************************************
         // Check if there is data to be received from line 2
         // If the connection was lost, try to re-establish
         //*****************************************************************************************
         pendingrcv = 0;
         if (IsSocketConnected(line2a_fd)) {
            rc = ioctl(line2a_fd, FIONREAD, &pendingrcv);
         } else {
            printf("Trunk: Line 2 connection dropped, trying to re-establish\n");
            // Lower RTS so remote side stops sending
            // Line and signal socket re-creation. First close the sockets
            close(line2a_fd);                      // Close data connection
            if (Fdx2)
               close(line2b_fd);                   // Close data connection
            line2state = NRDY;                    // Set line to Not Ready
            line2a_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (line2a_fd <= 0) {
               printf("Trunk: Cannot create line-2 socket\n");
               return;
            }  // End if (line2a_fd <= 0)
            if (Fdx2) {
               line2b_fd = socket(AF_INET, SOCK_STREAM, 0);
               if (line2b_fd <= 0) {
                  printf("Trunk: Cannot create line-2 Full Duplex socket\n");
                  return;
               }  // End if (line_fd <= 0)
            } // End if (Fdx)
         }  // End if (IsSocketConnected(line2a_fd))

         if (pendingrcv > 0) {
            LINErlen = read(line2a_fd, LINE_rbuf, sizeof(LINE_rbuf));
            if (Tdbg_flag == ON) {
               fprintf(T_trace, "%s Line 2 -> Line 1 Transmit Buffer: ",TimeStamp());
               for (int i = 0; i < LINErlen; i ++) {
                  fprintf(T_trace, "%02X ", LINE_rbuf[i]);
               } // End for (int i = 0;
               fprintf(T_trace, "\n");
               fflush(T_trace);
            }  // End if debug
            // Forward the received data to the other line
            if (Fdx1)
               rc = send(line1b_fd, LINE_rbuf, LINErlen, 0);
            else
               rc = send(line1a_fd, LINE_rbuf, LINErlen, 0);
            if (LINErlen != rc) {
               if (Tdbg_flag == ON)
                  fprintf(T_trace, "%s Trunk: Line 1 Transmit buffer size %d bytes, actual transmitted %d bytes\n",TimeStamp(),LINErlen,rc);
            }  // End  if (LINErlen != rc
         }  // End if (pendingrcv > 0)
      }  // End if ((line1state == RDY) && (line2state == RDY))
   }  // End while (1)
   return;
}

/*-------------------------------------------------------------------*/
/* Check if there is read activiy on the socket                      */
/* This is used by the caller to detect a connection break           */
/*-------------------------------------------------------------------*/
int  SocketReadAct(int fd) {
   int rc;
   fd_set fdset;
   struct timeval timeout;
   timeout.tv_sec = 0;
   timeout.tv_usec = 0;
   FD_ZERO(&fdset);
   FD_SET(fd, &fdset);
   return select(fd + 1, &fdset, NULL, NULL,  &timeout);
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

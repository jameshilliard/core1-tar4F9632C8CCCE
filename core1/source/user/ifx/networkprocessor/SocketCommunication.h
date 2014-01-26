/****************************************************************************
       Copyright (c) 2000, Infineon Technologies.  All rights reserved.

                               No Warranty
   Because the program is licensed free of charge, there is no warranty for
   the program, to the extent permitted by applicable law.  Except when
   otherwise stated in writing the copyright holders and/or other parties
   provide the program "as is" without warranty of any kind, either
   expressed or implied, including, but not limited to, the implied
   warranties of merchantability and fitness for a particular purpose. The
   entire risk as to the quality and performance of the program is with
   you.  should the program prove defective, you assume the cost of all
   necessary servicing, repair or correction.

   In no event unless required by applicable law or agreed to in writing
   will any copyright holder, or any other party who may modify and/or
   redistribute the program as permitted above, be liable to you for
   damages, including any general, special, incidental or consequential
   damages arising out of the use or inability to use the program
   (including but not limited to loss of data or data being rendered

   inaccurate or losses sustained by you or third parties or a failure of
   the program to operate with any other programs), even if such holder or
   other party has been advised of the possibility of such damages.
 ****************************************************************************
     Description:     
      Sow Jin Sze 13_05_2004: Socket TCP Stream for VxWorks
      Sow-Jin.Sze@infineon.com
 ***************************************************************************/
#include "sys/types.h"
#include "sys/uio.h"
#include <errno.h>
#include "sys/ioctl.h"
#include "fcntl.h"
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <net/if.h>
#include <strings.h>

typedef int INT;

typedef INT VOS_SOCKET;
typedef struct fd_set  VOS_FD_SETS;
typedef struct addr_struct VOS_ADDR_STRUCT;

#define true 1
#define false 0

#define WAIT_FOREVER NULL

typedef enum
{
   VOS_WAIT_FOREVER = WAIT_FOREVER,      /* Blocking */
   /*VOS_NO_WAIT      = NO_WAIT */           /* Non blocking */
} VOS_TIMEOUT_TYPE;


/* -------------------------------------------------------------------------- */
/* IP SOCKET SECTION                                                          */
/* -------------------------------------------------------------------------- */
void VOS_FD_ZERO(fd_set *pFdSet);
void VOS_FD_SET(INT nFd, fd_set *pFdSet);
INT VOS_Select ( INT nFd, fd_set *pRead, fd_set *pWrite, fd_set *pException,
                  INT nTimeout );
INT VOS_FD_GET(INT nFd, fd_set *pFdSet);
VOS_SOCKET VOS_Accept_Socket(VOS_SOCKET socketID,  struct sockaddr *pAddr);
INT VOS_Recv_Socket(VOS_SOCKET SocketID, char *pBuffer, INT nLength);
INT VOS_Send_Socket(VOS_SOCKET SocketID, char *pBuffer, INT nLength);
int VOS_Close_Socket (INT nFd);
INT VOS_Listen_Socket(VOS_SOCKET socketID, int nBacklog);


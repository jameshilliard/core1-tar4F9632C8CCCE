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
   distribute the program as permitted above, be liable to you for
   damages, including any general, special, incidental or consequential
   damages arising out of the use or inability to use the program
   (including but not limited to loss of data or data being rendered
   inaccurate or losses sustained by you or third parties or a failure of
   the program to operate with any other programs), even if such holder or
   other party has been advised of the possibility of such damages.
 ******************************************************************************
   Description:     
      Sow Jin Sze 13_05_2004: Socket TCP Stream for VxWorks
   
 ******************************************************************************/
#include "SocketCommunication.h"
#define ERROR -1

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

/******************************************************************************
Description:
   Clear a VOS_FD_SETS structure.
Arguments:
   pFdSet -  specifies the structure
Return Value:
   None.
Remarks:
   This function will be used in context with VOS_Select.
******************************************************************************/
void VOS_FD_ZERO(fd_set *pFdSet)
{
   FD_ZERO(pFdSet); 
}

/******************************************************************************
Description:
   Set in a VOS_FD_SETS structure the file descriptor bit.
Arguments:
   nFd -  specifies the file descriptor numer of the device
   pFdSet -  specifies the structure to clear
Return Value:
   None.
Remarks:
   This function will be used in context with VOS_Select.
******************************************************************************/
void VOS_FD_SET(INT nFd, fd_set *pFdSet)
{
       FD_SET(nFd,pFdSet);
}    

/******************************************************************************
Description:
   Pend (wait) on multiple file descriptors.
Arguments:
   nFd            -  specifies the file descriptor numer of the device
   pRead          - specifies the file descriptors
   pWrite         - not used
   pException     - not used
   nTimeout       - specifies behaviour if event is not available:
                    VOS_NO_WAIT: do not wait for the event
                    VOS_WAIT_FOREVER: wait till event is available
                    other int value: number of system ticks for timeout
Return Value:
   Returns 'false' in case of an error, otherwise 'true'.
Remarks:
   None. 
******************************************************************************/
INT VOS_Select ( INT nFd,
                  fd_set *pRead,
                  fd_set *pWrite,
                  fd_set *pException,
                  INT nTimeout )
{
   struct timeval   tv, *pTime;

   pTime = &tv;
   
   /* Socket Blocks Continously Currently : Wait Forever */
   pTime=NULL;

   /* call selct function itself */
   if(select(nFd, pRead, pWrite, pException,  pTime) == ERROR)
   {
      printf(" TCP_IP Server select error\n");
      return 0;
   }
   return 1;
}

/******************************************************************************
Description:
   Check a VOS_FD_SETS structure if the file descriptor bit is set.
Arguments:
   nFd -  specifies the file descriptor numer of the device
   pFdSet -  specifies the structure
Return Value:
   Returns 'true' if the file descriptor bit is set.
Remarks:
   This function will be used in context with VOS_Select.
******************************************************************************/
INT VOS_FD_GET(INT nFd, fd_set *pFdSet)
{
   if(FD_ISSET(nFd,pFdSet) !=0)
	return 1;
}

/******************************************************************************
Description:
   Accept a Connection from the socket.
Arguments:
   socketID   -  specifies the socket. Value has to be greater or equal zero
   pAddr      -  specifies a poINTer to the VOS address structure
Return Value:
   Returns the socket of the new accept connection. Is negative if an error
   occurs.
Remarks:
   None
******************************************************************************/
VOS_SOCKET VOS_Accept_Socket(VOS_SOCKET socketID,  struct sockaddr *pAddr)
{
   VOS_SOCKET tmp;
   INT addrlen = sizeof (struct sockaddr);

   tmp = accept(socketID, (struct sockaddr *)pAddr, &addrlen);
   return tmp;
}

/******************************************************************************
Description:
   Receives a package from a stream socket.
Arguments:
   socketID   -  specifies the socket. Value has to be greater or equal zero
   pBuffer    -  specifies the pointer to a buffer where the data will be copied
   nLength    -  specifies the size in byte of the buffer 'pBuffer'
Return Value:
   Returns the number of received bytes. Returns a negative value if an error
   occured
Remarks:
   None
******************************************************************************/
INT VOS_Recv_Socket(VOS_SOCKET SocketID, char *pBuffer, INT nLength)
{
   return recv(SocketID, pBuffer, nLength, 0);
}

/******************************************************************************
Description:
   Send a package to a stream socket.
Arguments:
   socketID   -  specifies the socket. Value has to be greater or equal zero
   pBuffer    -  specifies the pointer to a data buffer
   nLength    -  specifies the number of bytes should be sent
Return Value:
   Returns the number of sent bytes. Returns a negative value if an error
   occured
Remarks:
   None
******************************************************************************/
INT VOS_Send_Socket(VOS_SOCKET SocketID, char *pBuffer, INT nLength)
{
   INT n;
   n = send(SocketID, pBuffer, nLength, 0);
   return n;
}

/******************************************************************************
Description:
   Close a socket.
Arguments:
   nFd      - specifies the file descriptor numer of the socket
Return Value:
   Returns 'false' in case of an error, otherwise 'true'.
Remarks:
   None.
******************************************************************************/
INT VOS_Close_Socket (INT nFd)
{
   if (close(nFd) == ERROR)
      return false;
   return true;
}

/******************************************************************************
Description:
   Indicates that the server is willing to accept connection requests from
   clients for a TCP/IP socket.
Arguments:
   socketID   -  specifies the socket. Value has to be greater or equal zero
   nBacklog   -  specifies the number of connections to queue
Return Value:
   Returns 'false' in case of an error, otherwise 'true'.
Remarks:
   None
******************************************************************************/
INT VOS_Listen_Socket(VOS_SOCKET socketID, int nBacklog)
{
   if (listen(socketID, nBacklog) != 0)
      return false;
   return true;
}


/******************************************************************************
       Copyright (c) 2002, Infineon Technologies.  All rights reserved.
 
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
******************************************************************************/
/*
 * DANUBE_NETWORK.H
 *
 * Global DANUBE_NETWORk driver header file
 * 30-08-2005 Jin-Sze.Sow@infineon.com comments edited
 *
 */
/* Modification History
 * Sow-Jin-Sze 13_05_2004 Type Definations 
 * version 1.00.01 -- Taicheng 24_05_2006 Fixed DATA Memory Byte Read/Write 
 */
/* Socket Receive and Send functions
   multi-thread function creation, spawning
 */
/*
 * 240561:tc.chen 24/05/2006 Fixed Data memory read/write for byte access(H2D_DEBUG_WRITE_DM).
*/

#include "SocketCommunication.h"
#include "EthMp.h"
#include <string.h>
/*  Include header files for the device driver*/
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <semaphore.h>
#include <pthread.h>
#include <asm/danube/danube_mei_app.h>

#define ERROR -1
#define MSG_RECV_LENGTH 32
#define MP_FLAG_REPLY 1
#define HOST_DCE_FLAG 0x1000
#define MEI_SPACE_ACCESS 0xBE116000
//#define DEBUG_INFO
#define SEGMENT_SIZE (64*1024)

typedef struct
{
  unsigned int iAddress; // 32 bits
  unsigned int iData;    // 32 bits
}
Parameter;

typedef struct
{
  unsigned long iAddress; //32 bits
  unsigned long iReadCount;   //32 bits
  unsigned long buffer[20];  //    unsigned
}
ReadDebug_Parameter;

char danube_tcpmessage_version[]="1.00.01";

#define BYTE_SIZE   1 << 14
#define DWORD_SIZE  2 << 14
#define WORD_SIZE   0 << 14

/* Define for device driver code */
#define DANUBE_MEI_DEV                  "/dev/danube/mei"
/* -------------------------------------------------------------------------- */
/* IP SOCKET SECTION                                                          */
/* -------------------------------------------------------------------------- */
extern void VOS_FD_ZERO(fd_set *pFdSet);
extern void VOS_FD_SET(INT nFd, fd_set *pFdSet);
extern INT VOS_Select ( INT nFd, fd_set *pRead, fd_set *pWrite, fd_set *pException, INT nTimeout );
extern INT VOS_FD_GET(INT nFd, fd_set *pFdSet);
extern VOS_SOCKET VOS_Accept_Socket(VOS_SOCKET socketID,  struct sockaddr *pAddr);
extern INT VOS_Recv_Socket(VOS_SOCKET SocketID, char *pBuffer, INT nLength);
extern INT VOS_Send_Socket(VOS_SOCKET SocketID, char *pBuffer, INT nLength);
extern int VOS_Close_Socket (INT nFd);
extern INT VOS_Listen_Socket(VOS_SOCKET socketID, int nBacklog);
int TCP_Server_SendnReceiveMessage(int argc, char *argv[]); //posix thread
sem_t binary_sem;
char message[] =  "danube_posix_thread tcp_ip thread\n";
/******************************************************************************
*
* usrAppInit - initialize the users application
* added sem_init function 31-08-2005 - posix thread implementation for task priority
*/

#if 0
int main(int argc, char *argv[])
{
  int res=0;
  pthread_t *a_thread;
  void *thread_result;

  /* binary sem init */
  res = sem_init(&binary_sem,  0, 0);

  if(res!=0)
    printf("semaphore initialization failed \n");

  res = pthread_create(&a_thread, NULL, TCP_Server_SendnReceiveMessage(argc, argv), (void *)message);
  if(res!=0)
  {
    printf("tcp_ip osi layer 7 thread creation failed\n");
    return 1;
  }

  /* binary sem post */
  sem_post(&binary_sem);

  res= pthread_join(a_thread, &thread_result);
  if(res!=0)
  {
    printf("thread joined failed \n");
    return 1;
  }

  printf("TCP IP OSI LAYER 7 Send n Receive Message %s \n", (char *)thread_result);

  /* binary thread destroyed */
  sem_destroy(&binary_sem);

  return 0;

}
#endif

int main(int argc, char *argv[])
{
  TCP_Server_SendnReceiveMessage(argc, argv);
}


int Download_Firmware(char *filename, int mei_fd)
{
  int fd_image=0;
  char *buf=NULL;
  int size=0,read_size = SEGMENT_SIZE;
  struct stat file_stat;

  fd_image=open(filename, O_RDONLY);
  if (fd_image<=0)
  {
    printf("\n open %s fail.\n",filename);
    return -1;
  }
  if(lstat(filename, &file_stat)<0)
  {
    printf("\n lstat error");
    return -1;
  }
  size=file_stat.st_size;
  buf=malloc(read_size);
  if(buf==NULL)
  {
    printf("\n malloc failed in MEI main()");
    return -1;
  }
  lseek(fd_image, 0, SEEK_SET);
  lseek(mei_fd, 0, SEEK_SET);
  while(size>0)
  {
    static flag=1;
    if (size>SEGMENT_SIZE)
      read_size=SEGMENT_SIZE;
    else
      read_size=size;
    if(read(fd_image, buf, read_size)<=0)
    {
      printf("\n amazon_mei_image not present");
      return -1;
    }
    if(write(mei_fd, buf, read_size)!=read_size)
    {
      printf("\n write to mei driver fail");
      free(buf);
      return -1;
    }
    size-=read_size;
  }
  free(buf);
  close(fd_image);
}

void print_usage(char *name)
{
  printf("Usage:%s -m adsl_firmware [-d] [-i mei_device] [-a ip_address]\n", name);
  printf("example: %s -m /firmware/ModemHWE.bin\n",name);
  printf("\t-d\n\t\tEnable ARC JTAG Interface.\n");
  printf("\t-m\n\t\tSpecify the ADSL firmware file to use.\n");
  printf("\t-a\n\t\tSpecify the ip address to bind.\n");
  printf("\t-i\n\t\tSpecify the mei device to use; the default is %s.\n",DANUBE_MEI_DEV);
  printf("-- version:%s\n",danube_tcpmessage_version);
}

/*******************************************************************************
Description:
   Author: Sow-Jin-Sze 17_05_2004
   Description: This is the TCP/IP server program 
   TCP Server:
   1. Bind to Port gethostaddress
   2. Port 2000 
Arguments:
   1. Includes file SocketCommunication.c and SocketCommunication.h
Return:
   1 or 0   
********************************************************************************/

int TCP_Server_SendnReceiveMessage(int argc, char *argv[])
{
  INT         sockfd, nRet;
  INT         width,fd;
  INT         n_BytesReceived, n_BytesSent;
  fd_set      readFds,tempFds,acceptFds, tempacceptFds;
  VOS_SOCKET  socketaccept;
  char        DatastreamBuffer[512];
  struct      sockaddr_in my_addr;
  struct      in_addr serveraddr;     /* Server address : Address of Server */
  struct      sockaddr_in remoteaddr; /* client address*/
  struct      sockaddr_in receive_from_peer;
  socklen_t   namelen;
  struct      hostent *hostinfo;
  char        host[125];
  int         i;
  MPMessage   RxMsg;
  MPMessage   RxMsg1;
  MPMessage   TxMsg;
  short  int  function_opcode;
  int	       ErrorCode;
  int         MpFlag;
  int         payload;
  short int   itemp;
  int         mei_fd;
  Parameter   devicedata;
  ReadDebug_Parameter readdebugdevicedata;
  int         k,j,n;
  int         remote;
  int 	       so_reuseaddr=1;
  int         so_keepalive =1 ;
  int         so_debug =1;
  int         recv_width;
  struct      timeval tv, tv_sendtimeout;
  struct      linger struct_lg;
  unsigned long readdebug_count, readdebug_wordsize, readdebug_offset;
  // amazon_mei_read_debug message
  unsigned char *bPayload, *bData;   // 8 bits
  int        n_gpio_register;
  int  	  cmv_error = 4 ;
  char filename[512];
  char devname[512];
  int jtag_debug_enable=0;
  int ip_addr_flag=0;
  int firmare_flag=0;
  int c;

  /* Structure for passing data */
  j=0;

  /* Initialise file descriptor to zero */
  VOS_FD_ZERO(&readFds);
  VOS_FD_ZERO(&tempFds);
  memset(DatastreamBuffer, 0, sizeof(DatastreamBuffer));

  sprintf(devname,"%s",DANUBE_MEI_DEV);
  /* Set up socket parameters */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    printf("Error: Socket Created. \n");
    return 0;
  }
  /* set the socket to be reuseable */
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr));

    /* set up the struct timeval for the timeout If connected socket fails to respond to
    these messages, the connection is broken and processes writing to that socket are notified with
    an ENETRSET errno. This option takes an int value in the optval argument. This is a BOOL option */
  setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &so_keepalive, sizeof(so_keepalive) );

  /* SO_DEBUG */
  /* Enables recording of debugging information */
  setsockopt(sockfd, SOL_SOCKET, SO_DEBUG, &so_debug, sizeof(so_debug) );

  /* SO_LINGER linger on close if data present SO_LINGER controls the action taken when unsent messages are queued 
  on socket and a close(2) is performed */
  struct_lg.l_onoff=0;
  struct_lg.l_linger=0;
  setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &struct_lg, sizeof(struct_lg) );

  /* SO_RCVTIME0 and SO_SNDTIME0 Specify the sending or receiving timeouts until reporting an error. They are fixed to
  a protocol specific setting in Linux adn cannot be read or written They can be easily emulated using alarm */
  tv_sendtimeout.tv_sec=60;
  tv_sendtimeout.tv_usec=0;
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv_sendtimeout, sizeof(tv_sendtimeout) );

  while ( (c=getopt(argc, argv, "a:hi:m:d")) != EOF )
  {
    switch (c)
    {
    case 'a':
      if (inet_aton(optarg, &serveraddr) == 0)
      {
        printf("novalid ip address %s\n", optarg);
        /* Close pointers and descriptors */
        /* Return function */
        return -1;
      }
      ip_addr_flag = 1;
      break;

    case 'd':
      jtag_debug_enable = 1;
      break;
    case 'm':
      firmare_flag = 1;
      strcpy(filename,optarg);
      break;
    case 'i':
      strcpy(devname,optarg);
      break;
    case 'h':
    default:
      print_usage(argv[0]);
      /* Close pointers and descriptors */
      /* Return function */
      return -1;
    }//switch
  }//while

  if (firmare_flag==0)
  {
    printf("Error: please specify the firmware file.\n");
    print_usage(argv[0]);
    return -1;
  }

  if (argc == 2 || ip_addr_flag ==0)
  {
    /* Get own host name */
   if (gethostname(host, sizeof(host)) < 0) {
      printf("Can't get hostname");
    }else{
      printf("Hostname: %s", host);
    }
    hostinfo  = gethostbyname(host);
    printf(" HostAddress %s\n", inet_ntoa(*(struct in_addr *)*(hostinfo->h_addr_list)) );
    memcpy(&serveraddr, *hostinfo->h_addr_list, sizeof(struct in_addr));
  }


  /* open device driver file descriptor */
  mei_fd= open(devname, O_RDWR);
  if (mei_fd <= 0)
  {
    printf("ERROR: Opening %s fail.\n",devname);
    return -1;
  }

  if (jtag_debug_enable)
  {
    if(ioctl(mei_fd, DANUBE_MEI_JTAG_ENABLE)!=MEI_SUCCESS)
    {
      printf("\n DANUBE_MEI_JTAG_ENABLE failed");
      close(mei_fd);
      return -1;
    }
  }


  /* Socket Address */
  memset((char *) &my_addr, '\0', sizeof(struct sockaddr_in));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(2000); /* Listens at port 2000 */
    my_addr.sin_addr = serveraddr;

  printf("Server Address in String %s\n", inet_ntoa(my_addr.sin_addr.s_addr));

  if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))== -1)
  {
    printf("TCP_IP Error: Socket Binding Error\n");
  }

  if(listen(sockfd, 1)==ERROR)
  {
    printf("Error: Listening Error\n");
  }
  else
  {
    printf("TCP_IP Socket is Listening %d\n", sockfd);
  }

  nRet = Download_Firmware(filename,mei_fd);
  nRet=ioctl(mei_fd, DANUBE_MEI_START, NULL);
  FD_SET(sockfd, &readFds);
  width =  sockfd +1;

  /* Listen to the selected port */
  while(1)
  {
    /* Update the local file descriptor by the copy in the task parameter  */
    memcpy(&tempFds,&readFds,sizeof(fd_set));

    /* Wait for incoming events */
    n=select(width, &tempFds,(fd_set *)NULL, (fd_set *)NULL, NULL);

    /* Look if messages were received */
    if(FD_ISSET(sockfd,&tempFds)==1)
    {
      /* Received socket stream Accept connection from socket*/
      socketaccept= VOS_Accept_Socket(sockfd,(struct sockaddr *)&remoteaddr);
      printf("Client Address %s\n", inet_ntoa(remoteaddr.sin_addr.s_addr));

      /* Capture Received Message from Modem */
      /* n_BytesReceived=recv(socketaccept, (char *)&RxMsg, sizeof(RxMsg)/sizeof(unsigned short)*2 , 0); */
      /*  Process Received and Sent Data */
      while(1){
        /* Process next message from modem */
        /* time out for receive function */
        tv.tv_sec= 60;
        tv.tv_usec=0;
        FD_SET(socketaccept, &acceptFds);
        recv_width = socketaccept + 1 ;
        select(recv_width, &acceptFds,(fd_set *)NULL, (fd_set *)NULL, &tv);

        /* Check for receive time-out */
        /* Check if socket is disconnected */
        if(FD_ISSET(socketaccept ,&acceptFds)!=1){
          /* Check for receive time-out */
        }

        if(FD_ISSET(socketaccept ,&acceptFds)==1){
          n_BytesReceived=recv(socketaccept, (char *)&RxMsg,(sizeof(RxMsg)/sizeof(unsigned short))*2, 0);

          /* Error or termination from host */
          if(n_BytesReceived <=0){
            break;
          }

          ErrorCode =1;

#if defined DEBUG_INFO
          printf("RxMsg:Len:%d iFunction:%04x, Group:%04x, iAddress:%04x, iIndex:%04x \n",n_BytesReceived, RxMsg.iFunction,RxMsg.iGroup, RxMsg.iAddress, RxMsg.iIndex );
#endif
          /**If the message is the right size... (16 x 16 bit)  32 Bytes received*/
          if (n_BytesReceived == MSG_RECV_LENGTH){
            ErrorCode = 0;
            if ((RxMsg.iFunction & DCE_COMMAND) != 0)
            { 
                // Message for DCE, not Modem...
#if defined DEBUG_INFO
		printf("Message for DCE, not Modem\n");
#endif             	
              MpFlag = MP_FLAG_REPLY;
              // Obtain the function op_code
              function_opcode = (RxMsg.iFunction & 0x0FF0) >> 4;

              switch (function_opcode)
              {
              case H2DCE_DEBUG_RESET:
#if defined DEBUG_INFO
		printf("H2DC_DEBUG_RESET opcode\n");
#endif              	
                nRet=ioctl(mei_fd, DANUBE_MEI_RESET, NULL);
                if (nRet<0){
                  printf("Error TCP_IP ioctl: H2DCE_DEBUG_RESET\n");
                }else
		{
                	nRet=ioctl(mei_fd, DANUBE_MEI_START, NULL);
                	if (nRet<0){
                  		printf("Error TCP_IP ioctl: H2DCE_DEBUG_RESET\n");
                	}
		}
                function_opcode = DCE2H_DEBUG_RESET_ACK;
                TxMsg.iFunction=(function_opcode << 4) | DCE_COMMAND;
                TxMsg.iGroup=0;
                TxMsg.iAddress=0;
                TxMsg.iIndex=0;
                for (i = 0; i< NEW_MP_PAYLOAD_SIZE; i++)
                {
                  TxMsg.iPayload[i] = 0;
                }
                break;
                /*
                **	Reboot modem - Halt the modem, yank its reset line, reload its code...	
                **    then respond to WinHost
                */
              case H2DCE_DEBUG_REBOOT:
#if defined DEBUG_INFO
		printf("H2DCE_DEBUG_REBOOT opcode\n");
#endif                
                nRet=ioctl(mei_fd, DANUBE_MEI_REBOOT, NULL);
                if (nRet<0){
                  printf("Error TCP_IP ioctl: H2DCE_DEBUG_REBOOT\n");
                }else
		{
                	nRet=ioctl(mei_fd, DANUBE_MEI_START, NULL);
                	if (nRet<0){
                  		printf("Error TCP_IP ioctl: H2DCE_DEBUG_REBOOT\n");
                	}
                }
                TxMsg.iGroup=0;
                TxMsg.iAddress=0;
                TxMsg.iIndex=0;
                for (i = 0; i< NEW_MP_PAYLOAD_SIZE; i++)
                {
                  TxMsg.iPayload[i] = 0;
                }
                // Reboot and Reset has same function
                //send(socketaccept,(char *)&TxMsg, sizeof(&TxMsg), 0);
                break;

                /* 24.01.2005 : H2DCE Debug _Download */
              case H2DCE_DEBUG_DOWNLOAD:
#if defined DEBUG_INFO
		printf("H2DCE_DEBUG_DOWNLOAD opcode\n");
#endif                
                nRet = Download_Firmware(filename,mei_fd);
                if (nRet<0)
                {
                  printf("Error TCP_IP ioctl: H2DCE_DEBUG_DOWNLOAD fail.\n");
                }
                nRet=ioctl(mei_fd, DANUBE_MEI_DOWNLOAD, NULL);
                if (nRet<0){
                  printf("Error TCP_IP ioctl: H2DCE_DEBUG_DOWNLOAD\n");
                }
                TxMsg.iFunction=(function_opcode << 4) | DCE_COMMAND;
                TxMsg.iGroup=0;
                TxMsg.iAddress=0;
                TxMsg.iIndex=0;
                for (i = 0; i< NEW_MP_PAYLOAD_SIZE; i++){
                  TxMsg.iPayload[i] = 0;
                }  
                break;
                /*** 01- 09 - 2004:  H2DCE Debug_ readDebug *****/
              case H2DCE_DEBUG_READDEBUG:
#if defined DEBUG_INFO
		printf("H2DCE_DEBUG_READDEBUG opcode\n");
#endif 
                readdebugdevicedata.iAddress =(  (unsigned long)((RxMsg.iAddress & 0x0000FFFF) << 16) | (unsigned long)(RxMsg.iIndex & 0x0000FFFF ) );
                readdebug_wordsize=RxMsg.iFunction & 0xC000;
                readdebug_count= RxMsg.iFunction & 0x0F;

                function_opcode = H2DCE_DEBUG_READDEBUG_ACK;
                TxMsg.iFunction = (RxMsg.iFunction & 0xC000) |(function_opcode << 4) | HOST_DCE_FLAG |0x0010 | (RxMsg.iFunction & 0x0F);

                readdebugdevicedata.iReadCount=readdebug_count;

                switch(readdebug_wordsize){
                //     BYTE_SIZE       1 << 14
                case BYTE_SIZE:
                  if(readdebug_count % 4)// not divisible by 4
                  {
                    if( readdebugdevicedata.iAddress % 4) // Not an even boundary
                    {
                      readdebugdevicedata.iReadCount = (readdebug_count /4) + 1;
                      readdebug_offset = readdebugdevicedata.iAddress % 4;
                    }else{    // Even Dword boundary
                      readdebugdevicedata.iReadCount = (readdebug_count /4 ) + 1;
                      readdebug_offset = 0;
                    }
                  }else{  // Even DWord count
                    if( readdebugdevicedata.iAddress % 4) //        Not an even boundary
                    {
                      readdebugdevicedata.iReadCount = (readdebug_count /4) + 2;
                      readdebug_offset = readdebugdevicedata.iAddress % 4;
                    }else{  //      Even DWord boundary
                      readdebugdevicedata.iReadCount = (readdebug_count /4 );
                      readdebug_offset = 0;
                    }
                  }

                  readdebugdevicedata.iAddress = (readdebugdevicedata.iAddress & 0xFFFFFFFC );
                  // No data is returned to the CPE
                  // No offsets Only double word read is supported in this case is returned to the CPE user
                  nRet=ioctl(mei_fd, DANUBE_MEI_READDEBUG, &readdebugdevicedata);
                  if(nRet<0){
                    printf("Error TCP_IP ioctl: H2DCE_DEBUG_READDEBUG\n");
                  }
                  /* iPayLoad = 12 */
                  if(readdebugdevicedata.iReadCount > 12){
                    readdebugdevicedata.iReadCount = 12;
                  }

                  // 32 bit arc buffer = iPayload [8 bits]
                  /* send back data to useri of the following format 0xAABBCCDD */
                  /* iPayload[1]= 0xAA  0xBB  iPayload[0]= 0xCC 0xDD */

                  for(i=0; i<readdebugdevicedata.iReadCount; i++){
                    if(readdebug_offset == 0){
                      //TxMsg.iPayload[i] =(unsigned short) readdebugdevicedata.buffer[i + readdebug_offset];
                      j=i*2;
                      TxMsg.iPayload[j+1] = (short int)((readdebugdevicedata.buffer[i+readdebug_offset] & 0xFFFF0000) >> 16);
                      TxMsg.iPayload[j]   = (short int)(readdebugdevicedata.buffer[i+readdebug_offset] & 0x0000FFFF);
                    }else if (readdebug_offset == 1){
                      j=i*2;
                      //  printf("%d, %08x, %08x \n", readdebug_offset, readdebugdevicedata.buffer[j], readdebugdevicedata.buffer[j+1] );
                      /* swap the bytes to edian */
                      TxMsg.iPayload[j+1] = (short int)((readdebugdevicedata.buffer[j+1] & 0xFF000000)>> 24 )|(short int)((readdebugdevicedata.buffer[j+1] & 0x000000FF ) << 8) ;
                      TxMsg.iPayload[j]= (short int)((readdebugdevicedata.buffer[j] & 0x0000FF00) >> 8 )| (short int)((readdebugdevicedata.buffer[j+1] & 0x00FF0000)>> 8);
                      // swap due to endian */
                    }else if(readdebug_offset ==2){
                      j=i*2;
                      TxMsg.iPayload[j+1] = (short int)((readdebugdevicedata.buffer[j+1] & 0x0000FFFF) );
                      TxMsg.iPayload[j]   = (short int)((readdebugdevicedata.buffer[j] & 0xFFFF0000) >> 16 ) ;
                    }else if(readdebug_offset ==3){
                      j=i*2;
                      //printf("%d, %08x, %08x \n", readdebug_offset, readdebugdevicedata.buffer[j], readdebugdevicedata.buffer[j+1] );
                      /* swap the bytes to edian */
                      TxMsg.iPayload[j+1] = (short int)((readdebugdevicedata.buffer[j+1] & 0x0000FF00)>> 8 )|(short int)((readdebugdevicedata.buffer[j+2] & 0x00FF0000 ) >> 8) ;
                      TxMsg.iPayload[j]= (short int)((readdebugdevicedata.buffer[j+1] & 0x00FF0000) >> 16 )| (short int)((readdebugdevicedata.buffer[j+1] & 0x000000FF) << 8 );
                      // swap due to endian */
                    }
                  }
                  break;
                case WORD_SIZE:
                  /*
                  **      Read the data and send it back.
                  **      Trick cases are non-DWORD boundaries and
                  **      odd counts.
                  **
                  */
                  if(readdebug_count & 1 ) // odd number to read
                  {
                    if(readdebugdevicedata.iAddress & 2 ) // Odd Address
                    {
                      readdebugdevicedata.iReadCount = (readdebug_count /2) + 1;
                      readdebug_offset = 1;
                    }else{              // Even address
                      readdebugdevicedata.iReadCount = (readdebug_count /2) + 1;
                      readdebug_offset = 0;
                    }
                  }else{ // even number to read
                    if(readdebugdevicedata.iAddress & 2 ) // Odd Address
                    {
                      readdebugdevicedata.iReadCount = (readdebug_count /2) + 1;
                      readdebug_offset =1 ;
                    }else{       // even address
                      readdebugdevicedata.iReadCount = (readdebug_count /2);
                      readdebug_offset = 0;
                    }
                  }
                  readdebugdevicedata.iAddress = (readdebugdevicedata.iAddress & 0xFFFFFFFC );
                  // No offsets Only double word read is supported in this case is returned to the CPE user
                  if(readdebugdevicedata.iReadCount<=20){
                    // No offsets Only double word read is supported in this case is returned to the CPE user
                    nRet=ioctl(mei_fd, DANUBE_MEI_READDEBUG, &readdebugdevicedata);
                    if(nRet<0){
                      printf("Error TCP_IP ioctl: H2DCE_DEBUG_READDEBUG\n");
                    }
                    if(readdebugdevicedata.iReadCount > 12){
                      readdebugdevicedata.iReadCount = 12;
                    }

                    // 32 bit arc buffer = iPayload [8 bits]
                    /* send back data to useri of the following format 0xAAAABBBB */
     
                    for(i=0; i<readdebugdevicedata.iReadCount; i++){
                      if(readdebug_offset == 0){
                        j=i*2;
                        TxMsg.iPayload[j+1] = (short int)((readdebugdevicedata.buffer[i+readdebug_offset] & 0xFFFF0000) >> 16);
                        TxMsg.iPayload[j]   = (short int)(readdebugdevicedata.buffer[i+readdebug_offset] & 0x0000FFFF);
                      }else{
                        j=i*2;
                        TxMsg.iPayload[j+1] = (short int)((readdebugdevicedata.buffer[i+readdebug_offset] & 0x0000FFFF));
                        TxMsg.iPayload[j]   = (short int)((readdebugdevicedata.buffer[i+readdebug_offset+1] & 0xFFFF0000) >> 16);
                      }
                    }
                  }else{
                    printf("ReadCount greater than 20 \n");
                  }
                  break;
                case DWORD_SIZE:
                  /* No offsets Only double word read is supported in this case is returned to the CPE user*/
                  nRet=ioctl(mei_fd, DANUBE_MEI_READDEBUG, &readdebugdevicedata);
                  if(nRet<0){
                    printf("Error TCP_IP ioctl: H2DCE_DEBUG_READDEBUG doubleword \n");
                  }

                  readdebug_offset = 0;
                  if(readdebugdevicedata.iReadCount > 12){
                    readdebugdevicedata.iReadCount = 12;
                  }
                  /* 32 bit arc buffer = iPayload [16 bits] 0xAAAABBBB */
                  /* format: TxMsg.iPayload[1]= 0xAAAA TxMsg.iPayload[0]= 0xBBBB */
                  for(i=0; i<readdebugdevicedata.iReadCount; i++){
                    j=i*2;
                    TxMsg.iPayload[j+1] = (short int)((readdebugdevicedata.buffer[i+readdebug_offset] & 0xFFFF0000) >> 16);
                    TxMsg.iPayload[j]   = (short int)(readdebugdevicedata.buffer[i+readdebug_offset] & 0x0000FFFF);
                  }
                  break;
                default:
                  printf("Error H2DCE_DEBUG_READDEBUG\n");
                }

                TxMsg.iIndex = RxMsg.iIndex ;
                break;
                /***	Read an MEI register ***/
              case H2DCE_DEBUG_READ_MEI:
#if defined DEBUG_INFO
		printf("H2DCE_DEBUG_READ_MEI opcode\n");
#endif              
                payload = 2;

                devicedata.iAddress = RxMsg.iAddress  +  MEI_SPACE_ACCESS;
                devicedata.iData = (((RxMsg.iPayload[1] & 0x0000FFFF) <<16) + (RxMsg.iPayload [0] & 0xFFFF));
                switch(RxMsg.iGroup)	//	iGroup
                {
                case 0:	//	Both words
                  nRet=ioctl(mei_fd, DANUBE_MEI_CMV_READ, &devicedata);
                  if (nRet<0){
                    printf("Error TCP_IP ioctl: H2DCE_DEBUG_READ_MEI\n");
                  }
                  break;
                default:
                  break;
                }

                function_opcode = DCE2H_DEBUG_READ_MEI_REPLY;
                TxMsg.iFunction = (function_opcode << 4) | HOST_DCE_FLAG | payload;

                TxMsg.iAddress = RxMsg.iAddress;
                TxMsg.iIndex = RxMsg.iIndex ;

                TxMsg.iPayload[1]= 0;
                TxMsg.iPayload[0]= 0;
                TxMsg.iPayload[1]= (short int ) ((devicedata.iData & 0xFFFF0000) >> 16 );
                TxMsg.iPayload[0]= (short int ) (devicedata.iData & 0x0000FFFF);
                break;
                /*
                **	Write to MEI register
                */
              case H2DCE_DEBUG_WRITE_MEI:
#if defined DEBUG_INFO
		printf("H2DCE_DEBUG_WRITE_MEI opcode\n");
#endif               
                payload=2;
                devicedata.iAddress = RxMsg.iAddress  +  MEI_SPACE_ACCESS;
                devicedata.iData = (((RxMsg.iPayload[1] & 0x0000FFFF) <<16) + (RxMsg.iPayload [0] & 0xFFFF));
                nRet=ioctl(mei_fd, DANUBE_MEI_CMV_WRITE, &devicedata);
                if (nRet<0){
                  printf("Error TCP_IP ioctl: H2DCE_DEBUG_WRITE_MEI\n");
                }
                function_opcode = DCE2H_DEBUG_WRITE_MEI_REPLY;
                TxMsg.iFunction = (function_opcode << 4) | HOST_DCE_FLAG | payload;
                TxMsg.iAddress = RxMsg.iAddress;
                TxMsg.iIndex = RxMsg.iIndex ;

                TxMsg.iPayload[1]= 0;
                TxMsg.iPayload[0]= 0;
                TxMsg.iPayload[1]= (short int ) ((devicedata.iData & 0xFFFF0000) >> 16 );
                TxMsg.iPayload[0]= (short int ) (devicedata.iData & 0x0000FFFF);
                break;
                /*:w:W
                **	H2DCE_DEBUG_HALT
                */
              case H2DCE_DEBUG_HALT:
#if defined DEBUG_INFO
		printf("H2DCE_DEBUG_HALT opcode\n");
#endif              
                payload=1;
                nRet=ioctl(mei_fd, DANUBE_MEI_HALT, NULL);
                if (nRet<0){
                  printf("Error TCP_IP ioctl: H2DCE_DEBUG_HALT\n");
                }
                function_opcode = DCE2H_DEBUG_HALT_ACK;
                TxMsg.iFunction=(function_opcode << 4) | DCE_COMMAND | payload;
                TxMsg.iGroup=0;
                TxMsg.iAddress=0;
                TxMsg.iIndex=0;
                for (i = 0; i< NEW_MP_PAYLOAD_SIZE; i++)
                {
                  TxMsg.iPayload[i] = 0;
                }
                break;
                /*
                ** Remote
                */
              case H2DCE_DEBUG_REMOTE:
#if defined DEBUG_INFO
		printf("H2DCE_DEBUG_REMOTE opcode\n");
#endif               
                payload=1;
                remote = (RxMsg.iPayload[0] & 0xFFFF );
                nRet=ioctl(mei_fd, DANUBE_MEI_REMOTE, &remote);
                if(nRet<0){
                  printf("Error TCP_IP ioctl: H2DCE_DEBUG_REMOTE\n");
                }
                function_opcode = H2DCE_DEBUG_REMOTE + 1;
                TxMsg.iFunction = (function_opcode << 4) | DCE_COMMAND | payload;
                TxMsg.iGroup = 0;
                TxMsg.iAddress = 0;
                TxMsg.iIndex = 0;
                for (i = 0; i< NEW_MP_PAYLOAD_SIZE; i++)
                {
                  TxMsg.iPayload[i] = 0;
                }
                break;
	      case H2DCE_DEBUG_RUN:
                nRet=ioctl(mei_fd, DANUBE_MEI_RUN, NULL);
                if (nRet<0){
                  printf("Error TCP_IP ioctl: H2DCE_DEBUG_START\n");
                }
                //function_opcode = DCE2H_DEBUG_RESET_ACK;
                TxMsg.iFunction=(function_opcode << 4) | DCE_COMMAND;
                TxMsg.iGroup=0;
                TxMsg.iAddress=0;
                TxMsg.iIndex=0;
                for (i = 0; i< NEW_MP_PAYLOAD_SIZE; i++)
                {
                  TxMsg.iPayload[i] = 0;
                }
		break;
                /*
                **	Default
                */
              default:
                payload=1;
                function_opcode = DCE2H_ERROR_OPCODE_UNKNOWN;
                TxMsg.iFunction=(function_opcode << 4) | DCE_COMMAND | payload;
                TxMsg.iGroup=0;
                TxMsg.iAddress=0;
                TxMsg.iIndex=0;
                for (i = 0; i< NEW_MP_PAYLOAD_SIZE; i++)
                {
                  TxMsg.iPayload[i] = 0;
                }
                printf("ERROR TCP_IP: DCE2H_ERROR_OPCODE_UNKNOWN\n");
                break;
              }// End of switch message
            } // Message for DCE, not Modem...
            else
            {
#if defined DEBUG_INFO
		printf("Message for Modem, not DCE\n");
#endif            	
              /*** This message does not have the DCE bit set, so it's a message for the mode (i.e. CMV read, CMV write)
              send the correct message to the Mailbox of ARC wait for reply back from arch and receive it here. 
              send via mailbox of arc*/
              MpFlag = MP_FLAG_REPLY;

              /* Payload is ==1 */
              payload=1;

              function_opcode = (RxMsg.iFunction & 0x0FF0) >> 4;
              if( (function_opcode & 0x02) == 0 ){
                /* Case for CMV Message Code */
                switch (function_opcode)
                {
                case H2D_CMV_READ:
#if defined DEBUG_INFO
		printf("H2D_CMV_READ opcode\n");
#endif                 
                  /* Case for CMV read reply */
                  function_opcode = D2H_CMV_READ_REPLY;
	          payload = RxMsg.iFunction &0xF;
  
                  nRet=ioctl(mei_fd, DANUBE_MEI_CMV_WINHOST, &RxMsg);

                  if (nRet<0){
                      printf("Error TCP_IP ioctl: H2D_CMV_READ\n");
                  }

                  TxMsg = RxMsg;
                  TxMsg.iFunction = (function_opcode << 4)|payload ;
                  break;
                case H2D_CMV_WRITE:
#if defined DEBUG_INFO
		  printf("H2D_CMV_WRITE opcode\n");
                  printf("Function: %04x, iGroup: %04x, iAddress:%04x, iIndex: %04x, iPayload: %04x, iPayload: %04x \n", RxMsg.iFunction , RxMsg.iGroup, RxMsg.iAddress , RxMsg.iIndex, RxMsg.iPayload[0], RxMsg.iPayload[1] );
#endif
                  nRet=ioctl(mei_fd, DANUBE_MEI_CMV_WINHOST, &RxMsg);
                  if (nRet<0){
                    printf("Error TCP_IP ioctl: H2D_CMV_WRITE\n");
                  }
                  function_opcode = D2H_CMV_WRITE_REPLY;
                  TxMsg = RxMsg;
                  TxMsg.iFunction = (function_opcode << 4)|payload ;
                  break;
                default:
                  function_opcode=D2H_ERROR_CMV_UNKNOWN;
                  nRet=ioctl(mei_fd, DANUBE_MEI_CMV_WINHOST, &RxMsg);
                  if (nRet<0){
                    printf("Error TCP_IP ioctl: D2H_ERROR_CMV_UNKNOWN\n");
                  }
                  TxMsg = RxMsg;
                  TxMsg.iFunction = (function_opcode << 4)|payload ;
                  break;
                }
              }
              else
              {
                /* Case for Peek/Poke Messages */
                switch (function_opcode)
                {
                case H2D_DEBUG_READ_DM:
#if defined DEBUG_INFO
		printf("H2D_DEBUG_READ_DM opcode\n");
#endif                
                readdebug_wordsize=RxMsg.iFunction & 0xC000;
	        payload = RxMsg.iFunction &0xF;
		itemp = payload;
		readdebug_offset = 0;
                switch(readdebug_wordsize){
                  case BYTE_SIZE:
		    payload = (payload+1)/2 +1;
		    if (payload >12 )
			payload = 12;
		    readdebug_offset = RxMsg.iIndex %2;
		    RxMsg.iIndex -=readdebug_offset;
		    break;
                  case WORD_SIZE:
		    break;
                  case DWORD_SIZE:
		    payload = payload*2;
                    if(payload > 12){
                      payload = 12;
                    }
                  /* 32 bit arc buffer = iPayload [16 bits] 0xAAAABBBB */
                  /* format: TxMsg.iPayload[1]= 0xAAAA TxMsg.iPayload[0]= 0xBBBB */
                  break;
		    printf("Not support !!\n");
		    break;
		  }

	          RxMsg.iFunction = ((RxMsg.iFunction&(~0xF))|payload)&(~(0xC000));
                  nRet=ioctl(mei_fd, DANUBE_MEI_CMV_WINHOST, &RxMsg);
                  if (nRet<0){
                    printf("Error TCP_IP ioctl: H2D_DEBUG_READ_DM\n");
                  }
                  TxMsg = RxMsg;
                  function_opcode = D2H_DEBUG_READ_DM_REPLY;
		  payload = itemp;
                switch(readdebug_wordsize){
                  case BYTE_SIZE:
		    payload |= BYTE_SIZE;
		    break;
                  case WORD_SIZE:
		    break;
                  case DWORD_SIZE:
		    payload |= DWORD_SIZE;
                  /* 32 bit arc buffer = iPayload [16 bits] 0xAAAABBBB */
                  /* format: TxMsg.iPayload[1]= 0xAAAA TxMsg.iPayload[0]= 0xBBBB */
                  break;
		    break;
		  }
		  //offset data
		  if (readdebug_offset)
		  {
			int data_idx=0;
			for (data_idx = 0;data_idx<23;data_idx+=2)
			{
				char tmp;
				tmp = ((char *)TxMsg.iPayload)[data_idx];
				((char *)TxMsg.iPayload)[data_idx]= ((char *)TxMsg.iPayload)[data_idx+1];
				((char *)TxMsg.iPayload)[data_idx+1]= tmp;
			}
			for (data_idx = 0;data_idx<23;data_idx++)
			{
				((char *)TxMsg.iPayload)[data_idx]= ((char *)TxMsg.iPayload)[data_idx+1];
			}
			for (data_idx = 0;data_idx<23;data_idx+=2)
			{
				char tmp;
				tmp = ((char *)TxMsg.iPayload)[data_idx];
				((char *)TxMsg.iPayload)[data_idx]= ((char *)TxMsg.iPayload)[data_idx+1];
				((char *)TxMsg.iPayload)[data_idx+1]= tmp;
			}
  		  }
                  TxMsg.iFunction = (function_opcode << 4)|payload;
                  break;
                case H2D_DEBUG_READ_PM:
#if defined DEBUG_INFO
		printf("H2D_DEBUG_READ_PM opcode\n");
#endif                 
		  payload = RxMsg.iFunction &0xF;
                  nRet=ioctl(mei_fd, DANUBE_MEI_CMV_WINHOST, &RxMsg);
                  if (nRet<0){
                    printf("Error TCP_IP ioctl: H2D_DEBUG_READ_PM\n");
                  }
                  TxMsg = RxMsg;
                  function_opcode = D2H_DEBUG_READ_PM_REPLY;
                  TxMsg.iFunction = (function_opcode << 4)|payload;
                  break;
                case H2D_DEBUG_WRITE_DM:
#if defined DEBUG_INFO
                  printf("H2D_DEBUG_WRITE_DM\n");
                  printf("Function: %04x, iGroup: %04x, iAddress:%04x, iIndex: %04x, iPayload[0]:%04x, iPayload[1]: %04x  \n", RxMsg.iFunction , RxMsg.iGroup, RxMsg.iAddress , RxMsg.iIndex, RxMsg.iPayload[0], RxMsg.iPayload[1] );
#endif
                  readdebug_wordsize=RxMsg.iFunction & 0xC000; //240561:tc.chen
		  payload = RxMsg.iFunction &0xF;
                  nRet=ioctl(mei_fd,DANUBE_MEI_CMV_WINHOST, &RxMsg);
                  if (nRet<0){
                    printf("Error TCP_IP ioctl: H2D_DEBUG_WRITE_DM\n");
                  }

                  TxMsg = RxMsg;
                  function_opcode = D2H_DEBUG_WRITE_DM_REPLY;
                  TxMsg.iFunction = (function_opcode << 4)|payload | readdebug_wordsize; //240561:tc.chen
                  break;
                case H2D_DEBUG_WRITE_PM:
#if defined DEBUG_INFO
		printf("H2D_DEBUG_WRITE_PM opcode\n");
#endif                 
		  payload = RxMsg.iFunction &0xF;
                  nRet=ioctl(mei_fd, DANUBE_MEI_CMV_WINHOST, &RxMsg);
                  if (nRet<0){
                    printf("Error TCP_IP ioctl: H2D_DEBUG_WRITE_PM\n");
                  }
                  TxMsg = RxMsg;
                  function_opcode = D2H_DEBUG_WRITE_PM_REPLY;
                  TxMsg.iFunction = (function_opcode << 4)|payload;
                  break;
                default:
                  nRet=ioctl(mei_fd, DANUBE_MEI_CMV_WINHOST, &RxMsg);
                  if (nRet<0){
                    printf("Error TCP_IP default\n");
                  }
                  TxMsg = RxMsg;
                  function_opcode=D2H_ERROR_ADDR_UNKNOWN;
                  TxMsg.iFunction = (function_opcode << 4)|payload;
                }//End of switch case
              }
            }/* end of message for modem*/
            // Setup the new return Message
            if( (MP_FLAG_REPLY == MpFlag))
            {
              /* MSG_DONTWAIT Enables non-blocking operation if the operation would blacok EAGAIN is returned. #define MSG_DONTWAIT 0x40 */
              if(0 ==  ErrorCode)
              {
                /* Bytes of message sent has to be 32 bytes */
                n_BytesSent=send(socketaccept,(char *)&TxMsg,sizeof(TxMsg)/sizeof(unsigned short)*2, MSG_DONTWAIT );
                //n_BytesSent=send(socketaccept,(char *)&TxMsg,sizeof(TxMsg),0 );
                if(n_BytesSent == -1){
                  printf("Error TCP_IP: sendto error\n");
                }else{
                  /* Debug statement */
                }
              }
            }
          } /* End of message length ==32 */
        } //End of Fd_isset
      }/* End of while Loop */
      /* Clear from file descriptor */
      {
        printf("Clear and Close the file descriptor \n");
        FD_CLR(socketaccept,&acceptFds );
        VOS_Close_Socket(socketaccept);
      }
    }//end of if
  }//end of while(1)

  /* Close file descriptor */
  nRet=close(mei_fd);
  if (nRet == -1){
    printf(" Error TCP_IP: Closing file descriptor\n");
  }

  /* Free everything */
  return 0;
}




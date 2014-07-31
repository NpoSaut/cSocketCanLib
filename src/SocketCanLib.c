#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>
#include <unistd.h>

#include <stdio.h>

#include "SocketCanLib.h"

int SocketOpen (char *InterfaceName)
{
    int number;

    // Создаем висящий сокет
    number = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(number < 0)
    {
        int errsv = errno;
        return -100*errsv;
    }

    // Определяем индекс интерфейса
    struct ifreq ifr;
    strcpy(ifr.ifr_name, InterfaceName);

    // Полученный индекс будет записан в поле ifr.ifr_ifindex
    ioctl(number, SIOCGIFINDEX, &ifr);
    if(ifr.ifr_ifindex < 0)
    {
        int errsv = errno;
        return -10000*errsv;
    }

    // Включаем приём собственных сообщений
    int recv_own_msgs = 1; /* 0 = disabled (default), 1 = enabled */
    setsockopt(number, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
               &recv_own_msgs, sizeof(recv_own_msgs));
    
    // Timestamp
    const int timestamp_on = 1;
    if (setsockopt(number, SOL_SOCKET, SO_TIMESTAMP,
		    &timestamp_on, sizeof(timestamp_on)) < 0) {
      perror("setsockopt SO_TIMESTAMP");
      return -10;
    }

    // Ошибки
   can_err_mask_t err_mask = ( CAN_ERR_TX_TIMEOUT | CAN_ERR_LOSTARB | CAN_ERR_CRTL | CAN_ERR_PROT | CAN_ERR_TRX | CAN_ERR_ACK
                               | CAN_ERR_BUSOFF | CAN_ERR_BUSERROR | CAN_ERR_RESTARTED );
   if (setsockopt(number, SOL_CAN_RAW, CAN_RAW_ERR_FILTER,
              &err_mask, sizeof(err_mask)) < 0) {
      perror("setsockopt CAN_RAW_ERR_FILTER");
      return -11;
   }

    // Биндим сокет на нужный интерфейс
    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if ( bind(number, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
    {
         int errsv = errno;
         return -1000000*errsv;
    }

    return number;
}

int SocketClose (int number)
{
    if ( close (number) != 0)
    {
        int errsv = errno;
        return -1*errsv;
    }
    else
    {
        return 0;
    }
}

int SocketRead (int Socket, struct FrameBag *Bags, unsigned int BagsCount, int TimeoutMs)
{
  struct mmsghdr msgs[BagsCount];
  struct iovec iovs[BagsCount];
  char ctrlmsgs[BagsCount][(CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32)))];
  
  unsigned int i;
  for (i = 0; i < BagsCount; i++) {
    msgs[i].msg_hdr.msg_name = NULL;
    msgs[i].msg_hdr.msg_namelen = 0;
    
    iovs[i].iov_base = (void *) &Bags[i];
    iovs[i].iov_len = sizeof(struct can_frame);
    msgs[i].msg_hdr.msg_iov = &iovs[i];
    msgs[i].msg_hdr.msg_iovlen = 1;
    
    msgs[i].msg_hdr.msg_control = &ctrlmsgs[i];
    msgs[i].msg_hdr.msg_controllen = sizeof(ctrlmsgs[0]);
    
    msgs[i].msg_hdr.msg_flags = 0;
  }
  
  struct timespec timeout;
  timeout.tv_sec = TimeoutMs/1000;
  timeout.tv_nsec = (TimeoutMs%1000)*1000000;
  
  int rcount;
  rcount = recvmmsg(Socket, msgs, BagsCount, MSG_WAITFORONE, &timeout);
  
  for (i = 0; i < rcount; i ++) {
    struct timeval tv;
    struct cmsghdr *cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msgs[i].msg_hdr);
	  cmsg && (cmsg->cmsg_level == SOL_SOCKET);
	  cmsg = CMSG_NXTHDR(&msgs[i].msg_hdr,cmsg)) {
      if (cmsg->cmsg_type == SO_TIMESTAMP)
      {
	tv = *(struct timeval *)CMSG_DATA(cmsg);
	Bags[i].TimeStamp.seconds = tv.tv_sec;
	Bags[i].TimeStamp.microseconds = tv.tv_usec;
      }
    }
    
    if (msgs[i].msg_hdr.msg_flags & MSG_CONFIRM)
      Bags[i].Flags |= (1 << 0);
    
//     printf ("lib: message %02d: <%03x> [%02d] %02x %02x %02x %02x %02x %02x %02x %02x (%010ld.%06ld) +%08x\n", 
//       i,
//       Bags[i].Frame.can_id, Bags[i].Frame.can_dlc,
//       Bags[i].Frame.data[0],
//       Bags[i].Frame.data[1], 
//       Bags[i].Frame.data[2], 
//       Bags[i].Frame.data[3], 
//       Bags[i].Frame.data[4], 
//       Bags[i].Frame.data[5], 
//       Bags[i].Frame.data[6], 
//       Bags[i].Frame.data[7], 
//       Bags[i].TimeStamp.seconds,
//       Bags[i].TimeStamp.microseconds,
//       msgs[i].msg_hdr.msg_flags
//     );
  }
  
  return rcount;
}

int SocketWrite (int Socket, struct can_frame *Frame)
{
    return write(Socket, Frame, sizeof(struct can_frame)) == sizeof(struct can_frame);
}

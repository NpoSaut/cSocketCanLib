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

int SocketOpen (char *InterfaceName, int TxBuffSize, int RxBuffSize)
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
    // Нужно для того, чтобы не вырезался родитель skb->sk,
    // по которому происходит определение своего пакета  
    const int timestamping_on = 1;
    if (setsockopt(number, SOL_SOCKET, SO_TIMESTAMPING,
		    &timestamping_on, sizeof(timestamping_on)) < 0) {
      perror("setsockopt SO_TIMESTAMPING");
      return -9;
    }
    
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
    
    // Размеры буферов
    TxBuffSize = 352*TxBuffSize;
    RxBuffSize = 352*RxBuffSize;
    if (setsockopt(number, SOL_SOCKET, SO_SNDBUF,
		    &TxBuffSize, sizeof(TxBuffSize)) < 0) {
      perror("setsockopt SO_SNDBUF");
      return -12;
    }
    if (setsockopt(number, SOL_SOCKET, SO_RCVBUF,
		    &RxBuffSize, sizeof(RxBuffSize)) < 0) {
      perror("setsockopt SO_RCVBUF");
      return -13;
    }
    
    // Запрос переполнения
    const int dropmonitor_on = 1;
    if (setsockopt(number, SOL_SOCKET, SO_RXQ_OVFL,
		    &dropmonitor_on, sizeof(dropmonitor_on)) < 0) {
      perror("setsockopt SO_RXQ_OVFL");
      return -14;
    }
    
    // Биндим сокет на нужный интерфейс
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
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

void initTimers (struct timeval *now, struct timeval *end, int timeoutMs)
{
    gettimeofday (now, NULL);

    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    timeradd (now, &timeout, end);
}

int SocketRead (int Socket, struct FrameBag *Bags, unsigned int BagsCount, int TimeoutMs)
{
    struct mmsghdr msgs[BagsCount];
    struct iovec iovs[BagsCount];
    char ctrlmsgs[BagsCount][(CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32)))];

    unsigned int i;
    for (i = 0; i < BagsCount; i++)
    {
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

    struct timeval tNow, tEnd;
    for ( initTimers(&tNow, &tEnd, TimeoutMs); timercmp(&tNow, &tEnd, <); gettimeofday (&tNow, NULL) )
    {
        int rcount;
        rcount = recvmmsg(Socket, msgs, BagsCount, MSG_DONTWAIT, NULL);
        if (rcount >= 0)
        {
            for (i = 0; i < rcount; i ++)
            {
                struct timeval tv;
                struct cmsghdr *cmsg;

                for (cmsg = CMSG_FIRSTHDR(&msgs[i].msg_hdr);
                    cmsg && (cmsg->cmsg_level == SOL_SOCKET);
                    cmsg = CMSG_NXTHDR(&msgs[i].msg_hdr,cmsg))
                {
                    if (cmsg->cmsg_type == SO_TIMESTAMP)
                    {
                      tv = *(struct timeval *)CMSG_DATA(cmsg);
                      Bags[i].TimeStamp.seconds = tv.tv_sec;
                      Bags[i].TimeStamp.microseconds = tv.tv_usec;
                    }
                    else if (cmsg->cmsg_type == SO_RXQ_OVFL)
                      Bags[i].DroppedMessagesCount = *(__u32 *)CMSG_DATA(cmsg);
                }

                if (msgs[i].msg_hdr.msg_flags & MSG_CONFIRM)
                    Bags[i].Flags |= (1 << 0);
            }
            return rcount;
        }
        else
        {
            int errsv = errno;
            if (errsv == EAGAIN)
                continue;
            else
                return errsv;
        }
    }
    return 0;
}

int SocketWrite (int Socket, struct can_frame *Frame, int FramesCount)
{
  struct mmsghdr msgs[FramesCount];
  struct iovec iovs[FramesCount];
  memset (msgs, 0, sizeof (msgs));
  memset (iovs, 0, sizeof (iovs));
  
  unsigned int i;
  for (i = 0; i < FramesCount; i++) {
    msgs[i].msg_hdr.msg_name = NULL;;
    msgs[i].msg_hdr.msg_namelen = 0;
    
    iovs[i].iov_base = (void *) &Frame[i];
    iovs[i].iov_len = sizeof(struct can_frame);
    msgs[i].msg_hdr.msg_iov = &iovs[i];
    msgs[i].msg_hdr.msg_iovlen = 1;
  }
  
  int scount;
  scount = sendmmsg(Socket, msgs, FramesCount, 0);
  if ( scount < 0 )
  {
    int errsv = errno;
    return -errsv;
  }
  else
  {
    return scount;
  }
}

int SocketFlushInBuffer (int Socket)
{
  struct FrameBag bags[10];
  
  int lost = 1;
  while (lost)
  {
    lost = SocketRead (Socket, bags, 10, 1);
    if (lost < 0)
      return lost;
  }
  return 0;
}

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
#include <linux/can/j1939.h>
#include <linux/can/error.h>
#include <unistd.h>

#include <stdio.h>

#include "SocketJ1939Lib.h"

int J1939SocketOpen (const char *InterfaceName, int TxBuffSize, int RxBuffSize)
{
    int number;

    // Создаем висящий сокет
    number = socket(PF_CAN, SOCK_DGRAM, CAN_J1939);
    if(number < 0)
    {
        int errsv = errno;
        return -100*errsv;
    }

    const int disable = 0, enable = 1; // Для использования в setsockopt

    // Приём всех сообщений (не только адресованных нам)
    if ( setsockopt(number, SOL_CAN_J1939, SO_J1939_PROMISC, &enable, sizeof(int)) < 0 )
        return -10;

    // Включаем приём собственных сообщений
    if ( setsockopt(number, SOL_CAN_J1939, SO_J1939_RECV_OWN, &enable, sizeof(int)) < 0 )
        return -11;
    // Нужно для того, чтобы не вырезался родитель skb->sk,
    // по которому происходит определение своего пакета
    if ( setsockopt(number, SOL_SOCKET, SO_TIMESTAMPING, &enable, sizeof(int)) < 0 )
        return -12;

    // Timestamp
    if ( setsockopt(number, SOL_SOCKET, SO_TIMESTAMP, &enable, sizeof(int)) < 0 )
        return -13;

    // Размеры буферов
    if ( setsockopt(number, SOL_SOCKET, SO_SNDBUF, &TxBuffSize, sizeof(TxBuffSize)) < 0 )
        return -14;
    if ( setsockopt(number, SOL_SOCKET, SO_RCVBUF, &RxBuffSize, sizeof(RxBuffSize)) < 0 )
        return -15;

    // Запрос переполнения
    if ( setsockopt(number, SOL_SOCKET, SO_RXQ_OVFL, &enable, sizeof(enable)) < 0 )
        return -16;

    // Биндим сокет на нужный интерфейс

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

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    addr.can_addr.j1939.name = J1939_NO_NAME;
    addr.can_addr.j1939.addr = J1939_NO_ADDR;
    addr.can_addr.j1939.pgn = J1939_NO_PGN;
    if ( bind(number, (const sockaddr *)&addr, sizeof(addr)) < 0 )
    {
        int errsv = errno;
        return -1000000*errsv;
    }

    return number;
}

int J1939SocketClose (int number)
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

int J1939SocketRead (int Socket, struct J1939FrameBag *Bags, unsigned int BagsCount, int TimeoutMs)
{
    struct mmsghdr msgs[BagsCount];
    struct iovec iovs[BagsCount];
    struct sockaddr_can addr[BagsCount];
    char ctrlmsgs[BagsCount][
            CMSG_SPACE(sizeof(struct timeval))
          + CMSG_SPACE(sizeof(__u8)) /* dest addr */
          + CMSG_SPACE(sizeof(__u64)) /* dest name */
          + CMSG_SPACE(sizeof(__u8)) /* priority */
          ];

    unsigned int i;
    for (i = 0; i < BagsCount; i++)
    {
        memset(&msgs[i], 0, sizeof(msgs[0]));
        memset(&iovs[i], 0, sizeof(iovs[0]));
        memset(&addr[i], 0, sizeof(addr[0]));

        msgs[i].msg_hdr.msg_name = &addr[i];
        msgs[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_can);

        iovs[i].iov_base = (void *) &(Bags[i].Frame.data);
        iovs[i].iov_len = sizeof(Bags[i].Frame.data);
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
                    cmsg;
                    cmsg = CMSG_NXTHDR(&msgs[i].msg_hdr,cmsg))
                {
                    switch (cmsg->cmsg_level) {
                    case SOL_SOCKET:
                        if (cmsg->cmsg_type == SO_TIMESTAMP)
                        {
                            tv = *(struct timeval *)CMSG_DATA(cmsg);
                            Bags[i].TimeStamp.seconds = tv.tv_sec;
                            Bags[i].TimeStamp.microseconds = tv.tv_usec;
                        }
                        else if (cmsg->cmsg_type == SO_RXQ_OVFL)
                            Bags[i].DroppedMessagesCount = *(__u32 *)CMSG_DATA(cmsg);
                        break;
                    case SOL_CAN_J1939:

                        break;
                    }
                }

                Bags[i].Frame.pgn = addr[i].can_addr.j1939.pgn;
                Bags[i].Frame.length = msgs[i].msg_len;

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

//int J1939SocketWrite (int Socket, struct J1939Frame *Frame, int FramesCount)
//{
//    struct mmsghdr msgs[FramesCount];
//    struct iovec iovs[FramesCount];
//    struct sockaddr_can addr[FramesCount];

//    unsigned int i;
//    for (i = 0; i < FramesCount; i++)
//    {
//        memset (&msgs[i], 0, sizeof (msgs[0]));
//        memset (&iovs[i], 0, sizeof (iovs[0]));
//        memset (&addr[i], 0, sizeof (addr[0]));

//        addr[i].can_addr.j1939.name = J1939_NO_NAME;
//        addr[i].can_addr.j1939.addr = J1939_NO_ADDR;
//        addr[i].can_addr.j1939.pgn = Frame[i].pgn;

//        msgs[i].msg_hdr.msg_name = &addr[i];;
//        msgs[i].msg_hdr.msg_namelen = sizeof(addr[i]);

//        iovs[i].iov_base = (void *) &(Frame[i].data);
//        iovs[i].iov_len = Frame[i].length;
//        msgs[i].msg_hdr.msg_iov = &iovs[i];
//        msgs[i].msg_hdr.msg_iovlen = 1;
//    }

//    int scount;
//    scount = sendmmsg(Socket, msgs, FramesCount, 0);
//    if ( scount < 0 )
//    {
//        int errsv = errno;
//        return -errsv;
//    }
//    else
//    {
//        return scount;
//    }
//}

int J1939SocketFlushInBuffer (int Socket)
{
  struct J1939FrameBag bags[10];

  int lost = 1;
  while (lost)
  {
    lost = J1939SocketRead (Socket, bags, 10, 1);
    if (lost < 0)
      return lost;
  }
  return 0;
}

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
//    can_err_mask_t err_mask = ( CAN_ERR_TX_TIMEOUT | CAN_ERR_LOSTARB | CAN_ERR_CRTL | CAN_ERR_PROT | CAN_ERR_TRX | CAN_ERR_ACK
//                                | CAN_ERR_BUSOFF | CAN_ERR_BUSERROR | CAN_ERR_RESTARTED );
//    setsockopt(number, SOL_CAN_RAW, CAN_RAW_ERR_FILTER,
//               &err_mask, sizeof(err_mask));

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

int SocketRead (int Socket, struct can_frame *Frame)
{
  struct msghdr msg;
  
//   struct sockaddr_can addr;
//   addr.can_family = AF_CAN;
//   addr.can_ifindex = ifr.ifr_ifindex;
//   msg.msg_name = &addr;
//   msg.msg_namelen = sizeof(addr);
  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  
  struct iovec iov;
//   struct canfd_frame frame;
  iov.iov_base = Frame;
  iov.iov_len = sizeof(struct can_frame);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  
  char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32))];
  msg.msg_control = &ctrlmsg;
  msg.msg_controllen = sizeof(ctrlmsg);
  
  msg.msg_flags = 0;
  
  int nbytes;
  nbytes = recvmsg(Socket, &msg, 0);
  if (nbytes < 0) {
	  perror("read");
	  return -1;
  }
  
  struct timeval tv;
  
  struct cmsghdr *cmsg;
  for (cmsg = CMSG_FIRSTHDR(&msg);
	cmsg && (cmsg->cmsg_level == SOL_SOCKET);
	cmsg = CMSG_NXTHDR(&msg,cmsg)) {
	  if (cmsg->cmsg_type == SO_TIMESTAMP)
		  tv = *(struct timeval *)CMSG_DATA(cmsg);
  }
  
  return gettimeofday (&tv, NULL);
  
//     if ( read(Socket, Frame, sizeof(struct can_frame)) == sizeof(struct can_frame) )
//     {
//       struct timeval tv;
//       ioctl(Socket, SIOCGSTAMP, &tv);
//       return gettimeofday (&tv, NULL);
//     }
//     else
//     {
//       return -1;
//     }
}

int SocketWrite (int Socket, struct can_frame *Frame)
{
    return write(Socket, Frame, sizeof(struct can_frame)) == sizeof(struct can_frame);
}

#include "SocketJ1939Lib.h"

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

struct sockaddr_can src;
static struct iovec iov;
static struct msghdr msg;
static struct cmsghdr *cmsg;
static __u8 *buf;

static char ctrlmsg[
      CMSG_SPACE(sizeof(struct timeval))
    + CMSG_SPACE(sizeof(__u8)) /* dest addr */
    + CMSG_SPACE(sizeof(__u64)) /* dest name */
    + CMSG_SPACE(sizeof(__u8)) /* priority */
    ];

int main ()
{
    int s = J1939SocketOpen ("vcan0", J1939MaxDataLen*10, J1939MaxDataLen*10);
    J1939SocketFlushInBuffer(s);

    struct J1939FrameBag bag[3];
    memset(bag, 0, sizeof (bag));

    int i, j;
    for (i = 0; i < 100; i++)
    {
        int r;
        r = J1939SocketRead(s, bag, 3, 200);
        if (r > 0)
            for (j = 0; j < r; j ++)
                printf("message: (%x) [%x] %x%x\n", bag[j].Frame.length, bag[j].Frame.pgn, bag[j].Frame.data[0], bag[j].Frame.data[1]);
        sleep(5);
    }
    J1939SocketClose (s);
    return 0;
}

#include "SocketCanLib.h"

struct can_frame frame = { 0x111, 5, {1,2,3,4,5,6,7,8} };

int main ()
{
    int s = SocketOpen ("vcan0");
    SocketWrite (s, &frame);
    SocketClose (s);
    return 0;
}

#include "SocketCanLib.h"

struct can_frame frame = { 0x111, 5, {1,2,3,4,5,6,7,8} };

int main ()
{
    int s = SocketOpen ("vcan0", 1000, 1000);
    SocketWrite (s, &frame, 1);
    SocketClose (s);
    return 0;
}

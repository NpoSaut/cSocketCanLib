#include "SocketJ1939Lib.h"

struct J1939Frame frame = { 0x2300, 3, {0x01, 0x02, 0xAA}  };

int main ()
{
    int s = J1939SocketOpen ("vcan0", J1939MaxDataLen*10, J1939MaxDataLen*10);
//    s = J1939SocketWrite (s, &frame, 1);
    printf("write ret: %i\n", s);
    J1939SocketClose (s);
    return 0;
}

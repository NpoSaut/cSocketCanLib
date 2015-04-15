#include "SocketJ1939Lib.h"

struct J1939Frame frame = { 0x2300, {0x01, 0x02, 0xAA}  };

int main ()
{
    int s = J1939SocketOpen ("vcan0", J1939MaxDataLen*10, J1939MaxDataLen*10);
    printf("ret: %i\n", s);

    J1939FrameBag bag;
    while (true)
    {
        s = J1939SocketRead(s, &bag, 1, 10000);
        printf("read ret: $i\n", s);
        if (s > 0)
            printf("message: [%x] %x%x", bag.Frame.pgn, bag.Frame.data[0], bag.Frame.data[1]);
    }
    J1939SocketClose (s);
    return 0;
}

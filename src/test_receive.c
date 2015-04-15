#include <stdio.h>
#include <sys/time.h>

#include "SocketCanLib.h"

struct can_frame frame;
struct timeval t1, t2;

int main ()
{
    int s = SocketOpen ("vcan0", 1000, 1000);

    struct FrameBag bag;
    int yes = 0;
    gettimeofday (&t1, NULL);
    gettimeofday (&t2, NULL);
    while ( t2.tv_sec - t1.tv_sec < 1 )
    {
        if ( SocketRead (s, &bag, 1, 700) > 0
             && bag.Frame.can_id == 0x111
             && bag.Frame.can_dlc == 5
             && bag.Frame.data[0] == 1
             && bag.Frame.data[1] == 2
             && bag.Frame.data[2] == 3
             && bag.Frame.data[3] == 4
             && bag.Frame.data[4] == 5 )
        {
            yes = 1;
            break;
        }

        gettimeofday (&t2, NULL);
    }

    SocketClose (s);
    if (yes)
        printf ("Тесты пройдены.\n");
    else
        printf ("\n ~~~ !!!!!- ^ТЕСТ ЗАВАЛЕН^ -!!!! ~~~ \n\n");

    return yes ? 0 : -1;
}

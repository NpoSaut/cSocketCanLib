#include <linux/can.h>

int sf (int a);

// Открывает сокет на CAN-интерфейсе с именем InterfaceName
//  В случае успеха возвращает неотрицательный хендлер сокета
//  При ошибке возращает код ошибки в формате:
//   -100*errno для ошибок в socket()
//   -10000*errno для ошибок в ioctl()
//   -1000000*errno для ошибов в bind()
int SocketOpen (char *InterfaceName);

// Закрывает открытый сокет
//  В случае успеха возвращает 0
//  При ошибке возвращает код ошибки: -1*errno в функции close()
int SocketClose (int number);

// FrameBag Flags
// bit 0      : Loopback
typedef __u8 FrameBagFlags;

struct TimeVal
{
  unsigned int seconds;
  unsigned int microseconds;
};

struct FrameBag
{
  struct can_frame Frame;
  struct TimeVal TimeStamp;
  FrameBagFlags Flags;
};

// Читает сообщения из входящего буфера сокета.
//  При отсутствии сообщений в буфере блокируется до появления первого или истечении timeout
//  При наличии сообщений читает их и записывает в Bags
//  В случае успеха возвращает количество прочитанных сообщений (не больше BagsNumber)
//  При ошибке - отрицательный код ошибки:
//	-1	: Сокет закрыт
//	-255	: Не знамо что
int SocketRead (int Socket, struct FrameBag *Bags, unsigned int BagsCount, int TimeoutMs);

// Ставит сообщение в очередь SocketCan на отправку
//  Если очередь свободна, то не блокирует
//  Иначе блокируется до освобожения места в очереди
//  Возвращает 1 в случае успеха и отрицательный код ошибки при ошибке
int SocketWrite (int Socket, struct can_frame *Frame);

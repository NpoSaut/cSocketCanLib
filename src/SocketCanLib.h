// COPYRIGHT NPO SAUT
// mailto: nazemnykh.anton@gmail.com
// version: 2

#include <sys/socket.h> // for sa_familty_t in linux/can.h
#include <linux/can.h>

// Открывает сокет на CAN-интерфейсе с именем InterfaceName
//  TxBuffSize и RxBuffSize задают размеры входищего и сходящего буферов в сообщениях [3 .. 465] 
//  В случае успеха возвращает неотрицательный хендлер сокета
//  При ошибке возращает код ошибки в формате:
//   -100*errno для ошибок в socket()
//   -10000*errno для ошибок в ioctl()
//   -1000000*errno для ошибов в bind()
extern int SocketOpen (const char *InterfaceName, int TxBuffSize, int RxBuffSize);

// Закрывает открытый сокет
//  В случае успеха возвращает 0
//  При ошибке возвращает код ошибки: -1*errno в функции close()
int SocketClose (int number);

// FrameBag Flags
// bit 0      : Loopback
typedef __u8 FrameBagFlags;

struct __attribute__((packed, aligned(1))) TimeVal
{
  __u32 seconds;
  __u32 microseconds;
};

struct __attribute__((packed, aligned(1))) FrameBag
{
  struct can_frame Frame;
  struct TimeVal TimeStamp;
  FrameBagFlags Flags;
  __u32 DroppedMessagesCount;
};

// Читает сообщения из входящего буфера сокета.
//  Функция блокируется на время, не превыщающее TimeoutMs
//  Читает из входящего буфера драйвера не более чем BagsCount сообщений
//  В случае успеха возвращает количество прочитанных сообщений
//  При ошибке - отрицательный код ошибки:
//	-1	: Сокет закрыт
//	-255	: Не знамо что
int SocketRead (int Socket, struct FrameBag *Bags, unsigned int BagsCount, int TimeoutMs);

// Ставит сообщения в очередь SocketCan на отправку
//  Если очередь свободна, то не блокирует ???
//  Иначе блокируется до освобожения места в очереди ???
//  Возвращает 1 в случае успеха и отрицательный код ошибки при ошибке ???
int SocketWrite (int Socket, struct can_frame *Frame, int FramesCount);

// Очищает буфер принятых сообщений сокета
//  Функция блокирующая
//  При успехе возвращает 0, в случае ошибки отрицательный код ошибки
int SocketFlushInBuffer (int Socket);

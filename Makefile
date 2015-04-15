VERSION=2

ifndef OUTPUT_DIR
	ifdef CROSS_COMPILE
	OUTPUT_DIR=cross
	else
	OUTPUT_DIR=native
	endif
endif
SOURCE_DIR=src

all: clean_temp library_shared clean_temp

test: test_static_send test_shared_receive
	./test_shared_receive &
	./test_static_send

test_static_send: library_static
	$(CROSS_COMPILE)gcc $(CFLAGS) -static $(SOURCE_DIR)/test_send.c -L. -lSocketCanLib -o test_static_send
	$(CROSS_COMPILE)gcc $(CFLAGS) -static $(SOURCE_DIR)/test_j1939_send.c -L. -lSocketJ1939Lib -o test_static_j1939_send

test_shared_receive: library_shared
	$(CROSS_COMPILE)gcc $(CFLAGS) $(SOURCE_DIR)/test_receive.c -o test_shared_receive -L. -lSocketCanLib
	$(CROSS_COMPILE)gcc $(SOURCE_DIR)/test_receive.c -o test_shared_receive -L. -lSocketCanLib

library_shared: library_object
	$(CROSS_COMPILE)gcc $(CFLAGS) -shared -Wl,-soname,libSocketCanLib.so.$(VERSION) -o libSocketCanLib.so.$(VERSION) SocketCanLib.o
	$(CROSS_COMPILE)gcc $(CFLAGS) -shared -Wl,-soname,libSocketJ1939Lib.so.$(VERSION) -o libSocketJ1939Lib.so.$(VERSION) SocketJ1939Lib.o
	cp libSocketCanLib.so.$(VERSION) $(OUTPUT_DIR)/
	cp libSocketJ1939Lib.so.$(VERSION) $(OUTPUT_DIR)/

library_static: library_object
	ar rcs libSocketCanLib.a SocketCanLib.o
	ar rcs libSocketJ1939Lib.a SocketJ1939Lib.o
	cp libSocketCanLib.a $(OUTPUT_DIR)/
	cp libSocketJ1939Lib.a $(OUTPUT_DIR)/

library_object:
	$(CROSS_COMPILE)gcc $(CFLAGS) -c -fPIC $(SOURCE_DIR)/SocketCanLib.c -o SocketCanLib.o
	$(CROSS_COMPILE)gcc $(CFLAGS) -c -fPIC $(SOURCE_DIR)/SocketJ1939Lib.c -o SocketJ1939Lib.o

clean_temp:
	rm -rf *.o *.a *.dll *.so.* test_*
	
clean: clean_temp
	rm $(OUTPUT_DIR)/*.a $(OUTPUT_DIR)/*.so.* -rf

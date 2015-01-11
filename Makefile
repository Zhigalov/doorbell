all:
	cc hid-libusb.c -o hid-libusb.o -c
	cc main.c -o main.o -c
	cc -o main main.o hid-libusb.o -lusb-1.0

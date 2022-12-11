all: thermcam2 read_shm ctrlr imgcomb thaw

example-0: example-0.c
	gcc `pkg-config --cflags gtk+-3.0` -o example-0 example-0.c `pkg-config --libs gtk+-3.0`


doc_libvlc_gtk_player: doc_libvlc_gtk_player.c
	gcc -o gtk_player gtk_player.c `pkg-config --libs gtk+-3.0 libvlc` `pkg-config --cflags gtk+-3.0 libvlc`

ctrlr: ctrlr2.c globcss.h dispftns.c dispftns.h mkbmp.c
	# g++ -c dispftns.c -fpermissive -std=c++11
	gcc `pkg-config --cflags gtk+-3.0` -w -o ctrlr mkbmp.c ctrlr2.c dispftns.c `pkg-config --libs gtk+-3.0` -lm -Wint-conversion -Wincompatible-pointer-types                     

thermcam2: thermcam3.c mkbmp.c i2cctl.c dispftns.c
	g++ -c thermcam3.c -fpermissive -w -Wreturn-local-addr -std=c++11
	g++ -c dispftns.c -fpermissive -w -std=c++11
	gcc -c i2cctl.c -w -std=c11
	gcc -c mkbmp.c -w -std=c11
	g++ dispftns.o thermcam3.o i2cctl.o mkbmp.o "mlx90640-library/libMLX90640_API.a" -o thermcam2 -w -lwiringPi  -lm -std=c11 -Wreturn-local-addr -fpermissive
	

read_shm: readshm.c
	gcc readshm.c -o readshm -w -lm -Wcpp -std=c11

imgcomb: imgcmbsm.c dispftns.c mkbmp.c
	gcc imgcmbsm.c dispftns.c mkbmp.c -o ~/imgcomb -w -ljpeg -lpng -lm -std=c99

thaw: thaw.c
	gcc thaw.c -o thaw -w -lm -std=c99

example-4: example-4.c
	gcc `pkg-config --cflags gtk+-3.0` -o example-4 example-4.c `pkg-config --libs gtk+-3.0`



#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <stdlib.h>

#define BUFSIZE 32

const char* ip = "unknown";
in_port_t port;
uint64_t counter = 0;

struct timespec sleep_time = {
	.tv_sec = 0,
	.tv_nsec = 300000000
};

void* print_info(void* unused) {
	for(;;){
		printf("\e[5;0HUDP Pixelflut\nnc -u %s %d\npixels/s: %6" PRIu64, ip, port, counter*1000000000/((uint64_t)sleep_time.tv_nsec));
		counter = 0;
		nanosleep(&sleep_time, NULL);
	}
	return NULL;
}

int main(const int argc, const char const * const * argv) {
	if(argc < 2){
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		return 1;
	} else {
		port = atoi(argv[1]);
	}
	int width, height, bitspp, bytespp;
	uint32_t *data;

	// Öffnen des Gerätes
	int fd = open("/dev/fb0", O_RDWR);

	// Informationen über den Framebuffer einholen
	struct fb_var_screeninfo screeninfo;
	ioctl(fd, FBIOGET_VSCREENINFO, &screeninfo);

	// Beende, wenn die Farbauflösung nicht 32 Bit pro Pixel entspricht
	printf("Res: %ix%i, %i bpp\n", screeninfo.xres, screeninfo.yres, screeninfo.bits_per_pixel);
	bitspp = screeninfo.bits_per_pixel;
	if(bitspp != 32) {
		// Ausgabe der Fehlermeldung
		printf("Farbaufloesung = %i Bits pro Pixel\n", bitspp);
		printf("Bitte aendern Sie die Farbtiefe auf 32 Bits pro Pixel\n");
		close(fd);
		return 1; // Für den Programmabbruch geben wir einen Rückgabetyp != 0 aus.
	}

	width  = screeninfo.xres;
	height = screeninfo.yres;
	bytespp = bitspp/8; //Bytes pro Pixel berechnen

	// Zeiger auf den Framebufferspeicher anfordern
	data = (unsigned int*) mmap(0, width * height * bytespp, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	const int udpsock = socket(AF_INET6, SOCK_DGRAM, 0);
	struct sockaddr_in6 my_addr = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(port),
		.sin6_addr = IN6ADDR_ANY_INIT,
	};
	if(0 != bind(udpsock, (struct sockaddr*)&my_addr, sizeof(my_addr))) {
		perror("Could not bind");
	}
	// TODO: getifaddr() benutzen
	pthread_t print_info_thread;
	pthread_create(&print_info_thread, NULL, print_info, NULL);
	char buf[BUFSIZE];
	for(;;){
		if(0 > recv(udpsock, (void*)buf, BUFSIZE, 0)) {
			perror("Error in receive");
		} else {
			unsigned int x,y,r,g,b,n;
			n = sscanf(buf, "PX %d %d %02x%02x%02x", &x, &y, &r, &g, &b);
			if (n == 5 && x<width && y<height) {
				data[y*width+x] = r<<16 | g<<8 | b;
			}
			++counter;
		}
	}

	// Zeiger wieder freigeben
	munmap(data, width * height * bytespp);
	// Socket schließen
	close(udpsock);
	// Gerät schließen
	close(fd);
	// Rückgabewert
	return 0;
}

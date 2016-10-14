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
#include <linux/if_link.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <string.h>

#define BUFSIZE 32

in_port_t port;
uint64_t counter = 0;
char ** addresses = {0};

struct timespec sleep_time = {
	.tv_sec = 0,
	.tv_nsec = 300000000
};

void* print_info(void* unused) {
	for(;;){
		printf("\e[5;0HUDP Pixelflut\npixels/s: %6" PRIu64, counter*1000000000/((uint64_t)sleep_time.tv_nsec));
		printf("\nListening on port %d\n", port);
		for(int i=0; addresses[i] != NULL; ++i) {
			printf("\t%s\n", addresses[i]);
		}
		counter = 0;
		nanosleep(&sleep_time, NULL);
	}
	return NULL;
}

int is_loopback(struct sockaddr* sa) {
	if(sa->sa_family == AF_INET) {
		struct in_addr ia = ((struct sockaddr_in*)sa)->sin_addr;
		return (ntohl(ia.s_addr) == INADDR_LOOPBACK); // FIXME: byte order issue
	} else if (sa->sa_family == AF_INET6) {
		struct in6_addr ia = ((struct sockaddr_in6*)sa)->sin6_addr;
		return !memcmp(&ia, &in6addr_loopback, sizeof(struct in6_addr));
	}
	return 0;
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

	// IP-Adressen ermitteln
	struct ifaddrs *ifaddr = NULL, *ifa = NULL;
	int n, s, family;
	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		return 1;
	}
	for (ifa = ifaddr, n=0; ifa != NULL; ifa=ifa->ifa_next){ // zähle Adressen
		if (ifa->ifa_addr != NULL && !is_loopback(ifa->ifa_addr)) {
			++n;
		}
	}
	addresses = calloc(n+1, sizeof(void *));
	if (addresses == NULL) {
		perror("calloc");
		return 1;
	}
	for (ifa = ifaddr, n=0; ifa != NULL; ifa=ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}
		family = ifa->ifa_addr->sa_family;
		if ((family == AF_INET || family == AF_INET6) && !is_loopback(ifa->ifa_addr)) {
			addresses[n] = calloc(NI_MAXHOST, sizeof(char));
			if (addresses == NULL) {
				perror("calloc");
				return 1;
			}
			s = getnameinfo(ifa->ifa_addr,
				(family == AF_INET) ?
					sizeof(struct sockaddr_in) :
					sizeof(struct sockaddr_in6),
				addresses[n], NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
				if (s != 0) {
					printf("getnameinfo() failed: %s\n", gai_strerror(s));
					return 1;
				}
			strtok(addresses[n], "%");
			n++;
		}
	}
	freeifaddrs(ifaddr);


	const int udpsock = socket(AF_INET6, SOCK_DGRAM, 0);
	struct sockaddr_in6 my_addr = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(port),
		.sin6_addr = IN6ADDR_ANY_INIT,
	};
	if(0 != bind(udpsock, (struct sockaddr*)&my_addr, sizeof(my_addr))) {
		perror("Could not bind");
		return 1;
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

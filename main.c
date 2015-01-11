#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "hidapi.h"


char flag = 1;

void child_handler(int i) {
    int status;
    wait(&status);
    printf("process finished\n");
    flag = 0;
}


#define SPACE       0
#define DIGIT       1
#define CHAR        2
#define EXCL        3
#define DLMR        4

#define STATE_0     0
#define STATE_1     1
#define STATE_2     2
#define STATE_3     3
#define STATE_4     4
#define STATE_5     5
#define STATE_6     6
#define STATE_7     7
#define STATE_F     8
#define STATE_E     9

struct {
	int  code;
	char excl;
	char bell[512];
} map[1024];
int m_count = 0;

struct token {
	char c;
	char t;
} *tokens;
int t_count = 0;


unsigned char q[][5] = {
//  SPACE    DIGIT    CHAR     EXCL     DLMR
    STATE_0, STATE_1, STATE_E, STATE_E, STATE_0,     // STATE_0
    STATE_E, STATE_2, STATE_E, STATE_E, STATE_E,     // STATE_1
    STATE_E, STATE_3, STATE_E, STATE_E, STATE_E,     // STATE_2
    STATE_E, STATE_4, STATE_E, STATE_E, STATE_E,     // STATE_3
    STATE_6, STATE_E, STATE_E, STATE_5, STATE_E,     // STATE_4
    STATE_6, STATE_E, STATE_E, STATE_E, STATE_E,     // STATE_5
    STATE_6, STATE_7, STATE_7, STATE_7, STATE_E,     // STATE_6
    STATE_7, STATE_7, STATE_7, STATE_7, STATE_F,     // STATE_7
	STATE_F, STATE_F, STATE_F, STATE_F, STATE_F,     // STATE_F
    STATE_E, STATE_E, STATE_E, STATE_E, STATE_E      // STATE_E
};
unsigned char st = 0;
int t_index = 0;

unsigned char code[4];
unsigned char excl;
char bell[512];
int  b_index;

void state_0(char c) {}

void state_1(char c) {
	code[0] = c - 48;
}

void state_2(char c) {
	code[1] = c - 48;
}

void state_3(char c) {
	code[2] = c - 48;
}

void state_4(char c) {
	code[3] = c - 48;
}

void state_5(char c) {
	excl = 1;
}

void state_6(char c) {}

void state_7(char c) {
	bell[b_index++] = c;
}

void state_f(char c) {
	t_index--;
	bell[b_index] = 0;

	map[m_count].code = 1000* code[0] + 100 * code[1] + 10 * code[2] + code[3];
	map[m_count].excl = excl;
	strncpy(map[m_count].bell, bell, 512);
	m_count++;

	st = 0;
	b_index = 0;
	excl = 0;
}

void state_e(char c) {
	printf("parsing error\n");
}

void (*qfuncs[])(char c) = {
	state_0,
	state_1,
	state_2,
	state_3,
	state_4,
	state_5,
	state_6,
	state_7,
	state_f,
	state_e
};


hid_device *handle;
unsigned char buff[4] = {0, 0, 0, 0};

unsigned char state = 0;

unsigned char c = 0;
unsigned char code[4] = {0, 0, 0, 0};

unsigned char s = 0;
unsigned char d = 0;

int i = 0;

void readDevice() {
	if (buff[2] & 0b10000000) {
		printf(">>> BIG RED BUTTON ;-) <<<\n");
	}

	if (buff[1] != 0x00) {
		printf("Read from device: ");
		for (i = 0; i < 4; i++) {
			printf("%02hhx ", buff[i]);
		}
		printf("\n");
	}
}

void uwait() {
	buff[1] = 0x00;
	hid_get_feature_report(handle, buff, 4);
 	readDevice();
	if (buff[2] & 0b00000001) {
		printf("go to input [%d]\n", c);
// 		code[c++] = buff[1];
		state = 1;
// 		buff[1] = 0;
		buff[2] = 1;
		hid_send_feature_report(handle, buff, 4);
		usleep(250000);
		return;
	}
	
	if (!(s % 8)) {
// 		buff[1] = 0;
		buff[2] = 1;
		hid_send_feature_report(handle, buff, 4);
		usleep(100000);
		buff[2] = 0;
		hid_send_feature_report(handle, buff, 4);
		usleep(150000);
	} else {
		usleep(250000);
	}
	s++;
}

void uinput() {
	printf("input %d [%d]\n", d, c);
// 	buff[1] = 0x00;
	hid_get_feature_report(handle, buff, 4);
	readDevice();
	if (buff[1] != 0x00) {
		code[c++] = buff[1];

		if (c == 4) {
			state = 2;
			printf("go to bell\n");
			usleep(250000);
			return;
		}
		
		d = 0;
	}
	
	if (buff[2] & 0b00000001) {
		d = 0;
	}
	
	d++;
	if (d > 16) {
		d = 0;
		c = 0;
		state = 0;
// 		buff[1] = 0;
		buff[2] = 0;
		hid_send_feature_report(handle, buff, 4);
	}

	usleep(250000);
}

int dd = 1;
int j = 0;
void ubell() {
	pid_t pid;
	int status;

	unsigned int xcode = code[0] * 1000 + code[1] * 100 + code[2] * 10 + code[3];
	printf("bell [%d]\n", xcode);
	
// 	buff[1] = 0;
// 	for (j = 0; j < 25; j++) {
// 		dd ^= 1;
// 		buff[3] = dd;
// 		hid_send_feature_report(handle, buff, 4);
// 		usleep(100000);
// 	}
	
	buff[1] = 0;
	buff[3] = 1;
	hid_send_feature_report(handle, buff, 4);
	
	int i;
	char player[512];
	for (i = 0; i < m_count; i++) {
		if (xcode == map[i].code) {
			snprintf(player, 512, "%s", map[i].bell);
			break;
		}
	}
	if (i == m_count) {
		snprintf(player, 512, "%s", map[0].bell);
	}
	
// 	printf("!!! player: %s\n", player);

	flag = 1;
	switch (pid = fork()) {
		case -1:
			perror("fork");
//			return -1;

		case 0:
			printf("child\n");
			execl("/usr/bin/mpg123", "", "-q", player, NULL);
//			system(player);
			exit(7);
	}

	while (flag) {
		hid_get_feature_report(handle, buff, 4);
		if (buff[2] & 0b10000000) {
			if (flag) {
				printf("killing child...\n");
				kill(pid, SIGKILL);
			} else {
				flag = 0;
				printf("child dead...\n");
			}
		}
		usleep(250000);
	}


	c = 0;
	d = 0;
	state = 0;
	buff[1] = buff[2] = buff[3] = 0;
	hid_send_feature_report(handle, buff, 4);
	printf("go to wait\n");
	usleep(100000);
}

void (*funcs[3]) () = {uwait, uinput, ubell};

int main(void) {
	printf("parsing bell's config...\n");

	tokens = malloc(sizeof(struct token) * 1024 * 1024);

	FILE *fp = fopen("bells", "r");
	char c;

	while ((c = fgetc(fp)) != EOF) {
		if (strchr(" \t", c) != NULL) {
			tokens[t_count].c = c;
			tokens[t_count].t = SPACE;
			t_count++;
			continue;
		}

		if (strchr("0123456789", c) != NULL) {
			tokens[t_count].c = c;
			tokens[t_count].t = DIGIT;
			t_count++;
			continue;
		}

		if (c == '!') {
			tokens[t_count].c = c;
			tokens[t_count].t = EXCL;
			t_count++;
			continue;
		}

		if (c == '\n') {
			tokens[t_count].c = c;
			tokens[t_count].t = DLMR;
			t_count++;
			continue;
		}

		tokens[t_count].c = c;
		tokens[t_count].t = CHAR;
		t_count++;
	}

	fclose(fp);

	while (t_index < t_count) {
		st = q[st][tokens[t_index].t];
		(qfuncs[st])(tokens[t_index].c);

		t_index++;
	}
	if (st == STATE_7) {
		(qfuncs[STATE_F])(' ');
	}

	printf("found %d bells\n", m_count);
	int i;
	struct stat sts;
	for (i = 0; i < m_count; ++i) {
		if (stat(map[i].bell, &sts) == -1 && errno == ENOENT) {
			printf("bell file '%s'doesn't exist!\n", map[i].bell);
		}
	}

	
// 	struct hid_device_info *devs, *cur_dev;
// 	
// 	devs = hid_enumerate(0x0, 0x0);
// 	cur_dev = devs;
// 	
// 	while (cur_dev) {
// 		printf("Device found\n type: %04hx %04hx\n path: %s\n serial number: %ls",
// 			   cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
// 		printf("\n");
// 		printf(" Manufacturer: %ls\n", cur_dev->manufacturer_string);
// 		printf(" Product: %ls\n", cur_dev->product_string);
// 		cur_dev = cur_dev->next;
// 	}
// 	hid_free_enumeration(devs);

	wchar_t wstr[255];
	int res;
	
	handle = hid_open(0x16c0, 0x05df, NULL);
	hid_set_nonblocking(handle, 1);

	res = hid_get_manufacturer_string(handle, wstr, 255);
	printf("Manufacturer string: %ls\n", wstr);
	
	res = hid_get_product_string(handle, wstr, 255);
	printf("Product string: %ls\n", wstr);
	
	buff[0] = buff[1] = buff[2] = buff[3] = 0;
	hid_send_feature_report(handle, buff, 4);

	struct sigaction sa;
	sa.sa_handler = child_handler;
	sigaction(SIGCHLD, &sa, 0);

	while (1) {
		(funcs[state])();
	}
	
	
// 	int c = 0;
// 	unsigned int s = 0;
// 	while (1) {
// 		if (c == 4) {
// 			buff[1] = 0;
// 			if (code[0] == 4 && code[1] == 4 && code[2] == 7 && code[3] == 1) {
// 				buff[3] = 1;
// 				hid_send_feature_report(handle, buff, 4);
// 				usleep(200000);
// 				buff[3] = 0;
// 				hid_send_feature_report(handle, buff, 4);
// 				usleep(200000);
// 				buff[3] = 1;
// 				hid_send_feature_report(handle, buff, 4);
// 				usleep(200000);
// 				buff[3] = 0;
// 				hid_send_feature_report(handle, buff, 4);
// 			} else {
// 				buff[3] = 1;
// 				hid_send_feature_report(handle, buff, 4);
// 				usleep(500000);
// 				buff[3] = 0;
// 				hid_send_feature_report(handle, buff, 4);
// 			}
// 			c = 0;
// 			printf("-----------------------------\n");
// 		} else {
// // 			printf("!!! %d\n", c);
// 			res = hid_get_feature_report(handle, buff, 4);
// 			if (buff[1] != 0x00) {
// 				code[c++] = buff[1];			
// 				printf("Read from device: ");
// 				for (i = 0; i < res; i++) {
// 					printf("%02hhx ", buff[i]);
// 				}
// 				printf("\n");
// 			}
// 			if (!(s % 8)) {
// 				buff[3] = 1;
// 				hid_send_feature_report(handle, buff, 4);
// 				usleep(100000);
// 				buff[3] = 0;
// 				hid_send_feature_report(handle, buff, 4);
// 				usleep(150000);
// 			} else {
// 				usleep(250000);
// 			}
// 			s++;
// 		}
// 	}
	
/*	while (1) {
		res = hid_get_feature_report(handle, buff, 4);*/
/*		printf("Read from device: ");
		for (i = 0; i < res; i++) {
			printf("%02hhx ", buff[i]);
		}
		printf("\n");*/
// 		buff[1] ^= 1;
// 		hid_send_feature_report(handle, buff, 4);
// 		usleep(50000 + rand() % 500000);
// 	}


// 	buff[0] = buff[1] = buff[2] = buff[3] = 0;
// 	hid_send_feature_report(handle, buff, 4);

	hid_close(handle);

	return 0;
}

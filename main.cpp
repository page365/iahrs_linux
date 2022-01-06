#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/fcntl.h>

// �ø��� ��Ʈ�� ��� �ӵ� ����
#define SERIAL_PORT			"/dev/ttyUSB0"
#define SERIAL_SPEED		B115200

#define TRACE	printf

int serial_fd = -1;

int serial_open ()
{
	TRACE ("Try to open serial: %s\n", SERIAL_PORT); 

	serial_fd = open(SERIAL_PORT, O_RDWR|O_NOCTTY); // |O_NDELAY);
	if (serial_fd < 0) {
		printf ("Error unable to open %s\n", SERIAL_PORT);
		return -1;
	}
  	TRACE ("%s open success\n", SERIAL_PORT);

  	struct termios tio;
  	tcgetattr(serial_fd, &tio);
  	cfmakeraw(&tio);
	tio.c_cflag = CS8|CLOCAL|CREAD;
  	tio.c_iflag &= ~(IXON | IXOFF);
  	cfsetspeed(&tio, SERIAL_SPEED);
  	tio.c_cc[VTIME] = 0;
  	tio.c_cc[VMIN] = 0;

  	int err = tcsetattr(serial_fd, TCSAFLUSH, &tio);
  	if (err != 0) {
    	TRACE ("Error tcsetattr() function return error\n");
    	close(serial_fd);
		serial_fd = -1;
    	return -1;
  	}
	return 0;
}

int SendRecv(const char* command, double* returned_data, int data_length);

int main()
{
	serial_open();

	if (serial_fd >= 0) {
		// ��ɿ� ���� �о���� ������ ������ �޶�����.
		// ���⼭�� �ִ� 10���� �����͸� �е��� �޸𸮸� �Ҵ��Ѵ�.
		const int max_data = 10;
		double data[max_data];

		for (int i = 0; i < 1000; i++) {
			int no_data = SendRecv("e\n", data, max_data);	// Read Euler angle
			if (no_data >= 3) {
				printf("Euler angle = %f, %f, %f\n", data[0], data[1], data[2]);
			}

			no_data = SendRecv("q\n", data, max_data);		// Read Quaternion
			if (no_data >= 4) {
				printf("Quaternion = %f, %f, %f, %f\n", data[0], data[1], data[2], data[3]);
			}
			usleep(100*1000);	// 100 ms ���
		}

		close (serial_fd);
	}
	return 0;
}

static unsigned long GetTickCount() 
{
	// ms ������ �ð��� �����´�.
    struct timespec ts;
   
    clock_gettime (CLOCK_MONOTONIC, &ts);

    return ts.tv_sec*1000 + ts.tv_nsec/1000000;
}

int SendRecv(const char* command, double* returned_data, int data_length)
{
	// ������ ��ٸ��� �ִ� �ð� ����. ����: ms
	#define COMM_RECV_TIMEOUT	30	

	// �ø����� rx ���ۿ� �̹� ���� ���ڿ��� �ִٸ�, ����� �����ϱ� ���� �о rx ���۸� ����.
	char temp_buff[256];
	read (serial_fd, temp_buff, 256);

	int command_len = strlen(command);
	int n = write(serial_fd, command, command_len);
	if (n < 0) return -1;

	// ��ɿ� ���� ���� ���ڿ��� �����ϱ� ���� ���� �Ҵ�
	const int buff_size = 1024;
	int  recv_len = 0;
	char recv_buff[buff_size + 1];	// ������ EOS�� �߰��ϱ� ���� + 1

	unsigned long time_start = GetTickCount();

	while (recv_len < buff_size) {
		int n = read (serial_fd, recv_buff + recv_len, buff_size - recv_len);
		if (n < 0) {
			// ��� ���� ���� �߻�
			return -1;
		}
		else if (n == 0) {
			// �ƹ��� �����͵� ���� ���ߴ�. 1ms ��ٷ�����.
			usleep(1000);
		}
		else if (n > 0) {
			recv_len += n;

			// ���� ���ڿ� ���� \r �Ǵ� \n�� ���Դ��� üũ
			if (recv_buff[recv_len - 1] == '\r' || recv_buff[recv_len - 1] == '\n') {
				break;
			}
		}

		unsigned long time_current = GetTickCount();
		unsigned long time_delta = time_current - time_start;

		if (time_delta >= COMM_RECV_TIMEOUT) break;
	}
	recv_buff[recv_len] = '\0';

	// ������ ���� �Ǿ����� üũ�Ѵ�.
	if (recv_len > 0) {
		if (recv_buff[0] == '!') {
			return -1;
		}
	}

	// ���� ��ɰ� ���ƿ� �������� ������ ���Ѵ�.
	if (strncmp(command, recv_buff, command_len - 1) == 0) {
		if (recv_buff[command_len - 1] == '=') {
			int data_count = 0;

			char* p = &recv_buff[command_len];
			char* pp = NULL;

			for (int i = 0; i < data_length; i++) {
				if (p[0] == '0' && p[1] == 'x') {	// 16 ����
					returned_data[i] = strtol(p+2, &pp, 16);
					data_count++;
				}
				else {
					returned_data[i] = strtod(p, &pp);
					data_count++;
				}

				if (*pp == ',') {
					p = pp + 1;
				}
				else {
					break;
				}
			}
			return data_count;
		}
	}
	return 0;
}

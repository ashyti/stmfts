#include <dirent.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define STMFTS_INPUT_PATH	"/dev/input/"
#define STMFTS_INPUT_NAME_SIZE	256

struct stmfts_event {
	struct input_event ev;

	int id;
	int x;
	int y;
	int orientation;
	int major;
	int minor;
	int area;
};

static int stmfts_select(const struct dirent *ep)
{
	if(ep->d_type != DT_CHR)
		return 0;

	if(!strstr(ep->d_name, "event"))
		return 0;

	return 1;
}

int stmfts_read_event(int fd)
{
	int ret;
	struct stmfts_event sev;
	int slot;

	memset(&sev, 0, sizeof(sev));

	while(1) {
		ret = read(fd, &sev.ev, sizeof(sev.ev));

		if (ret < 0) {
			perror("unable to read from device");
			return -1;
		}
		if (ret != sizeof(sev.ev)) {
			error(0, EIO, "unable to read properly");
			return -1;
		}

		switch(sev.ev.type) {
		case EV_SYN:
			printf("touch event: [%d] id = %d, x = %d, y = %d, "
				"major = %d, minor = %d, orientation = %d, "
				"area = %d\n", slot, sev.id, sev.x, sev.y,
				sev.major, sev.minor, sev.orientation, sev.area);
			memset(&sev, 0, sizeof(sev));
			break;
		case EV_ABS:
			switch(sev.ev.code) {
			case ABS_MT_POSITION_X:
				sev.x = sev.ev.value;
				break;
			case ABS_MT_POSITION_Y:
				sev.y = sev.ev.value;
				break;
			case ABS_MT_TOUCH_MAJOR:
				sev.major = sev.ev.value;
				break;
			case ABS_MT_TOUCH_MINOR:
				sev.minor = sev.ev.value;
				break;
			case ABS_MT_ORIENTATION:
				sev.orientation = sev.ev.value;
				break;
			case ABS_MT_PRESSURE:
				sev.area = sev.ev.value;
				break;
			/* case ABS_MT_TRACKING_ID:
				sev.id = sev.ev.value;
				break; */
			case ABS_MT_SLOT:
				slot = sev.ev.value;
				break;
			default:
				fprintf(stderr, "*** unhandled event code"
					"(type = %x, code = %x, value = %d)\n",
					sev.ev.type, sev.ev.code, sev.ev.value);
			}
			break;
		default:
			fprintf(stderr, "*** unhandled event type"
				"(type = %x, code = %x, value = %d)\n",
				sev.ev.type, sev.ev.code, sev.ev.value);
		}
	}

	return -1;
}

int stmfts_open_event(char *fname)
{
	struct dirent **eps;
	int i;
	int n;
	int ret = 0;

	n = scandir(STMFTS_INPUT_PATH, &eps, stmfts_select, alphasort);
	if (!n) {
		perror ("couldn't open the directory");
		return -1;
	}

	for (i = 0; i < n; i++) {
		char path[64];
		int fd;
		char dev_name[STMFTS_INPUT_NAME_SIZE];

		sprintf(path, STMFTS_INPUT_PATH "%s", eps[i]->d_name);

		fd = open(path, O_RDONLY);
		if (fd < 0) {
			perror("cannot open file");
			return -1;
		}

		ret = ioctl(fd, EVIOCGNAME(STMFTS_INPUT_NAME_SIZE), dev_name);
		if (ret < 0) {
			perror("unable to get the name");
			close(fd);
			continue;
		}

		ret = strncmp(fname, dev_name, STMFTS_INPUT_NAME_SIZE);
		if (ret)
			continue;

		return fd;

		close(fd);
	}

	error(0, ENODEV, "input device not found");
	return -1;
}

int stmfts_close_event(int fd)
{
	return close(fd);
}

void stmfts_print_usage(char *argv)
{
	printf("usage: %s <input dev name>\n", argv);
	printf("\n");
	printf("\'input dev name\' is the name given by the device driver "
		"to the /dev/inputX interface\n");
}

int main(int argc, char *argv[])
{
	int fd;

	if (argc != 2) {
		error(0, EPERM, "missing event name");
		stmfts_print_usage(argv[0]);
		return -1;
	}

	fd = stmfts_open_event(argv[1]);
	if (fd < 0) {
		error(0, ENODEV, "input event not found");
		return -1;
	}

	fd = stmfts_read_event(fd);
	if (fd) {
		fprintf(stderr, "something went wrong\n");
		return -1;
	}

	return 0;
}

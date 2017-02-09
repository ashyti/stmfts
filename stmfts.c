#include <dirent.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum stmfts_ev_type {
	STMFTS_EV_TYPE_MT_TOUCH,
	STMFTS_EV_TYPE_HOVER,
	STMFTS_EV_TYPE_KEY
};

#define STMFTS_INPUT_PATH	"/dev/input/"
#define STMFTS_INPUT_NAME_SIZE	256

struct stmfts_event {
	struct input_event ev;

	enum stmfts_ev_type type;

	int id;
	int x;
	int y;
	int z; /* distance, for hovering */
	int orientation;
	int major;
	int minor;
	int area;

	/* key buttons */
	int key_menu;
	int key_back;
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
			switch (sev.type) {
			case STMFTS_EV_TYPE_MT_TOUCH:
				printf("touch: [%d] id = %d, x = %d, y = %d, "
					"major = %d, minor = %d, orientation = %d, "
					"area = %d\n", slot, sev.id, sev.x, sev.y,
					sev.major, sev.minor, sev.orientation, sev.area);
				break;
			case STMFTS_EV_TYPE_HOVER:
				printf("hover: x = %d, y = %d, z = %d\n",
					sev.x, sev.y, sev.z);
				break;
			case STMFTS_EV_TYPE_KEY:
				if (sev.key_menu < 0)
					printf("key: back %s\n", sev.key_back ? "pressed" : "released");
				else
					printf("key: menu %s\n", sev.key_menu ? "pressed" : "released");
				break;
			}
			memset(&sev, 0, sizeof(sev));
			break;
		case EV_ABS:
			switch(sev.ev.code) {
			case ABS_MT_POSITION_X:
				sev.type = STMFTS_EV_TYPE_MT_TOUCH;
				sev.x = sev.ev.value;
				break;
			case ABS_MT_POSITION_Y:
				sev.type = STMFTS_EV_TYPE_MT_TOUCH;
				sev.y = sev.ev.value;
				break;
			case ABS_MT_TOUCH_MAJOR:
				sev.type = STMFTS_EV_TYPE_MT_TOUCH;
				sev.major = sev.ev.value;
				break;
			case ABS_MT_TOUCH_MINOR:
				sev.type = STMFTS_EV_TYPE_MT_TOUCH;
				sev.minor = sev.ev.value;
				break;
			case ABS_MT_ORIENTATION:
				sev.type = STMFTS_EV_TYPE_MT_TOUCH;
				sev.orientation = sev.ev.value;
				break;
			case ABS_MT_PRESSURE:
				sev.type = STMFTS_EV_TYPE_MT_TOUCH;
				sev.area = sev.ev.value;
				break;
			/* case ABS_MT_TRACKING_ID:
				sev.id = sev.ev.value;
				break; */
			case ABS_MT_SLOT:
				sev.type = STMFTS_EV_TYPE_MT_TOUCH;
				slot = sev.ev.value;
				break;
			case ABS_X:
				sev.type = STMFTS_EV_TYPE_HOVER;
				sev.x = sev.ev.value;
				break;
			case ABS_Y:
				sev.type = STMFTS_EV_TYPE_HOVER;
				sev.y = sev.ev.value;
				break;
			case ABS_DISTANCE:
				sev.type = STMFTS_EV_TYPE_HOVER;
				sev.z = sev.ev.value;
				break;
			default:
				fprintf(stderr, "*** unhandled event code"
					"(type = %x, code = %x, value = %d)\n",
					sev.ev.type, sev.ev.code, sev.ev.value);
			}
			break;
		case EV_KEY:
			switch(sev.ev.code) {
			case KEY_MENU:
				sev.type = STMFTS_EV_TYPE_KEY;
				sev.key_back = -1;
				sev.key_menu = sev.ev.value;
				break;
			case KEY_BACK:
				sev.type = STMFTS_EV_TYPE_KEY;
				sev.key_menu = -1;
				sev.key_back = sev.ev.value;
				break;
			default:
				fprintf(stderr, "*** unhandled event code"
					"(type = %x, code = %x, value = %d)\n",
					sev.ev.type, sev.ev.code, sev.ev.value);
			}
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
		if (ret) {
			close(fd);
			continue;
		}

		return fd;
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

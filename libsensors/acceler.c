#define LOG_TAG "sensors_hw"
/* #define LOG_NDEBUG 0
#define LOG_NDEBUG_FUNCTION */
#ifndef LOG_NDEBUG_FUNCTION
#define LOGFUNC(...) ((void)0)
#else
#define LOGFUNC(...) (ALOGV(__VA_ARGS__))
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/input.h>

#include <cutils/log.h>
#include <hardware/sensors.h>

#include "acceler.h"

#define FILE_PATH	"/sys/bus/i2c/drivers/mma7660/0-004c/all_axis_g"
#define DATA_LEN	30
/*  -1,   0,  22 */

static int fd;

#define NSEC_PER_SEC    1000000000L
#define NSEC_PER_USEC   1000L
static inline int64_t timeval_to_ns(const struct timeval *tv)
{
        return ((int64_t) tv->tv_sec * NSEC_PER_SEC) +
                tv->tv_usec * NSEC_PER_USEC;
}

int acceler_init(void)
{
	int ret = 0;
	LOGFUNC("%s()", __FUNCTION__);

	fd = open(FILE_PATH, O_RDONLY);

	if (fd < 0)
		ret = fd;

	return ret;
}

int acceler_poll(sensors_event_t *data, int count)
{
	int ret;
	size_t len;
	struct timeval time;
	struct timespec delay_time;
	int x, y, z;
	char buf[DATA_LEN];

	LOGFUNC("%s()", __FUNCTION__);
	delay_time.tv_nsec = ACCELER_DELAY * 1000 * 1000;
	delay_time.tv_sec = 0;
	nanosleep(&delay_time, NULL);	

	lseek(fd, 0, SEEK_SET);
	
	ret = read(fd, buf, sizeof(buf));
	LOGFUNC("%s: read return %d", __FUNCTION__, ret);

	if (ret <= 0) {
		LOGFUNC("%s:read data fail", __FUNCTION__);
		return 0;
	}

	buf[ret] = 0;

	sscanf(buf, "%d, %d, %d", &x, &y, &z);

	data->acceleration.x = x / 2;
	data->acceleration.y = y / 2;
	data->acceleration.z = z / 2;
	data->acceleration.status = SENSOR_STATUS_ACCURACY_MEDIUM;
	gettimeofday(&time, NULL);
	data->timestamp = timeval_to_ns(&time);
	data->version = sizeof(sensors_event_t);
	data->type = SENSOR_TYPE_ACCELEROMETER;
	data->sensor = SENSORS_HANDLE_BASE + 1;

	LOGFUNC("%s:x=%d,y=%d,z=%d", __FUNCTION__, x, y, z);

	LOGFUNC("get one sensors_event_t");
	return 1;
}

int acceler_have_data(void)
{
	return 1;
}

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

#include <cutils/log.h>
#include <hardware/sensors.h>

#include "sensor_device.h"
#include "acceler.h"

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof(*a))


static struct sensor_t sensors[] = {
	{
		.name		= "mma7660",
		.vendor		= "freescale",
		.version	= 1,
		.handle		= SENSORS_HANDLE_BASE + 1,
		.type		= SENSOR_TYPE_ACCELEROMETER,
		.maxRange	= 30, 
		.resolution	= 1, /* need fix */
		.power		= 0,
		.minDelay	= ACCELER_DELAY,
	},
};

static struct sensor_device sensor_devices[] = {
	{
		.init = acceler_init,
		.poll = acceler_poll,
		.have_data = acceler_have_data,
	},
};

static int sensors_init(void)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(sensor_devices); i++) {
		ret = sensor_devices[i].init();
		if (ret)
			goto error;
	}

	return 0;

error:
	for (i--; i >= 0; i--) {
		if (sensor_devices[i].exit)
			sensor_devices[i].exit();
	}
	return ret;
}

static int sensors_activate(struct sensors_poll_device_t *dev,
	int handle, int enabled)
{
	int ret = 0;

	if (sensor_devices[handle - 1].activate)
		ret = sensor_devices[handle - 1].activate(enabled);

	return ret;
}

static int sensors_setDelay(struct sensors_poll_device_t *dev,
	int handle, int64_t ns)
{
	int ret = 0;

	if (sensor_devices[handle - 1].setDelay)
		ret = sensor_devices[handle - 1].setDelay(ns);

	return ret;
}

static int sensors_poll(struct sensors_poll_device_t *dev,
	sensors_event_t* data, int count)
{
	int i;
	int n;
	
	LOGFUNC("%s(%p,%p,%d)", __FUNCTION__, dev, data, count);
	n = 0;
	for (i = 0; i < ARRAY_SIZE(sensor_devices); i++) {
		if (n >= count)
			break;
		if (sensor_devices[i].have_data()) {
			n += sensor_devices[i].poll(data, count - n);
		}
	}
			
	return n;
}

static int sdev_close(struct hw_device_t *device)
{
	free(device);
	return 0;
}

static int sdev_open(const struct hw_module_t *module, const char *id,
	struct hw_device_t **device)
{
	struct sensors_poll_device_t *sdev;
	int ret;

	LOGFUNC("%s:%s", __FUNCTION__, id);

	if (strcmp(id, SENSORS_HARDWARE_POLL) != 0)
		return -EINVAL;

	sdev = malloc(sizeof(struct sensors_poll_device_t));

	if (!sdev)
		return -ENOMEM;

	memset(sdev, 0, sizeof(struct sensors_poll_device_t));
	sdev->common.tag = HARDWARE_DEVICE_TAG;
	sdev->common.version = HARDWARE_DEVICE_API_VERSION(0,1);
	sdev->common.module = (struct hw_module_t *)module;
	sdev->common.close = sdev_close;

	sdev->activate = sensors_activate;
	sdev->setDelay = sensors_setDelay;
	sdev->poll = sensors_poll;

	ret = sensors_init();
	if (ret)
		goto error;

	*device = (struct hw_device_t *)sdev;
	
	return 0;

error:
	free(sdev);
	return ret;
}

static int sensors_get_sensors_list(struct sensors_module_t *module,
	struct sensor_t const **list)
{
	*list = sensors;
	return ARRAY_SIZE(sensors);
}

static struct hw_module_methods_t hal_module_methods = {
	.open = sdev_open,
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = HARDWARE_MODULE_API_VERSION(0,1),
		.hal_api_version = HARDWARE_HAL_API_VERSION,
		.id = SENSORS_HARDWARE_MODULE_ID,
		.name = "Sensors HAL",
		.author = "Victor Wen",
		.methods = &hal_module_methods,
	},
	.get_sensors_list = sensors_get_sensors_list,
};


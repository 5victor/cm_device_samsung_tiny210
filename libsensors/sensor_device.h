#ifndef __SENSOR_DEVICE_H__
#define __SENSOR_DEVICE_H__

#include <hardware/sensors.h>

typedef struct sensor_device {
	int enabled;
	int64_t delay;
	
	/* must implement */
	int (*init)(void);

	void (*exit)(void);

	int (*activate)(int enabled);

	int (*setDelay)(int64_t ns);

	/* must implement */
	int (*poll)(sensors_event_t* data, int count);		

	/* must implement */
	int (*have_data)(void);
} sensor_device_t;

#endif

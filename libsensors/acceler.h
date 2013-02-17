#ifndef __ACCELER_H__
#define __ACCELER_H__

/* export function */

int acceler_init(void);

int acceler_poll(sensors_event_t* data, int count);

int acceler_have_data(void);

#define ACCELER_DELAY	100

#endif

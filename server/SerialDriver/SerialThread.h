//
//  SerialThread.h
//  SerialDriver
//
//  Created by Mikhail Naganov on 8/20/16.
//  Copyright © 2016 Mikhail Naganov. All rights reserved.
//

#ifndef SerialThread_h
#define SerialThread_h

#include <stdbool.h>

struct serial_thread_data_s;
typedef struct serial_thread_data_s **serial_thread_handle_t;

enum {
    PING_RESULT_NOT_CONNECTED = -1,
    PING_RESULT_OK = 0,
    PING_RESULT_COMMUNICATION_ERROR = 1,
};
typedef void (*ping_callback_t)(void *context, int result);

struct serial_thread_data_s {
    int (*start_ping)(serial_thread_handle_t self,
                      ping_callback_t callback, void *context);
    int (*stop_ping)(serial_thread_handle_t self);
    int (*volume_up)(serial_thread_handle_t self, bool precise);
    int (*volume_down)(serial_thread_handle_t self, bool precise);
    int (*shutdown)(serial_thread_handle_t self);
};

int init_serial_thread(serial_thread_handle_t *pHandle);

#endif /* SerialThread_h */

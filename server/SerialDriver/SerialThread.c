//
//  SerialThread.c
//  SerialDriver
//
//  Created by Mikhail Naganov on 8/20/16.
//  Copyright © 2016 Mikhail Naganov. All rights reserved.
//

#include "SerialThread.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>

int start_ping(serial_thread_handle_t self,
               ping_callback_t callback, void *context);
int stop_ping(serial_thread_handle_t self);
int volume_up(serial_thread_handle_t self, bool precise);
int volume_down(serial_thread_handle_t self, bool precise);
int shutdown_serial(serial_thread_handle_t self);

const struct serial_thread_data_s gSerialInterface = {
    start_ping,
    stop_ping,
    volume_up,
    volume_down,
    shutdown_serial
};

typedef struct ping_info_buffer_s {
    ping_callback_t callback;
    void *context;
} ping_info_buffer_t;

typedef struct serial_thread_data_impl_s {
    const struct serial_thread_data_s *d;
    int fd;
    struct termios originalAttrs;
    bool attrsSet;
    pthread_t thread;
    bool threadCreated;
    int pipeWrite;
    // Thread data
    int pipeRead;
    ping_info_buffer_t ping;
} serial_thread_data_impl_t;

const char *SERIAL_PATH = "/dev/tty.usbserial-AH015UG3";
const char PING = 'A';
const char VOLUP[6] = "UUUUUU";
const char VOLUPPREC[1] = "U";
const char VOLDOWN[6] = "DDDDDD";
const char VOLDOWNPREC[1] = "D";

const char *PIPE_PATH = "/tmp/SerialDriverPipe";
const char PIPE_QUIT = 'Q';
const char PIPE_START_PING = 'P';
const char PIPE_STOP_PING = 'p';
const char PIPE_VOL_UP = 'U';
const char PIPE_VOL_UP_PRECISE = 'u';
const char PIPE_VOL_DOWN = 'D';
const char PIPE_VOL_DOWN_PRECISE = 'd';

void* thread_main(void *self);

#define BAILOUT(msg) { stage = msg; goto error; }

int init_serial_thread(serial_thread_handle_t *pHandle) {
    int result;
    const char *stage = NULL;
    
    openlog("SerialDrv", (LOG_CONS|LOG_PERROR|LOG_PID), LOG_DAEMON);
    setlogmask(LOG_UPTO(LOG_WARNING));
    
    serial_thread_data_impl_t *serial =
        (serial_thread_data_impl_t*)malloc(sizeof(serial_thread_data_impl_t));
    serial->d = &gSerialInterface;
    serial->fd = -1;
    serial->attrsSet = false;
    serial->threadCreated = false;
    serial->pipeRead = -1;
    serial->pipeWrite = -1;
    *pHandle = (serial_thread_handle_t)serial;
    
    (void)remove(PIPE_PATH);
    if (mkfifo(PIPE_PATH, DEFFILEMODE) == -1) BAILOUT("create fifo");
    
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) == -1) BAILOUT("attr_init");
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    if (pthread_create(&serial->thread, &attr, &thread_main, serial) == -1) BAILOUT("create");
    serial->threadCreated = true;

    // Blocks until the threads opens the pipe for reading.
    serial->pipeWrite = open(PIPE_PATH, O_WRONLY);
    
    syslog(LOG_INFO, "created thread");
    return 0;
    
error:
    result = errno;
    syslog(LOG_ALERT, "init_serial_thread: %s: %m", stage);
    pthread_attr_destroy(&attr);
    return result;
}

int shutdown_serial(serial_thread_handle_t self) {
    serial_thread_data_impl_t *serial = (serial_thread_data_impl_t*)self;
    if (serial->threadCreated) {
        if (write(serial->pipeWrite, &PIPE_QUIT, 1) == -1) {
            syslog(LOG_ALERT, "can not ask the thread to quit: %m");
        } else {
            void *result;
            pthread_join(serial->thread, &result);
        }
        close(serial->pipeWrite);
        serial->pipeWrite = -1;
        serial->threadCreated = false;
    }
    (void)remove(PIPE_PATH);
    free(serial);

    closelog();
    return 0;
}


int start_ping(serial_thread_handle_t self, ping_callback_t callback, void *context) {
    int result;
    const char *stage = NULL;
    
    serial_thread_data_impl_t *serial = (serial_thread_data_impl_t*)self;
    if (!serial->threadCreated) return ENOSYS;
    ssize_t res = write(serial->pipeWrite, &PIPE_START_PING, sizeof(PIPE_START_PING));
    if (res != sizeof(PIPE_START_PING)) BAILOUT("writing cmd");
    
    ping_info_buffer_t buf;
    buf.callback = callback;
    buf.context = context;
    res = write(serial->pipeWrite, &buf, sizeof(buf));
    if (res != sizeof(buf)) BAILOUT("writing buf");
    
    return 0;
    
error:
    result = errno;
    syslog(LOG_ALERT, "start_ping: %s: %m", stage);
    return result;
}

int write_command(serial_thread_handle_t self, const char* func_name, char cmd);

int stop_ping(serial_thread_handle_t self) {
    return write_command(self, "stop_ping", PIPE_STOP_PING);
}

int volume_up(serial_thread_handle_t self, bool precise) {
    return write_command(self, "volume_up", precise ? PIPE_VOL_UP_PRECISE : PIPE_VOL_UP);
}

int volume_down(serial_thread_handle_t self, bool precise) {
    return write_command(self, "volume_down", precise ? PIPE_VOL_DOWN_PRECISE : PIPE_VOL_DOWN);
}

int write_command(serial_thread_handle_t self, const char* func_name, char cmd) {
    int result;
    const char *stage = NULL;
    
    serial_thread_data_impl_t *serial = (serial_thread_data_impl_t*)self;
    if (!serial->threadCreated) return ENOSYS;
    ssize_t res = write(serial->pipeWrite, &cmd, sizeof(cmd));
    if (res != sizeof(cmd)) BAILOUT("writing cmd");
    
    return 0;
    
error:
    result = errno;
    syslog(LOG_ALERT, "%s: %s: %m", func_name, stage);
    return result;
}

int try_open_serial(serial_thread_handle_t self);
void try_ping(serial_thread_handle_t self);
int close_serial(serial_thread_handle_t self);
int send_command(serial_thread_handle_t self,
                 const char *func_name,
                 const char *cmd, size_t cmd_len,
                 int retries);

void* thread_main(void *self) {
    serial_thread_data_impl_t *serial = (serial_thread_data_impl_t*)self;
    
    syslog(LOG_DEBUG, "thread: started");
    serial->pipeRead = open(PIPE_PATH, O_RDONLY);
    bool run = true;
    while (run) {
        try_open_serial(self);
        try_ping(self);

        struct pollfd fds[1];
        ssize_t result;
        fds[0].fd = serial->pipeRead;
        fds[0].events = POLL_IN;
        result = poll(fds, 1, 500);
        if (result > 0) {
            char command;
            result = read(serial->pipeRead, &command, 1);
            if (result > 0) {
                switch (command) {
                    case PIPE_QUIT:
                        run = false;
                        syslog(LOG_DEBUG, "thread: received quit command");
                        break;
                    case PIPE_START_PING:
                        result = read(serial->pipeRead, &serial->ping, sizeof(serial->ping));
                        if (result != sizeof(serial->ping)) {
                            memset(&serial->ping, 0, sizeof(serial->ping));
                            syslog(LOG_ALERT, "thread: can not read ping info %m");
                        } else {
                            syslog(LOG_DEBUG, "thread: received start ping command");
                        }
                        break;
                    case PIPE_STOP_PING:
                        serial->ping.callback = NULL;
                        syslog(LOG_DEBUG, "thread: received stop ping command");
                        break;
                    case PIPE_VOL_UP:
                        send_command(self, "vol_up", VOLUP, sizeof(VOLUP), 15);
                        break;
                    case PIPE_VOL_DOWN:
                        send_command(self, "vol_down", VOLDOWN, sizeof(VOLDOWN), 15);
                        break;
                    case PIPE_VOL_UP_PRECISE:
                        send_command(self, "vol_up_prec", VOLUPPREC, sizeof(VOLUPPREC), 15);
                        break;
                    case PIPE_VOL_DOWN_PRECISE:
                        send_command(self, "vol_down_prec", VOLDOWNPREC, sizeof(VOLDOWNPREC), 15);
                    break;
                }
            }
        }
    }
    close_serial(self);
    close(serial->pipeRead);
    serial->pipeRead = -1;
    syslog(LOG_DEBUG, "thread: exit");
    return NULL;
}

int open_serial(serial_thread_handle_t self);

int try_open_serial(serial_thread_handle_t self) {
    static struct timeval nextOpen = { 0, 0 };
    serial_thread_data_impl_t *serial = (serial_thread_data_impl_t*)self;
    if (serial->fd != -1) return 0;
    
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    if (!timercmp(&currentTime, &nextOpen, >)) return 1;
    
    if (open_serial(self) == 0) return 0;
    
    struct timeval delay = { 3, 0 };
    gettimeofday(&currentTime, NULL);
    timeradd(&currentTime, &delay, &nextOpen);
    return -1;
}

int open_serial(serial_thread_handle_t self) {
    int result;
    const char *stage = NULL;
    
    serial_thread_data_impl_t *serial = (serial_thread_data_impl_t*)self;

    serial->attrsSet = false;
    serial->fd = open(SERIAL_PATH, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial->fd == -1) BAILOUT("open serial");
    if (ioctl(serial->fd, TIOCEXCL) == -1) BAILOUT("acquire exclusive access");
    
    if (tcgetattr(serial->fd, &serial->originalAttrs) == -1) BAILOUT("get original attrs");
    struct termios options;
    memset(&options, 0, sizeof(options));
    options.c_cflag = CS8|CREAD|CLOCAL;
    cfsetspeed(&options, B9600);
    if (tcsetattr(serial->fd, TCSANOW, &options) == -1) BAILOUT("set attrs");
    serial->attrsSet = true;
    
    tcflush(serial->fd, TCIFLUSH);

    /* The sleep is required. Without a pause, ping attempts will be failing on read. */
    sleep(1);
    syslog(LOG_INFO, "opened serial");
    return 0;
    
error:
    result = errno;
    syslog(LOG_ALERT, "open: %s: %m", stage);
    close_serial(self);
    return result;
}

void try_ping(serial_thread_handle_t self) {
    static struct timeval nextPing = { 0, 0 };
    serial_thread_data_impl_t *serial = (serial_thread_data_impl_t*)self;
    if (serial->ping.callback == NULL) return;
    
    if (serial->fd == -1) {
        serial->ping.callback(serial->ping.context, PING_RESULT_NOT_CONNECTED);
    }
    
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    if (!timercmp(&currentTime, &nextPing, >)) return;
    
    if (serial->fd != - 1) {
        int ping_result = send_command(self, "ping", &PING, sizeof(PING), 3);
        serial->ping.callback(
            serial->ping.context,
            ping_result == 0 ? PING_RESULT_OK : PING_RESULT_COMMUNICATION_ERROR);
        if (ping_result != 0) {
            close_serial(self);
        }
    }

    struct timeval delay = { 1, 0 };
    gettimeofday(&currentTime, NULL);
    timeradd(&currentTime, &delay, &nextPing);
}

int send_command(serial_thread_handle_t self,
                 const char *func_name,
                 const char *cmd, size_t cmd_len,
                 int retries) {
    const int initial_retries = retries - 1;
    ssize_t result;
    const char *stage = NULL;

    serial_thread_data_impl_t *serial = (serial_thread_data_impl_t*)self;
    if (serial->fd == -1) {
        syslog(LOG_ALERT, "%s: no connection", func_name);
        return ENOSYS;
    }

    struct pollfd fds[1];
    char reply[100];
    memset(reply, 0, sizeof(reply));
    size_t reply_remained = cmd_len;
    if (write(serial->fd, cmd, cmd_len) != cmd_len) BAILOUT("write");
    tcdrain(serial->fd);
    while (reply_remained > 0) {
        if (retries-- <= 0) {
            syslog(LOG_ALERT, "%s: no more retries, remained to read: %zu", func_name, reply_remained);
            return ETIMEDOUT;
        }
        fds[0].fd = serial->fd;
        fds[0].events = POLL_IN;
        result = poll(fds, 1, 500);
        if (result > 0) {
            result = read(serial->fd, reply + (cmd_len - reply_remained), cmd_len);
            if (result > 0) {
                reply_remained -= result;
                if (memcmp(reply, cmd, cmd_len - reply_remained) != 0) {
                    errno = EINVAL; BAILOUT(reply);
                }
            }
        }
        if (result == 0) {
            syslog(LOG_WARNING, "timeout, retrying %s", func_name);
        } if (result < 0) BAILOUT("poll/read");
    }
    
    syslog(LOG_DEBUG, "%s succeeded, retries: %d", func_name, initial_retries - retries);
    return 0;
    
error:
    result = errno;
    syslog(LOG_ALERT, "%s: '%s': %m", func_name, stage);
    return (int)result;
}

#undef BAILOUT

int close_serial(serial_thread_handle_t self) {
    serial_thread_data_impl_t *serial = (serial_thread_data_impl_t*)self;
    if (serial->fd != -1) tcflush(serial->fd, TCIOFLUSH);
    if (serial->attrsSet) tcsetattr(serial->fd, TCSANOW, &serial->originalAttrs);
    if (serial->fd != -1) close(serial->fd);
    serial->attrsSet = false;
    serial->fd = -1;
    syslog(LOG_INFO, "closed serial");
    return 0;
}

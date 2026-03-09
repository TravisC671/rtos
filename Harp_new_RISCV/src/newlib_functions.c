
/*-----------------------------------------------------------*/
#include <FreeRTOS.h>
#include <task.h>
#include <time.h>
#include <sys/times.h>

int _gettimeofday(struct timeval *restrict tv)
{
  tv->tv_sec = 0;
  tv->tv_usec = 0;
  return 0;
}


clock_t _times(struct tms *buf)
{
  return (clock_t) -1;
}


int _open(const char *pathname, int flags, mode_t mode)
{
  return -1;
}

void _exit(int status)
{
  while(1);
}

int _close(int fd)
{
  return -1;
}

off_t _lseek(int fd, off_t offset, int whence)
{
  return (off_t) -1;
}

ssize_t _read(int fd, void  *buf, size_t count)
{
  return (off_t) -1;
}

ssize_t _write(int fd, void  *buf, size_t count)
{
  return (off_t) -1;
}

#include <sys/stat.h>

int _fstat(int fd, struct stat *statbuf)
{
  return -1;
}


int _isatty(int fd)
{
  return -1;
}


pid_t _getpid(void)
{
  // Could return the freertos handle?
  return (pid_t) -1;
}

int _kill(pid_t pid, int sig)
{
  // Could do some FreeRTOS equivalent?
  return -1;
}


int usleep(useconds_t usec)
{
  vTaskDelay(pdMS_TO_TICKS( usec/1000 ));
}

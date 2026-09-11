#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#undef errno
extern int errno;

int _close(int file)
{
    (void)file;
    errno = EBADF;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    errno = ESPIPE;
    return -1;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    errno = EIO;
    return -1;
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    return len;
}

void *_sbrk(ptrdiff_t incr)
{
    extern char _end;
    extern char _estack;
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0)
    {
        heap_end = &_end;
    }

    prev_heap_end = heap_end;
    if ((uintptr_t)(heap_end + incr) > ((uintptr_t)&_estack - 0x400U))
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += incr;
    return prev_heap_end;
}

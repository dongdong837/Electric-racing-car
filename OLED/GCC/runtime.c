#include <stddef.h>
#include <errno.h>

void _init(void) {}
void _fini(void) {}

void *_sbrk(ptrdiff_t increment)
{
    extern char end, __heap_limit;
    static char *current = &end;
    char *previous = current;
    if (increment < 0 || (size_t)increment > (size_t)(&__heap_limit - current)) {
        errno = ENOMEM;
        return (void *)-1;
    }
    current += increment;
    return previous;
}

/*
 * Minimal newlib system call stubs. The firmware has no console or
 * heap-backed stdio; these only exist to satisfy the linker.
 */
#include <errno.h>
#include <sys/stat.h>

int _close(int fd)                       { (void)fd; return -1; }
int _fstat(int fd, struct stat *st)      { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                      { (void)fd; return 1; }
int _lseek(int fd, int ptr, int dir)     { (void)fd; (void)ptr; (void)dir; return 0; }
int _read(int fd, char *p, int len)      { (void)fd; (void)p; (void)len; return 0; }
int _write(int fd, const char *p, int n) { (void)fd; (void)p; return n; }
int _getpid(void)                        { return 1; }
int _kill(int pid, int sig)              { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int status)                   { (void)status; for (;;) { } }

/* No dynamic allocation in this firmware. */
void *_sbrk(int incr)                    { (void)incr; errno = ENOMEM; return (void *)-1; }

/* Local fork hardening: use while holding the filesystem lock. */
#ifndef ESPER_FD_GUARD_H
#define ESPER_FD_GUARD_H
#define ESPER_LFS_VALID_FD(cache, size, fd) \
    ((fd) >= 0 && (unsigned)(fd) < (unsigned)(size) && (cache) != NULL && (cache)[(fd)] != NULL)
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef COMPAT_H
#define COMPAT_H

#ifdef __FreeBSD__
#define __dead __dead2
#endif /* __FreeBSD__ */

#if defined(__linux__) || defined(__CYGWIN__)
#ifdef __GNUC__
#define __dead		__attribute__((__noreturn__))
#else
#define __dead
#endif
#endif /* __linux__ || __CYGWIN__ */

#ifndef HAVE_STRLCPY
#include <sys/types.h>
size_t	strlcpy(char *, const char *, size_t);
#endif /* !HAVE_STRLCPY */

#ifndef HAVE_REALLOCARRAY
#include <sys/types.h>
void	*reallocarray(void *optr, size_t nmemb, size_t size)
#endif /* !HAVE_REALLOCARRAY */

#endif /* COMPAT_H */

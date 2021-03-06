/* RCSid $Id$ */
/*
 *  platform.h - header file for platform specific definitions
 */
#ifndef _RAD_PLATFORM_H_
#define _RAD_PLATFORM_H_

#if defined(_WIN32) || defined(_WIN64)

  #include <io.h>     /* _setmode() and stuff from unistd.h */
  #include <stdio.h>
  typedef long off_t;
  #define fdopen _fdopen
  #define read _read
  #define open _open
  #define close _close
  #define write _write
  #define ftruncate _chsize_s
  #define unlink _unlink
  #define fileno _fileno
  #define snprintf _snprintf
  #define vsnprintf _vsnprintf
  /* XXX should we check first if size_t is 32 bit? */
  #define fseeko _fseeki64
  #define lseek _lseek
  #define access _access
  #define mktemp _mktemp

  #include <string.h>
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
  #define strdup _strdup

  #include <windows.h>
  /* really weird defines by Microsoft in <resource.h>
	 generating lots of name collisions in Radiance. */
  #if defined(rad1)
    #undef rad1
    #undef rad2
    #undef rad3
    #undef rad4
    #undef rad5
    #undef rad6
    #undef rad7
    #undef rad8
    #undef rad9
  #endif
  #define sleep(s) Sleep(s*1000)

  #define NON_POSIX

  #include <sys/types.h>
  #include <sys/stat.h>
  #define RHAS_STAT
  #define S_IFREG _S_IFREG
  #define W_IFDIR _S_IFDIR

  #include <fcntl.h>  /* _O_BINARY, _O_TEXT */
  #include <stdlib.h> /* _fmode */
  #define SET_DEFAULT_BINARY() _fmode = _O_BINARY
  #define SET_FILE_BINARY(fp) _setmode(_fileno(fp),_O_BINARY)
  #define SET_FD_BINARY(fd) _setmode(fd,_O_BINARY)
  #define putenv _putenv

#else /* _WIN32 || _WIN64 */

  #ifdef AMIGA
    #define NON_POSIX
  #else
    /* assumedly posix systems */
	#include <unistd.h>
    #define RHAS_STAT
    #define RHAS_FORK_EXEC
  #endif

  /* everybody except Windows */

  /* NOPs */
  #define SET_DEFAULT_BINARY()
  #define SET_FILE_BINARY(fp)
  #define SET_FD_BINARY(fd)

#endif /* _WIN32 || _WIN64 */

#ifdef __cplusplus
extern "C" {
#endif

/* nothing to protect yet */

#ifdef __cplusplus
}
#endif
#endif /* _RAD_PLATFORM_H_ */


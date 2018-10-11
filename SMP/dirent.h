/*
* MSVC dirent.h compatibility header.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#ifndef SMP_DIRENT_H
#define SMP_DIRENT_H

#ifndef _MSC_VER
#   include_next <dirent.h>
#else

#if !defined(_68K_) && !defined(_MPPC_) && !defined(_X86_) && !defined(_IA64_) && !defined(_AMD64_) && defined(_M_IX86)
#   define _X86_
#endif
#include <stdio.h>
#include <stdarg.h>
#include <wtypes.h>
#include <windef.h>
#include <winbase.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/* Indicates that d_type field is available in dirent structure */
#define _DIRENT_HAVE_D_TYPE

/* Indicates that d_namlen field is available in dirent structure */
#define _DIRENT_HAVE_D_NAMLEN

/* Entries missing from MSVC 6.0 */
#if !defined(FILE_ATTRIBUTE_DEVICE)
#   define FILE_ATTRIBUTE_DEVICE 0x40
#endif

/* File type and permission flags for stat() */
#if !defined(S_IFMT)
#   define S_IFMT   _S_IFMT                     /* File type mask */
#endif
#if !defined(S_IFDIR)
#   define S_IFDIR  _S_IFDIR                    /* Directory */
#endif
#if !defined(S_IFCHR)
#   define S_IFCHR  _S_IFCHR                    /* Character device */
#endif
#if !defined(S_IFFIFO)
#   define S_IFFIFO _S_IFFIFO                   /* Pipe */
#endif
#if !defined(S_IFREG)
#   define S_IFREG  _S_IFREG                    /* Regular file */
#endif
#if !defined(S_IREAD)
#   define S_IREAD  _S_IREAD                    /* Read permission */
#endif
#if !defined(S_IWRITE)
#   define S_IWRITE _S_IWRITE                   /* Write permission */
#endif
#if !defined(S_IEXEC)
#   define S_IEXEC  _S_IEXEC                    /* Execute permission */
#endif
#if !defined(S_IFIFO)
#   define S_IFIFO _S_IFIFO                     /* Pipe */
#endif
#if !defined(S_IFBLK)
#   define S_IFBLK   0                          /* Block device */
#endif
#if !defined(S_IFLNK)
#   define S_IFLNK   0                          /* Link */
#endif
#if !defined(S_IFSOCK)
#   define S_IFSOCK  0                          /* Socket */
#endif

#if defined(_MSC_VER)
#if !defined(S_IRUSR)
#   define S_IRUSR  S_IREAD                     /* Read user */
#endif
#if !defined(S_IWUSR)
#   define S_IWUSR  S_IWRITE                    /* Write user */
#endif
#if !defined(S_IXUSR)
#   define S_IXUSR  0                           /* Execute user */
#endif
#if !defined(S_IRGRP)
#   define S_IRGRP  0                           /* Read group */
#endif
#if !defined(S_IWGRP)
#   define S_IWGRP  0                           /* Write group */
#endif
#if !defined(S_IXGRP)
#   define S_IXGRP  0                           /* Execute group */
#endif
#if !defined(S_IROTH)
#   define S_IROTH  0                           /* Read others */
#endif
#if !defined(S_IWOTH)
#   define S_IWOTH  0                           /* Write others */
#endif
#if !defined(S_IXOTH)
#   define S_IXOTH  0                           /* Execute others */
#endif
#endif

/* Maximum length of file name */
#if !defined(PATH_MAX)
#   define PATH_MAX MAX_PATH
#endif
#if !defined(FILENAME_MAX)
#   define FILENAME_MAX MAX_PATH
#endif
#if !defined(NAME_MAX)
#   define NAME_MAX FILENAME_MAX
#endif

/* File type flags for d_type */
#define DT_UNKNOWN  0
#define DT_REG      S_IFREG
#define DT_DIR      S_IFDIR
#define DT_FIFO     S_IFIFO
#define DT_SOCK     S_IFSOCK
#define DT_CHR      S_IFCHR
#define DT_BLK      S_IFBLK
#define DT_LNK      S_IFLNK

/* Macros for converting between st_mode and d_type */
#define IFTODT(mode) ((mode) & S_IFMT)
#define DTTOIF(type) (type)

/*
 * File type macros.  Note that block devices, sockets and links cannot be
 * distinguished on Windows and the macros S_ISBLK, S_ISSOCK and S_ISLNK are
 * only defined for compatibility.  These macros should always return false
 * on Windows.
 */
#define	S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define	S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#define	S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#define	S_ISLNK(mode)  (((mode) & S_IFMT) == S_IFLNK)
#define	S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)
#define	S_ISCHR(mode)  (((mode) & S_IFMT) == S_IFCHR)
#define	S_ISBLK(mode)  (((mode) & S_IFMT) == S_IFBLK)

/* Return the exact length of d_namlen without zero terminator */
#define _D_EXACT_NAMLEN(p) ((p)->d_namlen)

/* Return number of bytes needed to store d_namlen */
#define _D_ALLOC_NAMLEN(p) (PATH_MAX + 1)

/* Wide-character version */
struct _wdirent
{
    long d_ino;                                 /* Always zero */
    unsigned short d_reclen;                    /* Structure size */
    size_t d_namlen;                            /* Length of name without \0 */
    int d_type;                                 /* File type */
    wchar_t d_name[PATH_MAX + 1];               /* File name */
};
typedef struct _wdirent _wdirent;

struct _WDIR
{
    struct _wdirent ent;                        /* Current directory entry */
    WIN32_FIND_DATAW data;                      /* Private file data */
    int cached;                                 /* True if data is valid */
    HANDLE handle;                              /* Win32 search handle */
    wchar_t *patt;                              /* Initial directory name */
};
typedef struct _WDIR _WDIR;

static _WDIR *_wopendir(const wchar_t *dirname);
static struct _wdirent *_wreaddir(_WDIR *dirp);
static int _wclosedir(_WDIR *dirp);
static void _wrewinddir(_WDIR* dirp);

/* For compatibility with Symbian */
#define wdirent _wdirent
#define WDIR _WDIR
#define wopendir _wopendir
#define wreaddir _wreaddir
#define wclosedir _wclosedir
#define wrewinddir _wrewinddir

/* Multi-byte character versions */
struct dirent
{
    long d_ino;                                 /* Always zero */
    unsigned short d_reclen;                    /* Structure size */
    size_t d_namlen;                            /* Length of name without \0 */
    int d_type;                                 /* File type */
    char d_name[PATH_MAX + 1];                  /* File name */
};
typedef struct dirent dirent;

struct DIR
{
    struct dirent ent;
    struct _WDIR *wdirp;
};
typedef struct DIR DIR;

static DIR *opendir(const char *dirname);
static struct dirent *readdir(DIR *dirp);
static int closedir(DIR *dirp);
static void rewinddir(DIR* dirp);

/* Internal utility functions */
static WIN32_FIND_DATAW *dirent_first(_WDIR *dirp);
static WIN32_FIND_DATAW *dirent_next(_WDIR *dirp);

static int dirent_mbstowcs_s(
    size_t *pReturnValue,
    wchar_t *wcstr,
    size_t sizeInWords,
    const char *mbstr,
    size_t count);

static int dirent_wcstombs_s(
    size_t *pReturnValue,
    char *mbstr,
    size_t sizeInBytes,
    const wchar_t *wcstr,
    size_t count);

static void dirent_set_errno(int error);

/*
 * Open directory stream DIRNAME for read and return a pointer to the
 * internal working area that is used to retrieve individual directory
 * entries.
 */
static __inline _WDIR* _wopendir(const wchar_t *dirname)
{
    _WDIR *dirp = NULL;
    int error;

    /* Must have directory name */
    if (dirname == NULL || dirname[0] == '\0') {
        dirent_set_errno(ENOENT);
        return NULL;
    }

    /* Allocate new _WDIR structure */
    dirp = (_WDIR*)malloc(sizeof(struct _WDIR));
    if (dirp != NULL) {
        DWORD n;

        /* Reset _WDIR structure */
        dirp->handle = INVALID_HANDLE_VALUE;
        dirp->patt = NULL;
        dirp->cached = 0;

        /* Compute the length of full path plus zero terminator */
        n = GetFullPathNameW(dirname, 0, NULL, NULL);

        /* Allocate room for absolute directory name and search pattern */
        dirp->patt = (wchar_t*)malloc(sizeof(wchar_t) * n + 16);
        if (dirp->patt) {
            /*
             * Convert relative directory name to an absolute one.  This
             * allows rewinddir() to function correctly even when current
             * working directory is changed between opendir() and rewinddir().
             */
            n = GetFullPathNameW(dirname, n, dirp->patt, NULL);
            if (n > 0) {
                wchar_t *p;

                /* Append search pattern \* to the directory name */
                p = dirp->patt + n;
                if (dirp->patt < p) {
                    switch (p[-1]) {
                        case '\\':
                        case '/':
                        case ':':
                            /* Directory ends in path separator, e.g. c:\temp\ */
                            /*NOP*/;
                            break;

                        default:
                            /* Directory name doesn't end in path separator */
                            *p++ = '\\';
                    }
                }
                *p++ = '*';
                *p = '\0';

                /* Open directory stream and retrieve the first entry */
                if (dirent_first(dirp)) {
                    /* Directory stream opened successfully */
                    error = 0;
                } else {
                    /* Cannot retrieve first entry */
                    error = 1;
                    dirent_set_errno(ENOENT);
                }
            } else {
                /* Cannot retrieve full path name */
                dirent_set_errno(ENOENT);
                error = 1;
            }
        } else {
            /* Cannot allocate memory for search pattern */
            error = 1;
        }
    } else {
        /* Cannot allocate _WDIR structure */
        error = 1;
    }

    /* Clean up in case of error */
    if (error  &&  dirp) {
        _wclosedir(dirp);
        dirp = NULL;
    }

    return dirp;
}

/*
 * Read next directory entry.  The directory entry is returned in dirent
 * structure in the d_name field.  Individual directory entries returned by
 * this function include regular files, sub-directories, pseudo-directories
 * "." and ".." as well as volume labels, hidden files and system files.
 */
static __inline struct _wdirent* _wreaddir(_WDIR *dirp)
{
    WIN32_FIND_DATAW *datap;
    struct _wdirent *entp;

    /* Read next directory entry */
    datap = dirent_next(dirp);
    if (datap) {
        size_t n;
        DWORD attr;

        /* Pointer to directory entry to return */
        entp = &dirp->ent;

        /*
         * Copy file name as wide-character string.  If the file name is too
         * long to fit in to the destination buffer, then truncate file name
         * to PATH_MAX characters and zero-terminate the buffer.
         */
        n = 0;
        while (n < PATH_MAX  &&  datap->cFileName[n] != 0) {
            entp->d_name[n] = datap->cFileName[n];
            n++;
        }
        dirp->ent.d_name[n] = 0;

        /* Length of file name excluding zero terminator */
        entp->d_namlen = n;

        /* File type */
        attr = datap->dwFileAttributes;
        if ((attr & FILE_ATTRIBUTE_DEVICE) != 0) {
            entp->d_type = DT_CHR;
        } else if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            entp->d_type = DT_DIR;
        } else {
            entp->d_type = DT_REG;
        }

        /* Reset dummy fields */
        entp->d_ino = 0;
        entp->d_reclen = sizeof(struct _wdirent);
    } else {
        /* Last directory entry read */
        entp = NULL;
    }

    return entp;
}

/*
 * Close directory stream opened by opendir() function.  This invalidates the
 * DIR structure as well as any directory entry read previously by
 * _wreaddir().
 */
static __inline int _wclosedir(_WDIR *dirp)
{
    int ok;
    if (dirp) {
        /* Release search handle */
        if (dirp->handle != INVALID_HANDLE_VALUE) {
            FindClose(dirp->handle);
            dirp->handle = INVALID_HANDLE_VALUE;
        }

        /* Release search pattern */
        if (dirp->patt) {
            free(dirp->patt);
            dirp->patt = NULL;
        }

        /* Release directory structure */
        free(dirp);
        ok = /*success*/0;
    } else {
        /* Invalid directory stream */
        dirent_set_errno(EBADF);
        ok = /*failure*/-1;
    }
    return ok;
}

/*
 * Rewind directory stream such that _wreaddir() returns the very first
 * file name again.
 */
static __inline void _wrewinddir(_WDIR* dirp)
{
    if (dirp) {
        /* Release existing search handle */
        if (dirp->handle != INVALID_HANDLE_VALUE) {
            FindClose(dirp->handle);
        }

        /* Open new search handle */
        dirent_first(dirp);
    }
}

/* Get first directory entry (internal) */
static __inline WIN32_FIND_DATAW* dirent_first(_WDIR *dirp)
{
    WIN32_FIND_DATAW *datap;

    /* Open directory and retrieve the first entry */
    dirp->handle = FindFirstFileW(dirp->patt, &dirp->data);
    if (dirp->handle != INVALID_HANDLE_VALUE) {
        /* a directory entry is now waiting in memory */
        datap = &dirp->data;
        dirp->cached = 1;
    } else {
        /* Failed to re-open directory: no directory entry in memory */
        dirp->cached = 0;
        datap = NULL;
    }
    return datap;
}

/* Get next directory entry (internal) */
static __inline WIN32_FIND_DATAW* dirent_next(_WDIR *dirp)
{
    WIN32_FIND_DATAW *p;

    /* Get next directory entry */
    if (dirp->cached != 0) {
        /* A valid directory entry already in memory */
        p = &dirp->data;
        dirp->cached = 0;
    } else if (dirp->handle != INVALID_HANDLE_VALUE) {
        /* Get the next directory entry from stream */
        if (FindNextFileW(dirp->handle, &dirp->data) != FALSE) {
            /* Got a file */
            p = &dirp->data;
        } else {
            /* The very last entry has been processed or an error occured */
            FindClose(dirp->handle);
            dirp->handle = INVALID_HANDLE_VALUE;
            p = NULL;
        }
    } else {
        /* End of directory stream reached */
        p = NULL;
    }
    return p;
}

/*
 * Open directory stream using plain old C-string.
 */
static __inline DIR* opendir(const char *dirname)
{
    struct DIR *dirp;
    int error;

    /* Must have directory name */
    if (dirname == NULL || dirname[0] == '\0') {
        dirent_set_errno(ENOENT);
        return NULL;
    }

    /* Allocate memory for DIR structure */
    dirp = (DIR*)malloc(sizeof(struct DIR));
    if (dirp) {
        wchar_t wname[PATH_MAX + 1];
        size_t n;

        /* Convert directory name to wide-character string */
        error = dirent_mbstowcs_s(
            &n, wname, PATH_MAX + 1, dirname, PATH_MAX);
        if (!error) {
            /* Open directory stream using wide-character name */
            dirp->wdirp = _wopendir(wname);
            if (dirp->wdirp) {
                /* Directory stream opened */
                error = 0;
            } else {
                /* Failed to open directory stream */
                error = 1;
            }
        } else {
            /*
             * Cannot convert file name to wide-character string.  This
             * occurs if the string contains invalid multi-byte sequences or
             * the output buffer is too small to contain the resulting
             * string.
             */
            error = 1;
        }
    } else {
        /* Cannot allocate DIR structure */
        error = 1;
    }

    /* Clean up in case of error */
    if (error  &&  dirp) {
        free(dirp);
        dirp = NULL;
    }

    return dirp;
}

/*
 * Read next directory entry.
 *
 * When working with text consoles, please note that file names returned by
 * readdir() are represented in the default ANSI code page while any output to
 * console is typically formatted on another code page.  Thus, non-ASCII
 * characters in file names will not usually display correctly on console.  The
 * problem can be fixed in two ways: (1) change the character set of console
 * to 1252 using chcp utility and use Lucida Console font, or (2) use
 * _cprintf function when writing to console.  The _cprinf() will re-encode
 * ANSI strings to the console code page so many non-ASCII characters will
 * display correcly.
 */
static __inline struct dirent* readdir(DIR *dirp)
{
    WIN32_FIND_DATAW *datap;
    struct dirent *entp;

    /* Read next directory entry */
    datap = dirent_next(dirp->wdirp);
    if (datap) {
        size_t n;
        int error;

        /* Attempt to convert file name to multi-byte string */
        error = dirent_wcstombs_s(
            &n, dirp->ent.d_name, MAX_PATH + 1, datap->cFileName, MAX_PATH);

        /*
         * If the file name cannot be represented by a multi-byte string,
         * then attempt to use old 8+3 file name.  This allows traditional
         * Unix-code to access some file names despite of unicode
         * characters, although file names may seem unfamiliar to the user.
         *
         * Be ware that the code below cannot come up with a short file
         * name unless the file system provides one.  At least
         * VirtualBox shared folders fail to do this.
         */
        if (error  &&  datap->cAlternateFileName[0] != '\0') {
            error = dirent_wcstombs_s(
                &n, dirp->ent.d_name, MAX_PATH + 1, datap->cAlternateFileName,
                sizeof(datap->cAlternateFileName) /
                sizeof(datap->cAlternateFileName[0]));
        }

        if (!error) {
            DWORD attr;

            /* Initialize directory entry for return */
            entp = &dirp->ent;

            /* Length of file name excluding zero terminator */
            entp->d_namlen = n - 1;

            /* File attributes */
            attr = datap->dwFileAttributes;
            if ((attr & FILE_ATTRIBUTE_DEVICE) != 0) {
                entp->d_type = DT_CHR;
            } else if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                entp->d_type = DT_DIR;
            } else {
                entp->d_type = DT_REG;
            }

            /* Reset dummy fields */
            entp->d_ino = 0;
            entp->d_reclen = sizeof(struct dirent);
        } else {
            /*
             * Cannot convert file name to multi-byte string so construct
             * an errornous directory entry and return that.  Note that
             * we cannot return NULL as that would stop the processing
             * of directory entries completely.
             */
            entp = &dirp->ent;
            entp->d_name[0] = '?';
            entp->d_name[1] = '\0';
            entp->d_namlen = 1;
            entp->d_type = DT_UNKNOWN;
            entp->d_ino = 0;
            entp->d_reclen = 0;
        }
    } else {
        /* No more directory entries */
        entp = NULL;
    }

    return entp;
}

/*
 * Close directory stream.
 */
static __inline int closedir(DIR *dirp)
{
    int ok;
    if (dirp) {
        /* Close wide-character directory stream */
        ok = _wclosedir(dirp->wdirp);
        dirp->wdirp = NULL;

        /* Release multi-byte character version */
        free(dirp);
    } else {
        /* Invalid directory stream */
        dirent_set_errno(EBADF);
        ok = /*failure*/-1;
    }
    return ok;
}

/*
 * Rewind directory stream to beginning.
 */
static __inline void rewinddir(DIR* dirp)
{
    /* Rewind wide-character string directory stream */
    _wrewinddir(dirp->wdirp);
}

/* Convert multi-byte string to wide character string */
static __inline int dirent_mbstowcs_s(
    size_t *pReturnValue,
    wchar_t *wcstr,
    size_t sizeInWords,
    const char *mbstr,
    size_t count)
{
    int error;

#if _MSC_VER >= 1400
    /* Microsoft Visual Studio 2005 or later */
    error = mbstowcs_s(pReturnValue, wcstr, sizeInWords, mbstr, count);
#else
    /* Older Visual Studio or non-Microsoft compiler */
    size_t n;

    /* Convert to wide-character string */
    n = mbstowcs(wcstr, mbstr, count);
    if (n < sizeInWords) {
        /* Zero-terminate output buffer */
        if (wcstr) {
            wcstr[n] = 0;
        }

        /* Length of resuting multi-byte string WITH zero terminator */
        if (pReturnValue) {
            *pReturnValue = n + 1;
        }

        /* Success */
        error = 0;
    } else {
        /* Could not convert string */
        error = 1;
    }
#endif
    return error;
}

/* Convert wide-character string to multi-byte string */
static __inline int dirent_wcstombs_s(
    size_t *pReturnValue,
    char *mbstr,
    size_t sizeInBytes,
    const wchar_t *wcstr,
    size_t count)
{
    int error;

#if _MSC_VER >= 1400
    /* Microsoft Visual Studio 2005 or later */
    error = wcstombs_s(pReturnValue, mbstr, sizeInBytes, wcstr, count);
#else
    /* Older Visual Studio or non-Microsoft compiler */
    size_t n;

    /* Convert to multi-byte string */
    n = wcstombs(mbstr, wcstr, count);
    if (n < sizeInBytes) {
        /* Zero-terminate output buffer */
        if (mbstr) {
            mbstr[n] = '\0';
        }

        /* Lenght of resulting multi-bytes string WITH zero-terminator */
        if (pReturnValue) {
            *pReturnValue = n + 1;
        }

        /* Success */
        error = 0;
    } else {
        /* Cannot convert string */
        error = 1;
    }
#endif

    return error;
}

/* Set errno variable */
static __inline void dirent_set_errno(int error)
{
    /* Microsoft Visual Studio */
    _set_errno(error);
}

#endif /* _MSC_VER */

#endif /*SMP_DIRENT_H*/

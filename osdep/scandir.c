/*
 * scandir, alphasort - scan a directory
 *
 * implementation for systems that do not have it in libc
 */

#include "config.h"

#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/*
 * convenience helper function for scandir's |compar()| function: 
 * sort directory entries using strcoll(3)
 */
int
alphasort(const void *_a, const void *_b)
{
    struct dirent **a = (struct dirent **)_a;
    struct dirent **b = (struct dirent **)_b;
    return strcoll((*a)->d_name, (*b)->d_name);
}


#define strverscmp(a,b) strcoll(a,b) /* for now */

/*
 * convenience helper function for scandir's |compar()| function: 
 * sort directory entries using GNU |strverscmp()|
 */
int
versionsort(const void *_a, const void *_b)
{
    struct dirent **a = (struct dirent **)_a;
    struct dirent **b = (struct dirent **)_b;
    return strverscmp((*a)->d_name, (*b)->d_name);
}

/*
 * The scandir() function reads the directory dirname and builds an
 * array of pointers to directory entries using malloc(3).  It returns
 * the number of entries in the array.  A pointer to the array of
 * directory entries is stored in the location referenced by namelist.
 *
 * The select parameter is a pointer to a user supplied subroutine
 * which is called by scandir() to select which entries are to be
 * included in the array.  The select routine is passed a pointer to
 * a directory entry and should return a non-zero value if the
 * directory entry is to be included in the array.  If select is null,
 * then all the directory entries will be included.
 *
 * The compar parameter is a pointer to a user supplied subroutine
 * which is passed to qsort(3) to sort the completed array.  If this
 * pointer is null, the array is not sorted.
 */
int
scandir(const char *dirname,
	struct dirent ***ret_namelist,
	int (*select)(const struct dirent *),
	int (*compar)(const struct dirent **, const struct dirent **))
{
    int i, len;
    int used, allocated;
    DIR *dir;
    struct dirent *ent, *ent2;
    struct dirent **namelist = NULL;

    if ((dir = opendir(dirname)) == NULL)
	return -1;

    used = 0;
    allocated = 2;
    namelist = malloc(allocated * sizeof(struct dirent *));
    if (!namelist)
	goto error;

    while ((ent = readdir(dir)) != NULL) {

	if (select != NULL && !select(ent))
	    continue;

	/* duplicate struct direct for this entry */
	len = offsetof(struct dirent, d_name) + strlen(ent->d_name) + 1;
	if ((ent2 = malloc(len)) == NULL)
	    goto error;
	
	if (used >= allocated) {
	    allocated *= 2;
	    namelist = realloc(namelist, allocated * sizeof(struct dirent *));
	    if (!namelist)
		goto error;
	}
	memcpy(ent2, ent, len);
	namelist[used++] = ent2;
    }
    closedir(dir);

    if (compar)
	qsort(namelist, used, sizeof(struct dirent *),
	      (int (*)(const void *, const void *)) compar);

    *ret_namelist = namelist;
    return used;


error:
    closedir(dir);

    if (namelist) {
	for (i = 0; i < used; i++) 
	    free(namelist[i]);
	free(namelist);
    }
    return -1;
}


#if	STANDALONE_MAIN
int
main(int argc, char **argv)
{
	struct dirent **namelist;
	int i, n;

	n = scandir("/etc", &namelist, NULL, alphasort);

	for (i = 0; i < n; i++)
		printf("%s\n", namelist[i]->d_name);
}
#endif

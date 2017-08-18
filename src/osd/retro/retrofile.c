#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#ifdef SDLMAME_LINUX
#define __USE_LARGEFILE64
#endif
#ifndef SDLMAME_BSD
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
#define _XOPEN_SOURCE 500
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
// MAME headers
#include "retrofile.h"
#include <stddef.h>

//============================================================
//  GLOBAL IDENTIFIERS
//============================================================
/*
extern const char *sdlfile_socket_identifier;
extern const char *sdlfile_ptty_identifier;
*/
//============================================================
//  CONSTANTS
//============================================================

#if defined(WIN32)
#define PATHSEPCH '\\'
#define INVPATHSEPCH '/'
#else
#define PATHSEPCH '/'
#define INVPATHSEPCH '\\'
#endif

#define NO_ERROR    (0)

//============================================================
//  Prototypes
//============================================================

static UINT32 create_path_recursive(char *path);

//============================================================
//  error_to_file_error
//  (does filling this out on non-Windows make any sense?)
//============================================================

file_error error_to_file_error(UINT32 error)
{
	switch (error)
	{
	case ENOENT:
	case ENOTDIR:
		return FILERR_NOT_FOUND;

	case EACCES:
	case EROFS:
	#ifndef WIN32
	case ETXTBSY:
	#endif
	case EEXIST:
	case EPERM:
	case EISDIR:
	case EINVAL:
		return FILERR_ACCESS_DENIED;

	case ENFILE:
	case EMFILE:
		return FILERR_TOO_MANY_FILES;

	default:
		return FILERR_FAILURE;
	}
}


//============================================================
//  osd_open
//============================================================

file_error osd_open(const char *path, UINT32 openflags, osd_file **file, UINT64 *filesize)
{
	const char *mode;
	FILE *fileptr;
	char *tmpstr;
	tmpstr = NULL;

	tmpstr = (char *)malloc(strlen(path)+1);

	char *pathsep0 = strrchr(path, PATHSEPCH);

	if (pathsep0 != NULL){
		strncpy(tmpstr,path,pathsep0-path+1);
		tmpstr[pathsep0-path+1]=0;
	}
	else strcpy(tmpstr,path);

	// based on the flags, choose a mode
	if (openflags & OPEN_FLAG_WRITE)
	{
		if (openflags & OPEN_FLAG_READ)
			mode = (openflags & OPEN_FLAG_CREATE) ? "w+b" : "r+b";
		else
			mode = "wb";
	}
	else if (openflags & OPEN_FLAG_READ)
		mode = "rb";
	else{
		free(tmpstr);
		return FILERR_INVALID_ACCESS;
	}

	// open the file
	fileptr = fopen(path, mode);
	if (fileptr == NULL){

		// create the path if necessary
		if ((openflags & OPEN_FLAG_CREATE) && (openflags & OPEN_FLAG_CREATE_PATHS))
		{
			char *pathsep = strrchr(tmpstr, PATHSEPCH);
			if (pathsep != NULL)
			{
				int error;

				// create the path up to the file
				error = create_path_recursive(tmpstr);

				// attempt to reopen the file
				if (error == NO_ERROR)
				{
					fileptr = fopen(path, mode);
				}
			}
		}

		// if we still failed, clean up and osd_free
		if (fileptr == NULL)
		{
			free(tmpstr);
			return FILERR_NOT_FOUND;
		}

	}

	// store the file pointer directly as an osd_file
	*file = (osd_file *)fileptr;

	// get the size -- note that most fseek/ftell implementations are limited to 32 bits
	fseek(fileptr, 0, SEEK_END);
	*filesize = ftell(fileptr);
	fseek(fileptr, 0, SEEK_SET);

	free(tmpstr);

	return FILERR_NONE;
}


//============================================================
//  osd_read
//============================================================

file_error osd_read(osd_file *file, void *buffer, UINT64 offset, UINT32 length, UINT32 *actual)
{
	size_t count;

	// seek to the new location; note that most fseek implementations are limited to 32 bits
	fseek((FILE *)file, offset, SEEK_SET);

	// perform the read
	count = fread(buffer, 1, length, (FILE *)file);
	if (actual != NULL)
		*actual = count;

	return FILERR_NONE;
}


//============================================================
//  osd_write
//============================================================

file_error osd_write(osd_file *file, const void *buffer, UINT64 offset, UINT32 length, UINT32 *actual)
{
	size_t count;

	// seek to the new location; note that most fseek implementations are limited to 32 bits
	fseek((FILE *)file, offset, SEEK_SET);

	// perform the write
	count = fwrite(buffer, 1, length, (FILE *)file);
	if (actual != NULL)
		*actual = count;

	return FILERR_NONE;
}


//============================================================
//  osd_close
//============================================================

file_error osd_close(osd_file *file)
{
	// close the file handle
	fclose((FILE *)file);
	return FILERR_NONE;
}

//============================================================
//  osd_rmfile
//============================================================

file_error osd_rmfile(const char *filename)
{
	return remove(filename) ? FILERR_FAILURE : FILERR_NONE;
}

//============================================================
//  create_path_recursive
//============================================================

static UINT32 create_path_recursive(char *path)
{
	char *sep = strrchr(path, PATHSEPCH);
	UINT32 filerr;
	struct stat st;

	// if there's still a separator, and it's not the root, nuke it and recurse
	if (sep != NULL && sep > path && sep[0] != ':' && sep[-1] != PATHSEPCH)
	{
		*sep = 0;
		filerr = create_path_recursive(path);
		*sep = PATHSEPCH;
		if (filerr != NO_ERROR)
			return filerr;
	}

	// if the path already exists, we're done
	if (!stat(path, &st))
		return NO_ERROR;

	//printf("mkd(%s)\n",path);
	// create the path
	#ifdef WIN32
	if (mkdir(path) != 0)
	#else
	if (mkdir(path, 0777) != 0)
	#endif
		return error_to_file_error(errno);
	return NO_ERROR;
}

//============================================================
//  osd_get_physical_drive_geometry
//============================================================

int osd_get_physical_drive_geometry(const char *filename, UINT32 *cylinders, UINT32 *heads, UINT32 *sectors, UINT32 *bps)
{
	return FALSE;       // no, no way, huh-uh, forget it
}

//============================================================
//  osd_is_path_separator
//============================================================

static int osd_is_path_separator(char c)
{
	return (c == '/') || (c == '\\');
}

//============================================================
//  osd_is_absolute_path
//============================================================

int osd_is_absolute_path(const char *path)
{
	int result;

	if (osd_is_path_separator(path[0]))
		result = TRUE;
#if !defined(SDLMAME_WIN32) && !defined(SDLMAME_OS2) && !defined(WIIU) 
	else if (path[0] == '.')
		result = TRUE;
#elif defined(WIIU) 
	else if (*path && path[2] == ':')
		result = TRUE;
#else
	#ifndef UNDER_CE
	else if (*path && path[1] == ':')
		result = TRUE;
	#endif
#endif
	else
		result = FALSE;

	return result;
}


int osd_uchar_from_osdchar(UINT32 /* unicode_char */ *uchar, const char *osdchar, size_t count)
{
	// we assume a standard 1:1 mapping of characters to the first 256 unicode characters
	*uchar = (UINT8)*osdchar;
	return 1;
}


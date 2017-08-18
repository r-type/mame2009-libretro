#include <dirent.h>
#include <sys/stat.h>
#include "osdcore.h"
#include "retroos.h"

#if defined(SDLMAME_WIN32) || defined(SDLMAME_OS2)
#define PATHSEPCH '\\'
#define INVPATHSEPCH '/'
#else
#define PATHSEPCH '/'
#define INVPATHSEPCH '\\'
#endif

typedef struct dirent sdl_dirent;
typedef struct stat sdl_stat;

struct _osd_directory
{
	osd_directory_entry ent;
	sdl_dirent *data;
	DIR *fd;
	char *path;
};

//============================================================
//  osd_opendir
//============================================================

osd_directory *osd_opendir(const char *dirname)
{

	osd_directory *dir = NULL;
	char *tmpstr, *envstr;
	int i, j;

	dir = (osd_directory *) malloc(sizeof(*dir));

	if (dir)
	{printf("opendir 1\n");
		memset(dir, 0, sizeof(osd_directory));printf("opendir 11\n");
		dir->fd = NULL;
	}

	tmpstr = (char *) malloc/*_array*/(strlen(dirname)+1);

	strcpy(tmpstr, dirname);

	dir->fd = opendir(tmpstr);
	dir->path = tmpstr;

	if (dir && (dir->fd == NULL))
	{
		osd_free(dir->path);
		osd_free(dir);
		dir = NULL;
	}

	return dir;

}

static osd_dir_entry_type get_attributes_stat(const char *file)
{
	sdl_stat st;
	if(stat(file, &st))
		return (osd_dir_entry_type) 0;

	if (S_ISDIR(st.st_mode))
		return ENTTYPE_DIR;
	else
		return ENTTYPE_FILE;
}

static char *build_full_path(const char *path, const char *file)
{
	char *ret = (char *) malloc/*_array*/(strlen(path)+strlen(file)+2);
	char *p = ret;

	strcpy(p, path);
	p += strlen(path);
	*p++ = PATHSEPCH;
	strcpy(p, file);
	return ret;
}

static UINT64 osd_get_file_size(const char *file)
{
	sdl_stat st;
	if(stat(file, &st))
		return 0;
	return st.st_size;
}
//============================================================
//  osd_readdir
//============================================================

const osd_directory_entry *osd_readdir(osd_directory *dir)
{

	char *temp;
	dir->data = readdir(dir->fd);

	if (dir->data == NULL)
		return NULL;

	dir->ent.name = dir->data->d_name;
	temp = build_full_path(dir->path, dir->data->d_name);

	dir->ent.type = get_attributes_stat(temp);

	dir->ent.size = osd_get_file_size(temp);

	osd_free(temp);
	return &dir->ent;
}


//============================================================
//  osd_closedir
//============================================================

void osd_closedir(osd_directory *dir)
{
	if (dir->fd != NULL)
		closedir(dir->fd);
	osd_free(dir->path);
	osd_free(dir);

}



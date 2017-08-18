#include <unistd.h>
#include <stdint.h>

#include "osdepend.h"

#include "clifront.h"
#include "render.h"
#include "ui.h"
#include "uiinput.h"
#include "driver.h"
#include "input.h"
#include "libretro.h" 

#include "log.h"

char g_rom_dir[1024];

#define FUNC_PREFIX(x) rgb565_##x
#define PIXEL_TYPE UINT16
#define SRCSHIFT_R 3
#define SRCSHIFT_G 2
#define SRCSHIFT_B 3
#define DSTSHIFT_R 11
#define DSTSHIFT_G 5
#define DSTSHIFT_B 0

#include "rendersw.c"
 
#define FUNC_PREFIX(x) rgb888_##x
#define PIXEL_TYPE UINT32
#define SRCSHIFT_R 0
#define SRCSHIFT_G 0
#define SRCSHIFT_B 0
#define DSTSHIFT_R 16
#define DSTSHIFT_G 8
#define DSTSHIFT_B 0

#include "rendersw.c"

int mame_reset = -1;
static int ui_ipt_pushchar=-1;

static bool mouse_enable = false;
static bool videoapproach1_enable = false;
bool cheats_enable = false;

static void extract_basename(char *buf, const char *path, size_t size)
{
   const char *base = strrchr(path, '/');
   if (!base)
      base = strrchr(path, '\\');
   if (!base)
      base = path;

   if (*base == '\\' || *base == '/')
      base++;

   strncpy(buf, base, size - 1);
   buf[size - 1] = '\0';

   char *ext = strrchr(buf, '.');
   if (ext)
      *ext = '\0';
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   char *base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}
//============================================================
//  CONSTANTS
//============================================================

// fake a keyboard mapped to retro joypad 
enum
{
	KEY_F11,
	KEY_TAB,
	KEY_F3,
	KEY_F2,
	KEY_START,
	KEY_COIN,
	KEY_BUTTON_1,
	KEY_BUTTON_2,
	KEY_BUTTON_3,
	KEY_BUTTON_4,
	KEY_BUTTON_5,
	KEY_BUTTON_6, 
	KEY_JOYSTICK_U,
	KEY_JOYSTICK_D,
	KEY_JOYSTICK_L,
	KEY_JOYSTICK_R,
	KEY_TOTAL
}; 

#ifdef DEBUG_LOG
# define LOG(msg) fprintf(stderr, "%s\n", msg)
#else
# define LOG(msg)
#endif 

//============================================================
//  GLOBALS
//============================================================

// rendering target
static render_target *our_target = NULL;

// input device
static input_device *P1_device; // P1 JOYPAD
static input_device *P2_device; // P2 JOYPAD

static input_device *retrokbd_device; // KEYBD
static input_device *mouse_device;    // MOUSE

// state 
static UINT8/*16*/ P1_state[KEY_TOTAL];
static UINT8/*16*/ P2_state[KEY_TOTAL];
static UINT16 retrokbd_state[RETROK_LAST];
static UINT16 retrokbd_state2[RETROK_LAST];

struct kt_table
{
   const char  *   mame_key_name;
   int retro_key_name;
   input_item_id   mame_key;
};
static int mouseLX,mouseLY;
static int mouseBUT[4];
//static int mouse_enabled;

int optButtonLayoutP1 = 0; //for player 1
int optButtonLayoutP2 = 0; //for player 2

//enables / disables tate mode
static int tate = 0;
static int screenRot = 0;
int vertical,orient;

static char MgamePath[1024];
static char MgameName[512];

static int FirstTimeUpdate = 1;

bool retro_load_ok  = false;
int pauseg=0; 
int NEWGAME_FROM_OSD = 0;
float retro_aspect    = 1.0;
float retro_fps       = 60.0;
//============================================================
//  RETRO
//============================================================
static const char core[] = "mame2009";

static char XARGV[64][1024];
static const char* xargv_cmd[64];
int PARAMCOUNT=0;

#ifdef _WIN32
char slash = '\\';
#else
char slash = '/';
#endif

#include "retromapper.c"
#include "retroinput.c"
#include "retroosd.c"

// path configuration
#define NB_OPTPATH 11

static const char *dir_name[NB_OPTPATH]= {
    "cfg","nvram"/*,"hi"*/,"memcard","input",
    "states" ,"snaps","diff","samples",
    "artwork","cheat","ini"
};

static const char *opt_name[NB_OPTPATH]= {
    "-cfg_directory","-nvram_directory"/*,"-hiscore_directory"*/,"-memcard_directory","-input_directory",
    "-state_directory" ,"-snapshot_directory","-diff_directory","-samplepath",
    "-artpath","-cheatpath","-inipath"
};

int opt_type[NB_OPTPATH]={ // 0 for save_dir | 1 for system_dir
    0,0/*,0*/,0,
    0,0,0,0,
    1,1,1,1
};

//============================================================
//  main
//============================================================


static int parsePath(char* path, char* gamePath, char* gameName) {
	int i;
	int slashIndex = -1;
	int dotIndex = -1;
	int len = strlen(path);
	if (len < 1) {
		return 0;
	}

	for (i = len - 1; i >=0; i--) {
		if (path[i] == '/') {
			slashIndex = i;
			break;
		} else
		if (path[i] == '.') {
			dotIndex = i;
		}
	}
	if (slashIndex < 0 && dotIndex >0){
		strcpy(gamePath, ".\0");
		strncpy(gameName, path , dotIndex );
		gameName[dotIndex] = 0;
		write_log("gamePath=%s gameName=%s\n", gamePath, gameName);
		return 1;
	}
	if (slashIndex < 0 || dotIndex < 0) {
		return 0;
	}

	strncpy(gamePath, path, slashIndex);
	gamePath[slashIndex] = 0;
	strncpy(gameName, path + (slashIndex + 1), dotIndex - (slashIndex + 1));
	gameName[dotIndex - (slashIndex + 1)] = 0;

	write_log("gamePath=%s gameName=%s\n", gamePath, gameName);
	return 1;
}

static int getGameInfo(char* gameName, int* rotation, int* driverIndex) {
	int gameFound = 0;
	int drvindex;
//FIXME for 0.149 , prevouisly in driver.h
#if 1
	//check invalid game name
	if (gameName[0] == 0)
		return 0;

	for (drvindex = 0; drivers[drvindex]; drvindex++) {
		if ( (drivers[drvindex]->flags & GAME_NO_STANDALONE) == 0 &&
			mame_strwildcmp(gameName, drivers[drvindex]->name) == 0 ) {
				gameFound = 1;
				*driverIndex = drvindex;
				*rotation = drivers[drvindex]->flags & 0x7;
				write_log("%-18s\"%s\" rot=%i \n", drivers[drvindex]->name, drivers[drvindex]->description, *rotation);
		}
	}
#else 
	gameFound = 1;
#endif
	return gameFound;
} 

static void Add_Option(const char* option)
{
   static int pfirst = 0;

   if (pfirst == 0)
   {
      PARAMCOUNT=0;
      pfirst++;
   }

   sprintf(XARGV[PARAMCOUNT++], "%s", option);
}

static void Set_Path_Option(void)
{
   int i;
   char tmp_dir[256];

   /*Setup path option according to retro (save/system) directory,
    * or current if NULL. */

   for(i = 0; i < NB_OPTPATH; i++)
   {
      Add_Option((char*)(opt_name[i]));

      if(opt_type[i] == 0)
      {
         if (retro_save_directory)
            sprintf(tmp_dir, "%s%c%s%c%s", retro_save_directory, slash, core, slash,dir_name[i]);
         else
            sprintf(tmp_dir, "%s%c%s%c%s%c", ".", slash, core, slash,dir_name[i],slash);
      }
      else
      {
         if(retro_system_directory)
            sprintf(tmp_dir, "%s%c%s%c%s", retro_system_directory, slash, core, slash,dir_name[i]);
         else
            sprintf(tmp_dir, "%s%c%s%c%s%c", ".", slash, core, slash,dir_name[i],slash);
      }

      Add_Option((char*)(tmp_dir));
   }

}

static void Set_Default_Option(void)
{
   /* some hardcoded default options. */

   Add_Option(core);
/*
   if(throttle_enable)
      Add_Option("-throttle");
   else*/
   Add_Option("-nothrottle");
   Add_Option("-noautoframeskip");
   Add_Option("-joystick");
   Add_Option("-samplerate");
   Add_Option("48000");

   if(cheats_enable)
      Add_Option("-cheat");
   else
      Add_Option("-nocheat");

   if(mouse_enable)
      Add_Option("-mouse");
   else
      Add_Option("-nomouse");

/*
   if(write_config_enable)
      Add_Option("-writeconfig");

   if(read_config_enable)
      Add_Option("-readconfig");
   else
      Add_Option("-noreadconfig");

   if(auto_save_enable)
      Add_Option("-autosave");
*/
/*
   if(game_specific_saves_enable)
   {
      char option[50];
      Add_Option("-statename");
      sprintf(option,"%%g/%s",MgameName);
      Add_Option(option);
   }
*/
}

int executeGame(char* path) {
	// cli_frontend does the heavy lifting; if we have osd-specific options, we
	// create a derivative of cli_options and add our own

	int result = 0;
	int gameRot=0;

	int i,driverIndex;

   	for (i = 0; i < 64; i++)
      		xargv_cmd[i]=NULL;
	
	PARAMCOUNT=0;

	FirstTimeUpdate = 1;

	screenRot = 0;

	//split the path to directory and the name without the zip extension
	result = parsePath(path, MgamePath, MgameName);
	if (result == 0) {
		write_log("parse path failed! path=%s\n", path);
		strcpy(MgameName,path );
	//	return -1;
	}

	//Find the game info. Exit if game driver was not found.
	if (getGameInfo(MgameName, &gameRot, &driverIndex) == 0) {
		write_log("game not found: %s\n", MgameName);
		return -2;
	}

	//tate enabled
	if (tate) {
		//horizontal game
		if (gameRot == ROT0) {
			screenRot = 1;
		} else
		if (gameRot &  ORIENTATION_FLIP_X) {
			write_log("*********** flip X \n");
			screenRot = 3;
		}

	} else
	{
		if (gameRot != ROT0) {
			screenRot = 1;
			if (gameRot &  ORIENTATION_FLIP_X) {
				write_log("*********** flip X \n");
				screenRot = 2;
			}
		}
	}

	write_log("creating frontend... game=%s\n", MgameName);

   	Set_Default_Option();

   	Set_Path_Option();

   /* useless ? */
	if (tate)
	{
	      if (screenRot == 3)
	         Add_Option((char*) "-rol");
	}
	else
	{
	      if (screenRot == 2)
	         Add_Option((char*)"-rol");
	}

	Add_Option((char*)("-rompath"));

	Add_Option(MgamePath);

	Add_Option(MgameName);

	write_log("executing frontend... params:%i\n", PARAMCOUNT);

	for (i = 0; i < PARAMCOUNT; i++)
	{
		xargv_cmd[i] = (char*)(XARGV[i]);
      	   	write_log(" %s\n",XARGV[i]);
  	}

	result = cli_execute(PARAMCOUNT,(char**) xargv_cmd, NULL);
	write_log("mame cli_execute return:%d\n",result);
	xargv_cmd[PARAMCOUNT - 2] = NULL;

	return result;
}  
 
//============================================================
//  main
//============================================================

#ifdef __cplusplus
extern "C"
#endif
int mmain(int argc, const char *argv)
{
	static char gameName[1024];
	int result = 0;

	strcpy(gameName,argv);
	result = executeGame(gameName);
	if(result!=0)return -1;
	return 1;
}

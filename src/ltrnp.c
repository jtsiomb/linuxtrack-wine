#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <windows.h>
#include <ltlib.h>
#include "xkey.h"
#include "logger.h"
#include "cfg.h"

struct tir_data {
	short status;
	short frame;
	unsigned int checksum;
	float roll, pitch, yaw;
	float tx, ty, tz;
	float padding[9];
};

static struct sig_data {
	char dllsig[200];
	char appsig[200];
} sigdata = {
	"precise head tracking\n put your head into the game\n now go look around\n\n "
		"Copyright EyeControl Technologies",

	"hardware camera\n software processing data\n track user movement\n\n "
		"Copyright EyeControl Technologies"
};

static int ltr_initialized, tracking;


int WINAPI DllMain(void *hinst, unsigned int reason, void *reserved)
{
	switch(reason) {
	case DLL_PROCESS_ATTACH:
		init_logger("/tmp/npdll.log");

		logmsg("--- initializing linuxtrack ---\n");

		init_config();
		init_keyb();

		/* we defer linuxtrack initialization until we have the profile id
		 * (i.e until the program calls NP_RegisterProgramProfileID)
		 */
		break;

	case DLL_PROCESS_DETACH:
		logmsg("shutting down\n");
		shutdown_keyb();
		ltr_shutdown();
		cleanup_config();
		shutdown_logger();
		break;

	default:
		break;
	}

	return 1;
}


int __stdcall NPCLIENT_NP_GetSignature(struct sig_data *sig)
{
	assert(sizeof sigdata == 400);
	*sig = sigdata;

	logmsg("%s(c)\n", __func__);
	return 0;
}

int __stdcall NPCLIENT_NP_QueryVersion(short *ver)
{
	*ver = 0x0400;
	logmsg("%s\n", __func__);
	return 0;
}

int __stdcall NPCLIENT_NP_ReCenter(void)
{
	logmsg("%s\n", __func__);
	ltr_recenter();
	return 0;
}

int __stdcall NPCLIENT_NP_RegisterWindowHandle(void *handle)
{
	logmsg("%s\n", __func__);
	return 0;
}

int __stdcall NPCLIENT_NP_UnregisterWindowHandle(void)
{
	logmsg("%s\n", __func__);
	return 0;
}

int __stdcall NPCLIENT_NP_RegisterProgramProfileID(short id)
{
	const char *profile = app_name(id);
	if(!profile) {
		profile = "Default";
	} else {
		/* if we haven't ran this game before add it to the linuxtrack profiles
		 * (in ~/.linuxtrack) so that the user can configure it properly.
		 */
		add_ltr_profile(profile);
	}

	logmsg("Program profile: %s (%d)\n", profile, (int)id);

	if(ltr_initialized) {
		ltr_shutdown();
	}

	if(ltr_init((char*)profile) != 0) {
		logmsg("failed to initialize linuxtrack\n");
		return 1;
	}
	if(!tracking) {
		ltr_suspend();
	}
	ltr_initialized = 1;

	return 0;
}


int __stdcall NPCLIENT_NP_RequestData(short data)
{
	logmsg("%s\n", __func__);
	return 0;
}

int __stdcall NPCLIENT_NP_GetData(void *data)
{
	static short frame;
	unsigned int counter;
	struct tir_data *tir = data;

	memset(tir, 0, sizeof *tir);

	if(ltr_get_camera_update(&tir->yaw, &tir->pitch, &tir->roll,
				&tir->tx, &tir->ty, &tir->tz, &counter) < 0) {
		logmsg("ltr_get_camera_update failed!\n");
	}
	tir->frame = frame++;

	tir->yaw = -(tir->yaw / 180.0f) * 16384.0f;
	tir->pitch = -(tir->pitch / 180.0f) * 16384.0f;
	tir->roll = -(tir->roll / 180.0f) * 16384.0f;

	tir->tx = -tir->tx * 64.0f;
	tir->ty = tir->ty * 64.0f;
	tir->tz = tir->tz * 64.0f;

	/*logmsg("r(%.3f %.3f %.3f) t(%.3f %.3f %.3f)\n", tir->yaw, tir->pitch, tir->roll, tir->tx, tir->ty, tir->tz);*/

	return 0;
}


int __stdcall NPCLIENT_NP_StopCursor(void)
{
	logmsg("%s\n", __func__);
	return 0;
}

int __stdcall NPCLIENT_NP_StartCursor(void)
{
	logmsg("%s\n", __func__);
	return 0;
}


int __stdcall NPCLIENT_NP_StartDataTransmission(void)
{
	logmsg("%s\n", __func__);

	if(ltr_initialized) {
		ltr_wakeup();
		sleep(1);
		ltr_recenter();
	}
	tracking = 1;
	return 0;
}

int __stdcall NPCLIENT_NP_StopDataTransmission(void)
{
	logmsg("%s\n", __func__);
	ltr_suspend();
	return 0;
}

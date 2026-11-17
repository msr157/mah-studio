#include "MahMultichatDock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mah-multichat", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "MAH Multichat — Twitch IRC + YouTube/Kick dock helpers";
}

bool obs_module_load(void)
{
	auto *mainwin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainwin)
		return false;

	auto *dock = new MahMultichatDock();
	if (!obs_frontend_add_dock_by_id("mah-multichat-dock", "MAH Multichat", dock)) {
		delete dock;
		return false;
	}

	blog(LOG_INFO, "[mah-multichat] dock registered");
	return true;
}

void obs_module_unload(void) {}

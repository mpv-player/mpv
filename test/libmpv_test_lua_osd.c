/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libmpv_common.h"


struct osdTestValues {
	char* initial_volume;
	char* initial_mute;
	char* started_paused;
	char* initial_brightness;
	char* initial_saturation;
	char* initial_contrast;

	char* volume_after_change;
	char* second_mute;
	char* second_paused;
	char* brightness_after_change;
	char* saturation_after_change;
	char* contrast_after_change;
};


static void set_option_string(const char *name, const char *value)
{
    int ret = mpv_set_option_string(ctx, name, value);
    if (ret < 0)
        fail("mpv API error while setting option '%s' to '%s' (%s)\n", name, value, mpv_error_string(ret));
}


static void free_osd_values(struct osdTestValues* C_osd_values, struct osdTestValues* Lua_osd_values) {
	// Free everything
	mpv_free(C_osd_values->initial_volume);
	mpv_free(C_osd_values->initial_mute);
	mpv_free(C_osd_values->started_paused);
	mpv_free(C_osd_values->initial_brightness);
	mpv_free(C_osd_values->initial_saturation);
	mpv_free(C_osd_values->initial_contrast);

	mpv_free(C_osd_values->volume_after_change);
	mpv_free(C_osd_values->second_mute);
	mpv_free(C_osd_values->second_paused);
	mpv_free(C_osd_values->brightness_after_change);
	mpv_free(C_osd_values->saturation_after_change);
	mpv_free(C_osd_values->contrast_after_change);

	mpv_free(Lua_osd_values->initial_volume);
	mpv_free(Lua_osd_values->initial_mute);
	mpv_free(Lua_osd_values->started_paused);
	mpv_free(Lua_osd_values->initial_brightness);
	mpv_free(Lua_osd_values->initial_saturation);
	mpv_free(Lua_osd_values->initial_contrast);

	mpv_free(Lua_osd_values->volume_after_change);
	mpv_free(Lua_osd_values->second_mute);
	mpv_free(Lua_osd_values->second_paused);
	mpv_free(Lua_osd_values->brightness_after_change);
	mpv_free(Lua_osd_values->saturation_after_change);
	mpv_free(Lua_osd_values->contrast_after_change);
}


static void test_c_osd(struct osdTestValues* C_osd_values)
{

	C_osd_values->initial_volume = mpv_get_property_string(ctx, "volume");
	printf("Initial volume: %s\n", C_osd_values->initial_volume);

	C_osd_values->initial_mute = mpv_get_property_string(ctx, "mute");
	printf("Initial mute flag: %s\n", C_osd_values->initial_mute);

	C_osd_values->started_paused = mpv_get_property_string(ctx, "pause");
	printf("Started paused: %s\n", C_osd_values->started_paused);

	C_osd_values->initial_brightness = mpv_get_property_string(ctx, "brightness");
	printf("Initial brightness: %s\n", C_osd_values->initial_brightness);

	C_osd_values->initial_saturation = mpv_get_property_string(ctx, "saturation");
	printf("Initial saturation: %s\n", C_osd_values->initial_saturation);

	C_osd_values->initial_contrast = mpv_get_property_string(ctx, "contrast");
	printf("Initial contrast: %s\n", C_osd_values->initial_contrast);



	mpv_set_property_string(ctx, "volume", "60");
	C_osd_values->volume_after_change = mpv_get_property_string(ctx, "volume");
	printf("Volume after change: %s\n", C_osd_values->volume_after_change);

	mpv_set_property_string(ctx, "mute", "yes");
	C_osd_values->second_mute = mpv_get_property_string(ctx, "mute");
	printf("Mute flag after changing it: %s\n", C_osd_values->second_mute);
	
	mpv_set_property_string(ctx, "pause", "yes");
	C_osd_values->second_paused = mpv_get_property_string(ctx, "pause");
	printf("Paused after pause command: %s\n", C_osd_values->second_paused);

	mpv_set_property_string(ctx, "brightness", "-70");
	C_osd_values->brightness_after_change = mpv_get_property_string(ctx, "brightness");
	printf("Brightness after change: %s\n", C_osd_values->brightness_after_change);

	mpv_set_property_string(ctx, "saturation", "50");
	C_osd_values->saturation_after_change = mpv_get_property_string(ctx, "saturation");
	printf("Saturation after change: %s\n", C_osd_values->saturation_after_change);
	
	mpv_set_property_string(ctx, "contrast", "4");
	C_osd_values->contrast_after_change = mpv_get_property_string(ctx, "contrast");
	printf("Contrast after change: %s\n", C_osd_values->contrast_after_change);



}



static void test_lua_osd(struct osdTestValues* Lua_osd_values)
{

	Lua_osd_values->initial_volume = mpv_get_property_string(ctx, "volume");
	printf("Initial volume: %s\n", Lua_osd_values->initial_volume);

	Lua_osd_values->initial_mute = mpv_get_property_string(ctx, "mute");
	printf("Initial mute flag: %s\n", Lua_osd_values->initial_mute);

	Lua_osd_values->started_paused = mpv_get_property_string(ctx, "pause");
	printf("Started paused: %s\n", Lua_osd_values->started_paused);

	Lua_osd_values->initial_brightness = mpv_get_property_string(ctx, "brightness");
	printf("Initial brightness: %s\n", Lua_osd_values->initial_brightness);

	Lua_osd_values->initial_saturation = mpv_get_property_string(ctx, "saturation");
	printf("Initial saturation: %s\n", Lua_osd_values->initial_saturation);

	Lua_osd_values->initial_contrast = mpv_get_property_string(ctx, "contrast");
	printf("Initial contrast: %s\n", Lua_osd_values->initial_contrast);


	mpv_set_property_string(ctx, "volume", "60");
	Lua_osd_values->volume_after_change = mpv_get_property_string(ctx, "volume");
	printf("Volume after change: %s\n", Lua_osd_values->volume_after_change);

	mpv_set_property_string(ctx, "mute", "yes");
	Lua_osd_values->second_mute = mpv_get_property_string(ctx, "mute");
	printf("Mute flag after changing it: %s\n", Lua_osd_values->second_mute);

	mpv_set_property_string(ctx, "pause", "yes");
	Lua_osd_values->second_paused = mpv_get_property_string(ctx, "pause");
	printf("Paused after pause command: %s\n", Lua_osd_values->second_paused);

	mpv_set_property_string(ctx, "brightness", "-70");
	Lua_osd_values->brightness_after_change = mpv_get_property_string(ctx, "brightness");
	printf("Brightness after change: %s\n", Lua_osd_values->brightness_after_change);

	mpv_set_property_string(ctx, "saturation", "50");
	Lua_osd_values->saturation_after_change = mpv_get_property_string(ctx, "saturation");
	printf("Saturation after change: %s\n", Lua_osd_values->saturation_after_change);
	
	mpv_set_property_string(ctx, "contrast", "4");
	Lua_osd_values->contrast_after_change = mpv_get_property_string(ctx, "contrast");
	printf("Luaontrast after change: %s\n", Lua_osd_values->contrast_after_change);




}

static void test_compare_values(struct osdTestValues* C_osd_values, struct osdTestValues* Lua_osd_values)
{

	int number_of_matches = 0;
	const char *matched = "%s: Matched.\n";
	const char *no_matched = "%s: DID NOT MATCH (C OSD = %s; Lua OSD = %s)\n";

	if (strcmp(C_osd_values->initial_volume, Lua_osd_values->initial_volume)) {
		printf(no_matched, "Initial volume", C_osd_values->initial_volume, Lua_osd_values->initial_volume);
	} else {
		printf(matched, "Initial volume");
		number_of_matches++;
	}

	if (strcmp(C_osd_values->initial_mute, Lua_osd_values->initial_mute)) {
		printf(no_matched, "Initial mute flag", C_osd_values->initial_mute, Lua_osd_values->initial_mute);
	} else {
		printf(matched, "Initial mute flag");
		number_of_matches++;
	}

	if (strcmp(C_osd_values->started_paused, Lua_osd_values->started_paused)) {
		printf(no_matched, "Started paused", C_osd_values->started_paused, Lua_osd_values->started_paused);
	} else {
		printf(matched, "Started paused");
		number_of_matches++;
	}

	if (strcmp(C_osd_values->initial_brightness, Lua_osd_values->initial_brightness)) {
		printf(no_matched, "Initial brightness", C_osd_values->initial_brightness, Lua_osd_values->initial_brightness);
	} else {
		printf(matched, "Initial brightness");
		number_of_matches++;
	}

	if (strcmp(C_osd_values->initial_saturation, Lua_osd_values->initial_saturation)) {
		printf(no_matched, "Initial saturation", C_osd_values->initial_saturation, Lua_osd_values->initial_saturation);
	} else {
		printf(matched, "Initial saturation");
		number_of_matches++;
	}

	if (strcmp(C_osd_values->initial_contrast, Lua_osd_values->initial_contrast)) {
		printf(no_matched, "Initial contrast", C_osd_values->initial_contrast, Lua_osd_values->initial_contrast);
	} else {
		printf(matched, "Initial contrast");
		number_of_matches++;
	}


	
	if (strcmp(C_osd_values->volume_after_change, Lua_osd_values->volume_after_change)) {
		printf(no_matched, "Volume after change", C_osd_values->volume_after_change, Lua_osd_values->volume_after_change);
	} else {
		printf(matched, "Volume after change");
		number_of_matches++;
	}


	if (strcmp(C_osd_values->second_mute, Lua_osd_values->second_mute)) {
		printf(no_matched, "Mute flag after changing it", C_osd_values->second_mute, Lua_osd_values->second_mute);
	} else {
		printf(matched, "Mute flag after changing it");
		number_of_matches++;
	}


	if (strcmp(C_osd_values->second_paused, Lua_osd_values->second_paused)) {
		printf(no_matched, "Paused after pause command", C_osd_values->second_paused, Lua_osd_values->second_paused);
	} else {
		printf(matched, "Paused after pause command");
		number_of_matches++;
	}

	if (strcmp(C_osd_values->brightness_after_change, Lua_osd_values->brightness_after_change)) {
		printf(no_matched, "Brightness after change", C_osd_values->brightness_after_change, Lua_osd_values->brightness_after_change);
	} else {
		printf(matched, "Brightness after change");
		number_of_matches++;
	}

	if (strcmp(C_osd_values->saturation_after_change, Lua_osd_values->saturation_after_change)) {
		printf(no_matched, "Saturation after change", C_osd_values->saturation_after_change, Lua_osd_values->saturation_after_change);
	} else {
		printf(matched, "Saturation after change");
		number_of_matches++;
	}

	if (strcmp(C_osd_values->contrast_after_change, Lua_osd_values->contrast_after_change)) {
		printf(no_matched, "Contrast after change", C_osd_values->contrast_after_change, Lua_osd_values->contrast_after_change);
	} else {
		printf(matched, "Contrast after change");
		number_of_matches++;
	}


	fflush(stdout);							// to ensure correct order of prints
	if (number_of_matches != 12) {
		fail("\nSome values of Lua's OSD did not match the intended values from the C OSD.\n");
	}


	free_osd_values(C_osd_values, Lua_osd_values);
}




int main(void)
{
	struct osdTestValues C_osd_values;
	struct osdTestValues Lua_osd_values;
		

    ctx = mpv_create();
    if (!ctx)
        return 1;

    atexit(exit_cleanup);

    set_option_string("osd-lua", "no");
    initialize();

    printf("================ TEST: Lua OSD ================\n");

    printf("--------------- Running C OSD to register expected values ---------------\n");
	test_c_osd(&C_osd_values);
    printf("--------------- Finished running C OSD ---------------\n");

    command_string("quit");

	while (wrap_wait_event()->event_id != MPV_EVENT_SHUTDOWN) {}
	
// -----------------------------------------------------------------------------------------

	ctx = mpv_create();
    if (!ctx)
        return 1;

    atexit(exit_cleanup);

    set_option_string("osd-lua", "yes");
    initialize();

    printf("--------------- Running Lua OSD to register values ---------------\n");
    test_lua_osd(&Lua_osd_values);
    printf("--------------- Finished running Lua OSD ---------------\n");

    command_string("quit");
    while (wrap_wait_event()->event_id != MPV_EVENT_SHUTDOWN) {}

    printf("================ SHUTDOWN ================\n");

// -----------------------------------------------------------------------------------------

	printf("--------------- Comparing Lua OSD values with C OSD values ---------------\n");
	test_compare_values(&C_osd_values, &Lua_osd_values);
    printf("--------------- Finished comparing values ---------------\n");

    return 0;
}


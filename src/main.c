/*
 * Pokémon Mini (PokeMini) — standalone Retro-Go SD dynamic core.
 *
 * Entry (run_dynamic_core):
 *   void app_main(uint8_t load_state, uint8_t start_paused, int8_t save_slot)
 *
 * Hot interpreter / video / audio .text lives in ITCM (pokemini_core.ld).
 * Working buffers that used to sit in ITCM via itc_malloc now use DTCM
 * (dtc_malloc). ROMs go to RAM_EMU (or flash cache for large carts).
 */

#include <odroid_system.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "gw_lcd.h"
#include "gw_buttons.h"
#include "common.h"
#include "rom_manager.h"
#include "appid.h"
#include "gw_malloc.h"
#include "rg_storage.h"

#include "PokeMini.h"
#include "MinxIO.h"
#include "PMCommon.h"
#include "Hardware.h"
#include "Joystick.h"
#include "MinxAudio.h"
#include "Video.h"
#include "Video_x3.h"

#include "pkmini_i18n.h"

#ifndef HOST_BUILD
#include "gw_core_bridge.h"
#else
#include "host_compat.h"
#endif

#define PKMINI_FPS 72
#define PKMINI_SAMPLE_RATE 44100
/* (PKMINI_SAMPLE_RATE/PKMINI_FPS) = 612.5 */
#define PKMINI_BUFFER_LENGTH_MIN 612
#define PKMINI_BUFFER_LENGTH_MAX 613
static int16_t pkmini_audio_samples[PKMINI_BUFFER_LENGTH_MAX];

#define SOUNDBUFFER 2048
#define PMSOUNDBUFF (SOUNDBUFFER * 2)
static int32_t low_pass_range = (60 * 65536) / 100;
static int32_t low_pass_prev;

#define PKMINI_SCALE 3
#define PKMINI_WIDTH 96
#define PKMINI_HEIGHT 64

uint16_t pkmini_data[PKMINI_SCALE * PKMINI_WIDTH * PKMINI_SCALE * PKMINI_HEIGHT];

static void blit(void);

static bool LoadState(const char *savePathName)
{
    return PokeMini_LoadSSStream(savePathName, 0);
}

static bool SaveState(const char *savePathName)
{
    return PokeMini_SaveSSStream(savePathName, 0);
}

static void *Screenshot(void)
{
    lcd_wait_for_vblank();
    blit();
    return lcd_get_active_buffer();
}

static void pkmini_sram_save_cb(void)
{
    if (PokeMini_EEPROMWritten) {
        PokeMini_SaveEEPROMFile(CommandLine.eeprom_file);
    }
}

static void blit(void)
{
    uint16_t x_offset = (WIDTH - PKMINI_WIDTH * PKMINI_SCALE) / 2;
    uint16_t y_offset = (HEIGHT - PKMINI_HEIGHT * PKMINI_SCALE) / 2;
    uint16_t *src_buffer = pkmini_data;
    uint16_t *dst_buffer = (uint16_t *)lcd_get_active_buffer() + y_offset * WIDTH + x_offset;
    for (int y = 0; y < PKMINI_HEIGHT * PKMINI_SCALE; y++) {
        memcpy(dst_buffer, (void *)src_buffer, PKMINI_WIDTH * PKMINI_SCALE * sizeof(uint16_t));
        src_buffer += PKMINI_WIDTH * PKMINI_SCALE;
        dst_buffer += WIDTH;
    }
}

static void ZeroArray(uint16_t array[], int size)
{
    for (int i = 0; i < size; i++)
        array[i] = 0;
}

static void ReverseArray(uint16_t array[], int size)
{
    for (int i = 0, j = size; i < j; i++, j--) {
        uint16_t tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
}

static void pkmini_pcm_submit(void)
{
    if (common_emu_sound_loop_is_muted()) {
        return;
    }

    int32_t factor = common_emu_sound_get_volume();
    int16_t *sound_buffer = audio_get_active_buffer();
    uint16_t sound_buffer_length = audio_get_buffer_length();

    for (int i = 0; i < sound_buffer_length; i++) {
        int32_t sample = pkmini_audio_samples[i];
        sound_buffer[i] = (sample * factor) >> 8;
    }
}

static void ApplyLowPassFilterUpmix(int16_t *buffer, int32_t length)
{
    int32_t low_pass = low_pass_prev;
    int32_t factor_a = low_pass_range;
    int32_t factor_b = 0x10000 - factor_a;

    do {
        low_pass = (low_pass * factor_a) + (*buffer * factor_b);
        low_pass >>= 16;
        *buffer++ = (int16_t)low_pass;
    } while (--length);

    low_pass_prev = low_pass;
}

static void SetPixelOffset(void)
{
    if (CommandLine.rumblelvl) {
        int row_offset = PokeMini_GenRumbleOffset(PKMINI_WIDTH * PKMINI_SCALE) * PKMINI_SCALE;
        int buffer_size = PKMINI_WIDTH * PKMINI_SCALE * PKMINI_HEIGHT * PKMINI_SCALE;

        if (row_offset < 0) {
            row_offset = buffer_size + row_offset;
            ZeroArray(pkmini_data, buffer_size - row_offset - 1);
            ReverseArray(pkmini_data + buffer_size - row_offset, row_offset - 1);
        } else {
            ReverseArray(pkmini_data, buffer_size - row_offset - 1);
            ZeroArray(pkmini_data + buffer_size - row_offset, row_offset - 1);
        }

        ReverseArray(pkmini_data, buffer_size - 1);
    }
}

static int load_rom(void)
{
    uint32_t size = 0;
    uint8_t *buffer;
    FILE *f;

    PokeMini_FreeColorInfo();

    f = fopen(ACTIVE_FILE->path, "rb");
    if (f != NULL) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fclose(f);
        if (sz > 0)
            size = (uint32_t)sz;
    }

    if ((size <= 0x2100) || (size > 0x200000))
        return 0;

    PM_ROM_Mask = GetMultiple2Mask(size);
    PM_ROM_Size = PM_ROM_Mask + 1;

    if ((size_t)PM_ROM_Size > ram_get_free_size()) {
        buffer = odroid_overlay_cache_file_in_flash(ACTIVE_FILE->path, &size, false);
    } else {
        buffer = ram_malloc(PM_ROM_Size);
        if (buffer) {
            odroid_overlay_cache_file_in_ram(ACTIVE_FILE->path, (uint8_t *)buffer);
        }
    }

    PM_ROM = buffer;
    NewMulticart();
    return 1;
}

static void Shutdown(void)
{
    char *system_config_path = odroid_system_get_path(ODROID_PATH_SYSTEM_CONFIG, ACTIVE_FILE->path);
    FILE *file = fopen(system_config_path, "w");
    if (file) {
        fwrite(&CommandLine, sizeof(CommandLine), 1, file);
        fclose(file);
    }
    free(system_config_path);

    PokeMini_Destroy();
}

static void handle_joystick_input(odroid_gamepad_state_t *joystick)
{
    MinxIO_Keypad(MINX_KEY_A, joystick->values[ODROID_INPUT_A] == 1);
    MinxIO_Keypad(MINX_KEY_B, joystick->values[ODROID_INPUT_B] == 1);
    MinxIO_Keypad(MINX_KEY_C,
                  joystick->values[ODROID_INPUT_START] == 1 ||
                  joystick->values[ODROID_INPUT_X] == 1);
    MinxIO_Keypad(MINX_KEY_SHOCK,
                  joystick->values[ODROID_INPUT_SELECT] == 1 ||
                  joystick->values[ODROID_INPUT_Y] == 1);
    MinxIO_Keypad(MINX_KEY_UP, joystick->values[ODROID_INPUT_UP] == 1);
    MinxIO_Keypad(MINX_KEY_DOWN, joystick->values[ODROID_INPUT_DOWN] == 1);
    MinxIO_Keypad(MINX_KEY_LEFT, joystick->values[ODROID_INPUT_LEFT] == 1);
    MinxIO_Keypad(MINX_KEY_RIGHT, joystick->values[ODROID_INPUT_RIGHT] == 1);
}

static void pkmini_apply_changes(void)
{
    PokeMini_VideoPalette_Index(CommandLine.palette, NULL, CommandLine.lcdcontrast, CommandLine.lcdbright);
    PokeMini_ApplyChanges();
}

#define PALETTE_COUNT 14

static bool palette_update_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    (void)repeat;
    int pal = CommandLine.palette;
    int max = PALETTE_COUNT - 1;

    if (event == ODROID_DIALOG_PREV) pal = pal > 0 ? pal - 1 : max;
    if (event == ODROID_DIALOG_NEXT) pal = pal < max ? pal + 1 : 0;

    if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT) {
        CommandLine.palette = pal;
        pkmini_apply_changes();
    }

    const char *palette_name;
    switch (pal) {
    case 0: palette_name = gw_i18n(pkmini_i18n_pal_default); break;
    case 1: palette_name = gw_i18n(pkmini_i18n_pal_old); break;
    case 2: palette_name = gw_i18n(pkmini_i18n_pal_bw); break;
    case 3: palette_name = gw_i18n(pkmini_i18n_pal_green); break;
    case 4: palette_name = gw_i18n(pkmini_i18n_pal_green_inv); break;
    case 5: palette_name = gw_i18n(pkmini_i18n_pal_red); break;
    case 6: palette_name = gw_i18n(pkmini_i18n_pal_red_inv); break;
    case 7: palette_name = gw_i18n(pkmini_i18n_pal_blue_lcd); break;
    case 8: palette_name = gw_i18n(pkmini_i18n_pal_led); break;
    case 9: palette_name = gw_i18n(pkmini_i18n_pal_girl); break;
    case 10: palette_name = gw_i18n(pkmini_i18n_pal_blue); break;
    case 11: palette_name = gw_i18n(pkmini_i18n_pal_blue_inv); break;
    case 12: palette_name = gw_i18n(pkmini_i18n_pal_sepia); break;
    case 13: palette_name = gw_i18n(pkmini_i18n_pal_bw_inv); break;
    default: palette_name = "Unknown"; break;
    }

    sprintf(option->value, "%10s", palette_name);
    return event == ODROID_DIALOG_ENTER;
}

static bool lcd_filter_update_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    (void)repeat;
    int filter = CommandLine.lcdfilter;
    int max = 2;

    if (event == ODROID_DIALOG_PREV) filter = filter > 0 ? filter - 1 : max;
    if (event == ODROID_DIALOG_NEXT) filter = filter < max ? filter + 1 : 0;

    if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT) {
        CommandLine.lcdfilter = filter;
        pkmini_apply_changes();
    }

    const char *filter_name;
    switch (filter) {
    case 0: filter_name = gw_i18n(pkmini_i18n_filt_none); break;
    case 1: filter_name = gw_i18n(pkmini_i18n_filt_dot); break;
    case 2: filter_name = gw_i18n(pkmini_i18n_filt_scan); break;
    default: filter_name = "Unknown"; break;
    }

    sprintf(option->value, "%10s", filter_name);
    return event == ODROID_DIALOG_ENTER;
}

static bool lcd_mode_update_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    (void)repeat;
    int mode = CommandLine.lcdmode;
    int max = 2;

    if (event == ODROID_DIALOG_PREV) mode = mode > 0 ? mode - 1 : max;
    if (event == ODROID_DIALOG_NEXT) mode = mode < max ? mode + 1 : 0;

    if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT) {
        CommandLine.lcdmode = mode;
        pkmini_apply_changes();
    }

    const char *mode_name;
    switch (mode) {
    case 0: mode_name = gw_i18n(pkmini_i18n_mode_analog); break;
    case 1: mode_name = gw_i18n(pkmini_i18n_mode_3); break;
    case 2: mode_name = gw_i18n(pkmini_i18n_mode_2); break;
    default: mode_name = "Unknown"; break;
    }

    sprintf(option->value, "%10s", mode_name);
    return event == ODROID_DIALOG_ENTER;
}

static char *piezo_filter_names[2] = { "\x5", "\x6" };

static bool piezo_filter_update_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    (void)repeat;
    int filter = CommandLine.piezofilter;
    int max = 1;

    if (event == ODROID_DIALOG_PREV) filter = filter > 0 ? filter - 1 : max;
    if (event == ODROID_DIALOG_NEXT) filter = filter < max ? filter + 1 : 0;

    if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT) {
        CommandLine.piezofilter = filter;
        pkmini_apply_changes();
    }
    sprintf(option->value, "%10s", piezo_filter_names[filter]);
    return event == ODROID_DIALOG_ENTER;
}

static char *low_pass_filter_names[2] = { "\x5", "\x6" };

static bool low_pass_filter_update_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    (void)repeat;
    int filter = CommandLine.lowpassfilter;
    int max = 1;

    if (event == ODROID_DIALOG_PREV) filter = filter > 0 ? filter - 1 : max;
    if (event == ODROID_DIALOG_NEXT) filter = filter < max ? filter + 1 : 0;

    if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT) {
        CommandLine.lowpassfilter = filter;
    }
    sprintf(option->value, "%10s", low_pass_filter_names[filter]);
    return event == ODROID_DIALOG_ENTER;
}

_Noreturn void app_main(uint8_t load_state, uint8_t start_paused, int8_t save_slot)
{
    char palette_values[40];
    char lcd_filter_values[32];
    char lcd_mode_values[32];
    char piezo_filter_values[16];
    char low_pass_filter_values[16];
    odroid_dialog_choice_t options[] = {
        {100, gw_i18n(pkmini_i18n_palette), (char *)palette_values, 1, &palette_update_cb},
        {100, gw_i18n(pkmini_i18n_lcd_filter), (char *)lcd_filter_values, 1, &lcd_filter_update_cb},
        {100, gw_i18n(pkmini_i18n_lcd_mode), (char *)lcd_mode_values, 1, &lcd_mode_update_cb},
        {100, gw_i18n(pkmini_i18n_piezo), (char *)piezo_filter_values, 1, &piezo_filter_update_cb},
        {100, gw_i18n(pkmini_i18n_lpf), (char *)low_pass_filter_values, 1, &low_pass_filter_update_cb},
        ODROID_DIALOG_CHOICE_LAST
    };
    TPokeMini_VideoSpec *video_spec = NULL;
    odroid_gamepad_state_t joystick;

    if (start_paused) {
        common_emu_state.pause_after_frames = 2;
    } else {
        common_emu_state.pause_after_frames = 0;
    }
    common_emu_state.frame_time_10us = (uint16_t)(100000 / PKMINI_FPS + 0.5f);

    lcd_set_refresh_rate(PKMINI_FPS);

    odroid_system_init(APPID_CORE, PKMINI_SAMPLE_RATE);
    odroid_system_emu_init(&LoadState, &SaveState, &Screenshot, &Shutdown, NULL, &pkmini_sram_save_cb, NULL);

    audio_start_playing_full_length(PKMINI_BUFFER_LENGTH_MAX + PKMINI_BUFFER_LENGTH_MIN);

    CommandLineInit();
    strcpy(CommandLine.bios_file, "/bios/mini/bios.min");

    char *sram_path = odroid_system_get_path(ODROID_PATH_SAVE_SRAM, ACTIVE_FILE->path);
    strcpy(CommandLine.eeprom_file, sram_path);
    free(sram_path);

    CommandLine.palette = 0;
    CommandLine.lcdfilter = 1;
    CommandLine.lcdmode = 0;
    CommandLine.piezofilter = 1;
    CommandLine.lowpassfilter = 1;
    CommandLine.forcefreebios = 0;
    CommandLine.eeprom_share = 0;
    CommandLine.updatertc = 2;

    char *system_config_path = odroid_system_get_path(ODROID_PATH_SYSTEM_CONFIG, ACTIVE_FILE->path);
    {
        rg_stat_t st = rg_storage_stat(system_config_path);
        if (st.exists && st.size == sizeof(CommandLine)) {
            FILE *file = fopen(system_config_path, "r");
            if (file) {
                fread(&CommandLine, sizeof(CommandLine), 1, file);
                fclose(file);
            }
        }
    }
    free(system_config_path);

    video_spec = (TPokeMini_VideoSpec *)&PokeMini_Video3x3;

    PokeMini_SetVideo(video_spec, 16, 0, 0);
    PokeMini_Create(0, PMSOUNDBUFF);
    PokeMini_VideoPalette_Init(PokeMini_BGR16, 0 /* disable high colour */);
    PokeMini_VideoPalette_Index(CommandLine.palette, NULL, CommandLine.lcdcontrast, CommandLine.lcdbright);
    PokeMini_ApplyChanges();
    MinxAudio_ChangeEngine(CommandLine.sound);

    load_rom();

    MinxIO_FormatEEPROM();
    if (rg_storage_stat(CommandLine.eeprom_file).exists) {
        PokeMini_LoadEEPROMFile(CommandLine.eeprom_file);
    }

    PokeMini_Reset(0);

    if (load_state) {
        odroid_system_emu_load_state(save_slot);
    } else {
        lcd_clear_buffers();
    }

    while (true) {
        wdog_refresh();

        bool drawFrame = common_emu_frame_loop();

        odroid_input_read_gamepad(&joystick);
        common_emu_input_loop(&joystick, options, &blit);
        common_emu_input_loop_handle_turbo(&joystick);

        handle_joystick_input(&joystick);

        PokeMini_EmulateFrame();

        MinxAudio_GetSamplesS16Ch(pkmini_audio_samples, audio_get_buffer_length(), 1);
        if (CommandLine.lowpassfilter) {
            ApplyLowPassFilterUpmix(pkmini_audio_samples, audio_get_buffer_length());
        }
        pkmini_pcm_submit();

        if (drawFrame) {
            PokeMini_VideoBlit((uint16_t *)pkmini_data, PKMINI_WIDTH * PKMINI_SCALE);
            if (PokeMini_Rumbling)
                SetPixelOffset();
            blit();
            lcd_swap();
        }
        common_ingame_overlay();

        common_emu_sound_sync(false);
    }
}

/*
  PokeMini - Pokémon-Mini Emulator
  Copyright (C) 2009-2015  JustBurn

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// Savestate file ID
//   Change only if savestate structure file changes!
#define PokeMini_ID 0x006B4D50

#include "PokeMini.h"
#include <time.h>
#ifndef TARGET_GNW
#include <streams/file_stream.h>
#else
#include "rg_storage.h"
#endif

// Include Free BIOS
#include "freebios.h"
int PokeMini_FreeBIOS = 0;	// Using freebios?

int PokeMini_Flags = 0;		// Configuration flags
uint8_t PM_BIOS[4096];		// Pokemon-Mini BIOS ($000000 to $000FFF, 4096)
uint8_t PM_RAM[8192];		// Pokemon-Mini RAM  ($001000 to $002100, 4096 + 256)
uint8_t *PM_ROM = NULL;		// Pokemon-Mini ROM  ($002100 to $1FFFFF, Up to 2MB)
#ifndef TARGET_GNW
int PM_ROM_Alloc = 0;		// Pokemon-Mini ROM Allocated on memory?
#endif
int PM_ROM_Size = 0;		// Pokemon-Mini ROM Size
int PM_ROM_Mask = 0;		// Pokemon-Mini ROM Mask
int PokeMini_LCDMode = 0;	// LCD Mode
int PokeMini_ColorFormat = 0;	// Color Format (0 = 8x8, 1 = 4x4)
int PokeMini_HostBattStatus = 0;// Host battery status
int PokeMini_RumbleAnim = 0;	// Rumble animation

int PokeMini_RumbleAmount[4] = { -2,  1, -1,  2 };
const int PokeMini_RumbleAmountTable[16] = {
	 0,  0,  0,  0,
	-1,  0,  1,  0,
	-1,  1, -1,  1,
	-2,  1, -1,  2,
};

const uint8_t PM_IO_INIT[256] = {
	0x7F, 0x20, 0x5C, 0xff, 0xff, 0xff, 0xff, 0xff, // $00~$07 System Control
	0x01, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, // $08~$0F Second Counter
	0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $10~$17 Battery Sensor
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, // $18~$1F Timers Controls
	0x00, 0x30, 0x02, 0x00, 0x02, 0x00, 0x00, 0x40, // $20~$27 IRQ
	0x00, 0xC0, 0x40, 0xff, 0xff, 0xff, 0xff, 0xff, // $28~$2F IRQ
	0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, // $30~$37 Timer 1
	0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, // $38~$3F Timer 2
	0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, // $40~$47 256Hz Counter + ???
	0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, // $48~$4F Timer 3
	0xFF, 0x00, 0xFF, 0x00, 0x01, 0x01, 0xff, 0xff, // $50~$57 Keypad + ???
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $58~$5F Unused
	0x32, 0x64, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, // $60~$67 I/O
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $68~$6F Unused
	0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $70~$77 Audio + ???
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $78~$7F Unused
	0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $80~$87 PRC
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $88~$8F PRC
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $90~$97 Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $98~$9F Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $A0~$A7 Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $A8~$AF Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $B0~$B7 Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $B8~$BF Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $C0~$C7 Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $C8~$CF Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $D0~$D7 Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $D8~$DF Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $E0~$E7 Unused
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // $E8~$EF Unused
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $F0~$F7 ???
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x40, 0xFF  // $F8~$FF LCD I/O
};

// Number of cycles to process on hardware
int PokeHWCycles = 0;

// Create emulator and all interfaces
int PokeMini_Create(int flags, int soundfifo)
{
	// Clear BIOS & RAM
	memset(PM_BIOS, 0xFF, 4096);
	memset(PM_RAM, 0xFF, 8192);	// RAM + IO

	// Dummy ROM (In case LoadROM fail)
#ifndef TARGET_GNW
	PM_ROM_Alloc = 0;
#endif
	PM_ROM_Mask = 0x1FFF;
	PM_ROM_Size = 0x2000;
	PM_ROM = (unsigned char *)PM_RAM;

	// Clear & Init State
	memcpy(PM_IO, PM_IO_INIT, 256);
	if (!MinxCPU_Create()) return 0;
	if (!MinxTimers_Create()) return 0;
	if (!MinxIO_Create()) return 0;
	if (!MinxIRQ_Create()) return 0;
	if (!MinxPRC_Create()) return 0;
	if (!MinxColorPRC_Create()) return 0;
	if (!MinxLCD_Create()) return 0;
	if (!MinxAudio_Create(soundfifo, soundfifo)) return 0;

	// Initialize variables
	PokeMini_Flags = flags;

	// Load FreeBIOS
	PokeMini_LoadFreeBIOS();

#ifndef PERFORMANCE
	// Initialize multicart
	NewMulticart();
	SetMulticart(CommandLine.multicart);
#endif

	return 1;
}

// Destroy emulator and all interfaces
void PokeMini_Destroy()
{
	// Destroy all interfaces
	MinxAudio_Destroy();
	MinxLCD_Destroy();
	MinxPRC_Destroy();
	MinxColorPRC_Destroy();
	MinxIRQ_Destroy();
	MinxIO_Destroy();
	MinxTimers_Destroy();
	MinxCPU_Destroy();

	// Free ROM if was allocated
#ifndef TARGET_GNW
	if (PM_ROM_Alloc) {
		free(PM_ROM);
		PM_ROM = NULL;
		PM_ROM_Alloc = 0;
	}
#endif

	// Free color info
	PokeMini_FreeColorInfo();
}

// Apply changes from command lines
void PokeMini_ApplyChanges(void)
{
	int i;
	if (CommandLine.low_battery != 2) MinxIO_BatteryLow(CommandLine.low_battery);
	PokeMini_SetLCDMode(CommandLine.lcdmode);
	PokeMini_SetVideo(PokeMini_VideoCurrent, PokeMini_VideoDepth, CommandLine.lcdfilter, CommandLine.lcdmode);
	MinxAudio_ChangeFilter(CommandLine.piezofilter);
	for (i=0; i<4; i++) PokeMini_RumbleAmount[i] = PokeMini_RumbleAmountTable[((CommandLine.rumblelvl & 3) << 2) | i];
}

// Low power battery emulation
void PokeMini_LowPower(int enable)
{
	if (CommandLine.low_battery == 2) {
		PokeMini_HostBattStatus = enable;
		if (PokeMini_BatteryStatus != enable) MinxIO_BatteryLow(enable);
	}
}

// Set LCD mode
void PokeMini_SetLCDMode(int mode)
{
	PokeMini_LCDMode = mode;
	MinxPRC_Render = MinxPRC_Render_Mono;
	if (mode == LCDMODE_COLORS) {
		if (PokeMini_ColorFormat == 1) MinxPRC_Render = MinxPRC_Render_Color4;
		else MinxPRC_Render = MinxPRC_Render_Color8;
	}
}

// Generate rumble offset
int PokeMini_GenRumbleOffset(int pitch)
{
	return PokeMini_RumbleAmount[PokeMini_RumbleAnim++ & 3] * pitch;
}

// Load BIOS from file
int PokeMini_LoadBIOSFile(const char *filename)
{
	int64_t readbytes;
	// Open file
#ifndef TARGET_GNW
	RFILE *fbios = filestream_open(filename,
			RETRO_VFS_FILE_ACCESS_READ,
			RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!fbios)
		return 0;

	// Read content
	readbytes = filestream_read(fbios, PM_BIOS, 4096);
	PokeMini_FreeBIOS = 0;

	// Close file
	filestream_close(fbios);
#else
	FILE *fbios = fopen(filename, "rb");
	if (!fbios)
		return 0;
	readbytes = fread(PM_BIOS, 1, 4096, fbios);
	PokeMini_FreeBIOS = 0;
	fclose(fbios);
#endif

	return (readbytes == 4096);
}

// Load FreeBIOS
int PokeMini_LoadFreeBIOS(void)
{
	PokeMini_FreeBIOS = 1;
	memcpy(PM_BIOS, FreeBIOS, 4096);
	return 1;
}

// New MIN ROM
#ifndef TARGET_GNW
int PokeMini_NewMIN(uint32_t size)
{
	// Allocate ROM and set cartridge size
	if (PM_ROM_Alloc) {
		free(PM_ROM);
		PM_ROM = NULL;
		PM_ROM_Alloc = 0;
	}
	PM_ROM_Mask = GetMultiple2Mask(size);
	PM_ROM_Size = PM_ROM_Mask + 1;
	PM_ROM = (uint8_t *)malloc(PM_ROM_Size);
	if (!PM_ROM)
		return 0;
	memset(PM_ROM, 0xFF, PM_ROM_Size);
	PM_ROM_Alloc = 1;

	return 1;
}
#endif

/* Free color information */
void PokeMini_FreeColorInfo(void)
{
	if (PRCColorMap)
	{
		free(PRCColorMap);
		PRCColorMap = NULL;
	}
}

/* Synchronize host time */
int PokeMini_SyncHostTime(void)
{
#ifndef NO_RTC
	/* Modify EEPROM for host time */
	if (CommandLine.updatertc == 2)
	{
		time_t tim = time(NULL);
		struct tm *now = localtime(&tim);
		MinxIO_SetTimeStamp(now->tm_year % 100, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);
		PMR_SEC_CTRL = 0x01;
		MinxTimers.SecTimerCnt = 0;
		return 1;
	}
#endif
	return 0;
}

// Load EEPROM
int PokeMini_LoadEEPROMFile(const char *filename)
{
	int64_t readbytes;
	// Open file
#ifndef TARGET_GNW
	RFILE *fi = filestream_open(filename,
			RETRO_VFS_FILE_ACCESS_READ,
			RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!fi)
		return 0;

	// Read content
	readbytes = filestream_read(fi, EEPROM, 8192);
	filestream_close(fi);
#else
	FILE *fi = fopen(filename, "rb");
	if (!fi)
		return 0;
	readbytes = fread(EEPROM, 1, 8192, fi);
	fclose(fi);
#endif

	return (readbytes == 8192);
}

// Save EEPROM
int PokeMini_SaveEEPROMFile(const char *filename)
{
	int64_t writebytes;
	// Open file
#ifndef TARGET_GNW
	RFILE *fo = filestream_open(filename,
			RETRO_VFS_FILE_ACCESS_WRITE,
			RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!fo)
		return 0;

	// Read content
	writebytes = filestream_write(fo, EEPROM, 8192);
	filestream_close(fo);
#else
	FILE *fo = fopen(filename, "wb");
	if (!fo)
		return 0;
	writebytes = fwrite(EEPROM, 1, 8192, fo);
	fclose(fo);
#endif

	return (writebytes == 8192);
}

#ifndef TARGET_GNW
// Load emulator state from memory stream
int PokeMini_LoadSSStream(uint8_t *buffer, uint64_t size)
{
	memstream_t *stream;
	int readbytes;
	char PMiniStr[PMTMPV];
	uint32_t PMiniID, StatTime, BSize;

	// Open memory stream
	memstream_set_buffer(buffer, size);
	stream = memstream_open(0);
	if (stream == NULL)
		return 0;

	// Read content
	PMiniStr[12] = 0;
	readbytes = memstream_read(stream, PMiniStr, 12);	// Read File ID
	if ((readbytes != 12) || strcmp(PMiniStr, "PokeMiniStat")) {
		memstream_close(stream);
		memstream_set_buffer(NULL, 0);
		return 0;
	}
	readbytes = memstream_read(stream, &PMiniID, 4);	// Read State ID
	if ((readbytes != 4) || (PMiniID != PokeMini_ID)) {
		memstream_close(stream);
		memstream_set_buffer(NULL, 0);
		return 0;
	}
	readbytes = memstream_read(stream, &StatTime, 4);	// Read Time
	if (readbytes != 4) {
		memstream_close(stream);
		memstream_set_buffer(NULL, 0);
		return 0;
	}

	// Read State Structure
	PMiniStr[4] = 0;
	while (memstream_pos(stream) < size) {
		readbytes = memstream_read(stream, PMiniStr, 4);
		if (readbytes != 4) {
			memstream_close(stream);
			memstream_set_buffer(NULL, 0);
			return 0;
		}
		readbytes = memstream_read(stream, &BSize, 4);
		if (readbytes != 4) {
			memstream_close(stream);
			memstream_set_buffer(NULL, 0);
			return 0;
		}
		if (!strcmp(PMiniStr, "RAM-")) {		// RAM
			readbytes = memstream_read(stream, PM_RAM, 0x1000);
			if ((BSize != 0x1000) || (readbytes != 0x1000)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "REG-")) {		// Register I/O
			readbytes = memstream_read(stream, PM_IO, 256);
			if ((BSize != 256) || (readbytes != 256)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "CPU-")) {		// CPU
			if (!MinxCPU_LoadStateStream(stream, BSize)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "IRQ-")) {		// IRQ
			if (!MinxIRQ_LoadStateStream(stream, BSize)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "TMR-")) {		// Timers
			if (!MinxTimers_LoadStateStream(stream, BSize)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "PIO-")) {		// Parallel IO
			if (!MinxIO_LoadStateStream(stream, BSize)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "PRC-")) {		// PRC
			if (!MinxPRC_LoadStateStream(stream, BSize)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "CPM-")) {		// Color PRC
			if (!MinxColorPRC_LoadStateStream(stream, BSize)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "LCD-")) {		// LCD
			if (!MinxLCD_LoadStateStream(stream, BSize)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "AUD-")) {		// Audio
			if (!MinxAudio_LoadStateStream(stream, BSize)) {
				memstream_close(stream);
				memstream_set_buffer(NULL, 0);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "END-")) {
			break;
		}
	}
	memstream_close(stream);
	memstream_set_buffer(NULL, 0);

	// Update RTC if requested
	if (CommandLine.updatertc == 1)
		MinxTimers.SecTimerCnt += (uint32_t)time(NULL) - StatTime;

	// Synchronize with host time
	PokeMini_SyncHostTime();

	return 1;
}
#else
// Load emulator state from memory stream
int PokeMini_LoadSSStream(const char *filename, uint64_t size)
{
	FILE *file;
	int readbytes;
	char PMiniStr[PMTMPV];
	uint32_t PMiniID, StatTime, BSize;

	// Open memory stream
	file = fopen(filename, "rb");
	if (file == NULL)
		return 0;

	// Read content
	PMiniStr[12] = 0;
	readbytes = fread(PMiniStr, 1, 12, file);	// Read File ID
	if ((readbytes != 12) || strcmp(PMiniStr, "PokeMiniStat")) {
		fclose(file);
		return 0;
	}
	readbytes = fread(&PMiniID, 1, 4, file);	// Read State ID
	if ((readbytes != 4) || (PMiniID != PokeMini_ID)) {
		fclose(file);
		return 0;
	}
	readbytes = fread(&StatTime, 1, 4, file);	// Read Time
	if (readbytes != 4) {
		fclose(file);
		return 0;
	}

	// Read State Structure
	PMiniStr[4] = 0;
	while (1) {
		readbytes = fread(PMiniStr, 1, 4, file);
		if (readbytes != 4) {
			fclose(file);
			return 0;
		}
		readbytes = fread(&BSize, 1, 4, file);
		if (readbytes != 4) {
			fclose(file);
			return 0;
		}
		if (!strcmp(PMiniStr, "RAM-")) {		// RAM
			readbytes = fread(PM_RAM, 1, 0x1000, file);
			if ((BSize != 0x1000) || (readbytes != 0x1000)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "REG-")) {		// Register I/O
			readbytes = fread(PM_IO, 1, 256, file);
			if ((BSize != 256) || (readbytes != 256)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "CPU-")) {		// CPU
			if (!MinxCPU_LoadStateStream(file, BSize)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "IRQ-")) {		// IRQ
			if (!MinxIRQ_LoadStateStream(file, BSize)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "TMR-")) {		// Timers
			if (!MinxTimers_LoadStateStream(file, BSize)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "PIO-")) {		// Parallel IO
			if (!MinxIO_LoadStateStream(file, BSize)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "PRC-")) {		// PRC
			if (!MinxPRC_LoadStateStream(file, BSize)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "CPM-")) {		// Color PRC
			if (!MinxColorPRC_LoadStateStream(file, BSize)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "LCD-")) {		// LCD
			if (!MinxLCD_LoadStateStream(file, BSize)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "AUD-")) {		// Audio
			if (!MinxAudio_LoadStateStream(file, BSize)) {
				fclose(file);
				return 0;
			}
		} else if (!strcmp(PMiniStr, "END-")) {
			break;
		}
	}
	fclose(file);

	// Update RTC if requested
	if (CommandLine.updatertc == 1)
		MinxTimers.SecTimerCnt += (uint32_t)time(NULL) - StatTime;

	// Synchronize with host time
	PokeMini_SyncHostTime();

	return 1;
}
#endif

#ifndef TARGET_GNW
/* Save emulator state to memory stream */
int PokeMini_SaveSSStream(uint8_t *buffer, uint64_t size)
{
	/* TODO: Error check bytes written */
	memstream_t *stream;
	uint32_t PMiniID, StatTime, BSize;

	// Open memory stream
	memstream_set_buffer(buffer, size);
	stream = memstream_open(1);
	if (stream == NULL)
		return 0;

	// Write content
	memstream_write(stream, (void *)"PokeMiniStat", 12);	// Write File ID
	PMiniID = PokeMini_ID;
	memstream_write(stream, &PMiniID, 4);	// Write State ID
	StatTime = (uint32_t)time(NULL);
	memstream_write(stream, &StatTime, 4);	// Write Time

	// Read State Structure
	// - RAM
	memstream_write(stream, (void *)"RAM-", 4);
	BSize = 0x1000;
	memstream_write(stream, &BSize, 4);
	memstream_write(stream, PM_RAM, 0x1000);
	// - Registers I/O
	memstream_write(stream, (void *)"REG-", 4);
	BSize = 256;
	memstream_write(stream, &BSize, 4);
	memstream_write(stream, PM_IO, 256);
	// - CPU Interface
	memstream_write(stream, (void *)"CPU-", 4);
	MinxCPU_SaveStateStream(stream);
	// - IRQ Interface
	memstream_write(stream, (void *)"IRQ-", 4);
	MinxIRQ_SaveStateStream(stream);
	// - Timers Interface
	memstream_write(stream, (void *)"TMR-", 4);
	MinxTimers_SaveStateStream(stream);
	// - Parallel IO Interface
	memstream_write(stream, (void *)"PIO-", 4);
	MinxIO_SaveStateStream(stream);
	// - PRC Interface
	memstream_write(stream, (void *)"PRC-", 4);
	MinxPRC_SaveStateStream(stream);
	// - Color PRC Interface
	memstream_write(stream, (void *)"CPM-", 4);
	MinxColorPRC_SaveStateStream(stream);
	// - LCD Interface
	memstream_write(stream, (void *)"LCD-", 4);
	MinxLCD_SaveStateStream(stream);
	// - Audio Interface
	memstream_write(stream, (void *)"AUD-", 4);
	MinxAudio_SaveStateStream(stream);
	// - EOF
	memstream_write(stream, (void *)"END-", 4);
	BSize = 0;
	memstream_write(stream, &BSize, 4);
	memstream_close(stream);
	memstream_set_buffer(NULL, 0);

	return 1;
}
#else
/* Save emulator state to memory stream */
int PokeMini_SaveSSStream(const char *filename, uint64_t size)
{
	/* TODO: Error check bytes written */
	FILE *file;
	uint32_t PMiniID, StatTime, BSize;

	// Open memory stream
	file = fopen(filename, "wb");
	if (file == NULL)
		return 0;

	// Write content
	fwrite("PokeMiniStat", 1, 12, file);	// Write File ID
	PMiniID = PokeMini_ID;
	fwrite(&PMiniID, 1, 4, file);	// Write State ID
	StatTime = (uint32_t)time(NULL);
	fwrite(&StatTime, 1, 4, file);	// Write Time

	// Read State Structure
	// - RAM
	fwrite("RAM-", 1, 4, file);
	BSize = 0x1000;
	fwrite(&BSize, 1, 4, file);
	fwrite(PM_RAM, 1, 0x1000, file);
	// - Registers I/O
	fwrite("REG-", 1, 4, file);
	BSize = 256;
	fwrite(&BSize, 1, 4, file);
	fwrite(PM_IO, 1, 256, file);
	// - CPU Interface
	fwrite("CPU-", 1, 4, file);
	MinxCPU_SaveStateStream(file);
	// - IRQ Interface
	fwrite("IRQ-", 1, 4, file);
	MinxIRQ_SaveStateStream(file);
	// - Timers Interface
	fwrite("TMR-", 1, 4, file);
	MinxTimers_SaveStateStream(file);
	// - Parallel IO Interface
	fwrite("PIO-", 1, 4, file);
	MinxIO_SaveStateStream(file);
	// - PRC Interface
	fwrite("PRC-", 1, 4, file);
	MinxPRC_SaveStateStream(file);
	// - Color PRC Interface
	fwrite("CPM-", 1, 4, file);
	MinxColorPRC_SaveStateStream(file);
	// - LCD Interface
	fwrite("LCD-", 1, 4, file);
	MinxLCD_SaveStateStream(file);
	// - Audio Interface
	fwrite("AUD-", 1, 4, file);
	MinxAudio_SaveStateStream(file);
	// - EOF
	fwrite("END-", 1, 4, file);
	BSize = 0;
	fwrite(&BSize, 1, 4, file);
	fclose(file);

	return 1;
}
#endif

// Reset emulation
void PokeMini_Reset(int hardreset)
{
	// Reset IO
	if (hardreset)
	{
		memset(PM_RAM, 0xFF, 8192);
		memcpy(PM_IO, PM_IO_INIT, 256);
	}

	// Reset all components
	MinxTimers_Reset(hardreset);
	MinxIRQ_Reset(hardreset);
	MinxIO_Reset(hardreset);
	MinxPRC_Reset(hardreset);
	MinxColorPRC_Reset(hardreset);
	MinxLCD_Reset(hardreset);
	MinxAudio_Reset(hardreset);
	MinxCPU_Reset(hardreset);

	// Change BIOS
#ifndef TARGET_GNW
	if (!PokeMini_FreeBIOS && CommandLine.forcefreebios)
		PokeMini_LoadFreeBIOS();
	if (PokeMini_FreeBIOS && !CommandLine.forcefreebios)
	{
		PokeMini_LoadFreeBIOS();
		if (StringIsSet(CommandLine.bios_file))
			if (FileExist(CommandLine.bios_file))
				PokeMini_LoadBIOSFile(CommandLine.bios_file);
	}
#else
	PokeMini_LoadFreeBIOS();
	if (rg_storage_exists(CommandLine.bios_file))
		PokeMini_LoadBIOSFile(CommandLine.bios_file);
#endif

	// Synchronize with host time
	PokeMini_SyncHostTime();

#ifndef PERFORMANCE
	// Set multicart type
	SetMulticart(CommandLine.multicart);
#endif
}

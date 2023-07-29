/*****************************************************************************
 * This file is a part of ExtIoOverNet software.
 * 
 * Copyright (C) 2023 Roman Ukhov. All rights reserved.
 * 
 * Licensed under the GNU General Public License Version 3 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at 
 * https://www.gnu.org/licenses/gpl-3.0.txt
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: 2023 Roman Ukhov <ukhov.roman@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *****************************************************************************/

#include "../ExtIO_API/LC_ExtIO_Types.h"
#include "../utils/log.h"
#include "service.h"
#include "dll_main.h"

boost::log::sources::logger_mt& log()
{
	static boost::log::sources::logger_mt _log;
	return _log;
}

#define UNIMPLEMENTED() \
	LOG(warning) << "! > Unimplemented: " << __FUNCDNAME__;

// #1
extern "C" bool BOOST_SYMBOL_EXPORT __stdcall InitHW(char* name, char* model, int& type)
{
	strcpy_s(name, 63, "ExtIoOverNet");
	strcpy_s(model, 15, "ExtIoOverNet");
	name[63] = 0;
	model[15] = 0;

	type = exthwUSBdata16;

	DllInit();

	BOOST_LOG_FUNCTION();
	
	auto srv = GetLockedService();
	if (!srv)
		return false;

	return srv->InitHW(name, model, type);
}

//---------------------------------------------------------------------------
extern "C"
bool BOOST_SYMBOL_EXPORT WINAPI OpenHW(void)
{
	auto srv = GetLockedService();
	if (!srv)
		return false;

	return srv->OpenHW();
}

//---------------------------------------------------------------------------
extern "C"
int  BOOST_SYMBOL_EXPORT WINAPI StartHW(long LOfreq)
{
	auto srv = GetLockedService();
	if (!srv)
		return -1;
	return srv->StartHW(LOfreq);
}

//---------------------------------------------------------------------------
extern "C"
int64_t BOOST_SYMBOL_EXPORT WINAPI StartHW64(int64_t LOfreq)
{
	UNIMPLEMENTED();
	return -1;
}

//---------------------------------------------------------------------------
extern "C"
void BOOST_SYMBOL_EXPORT WINAPI StopHW(void)
{
	auto srv = GetLockedService();
	if (!srv) return;
	return srv->StopHW();
}

//---------------------------------------------------------------------------
extern "C"
void BOOST_SYMBOL_EXPORT WINAPI CloseHW(void)
{
	DllDeinit();
}

//---------------------------------------------------------------------------
extern "C"
int  BOOST_SYMBOL_EXPORT WINAPI SetHWLO(long LOfreq)
{
	auto srv = GetLockedService();
	if (!srv)
		return -1;
	return srv->SetHWLO(LOfreq);
}

extern "C"
int64_t BOOST_SYMBOL_EXPORT WINAPI SetHWLO64(int64_t LOfreq)
{
	auto srv = GetLockedService();
	if (!srv)
		return -1;
	return srv->SetHWLO64(LOfreq);
}

//---------------------------------------------------------------------------
extern "C"
int  BOOST_SYMBOL_EXPORT WINAPI GetStatus(void)
{
	UNIMPLEMENTED();
	return 0;
}


// #2
extern "C" void BOOST_SYMBOL_EXPORT WINAPI SetCallback(pfnExtIOCallback funcptr)
{
	auto srv = GetLockedService();
	if (!srv)
		return;

	srv->SetCallback(funcptr);
}


//---------------------------------------------------------------------------
extern "C"
long BOOST_SYMBOL_EXPORT WINAPI GetHWLO(void)
{
	UNIMPLEMENTED();
	return -1;
}


extern "C"
int64_t BOOST_SYMBOL_EXPORT WINAPI GetHWLO64(void)
{
	UNIMPLEMENTED();
	return -1;
}

//---------------------------------------------------------------------------
extern "C"
long BOOST_SYMBOL_EXPORT WINAPI GetHWSR(void)
{
	auto srv = GetLockedService();
	if (!srv)
		return -1;
	return srv->GetHWSR();
}


//---------------------------------------------------------------------------

// extern "C" long BOOST_SYMBOL_EXPORT WINAPI GetTune(void);
// extern "C" void BOOST_SYMBOL_EXPORT WINAPI GetFilters(int& loCut, int& hiCut, int& pitch);
// extern "C" char BOOST_SYMBOL_EXPORT WINAPI GetMode(void);
// extern "C" void BOOST_SYMBOL_EXPORT WINAPI ModeChanged(char mode);
// extern "C" void BOOST_SYMBOL_EXPORT WINAPI IFLimitsChanged(long low, long high);
// extern "C" void BOOST_SYMBOL_EXPORT WINAPI TuneChanged(long freq);

// extern "C" void    BOOST_SYMBOL_EXPORT WINAPI TuneChanged64(int64_t freq);
// extern "C" int64_t BOOST_SYMBOL_EXPORT WINAPI GetTune64(void);
// extern "C" void    BOOST_SYMBOL_EXPORT WINAPI IFLimitsChanged64(int64_t low, int64_t high);

//---------------------------------------------------------------------------

// extern "C" void BOOST_SYMBOL_EXPORT WINAPI RawDataReady(long samprate, int *Ldata, int *Rdata, int numsamples)

//---------------------------------------------------------------------------
extern "C"
void BOOST_SYMBOL_EXPORT WINAPI VersionInfo(const char* progname, int ver_major, int ver_minor)
{
	auto srv = GetLockedService();
	if (!srv)
		return;
	return srv->VersionInfo(progname, ver_major, ver_minor);
}

//---------------------------------------------------------------------------

// following "Attenuator"s visible on "RF" button

extern "C" int BOOST_SYMBOL_EXPORT WINAPI GetAttenuators(int atten_idx, float* attenuation)
{
	auto srv = GetLockedService();
	if (!srv)
		return -1;
	return srv->GetAttenuators(atten_idx, attenuation);
}

extern "C"
int BOOST_SYMBOL_EXPORT WINAPI GetActualAttIdx(void)
{
	auto srv = GetLockedService();
	if (!srv)
		return -1;
	return srv->GetActualAttIdx();
}

extern "C"
int BOOST_SYMBOL_EXPORT WINAPI SetAttenuator(int atten_idx)
{
	UNIMPLEMENTED();
	return -1;
}


//---------------------------------------------------------------------------

// optional function to get AGC Mode: AGC_OFF (always agc_index = 0), AGC_SLOW, AGC_MEDIUM, AGC_FAST, ...
// this functions is called with incrementing idx
//    - until this functions returns != 0, which means that all agc modes are already delivered

extern "C"
int BOOST_SYMBOL_EXPORT WINAPI ExtIoGetAGCs(int agc_idx, char* text)	// text limited to max 16 char
{
	auto srv = GetLockedService();
	if (!srv)
		return 0;
	return srv->ExtIoGetAGCs(agc_idx, text);
}


extern "C"
int BOOST_SYMBOL_EXPORT WINAPI ExtIoGetActualAGCidx(void)
{
	auto srv = GetLockedService();
	if (!srv)
		return 0;
	return srv->ExtIoGetActualAGCidx();
}

extern "C"
int BOOST_SYMBOL_EXPORT WINAPI ExtIoSetAGC(int agc_idx)
{
	UNIMPLEMENTED();
	return -1;
}

// optional: HDSDR >= 2.62
extern "C"
int BOOST_SYMBOL_EXPORT WINAPI ExtIoShowMGC(int agc_idx)		// return 1, to continue showing MGC slider on AGC
// return 0, is default for not showing MGC slider
{
	auto srv = GetLockedService();
	if (!srv)
		return 0;
	return srv->ExtIoShowMGC(agc_idx);
}

//---------------------------------------------------------------------------

// following "MGC"s visible on "IF" button

extern "C"
int BOOST_SYMBOL_EXPORT WINAPI ExtIoGetMGCs(int mgc_idx, float* gain)
{
	auto srv = GetLockedService();
	if (!srv)
		return 0;
	return srv->ExtIoGetMGCs(mgc_idx, gain);
}

extern "C"
int BOOST_SYMBOL_EXPORT WINAPI ExtIoGetActualMgcIdx(void)
{
	auto srv = GetLockedService();
	if (!srv)
		return 0;
	return srv->ExtIoGetActualMgcIdx();
}

extern "C"
int BOOST_SYMBOL_EXPORT WINAPI ExtIoSetMGC(int mgc_idx)
{
	UNIMPLEMENTED();
	return -1;
}

//---------------------------------------------------------------------------

extern "C"
int BOOST_SYMBOL_EXPORT WINAPI ExtIoGetSrates(int srate_idx, double* samplerate)
{
	auto srv = GetLockedService();
	if (!srv)
		return 0;
	return srv->ExtIoGetSrates(srate_idx, samplerate);
}

extern "C"
int  BOOST_SYMBOL_EXPORT WINAPI ExtIoGetActualSrateIdx(void)
{
	auto srv = GetLockedService();
	if (!srv)
		return 0;
	return srv->ExtIoGetActualSrateIdx();
}

extern "C"
int  BOOST_SYMBOL_EXPORT WINAPI ExtIoSetSrate(int srate_idx)
{
	auto srv = GetLockedService();
	if (!srv)
		return 0;
	return srv->ExtIoSetSrate(srate_idx);
}

extern "C"
long BOOST_SYMBOL_EXPORT WINAPI ExtIoGetBandwidth(int srate_idx)
{
	auto srv = GetLockedService();
	if (!srv)
		return 0;
	return srv->ExtIoGetBandwidth(srate_idx);
}

//---------------------------------------------------------------------------

extern "C"
int  BOOST_SYMBOL_EXPORT WINAPI ExtIoGetSetting(int idx, char* description, char* value)
{
	UNIMPLEMENTED();
	return -1;
}


extern "C"
void BOOST_SYMBOL_EXPORT WINAPI ExtIoSetSetting(int idx, const char* value)
{
	UNIMPLEMENTED();
}

extern "C"
void BOOST_SYMBOL_EXPORT WINAPI ShowGUI(void)
{
	auto srv = GetLockedService();
	if (!srv) return;
	return srv->ShowGUI();
}

extern "C"
void BOOST_SYMBOL_EXPORT WINAPI HideGUI(void)
{
	auto srv = GetLockedService();
	if (!srv) return;
	return srv->HideGUI();
}

extern "C"
void BOOST_SYMBOL_EXPORT WINAPI SwitchGUI(void)
{
	auto srv = GetLockedService();
	if (!srv) return;
	return srv->SwitchGUI();
}

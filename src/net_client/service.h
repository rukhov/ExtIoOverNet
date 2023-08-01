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

#pragma once

#include "options.h"
#include "../ExtIO_API/LC_ExtIO_Types.h"

class IExtIO_API
{
public:
    virtual ~IExtIO_API() = default;

    // All calls from other thread
    // Blocking calls
    virtual bool InitHW(char* name, char* model, int& type) = 0;
    virtual bool OpenHW() = 0;
    virtual int SetHWLO(long LOfreq) = 0;
    virtual int64_t SetHWLO64(int64_t LOfreq) = 0;
    virtual long GetHWSR() = 0;
    virtual int StartHW(long extLOfreq) = 0;
    virtual void StopHW(void) = 0;
    virtual void SetCallback(pfnExtIOCallback funcptr) = 0;
    virtual void VersionInfo(const char* progname, int ver_major, int ver_minor) = 0;
    virtual int GetAttenuators(int atten_idx, float* attenuation) = 0;
    virtual int GetActualAttIdx(void) = 0;
    virtual int ExtIoShowMGC(int agc_idx) = 0;
    virtual void ShowGUI(void) = 0;
    virtual void HideGUI(void) = 0;
    virtual void SwitchGUI(void) = 0;
    virtual int ExtIoGetAGCs(int agc_idx, char* text) = 0;
    virtual int ExtIoGetActualAGCidx(void) = 0;
    virtual int ExtIoGetMGCs(int mgc_idx, float* gain) = 0;
    virtual int ExtIoGetActualMgcIdx(void) = 0;
    virtual int ExtIoGetSrates(int srate_idx, double* samplerate) = 0;
    virtual int ExtIoGetActualSrateIdx(void) = 0;
    virtual int ExtIoSetSrate(int srate_idx) = 0;
    virtual long ExtIoGetBandwidth(int srate_idx) = 0;
};

class IService: public IExtIO_API
{
public:
    virtual ~IService() = default;

    // All calls from other thread
    // Blocking calls

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsConnected() const = 0;
};


std::shared_ptr<IService> GetLockedService();

std::future<std::weak_ptr<IService>> MakeService(
    boost::asio::strand<boost::asio::io_context::executor_type>&& strand,
    const Options& opts);

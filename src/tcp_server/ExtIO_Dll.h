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

#include "../ExtIO_API/LC_ExtIO_Types.h"
#include <boost/dll/import.hpp> 

class ExtIO_Dll final
{
    boost::dll::shared_library lib;

    template<typename FbT>
    std::function<FbT> GetFn(const std::string& fnName)
    {
        try{
            if(lib.has(fnName))
                return lib.get<FbT>(fnName);
        }
        catch (...)
        {
        }
        return {};
    }

public:

    ~ExtIO_Dll();

    bool load(const std::string&);
    void unload();

    using fnInitHW = bool __stdcall (char* name, char* model, int& hwtype);
    std::function<fnInitHW>         InitHW;
    using fnOpenHW = bool __stdcall (void);
    std::function<fnOpenHW>         OpenHW;
    using fnCloseHW = void __stdcall (void);
    std::function<fnCloseHW>        CloseHW;
    using fnStartHW = int __stdcall (long extLOfreq);
    std::function<fnStartHW>        StartHW;
    using fnStopHW = void __stdcall (void);
    std::function<fnStopHW>         StopHW;
    using fnSetCallback = void __stdcall (pfnExtIOCallback funcptr);
    std::function<fnSetCallback>    SetCallback;
    using fnSetHWLO = int __stdcall (long extLOfreq);
    std::function<fnSetHWLO>        SetHWLO;
    using fnSetHWLO64 = int64_t __stdcall (int64_t extLOfreq);
    std::function<fnSetHWLO64>      SetHWLO64;
    using fnGetHWSR = long __stdcall (void);
    std::function<fnGetHWSR>        GetHWSR;
    using fnGetAttenuators = int __stdcall (int idx, float* attenuation);
    std::function<fnGetAttenuators> GetAttenuators;
    using fnGetActualAttIdx = int __stdcall (void);
    std::function<fnGetActualAttIdx> GetActualAttIdx;
    using fnExtIoShowMGC = int __stdcall (int agc_idx);
    std::function<fnExtIoShowMGC>   ExtIoShowMGC;
    using fnExtIoGetAGCs = int __stdcall(int agc_idx, char* text);
    std::function<fnExtIoGetAGCs>   ExtIoGetAGCs;
    using fnExtIoGetActualAGCidx = int __stdcall(void);
    std::function<fnExtIoGetActualAGCidx> ExtIoGetActualAGCidx;
    using fnExtIoGetMGCs = int __stdcall (int mgc_idx, float* gain);
    std::function<fnExtIoGetMGCs>   ExtIoGetMGCs;
    using fnExtIoGetActualMgcIdx = int __stdcall(void);
    std::function<fnExtIoGetActualMgcIdx> ExtIoGetActualMgcIdx;
    using fnExtIoGetSrates = int __stdcall(int srate_idx, double* samplerate);
    std::function<fnExtIoGetSrates> ExtIoGetSrates;
    using fnExtIoGetActualSrateIdx = int __stdcall(void);
    std::function<fnExtIoGetActualSrateIdx> ExtIoGetActualSrateIdx;
    using fnExtIoSetSrate = int __stdcall(int srate_idx);
    std::function<fnExtIoSetSrate>  ExtIoSetSrate;
    using fnExtIoGetBandwidth = int __stdcall(int srate_idx);
    std::function<fnExtIoGetBandwidth> ExtIoGetBandwidth;

    using fnShowGUI = void __stdcall(void);
    std::function<fnShowGUI>        ShowGUI;
    using fnHideGUI = void __stdcall(void);
    std::function<fnHideGUI>        HideGUI;
    using fnSwitchGUI = void __stdcall (void);
    std::function<fnSwitchGUI>      SwitchGUI;


    using fnExtIoSDRInfo = void __stdcall (int extSDRInfo, int additionalValue, void* additionalPtr);
    std::function<fnExtIoSDRInfo>   ExtIoSDRInfo;
    
    using fnVersionInfo = void __stdcall (const char* progname, int ver_major, int ver_minor);
    std::function<fnVersionInfo>    VersionInfo;

};

std::unique_ptr<ExtIO_Dll> LoadLibrary(const std::string& libraryName);
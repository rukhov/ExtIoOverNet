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


#include "ExtIO_Dll.h"

namespace
{

}

ExtIO_Dll::~ExtIO_Dll()
{
    if (lib.is_loaded()) lib.unload();
}

bool ExtIO_Dll::load(const std::string& libraryPath)
{
    boost::system::error_code ec;

    lib.load(libraryPath, ec, boost::dll::load_mode::default_mode);

    if (ec.failed() || !lib.is_loaded())
        return false;

    InitHW =        GetFn<ExtIO_Dll::fnInitHW>("InitHW");
    OpenHW =        GetFn<ExtIO_Dll::fnOpenHW>("OpenHW");
    CloseHW =       GetFn<ExtIO_Dll::fnCloseHW>("CloseHW");
    StartHW =       GetFn<ExtIO_Dll::fnStartHW>("StartHW");
    StopHW =        GetFn<ExtIO_Dll::fnStopHW>("StopHW");
    SetCallback =   GetFn<ExtIO_Dll::fnSetCallback>("SetCallback");
    SetHWLO =       GetFn<ExtIO_Dll::fnSetHWLO>("SetHWLO");
    SetHWLO64 =     GetFn<ExtIO_Dll::fnSetHWLO64>("SetHWLO64");
    GetHWSR =       GetFn<ExtIO_Dll::fnGetHWSR>("GetHWSR");
    GetAttenuators =GetFn<ExtIO_Dll::fnGetAttenuators>("GetAttenuators");
    GetActualAttIdx=GetFn<ExtIO_Dll::fnGetActualAttIdx>("GetActualAttIdx");
    ExtIoShowMGC =  GetFn<ExtIO_Dll::fnExtIoShowMGC>("ExtIoShowMGC");
    ExtIoGetAGCs =  GetFn<ExtIO_Dll::fnExtIoGetAGCs>("ExtIoGetAGCs");
    ExtIoGetActualAGCidx = GetFn<ExtIO_Dll::fnExtIoGetActualAGCidx>("ExtIoGetActualAGCidx");
    ExtIoGetMGCs =  GetFn<ExtIO_Dll::fnExtIoGetMGCs>("ExtIoGetMGCs");
    ExtIoGetActualMgcIdx = GetFn<ExtIO_Dll::fnExtIoGetActualMgcIdx>("ExtIoGetActualMgcIdx");
    ExtIoGetSrates= GetFn<ExtIO_Dll::fnExtIoGetSrates>("ExtIoGetSrates");
    ExtIoGetActualSrateIdx = GetFn<ExtIO_Dll::fnExtIoGetActualSrateIdx>("ExtIoGetActualSrateIdx");
    ExtIoSetSrate = GetFn<ExtIO_Dll::fnExtIoSetSrate>("ExtIoSetSrate");
    ExtIoGetBandwidth = GetFn<ExtIO_Dll::fnExtIoGetBandwidth>("ExtIoGetBandwidth");

    ShowGUI =       GetFn<ExtIO_Dll::fnShowGUI>("ShowGUI");
    HideGUI =       GetFn<ExtIO_Dll::fnHideGUI>("HideGUI");
    SwitchGUI =     GetFn<ExtIO_Dll::fnSwitchGUI>("SwitchGUI");

    ExtIoSDRInfo =  GetFn<fnExtIoSDRInfo>("ExtIoSDRInfo");

    VersionInfo =   GetFn<ExtIO_Dll::fnVersionInfo>("VersionInfo");

    return true;
}

void ExtIO_Dll::unload()
{

}

std::unique_ptr<ExtIO_Dll> LoadLibrary(const std::string& libraryName)
{

    auto p = std::make_unique<ExtIO_Dll>();

    if (!p->load(libraryName))
        return {};

    return p;
}
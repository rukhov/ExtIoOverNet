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

#include "Protocol.pb.h"

namespace Protocol
{
    constexpr uint32_t c_magicNum = 4378;

    inline ExtIO_TCP_Proto::Message Make_Hello_Msg(
        uint64_t versionNumber,
        const std::string& clientVersionName)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsHello hello;
        ExtIO_TCP_Proto::ProtocolVersion version;
        version.set_version_number(versionNumber);
        version.set_client_version_name(clientVersionName);
        *hello.mutable_version() = version;
        *msg.mutable_hello() = hello;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_InitHW_Msg(
        const std::optional<bool>& result,
        const std::optional<std::string>& name, 
        const std::optional<std::string>& model, 
        const std::optional<int>& type)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsInitHW inithw;
        if(result.has_value()) inithw.set_result(*result);
        if(name.has_value()) inithw.set_name(*name);
        if(model.has_value()) inithw.set_model(*model);
        if(type.has_value()) inithw.set_type(*type);
        *msg.mutable_inithw() = inithw;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_Error_Msg(
        ExtIO_TCP_Proto::ErrorCode err)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsError error;
        error.set_error(err);
        *msg.mutable_error() = error;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_LoadExtIOApi_Msg(
        std::optional<ExtIO_TCP_Proto::ErrorCode> err)
    {
        ExtIO_TCP_Proto::Message msg;
        if(err.has_value()) msg.set_resultcode(*err);
        ExtIO_TCP_Proto::RqsLoadExtIOApi LoadAPI;
        *msg.mutable_loadextioapi() = LoadAPI;
        return msg;
    }

    inline std::unique_ptr<ExtIO_TCP_Proto::Message> Make_ExtIOCallback_Msg(
        int cnt, 
        int status, 
        float IQoffs, 
        void* IQdata,
        size_t SampleSize)
    {
        auto msg = std::make_unique<ExtIO_TCP_Proto::Message>();
        ExtIO_TCP_Proto::RqsExtIOCallback ExtIOCallback;
        auto& data = *msg->mutable_extiocallback();
        data.set_status(status);
        data.set_iqoffs(IQoffs);
        data.set_cnt(cnt);
        if (cnt > 0) {
            auto& str = *data.mutable_iqdata();
            str.append(
                reinterpret_cast<const char*>(IQdata), 
                static_cast<size_t>(cnt)*SampleSize);
            auto s = str.size();
        }
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_OpenHW_Msg(const std::optional<bool>& result)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsOpenHW OpenHW;
        if (result.has_value())
            OpenHW.set_result(*result);
        *msg.mutable_openhw() = OpenHW;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_SetHWLO_Msg(
        const std::optional<int32_t>& result,
        const std::optional<int32_t>& LOfreq)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsSetHWLO SetHWLO;
        if (result.has_value()) SetHWLO.set_result(*result);
        if (LOfreq.has_value()) SetHWLO.set_lofreq(*LOfreq);
        *msg.mutable_sethwlo() = SetHWLO;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_SetHWLO64_Msg(
        const std::optional<int64_t>& result,
        const std::optional<int64_t>& LOfreq)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsSetHWLO64 SetHWLO;
        if (result.has_value()) SetHWLO.set_result(*result);
        if (LOfreq.has_value()) SetHWLO.set_lofreq(*LOfreq);
        *msg.mutable_sethwlo64() = SetHWLO;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_GetHWSR_Msg(
        const std::optional<int32_t>& result)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsGetHWSR GetHWSR;
        if (result.has_value()) GetHWSR.set_result(*result);
        *msg.mutable_gethwsr() = GetHWSR;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_StartHW_Msg(
        const std::optional<int32_t>& result,
        const std::optional<int32_t>& extLOfreq)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsStartHW StartHW;
        if (result.has_value()) StartHW.set_result(*result);
        if (extLOfreq.has_value()) StartHW.set_extlofreq(*extLOfreq);
        *msg.mutable_starthw() = StartHW;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_StopHW_Msg(
        std::optional<ExtIO_TCP_Proto::ErrorCode> err)
    {
        ExtIO_TCP_Proto::Message msg;
        if (err.has_value()) msg.set_resultcode(*err);
        ExtIO_TCP_Proto::RqsStopHW StopHW;
        *msg.mutable_stophw() = StopHW;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_VersionInfo_Msg(
        const std::optional<std::string>& progname,
        const std::optional<int32_t>& ver_major,
        const std::optional<int32_t>& ver_minor)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsVersionInfo VersionInfo;
        if (progname.has_value()) VersionInfo.set_progname(*progname);
        if (ver_major.has_value()) VersionInfo.set_ver_major(*ver_major);
        if (ver_minor.has_value()) VersionInfo.set_ver_minor(*ver_minor);
        *msg.mutable_versioninfo() = VersionInfo;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_GetAttenuators_Msg(
        const std::optional<int32_t>& result,
        const std::optional<int32_t>& atten_idx,
        const std::optional<float>& attenuation)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsGetAttenuators thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        if (atten_idx.has_value()) thisMsg.set_atten_idx(*atten_idx);
        if (attenuation.has_value()) thisMsg.set_attenuation(*attenuation);
        *msg.mutable_getattenuators() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_GetActualAttIdx_Msg(
        const std::optional<int32_t>& result)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsGetActualAttIdx thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        *msg.mutable_getactualattidx() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ExtIoShowMGC_Msg(
        const std::optional<int32_t>& result,
        const std::optional<int32_t>& agc_idx)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsExtIoShowMGC thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        if (agc_idx.has_value()) thisMsg.set_agc_idx(*agc_idx);
        *msg.mutable_extioshowmgc() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ShowGUI_Msg()
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsShowGUI thisMsg;
        *msg.mutable_showgui() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_HideGUI_Msg()
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsHideGUI thisMsg;
        *msg.mutable_hidegui() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_SwitchGUI_Msg()
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsSwitchGUI thisMsg;
        *msg.mutable_switchgui() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ExtIoGetAGCs_Msg(
        const std::optional<int32_t>& result,
        const std::optional<int32_t>& agc_idx,
        const std::optional<std::string>& text)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsExtIoGetAGCs thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        if (agc_idx.has_value()) thisMsg.set_agc_idx(*agc_idx);
        if (text.has_value()) thisMsg.set_text(*text);
        *msg.mutable_extiogetagcs() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ExtIoGetActualAGCidx_Msg(
        const std::optional<int32_t>& result)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsExtIoGetActualAGCidx thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        *msg.mutable_extiogetactualagcidx() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ExtIoGetMGCs_Msg(
        const std::optional<int32_t>& result,
        const std::optional<int32_t>& mgc_idx,
        const std::optional<float>& gain)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsExtIoGetMGCs thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        if (mgc_idx.has_value()) thisMsg.set_mgc_idx(*mgc_idx);
        if (gain.has_value()) thisMsg.set_gain(*gain);
        *msg.mutable_extiogetmgcs() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ExtIoGetActualMgcIdx_Msg(
        const std::optional<int32_t>& result)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsExtIoGetActualMgcIdx thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        *msg.mutable_extiogetactualmgcidx() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ExtIoGetSrates_Msg(
        const std::optional<int32_t>& result,
        const std::optional<int32_t>& srate_idx,
        const std::optional<double>& samplerate)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsExtIoGetSrates thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        if (srate_idx.has_value()) thisMsg.set_srate_idx(*srate_idx);
        if (samplerate.has_value()) thisMsg.set_samplerate(*samplerate);
        *msg.mutable_extiogetsrates() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ExtIoGetActualSrateIdx_Msg(
        const std::optional<int32_t>& result)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsExtIoGetActualSrateIdx thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        *msg.mutable_extiogetactualsrateidx() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ExtIoSetSrate_Msg(
        const std::optional<int32_t>& result,
        const std::optional<int32_t>& srate_idx)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsExtIoSetSrate thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        if (srate_idx.has_value()) thisMsg.set_srate_idx(*srate_idx);
        *msg.mutable_extiosetsrate() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_ExtIoGetBandwidth_Msg(
        const std::optional<int32_t>& result,
        const std::optional<int32_t>& srate_idx)
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsExtIoGetBandwidth thisMsg;
        if (result.has_value()) thisMsg.set_result(*result);
        if (srate_idx.has_value()) thisMsg.set_srate_idx(*srate_idx);
        *msg.mutable_extiogetbandwidth() = thisMsg;
        return msg;
    }

    inline ExtIO_TCP_Proto::Message Make_Ping_Msg()
    {
        ExtIO_TCP_Proto::Message msg;
        ExtIO_TCP_Proto::RqsPing thisMsg;
        *msg.mutable_ping() = thisMsg;
        return msg;
    }
}

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

#include "stdafx.h"

#include "session.h"
#include "../utils/Connection.h"
#include "../utils/Protocol.h"
#include "../utils/IsAlive.h"
#include "../utils/log.h"
#include "ExtIO_DLL.h"
#include "options.h"
#include "WindowsMessageLoop.h"

using namespace boost;

namespace
{
    using namespace Protocol;

    template<size_t idx>
    struct ExtIOCallbackNode
    {
        static inline std::mutex mx;
        static inline std::function<int(int, int, float, void*)> fn;
        static int cb(int cnt, int status, float IQoffs, void* IQdata)
        {
            std::lock_guard _(mx);
            if (!fn) return -1;
            return fn(cnt, status, IQoffs, IQdata);
        }
    };

    struct ExtIOCallback
    {
        using ExtIOCallbackFnT = std::function<int(int, int, float, void*)>;

        std::mutex& mx;
        pfnExtIOCallback  cb;
        ExtIOCallbackFnT& fn;
    };

    ExtIOCallback g_extIOCallbacks[] = {
        {ExtIOCallbackNode<0>::mx, ExtIOCallbackNode<0>::cb, ExtIOCallbackNode<0>::fn },
        {ExtIOCallbackNode<1>::mx, ExtIOCallbackNode<1>::cb, ExtIOCallbackNode<1>::fn },
        {ExtIOCallbackNode<2>::mx, ExtIOCallbackNode<2>::cb, ExtIOCallbackNode<2>::fn },
        {ExtIOCallbackNode<3>::mx, ExtIOCallbackNode<3>::cb, ExtIOCallbackNode<3>::fn },
        {ExtIOCallbackNode<4>::mx, ExtIOCallbackNode<4>::cb, ExtIOCallbackNode<4>::fn },
    };

    constexpr size_t g_extIOCallbacksNumber = sizeof(g_extIOCallbacks) / sizeof(g_extIOCallbacks[0]);
    unsigned g_nextExtIOCallbacksIdx = 0;
    std::mutex g_nextExtIOCallbacksIdxMx;

    std::tuple<unsigned, pfnExtIOCallback> SetupExtIOCallback(ExtIOCallback::ExtIOCallbackFnT&& fn)
    {
        std::scoped_lock _(g_nextExtIOCallbacksIdxMx);
        for (size_t i = 0; i < g_extIOCallbacksNumber; ++i) {
            const auto handle = g_nextExtIOCallbacksIdx++;
            if (g_nextExtIOCallbacksIdx == g_extIOCallbacksNumber)
                g_nextExtIOCallbacksIdx = 0;
            std::scoped_lock __(g_extIOCallbacks[handle].mx);
            if (g_extIOCallbacks[handle].fn)
                continue;
            g_extIOCallbacks[handle].fn = std::move(fn);
            return std::make_tuple(handle, g_extIOCallbacks[handle].cb);
        }
        return {};
    }

    void FreeExtIOCallback(unsigned handle)
    {
        assert(handle < g_extIOCallbacksNumber);
        std::scoped_lock __(g_extIOCallbacks[handle].mx);
        assert(g_extIOCallbacks[handle].fn);
        g_extIOCallbacks[handle].fn = {};
    }

    struct exec_ctx
    {
        boost::asio::io_context _ctx;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _work_guard;
        asio::strand<asio::io_context::executor_type> _strand;
        std::vector<std::thread> _pool;

        exec_ctx(size_t threadNumber = 1)
            : _ctx(threadNumber)
            , _work_guard(_ctx.get_executor())
            , _strand(boost::asio::make_strand(_ctx))
        {
            for (size_t i = 0; i < threadNumber; ++i)
                _pool.emplace_back([this]() { _ctx.run(); });
        }

        ~exec_ctx()
        {
            for (auto& t : _pool)
                t.detach();
        }
    };

    struct HW_cache
    {
        extHWtypeT dataType = exthwNone;
        std::string hwName;
        std::string hwModel;

        size_t SampleSize()
        {
            switch (dataType)
            {
            case exthwUSBdata16:
                return 4;
            case exthwFullPCM32:
            case exthwUSBdata32:
            case exthwUSBfloat32:
                return 8;
            case exthwUSBdata24:
                return 6;
            case exthwUSBdataU8:
            case exthwUSBdataS8:
                return 2;
            }
            assert(0);
            return 2;
        }
    };

    class Session : public ISession, public std::enable_shared_from_this<Session>
    {
        std::shared_ptr<exec_ctx> _ctx;
        std::shared_ptr<Session> _this;
        std::unique_ptr<IConnection> _connection;
        std::unique_ptr<IParser> _proto;
        std::unique_ptr<ExtIO_Dll> _dll;
        bool _bOpenHWSuccidded = false;
        AliveInstance _inst;
        HW_cache _hwCache;
        unsigned _callBackHandle = -1;
        std::unique_ptr<IMessageLoop> _msgLoop;

    public:

        Session(
            std::shared_ptr<exec_ctx>&& ctx,
            asio::ip::tcp::socket&& socket)
            : _ctx(std::move(ctx))
            , _connection(MakeConnection(_ctx->_strand))
            , _proto(Protocol::MakeParser(*_connection))
            , _msgLoop(MakeMessageLoop())
        {
            _connection->Attach(std::move(socket));
        }

        ~Session()
        {
            if(_callBackHandle != -1) FreeExtIOCallback(_callBackHandle);
            LOG(info) << "Session destroyed.";
        }

        auto AliveFlag()
        {
            return ::AliveFlag(_inst);
        }

        void AsyncStart()
        {
            _this = shared_from_this();

            asio::defer(_ctx->_strand, [this]() {InitDialog(); });
        }

        void AsyncDestroySession(const std::string& reason)
        {
            LOG(trace) << reason;

            if (_dll)
            {
                if (_bOpenHWSuccidded)
                {
                    //LOG(trace) << "Closing HW.#1";
                    //_dll->StopHW();
                    //_dll->SetCallback(nullptr);
                    //_dll->CloseHW();
                    //LOG(trace) << "Closed HW.#1";
                }
            }

            _connection->AsyncDisconnect(
                [this, a = AliveFlag()](const boost::system::error_code& ec)
                {
                    LOG(trace) << "Session destroing is in a progress, alive: " << a.IsAlive();

                    if (!a.IsAlive())
                        return;

                    if (_dll && _bOpenHWSuccidded)
                    {
                        LOG(trace) << "Closing HW.";
                        //_dll->StopHW();
                        _dll->CloseHW();
                    }

                    LOG(trace) << "Deleting session.";

                    _bOpenHWSuccidded = false;
                    _this.reset();
                });
        }

        // ISession
    private:

        void AyncStop() override
        {
            AsyncDestroySession("Stop called.");
        }

    private:

        void InitDialog()
        {
            LOG(trace) << "Session started.";

            OnServingRequestFinishedCB()({});
        }

        AsyncCb_T OnServingRequestFinishedCB()
        {
            return [this, a = AliveFlag()](const boost::system::error_code& lastOperationEc)
            {
                if (!a.IsAlive())
                    return;

                if (lastOperationEc.failed())
                {
                    AsyncDestroySession("Last operation failed: " + lastOperationEc.message());
                    return;
                }

                LOG(trace) << "Ready to serve.";

                _proto->AsyncReceiveRequest(
                    [this, a = AliveFlag()]
                    (const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& msg, int64_t did) {
                        if (!a.IsAlive())
                            return;
                        OnMessage(ec, msg, did);
                    });
            };
        }

        //
        void OnMessage(const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& msg, int64_t did)
        {
            if (ec.failed())
                return OnCommunicationError(ec);

            LOG(trace) << "New request [" << did << "] (" << _proto->GetMessageName(msg) << ") received...";
            std::optional<ExtIO_TCP_Proto::Message> responce;
            switch (msg.Content_case())
            {
            case ExtIO_TCP_Proto::Message::ContentCase::kHello:
                responce = OnHello(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kInitHW:
                responce = OnInitHW(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kLoadExtIOApi:
                responce = OnLoadExtIOApi(msg, did); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kOpenHW:
                responce = OnOpenHW(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kSetHWLO:
                responce = OnSetHWLO(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kSetHWLO64:
                responce = OnSetHWLO64(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kGetHWSR:
                responce = OnGetHWSR(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kStartHW:
                responce = OnStartHW(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kStopHW:
                responce = OnStopHW(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kVersionInfo:
                responce = OnVersionInfo(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kGetAttenuators:
                responce = OnGetAttenuators(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kGetActualAttIdx:
                responce = OnGetActualAttIdx(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIoShowMGC:
                responce = OnExtIoShowMGC(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIoGetAGCs:
                responce = OnExtIoGetAGCs(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIoGetActualAGCidx:
                responce = OnExtIoGetActualAGCidx(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIoGetMGCs:
                responce = OnExtIoGetMGCs(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIoGetActualMgcIdx:
                responce = OnExtIoGetActualMgcIdx(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIoGetSrates:
                responce = OnExtIoGetSrates(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIoGetActualSrateIdx:
                responce = OnExtIoGetActualSrateIdx(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIoSetSrate:
                responce = OnExtIoSetSrate(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIoGetBandwidth:
                responce = OnExtIoGetBandwidth(msg); break;

            case ExtIO_TCP_Proto::Message::ContentCase::kShowGUI:
                responce = OnShowGUI(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kHideGUI:
                responce = OnHideGUI(msg); break;
            case ExtIO_TCP_Proto::Message::ContentCase::kSwitchGUI:
                responce = OnSwitchGUI(msg); break;
            default:
                responce = OnUnhandledMessage(msg);
            }

            if (responce.has_value()) {
                LOG(trace) << "Request [" << did << "] (" << _proto->GetMessageName(msg) << ") sending responce with ("
                    << _proto->GetMessageName(*responce) << ") content...";
                _proto->AsyncSendResponce(*responce, did, OnServingRequestFinishedCB());
            }
            else
                OnServingRequestFinishedCB()({});
        }

        void OnCommunicationError(const boost::system::error_code& ec)
        {
            CheckRcvError(ec);

            AsyncDestroySession("Communication error: " + ec.message());
        }

        std::optional<ExtIO_TCP_Proto::Message> OnInitHW(const ExtIO_TCP_Proto::Message& inmsg)
        {
            auto& inithw = inmsg.inithw();
            return Protocol::Make_InitHW_Msg(
                { true }, { _hwCache.hwName}, { _hwCache.hwModel}, { _hwCache.dataType });
        }

        std::optional<ExtIO_TCP_Proto::Message> OnHello(const ExtIO_TCP_Proto::Message& inmsg)
        {
            auto& hello = inmsg.hello();

            return Protocol::Make_Hello_Msg(
                Protocol::c_protocolVersion,
                "ExtIO_TCP_server.dll");
        }

        std::optional<ExtIO_TCP_Proto::Message> OnLoadExtIOApi(const ExtIO_TCP_Proto::Message& inmsg, int64_t did)
        {
            auto& LoadExtIOApi = inmsg.loadextioapi();

            _dll = LoadLibrary(Options::get().extIoSharedLibName);
            if (!_dll)
            {
                AsyncDestroySession("Load ExtIO module <" + Options::get().extIoSharedLibName + "> is failed.");
                return {};
            }
            
            LOG(trace) << "Load ExtIO module <" + Options::get().extIoSharedLibName + "> success.";

            asio::post(
                _ctx->_strand,
                [this, a = AliveFlag(), did] () {
                    if (!a.IsAlive() || !_dll) return;

                    auto onFail = [this, a, did]
                    (std::string text, ExtIO_TCP_Proto::ErrorCode ec) mutable {
                        if (!a.IsAlive()) return;
                        //AsyncDestroySession(text);
                        auto msg = Protocol::Make_LoadExtIOApi_Msg(ec);
                        _proto->AsyncSendResponce(msg, did, 
                            [this, text, a](const boost::system::error_code& ec) mutable {
                                if (!a.IsAlive()) return;
                                AsyncDestroySession(text);
                            }
                        );
                    };

                    char arg1[128];
                    char arg2[128];
                    int arg3;

                    if (!_dll->InitHW(arg1, arg2, arg3))
                    {
                        _dll.reset();
                        onFail("initHW was FAILED.", ExtIO_TCP_Proto::ErrorCode::LogicError);
                        return;
                    }

                    _hwCache.dataType = (extHWtypeT)arg3;
                    _hwCache.hwName = arg1;
                    _hwCache.hwModel = arg2;

                    LOG(trace) << "InitHW succeeded, name: "
                        << _hwCache.hwName << "; model: " << _hwCache.hwModel;

                    auto [handle, staticCb] = SetupExtIOCallback([this](int cnt, int status, float IQoffs, void* IQdata) {
                        return ExtIOCallback(cnt, status, IQoffs, IQdata);
                        });

                    _callBackHandle = handle;
                    LOG(trace) << "ExtIOCallback allocated: " << _callBackHandle;
                    _dll->SetCallback(staticCb);

                    {
                        // The SDRPlay ExtIO module requires it to call 
                        // OpenHW API from withing a message thread.
                        // Because it creates some UI widgets which require 
                        // a message loop to function properly.
                        auto [p, f] = MakePFPair<bool>();
                        _msgLoop->post([this, a = AliveFlag(), p]() mutable {
                            if (!a.IsAlive()) return;
                            p->set_value(_dll->OpenHW());
                            });
                        if (!f.get())
                        {
                            onFail("OpenHW was FAILED.", ExtIO_TCP_Proto::ErrorCode::LogicError);
                            return;
                        }
                    }

                    LOG(trace) << "OpenHW succeeded.";

                    _bOpenHWSuccidded = true;

                    auto msg = Protocol::Make_LoadExtIOApi_Msg(ExtIO_TCP_Proto::ErrorCode::Success);
                    _proto->AsyncSendResponce(msg, did, OnServingRequestFinishedCB());
                }
            );
            return {};
        }

        std::optional<ExtIO_TCP_Proto::Message> OnUnhandledMessage(const ExtIO_TCP_Proto::Message& inmsg)
        {
            LOG(trace) << "Unexpected message: " << inmsg.DebugString();
            return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::NotImplemented);
        }

        std::optional<ExtIO_TCP_Proto::Message> OnOpenHW(const ExtIO_TCP_Proto::Message& inmsg)
        {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            auto [p, f] = MakePFPair<bool>();
            _msgLoop->post([this, a = AliveFlag(), p]() mutable {
                if (!a.IsAlive()) return;
                p->set_value(_dll->OpenHW());
                });
            return Protocol::Make_OpenHW_Msg(f.get());
        }

        std::optional<ExtIO_TCP_Proto::Message> OnSetHWLO(const ExtIO_TCP_Proto::Message& inmsg)
        {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            long lofreq = 0;
            if (inmsg.sethwlo().has_lofreq()) lofreq = inmsg.sethwlo().lofreq();
            auto result = _dll->SetHWLO(lofreq);
            return Protocol::Make_SetHWLO_Msg({ result }, {});
        }

        std::optional<ExtIO_TCP_Proto::Message> OnSetHWLO64(const ExtIO_TCP_Proto::Message& inmsg)
        {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            int64_t lofreq = 0;
            if (inmsg.sethwlo64().has_lofreq()) lofreq = inmsg.sethwlo64().lofreq();
            int64_t result = -1;
            if (_dll->SetHWLO64)
            {
                result = _dll->SetHWLO64 ? _dll->SetHWLO64(lofreq) : -1;
            }
            else
            {
                LOG(trace) << "Dll has not SetHWLO64 implemented, fall back to SetHWLO.";
                result = _dll->SetHWLO ? _dll->SetHWLO((int32_t)lofreq) : -1;
            }
            return Protocol::Make_SetHWLO64_Msg({ result }, {});
        }

        std::optional<ExtIO_TCP_Proto::Message> OnGetHWSR(const ExtIO_TCP_Proto::Message& inmsg)
        {
            long result = -1;
            if (_dll) result = _dll->GetHWSR();
            return Protocol::Make_GetHWSR_Msg({ result });
        }

        
        std::optional<ExtIO_TCP_Proto::Message> OnStartHW(const ExtIO_TCP_Proto::Message& inmsg)
        {
            int32_t result = -1;
            long extLOfreq = -1;
            const auto& starthw = inmsg.starthw();
            if (starthw.has_extlofreq()) extLOfreq = starthw.extlofreq();
            if (_dll) result = _dll->StartHW(extLOfreq);
            return Protocol::Make_StartHW_Msg({ result }, {});
        }

        std::optional<ExtIO_TCP_Proto::Message> OnStopHW(const ExtIO_TCP_Proto::Message& inmsg)
        {
            LOG(trace) << "Before StopHW call.";
            if (_dll) _dll->StopHW();
            LOG(trace) << "After StopHW call.";
            return Protocol::Make_StopHW_Msg({ ExtIO_TCP_Proto::ErrorCode::Success});
        }

        int ExtIOCallback(int cnt, int status, float IQoffs, void* IQdata)
        {
            if(cnt<=0) 
                LOG(trace) << "ExtIOCallback is called with cnt: " << cnt << "; status: " << status;

            auto msg = Protocol::Make_ExtIOCallback_Msg(
                cnt, status, IQoffs, IQdata, _hwCache.SampleSize());

            boost::asio::dispatch(_ctx->_strand, [this, a = AliveFlag(), msg = std::move(msg)]() mutable {
                if (!a.IsAlive() || !_bOpenHWSuccidded || !_proto) {
                    return;
                }
                _proto->AsyncSendMessage(std::move(msg), [](const boost::system::error_code& ec) {});
            });

            return 0;
        }

        std::optional<ExtIO_TCP_Proto::Message> OnVersionInfo(const ExtIO_TCP_Proto::Message& request) {
            if (_dll && _dll->VersionInfo)
            {
                const auto& msg = request.versioninfo();
                const char* progname = nullptr;
                int ver_major = -1, ver_minor = -1;
                if (msg.has_progname()) progname = msg.progname().c_str();
                if (msg.has_ver_major()) ver_major = msg.ver_major();
                if (msg.has_ver_minor()) ver_minor = msg.ver_minor();
                _dll->VersionInfo(progname, ver_major, ver_minor);
            }
            return Protocol::Make_VersionInfo_Msg({}, {}, {});
        }

        std::optional<ExtIO_TCP_Proto::Message> OnGetAttenuators(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            auto const& msg = request.getattenuators();
            if (!msg.has_atten_idx())
                return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::InvalidArgument);
            float attenuator = 0.;
            auto retVal = _dll->GetAttenuators(msg.atten_idx(), &attenuator);
            return Protocol::Make_GetAttenuators_Msg({ retVal }, {}, { attenuator });
        }

        std::optional<ExtIO_TCP_Proto::Message> OnGetActualAttIdx(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            auto const& msg = request.getactualattidx();
            auto result = _dll->GetActualAttIdx();
            return Protocol::Make_GetActualAttIdx_Msg(result);
        }

        std::optional<ExtIO_TCP_Proto::Message> OnExtIoShowMGC(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            if(!_dll->ExtIoShowMGC) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::NotImplemented);
            auto const& msg = request.extioshowmgc();
            if (!msg.has_agc_idx())
                return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::InvalidArgument);
            auto result = _dll->ExtIoShowMGC(msg.agc_idx());
            return Protocol::Make_ExtIoShowMGC_Msg(result, {});
        }

        std::optional<ExtIO_TCP_Proto::Message> OnShowGUI(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            if (!_dll->ShowGUI) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::NotImplemented);
            _dll->ShowGUI();
            return {};
        }

        std::optional<ExtIO_TCP_Proto::Message> OnHideGUI(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            if (!_dll->HideGUI) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::NotImplemented);
            _dll->HideGUI();
            return {};
        }

        std::optional<ExtIO_TCP_Proto::Message> OnSwitchGUI(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            if (!_dll->SwitchGUI) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::NotImplemented);
            _dll->SwitchGUI();
            return {};
        }

        std::optional<ExtIO_TCP_Proto::Message> OnExtIoGetAGCs(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            auto const& msg = request.extiogetagcs();
            if (!msg.has_agc_idx())
                return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::InvalidArgument);
            char text[EXTIO_MAX_AGC_VALUES]; text[0]=0;
            int retVal = -1;
            if(_dll->ExtIoGetAGCs)
                retVal = _dll->ExtIoGetAGCs(msg.agc_idx(), text);
            return Protocol::Make_ExtIoGetAGCs_Msg(retVal, {}, text);
        }

        std::optional<ExtIO_TCP_Proto::Message> OnExtIoGetActualAGCidx(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            auto const& msg = request.extiogetactualagcidx();
            int result = -1;
            if(_dll->ExtIoGetActualAGCidx)
                result = _dll->ExtIoGetActualAGCidx();
            return Protocol::Make_ExtIoGetActualAGCidx_Msg(result);
        }

        std::optional<ExtIO_TCP_Proto::Message> OnExtIoGetMGCs(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            auto const& msg = request.extiogetmgcs();
            if (!msg.has_mgc_idx())
                return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::InvalidArgument);
            float gain = 0.;
            int retVal = -1;
            if (_dll->ExtIoGetMGCs)
                retVal = _dll->ExtIoGetMGCs(msg.mgc_idx(), &gain);
            return Protocol::Make_ExtIoGetMGCs_Msg(retVal, {}, gain);
        }

        std::optional<ExtIO_TCP_Proto::Message> OnExtIoGetActualMgcIdx(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            auto const& msg = request.extiogetactualmgcidx();
            int result = -1;
            if (_dll->ExtIoGetActualMgcIdx)
                result = _dll->ExtIoGetActualMgcIdx();
            return Protocol::Make_ExtIoGetActualMgcIdx_Msg(result);
        }

        std::optional<ExtIO_TCP_Proto::Message> OnExtIoGetSrates(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            auto const& msg = request.extiogetsrates();
            if (!msg.has_srate_idx())
                return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::InvalidArgument);
            double samplerate = 0.;
            int retVal = -1;
            if (_dll->ExtIoGetSrates)
                retVal = _dll->ExtIoGetSrates(msg.srate_idx(), &samplerate);
            return Protocol::Make_ExtIoGetSrates_Msg(retVal, {}, samplerate);
        }

        std::optional<ExtIO_TCP_Proto::Message> OnExtIoGetActualSrateIdx(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            auto const& msg = request.extiogetactualsrateidx();
            int result = -1;
            if (_dll->ExtIoGetActualSrateIdx)
                result = _dll->ExtIoGetActualSrateIdx();
            return Protocol::Make_ExtIoGetActualSrateIdx_Msg(result);
        }

        std::optional<ExtIO_TCP_Proto::Message> OnExtIoSetSrate(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            if (!_dll->ExtIoSetSrate) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::NotImplemented);
            auto const& msg = request.extiosetsrate();
            if (!msg.has_srate_idx())
                return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::InvalidArgument);
            auto result = _dll->ExtIoSetSrate(msg.srate_idx());
            return Protocol::Make_ExtIoSetSrate_Msg(result, {});
        }

        std::optional<ExtIO_TCP_Proto::Message> OnExtIoGetBandwidth(const ExtIO_TCP_Proto::Message& request) {
            if (!_dll) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::ExtIO_DllIsNotLoaded);
            if (!_dll->ExtIoGetBandwidth) return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::NotImplemented);
            auto const& msg = request.extiogetbandwidth();
            if (!msg.has_srate_idx())
                return Protocol::Make_Error_Msg(ExtIO_TCP_Proto::ErrorCode::InvalidArgument);
            auto result = _dll->ExtIoGetBandwidth(msg.srate_idx());
            return Protocol::Make_ExtIoGetBandwidth_Msg(result, {});
        }

    private:

        bool CheckRcvError(const boost::system::error_code& ec)
        {
            if(ec.failed())
            {
                LOG(trace) << "Rcv error: " << ec.message();
                return false;
            }

            return true;
        }
    };
}

std::future<std::weak_ptr<ISession>> MakeSession(
    strand_type&& _strand,
    socket_type&& socket)
{
    std::promise<std::weak_ptr<ISession>> prom;

    auto retVal = prom.get_future();

    // limitation: for SDRPlay only one thread.
    // As it is singlethreaded.
    auto ctx = std::make_shared<exec_ctx>(1);

    asio::post(
        ctx->_strand,
        [prom=std::move(prom), ctx, socket = std::move(socket)]() mutable
        {
            auto p = std::make_shared<Session>(std::move(ctx), std::move(socket));
            prom.set_value(p);
            p->AsyncStart();
        }
    );

    return retVal;
}

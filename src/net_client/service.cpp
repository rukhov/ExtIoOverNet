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

#include "service.h"
#include "../utils/log.h"
#include "../utils/Connection.h"
#include "../utils/Protocol.h"
#include "../utils/Messages.h"
#include "../utils/IsAlive.h"
#include "../utils/Mutexed.h"
#include "../utils/AtScopeExit.h"

using namespace boost;

namespace
{
    class Service : public IService, public std::enable_shared_from_this<Service>
    {
        using strand_type = asio::strand<asio::io_context::executor_type>;
        using work_guard_type = asio::executor_work_guard<strand_type>;
        using resolve_results_type = asio::ip::tcp::resolver::results_type;
        using deadline_timer = boost::asio::deadline_timer;

        std::shared_ptr<Service> _pThis;

        strand_type _strand;
        std::unique_ptr<work_guard_type> _work_guard;
        const Options& _options;

        std::unique_ptr<IConnection> _connection;
        std::shared_ptr<Protocol::IParser> _proto;

        deadline_timer _reconnect_timer;
        bool _connectionEstablished = false;
        bool _apiLoaded = false;
        Mutexed<pfnExtIOCallback> _pfnExtIOCallback = nullptr;
        std::vector<std::promise<void>> _initWaiters;

        AliveInstance _inst;

    public:

        Service(boost::asio::strand<boost::asio::io_context::executor_type>&& strand, const Options& opts)
            : _strand(strand)
            , _work_guard(std::make_unique<work_guard_type>(asio::make_work_guard(_strand)))
            , _options(opts)
            , _reconnect_timer(_strand)
        {
            LOG(trace) << "Service constructed.";

            _connection = MakeConnection(_strand);
        }

        ~Service()
        {
            LOG(trace) << "Service destructed.";
            _work_guard.reset();
        }

        auto AliveFlag()
        {
            return ::AliveFlag(_inst);
        }

        void Init()
        {
            _pThis = shared_from_this();
        }

    private:

        void Cancel()
        {
            _connectionEstablished = false;
            _reconnect_timer.cancel();
            if (_proto) _proto->Cancel();
            if (_connection) _connection->Cancel();
            _connection->Close();
            _proto.reset();
        }

        void Connect(IConnection::CbT&& cb)
        {
            BOOST_LOG_NAMED_SCOPE("Connect");
            LOG(trace) << "Service started, connecting.";

            _connection->Connect(_options.serverAddress, _options.serverPort, 
                [this, a = AliveFlag(), cb = std::move(cb)](const boost::system::error_code& ec) mutable
                {
                    if (!a.IsAlive()) return;
                    OnConnected(ec, std::move(cb));
                }
            );
        }

        bool CheckErrorCode(const boost::system::error_code& ec)
        {
            if (!ec.failed())
                return true;

            if(_proto) _proto->Cancel();
            if(_connection) _connection->Cancel();

            LOG(trace) << "Communication error: " << ec.message();

            if (_connectionEstablished)
                OnDisconnected();

            _connectionEstablished = false;
            RestartConnection(8000);

            return false;
        }

        void OnConnected(const boost::system::error_code& ec, IConnection::CbT&& cb)
        {
            if (!CheckErrorCode(ec))
                return;

            _connectionEstablished = true;

            _proto = Protocol::MakeParser(*_connection);

            //AsyncReadData();
            AsyncReadRequest();

            auto msg = Protocol::Make_Hello_Msg(
                Protocol::c_protocolVersion, 
                "ExtIO_TCP_client.dll");

            auto h = [this, a = AliveFlag(), cb = std::move(cb)]
            (const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& res, int64_t did) mutable {
                if (!a.IsAlive()) return;
                auto e = OnHello(ec, res, did);
                if (cb) cb(e);
            };

            _proto->AsyncSendRequest(msg, std::move(h));
        }

        void OnDisconnected()
        {
            _apiLoaded = false;
            LOG(trace) << "Disconnected, force Client to stop.";

            auto fn = _pfnExtIOCallback.lock();
            if (*fn)
            {
                (*fn)(-1, extHw_Stop, .0, nullptr);
                (*fn)(-1, extHw_Disconnected, .0, nullptr);
            }
        }

        void AsyncReadRequest()
        {
            _proto->AsyncReceiveRequest(
                [this, a = AliveFlag()]
                (const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& msg, int64_t did) {
                    if (!a.IsAlive())
                        return;
                    OnMessage(ec, msg, did);
                });
        }

        void OnMessage(const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& msg, int64_t did)
        {
            if (!CheckErrorCode(ec))
                return;

            switch (msg.Content_case())
            {
            case ExtIO_TCP_Proto::Message::ContentCase::kExtIOCallback:
                return OnExtIOCallback(ec, msg, did);
            default:
                LOG(trace) << "Unexpected message: " << msg.DebugString();
            }

            AsyncReadRequest();
        }

        void OnExtIOCallback(const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& inmsg, int64_t did)
        {
            auto& data = inmsg.extiocallback();

            if (data.cnt() <= 0)
            {
                LOG(trace) << "ExtIOCallback request received, cnt: "
                    << data.cnt()
                    << "; status: " << data.status()
                    << "; data.size(): " << data.iqdata().size();
            }

            auto p = _pfnExtIOCallback.lock();
            if (*p)
            {
                (*p)(data.cnt(), data.status(), data.iqoffs(), const_cast<char*>(data.iqdata().data()));
            }
        }

        void RestartConnection(unsigned delayMs)
        {
            _reconnect_timer.expires_from_now(boost::posix_time::milliseconds(delayMs));

            auto restart = [this]() {
                
                if (!_connection->IsConnected())
                {
                    return OnReconnect({});
                }

                _connection->AsyncDisconnect(
                    [this, a = AliveFlag()](const boost::system::error_code& error) {
                        if (!a.IsAlive()) return;
                        OnReconnect(error);
                    });
            };

            if (!delayMs)
                return restart();
            
            _reconnect_timer.async_wait([this, restart = std::move(restart)](const boost::system::error_code& error) {
                if (error.failed())
                    return;
                restart();
                });
        }

        void OnReconnect(const boost::system::error_code& ec)
        {
            if (ec.failed())
            {
                LOG(trace) << "Connection  failed: " << ec.message();
                return;
            }

            Connect({});
        }

        boost::system::error_code OnHello(const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& res, int64_t did)
        {
            if (!res.IsInitialized() || !res.has_hello())
            {
                LOG(trace) << "Inconsistent hello responce.";
                return {};
            }

            LOG(trace) << "Hello responce received: " << res.hello().version().client_version_name();

            auto msg = Protocol::Make_LoadExtIOApi_Msg(
                ExtIO_TCP_Proto::ErrorCode::Unexpected);

            auto h = [this, a = AliveFlag()]
            (const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& msg, int64_t did) mutable {
                if (!a.IsAlive()) return;
                LOG(trace) << "LoadExtIOApi responce received.";
                _apiLoaded = true;
                for (auto& w : _initWaiters)
                    w.set_value();
                _initWaiters.clear();

                auto cb = _pfnExtIOCallback.lock();
                if (*cb) {
                    //(*cb)(-1, extHw_READY, .0, nullptr);
                    //(*cb)(-1, extHw_RUNNING, .0, nullptr);
                    //(*cb)(-1, extHw_Start, .0, nullptr);
                }
            };

            _proto->AsyncSendRequest(msg, std::move(h));

            return {};
        }

        std::future_status WaitForConnection(uint32_t ms = 30000)
        {
            std::promise<void> prm;
            auto fut = prm.get_future();

            asio::defer(_strand,
                [this, prm = std::move(prm), a = AliveFlag()]() mutable {
                    if (!a.IsAlive())
                    {
                        prm.set_value();
                        return;
                    }
                    if (!_apiLoaded)
                        _initWaiters.push_back(std::move(prm));
                    else
                        prm.set_value();
                });

            return fut.wait_for(std::chrono::milliseconds(ms));
        }

        // IService
        // Another thread context
        // Blocking calls
    private:
        bool Start() override
        {
            asio::dispatch(_strand, 
                [this, a = AliveFlag()]() {
                    if (!a.IsAlive()) return;
                    if (!_connectionEstablished)
                        Connect([this](const boost::system::error_code& ec) {});
                });

            auto res = WaitForConnection();
            if (res == std::future_status::ready)
                return true;

            return false;
        }

        void Stop() override
        {
            std::promise<void> prm;

            auto f = prm.get_future();

            asio::defer(_strand, [this, a = AliveFlag(), prm = std::move(prm)]() mutable {

                if (!a.IsAlive()) return;

                LOG(trace) << "Service finishing is in a progress..";

                Cancel();
                _pThis.reset();
                prm.set_value();
                });

            auto status = f.wait_for(std::chrono::milliseconds(20000));

            if (status == std::future_status::timeout) 
            {
                LOG(trace) << "Service finish timedout.";
                _reconnect_timer.cancel();
                _work_guard.reset();
            }else
                LOG(trace) << "Service finish is done.";
        }

        // IExtIO_API
        // All calls from other thread context
    private:

        bool InitHW(char* name, char* model, int& type) override
        {
            if (!Start())
                return false;

            LOG(trace) << "InitHW called";

            auto [p, f] = Protocol::MakePFPair<ExtIO_TCP_Proto::Message>();

            auto h = [this, a = AliveFlag(), p]
            (const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& res, int64_t did) mutable {
                if (!a.IsAlive() || !_connectionEstablished) return;
                if (!res.has_inithw())
                {
                    p->set_value({});
                    return;
                }
                LOG(trace) << "InitHW responce: " 
                    << res.inithw().has_result() ? res.inithw().result() : false;
                p->set_value(res);
                return;
            };

            asio::defer(_strand, [this, a = AliveFlag(), h = std::move(h)]() mutable {
                if (!a.IsAlive() || !_connectionEstablished) return;
                _proto->AsyncSendRequest(Protocol::Make_InitHW_Msg({}, {}, {}, {}), std::move(h));
                });

            auto msg = f.get();
            if (!msg.has_inithw())
                return false;

            auto& inithw = msg.inithw();

            if (inithw.has_name())
                strncpy_s(name, EXTIO_MAX_NAME_LEN, inithw.name().c_str(), inithw.name().length());
            if (inithw.has_model()) {
                strncpy_s(model, 63, inithw.model().c_str(), inithw.model().length());
            }
            if (inithw.has_type())
                type = inithw.type();

            return inithw.has_result() ? inithw.result() : true;
        }

        bool OpenHW() override
        {
            return true;
        }

        int SetHWLO(long LOfreq) override
        {
            LOG(trace) << "SetHWLO is called.";
            auto msg = SyncSendRequest(
                Protocol::Make_SetHWLO_Msg({}, LOfreq));
            if (!msg.has_sethwlo())
                return -1;
            auto const& data = msg.sethwlo();
            if (data.has_result())
                return data.result();
            return -1;
        }

        int64_t SetHWLO64(int64_t LOfreq) override
        {
            LOG(trace) << "SetHWLO64 is called.";
            auto msg = SyncSendRequest(
                Protocol::Make_SetHWLO64_Msg({}, LOfreq));
            if (!msg.has_sethwlo64())
                return -1;
            auto const& data = msg.sethwlo64();
            if (data.has_result())
                return data.result();
            return -1;
        }

        long GetHWSR() override
        {
            LOG(trace) << "GetHWSR called";

            auto [p, f] = Protocol::MakePFPair<long>();

            auto msg = Protocol::Make_GetHWSR_Msg({});

            auto h = [this, a = AliveFlag(), p]
            (const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& res, int64_t did) mutable {
                if (!a.IsAlive() || !_connectionEstablished) return;
                if (!res.has_gethwsr())
                {
                    p->set_value(-1);
                    return;
                }
                if (!res.gethwsr().has_result())
                {
                    p->set_value(-1);
                    return;
                }
                LOG(trace) << "GetHWSR responce: " << res.gethwsr().result();
                p->set_value(res.gethwsr().result());
            };

            asio::defer(_strand, [this, a = AliveFlag(), msg = std::move(msg), h = std::move(h)]() mutable {
                if (!a.IsAlive() || !_connectionEstablished) return;
                _proto->AsyncSendRequest(msg, std::move(h));
                });

            return f.get();
        }

        int StartHW(long extLOfreq) override
        {
            LOG(trace) << "StartHW called";

            auto [p, f] = Protocol::MakePFPair<int>();

            auto msg = Protocol::Make_StartHW_Msg({}, extLOfreq);

            auto h = [this, a = AliveFlag(), p]
            (const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& res, int64_t did) mutable {
                if (!a.IsAlive() || !_connectionEstablished) return;
                if (!res.has_starthw())
                {
                    p->set_value(-1);
                    return;
                }
                if (!res.starthw().has_result())
                {
                    p->set_value(-1);
                    return;
                }
                LOG(trace) << "StartHW responce: " << res.starthw().result();
                p->set_value(res.starthw().result());
            };

            asio::defer(_strand, [this, a = AliveFlag(), msg = std::move(msg), h = std::move(h)]() mutable {
                if (!a.IsAlive() || !_connectionEstablished) return;
                _proto->AsyncSendRequest(msg, std::move(h));
                });

            return f.get();
        }

        void StopHW(void) override
        {
            LOG(trace) << "StopHW called";

            auto [p, f] = Protocol::MakePFPair<void>();

            asio::defer(_strand, [this, p, a = AliveFlag()]() mutable {
                if (!a.IsAlive() || !_connectionEstablished) {
                    p->set_value();
                    return;
                }
                _proto->AsyncSendRequest(Protocol::Make_StopHW_Msg({}),
                    [this, a, p]
                    (const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& res, int64_t did) mutable {
                        AtScopeExit _([p]() {p->set_value(); });
                        if (!a.IsAlive() || !_connectionEstablished || !res.has_stophw())
                            return;
                        LOG(trace) << "StopHW responce.";
                    });
                });

            return f.get();
        }

        void SetCallback(pfnExtIOCallback funcptr) override
        {
            LOG(trace) << "SetCallback is called.";
            std::promise<void> prm;
            auto f = prm.get_future();
            asio::defer(_strand, [this, a = AliveFlag(), funcptr, prm = std::move(prm)]() mutable {
                if (!a.IsAlive()) return;
                (*_pfnExtIOCallback.lock()) = funcptr;
                prm.set_value();
                });
            f.get();
        }

        ExtIO_TCP_Proto::Message SyncSendRequest(ExtIO_TCP_Proto::Message&& rqs)
        {
            LOG(trace) << "New request: " << Protocol::IParser::GetMessageName(rqs);
            auto [p, f] = Protocol::MakePFPair<ExtIO_TCP_Proto::Message>();
            int64_t _did = 0;
            auto const rqsContentCase = rqs.Content_case();
            auto h = [this, a = AliveFlag(), p, &_did]
            (const boost::system::error_code& ec, const ExtIO_TCP_Proto::Message& res, int64_t did) mutable {
                if (!a.IsAlive() || !_connectionEstablished) {
                    p->set_value({});
                    return;
                }
                _did = did;
                p->set_value(res);
            };
            asio::dispatch(_strand, [this, a = AliveFlag(), rqs = std::move(rqs), h = std::move(h)]() mutable {
                if (!a.IsAlive() || !_connectionEstablished) {
                    h({}, {}, -1);
                    return;
                }
                _proto->AsyncSendRequest(rqs, std::move(h));
                });
            auto responce = std::move(f.get());
            LOG(trace)
                << "Responce [" << _did << "]: " 
                << Protocol::IParser::GetMessageName(responce)
                << (rqsContentCase == responce.Content_case()) ? "" : " >>> responce type is differ from request!";
            return responce;
        }

        void SyncSendNotify(ExtIO_TCP_Proto::Message&& rqs)
        {
            LOG(trace) << "New notify: " << Protocol::IParser::GetMessageName(rqs);
            auto [p, f] = Protocol::MakePFPair<void>();
            auto h = [this, a = AliveFlag(), p]
            (const boost::system::error_code& ec) mutable {
                if (!a.IsAlive() || !_connectionEstablished) {
                    p->set_value();
                    return;
                }
                p->set_value();
            };
            asio::dispatch(_strand, [this, a = AliveFlag(), rqs = std::move(rqs), h = std::move(h)]() mutable {
                if (!a.IsAlive() || !_connectionEstablished) {
                    h({});
                    return;
                }
                _proto->AsyncSendResponce(rqs, 0, std::move(h));
                });
            f.get();
        }

        void VersionInfo(const char* progname, int ver_major, int ver_minor) override {
            LOG(trace) << "VersionInfo is called.";
            auto msg = SyncSendRequest(
                Protocol::Make_VersionInfo_Msg(progname, ver_major, ver_minor));
        }

        int GetAttenuators(int atten_idx, float* attenuation) override {
            LOG(trace) << "GetAttenuators[" << atten_idx << "] is called.";
            auto msg = SyncSendRequest(
                Protocol::Make_GetAttenuators_Msg({}, atten_idx, {}));
            if (msg.has_getattenuators()) {
                auto const& ga = msg.getattenuators();
                if (ga.has_attenuation()) {
                    *attenuation = ga.attenuation();
                    //LOG(trace) << " > Attenuator[" << atten_idx << "]=" << *attenuation;
                }
                if (ga.has_result())
                    return ga.result();
            }
            return -1;
        }

        int GetActualAttIdx(void) override {
            LOG(trace) << "GetActualAttIdx is called.";
            auto responce = SyncSendRequest(
                Protocol::Make_GetActualAttIdx_Msg({}));
            if (responce.has_getactualattidx()) {
                auto const& msg = responce.getactualattidx();
                if (msg.has_result())
                    return msg.result();
            }
            return -1;
        }

        int ExtIoShowMGC(int agc_idx) override
        {
            auto responce = SyncSendRequest(
                Protocol::Make_ExtIoShowMGC_Msg({}, agc_idx));
            if (responce.has_extioshowmgc()) {
                auto const& msg = responce.extioshowmgc();
                if (msg.has_result())
                    return msg.result();
            }
            return 0;
        }

        void ShowGUI(void) override
        {
            SyncSendNotify(Protocol::Make_ShowGUI_Msg());
        }

        void HideGUI(void) override
        {
            SyncSendNotify(Protocol::Make_HideGUI_Msg());
        }

        void SwitchGUI(void) override
        {
            SyncSendNotify(Protocol::Make_SwitchGUI_Msg());
        }

        int ExtIoGetAGCs(int agc_idx, char* text) override
        {
            text[0] = 0;
            LOG(trace) << "ExtIoGetAGCs[" << agc_idx << "] is called.";
            auto msg = SyncSendRequest(
                Protocol::Make_ExtIoGetAGCs_Msg({}, agc_idx, {}));
            if (msg.has_extiogetagcs()) {
                auto const& data = msg.extiogetagcs();
                if (data.has_text())
                    strcpy(text, data.text().c_str());
                if (data.has_result())
                    return data.result();
            }
            return -1;
        }

        int ExtIoGetActualAGCidx(void) override
        {
            LOG(trace) << "ExtIoGetActualAGCidx is called.";
            auto responce = SyncSendRequest(
                Protocol::Make_ExtIoGetActualAGCidx_Msg({}));
            if (responce.has_extiogetactualagcidx()) {
                auto const& msg = responce.extiogetactualagcidx();
                if (msg.has_result())
                    return msg.result();
            }
            return -1;
        }

        int ExtIoGetMGCs(int mgc_idx, float* gain) override
        {
            *gain = 0.;
            LOG(trace) << "ExtIoGetMGCs[" << mgc_idx << "] is called.";
            auto msg = SyncSendRequest(
                Protocol::Make_ExtIoGetMGCs_Msg({}, mgc_idx, {}));
            if (msg.has_extiogetmgcs()) {
                auto const& data = msg.extiogetmgcs();
                if (data.has_gain())
                    *gain = data.gain();
                if (data.has_result())
                    return data.result();
            }
            return -1;
        }

        int ExtIoGetActualMgcIdx(void) override
        {
            LOG(trace) << "ExtIoGetActualMgcIdx is called.";
            auto responce = SyncSendRequest(
                Protocol::Make_ExtIoGetActualMgcIdx_Msg({}));
            if (responce.has_extiogetactualmgcidx()) {
                auto const& msg = responce.extiogetactualmgcidx();
                if (msg.has_result())
                    return msg.result();
            }
            return -1;
        }

        int ExtIoGetSrates(int srate_idx, double* samplerate) override
        {
            *samplerate = 0.;
            LOG(trace) << "ExtIoGetSrates[" << srate_idx << "] is called.";
            auto msg = SyncSendRequest(
                Protocol::Make_ExtIoGetSrates_Msg({}, srate_idx, {}));
            if (msg.has_extiogetsrates()) {
                auto const& data = msg.extiogetsrates();
                if (data.has_samplerate()) {
                    *samplerate = data.samplerate();
                    LOG(trace) << "ExtIoGetSrates[" << srate_idx << "] = " << *samplerate;
                }
                if (data.has_result())
                    return data.result();
            }
            return -1;
        }

        int ExtIoGetActualSrateIdx(void) override
        {
            LOG(trace) << "ExtIoGetActualSrateIdx is called.";
            auto responce = SyncSendRequest(
                Protocol::Make_ExtIoGetActualSrateIdx_Msg({}));
            if (responce.has_extiogetactualsrateidx()) {
                auto const& msg = responce.extiogetactualsrateidx();
                if (msg.has_result()) {
                    LOG(trace) << "ExtIoGetActualSrateIdx = " << msg.result();
                    return msg.result();
                }
            }
            return -1;
        }

        int ExtIoSetSrate(int srate_idx) override
        {
            LOG(trace) << "ExtIoSetSrate(" << srate_idx << ") is called.";
            auto responce = SyncSendRequest(
                Protocol::Make_ExtIoSetSrate_Msg({}, srate_idx));
            if (responce.has_extiosetsrate()) {
                auto const& msg = responce.extiosetsrate();
                if (msg.has_result())
                    return msg.result();
            }
            return -1;
        }

        long ExtIoGetBandwidth(int srate_idx) override
        {
            LOG(trace) << "ExtIoGetBandwidth(" << srate_idx << ") is called.";
            auto responce = SyncSendRequest(
                Protocol::Make_ExtIoGetBandwidth_Msg({}, srate_idx));
            if (responce.has_extiogetbandwidth()) {
                auto const& msg = responce.extiogetbandwidth();
                if (msg.has_result())
                    return msg.result();
            }
            return -1;
        }

        // HandlerBase
    private:
    };
}

std::future<std::weak_ptr<IService>>
    MakeService(
        boost::asio::strand<boost::asio::io_context::executor_type>&& _strand,
        const Options& opts)
{
    std::promise<std::weak_ptr<IService>> prm;

    auto retVal = prm.get_future();

    auto s = std::make_shared<boost::asio::strand<boost::asio::io_context::executor_type>>(std::move(_strand));
    
    asio::post(*s,
        [prm=std::move(prm), s, &opts]() mutable
        {
            auto p = std::make_shared<Service>(std::move(*s), opts);
            p->Init();
            prm.set_value(p);
        });

    return retVal;
}

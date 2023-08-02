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

#include "Protocol.h"

#include "AtScopeExit.h"
#include "IsAlive.h"

#include "log.h"

namespace
{
    namespace ProtoBuf = ExtIO_TCP_Proto;
    using namespace Protocol;
    using namespace boost;

    class Stream : public google::protobuf::io::ZeroCopyOutputStream
    {
        IConnection::buffer_type& buf;
        int64_t bytesWritten = 0;

    public:

        bool Next(void** data, int* size) override
        {
            *data = buf.data();
            *size = (int)buf.size();
            bytesWritten += *size;
            return true;
        }

        void BackUp(int count) override
        {
            buf.resize(buf.size() - count);
            bytesWritten -= count;
        }

        int64_t ByteCount() const override
        {
            return bytesWritten;
        }

    public:

        Stream(IConnection::buffer_type& _buf)
            : buf(_buf)
        {

        }
    };

    void SerializePackage(const ExtIO_TCP_Proto::PackagedMessage& pkg, IConnection::buffer_type& buf)
    {
        assert(pkg.IsInitialized());
        buf.resize(pkg.ByteSizeLong());
        Stream s(buf);
        google::protobuf::io::CodedOutputStream os(&s);
        pkg.SerializeToCodedStream(&os);
        buf.resize(s.ByteCount());
    }

    void DeserializePackage(ExtIO_TCP_Proto::PackagedMessage& pkg, const IConnection::buffer_type& buf)
    {
        google::protobuf::io::CodedInputStream in((const uint8_t*)buf.data(), buf.size());
        pkg.MergeFromCodedStream(&in);
    }

    class ProtoImpl : public Protocol::IParser
    {
        using RequestMapT = std::map<int64_t, OnMsgCb_T>;

        IConnection& _connection;

        std::atomic_int _readIsInProgress = 0;
        int64_t _nextDialogId = 0;
        RequestMapT _requestMap;
        OnMsgCb_T _requestHandler;

        AliveInstance _inst;

    public:

        ProtoImpl(IConnection& connection)
            : _connection(connection)
        {

        }

        ~ProtoImpl()
        {
            Cancel();
        }

    private:

        auto AliveFlag()
        {
            return ::AliveFlag(_inst);
        }

        int64_t MakeDialogId()
        {
            if (_nextDialogId == 0)
                _nextDialogId = 1;
            else if (_nextDialogId > 0)
                ++_nextDialogId;
            else
                --_nextDialogId;
            return _nextDialogId;
        }

        void AddReadHandler(int64_t dialogId, OnMsgCb_T&& handler)
        {
            if (dialogId == 0)
            {
                _requestHandler = std::move(handler);
            }
            else
            {
                assert(_requestMap.find(dialogId) == _requestMap.end());
                _requestMap[dialogId] = std::move(handler);
            }

            if (!_readIsInProgress)
            {
                auto packet = std::make_shared<IConnection::buffer_type>();
                AsyncReadPacket(
                    packet,
                    [this, packet, a = AliveFlag()](const boost::system::error_code& ec)
                    {
                        if (!a.IsAlive())
                            return;
                        OnAsyncReadDone(ec, packet); 
                    }
                );
            }
        }

        void OnAsyncReadDone(const boost::system::error_code& ec, IConnection::buffer_ptr buf)
        {
            assert(_readIsInProgress>0);
            --_readIsInProgress;
            bool handled = false;

            if (ec.failed())
            {
                if (_requestHandler)
                {
                    _requestHandler(ec, {}, 0);
                    _requestHandler = {};
                    handled = true;
                }
                else if (_requestMap.size())
                {
                    auto it = _requestMap.begin();
                    it->second(ec, {}, 0);
                    _requestMap.erase(it->first);
                    handled = true;
                }
            }
            else
            {
                assert(buf->packet_type() != PacketBuffer::PacketType::RawData);

                ExtIO_TCP_Proto::PackagedMessage pkg;
                DeserializePackage(pkg, *buf);

                const int64_t did = pkg.dialog_id();

                if (did == 0 || pkg.type() == ExtIO_TCP_Proto::MsgType::Request)
                {
                    if (_requestHandler)
                    {
                        _requestHandler(ec, pkg.msg(), did);
                        handled = true;
                    }
                }
                else
                {
                    auto it = _requestMap.find(did);
                    if (it != _requestMap.end())
                    {
                        it->second(ec, pkg.msg(), did);
                        _requestMap.erase(it->first);
                        handled = true;
                    }
                }

                if(!handled)
                {
                    LOG(trace) << "Unhandled request: " << pkg.DebugString();
                }
            }

            if (_requestHandler || _requestMap.size())
            {
                auto packet = std::make_shared<IConnection::buffer_type>();
                AsyncReadPacket(
                    packet,
                    [this, packet, a = AliveFlag()](const boost::system::error_code& ec) {
                        if (!a.IsAlive())
                            return;
                        OnAsyncReadDone(ec, packet);
                    }
                );
            }
        }

        void AsyncReadPacket(const IConnection::buffer_ptr& buf, AsyncCb_T&& cb)
        {
            assert(_readIsInProgress >= 0);
            ++_readIsInProgress;
            _connection.AsyncReadPacket(
                buf,
                [this, cb = std::move(cb), a = AliveFlag()](const boost::system::error_code& ec) mutable {
                    if (!a.IsAlive())
                        return;
                    cb(ec);
                });
        }

        // IParser
    private:

        void Cancel() override
        {
            _connection.Cancel();

            _readIsInProgress = 0;
            _nextDialogId = 0;
            _requestMap.clear();
            _requestHandler = {};
        }

        void AsyncDisconnect(AsyncCb_T&& cb) override
        {
            _connection.AsyncDisconnect(std::move(cb));
        }

        void _AsyncReadData(const IConnection::buffer_ptr& buf, AsyncCb_T&& cb) 
        {
            //assert(!_currentDataHandler);

            //_currentDataHandler = std::move(cb);

            AsyncReadPacket(
                buf,
                [this, buf, a = AliveFlag()](const boost::system::error_code& ec) {
                    if (!a.IsAlive())
                        return;
                    OnAsyncReadDone(ec, buf);
                }
            );
        }

        void AsyncReceiveRequest(OnMsgCb_T&& h) override
        {
            AddReadHandler(0, std::move(h));
        }

        void AsyncReceiveResponce(int64_t did, OnMsgCb_T&& h) override
        {
            AddReadHandler(did, std::move(h));
        }

        void AsyncSendRequest(const ExtIO_TCP_Proto::Message& msg, OnMsgCb_T&& h) override
        {
            ExtIO_TCP_Proto::PackagedMessage package;

            auto did = MakeDialogId();

            *package.mutable_msg() = msg;
            package.set_type(ExtIO_TCP_Proto::MsgType::Request);
            package.set_dialog_id(did);

            auto requestHandler = [this, h = std::move(h), did, a = AliveFlag()]
            (const boost::system::error_code& ec) mutable {
                if (!a.IsAlive())
                    return;

                if (ec.failed())
                {
                    h(ec, {}, did);
                    return;
                }

                AsyncReceiveResponce(did, std::move(h));
            };

            auto p = std::make_shared<IConnection::buffer_type>();
            SerializePackage(package, *p);
            p->set_packet_type(PacketBuffer::PacketType::Message);
            _connection.AsyncWritePacket(p, std::move(requestHandler));
        }

        void AsyncSendResponce(const ExtIO_TCP_Proto::Message& msg, int64_t did, AsyncCb_T&& h) override
        {
            ExtIO_TCP_Proto::PackagedMessage package;
            package.set_dialog_id(did);
            package.set_type(ExtIO_TCP_Proto::MsgType::Responce);
            *package.mutable_msg() = msg;
            auto p = std::make_shared<IConnection::buffer_type>();
            SerializePackage(package, *p);
            p->set_packet_type(PacketBuffer::PacketType::Message);
            _connection.AsyncWritePacket(p, std::move(h));
        }

        void AsyncSendMessage(std::unique_ptr<ExtIO_TCP_Proto::Message>&& msg, AsyncCb_T&& h) override
        {
            ExtIO_TCP_Proto::PackagedMessage package;
            package.set_dialog_id(0);
            package.set_type(ExtIO_TCP_Proto::MsgType::Responce);
            package.set_allocated_msg(msg.release());
            auto p = std::make_shared<IConnection::buffer_type>();
            SerializePackage(package, *p);
            p->set_packet_type(PacketBuffer::PacketType::Message);
            _connection.AsyncWritePacket(p, std::move(h));
        }
    };
}

namespace Protocol
{
    std::string const& IParser::GetMessageName(const ExtIO_TCP_Proto::Message& msg)
    {
        if (!msg.IsInitialized()) {
            static std::string un = "<Uninitialized>";
            return un;
        }
        auto const contentField = ExtIO_TCP_Proto::Message::descriptor()->field(msg.Content_case() - 1);
        return contentField->name();
    }

    std::unique_ptr<IParser> MakeParser(
        IConnection& c)
    {
        auto retVal = std::make_unique<ProtoImpl>(c);
        return retVal;
    }
}

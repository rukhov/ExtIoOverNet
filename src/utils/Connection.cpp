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

#include "Connection.h"
#include "IsAlive.h"
#include "AtScopeExit.h"
#include <queue>

#include "log.h"

namespace
{
    class Connection : public IConnection
    {
        using socket_type = boost::asio::ip::tcp::socket;
        using resolver_type = boost::asio::ip::tcp::resolver;
        using resolve_results_type = resolver_type::results_type;

        IConnection::strand_type& _strand;

        socket_type _socket;
        resolver_type _host_resolver;
        resolve_results_type _resolve_result;
        std::string _hostName;
        uint16_t _port;
        std::atomic_bool _isConnected = false;
        AliveInstance _inst;
        std::atomic_uint _asyncReadRecursionCounter = { 0 };
        std::atomic_uint _asyncWriteRecursionCounter = { 0 };
        uint64_t _nextPacketId = 0;

        struct queue_item
        {
            buffer_ptr buf;
            CbT cb;
        };

        std::queue<queue_item> _readQueue;
        std::queue<queue_item> _writeQueue;

    public:
        Connection(IConnection::strand_type& strand)
            : _strand(strand)
            , _socket(_strand)
            , _host_resolver(_strand)
        {
            
        }

        ~Connection() 
        {
        }

    private:

        auto AliveFlag()
        {
            return ::AliveFlag(_inst);
        }

        uint64_t nextPackedId()
        {
            return _nextPacketId++;
        }

        // IConnection
    private:
        virtual void Connect(std::string_view host, uint16_t port, CbT&& cb) override
        {
            _hostName = host;
            _port = port;

            LOG(trace) << "Start connecting to host <" << host << ">.";

            _host_resolver.async_resolve(
                host, std::to_string(port),
                [this, cb = std::move(cb), a=AliveFlag()](const boost::system::error_code& error, resolve_results_type results) mutable
                { 
                    if (!a.IsAlive()) return;
                    OnResolved(error, results, std::move(cb)); 
                });
        }

        void Attach(boost::asio::ip::tcp::socket&& socket) override
        {
            _isConnected = true;

            _socket = { _strand,
                boost::asio::ip::tcp::v4(),
                socket.release() };

            SetupOptions();
        }

        void Cancel() override
        {
            try
            {
                _host_resolver.cancel();
                if(_socket.is_open())_socket.cancel();
            }
            catch (const std::exception& e)
            {
                LOG(warning) << e.what();
            }
            _resolve_result = {};
            _asyncReadRecursionCounter = 0;
            _asyncWriteRecursionCounter = 0;
            while(!_readQueue.empty())
            {
                auto cb = std::move(_readQueue.front());
                _readQueue.pop();
                cb.cb(std::make_error_code(std::errc::operation_canceled));

            }
            _writeQueue = {};
            while (!_writeQueue.empty())
            {
                auto cb = std::move(_writeQueue.front());
                _writeQueue.pop();
                cb.cb(std::make_error_code(std::errc::operation_canceled));

            }

            if (_socket.is_open())
                _socket.cancel();
        }

        void Close() override
        {
            if (_socket.is_open())
                _socket.close();
        }

        void AsyncDisconnect(CbT&& cb) override
        {
            //if (!_socket.is_open()) return;

            try 
            {
                _host_resolver.cancel();
                if(_socket.is_open()) _socket.cancel();
            }
            catch (const boost::system::system_error& e)
            {
                LOG(trace) << "Failed to cancel operstions on socket: " << e.what() << ".";
            }

            boost::asio::defer(_strand, 
                [this, cb = std::move(cb), a = AliveFlag()]() mutable {
                    if (!a.IsAlive()) return;
                    _isConnected = false;
                    if (_socket.is_open()) _socket.close();
                    _resolve_result = {};
                    cb({});
                });
        }

        bool IsConnected() const override
        {
            assert( _strand.running_in_this_thread() );

            return _isConnected;
        }

        void AsyncWritePacket(const buffer_ptr& buf, CbT&& cb) override
        {
            assert(_strand.running_in_this_thread());

            assert(buf->raw_size() < 1*1024*1024);
            assert(buf->packet_type() == PacketBuffer::PacketType::Message ||
                buf->packet_type() == PacketBuffer::PacketType::RawData);

            {
                auto& pkt = const_cast<buffer_type&>(*buf);

                //if (pkt.size() > 50 * 1024) pkt.fill(0xaa);

                pkt.write_crc_to_head();
                pkt.set_id(nextPackedId());
            }

            if (_asyncWriteRecursionCounter)
            {
                _writeQueue.push({ buf , std::move(cb)});
                //LOG(trace) << "Queued write operation, queue size: " << _writeQueue.size();
                return;
            }

            ++_asyncWriteRecursionCounter;

            boost::asio::async_write(_socket, boost::asio::const_buffer(buf->head_data(), buf->raw_size()),
                [this, buf, cb = std::move(cb),a = AliveFlag()]
                (const boost::system::error_code& ec, std::size_t bytes_transferred) mutable
                {
                    if (!a.IsAlive()) return;
                    {
                        AtScopeExit _([a = &_asyncWriteRecursionCounter]() { --(*a);  });
                        if (ec.failed())
                        {
                            if (cb) cb(ec);
                            return;
                        }

                        if (bytes_transferred != buf->raw_size())
                        {
                            if (cb) cb(std::make_error_code(std::errc::io_error));
                            return;
                        }
                        /*
                        LOG(trace) << "Packet sent, type: "
                            << (int)buf->packet_type()
                            << "; packet_size: " << buf->packet_size()
                            << "; size: " << buf->size()
                            << "; crc1: " << buf->calc_crc()
                            << "; crc2: " << buf->get_crc()
                            << "; id: " << buf->get_id();
                            */
                    }

                    queue_item nextTask;
                    
                    if (!_writeQueue.empty())
                    {
                        nextTask = std::move(_writeQueue.front());
                        //LOG(trace) << "Dequeued write operation, queue size: " << _writeQueue.size();
                        _writeQueue.pop();
                    }

                    if (cb) cb(ec);

                    if (nextTask.buf)
                        AsyncWritePacket(nextTask.buf, std::move(nextTask.cb));
                });
        }

        void AsyncReadPacket(const buffer_ptr& buf, CbT&& cb) override
        {
            assert(_strand.running_in_this_thread());

            if (_asyncReadRecursionCounter)
            {
                _readQueue.push({buf, std::move(cb)});
                return;
            }

            ++_asyncReadRecursionCounter;

            boost::asio::async_read(_socket, boost::asio::mutable_buffer(buf->head_data(), buf->head_size()),
            [this, buf, cb = std::move(cb), a = AliveFlag()]
            (const boost::system::error_code& ec, std::size_t bytes_transferred) mutable
                {
                    if (!a.IsAlive()) return;
                    if (ec.failed())
                    {
                        if (cb) cb(ec);
                        return;
                    }

                    if (bytes_transferred != buf->head_size())
                    {
                        if (cb) cb(std::make_error_code(std::errc::protocol_error));
                        return;
                    }

                    if (buf->packet_type() != PacketBuffer::PacketType::RawData &&
                        buf->packet_type() != PacketBuffer::PacketType::Message)
                    {
                        if (cb) cb(std::make_error_code(std::errc::bad_message));
                        return;
                    }

                    if (buf->size() > 1*1024*1024)
                    {
                        if (cb) cb(std::make_error_code(std::errc::bad_message));
                        return;
                    }

                    buf->resize(buf->size());

                    boost::asio::async_read(
                        _socket, boost::asio::mutable_buffer(buf->data(), buf->size()),
                        boost::asio::transfer_exactly(buf->size()),
                    [this, buf, cb = std::move(cb), a = AliveFlag()]
                    (const boost::system::error_code& ec, std::size_t bytes_transferred) mutable
                        {
                            if (!a.IsAlive()) return;
                            if (ec.failed())
                            {
                                if (cb) cb(ec);
                                return;
                            }

                            if (bytes_transferred != buf->size())
                            {
                                if (cb) cb(std::make_error_code(std::errc::protocol_error));
                                return;
                            }

                            if (buf->calc_crc() != buf->get_crc())
                            {
                                LOG(trace) << "Packet read crc error, type: " 
                                    << (int)buf->packet_type() 
                                    << "; packet_size: " << buf->packet_size()
                                    << "; size: " << buf->size()
                                    << "; crc1: " << buf->calc_crc()
                                    << "; crc2: " << buf->get_crc()
                                    << "; id: " << buf->get_id();

                                if (cb) cb(std::make_error_code(std::errc::io_error));
                                return;
                            }

                            --_asyncReadRecursionCounter;

                            //LOG(trace) << "Packet read, type: " << (int)buf->packet_type() << "; id: " << buf->get_id();

                            queue_item nextTask;

                            if (!_readQueue.empty())
                            {
                                nextTask = std::move(_readQueue.front());
                                //LOG(trace) << "Dequeued read operation, queue size: " << _readQueue.size();
                                _readQueue.pop();
                            }

                            if (cb) cb(ec);

                            if (nextTask.buf)
                                AsyncReadPacket(nextTask.buf, std::move(nextTask.cb));
                        });
                });
        }

    private:

        void SetupOptions()
        {
            boost::asio::socket_base::keep_alive option(true);
            _socket.set_option(option);

            // the timeout value
            unsigned int timeout_milli = 10000;

            // platform-specific switch
#if defined _WIN32 || defined WIN32 || defined OS_WIN64 || defined _WIN64 || defined WIN64 || defined WINNT
  // use windows-specific time
            int32_t timeout = timeout_milli;
            setsockopt(_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            setsockopt(_socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
  // assume everything else is posix
            struct timeval tv;
            tv.tv_sec = timeout_milli / 1000;
            tv.tv_usec = (timeout_milli % 1000) * 1000;
            setsockopt(_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(_socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif        
        }

        void OnResolved(const boost::system::error_code& error, resolve_results_type results, CbT&& cb)
        {
            if (error.failed())
            {
                LOG(trace)
                    << "Cannot resolve host <"
                    << _hostName
                    << "> error: <"
                    << error.message()
                    << ">. Canceling...";
                return;
            }

            LOG(trace) << "Host <" << _hostName << "> is resolved.";

            {
                std::string text;
                for (const auto& r : results) {
                    text += "<" + r.endpoint().address().to_string() + ">; ";
                }
                LOG(trace) << "Host <" << _hostName << "> is resolved to: " << text;
            }

            _resolve_result = results;

            auto endp = _resolve_result.begin();

            {
                // find ip v4 endpoint
                for(auto p = _resolve_result.begin(); p != _resolve_result.end(); ++p)
                {
                    auto prtc = p->endpoint().protocol();
                    if (prtc == boost::asio::ip::tcp::v4()) {
                        endp = p;
                        break;
                    }
                }
            }

            LOG(trace) << "Connecting to <" << endp->endpoint() << ">.";

            _socket.async_connect(*endp,
                [this, endp, cb = std::move(cb), a = AliveFlag()](const boost::system::error_code& error) mutable {
                    if (!a.IsAlive()) return;
                    OnConnect(error, endp, std::move(cb));
                });
        }

        void OnConnect(const boost::system::error_code& error, resolve_results_type::iterator it, CbT&& cb)
        {
            if (error.failed())
            {
                LOG(trace)
                    << "Cannot connect to host <"
                    << it->host_name()
                    << "> error: <"
                    << error.message()
                    << ">. Canceling...";
                cb(error);
                return;
            }

            _isConnected = true;

            SetupOptions();

            cb(error);
        }
    };
}

std::unique_ptr<IConnection> MakeConnection(IConnection::strand_type& strand)
{
    return std::make_unique<Connection>(strand);
}

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

#include <boost/function.hpp>
#include <boost/crc.hpp>

class PacketBuffer
{
public:

#pragma pack(push)
#pragma pack(1)
    enum class PacketType : char {
        RawData = 0,
        Message = 1,
    };

    struct PacketHead
    {
        PacketType type = PacketType::RawData;
        uint32_t size = 0;
        uint32_t crc = 0;
        uint64_t id = 0;
    };
#pragma pack(pop)

    PacketBuffer() {
        _data.reserve(128);
        _data.resize(headSize);
        set_packet_type(PacketType::Message);
        head().size = 0;
    }

    void reserve( size_t s ) {
        _data.reserve(s + headSize);
    }

    size_t size() const {
        return head().size;
    }

    void resize(size_t s) {
        head().size = s;
        return _data.resize(s + headSize);
    }

    void* data() {
        return _data.data() + headSize;
    }

    const void* data() const {
        return _data.data() + headSize;
    }

    const PacketType& packet_type() const {
        return head().type;
    }

    void set_packet_type(PacketType t) {
        head().type = t;
    }

    size_t packet_size() const {
        return head().size;
    }

    void* head_data() {
        return _data.data();
    }

    const void* head_data() const {
        return _data.data();
    }

    size_t raw_size() const {
        return _data.size();
    }

    static constexpr size_t head_size() {
        return headSize;
    }

    uint32_t calc_crc() const {
        boost::crc_32_type result;
        result.process_bytes(data(), size());
        return result.checksum();
    }

    uint32_t get_crc() const {
        return head().crc;
    }

    void write_crc_to_head()
    {
        head().crc = calc_crc();
    }

    void set_id(uint64_t id) {
        head().id = id;
    }

    uint64_t get_id() const {
        return head().id;
    }

    void fill(uint8_t b) {
        auto* s = (uint8_t*)data();
        auto* e = s + size();
        for (; s != e; ++s)
            *s = b;
    }

private:

    PacketHead const& head() const
    {
        return *reinterpret_cast<PacketHead const*>(_data.data());
    }

    PacketHead& head()
    {
        return *reinterpret_cast<PacketHead*>(_data.data());
    }

    std::vector<uint8_t> _data;
    static constexpr size_t headSize = sizeof(PacketHead);
};

class IConnection
{
public:

    using buffer_type = PacketBuffer;
    using buffer_ptr = std::shared_ptr<buffer_type>;
    using strand_type = boost::asio::strand<boost::asio::io_context::executor_type>;
    using CbT = std::move_only_function<void(const boost::system::error_code&)>;

    virtual ~IConnection() = default;

    virtual void Connect(std::string_view host, uint16_t port, CbT&&) = 0;
    virtual void Attach(boost::asio::ip::tcp::socket&& socket) = 0;
    virtual void Cancel() = 0;
    virtual void Close() = 0;
    virtual bool IsConnected() const = 0;
    virtual void AsyncDisconnect(CbT&& cb) = 0;
    virtual void AsyncWritePacket(const buffer_ptr& buf, CbT&& cb) = 0;
    virtual void AsyncReadPacket(const buffer_ptr& buf, CbT&& cb) = 0;
};

std::unique_ptr<IConnection> MakeConnection(IConnection::strand_type& strand);

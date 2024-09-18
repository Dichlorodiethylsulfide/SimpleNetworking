//
//          Created by Riki Lowe on 17/09/2024.
//
//              Copyright Riki Lowe 2024
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "ISocket.h"

// same windows or posix flags
#define DEFAULT_RECV_MSG_FLAG 0 
#define DEFAULT_SEND_MSG_FLAG 0 


class UDPSocket : public ISocket
{
public:
    using BoostAddress = boost::asio::ip::address;
    using BoostAddressV4 = boost::asio::ip::address_v4;
    using BoostUDP = boost::asio::ip::udp;

    
    UDPSocket(SocketType type, SocketMode mode, const std::string& address, uint16_t port, SocketRole role = DefaultRole, uint32_t timeoutMs = 20, uint32_t byteIntervalMs = 10)
        : ISocket(type, mode, role, address, port, timeoutMs, byteIntervalMs), m_udpSocket(m_ioService)
    {
    }

    ~UDPSocket() override
    {
        UDPSocket::close();
    }

    bool CheckForError(const std::function<void(boost::system::error_code&)>& operation, bool isUDP = false) override
    {
        return ISocket::CheckForError(operation, true);
    }

    bool isOpen() const override
    {
        return m_udpSocket.is_open();
    }

    void close() override
    {
        if(isOpen())
        {
            CheckForError([this](boost::system::error_code& err)
            {
                m_udpSocket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, err);
                m_udpSocket.close(err);
            });
            ISocket::close();
        }
    }
    
    bool setSocketTimeOut(int flag, uint32_t timeOutMs)
    {
    #ifndef _WIN32
        return true;
    #else
        return CheckForError([this, flag, timeOutMs](auto& err)
        {
            if(flag == SO_RCVTIMEO)
            {
                m_udpSocket.set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>(timeOutMs), err);
            }
            else
            {
                m_udpSocket.set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_SNDTIMEO>(timeOutMs), err);
            }
        });
    #endif
    }

    std::string getSocketName() const override
    {
        return m_address + ":" + std::to_string(m_port);
    }

protected:
    SocketResult internalReadData(unsigned char* data, size_t bufferSize) override
    {
        SocketResult result;
        result.success = CheckForError([this, data, bufferSize, &result](boost::system::error_code& err)
        {
            while(m_udpSocket.available() == 0) /// \todo change to receive whilst this is > 0
            {
                std::this_thread::yield();
            }
            result.bytes = m_udpSocket.receive_from(boost::asio::buffer(data, bufferSize), m_endPoint, DEFAULT_RECV_MSG_FLAG, err);
        });
        return result;
    }

    BoostUDP::socket m_udpSocket;
    BoostUDP::endpoint m_endPoint;
};


class UDPClient : public UDPSocket
{
public:
    UDPClient(SocketType type, const std::string& address, uint16_t port)
        : UDPSocket(type, SocketMode::Write, address, port, SocketRole::Client)
    {
    }

    bool open() override
    {
        if(CheckForError([&](auto& err)
        {
            m_udpSocket.open(BoostUDP::v4());
        }) == false)
        {
            return false;
        }
        m_address = toV4();
        m_endPoint = BoostUDP::endpoint(BoostAddress::from_string(m_address), m_port);

        m_initialised = CheckForError([this](boost::system::error_code& err)
        {
            m_udpSocket = BoostUDP::socket(m_ioService, BoostUDP::v4());
            m_udpSocket.connect(m_endPoint, err);
        });

        if(!m_initialised)
        {
            close();
            return false;
        }

        if(m_timeoutMs > 0)
        {
            setSocketTimeOut(SO_RCVTIMEO, m_timeoutMs);
        }

        return ISocket::open();
    }

protected:
    SocketResult internalWriteData(unsigned char* data, size_t bufferSize) override
    {
        SocketResult result;
        result.success = CheckForError([this, data, bufferSize, &result](boost::system::error_code& err)
        {
            result.bytes = m_udpSocket.send(boost::asio::buffer(data, bufferSize), DEFAULT_SEND_MSG_FLAG, err);
        });
        return result;
    }
};

class UDPServer : public UDPSocket
{
public:
    UDPServer(SocketType type, const std::string& address, uint16_t port)
        : UDPSocket(type, SocketMode::Read, address, port, SocketRole::Server)
    {
    }

    bool open() override
    {
        if(CheckForError([&](auto& err)
        {
            m_udpSocket.open(BoostUDP::v4());
        }) == false)
        {
            return false;
        }
        m_address = toV4();
        if(m_address.empty())
        {
            /// \todo I've seen implementations use 0.0.0.0 to denote 'any' not sure if we can use that here
            m_endPoint = BoostUDP::endpoint(BoostAddressV4::any(), m_port);
        }
        else
        {
            m_initialised = CheckForError([this](boost::system::error_code& err)
            {
                m_endPoint = BoostUDP::endpoint(BoostAddress::from_string(m_address), m_port);
            });
        }

        m_initialised = CheckForError([this](boost::system::error_code& err)
        {
            m_udpSocket = BoostUDP::socket(m_ioService, BoostUDP::v4());
            m_udpSocket.bind(m_endPoint, err);
        });

        if(!m_initialised)
        {
            close();
            return false;
        }

        if(m_timeoutMs > 0)
        {
            setSocketTimeOut(SO_RCVTIMEO, m_timeoutMs);
        }

        return ISocket::open();
    }

protected:
    SocketResult internalWriteData(unsigned char* data, size_t bufferSize) override
    {
        SocketResult result;
        result.success = CheckForError([this, data, bufferSize, &result](boost::system::error_code& err)
        {
            result.bytes = m_udpSocket.send_to(boost::asio::buffer(data, bufferSize), m_endPoint, DEFAULT_SEND_MSG_FLAG, err);
        });
        return result;
    }
};
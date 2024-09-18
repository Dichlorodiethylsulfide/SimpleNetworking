//
//          Created by Riki Lowe on 17/09/2024.
//
//              Copyright Riki Lowe 2024
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

/*

// Not ready - needs further testing as of 17/09/2024

#include "ISocket.h"

#define DEFAULT_TCP_RECV_MSG_FLAG 0 // same windows or posix flags
#define DEFAULT_TCP_SEND_MSG_FLAG 0 // same windows or posix flags

class TCPSocket : public ISocket
{
public:
    using BoostAddress = boost::asio::ip::address;
    using BoostAddressV4 = boost::asio::ip::address_v4;
    using BoostTCP = boost::asio::ip::tcp;

    TCPSocket(SocketType type, SocketMode mode, const std::string& address, uint16_t port, SocketRole role = DefaultRole, uint32_t timeoutMs = 20, uint32_t byteIntervalMs = 10)
    : ISocket(type, mode, role, address, port, timeoutMs, byteIntervalMs, static_cast<size_t>(BufferSize::TCP), static_cast<size_t>(BufferSize::TCP)), m_tcpSocket(m_ioService)
    {
    }

    std::string getSocketName() const override
    {
        return m_address + ":" + std::to_string(m_port);
    }

protected:
    SocketResult internalReadData(unsigned char* data, size_t bufferSize) override
    {
        SocketResult result;
        result.success = CheckForError([&](boost::system::error_code& err)
        {
            result.bytes = m_tcpSocket.receive(boost::asio::buffer(data, bufferSize), DEFAULT_TCP_RECV_MSG_FLAG, err);
        });
        return result;
    }

    SocketResult internalWriteData(unsigned char* data, size_t bufferSize) override
    {
        SocketResult result;
        result.success = CheckForError([&](boost::system::error_code& err)
        {
            result.bytes = m_tcpSocket.send(boost::asio::buffer(data, bufferSize), DEFAULT_TCP_SEND_MSG_FLAG, err);
        });
        return result;
    }
    
    BoostTCP::socket m_tcpSocket;
    BoostTCP::endpoint m_endPoint;
};

class TCPClient : public TCPSocket
{
public:
    TCPClient(SocketType type, const std::string& address, uint16_t port)
        : TCPSocket(type, SocketMode::Write, address, port, SocketRole::Client)
    {
    }

    bool open() override
    {
        m_address = toV4();
        m_initialised = CheckForError([this](boost::system::error_code& err)
        {
            m_endPoint = BoostTCP::endpoint(BoostAddress::from_string(m_address, err), m_port);
        });
        if(m_initialised)
        {
            m_initialised = CheckForError([this](boost::system::error_code& err)
            {
                m_tcpSocket.connect(m_endPoint, err);
            });
        }
        return m_initialised && isOpen();
    }
};

class TCPServer : public TCPSocket
{
public:
    TCPServer(SocketType type, const std::string& address, uint16_t port)
        : TCPSocket(type, SocketMode::Read, address, port, SocketRole::Server), m_acceptor(m_ioService, m_endPoint.protocol())
    {
    }

    void close() override
    {
        if(isOpen())
        {
            if (m_acceptor.is_open() && m_acceptorListeningState)
            {
                //In case the acceptor is blocking in listening state
                CheckForError([this](boost::system::error_code& err)
                {
                    BoostTCP::socket tempTcpSocket(m_ioService, BoostTCP::v4());
                    tempTcpSocket.connect(m_endPoint, err);
                });
            }

            CheckForError([this](boost::system::error_code& err)
            {
                m_acceptor.non_blocking(true, err);
                if (!err)
                {
                    m_acceptor.cancel(err);
                    if (!err)
                    {
                        m_acceptor.close(err);
                    }
                }
            });
            CheckForError([this](boost::system::error_code& err)
            {
                m_tcpSocket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, err);
                m_tcpSocket.close(err);
            });
            ISocket::close();
        }
    }
    
    bool listen()
    {
        m_acceptorListeningState = true;

        return CheckForError([this](boost::system::error_code& err)
        {
            if (isOpen())
            {
                m_acceptor.listen(boost::asio::socket_base::max_connections, err);
                if (!err)
                {
                    // m_tcpSocket = BoostTCP::socket(m_ioService);
                    m_acceptor.accept(m_tcpSocket, err);
                }
                m_acceptorListeningState = false;
            }
        });
    }

private:
    BoostTCP::acceptor m_acceptor;
    bool m_acceptorListeningState = false;
};
*/

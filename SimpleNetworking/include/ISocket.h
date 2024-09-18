//
//          Created by Riki Lowe on 17/09/2024.
//
//              Copyright Riki Lowe 2024
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <boost/asio.hpp>
#include <future>
#include <map>
#include <exception>

using namespace boost::asio;

enum class BufferSize : size_t
{
    Default = 1024,
    UDP = 1024,
    TCP = 512
};

// Could move NetworkMode from mosys::tracking here instead
enum class SocketMode
{
    Read = 0,
    Write,
    ReadWrite
};

enum class SocketType
{
    Blocking,
    NonBlocking
};

// Semantic -- Client is a writer, Server is a reader but both could implement the functions of the other?
enum class SocketRole
{
    Server,
    Client
};

static constexpr SocketRole DefaultRole = SocketRole::Client;

class NetworkingBuffer
{
public:
    using Buffer = std::vector<uint8_t>;
    
    NetworkingBuffer() :
    NetworkingBuffer(
        static_cast<size_t>(BufferSize::Default),
        static_cast<size_t>(BufferSize::Default)
        )
    {
    }
    
    NetworkingBuffer(const size_t sendBufferSize, const size_t recvBufferSize)
    {
        m_sendBuffer.resize(sendBufferSize);
        m_recvBuffer.resize(recvBufferSize);
    }

    virtual ~NetworkingBuffer() = default;

    void resizeSendBuffer(const size_t sendBufferSize)
    {
        m_sendBuffer.resize(sendBufferSize);
    }

    void resizeRecvBuffer(const size_t recvBufferSize)
    {
        m_recvBuffer.resize(recvBufferSize);
    }

    inline Buffer& getSendBuffer()
    {
        return m_sendBuffer;
    }

    inline Buffer& getRecvBuffer()
    {
        return m_recvBuffer;
    }
    
private:
    Buffer m_sendBuffer;
    Buffer m_recvBuffer;
};

// More general use case error handling
class ErrorHandler
{
public:
    void setLastErrorMessage(const std::string& errorMessage)
    {
        m_lastErrorMessage = errorMessage;
    }
    std::string getLastErrorMessage() // clears and returns the last error
    {
        if(m_lastErrorMessage.empty())
        {
            return "";
        }
        std::string lastErrorMessage = m_lastErrorMessage;
        m_lastErrorMessage = "";
        return lastErrorMessage;
    }
    
protected:
    std::string m_lastErrorMessage; // most recent error message
    std::function<void(const boost::system::error_code& error)> m_lastErrorCallback;
};

// Specifically for networking
class NetworkingErrorHandler : public ErrorHandler
{
public:
    enum class Severity
    {
        NoError,
        Permissive,
        Warning,
        Bad
    };
    static Severity getErrorSeverity(const boost::system::error_code& err, bool isUDP)
    {
        // see SocketErrorHandler.cpp
        if(err.failed())
        {
            return Severity::Bad;
        }
        return Severity::NoError;
    }
    virtual bool CheckForError(const std::function<void(boost::system::error_code&)>& operation, bool isUDP = false)
    {
        m_error = boost::system::error_code(); // clear error
        operation(m_error);
        if(Severity::Bad == getErrorSeverity(m_error, isUDP))
        {
            if(m_lastErrorCallback)
            {
                m_lastErrorCallback(m_error);
            }
            setLastErrorMessage(m_error.message());
            return false;
        }
        return true;
    }
private:
    boost::system::error_code m_error; // most recent error code
};

struct SocketResult
{
    size_t bytes; // Bytes read or written
    bool success; // was it successful? we could read/write *some* bytes but not all and then fail

    explicit operator bool() const
    {
        return success;
    }
};

class ISocket : public NetworkingBuffer, public NetworkingErrorHandler
{
public:
    using ReceivedCallback = std::function<bool(unsigned char*, size_t, bool)>;
    using ReceivedCallbackByte = std::function<void(unsigned char, size_t, bool)>;
    using AsyncSocketResult = std::shared_future<SocketResult>;

    ISocket(SocketType socketType, SocketMode socketMode, SocketRole socketRole, const std::string address, uint16_t port, uint32_t timeoutMs, uint32_t byteIntervalMs, size_t recvBufferSize, size_t sendBufferSize) // replace with chrono
    : NetworkingBuffer(recvBufferSize, sendBufferSize), m_address(address), m_port(port), m_timeoutMs(timeoutMs), m_byteIntervalMs(byteIntervalMs), m_mode(socketMode), m_type(socketType), m_role(socketRole)
    {
    }

    ISocket(SocketType socketType, SocketMode socketMode, SocketRole socketRole, const std::string address, uint16_t port, uint32_t timeoutMs = 20, uint32_t byteIntervalMs = 10)
    : ISocket(socketType, socketMode, socketRole, address, port, timeoutMs, byteIntervalMs, static_cast<size_t>(BufferSize::Default), static_cast<size_t>(BufferSize::Default))
    {
    }

    virtual bool open() = 0;
    
    virtual bool isOpen() const = 0;

    virtual std::string getSocketName() const = 0;
    
    std::string toV4()
    {
        // check if valid IPv4
        if(m_role == SocketRole::Client)
        {
            if (m_address == "localhost" || m_address == "")
            {
                return "127.0.0.1";
            }
        }
        else if(m_role == SocketRole::Server)
        {
            if (m_address == "localhost") // blank "" is permissible on servers
            {
                return "127.0.0.1";
            }
        }
        return m_address;
    }

    virtual void setAddress(const std::string& address)
    {
        throw std::bad_exception();
    }

    virtual void close()
    {
        m_ioService.stop();
        m_initialised = false;
    }
    
    AsyncSocketResult readData(unsigned char* data, size_t bufferSize)
    {
        auto task = std::async(std::launch::async, [this, data, bufferSize]()
        {
            return checkedReadData(data, bufferSize);
        });
        if(m_type == SocketType::Blocking)
        {
            task.wait();
        }
        return task.share();
    }
    AsyncSocketResult readDataCallback(const ReceivedCallback& recvCallback, size_t responseSize)
    {
        auto task = std::async(std::launch::async, [this, recvCallback, responseSize]()
        {
            return checkedReadDataCallback(recvCallback, responseSize);
        });
        if(m_type == SocketType::Blocking)
        {
            task.wait();
        }
        return task.share();
    }
    AsyncSocketResult writeData(unsigned char* data, size_t bufferSize)
    {
        auto task = std::async(std::launch::async, [this, data, bufferSize]()
        {
            return checkedWriteData(data, bufferSize);
        });
        if(m_type == SocketType::Blocking)
        {
            task.wait();
        }
        return task.share();
    }

    template<typename T, std::conditional_t<std::is_integral_v<std::remove_cv_t<T>> && !std::is_pointer_v<std::remove_cv_t<T>>, std::remove_cv_t<T>, void> = 0>
    AsyncSocketResult send(T value)
    {
        return writeData((unsigned char*)&value, sizeof(T));
    }
    
protected:
    // Implement an internal blocking function for both modes
    virtual SocketResult internalReadData(unsigned char* data, size_t bufferSize) = 0;
    virtual SocketResult internalWriteData(unsigned char* data, size_t bufferSize) = 0;

    SocketResult checkedReadData(unsigned char* data, size_t bufferSize)
    {
        std::lock_guard lock(m_socketMutex);
        if(CheckIsValid(SocketMode::Read, data, bufferSize) == false)
        {
            return {};
        }
        return internalReadData(data, bufferSize);
    }
    SocketResult checkedReadDataCallback(const ReceivedCallback& recvCallback, size_t responseSize)
    {
        std::lock_guard lock(m_socketMutex);
        auto& recvBuffer = getRecvBuffer();
        if(CheckIsValid(SocketMode::Read, recvBuffer.data(), recvBuffer.size()) == false)
        {
            return {};
        }
        auto result = internalReadData(recvBuffer.data(), recvBuffer.size());
        if (recvCallback != nullptr)
        {
            result.success &= recvCallback(recvBuffer.data(), result.bytes, (result.success && result.bytes >= responseSize));
        }
        return result;
    }
    SocketResult checkedWriteData(unsigned char* data, size_t bufferSize)
    {
        std::lock_guard lock(m_socketMutex);
        if(CheckIsValid(SocketMode::Write, data, bufferSize) == false)
        {
            return {};
        }
        return internalWriteData(data, bufferSize);
    }
    
    bool CheckIsValid(SocketMode expectedMode, unsigned char* data, size_t bufferSize)
    {
        if (!m_initialised)
        {
            setLastErrorMessage("Port was not initialised");
            return false;
        }
        if(m_mode != SocketMode::ReadWrite && expectedMode != m_mode)
        {
            static const std::map<SocketMode, std::string> modes
            {
                { SocketMode::Read, "Read" },
                { SocketMode::Write, "Write" }
            };
            std::stringstream modeStream;
            modeStream << "Cannot " << modes.at(expectedMode) << " data on a " << modes.at(m_mode) << " port";
            setLastErrorMessage(modeStream.str());
            return false;
        }
        if(!data)
        {
            setLastErrorMessage("No buffer provided");
            return false;
        }
        if(!bufferSize)
        {
            setLastErrorMessage("No buffer size provided");
            return false;
        }
        return true;
    }
    bool m_initialised = false;
    std::string m_address;
    uint16_t m_port;
    uint32_t m_timeoutMs;
    uint32_t m_byteIntervalMs;
    std::future<void> m_socketFuture;
    std::mutex m_socketMutex;
    std::atomic_bool m_readContinuously = false;
    SocketMode m_mode = SocketMode::Read;
    SocketType m_type = SocketType::Blocking;
    SocketRole m_role = SocketRole::Client;
    
    // Boost - general
    // sockets and endpoints should be per-type udp::socket for UDPSocket type
    io_service m_ioService;
};

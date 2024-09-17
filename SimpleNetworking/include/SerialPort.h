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

#ifdef _WIN32
#include <winbase.h>
namespace
{
    void FlushSerialReceive(boost::asio::serial_port& port)
    {
        auto err = PurgeComm(port.native_handle(), PURGE_RXCLEAR);
    }
}
#else
namespace
{
    void FlushSerialReceive(boost::asio::serial_port& port)
    {
        // \todo implement FlushSerialReceive for Linux
        assert(false && "FlushSerialReceive not implemented for Linux");
    }
}
#endif

class SerialPort : public ISocket
{
public:
    // Readability and to reduce repetition
    using BoostSerial = boost::asio::serial_port_base;
    using BaudRate = BoostSerial::baud_rate;
    using Parity = BoostSerial::parity;
    using CharSize = BoostSerial::character_size;
    using FlowCtrl = BoostSerial::flow_control;
    using StopBits = BoostSerial::stop_bits;

    SerialPort (SocketType type, SocketMode mode, uint16_t port, SocketRole role = DefaultRole) // for now SerialPort is client-only
    : ISocket(type, mode, role, "COM", port), m_serialPort(m_ioService)
    {
    }

    ~SerialPort() override
    {
        SerialPort::close();
    }

    bool open() override
    {
        close();
        auto baudRate = BaudRate(getBaudRate());
        auto parity = Parity(Parity::none);
        auto charSize = CharSize(8);
        auto flowControl = FlowCtrl(FlowCtrl::none);
        auto stopBits = StopBits(StopBits::one);
        internalOpen(getSocketName(), baudRate, parity, charSize, flowControl, stopBits);
        return m_initialised && isOpen();
    }

    
    virtual uint32_t getBaudRate()
    {
        return 57600;
    }

    bool isOpen() const override
    {
        return m_serialPort.is_open();
    }

    void close() override
    {
        /*if(m_readContinuously)
        {
            stopReadingContinuously();
        }*/
        if(isOpen())
        {
            boost::system::error_code err;
            m_serialPort.close(err);
            /// \todo Check error?
            ISocket::close();
        }
    }

    std::string getSocketName() const override
    {
        /// \todo could just turn this into a member since it can't change?
        static constexpr const char* serialPortConst = R"(\\.\)";
        return serialPortConst + m_address + std::to_string(m_port);
    }
    
    
protected:
    SocketResult internalReadData(unsigned char* data, size_t bufferSize) override
    {
        if(!m_readContinuously)
        {
            // if we are not reading continuously, we can flush
            FlushSerialReceive(m_serialPort);
        }
        // Blocking
        std::lock_guard lock(m_socketMutex);
        SocketResult result;
        result.success = CheckForError([&](auto& err)
        {
            result.bytes = m_serialPort.read_some(boost::asio::buffer(data, bufferSize), err);
        });
        return result;
    }
    
    SocketResult internalWriteData(unsigned char* data, size_t bufferSize) override
    {
        std::lock_guard lock(m_socketMutex);
        SocketResult result;
        result.success = CheckForError([&](auto& err)
        {
            result.bytes = boost::asio::write(m_serialPort, boost::asio::buffer(data, bufferSize), err);
        });
        if (result.bytes != bufferSize)
        {
            setLastErrorMessage("Write failed, wrote " + std::to_string(result.bytes) + " bytes, expected to write " + std::to_string(bufferSize));
        }
        return result;
    }
    
    void internalOpen(const std::string& deviceName,
        BaudRate baud_rate,
        Parity opt_parity,
        CharSize opt_csize,
        FlowCtrl opt_flow,
        StopBits opt_stop)
    {
        m_initialised = CheckForError([&](auto& err)
        {
            m_serialPort.open(deviceName, err);
           if (!err)
           {
               m_serialPort.set_option(baud_rate, err);
           }
           if (!err)
           {
               m_serialPort.set_option(opt_parity, err);
           }
           if (!err)
           {
               m_serialPort.set_option(opt_csize, err);
           }
           if (!err)
           {
               m_serialPort.set_option(opt_flow, err);
           }
           if (!err)
           {
               m_serialPort.set_option(opt_stop, err);
           }
        });
    }
    
    serial_port m_serialPort;

};

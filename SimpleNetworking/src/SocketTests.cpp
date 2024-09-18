//
//          Created by Riki Lowe on 17/09/2024.
//
//              Copyright Riki Lowe 2024
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#pragma optimize("", off)

#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"

#include <thread>
#include <chrono>
#include <iostream>

#include "../include/SerialPort.h"
#include "../include/UDPSocket.h"

#define SERIAL_PORT_RUNNING 0

#define SERIAL_PORT_READ 1
#define SERIAL_PORT_WRITE 2

#define UDP_PORT 10015

void PrepareReadWriteSerialPorts(std::shared_ptr<SerialPort>& readPort, std::shared_ptr<SerialPort>& writePort, SocketType type)
{
    readPort = std::make_shared<SerialPort>(type, SocketMode::Read, SERIAL_PORT_READ);
    writePort = std::make_shared<SerialPort>(type, SocketMode::Write, SERIAL_PORT_WRITE);
    REQUIRE_FALSE(readPort->isOpen());
    REQUIRE_FALSE(writePort->isOpen());
    REQUIRE(readPort->open());
    REQUIRE(writePort->open());
    REQUIRE(readPort->isOpen());
    REQUIRE(writePort->isOpen());
}

template<typename PortType, typename PortType2>
void StopReadWritePorts(const std::shared_ptr<PortType>& readPort, const std::shared_ptr<PortType2>& writePort)
{
    REQUIRE(readPort->getLastErrorMessage().empty());
    REQUIRE(writePort->getLastErrorMessage().empty());
    readPort->close();
    writePort->close();
    REQUIRE_FALSE(readPort->isOpen());
    REQUIRE_FALSE(writePort->isOpen());
}

void PrepareReadWriteUDPPorts(std::shared_ptr<UDPServer>& readPort, std::shared_ptr<UDPClient>& writePort, SocketType type)
{
    readPort = std::make_shared<UDPServer>(type, "", UDP_PORT);
    writePort = std::make_shared<UDPClient>(type, "", UDP_PORT);
    REQUIRE_FALSE(readPort->isOpen());
    REQUIRE(readPort->open());
    REQUIRE_FALSE(writePort->isOpen());
    REQUIRE(writePort->open());
}


std::string Data = "Hello World!";
auto successCallback = [](unsigned char* buffer, size_t size, bool valid)
{
    if(!valid)
    {
        REQUIRE(false);
    }
    REQUIRE((std::string((char*)buffer) == Data));
    return true;
};

#if SERIAL_PORT_RUNNING

TEST_CASE("Do blocking serial ports work?", "[sockets]")
{
    std::shared_ptr<SerialPort> readPort, writePort;
    PrepareReadWriteSerialPorts(readPort, writePort, SocketType::Blocking);
    std::mutex m_readMutex;
    std::unique_lock m_readLock(m_readMutex);
    std::condition_variable m_readCondition;
    // Blocking read and write at the same time doesn't work (obviously) so just shunt the readPort onto another thread
    std::thread* readThread = new std::thread([&m_readCondition, readPort]()
    {
        REQUIRE(readPort->readDataCallback(successCallback, Data.length()).get());
        REQUIRE(readPort->getLastErrorMessage().empty());
        m_readCondition.notify_one();
    });
    using namespace std::chrono;
    std::this_thread::sleep_for(1s);
    REQUIRE(writePort->writeData((unsigned char*)Data.data(), Data.length()).get());
    m_readCondition.wait(m_readLock);
    readThread->join();
    delete readThread;
    StopReadWritePorts(readPort, writePort);
}

TEST_CASE("Do non-blocking serial ports work?", "[sockets]")
{
    std::shared_ptr<SerialPort> readPort, writePort;
    PrepareReadWriteSerialPorts(readPort, writePort, SocketType::NonBlocking);
    auto readPortFuture = readPort->readDataCallback(successCallback, Data.length());
    using namespace std::chrono;
    std::this_thread::sleep_for(1s);
    auto writePortFuture = writePort->writeData((unsigned char*)Data.data(), Data.length());
    REQUIRE(writePortFuture.get());
    REQUIRE(readPortFuture.get());
    StopReadWritePorts(readPort, writePort);
}

TEST_CASE("Can we write on Read Serial Ports and vice versa?", "[sockets]")
{
    std::shared_ptr<SerialPort> readPort, writePort;
    PrepareReadWriteSerialPorts(readPort, writePort, SocketType::Blocking);
    // Everything should be open / working at this point
    // However, we shouldn't be able to writeData on a readPort and vice versa
    REQUIRE_FALSE(readPort->writeData((unsigned char*)Data.data(), Data.length()).get());
    REQUIRE_FALSE(readPort->getLastErrorMessage().empty());
    REQUIRE_FALSE(writePort->readDataCallback([&](unsigned char* buffer, size_t size, bool valid)
    {
        if(valid)
        {
            REQUIRE(false);
        }
        return true; // we can return true and still pass the REQUIRE_FALSE because reading the actual bytes will have failed
    }, Data.length()).get()); // don't bother saving the Future, just call get()
    // We want to make sure an error was logged
    REQUIRE_FALSE(writePort->getLastErrorMessage().empty());
    readPort->close();
    writePort->close();
    REQUIRE_FALSE(readPort->isOpen());
    REQUIRE_FALSE(writePort->isOpen());
}

TEST_CASE("Can we use uninitialised Serial ports?", "[sockets]")
{
    auto readPort = std::make_shared<SerialPort>(SocketType::Blocking, SocketMode::Read, SERIAL_PORT_READ);
    auto writePort = std::make_shared<SerialPort>(SocketType::Blocking, SocketMode::Write, SERIAL_PORT_WRITE);
    // Don't open the ports and don't provide buffers
    REQUIRE_FALSE(readPort->readData(nullptr, 0).get());
    REQUIRE_FALSE(writePort->writeData(nullptr, 0).get());
    // Open ports but still don't provide buffers
    REQUIRE(readPort->open());
    REQUIRE(writePort->open());
    REQUIRE_FALSE(readPort->readData(nullptr, 0).get());
    REQUIRE_FALSE(writePort->writeData(nullptr, 0).get());
}

#endif

TEST_CASE("Do blocking UDP ports work?", "[sockets]")
{
    std::shared_ptr<UDPServer> readPort;
    std::shared_ptr<UDPClient> writePort;
    PrepareReadWriteUDPPorts(readPort, writePort, SocketType::Blocking);
    std::mutex m_readMutex;
    std::unique_lock m_readLock(m_readMutex);
    std::condition_variable m_readCondition;
    // Blocking read and write at the same time doesn't work (obviously) so just shunt the readPort onto another thread
    std::thread* readThread = new std::thread([&m_readCondition, &readPort]()
    {
        REQUIRE(readPort->readDataCallback(successCallback, Data.length()).get());
        REQUIRE(readPort->getLastErrorMessage().empty());
        m_readCondition.notify_one();
    });
    using namespace std::chrono;
    std::this_thread::sleep_for(5s);
    REQUIRE(writePort->writeData((unsigned char*)Data.data(), Data.length()).get());
    m_readCondition.wait(m_readLock);
    readThread->join();
    StopReadWritePorts(readPort, writePort);
}

TEST_CASE("Do non-blocking UDP ports work?", "[sockets]")
{
    std::shared_ptr<UDPServer> readPort;
    std::shared_ptr<UDPClient> writePort;
    PrepareReadWriteUDPPorts(readPort, writePort, SocketType::NonBlocking);
    auto readPortFuture = readPort->readDataCallback(successCallback, Data.length());
    using namespace std::chrono;
    std::this_thread::sleep_for(5s);
    auto writePortFuture = writePort->writeData((unsigned char*)Data.data(), Data.length());
    REQUIRE(writePortFuture.get());
    REQUIRE(readPortFuture.get());
    StopReadWritePorts(readPort, writePort);
}

TEST_CASE("Can we write on Read UDP Ports and vice versa?", "[sockets]")
{
    std::shared_ptr<UDPServer> readPort;
    std::shared_ptr<UDPClient> writePort;
    PrepareReadWriteUDPPorts(readPort, writePort, SocketType::Blocking);
    // Everything should be open / working at this point
    // However, we shouldn't be able to writeData on a readPort and vice versa
    REQUIRE_FALSE(readPort->writeData((unsigned char*)Data.data(), Data.length()).get());
    REQUIRE_FALSE(readPort->getLastErrorMessage().empty());
    REQUIRE_FALSE(writePort->readDataCallback([&](unsigned char* buffer, size_t size, bool valid)
    {
        if(valid)
        {
            REQUIRE(false);
        }
        return true; // we can return true and still pass the REQUIRE_FALSE because reading the actual bytes will have failed
    }, Data.length()).get()); // don't bother saving the Future, just call get()
    // We want to make sure an error was logged
    REQUIRE_FALSE(writePort->getLastErrorMessage().empty());
    readPort->close();
    writePort->close();
    REQUIRE_FALSE(readPort->isOpen());
    REQUIRE_FALSE(writePort->isOpen());
}

TEST_CASE("Can we use uninitialised UDP ports?", "[sockets]")
{
    auto readPort = std::make_shared<UDPServer>(SocketType::Blocking, "", UDP_PORT);
    auto writePort = std::make_shared<UDPClient>(SocketType::Blocking, "", UDP_PORT);
    // Don't open the ports and don't provide buffers
    REQUIRE_FALSE(readPort->readData(nullptr, 0).get());
    REQUIRE_FALSE(writePort->writeData(nullptr, 0).get());
    // Open ports but still don't provide buffers
    REQUIRE(readPort->open());
    REQUIRE(writePort->open());
    REQUIRE_FALSE(readPort->readData(nullptr, 0).get());
    REQUIRE_FALSE(writePort->writeData(nullptr, 0).get());
}

#pragma optimize("", on)
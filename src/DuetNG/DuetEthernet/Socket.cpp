/*
 * Socket.cpp
 *
 *  Created on: 25 Dec 2016
 *      Author: David
 */

#include "Socket.h"

#include "NetworkTransaction.h"
#include "NetworkBuffer.h"
#include "socketlib.h"

//***************************************************************************************************
// Socket class

Socket::Socket() : currentTransaction(nullptr), receivedData(nullptr)
{
}

// Initialise a TCP socket
void Socket::Init(SocketNumber skt, Port serverPort)
{
	socketNum = skt;
	localPort = serverPort;
	ReInit();
}

void Socket::ReInit()
{
	// Discard any received data
	while (receivedData != nullptr)
	{
		receivedData = receivedData->Release();
	}
	ReleaseTransaction();

	persistConnection = true;
	isTerminated = false;
	isSending = false;
	state = SocketState::inactive;

	// Re-initialise the socket on the W5500
//debugPrintf("About to initialise socket %u\n", socketNum);
	socket(socketNum, Sn_MR_TCP, localPort, 0x00);
//debugPrintf("Initialised socket %u\n", socketNum);
}

// Close a connection when the last packet has been sent
void Socket::Close()
{
	disconnectNoWait(socketNum);
}

// Release the current transaction
void Socket::ReleaseTransaction()
{
	if (currentTransaction != nullptr)
	{
		currentTransaction->Release();
		currentTransaction = nullptr;
	}
}

// Terminate a connection immediately
void Socket::Terminate()
{
	disconnectNoWait(socketNum);
	isTerminated = true;
	state = SocketState::inactive;
	while (receivedData != nullptr)
	{
		receivedData = receivedData->Release();
	}
	ReleaseTransaction();
}

// Test whether we have a connection on this socket
bool Socket::IsConnected() const
{
	const uint8_t stat = getSn_SR(socketNum);
	return stat == SOCK_ESTABLISHED || stat == SOCK_CLOSE_WAIT;
}

// Return true if there is or may soon be more data to read
bool Socket::HasMoreDataToRead() const
{
	return (receivedData != nullptr && receivedData->TotalRemaining() != 0)		// already have more data
		|| getSn_SR(socketNum) == SOCK_ESTABLISHED;								// still fully connected, so we may receive more
}

bool Socket::CanWrite() const
{
	const uint8_t stat = getSn_SR(socketNum);
	return stat == SOCK_ESTABLISHED || stat == SOCK_CLOSE_WAIT;
}

// Return true if we are in the sending phase
bool Socket::IsSending() const
{
	return currentTransaction != nullptr && currentTransaction->IsSending();
}

// Read 1 character from the receive buffers, returning true if successful
bool Socket::ReadChar(char& c)
{
	while (receivedData != nullptr && receivedData->IsEmpty())
	{
		receivedData = receivedData->Release();		// discard empty buffer at head of chain
	}

	if (receivedData == nullptr)
	{
		c = 0;
		return false;
	}

	bool ret = receivedData->ReadChar(c);
	if (receivedData->IsEmpty())
	{
		receivedData = receivedData->Release();
	}
	return ret;
}

// Return a pointer to data in a buffer and a length available, and mark the data as taken
bool Socket::ReadBuffer(const char *&buffer, size_t &len)
{
	while (receivedData != nullptr && receivedData->IsEmpty())
	{
		receivedData = receivedData->Release();		// discard empty buffer at head of chain
	}

	if (receivedData == nullptr)
	{
		return false;
	}

	len = NetworkBuffer::bufferSize;				// initial value passed to TakeData is the maximum amount we will take
	buffer = reinterpret_cast<const char*>(receivedData->TakeData(len));
//	debugPrintf("Taking %d bytes\n", len);
	return true;
}

// Poll a socket to see if it needs to be serviced
void Socket::Poll(bool full)
{
	switch(getSn_SR(socketNum))
	{
	case SOCK_INIT:
		// Socket has been initialised but is not listening yet
		if (localPort != 0)			// localPort for the FTP data socket is 0 until we have decided what port number to use
		{
			ExecCommand(socketNum, Sn_CR_LISTEN);
		}
		break;

	case SOCK_LISTEN:				// Socket is listening but no client has connected to it yet
		break;

	case SOCK_ESTABLISHED:			// A client is connected to this socket
		if (getSn_IR(socketNum) & Sn_IR_CON)
		{
			// New connection, so retrieve the sending IP address and port, and clear the interrupt
			getSn_DIPR(socketNum, reinterpret_cast<uint8_t*>(&remoteIPAddress));
			remotePort = getSn_DPORT(socketNum);
			setSn_IR(socketNum, Sn_IR_CON);
		}

		// See if the socket has received any data
		{
			const uint16_t len = getSn_RX_RSR(socketNum);
			if (len != 0)
			{
//				debugPrintf("%u available\n", len);
				// There is data available, so allocate a buffer
				NetworkBuffer * const buf = NetworkBuffer::Allocate();
				if (buf != nullptr)
				{
					const int32_t ret = recv(socketNum, buf->Data(), min<size_t>(len, NetworkBuffer::bufferSize));
//					debugPrintf("ret %d\n", ret);
					if (ret > 0)
					{
						buf->dataLength = (size_t)ret;
						buf->readPointer = 0;
						NetworkBuffer::AppendToList(&receivedData, buf);
					}
					else
					{
						buf->Release();
//						debugPrintf("Bad receive, code = %d\n", ret);
					}
				}
				else debugPrintf("no buffer\n");
			}

			if (receivedData != nullptr && currentTransaction == nullptr)
			{
				currentTransaction = NetworkTransaction::Allocate();
				if (currentTransaction != nullptr)
				{
					currentTransaction->Set(this, TransactionStatus::receiving);
				}
			}
		}

		// See if we can send any data.
		// Currently we don't send if we are being called from hsmci because we don't want to risk releasing a buffer that we may be reading data into.
		// We could use a buffer locking mechanism instead. However, the speed of sending is not critical, so we don't do that yet.
		if (full && IsSending() && TrySendData())
		{
			ReleaseTransaction();
			ExecCommand(socketNum, Sn_CR_DISCON);
		}
		break;

	case SOCK_CLOSE_WAIT:			// A client has asked to disconnect
#ifdef _HTTPSERVER_DEBUG_
		printf("> HTTPSocket[%d] : ClOSE_WAIT\r\n", socketNum);
#endif
		if (!IsSending() || TrySendData())
		{
			ReleaseTransaction();
			ExecCommand(socketNum, Sn_CR_DISCON);
		}
		break;

	case SOCK_CLOSED:
#ifdef _HTTPSERVER_DEBUG_
		printf("> HTTPSocket[%d] : CLOSED\r\n", s);
#endif
		if (socket(socketNum, Sn_MR_TCP, localPort, 0x00) == socketNum)    // Reinitialize the socket
		{
#ifdef _HTTPSERVER_DEBUG_
			printf("> HTTPSocket[%d] : OPEN\r\n", socketNum);
#endif
		}
		break;

	default:
		break;
	} // end of switch

#ifdef _USE_WATCHDOG_
	HTTPServer_WDT_Reset();
#endif
}

// Try to send data, returning true if all data has been sent and we ought to close the socket
// We have already checked that the socket is in the ESTABLISHED or CLOSE_WAIT state.
bool Socket::TrySendData()
{
	if (isSending)									// are we already sending?
	{
		const uint8_t tmp = getSn_IR(socketNum);
		if (tmp & Sn_IR_SENDOK)						// did the previous send complete?
		{
			setSn_IR(socketNum, Sn_IR_SENDOK);		// if yes
			isSending = false;
		}
		else if(tmp & Sn_IR_TIMEOUT)				// did it time out?
		{
			isSending = false;
			disconnectNoWait(socketNum);			// if so, close the socket
			return true;							// and release buffers etc.
		}
		else
		{
			//debugPrintf("Waiting for data to be sent\n");
			return false;							// last send is still in progress
		}
	}

	// Socket is free to send
	if (currentTransaction->GetStatus() == TransactionStatus::finished)
	{
		return true;
	}

	size_t freesize = (size_t)getSn_TX_FSR(socketNum);
	uint16_t ptr = getSn_TX_WR(socketNum);
	bool sent = false;
	while (freesize != 0)
	{
		size_t length = freesize;
		const uint8_t *data = currentTransaction->GetDataToSend(length);
		if (data == nullptr)
		{
			break;									// no more data or can't allocate a file buffer
		}
		//debugPrintf("rp=%04x tp=%04x\n", getSn_TX_RD(socketNum), getSn_TX_WR(socketNum));
		wiz_send_data_at(socketNum, data, length, ptr);
		//debugPrintf("Wrote %u bytes of %u free, %02x %02x %02x\n", length, freesize, data[0], data[1], data[2]);
		freesize -= length;
		ptr += (uint16_t)length;
		sent = true;
	}
	if (sent)
	{
		//debugPrintf("Sending data, rp=%04x tp=%04x\n", getSn_TX_RD(socketNum), getSn_TX_WR(socketNum));
		ExecCommand(socketNum, Sn_CR_SEND);
		isSending = true;
	}
	else if (currentTransaction->GetStatus() == TransactionStatus::finished)
	{
		return true;								// there was no more data to send
	}

	return false;
}

// Discard any received data for this transaction
void Socket::DiscardReceivedData()
{
	while (receivedData != nullptr)
	{
		receivedData = receivedData->Release();
	}
}

// End

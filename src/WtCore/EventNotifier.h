/*!
 * \file EventCaster.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief UDP�㲥������
 */
#pragma once

#include <boost/asio.hpp>
#include <queue>

#include "../Includes/WTSMarcos.h"
#include "../Includes/WTSObject.hpp"
#include "../Share/StdUtils.hpp"


NS_OTP_BEGIN
class WTSTradeInfo;
class WTSOrderInfo;
class WTSVariant;

class EventNotifier
{
public:
	EventNotifier();
	~EventNotifier();

	typedef boost::asio::ip::udp::endpoint	EndPoint;
	typedef std::vector<EndPoint>			ReceiverList;

private:
	void	handle_send_broad(const EndPoint& ep, const boost::system::error_code& error, std::size_t bytes_transferred); 
	void	handle_send_multi(const EndPoint& ep, const boost::system::error_code& error, std::size_t bytes_transferred); 

	void	notify(const char* trader, const std::string& data, uint32_t dataType);

	void	tradeToJson(uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo, std::string& output);
	void	orderToJson(uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo, std::string& output);

public:
	bool	init(WTSVariant* cfg);
	void	start();
	void	stop();

	bool	addBRecver(const char* remote, int port);
	bool	addMRecver(const char* remote, int port, int sendport);

	void	notify(const char* trader, uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo);
	void	notify(const char* trader, uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo);

	void	notify(const char* trader, const std::string& message);

private:
	typedef boost::asio::ip::udp::socket	UDPSocket;
	typedef std::shared_ptr<UDPSocket>	UDPSocketPtr;

	enum 
	{ 
		max_length = 2048 
	};

	boost::asio::ip::udp::endpoint	m_senderEP;
	char			m_data[max_length];

	std::string		m_strGroupTag;
	bool			m_bReady;

	//�㲥
	ReceiverList	m_listRawRecver;
	UDPSocketPtr	m_sktBroadcast;

	typedef std::pair<UDPSocketPtr,EndPoint>	MulticastPair;
	typedef std::vector<MulticastPair>	MulticastList;
	MulticastList	m_listRawGroup;
	boost::asio::io_service		m_ioservice;
	StdThreadPtr	m_thrdIO;

	StdThreadPtr	m_thrdCast;
	StdCondVariable	m_condCast;
	StdUniqueMutex	m_mtxCast;
	bool			m_bTerminated;

	typedef struct _NotifyData
	{
		std::string	_trader;
		uint32_t	_datatype;
		std::string	_data;

		_NotifyData(const char* trader, const std::string& data, uint32_t dataType = 0)
			: _data(data), _datatype(dataType), _trader(trader)
		{
		}
	} NotifyData;

	std::queue<NotifyData>		m_dataQue;
};

NS_OTP_END
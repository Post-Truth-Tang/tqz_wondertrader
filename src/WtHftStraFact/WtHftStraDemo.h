#pragma once
#include <unordered_set>
#include <memory>
#include <thread>
#include <mutex>
#include <iostream>

#include <time.h>
#include <string.h>
#include <vector>
#include <sstream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include "../Includes/HftStrategyDefs.h"
#include "../Includes/WTSTradeDef.hpp"

#include "../Share/JsonToVariant.hpp"
#include "../Share/BoostFile.hpp"


#include "../../include/rapidjson/document.h"
#include "../../include/rapidjson/writer.h"
#include "../../include/rapidjson/stringbuffer.h"
#include "../../include/rapidjson/filewritestream.h"
#include "../../include/rapidjson/prettywriter.h"
#include "../../include/rapidjson/filereadstream.h"

#include "TQZHftAlphas/TQZHftMa.h"
#include "TQZHftAlphas/TQZHftMacd.h"
#include "TQZHftAlphas/TQZHftDiff.h"
#include "TQZHftAlphas/TQZHftLastTradedQuantity.h"
#include "TQZHftAlphas/TQZHftBook.h"


using namespace boost::gregorian;
using namespace std;
using namespace hftalphas;


enum TQZOrderType {
	DEFAULT_ORDER_TYPE,
	NO_TYPE,

	BUY_TYPE,
	SELL_TYPE,
	SHORT_TYPE,
	COVER_TYPE
};

enum TQZSessionStatus {
	DEFAULT_STATUS,

	RE_CONNECT_STATUS,

	CLOSE_POSTIONS_STATUS,
	MARKET_MAKING_STATUS
};

enum TQZLogFileType {
	TRADE_CHANGE_TYPE,
	CANCEL_ORDER_COUNTS_TYPE
};

struct TQZLogMessage {

	TQZLogMessage() {};
	
	TQZLogMessage(std::string code, uint32_t orderid, double currentPrice, double orderPrice, std::string orderType, std::string currentMarketTime, std::string sendOrderTime, std::string orderComment)
		: code(code)
		, orderid(orderid)
		, currentPrice(currentPrice)
		, orderPrice(orderPrice)
		, orderType(orderType)
		, currentMarketTime(currentMarketTime)
		, sendOrderTime(sendOrderTime)
		, orderComment(orderComment)
	{
	}

	string code;

	uint32_t orderid;

	double currentPrice;
	double orderPrice;

	std::string orderType;
	std::string currentMarketTime;
	std::string sendOrderTime;

	std::string orderComment;
};


class WtHftStraDemo : public HftStrategy {
public:
	WtHftStraDemo(const char* id);
	~WtHftStraDemo();

public:
	virtual const char* getName() override;

	virtual const char* getFactName() override;

	virtual bool init(WTSVariant* cfg) override;

	virtual void on_init(HftStraBaseCtx* ctx) override;

	virtual void on_tick(HftStraBaseCtx* ctx, const char* code, WTSTickData* newTick) override;

	virtual void on_bar(HftStraBaseCtx* ctx, const char* code, const char* period, uint32_t times, WTSBarStruct* newBar) override;

	virtual void on_trade(HftStraBaseCtx* ctx, uint32_t localid, const char* stdCode, bool isBuy, double qty, double price, const char* userTag) override;

	virtual void on_order(HftStraBaseCtx* ctx, uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag) override;

	virtual void on_position(HftStraBaseCtx* ctx, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail) override;

	virtual void on_channel_ready(HftStraBaseCtx* ctx) override;

	virtual void on_channel_lost(HftStraBaseCtx* ctx) override;

	virtual void on_entrust(uint32_t localid, bool bSuccess, const char* message, const char* userTag) override;

private:
	typedef std::unordered_set<uint32_t> IDSet;
	typedef std::map<uint32_t, TQZLogMessage> MessageMap;
	typedef std::vector<WTSTickData> Ticks;
	typedef std::unique_ptr<Ticks> TicksUPtr;

	WTSTickData*	_last_tick;
	HftStraBaseCtx*	_ctx;

	std::string		_code;
	std::string		_tradeChange_log_filename;

	uint32_t		_marketMaking_scan_interval;

	uint32_t		_long_order_offset;
	uint32_t		_short_order_offset;

	uint32_t		_offset_open_minutes;
	uint32_t		_offset_close_minutes;

	IDSet			_code_buy_orders;
	IDSet			_code_short_orders;
	IDSet			_code_sell_orders;
	IDSet			_code_cover_orders;

	uint32_t		_code_buy_order;
	uint32_t		_code_short_order;
	uint32_t		_code_sell_order;
	uint32_t		_code_cover_order;

	uint64_t		_last_scan_time;
	double			_last_mid_price;

	bool			_channel_ready;
	bool			_re_connect_ready;

	bool			_code_buy_lock;
	bool			_code_sell_lock;
	bool			_code_short_lock;
	bool			_code_cover_lock;

	bool			_re_marketMaking;

	bool			_record_hft_log;

	uint32_t		_cancel_limit_counts;

	rapidjson::Document _sessionsDocument;
	rapidjson::Document _commoditiesDocument;

	TQZSessionStatus _current_session_status;

	MessageMap _log_message_map;

	TicksUPtr _ticks_uptr;


private:
	/// cancel order part.
	void __tqz_cancelOrder(const std::string code, const uint32_t orderId);

	void __tqz_cancelOrders(const std::string code, const IDSet ordersIds);
	
	void __tqz_cancelAllOrders();

	
	/// get askPrice | bidPrice | midPrice | upperlimit | lowerlimit | orderType.
	double __tqz_getAskPrice(const std::string code);
	
	double __tqz_getBidPrice(const std::string code);

	double __tqz_getCurrentMidPrice(const std::string code);

	double __tqz_getLongPrice(const std::string code, const int offsetTicks);

	double __tqz_getShortPrice(const std::string code, const int offsetTicks);

	double __tqz_getUpperlimitPrice(const std::string code);

	double __tqz_getLowerlimitPrice(const std::string code);

	double __tqz_getMarketMakingLongPrice(const std::string code, const double currentMidPrice, const int offsetTicks);

	double __tqz_getMarketMakingShortPrice(const std::string code, const double currentMidPrice, const int offsetTicks);
	
	bool __isBeyondUpperlimitOrLowerlimit(const string code, const int longOrderOffsetTicks, const int shortOrderOffsetTicks);
	
	TQZOrderType __tqz_getOrderType(const uint32_t orderId);

	bool __isBelongToHft(const uint32_t orderId);

	bool __midPriceIsChange(int offsetTicks);


	/// session part.
	void __initSessionStatus(TQZSessionStatus sessionStatus);

	int __tqz_getCurrentHourMinute();
	
	rapidjson::Document __tqz_loadJsonDocument(const std::string jsonPath);
	
	std::string __tqz_getSession(const std::string symbolCode);
	
	bool __isTradingTime(const std::string sessionString);

	bool __isEntrustable(std::string code);
	
	bool __isClosePositionsTime(const std::string sessionString);

	void __tqz_receiveCodeOnlyCloseCode(const uint32_t localid, const std::string code, const double lots);

	int __tqz_resetToHourMinute(int toHourMinute, const int offsetCloseMinutes);

	/// log part.
	std::string __tqz_getLogFileName(const TQZLogFileType logfileType);

	std::string __tqz_getCurrentTime();

	std::string __tqz_getCurrentMarketTime();

	std::string __tqz_getTimeString(const uint64_t hours, const uint64_t minutes, const uint64_t seconds, const uint64_t milliseconds);

	void __tqz_writeStrategyTradeLog(const uint32_t orderid, const double receiveTradePrice, const double lots);

	void __tqz_writeCancelOrderCountsLog(const std::string code);

	TQZLogMessage __tqz_getNewLogMessage(std::string code, uint32_t orderid, double currentPrice, double orderPrice, char* orderType, std::string currentMarketTime, std::string sendOrderTime, std::string orderComment);


	/// re connect part.
	void tqz_doReconnect();

	bool __tqz_queryReconnectResult();


	/// scan interval part.
	uint64_t __getCurrentTimestamp();

	bool __isNewScanInterval(const uint32_t scanInterval);


	/// tqz send order part. (can't use under market making condition)
	void tqz_buy(const std::string code, const double lots, const int offsetTicks, const std::string orderComment);
	void tqz_sell(const std::string code, const double lots, const int offsetTicks, const std::string orderComment);
	void tqz_short(const std::string code, const double lots, const int offsetTicks, const std::string orderComment);
	void tqz_cover(const std::string code, const double lots, const int offsetTicks, const std::string orderComment);

	void tqz_marketMaking(const int codeLongOffsetTicks, const int codeShortOffsetTicks);

	void tqz_closePositions(const std::string code, uint32_t closePositionsOffsetTicks);


	/// lock & unlock order part
	void __lockBuy();
	void __lockSell();
	void __lockShort();
	void __lockCover();

	void __unlockBuy();
	void __unlockSell();
	void __unlockShort();
	void __unlockCover();

	void __unlockAllOrders();

	bool __closeCodeIsLock(bool reSendLockOrder);

	bool __marketMakingIsLock(bool cancelLockOrder);

	bool __isMarketMakingAble();


	/// clear session cache.
	void __tqz_clearPreviousSessionCache();

	
	/// hft alpha part.
	int32_t totalAlphasValue(TicksUPtr& ticksUPtr);

	uint32_t getLongOffsetValue();
	uint32_t getShortOffsetValue();

	bool updateTicks(WTSTickData tickData);
};

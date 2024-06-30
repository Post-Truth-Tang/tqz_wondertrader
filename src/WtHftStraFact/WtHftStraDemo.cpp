#include "WtHftStraDemo.h"
//#include "../Includes/IHftStraCtx.h"
#include "../WtCore/HftStraBaseCtx.h"

#include "../Includes/WTSVariant.hpp"
#include "../Includes/WTSDataDef.hpp"
#include "../Includes/WTSContractInfo.hpp"

#include "../Share/TimeUtils.hpp"
#include "../Share/decimal.h"


extern const char* FACT_NAME;

/// alpha params
#define OPEN_CLOSE_POSITION_OFFSET_TICKS 2
#define UNIT 1
#define MIDPRICE_OFFSET_TICKS 1
#define ALPHA_TICK_COUNTS 600


WtHftStraDemo::WtHftStraDemo(const char* id): HftStrategy(id)
	, _last_tick(nullptr)
	, _last_scan_time(0)
	, _channel_ready(false)
	, _re_connect_ready(false)
	, _last_mid_price(-1)
	, _current_session_status(DEFAULT_STATUS)
	, _code_buy_lock(false)
	, _code_sell_lock(false)
	, _code_short_lock(false)
	, _code_cover_lock(false)
	, _re_marketMaking(true)
	, _ticks_uptr(make_unique<Ticks>())
{
	this->_commoditiesDocument = this->__tqz_loadJsonDocument("commodities.json");
	this->_sessionsDocument = this->__tqz_loadJsonDocument("sessions.json");
}

WtHftStraDemo::~WtHftStraDemo() {
	if (_last_tick)
		_last_tick->release();
	
	this->_ticks_uptr.reset();

	this->__tqz_cancelAllOrders();
}

const char* WtHftStraDemo::getName() {
	return "HftDemoStrategy";
}

const char* WtHftStraDemo::getFactName() {
	return FACT_NAME;
}

bool WtHftStraDemo::init(WTSVariant* cfg) {
	this->_code = cfg->getCString("code");

	this->_marketMaking_scan_interval = cfg->getUInt32("market_making_scan_interval");

	this->_long_order_offset = cfg->getUInt32("long_order_offset");
	this->_short_order_offset = cfg->getUInt32("short_order_offset");

	this->_offset_open_minutes = cfg->getUInt32("offset_open_minutes");
	this->_offset_close_minutes = cfg->getUInt32("offset_close_minutes");

	this->_record_hft_log = cfg->getBoolean("record_hft_log");

	this->_cancel_limit_counts = cfg->getUInt32("cancel_limit_counts");

	return true;
}

void WtHftStraDemo::on_init(HftStraBaseCtx* ctx) {

	ctx->stra_sub_ticks(_code.c_str());
	
	this->_ctx = ctx;
}

void WtHftStraDemo::on_tick(HftStraBaseCtx* ctx, const char* code, WTSTickData* newTick) {
	if (0 != this->_code.compare(code))
		return;
	if (!this->__isTradingTime(code))
		return;
	if (!this->_channel_ready)
		return;
	if (!this->_re_connect_ready) {
		if (!this->__isNewScanInterval(this->_marketMaking_scan_interval))
			return;

		/// query re_connect_ready & do reconnect.
		this->_re_connect_ready = this->__tqz_queryReconnectResult();
		if (!this->_re_connect_ready)
			this->tqz_doReconnect();

		return;
	}
	if (!this->updateTicks(*newTick))
		return;

	
	if (this->__isClosePositionsTime(code)) {
		if (this->_current_session_status != CLOSE_POSTIONS_STATUS)
			this->__initSessionStatus(CLOSE_POSTIONS_STATUS);
		if (!this->__isNewScanInterval(this->_marketMaking_scan_interval))
			return;

		if (this->__marketMakingIsLock(true))
			return;
		if (this->__closeCodeIsLock(true))
			return;

		this->tqz_closePositions(this->_code, OPEN_CLOSE_POSITION_OFFSET_TICKS);
	} else {
		if (this->_current_session_status != MARKET_MAKING_STATUS)
			this->__initSessionStatus(MARKET_MAKING_STATUS);

		if (this->__closeCodeIsLock(true))
			return;

		/// scan only new time slice.
		if (!this->__isNewScanInterval(this->_marketMaking_scan_interval))
			return;
		if (!this->__midPriceIsChange(MIDPRICE_OFFSET_TICKS))
			return;
		
		/// do market making with time slice.
		if (this->_re_marketMaking) {
			this->tqz_marketMaking(this->getLongOffsetValue(), this->getShortOffsetValue());
			this->_re_marketMaking = !this->_re_marketMaking;
		} else {
			this->__marketMakingIsLock(true);
		}
		
	}
}


int32_t WtHftStraDemo::totalAlphasValue(TicksUPtr& ticksUPtr) {
	int32_t maAlphaValue = MaAlpha::alphaValue(ticksUPtr, 20);
	/*int32_t macdAlphaValue = MacdAlpha::alphaValue(ticksUPtr, 9, 12, 26);
	int32_t diffAlphaValue = DiffAlpha::alphaValue(ticksUPtr, 20);
	int32_t lastTradedQuantityAlphaValue = LastTradedQuantityAlpha::alphaValue(ticksUPtr, 20);
	int32_t bookAlphaValue = BookAlpha::alphaValue(ticksUPtr, 20);*/

	/*
	/// output all values to terminal for test...
	std::cout << "-------------------------------------------" << std::endl;
	//std::cout << "maAlphaValue: " << maAlphaValue << std::endl;
	std::cout << "macdAlphaValue: " << macdAlphaValue << std::endl;
	//std::cout << "diffAlphaValue: " << diffAlphaValue << std::endl;
	std::cout << "lastTradedQuantityAlphaValue: " << lastTradedQuantityAlphaValue << std::endl;
	//std::cout << "bookAlphaValue: " << bookAlphaValue << std::endl;
	std::cout << "-------------------------------------------" << std::endl;
	*/

	//return (maAlphaValue + macdAlphaValue + diffAlphaValue + lastTradedQuantityAlphaValue + bookAlphaValue);
	return maAlphaValue;
}

uint32_t WtHftStraDemo::getLongOffsetValue() {
	int32_t totalAlphasValue = this->totalAlphasValue(this->_ticks_uptr);

	return (totalAlphasValue < 0) ? this->_long_order_offset : (this->_long_order_offset + totalAlphasValue);
}

uint32_t WtHftStraDemo::getShortOffsetValue() {
	int32_t totalAlphasValue = this->totalAlphasValue(this->_ticks_uptr);

	return (totalAlphasValue < 0) ? this->_short_order_offset : (this->_short_order_offset + totalAlphasValue);
}


bool WtHftStraDemo::updateTicks(WTSTickData tickData) {
	if (this->_ticks_uptr->size() == ALPHA_TICK_COUNTS)
		this->_ticks_uptr->erase(this->_ticks_uptr->begin());

	this->_ticks_uptr->push_back(tickData);

	return (this->_ticks_uptr->size() == ALPHA_TICK_COUNTS);
}



void WtHftStraDemo::on_trade(HftStraBaseCtx* ctx, uint32_t localid, const char* stdCode, bool isBuy, double qty, double price, const char* userTag) {
	if (this->__isBelongToHft(localid)) { // localid is code
		this->__tqz_receiveCodeOnlyCloseCode(localid, this->_code, qty);
	} else {
		/// this->_ctx->stra_log_text("[WtHftStraDemo::on_trade], localid(%d) is not belong to hft strategy(%s)", localid, stdCode);
	}

	this->__tqz_writeStrategyTradeLog(localid, price, qty);
}

void WtHftStraDemo::on_order(HftStraBaseCtx* ctx, uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag) { // 下单成功后调用
	if (0 != this->_code.compare(stdCode))
		return;
	if (!isCanceled) // order is not cancel.
		return;

	TQZOrderType orderType = this->__tqz_getOrderType(localid); // get order type first.
	if (this->__isBelongToHft(localid)) { // localid is inside.

		switch (orderType) {
			case BUY_TYPE: {
				this->__unlockBuy();
				this->tqz_marketMaking(this->getLongOffsetValue(), this->getShortOffsetValue());
				break;
			}
			case SELL_TYPE: {
				std::string orderComment(this->_re_connect_ready ? "code_sellOrder_reSend" : "code_sellOrder_reConnect_reSend");

				if (this->_code_sell_lock) // re send code sell order.
					this->tqz_sell(this->_code, UNIT, OPEN_CLOSE_POSITION_OFFSET_TICKS, orderComment.c_str());

				break;
			}
			case SHORT_TYPE: {
				this->__unlockShort();
				this->tqz_marketMaking(this->getLongOffsetValue(), this->getShortOffsetValue());
				break;
			}
			case COVER_TYPE: {
				std::string orderComment(this->_re_connect_ready ? "code_coverOrder_reSend" : "code_coverOrder_reConnect_reSend");

				if (this->_code_cover_lock) // re send code cover order.
					this->tqz_cover(this->_code, UNIT, OPEN_CLOSE_POSITION_OFFSET_TICKS, orderComment.c_str());

				break;
			}
			default:
				break;
		}

	} else {
		this->_ctx->stra_log_text("[WtHftStraDemo::on_order] localid is not code, stdCode: %s, localid: %d", stdCode, localid);
	}

}


void WtHftStraDemo::on_channel_ready(HftStraBaseCtx* ctx) {
	std::cout << "HFT_strategy code: " << this->_code << "\n\n" << std::endl;
	
	this->_tradeChange_log_filename = this->__tqz_getLogFileName(TRADE_CHANGE_TYPE);

	this->_channel_ready = true;
	this->__initSessionStatus(RE_CONNECT_STATUS);
}

void WtHftStraDemo::on_entrust(uint32_t localid, bool bSuccess, const char* message, const char* userTag) {
	this->_ctx->stra_log_text("WtHftStraDemo::on_entrust  bSuccess: %d, message: %s\n\n", bSuccess, message);

	TQZOrderType orderType = this->__tqz_getOrderType(localid);
	switch (orderType) {
		case BUY_TYPE:
			this->__unlockBuy();
			break;
		case SELL_TYPE:
			this->__unlockSell();
			break;
		case SHORT_TYPE:
			this->__unlockShort();
			break;
		case COVER_TYPE:
			this->__unlockCover();
			break;
		default:
			break;
	}
}

void WtHftStraDemo::on_channel_lost(HftStraBaseCtx* ctx) {
	this->_channel_ready = false;
}

void WtHftStraDemo::on_bar(HftStraBaseCtx* ctx, const char* code, const char* period, uint32_t times, WTSBarStruct* newBar) {

}

void WtHftStraDemo::on_position(HftStraBaseCtx* ctx, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail) {

}


void WtHftStraDemo::tqz_buy(const std::string code, const double lots, const int offsetTicks, const std::string orderComment) {
	if (!this->__isEntrustable(code))
		return;

	this->__lockBuy();

	// send buy order & update local var.
	double buyOrderPrice = this->__tqz_getLongPrice(code, offsetTicks);
	uint32_t buyOrderLocalid = this->_ctx->stra_enter_long(code.c_str(), buyOrderPrice, lots, orderComment.c_str());
	std::string currentMarketTimeString = this->__tqz_getCurrentMarketTime();
	std::string sendOrderTimeString = this->__tqz_getCurrentTime();
	this->_code_buy_order = buyOrderLocalid;
	this->_code_buy_orders.insert(buyOrderLocalid);

	if (this->_record_hft_log) // recode log or not.
		this->_log_message_map[buyOrderLocalid] = this->__tqz_getNewLogMessage(
			code,
			buyOrderLocalid,
			this->_ctx->stra_get_price(code.c_str()),
			buyOrderPrice,
			"buy_order",
			currentMarketTimeString,
			sendOrderTimeString,
			orderComment
		);
}

void WtHftStraDemo::tqz_sell(const std::string code, const double lots, const int offsetTicks, const std::string orderComment) {
	if (!this->__isEntrustable(code))
		return;
	this->__lockSell();

	// send sell order & update local var.
	double sellOrderPrice = this->__tqz_getShortPrice(code, offsetTicks);
	uint32_t sellOrderLocalid = this->_ctx->stra_exit_long(code.c_str(), sellOrderPrice, lots, orderComment.c_str(), true);
	std::string currentMarketTimeString = this->__tqz_getCurrentMarketTime();
	std::string sendOrderTimeString = this->__tqz_getCurrentTime();
	this->_code_sell_order = sellOrderLocalid;
	this->_code_sell_orders.insert(sellOrderLocalid);

	if (this->_record_hft_log) // recode log or not.
		this->_log_message_map[sellOrderLocalid] = this->__tqz_getNewLogMessage(
			code,
			sellOrderLocalid,
			this->_ctx->stra_get_price(code.c_str()),
			sellOrderPrice,
			"sell_order",
			currentMarketTimeString,
			sendOrderTimeString,
			orderComment
		);
}

void WtHftStraDemo::tqz_short(const std::string code, const double lots, const int offsetTicks, const std::string orderComment) {
	if (!this->__isEntrustable(code))
		return;

	this->__lockShort();

	double shortOrderPrice = this->__tqz_getShortPrice(code, offsetTicks);
	uint32_t shortOrderLocalid = this->_ctx->stra_enter_short(code.c_str(), shortOrderPrice, lots, orderComment.c_str());
	std::string currentMarketTimeString = this->__tqz_getCurrentMarketTime();
	std::string sendOrderTimeString = this->__tqz_getCurrentTime();
	this->_code_short_order = shortOrderLocalid;
	this->_code_short_orders.insert(shortOrderLocalid);

	if (this->_record_hft_log) // recode log or not.
		this->_log_message_map[shortOrderLocalid] = this->__tqz_getNewLogMessage(
			code,
			shortOrderLocalid,
			this->_ctx->stra_get_price(code.c_str()),
			shortOrderPrice,
			"short_order",
			currentMarketTimeString,
			sendOrderTimeString,
			orderComment
		);
}

void WtHftStraDemo::tqz_cover(const std::string code, const double lots, const int offsetTicks, const std::string orderComment) {
	if (!this->__isEntrustable(code))
		return;

	this->__lockCover();

	// send cover order & update local var.
	double coverOrderPrice = this->__tqz_getLongPrice(code, offsetTicks);
	uint32_t coverOrderLocalid = this->_ctx->stra_exit_short(code.c_str(), coverOrderPrice, lots, orderComment.c_str(), true);
	std::string currentMarketTimeString = this->__tqz_getCurrentMarketTime();
	std::string sendOrderTimeString = this->__tqz_getCurrentTime();
	this->_code_cover_order = coverOrderLocalid;
	this->_code_cover_orders.insert(coverOrderLocalid);

	if (this->_record_hft_log) // recode log or not.
		this->_log_message_map[coverOrderLocalid] = this->__tqz_getNewLogMessage(
			code,
			coverOrderLocalid,
			this->_ctx->stra_get_price(code.c_str()),
			coverOrderPrice,
			"cover_order",
			currentMarketTimeString,
			sendOrderTimeString,
			orderComment
		);
}

void WtHftStraDemo::tqz_marketMaking(const int codeLongOffsetTicks, const int codeShortOffsetTicks) {
	if (!this->__isMarketMakingAble())
		return;

	this->__lockBuy(); // lock market_making.
	this->__lockShort();

	std::string buyOrderComment("code_buyOrder_marketMaking");
	std::string shortOrderComment("code_shortOrder_marketMaking");

	double currentMidPrice = this->_last_mid_price; // make sure mid price of market making is same.
	double buyOrderPrice = this->__tqz_getMarketMakingLongPrice(this->_code.c_str(), currentMidPrice, codeLongOffsetTicks);
	double shortOrderPrice = this->__tqz_getMarketMakingShortPrice(this->_code.c_str(), currentMidPrice, codeShortOffsetTicks);

	uint32_t buyOrderLocalid = this->_ctx->stra_enter_long(this->_code.c_str(), buyOrderPrice, UNIT, buyOrderComment.c_str());
	uint32_t shortOrderLocalid = this->_ctx->stra_enter_short(this->_code.c_str(), shortOrderPrice, UNIT, shortOrderComment.c_str());
	std::string currentMarketTimeString = this->__tqz_getCurrentMarketTime();
	std::string sendOrderTimeString = this->__tqz_getCurrentTime();
	this->_code_buy_order = buyOrderLocalid;
	this->_code_buy_orders.insert(buyOrderLocalid);
	this->_code_short_order = shortOrderLocalid;
	this->_code_short_orders.insert(shortOrderLocalid);

	if (!this->_record_hft_log)
		return;
	double currentPrice = this->_ctx->stra_get_price(this->_code.c_str()); // current market price.
	this->_log_message_map[buyOrderLocalid] = this->__tqz_getNewLogMessage(this->_code, buyOrderLocalid, currentPrice, buyOrderPrice, "buy_order", currentMarketTimeString, sendOrderTimeString, buyOrderComment);
	this->_log_message_map[shortOrderLocalid] = this->__tqz_getNewLogMessage(this->_code, shortOrderLocalid, currentPrice, shortOrderPrice, "short_order", currentMarketTimeString, sendOrderTimeString, shortOrderComment);
}

void WtHftStraDemo::tqz_closePositions(const std::string code, const uint32_t closePositionsOffsetTicks) {

	if (this->_ctx->tqz_getLongPosition(code.c_str()) > 0) { /// close long positions.
		this->tqz_sell(code, UNIT, closePositionsOffsetTicks, "code_sellOrder_closePositions");
	} else if (this->_ctx->tqz_getShortPosition(code.c_str()) > 0) { /// close short positions.
		this->tqz_cover(code, UNIT, closePositionsOffsetTicks, "code_coverOrder_closePositions");
	}
}

void WtHftStraDemo::tqz_doReconnect() {
	if (this->__closeCodeIsLock(true))
		return;

	double currentCodeLongLots = this->_ctx->tqz_getLongPosition(this->_code.c_str());
	double currentCodeShortLots = this->_ctx->tqz_getShortPosition(this->_code.c_str());
	if (currentCodeLongLots > 0 && currentCodeLongLots >= currentCodeShortLots) { /// have long position & long lots big.
		this->tqz_sell(this->_code, UNIT, OPEN_CLOSE_POSITION_OFFSET_TICKS, "code_sellOrder_reConnect");
	} else if (currentCodeShortLots > 0 && currentCodeLongLots < currentCodeShortLots) { /// have short position & short lots big.
		this->tqz_cover(this->_code, UNIT, OPEN_CLOSE_POSITION_OFFSET_TICKS, "code_coverOrder_reConnect");
	}
}

bool WtHftStraDemo::__isEntrustable(std::string code) {
	if (this->__isBeyondUpperlimitOrLowerlimit(code, OPEN_CLOSE_POSITION_OFFSET_TICKS, OPEN_CLOSE_POSITION_OFFSET_TICKS)) /// because cancel limit is 100.
		return false;
	if (this->_ctx->tqz_getCancelCounts(code.c_str()) > (this->_cancel_limit_counts + 10)) {
		this->_ctx->stra_log_text("[WtHftStraDemo::__isEntrustable], out of cancel limit counts when entrust, code: %s, 如果这条log被记录了, 说明策略执行逻辑被强行干扰了", code.c_str());
		return false;
	}

	return true;
}


void WtHftStraDemo::__initSessionStatus(TQZSessionStatus sessionStatus) {

	this->_current_session_status = sessionStatus;

	switch (sessionStatus) {
		case CLOSE_POSTIONS_STATUS: {
			break;
		}
		case MARKET_MAKING_STATUS: { // position and order is empty in theory.
			this->__unlockAllOrders();
			this->__tqz_clearPreviousSessionCache();

			this->_re_marketMaking = true;

			break;
		}
		case RE_CONNECT_STATUS: {
			this->__tqz_writeCancelOrderCountsLog(this->_code);

			this->_re_connect_ready = this->__tqz_queryReconnectResult();
		}
		default:
			break;
	}
}


bool WtHftStraDemo::__tqz_queryReconnectResult() {
	double codeLongLots = this->_ctx->tqz_getLongPosition(this->_code.c_str());
	double codeShortLots = this->_ctx->tqz_getShortPosition(this->_code.c_str());

	return (codeLongLots < 0.00001 && codeShortLots < 0.00001); // hold positions is empty.
}

void WtHftStraDemo::__tqz_clearPreviousSessionCache() {

	// 缓存清理无效, 成员变量换成指针类型; (每小节初始化时清理一次)
	this->_code_buy_orders.erase(this->_code_buy_orders.begin(), this->_code_buy_orders.end());
	this->_code_sell_orders.erase(this->_code_sell_orders.begin(), this->_code_sell_orders.end());
	this->_code_short_orders.erase(this->_code_short_orders.begin(), this->_code_short_orders.end());
	this->_code_cover_orders.erase(this->_code_cover_orders.begin(), this->_code_cover_orders.end());

	this->_log_message_map.clear();
}

void WtHftStraDemo::__tqz_cancelOrder(const std::string code, const uint32_t orderId) {
	if (this->_ctx->tqz_getCancelCounts(code.c_str()) > (this->_cancel_limit_counts + 15)) {
		this->_ctx->stra_log_text("[WtHftStraDemo::__tqz_cancelOrder] out of cancel limit counts when cancel order, code: %s, orderId: %d, 如果这条log被记录了, 说明策略执行逻辑被强行干扰了", code.c_str(), orderId);
		return;
	}

	this->_ctx->stra_cancel(orderId);
}

void WtHftStraDemo::__tqz_cancelOrders(const std::string code, const IDSet orderIds) {
	for (auto& localid : orderIds)
		this->__tqz_cancelOrder(code, localid);
}

void WtHftStraDemo::__tqz_cancelAllOrders() {
	this->__tqz_cancelOrders(this->_code, this->_code_buy_orders);
	this->__tqz_cancelOrders(this->_code, this->_code_sell_orders);
	this->__tqz_cancelOrders(this->_code, this->_code_short_orders);
	this->__tqz_cancelOrders(this->_code, this->_code_cover_orders);
}

TQZOrderType WtHftStraDemo::__tqz_getOrderType(const uint32_t orderId) {

	TQZOrderType orderType = DEFAULT_ORDER_TYPE;
	if (this->_code_buy_orders.find(orderId) != std::end(this->_code_buy_orders)) {
		orderType = BUY_TYPE;
	} else if (this->_code_sell_orders.find(orderId) != std::end(this->_code_sell_orders)) {
		orderType = SELL_TYPE;
	} else if (this->_code_short_orders.find(orderId) != std::end(this->_code_short_orders)) {
		orderType = SHORT_TYPE;
	} else if (this->_code_cover_orders.find(orderId) != std::end(this->_code_cover_orders)) {
		orderType = COVER_TYPE;
	} else {
		orderType = NO_TYPE;
	}

	return orderType;
}

double WtHftStraDemo::__tqz_getAskPrice(const std::string code) {
	return this->_ctx->stra_get_last_tick(code.c_str())->askprice(0);
}

double WtHftStraDemo::__tqz_getBidPrice(const std::string code) {
	return this->_ctx->stra_get_last_tick(code.c_str())->bidprice(0);
}

double WtHftStraDemo::__tqz_getCurrentMidPrice(const std::string code) {
	return (this->__tqz_getAskPrice(code) + this->__tqz_getBidPrice(code)) * 0.5;
}

bool WtHftStraDemo::__midPriceIsChange(int offsetTicks) {
	double currentMidPrice = this->__tqz_getCurrentMidPrice(this->_code);
	
	double minPriceTick = this->_ctx->stra_get_comminfo(this->_code.c_str())->getPriceTick();
	double midPriceOffset = abs(currentMidPrice - this->_last_mid_price);

	bool midPriceIsChange = (midPriceOffset >= (minPriceTick * offsetTicks));
	if (midPriceIsChange)
		this->_last_mid_price = currentMidPrice;
		
	return midPriceIsChange;
}

double WtHftStraDemo::__tqz_getLowerlimitPrice(const std::string code) {
	return this->_ctx->stra_get_last_tick(code.c_str())->lowerlimit();
}

double WtHftStraDemo::__tqz_getUpperlimitPrice(const std::string code) {
	return this->_ctx->stra_get_last_tick(code.c_str())->upperlimit();
}

double WtHftStraDemo::__tqz_getMarketMakingLongPrice(const std::string code, const double currentMidPrice, const int offsetTicks) {

	double minPriceTick = this->_ctx->stra_get_comminfo(code.c_str())->getPriceTick();
	double marketMakerLongOrderPrice = currentMidPrice - minPriceTick * offsetTicks;

	int longOrderPriceTicks = static_cast<int>(floor(marketMakerLongOrderPrice / minPriceTick));

	return longOrderPriceTicks * minPriceTick;
}

double WtHftStraDemo::__tqz_getMarketMakingShortPrice(const std::string code, const double currentMidPrice, const int offsetTicks) {
	double minPriceTick = this->_ctx->stra_get_comminfo(code.c_str())->getPriceTick();
	double marketMakerShortOrderPrice = currentMidPrice + minPriceTick * offsetTicks;
	
	int shortOrderPriceTicks = static_cast<int>(ceil(marketMakerShortOrderPrice / minPriceTick));

	return shortOrderPriceTicks * minPriceTick;
}

bool WtHftStraDemo::__isBeyondUpperlimitOrLowerlimit(const string code, const int longOrderOffsetTicks, const int shortOrderOffsetTicks) {

	if (this->__tqz_getLongPrice(code, longOrderOffsetTicks) >= this->__tqz_getUpperlimitPrice(code))
		return true;
	if (this->__tqz_getShortPrice(code, shortOrderOffsetTicks) <= this->__tqz_getLowerlimitPrice(code))
		return true;

	return false;
}

double WtHftStraDemo::__tqz_getLongPrice(const std::string code, const int offsetTicks = 0) {

	WTSCommodityInfo* codeInfo = this->_ctx->stra_get_comminfo(code.c_str());

	double longPrice = this->__tqz_getAskPrice(code.c_str()) + codeInfo->getPriceTick() * offsetTicks;

	if (longPrice >= this->__tqz_getUpperlimitPrice(code))
		longPrice = this->__tqz_getUpperlimitPrice(code);
	if (longPrice <= this->__tqz_getLowerlimitPrice(code))
		longPrice = this->__tqz_getLowerlimitPrice(code);

	return longPrice;
}

double WtHftStraDemo::__tqz_getShortPrice(const std::string code, const int offsetTicks = 0) {

	WTSCommodityInfo* codeInfo = this->_ctx->stra_get_comminfo(code.c_str());

	double shortPrice = this->__tqz_getBidPrice(code) - codeInfo->getPriceTick() * offsetTicks;

	if (shortPrice <= this->__tqz_getLowerlimitPrice(code))
		shortPrice = this->__tqz_getLowerlimitPrice(code);
	if (shortPrice >= this->__tqz_getUpperlimitPrice(code))
		shortPrice = this->__tqz_getUpperlimitPrice(code);

	return shortPrice;
}


void WtHftStraDemo::__tqz_receiveCodeOnlyCloseCode(const uint32_t localid, const std::string code, const double lots) {
	switch (this->__tqz_getOrderType(localid)) {
		case BUY_TYPE: {
			this->__tqz_cancelOrder(code, this->_code_short_order);

			if (this->_ctx->tqz_getLongPosition(code.c_str()) > 0)
				this->tqz_sell(code, lots, OPEN_CLOSE_POSITION_OFFSET_TICKS, "code_sellOrder_onlyClose");

			break;
		} case SELL_TYPE: { // close positions time.
			this->__unlockBuy();
			this->__unlockSell();

			this->_re_marketMaking = true;

			break;
		} case SHORT_TYPE: {
			this->__tqz_cancelOrder(code, this->_code_buy_order);

			if (this->_ctx->tqz_getShortPosition(code.c_str()) > 0)
				this->tqz_cover(code, lots, OPEN_CLOSE_POSITION_OFFSET_TICKS, "code_coverOrder_onlyClose");

			break;
		} case COVER_TYPE: { // close positions time.
			this->__unlockShort();
			this->__unlockCover();

			this->_re_marketMaking = true;

			break;
		}
	}
}

bool WtHftStraDemo::__isBelongToHft(const uint32_t orderId) {

	bool isBelongToHft = false;
	if (this->_code_buy_orders.find(orderId) != std::end(this->_code_buy_orders)) {
		isBelongToHft = true;
	} else if (this->_code_sell_orders.find(orderId) != std::end(this->_code_sell_orders)) {
		isBelongToHft = true;
	} else if (this->_code_short_orders.find(orderId) != std::end(this->_code_short_orders)) {
		isBelongToHft = true;
	} else if (this->_code_cover_orders.find(orderId) != std::end(this->_code_cover_orders)) {
		isBelongToHft = true;
	}

	return isBelongToHft;
}


int WtHftStraDemo::__tqz_getCurrentHourMinute() {
	struct tm *newtime;
	time_t long_time;
	time(&long_time);
	newtime = localtime(&long_time);

	return newtime->tm_hour * 100 + newtime->tm_min;
}

rapidjson::Document WtHftStraDemo::__tqz_loadJsonDocument(const std::string jsonPath) {
	char readBuffer[65536];

	FILE* filePoint = fopen(jsonPath.c_str(), "r");

	rapidjson::FileReadStream fileStream(filePoint, readBuffer, sizeof(readBuffer));
	rapidjson::Document document;

	document.ParseStream(fileStream);

	fclose(filePoint);

	return document;
}

std::string WtHftStraDemo::__tqz_getSession(const std::string symbolCode) {

	std::vector<std::string> elements;
	std::stringstream stringStream(symbolCode.c_str());
	std::string item;

	int index = 0;
	std::string exchangeString;
	std::string symString;
	std::string symbolYearMonth;
	while (std::getline(stringStream, item, '.')) {
		elements.push_back(item);
		if (index == 0) {
			exchangeString = item;
		} else if (index == 1) {
			symString = item;
		} else if (index == 2) {
			symbolYearMonth = item;
		}
		index++;
	}

	return this->_commoditiesDocument[exchangeString.c_str()][symString.c_str()]["session"].GetString();
}

bool WtHftStraDemo::__isTradingTime(const std::string code) {

	string sessionString = this->__tqz_getSession(code);

	rapidjson::Value& sections = this->_sessionsDocument[sessionString.c_str()]["sections"];
	
	bool tradeable = false;
	if (sections.IsArray() && !sections.Empty()) {

		for (auto& section : sections.GetArray()) {
			if (!section.IsObject()) // type of section is not object.
				continue;
			if (!(section.HasMember("from") && section.HasMember("to"))) // has no from key or to key.
				continue;

			int currentHourMinute = this->__tqz_getCurrentHourMinute();

			if (section["from"].GetInt() <= currentHourMinute && currentHourMinute < section["to"].GetInt()) { // is trading time.
				tradeable = true;
			}
			if (section["from"].GetInt() > section["to"].GetInt()) { // is night trading time.
				if (currentHourMinute >= section["from"].GetInt() || currentHourMinute < section["to"].GetInt()) {
					tradeable = true;
				}
			}
		}
	} else {
		throw exception("result is not array type or empty array");
	}

	return tradeable;
}

bool WtHftStraDemo::__isClosePositionsTime(const std::string code) {

	string sessionString = this->__tqz_getSession(code);

	rapidjson::Value& sections = this->_sessionsDocument[sessionString.c_str()]["sections"];

	bool isClosePositionsTime = false;

	if (sections.IsArray() && !sections.Empty()) {
		for (auto& section : sections.GetArray()) {
			if (!section.IsObject()) // type of section is not object.
				continue;
			if (!(section.HasMember("from") && section.HasMember("to"))) // has no from key or to key.
				continue;

			uint32_t toHourMinute = this->__tqz_resetToHourMinute(section["to"].GetInt(), this->_offset_close_minutes);
			uint32_t currentHourMinute = this->__tqz_getCurrentHourMinute();
			if ((toHourMinute - this->_offset_close_minutes) <= currentHourMinute && currentHourMinute < toHourMinute) // is close positions time.
				isClosePositionsTime = true;
		}
	} else {
		throw exception("result is not array type or empty array");
	}

	return isClosePositionsTime;
}

int WtHftStraDemo::__tqz_resetToHourMinute(int toHourMinute, const int offsetCloseMinutes = 0){
	int toMinute = toHourMinute % 100;
	int toHour = toHourMinute / 100;
	int offsetMinutes = offsetCloseMinutes % 60;
	int offsetHours = offsetCloseMinutes / 60;

	int newMinute = 60 + toMinute; // reset minutes
	int newHour = (toHour - (offsetHours % 24 + 1) + 24) % 24; // reset hours
	if (toMinute - offsetMinutes < 0 || offsetHours != 0) // reset toHourMinute or not
		toHourMinute = newHour * 100 + newMinute;

	return toHourMinute;
}

std::string WtHftStraDemo::__tqz_getLogFileName(const TQZLogFileType logfileType) {

	switch (logfileType) {
		case TRADE_CHANGE_TYPE:
			return "hft_tradeChange_" + to_string(this->_ctx->tqz_getTradingDate());
		case CANCEL_ORDER_COUNTS_TYPE:
			return "hft_cancelOrderCounts_" + to_string(this->_ctx->tqz_getTradingDate());
		default:
			return "";
	}
}

std::string WtHftStraDemo::__tqz_getCurrentTime() {

	const boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
	const boost::posix_time::time_duration timeOfDay = now.time_of_day();

	const uint64_t hours = timeOfDay.hours();
	const uint64_t minutes = timeOfDay.minutes();
	const uint64_t seconds = timeOfDay.seconds();
	const uint64_t milliseconds = timeOfDay.total_milliseconds() - (hours * 3600 + minutes * 60 + seconds) * 1000;

	return this->__tqz_getTimeString(hours, minutes, seconds, milliseconds);
}

std::string WtHftStraDemo::__tqz_getCurrentMarketTime() {

	uint32_t time = this->_ctx->stra_get_time();
	uint32_t secs = this->_ctx->stra_get_secs();

	int hours = time / 100;
	int minutes = time % 100;
	int seconds = secs / 1000;
	int milliseconds = secs % 1000;

	return this->__tqz_getTimeString(hours, minutes, seconds, milliseconds);
}


std::string WtHftStraDemo::__tqz_getTimeString(const uint64_t hours, const uint64_t minutes, const uint64_t seconds, const uint64_t milliseconds) {
	std::string hoursString = to_string(hours);
	if (hours < 10)
		hoursString = "0" + to_string(hours);

	std::string minutesString = to_string(minutes);
	if (minutes < 10)
		minutesString = "0" + to_string(minutes);

	std::string secondsString = to_string(seconds);
	if (seconds < 10)
		secondsString = "0" + to_string(seconds);

	std::string millisecondsString = to_string(milliseconds);
	if (milliseconds < 10) {
		millisecondsString = "00" + to_string(milliseconds);
	} else if (milliseconds < 100) {
		millisecondsString = "0" + to_string(milliseconds);
	}

	return hoursString + ":" + minutesString + ":" + secondsString + "." + millisecondsString;
}


void WtHftStraDemo::__tqz_writeStrategyTradeLog(const uint32_t orderid, const double receiveTradePrice, const double lots) {
	if (this->_log_message_map.find(orderid) == this->_log_message_map.end()) 
		return;

	TQZLogMessage logMessage = this->_log_message_map[orderid];

	std::string logString = "[code|" + logMessage.code + ",order_type|" + logMessage.orderType + ",orderid|" + to_string(logMessage.orderid) + ",market_price_of_send_order|" + to_string(logMessage.currentPrice) + ",market_time_of_send_order|" + logMessage.currentMarketTime + ",send_order_price|" + to_string(logMessage.orderPrice) + ",send_order_time|" + logMessage.sendOrderTime + ",receive_trade_price|" + to_string(receiveTradePrice) + ",receive_trade_time|" + this->__tqz_getCurrentTime() + ",lots|" + to_string(lots) + ",volScale|" + to_string(this->_ctx->stra_get_comminfo(this->_code.c_str())->getVolScale()) + ",order_comment|" + logMessage.orderComment + "]";
	this->_ctx->tqz_writeLog(this->_tradeChange_log_filename, logString);
}

void WtHftStraDemo::__tqz_writeCancelOrderCountsLog(const std::string code) {
	int now = this->__tqz_getCurrentHourMinute();
	if (now < 1500 || now > 1630)
		return;

	uint32_t cancel_order_counts = this->_ctx->tqz_getCancelCounts(code.c_str());

	std::string logString = "[code|" + code + ",cancel_order_counts|" + to_string(cancel_order_counts) + "]";
	this->_ctx->tqz_writeLog(this->__tqz_getLogFileName(CANCEL_ORDER_COUNTS_TYPE), logString);
}

TQZLogMessage WtHftStraDemo::__tqz_getNewLogMessage(std::string code, uint32_t orderid, double currentPrice, double orderPrice, char* orderType, std::string currentMarketTime, std::string sendOrderTime, std::string orderComment) {
	return TQZLogMessage(code, orderid, currentPrice, orderPrice, orderType, currentMarketTime, sendOrderTime, orderComment);
}

uint64_t WtHftStraDemo::__getCurrentTimestamp() {

	const boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
	const boost::posix_time::time_duration timeOfDay = now.time_of_day();
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, boost::gregorian::Jan, 1));
	boost::posix_time::time_duration time_from_epoch = boost::posix_time::second_clock::universal_time() - epoch;

	return time_from_epoch.total_seconds();
}

bool WtHftStraDemo::__isNewScanInterval(const uint32_t scanInterval) {

	uint64_t now = this->__getCurrentTimestamp();
	bool isNew = (now - this->_last_scan_time < scanInterval) ? false : true;

	if (isNew)
		this->_last_scan_time = now;

	return isNew;
}


void WtHftStraDemo::__lockBuy() {
	this->_code_buy_lock = true;
}

void WtHftStraDemo::__lockSell() {
	this->_code_sell_lock = true;
}

void WtHftStraDemo::__lockShort() {
	this->_code_short_lock = true;
}

void WtHftStraDemo::__lockCover() {
	this->_code_cover_lock = true;
}

void WtHftStraDemo::__unlockBuy() {
	this->_code_buy_lock = false;
}

void WtHftStraDemo::__unlockSell() {
	this->_code_sell_lock = false;
}

void WtHftStraDemo::__unlockShort() {
	this->_code_short_lock = false;
}

void WtHftStraDemo::__unlockCover() {
	this->_code_cover_lock = false;
}

void WtHftStraDemo::__unlockAllOrders() {
	this->__unlockBuy();
	this->__unlockSell();
	this->__unlockShort();
	this->__unlockCover();

	this->__unlockBuy();
	this->__unlockSell();
	this->__unlockShort();
	this->__unlockCover();
}


bool WtHftStraDemo::__isMarketMakingAble() {
	if (this->__marketMakingIsLock(false))
		return false;
	if (this->_ctx->tqz_getCancelCounts(this->_code.c_str()) > this->_cancel_limit_counts)
		return false;
	if (this->_current_session_status != MARKET_MAKING_STATUS)
		return false;
	if (this->__isBeyondUpperlimitOrLowerlimit(this->_code, this->_long_order_offset, this->_short_order_offset)) // judge this->_re_market_making is true or false, then modify it...
		return false;

	return true;
}

bool WtHftStraDemo::__closeCodeIsLock(bool reSendLockOrder) {
	bool isLock = (this->_code_sell_lock || this->_code_cover_lock);

	if (!reSendLockOrder)
		return isLock;

	if (this->_code_sell_lock)
		this->__tqz_cancelOrder(this->_code, this->_code_sell_order);
	if (this->_code_cover_lock)
		this->__tqz_cancelOrder(this->_code, this->_code_cover_order);

	return isLock;
}

bool WtHftStraDemo::__marketMakingIsLock(bool cancelLockOrder) {
	bool isLock = !(!this->_code_buy_lock && !this->_code_short_lock);

	if (!cancelLockOrder)
		return isLock;

	if (this->_code_buy_lock)
		this->__tqz_cancelOrder(this->_code, this->_code_buy_order);
	if (this->_code_short_lock)
		this->__tqz_cancelOrder(this->_code, this->_code_short_order);

	return isLock;
}
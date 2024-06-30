/*!
 * \file TraderAdapter.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#pragma once

#include "../Includes/ExecuteDefs.h"
#include "../Includes/FasterDefs.h"
#include "../Includes/ITraderApi.h"
#include "../Share/BoostFile.hpp"
#include "../Share/StdUtils.hpp"

NS_OTP_BEGIN
class WTSVariant;
class ActionPolicyMgr;
class WTSContractInfo;
class WTSCommodityInfo;
class WtLocalExecuter;
class EventNotifier;

class ITrdNotifySink;

class TraderAdapter : public ITraderSpi {
public:
	TraderAdapter(EventNotifier* caster = NULL);
	~TraderAdapter();

	typedef enum tagAdapterState {
		AS_NOTLOGIN,		//δ��¼
		AS_LOGINING,		//���ڵ�¼
		AS_LOGINED,			//�ѵ�¼
		AS_LOGINFAILED,		//��¼ʧ��
		AS_POSITION_QRYED,	//��λ�Ѳ�
		AS_ORDERS_QRYED,	//�����Ѳ�
		AS_TRADES_QRYED,	//�ɽ��Ѳ�
		AS_ALLREADY			//ȫ������
	} AdapterState;

	typedef struct _PosItem {
		//�������
		double	l_newvol;
		double	l_newavail;
		double	l_prevol;
		double	l_preavail;

		//�ղ�����
		double	s_newvol;
		double	s_newavail;
		double	s_prevol;
		double	s_preavail;

		_PosItem() {
			memset(this, 0, sizeof(_PosItem));
		}

		double total_pos(bool isLong = true) const {
			if (isLong)
				return l_newvol + l_prevol;
			else
				return s_newvol + s_prevol;
		}

		double avail_pos(bool isLong = true) const {
			if (isLong)
				return l_newavail + l_preavail;
			else
				return s_newavail + s_preavail;
		}

	} PosItem;

	typedef struct _RiskParams {
		uint32_t	_order_times_boundary;
		uint32_t	_order_stat_timespan;
		uint32_t	_order_total_limits;

		uint32_t	_cancel_times_boundary;
		uint32_t	_cancel_stat_timespan;
		uint32_t	_cancel_total_limits;

		_RiskParams() {
			memset(this, 0, sizeof(_RiskParams));
		}
	} RiskParams;

public:
	bool init(const char* id, WTSVariant* params, IBaseDataMgr* bdMgr, ActionPolicyMgr* policyMgr);

	void release();

	bool run();

	inline const char* id() const{ return _id.c_str(); }

	AdapterState state() const{ return _state; }

	void addSink(ITrdNotifySink* sink){
		_sinks.insert(sink);
	}

private:
	uint32_t doEntrust(WTSEntrust* entrust);
	bool	doCancel(WTSOrderInfo* ordInfo);

	inline void	printPosition(const char* stdCode, const PosItem& pItem);

	inline WTSContractInfo* getContract(const char* stdCode);
	inline WTSCommodityInfo* getCommodify(const char* stdCommID);

	const RiskParams* getRiskParams(const char* stdCode);

	void initSaveData();

	inline void	logTrade(uint32_t localid, const char* stdCode, WTSTradeInfo* trdInfo);
	inline void	logOrder(uint32_t localid, const char* stdCode, WTSOrderInfo* ordInfo);

	void saveData(WTSArray* ayFunds = NULL);

public:
	double getPosition(const char* stdCode, int32_t flag = 3);
	OrderMap* getOrders(const char* stdCode);
	double getUndoneQty(const char* stdCode) {
		auto it = _undone_qty.find(stdCode);
		if (it != _undone_qty.end())
			return it->second;

		return 0;
	}

	uint32_t openLong(const char* stdCode, double price, double qty);
	uint32_t openShort(const char* stdCode, double price, double qty);
	uint32_t closeLong(const char* stdCode, double price, double qty, bool isToday = false);
	uint32_t closeShort(const char* stdCode, double price, double qty, bool isToday = false);
	
	OrderIDs buy(const char* stdCode, double price, double qty);
	OrderIDs sell(const char* stdCode, double price, double qty);
	OrderIDs cancel(const char* stdCode, bool isBuy, double qty = 0);
	bool	 cancel(uint32_t localid);

	inline bool	isTradeEnabled(const char* stdCode) const;

	bool	checkCancelLimits(const char* stdCode);
	bool	checkOrderLimits(const char* stdCode);


public:
	//////////////////////////////////////////////////////////////////////////
	//ITraderSpi�ӿ�
	virtual void handleEvent(WTSTraderEvent e, int32_t ec) override;

	virtual void onLoginResult(bool bSucc, const char* msg, uint32_t tradingdate) override;

	virtual void onLogout() override;

	virtual void onRspEntrust(WTSEntrust* entrust, WTSError *err) override;

	virtual void onRspAccount(WTSArray* ayAccounts) override;

	virtual void onRspPosition(const WTSArray* ayPositions) override;

	virtual void onRspOrders(const WTSArray* ayOrders) override;

	virtual void onRspTrades(const WTSArray* ayTrades) override;

	virtual void onPushOrder(WTSOrderInfo* orderInfo) override;

	virtual void onPushTrade(WTSTradeInfo* tradeRecord) override;

	virtual void onTraderError(WTSError* err) override;

	virtual IBaseDataMgr* getBaseDataMgr() override;

	virtual void handleTraderLog(WTSLogLevel ll, const char* format, ...) override;
	
	virtual uint32_t tqz_getCancelCounts(const char* stdCode);


private:
	WTSVariant*			_cfg;
	std::string			_id;
	std::string			_order_pattern;

	ITraderApi*			_trader_api;
	FuncDeleteTrader	_remover;
	AdapterState		_state;

	EventNotifier*		_notifier;

	faster_hashset<ITrdNotifySink*>	_sinks;

	IBaseDataMgr*		_bd_mgr;
	ActionPolicyMgr*	_policy_mgr;

	faster_hashmap<std::string, PosItem> _positions;

	StdUniqueMutex  _mtx_orders;
	OrderMap*		_orders;
	faster_hashset<std::string> _orderids;	//��Ҫ���ڱ����û�д������ö���

	faster_hashmap<std::string, double> _undone_qty;	//δ�������

	typedef WTSHashMap<std::string>	TradeStatMap;
	TradeStatMap*	_stat_map;	// ͳ������

	//����������ʱ���ڵ�����,��Ҫ��Ϊ�˿���˲�����������õ�
	typedef std::vector<uint64_t> TimeCacheList;
	typedef faster_hashmap<std::string, TimeCacheList> CodeTimeCacheMap;
	CodeTimeCacheMap	_order_time_cache;	//�µ�ʱ�仺��
	CodeTimeCacheMap	_cancel_time_cache;	//����ʱ�仺��

	//����������,�ͻ���뵽�ų�����
	faster_hashset<std::string>	_exclude_codes;
	
	typedef faster_hashmap<std::string, RiskParams>	RiskParamsMap;
	RiskParamsMap	_risk_params_map;
	bool			_risk_mon_enabled;

	bool			_save_data;			//�Ƿ񱣴潻����־
	BoostFilePtr	_trades_log;		//����������־
	BoostFilePtr	_orders_log;		//����������־
	std::string		_rt_data_file;		//ʵʱ�����ļ�
};

typedef std::shared_ptr<TraderAdapter>				TraderAdapterPtr;
typedef faster_hashmap<std::string, TraderAdapterPtr>	TraderAdapterMap;


//////////////////////////////////////////////////////////////////////////
//TraderAdapterMgr
class TraderAdapterMgr : private boost::noncopyable
{
public:
	void	release();

	void	run();

	const TraderAdapterMap& getAdapters() const { return _adapters; }

	TraderAdapterPtr getAdapter(const char* tname);

	bool	addAdapter(const char* tname, TraderAdapterPtr& adapter);

private:
	TraderAdapterMap	_adapters;
};

NS_OTP_END
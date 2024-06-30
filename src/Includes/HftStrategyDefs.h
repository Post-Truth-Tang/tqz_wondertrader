/*!
 * \file HftStrategyDefs.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#pragma once
#include <string>
#include <stdint.h>

#include "../Includes/WTSMarcos.h"

NS_OTP_BEGIN
class WTSVariant;

//class IHftStraCtx;
class HftStraBaseCtx;

class WTSTickData;
class WTSOrdDtlData;
class WTSOrdQueData;
class WTSTransData;
struct WTSBarStruct;
NS_OTP_END

USING_NS_OTP;

class HftStrategy
{
public:
	HftStrategy(const char* id) :_id(id){}
	virtual ~HftStrategy(){}

public:
	/*
	*	ִ�е�Ԫ����
	*/
	virtual const char* getName() = 0;

	/*
	*	����ִ������������
	*/
	virtual const char* getFactName() = 0;

	/*
	*	��ʼ��
	*/
	virtual bool init(WTSVariant* cfg){ return true; }

	virtual const char* id() const { return _id.c_str(); }

	//�ص�����
	virtual void on_init(HftStraBaseCtx* ctx) = 0;
	virtual void on_session_begin(HftStraBaseCtx* ctx, uint32_t uTDate) {}
	virtual void on_session_end(HftStraBaseCtx* ctx, uint32_t uTDate) {}

	virtual void on_tick(HftStraBaseCtx* ctx, const char* code, WTSTickData* newTick) = 0;
	virtual void on_order_queue(HftStraBaseCtx* ctx, const char* code, WTSOrdQueData* newOrdQue) {}
	virtual void on_order_detail (HftStraBaseCtx* ctx, const char* code, WTSOrdDtlData* newOrdDtl) {}
	virtual void on_transaction(HftStraBaseCtx* ctx, const char* code, WTSTransData* newTrans) {}
	virtual void on_bar(HftStraBaseCtx* ctx, const char* code, const char* period, uint32_t times, WTSBarStruct* newBar) = 0;

	virtual void on_trade(HftStraBaseCtx* ctx, uint32_t localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag) = 0;
	virtual void on_position(HftStraBaseCtx* ctx, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail) = 0;
	virtual void on_order(HftStraBaseCtx* ctx, uint32_t localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag) = 0;
	virtual void on_channel_ready(HftStraBaseCtx* ctx) = 0;
	virtual void on_channel_lost(HftStraBaseCtx* ctx) = 0;
	virtual void on_entrust(uint32_t localid, bool bSuccess, const char* message, const char* userTag) = 0;

protected:
	std::string _id;
};

//////////////////////////////////////////////////////////////////////////
//���Թ����ӿ�
typedef void(*FuncEnumHftStrategyCallback)(const char* factName, const char* straName, bool isLast);

class IHftStrategyFact
{
public:
	IHftStrategyFact(){}
	virtual ~IHftStrategyFact(){}

public:
	/*
	*	��ȡ������
	*/
	virtual const char* getName() = 0;

	/*
	*	ö�ٲ���
	*/
	virtual void enumStrategy(FuncEnumHftStrategyCallback cb) = 0;

	/*
	*	�������ƴ���ִ�е�Ԫ
	*/
	virtual HftStrategy* createStrategy(const char* name, const char* id) = 0;

	/*
	*	ɾ��ִ�е�Ԫ
	*/
	virtual bool deleteStrategy(HftStrategy* stra) = 0;
};

//����ִ�й���
typedef IHftStrategyFact* (*FuncCreateHftStraFact)();
//ɾ��ִ�й���
typedef void(*FuncDeleteHftStraFact)(IHftStrategyFact* &fact);
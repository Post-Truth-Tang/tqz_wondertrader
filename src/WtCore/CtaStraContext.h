/*!
 * \file CtaStraContext.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#pragma once
#include "CtaStraBaseCtx.h"

#include "../Includes/WTSDataDef.hpp"

NS_OTP_BEGIN
class WtCtaEngine;
NS_OTP_END

USING_NS_OTP;

class CtaStrategy;

class CtaStraContext : public CtaStraBaseCtx
{
public:
	CtaStraContext(WtCtaEngine* engine, const char* name);
	virtual ~CtaStraContext();

	void set_strategy(CtaStrategy* stra){ _strategy = stra; }
	CtaStrategy* get_stragety() { return _strategy; }

public:
	//�ص�����
	virtual void on_init() override;
	virtual void on_session_begin(uint32_t uTDate) override;
	virtual void on_session_end(uint32_t uTDate) override;
	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) override;
	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) override;
	virtual void on_mainkline_updated(uint32_t curDate, uint32_t curTime) override;

private:
	CtaStrategy*		_strategy;
};



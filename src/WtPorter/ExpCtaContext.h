/*!
 * \file PyCtaContext.h
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#pragma once
#include "../WtCore/CtaStraBaseCtx.h"

USING_NS_OTP;

class ExpCtaContext : public CtaStraBaseCtx
{
public:
	ExpCtaContext(WtCtaEngine* env, const char* name);
	virtual ~ExpCtaContext();

public:
	virtual void on_init() override;

	virtual void on_session_begin(uint32_t uDate) override;

	virtual void on_session_end(uint32_t uDate) override;

	virtual void on_tick_updated(const char* stdCode, WTSTickData* newTick) override;

	virtual void on_bar_close(const char* stdCode, const char* period, WTSBarStruct* newBar) override;

	virtual void on_mainkline_updated(uint32_t curDate, uint32_t curTime) override;
};


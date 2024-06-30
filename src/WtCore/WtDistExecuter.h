#pragma once
#include "IExecCommand.h"

NS_OTP_BEGIN
class WTSVariant;

class WtDistExecuter : public IExecCommand
{
public:
	WtDistExecuter(const char* name);
	virtual ~WtDistExecuter();

public:
	/*
	 *	��ʼ��ִ����
	 *	�����ʼ������
	 */
	bool init(WTSVariant* params);


public:
	//////////////////////////////////////////////////////////////////////////
	//IExecCommand
	virtual void set_position(const faster_hashmap<std::string, double>& targets) override;


	virtual void on_position_changed(const char* stdCode, double targetPos) override;


	virtual void on_tick(const char* stdCode, WTSTickData* newTick) override;

private:
	void		writeLog(const char* fmt, ...);

private:
	WTSVariant*			_config;

	uint32_t			_scale;

	faster_hashmap<std::string, double> _target_pos;
};
NS_OTP_END

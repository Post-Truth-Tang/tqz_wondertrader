/*!
 * \file WtRtRunner.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#include "WtRtRunner.h"
#include "ExpCtaContext.h"
#include "ExpSelContext.h"
#include "ExpHftContext.h"

#include "ExpParser.h"
#include "ExpExecuter.h"

#include "../WtCore/WtHelper.h"
#include "../WtCore/CtaStraContext.h"
#include "../WtCore/HftStraContext.h"
#include "../WtCore/SelStraContext.h"

#include "../WTSTools/WTSLogger.h"

#include "../Share/JsonToVariant.hpp"
#include "../Share/StdUtils.hpp"
#include "../Share/TimeUtils.hpp"
#include "../Share/CodeHelper.hpp"

#include "../Includes/WTSContractInfo.hpp"

#include "../WTSUtils/SignalHook.hpp"

#ifdef _WIN32
#define my_stricmp _stricmp
#else
#define my_stricmp strcasecmp
#endif

extern const char* getBinDir();

WtRtRunner::WtRtRunner()
	: _data_store(NULL)
	, _cb_cta_init(NULL)
	, _cb_cta_tick(NULL)
	, _cb_cta_calc(NULL)
	, _cb_cta_bar(NULL)
	, _cb_cta_sessevt(NULL)

	, _cb_sel_init(NULL)
	, _cb_sel_tick(NULL)
	, _cb_sel_calc(NULL)
	, _cb_sel_bar(NULL)
	, _cb_sel_sessevt(NULL)

	, _cb_hft_init(NULL)
	, _cb_hft_tick(NULL)
	, _cb_hft_bar(NULL)
	, _cb_hft_ord(NULL)
	, _cb_hft_trd(NULL)
	, _cb_hft_entrust(NULL)
	, _cb_hft_chnl(NULL)

	, _cb_hft_orddtl(NULL)
	, _cb_hft_ordque(NULL)
	, _cb_hft_trans(NULL)

	, _cb_hft_sessevt(NULL)

	, _cb_exec_cmd(NULL)
	, _cb_exec_init(NULL)

	, _cb_parser_evt(NULL)
	, _cb_parser_sub(NULL)

	, _cb_evt(NULL)
	, _is_hft(false)
	, _is_sel(false)
{
	install_signal_hooks([](const char* message) {
		WTSLogger::error(message);
	});
}


WtRtRunner::~WtRtRunner()
{
}

bool WtRtRunner::init(const char* logCfg /* = "logcfg.prop" */, bool isFile /* = true */, const char* genDir)
{
	if(isFile) {
		std::string path = WtHelper::getCWD() + logCfg;
		WTSLogger::init(path.c_str(), true);
	} else {
		WTSLogger::init(logCfg, false);
	}
	
	WtHelper::setInstDir(getBinDir());
	WtHelper::setGenerateDir(genDir);
	return true;
}

void WtRtRunner::registerEvtCallback(FuncEventCallback cbEvt)
{
	_cb_evt = cbEvt;

	_cta_engine.regEventListener(this);
	_hft_engine.regEventListener(this);
	_sel_engine.regEventListener(this);
}

void WtRtRunner::registerParserPorter(FuncParserEvtCallback cbEvt, FuncParserSubCallback cbSub)
{
	_cb_parser_evt = cbEvt;
	_cb_parser_sub = cbSub;

	WTSLogger::info("Callbacks of Extented Parser registration done");
}

void WtRtRunner::registerExecuterPorter(FuncExecInitCallback cbInit, FuncExecCmdCallback cbExec)
{
	_cb_exec_init = cbInit;
	_cb_exec_cmd = cbExec;

	WTSLogger::info("Callbacks of Extented Executer registration done");
}

void WtRtRunner::registerCtaCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt)
{
	_cb_cta_init = cbInit;
	_cb_cta_tick = cbTick;
	_cb_cta_calc = cbCalc;
	_cb_cta_bar = cbBar;
	_cb_cta_sessevt = cbSessEvt;

	WTSLogger::info("Callbacks of CTA engine registration done");
}

void WtRtRunner::registerSelCallbacks(FuncStraInitCallback cbInit, FuncStraTickCallback cbTick, FuncStraCalcCallback cbCalc, FuncStraBarCallback cbBar, FuncSessionEvtCallback cbSessEvt)
{
	_cb_sel_init = cbInit;
	_cb_sel_tick = cbTick;
	_cb_sel_calc = cbCalc;
	_cb_sel_bar = cbBar;

	_cb_sel_sessevt = cbSessEvt;

	WTSLogger::info("Callbacks of SEL engine registration done");
}

void WtRtRunner::registerHftCallbacks(
	FuncStraInitCallback cbInit, 
	FuncStraTickCallback cbTick,
	FuncStraBarCallback cbBar, 
	FuncHftChannelCallback cbChnl,
	FuncHftOrdCallback cbOrd,
	FuncHftTrdCallback cbTrd, 
	FuncHftEntrustCallback cbEntrust,
	FuncStraOrdDtlCallback cbOrdDtl,
	FuncStraOrdQueCallback cbOrdQue, 
	FuncStraTransCallback cbTrans, 
	FuncSessionEvtCallback cbSessEvt
)
{
	_cb_hft_init = cbInit;
	_cb_hft_tick = cbTick;
	_cb_hft_bar = cbBar;

	_cb_hft_chnl = cbChnl;
	_cb_hft_ord = cbOrd;
	_cb_hft_trd = cbTrd;
	_cb_hft_entrust = cbEntrust;

	_cb_hft_orddtl = cbOrdDtl;
	_cb_hft_ordque = cbOrdQue;
	_cb_hft_trans = cbTrans;

	_cb_hft_sessevt = cbSessEvt;

	WTSLogger::info("Callbacks of HFT engine registration done");
}

bool WtRtRunner::createExtParser(const char* id)
{
	ParserAdapterPtr adapter(new ParserAdapter);
	ExpParser* parser = new ExpParser(id);
	adapter->initExt(id, parser, _engine, _engine->get_basedata_mgr(), _engine->get_hot_mgr());
	_parsers.addAdapter(id, adapter);
	WTSLogger::info("Extended parser created");
	return true;
}

bool WtRtRunner::createExtExecuter(const char* id)
{
	ExpExecuter* executer = new ExpExecuter(id);
	executer->init();
	_cta_engine.addExecuter(ExecCmdPtr(executer));
	WTSLogger::info("Extended Executer created");
	return true;
}

uint32_t WtRtRunner::createCtaContext(const char* name)
{
	ExpCtaContext* ctx = new ExpCtaContext(&_cta_engine, name);
	_cta_engine.addContext(CtaContextPtr(ctx));
	return ctx->id();
}

uint32_t WtRtRunner::createHftContext(const char* name, const char* trader, bool bAgent /* = true */)
{
	ExpHftContext* ctx = new ExpHftContext(&_hft_engine, name, bAgent);
	_hft_engine.addContext(HftContextPtr(ctx));
	TraderAdapterPtr trdPtr = _traders.getAdapter(trader);
	if(trdPtr)
	{
		ctx->setTrader(trdPtr.get());
		trdPtr->addSink(ctx);
	}
	else
	{
		WTSLogger::error("Trader %s not exists, Binding trader to HFT strategy failed", trader);
	}
	return ctx->id();
}

uint32_t WtRtRunner::createSelContext(const char* name, uint32_t date, uint32_t time, const char* period, const char* trdtpl /* = "CHINA" */, const char* session/* ="TRADING" */)
{
	TaskPeriodType ptype;
	if (my_stricmp(period, "d") == 0)
		ptype = TPT_Daily;
	else if (my_stricmp(period, "w") == 0)
		ptype = TPT_Weekly;
	else if (my_stricmp(period, "m") == 0)
		ptype = TPT_Monthly;
	else if (my_stricmp(period, "y") == 0)
		ptype = TPT_Yearly;
	else if (my_stricmp(period, "min") == 0)
		ptype = TPT_Minute;
	else
		ptype = TPT_None;

	ExpSelContext* ctx = new ExpSelContext(&_sel_engine, name);

	_sel_engine.addContext(SelContextPtr(ctx), date, time, ptype, true, trdtpl, session);

	return ctx->id();
}

CtaContextPtr WtRtRunner::getCtaContext(uint32_t id)
{
	return _cta_engine.getContext(id);
}

HftContextPtr WtRtRunner::getHftContext(uint32_t id)
{
	return _hft_engine.getContext(id);
}

SelContextPtr WtRtRunner::getSelContext(uint32_t id)
{
	return _sel_engine.getContext(id);
}

void WtRtRunner::ctx_on_bar(uint32_t id, const char* stdCode, const char* period, WTSBarStruct* newBar, EngineType eType /* = ET_CTA */)
{
	switch (eType)
	{
	case ET_CTA: if (_cb_cta_bar) _cb_cta_bar(id, stdCode, period, newBar); break;
	case ET_HFT: if (_cb_hft_bar) _cb_hft_bar(id, stdCode, period, newBar); break;
	case ET_SEL: if (_cb_sel_bar) _cb_sel_bar(id, stdCode, period, newBar); break;
	default:
		break;
	}
}

void WtRtRunner::ctx_on_calc(uint32_t id, uint32_t curDate, uint32_t curTime, EngineType eType /* = ET_CTA */)
{
	switch (eType)
	{
	case ET_CTA: if (_cb_cta_calc) _cb_cta_calc(id, curDate, curTime); break;
	case ET_SEL: if (_cb_sel_calc) _cb_sel_calc(id, curDate, curTime); break;
	default:
		break;
	}
}

void WtRtRunner::ctx_on_init(uint32_t id, EngineType eType/* = ET_CTA*/)
{
	switch (eType)
	{
	case ET_CTA: if (_cb_cta_init) _cb_cta_init(id); break;
	case ET_HFT: if (_cb_hft_init) _cb_hft_init(id); break;
	case ET_SEL: if (_cb_sel_init) _cb_sel_init(id); break;
	default:
		break;
	}
}

void WtRtRunner::ctx_on_session_event(uint32_t id, uint32_t curTDate, bool isBegin /* = true */, EngineType eType /* = ET_CTA */)
{
	switch (eType)
	{
	case ET_CTA: if (_cb_cta_sessevt) _cb_cta_sessevt(id, curTDate, isBegin); break;
	case ET_HFT: if (_cb_hft_sessevt) _cb_hft_sessevt(id, curTDate, isBegin); break;
	case ET_SEL: if (_cb_sel_sessevt) _cb_sel_sessevt(id, curTDate, isBegin); break;
	default:
		break;
	}
}

void WtRtRunner::ctx_on_tick(uint32_t id, const char* stdCode, WTSTickData* newTick, EngineType eType /* = ET_CTA */)
{
	switch (eType)
	{
	case ET_CTA: if (_cb_cta_tick) _cb_cta_tick(id, stdCode, &newTick->getTickStruct()); break;
	case ET_HFT: if (_cb_hft_tick) _cb_hft_tick(id, stdCode, &newTick->getTickStruct()); break;
	case ET_SEL: if (_cb_sel_tick) _cb_sel_tick(id, stdCode, &newTick->getTickStruct()); break;
	default:
		break;
	}
}

void WtRtRunner::hft_on_channel_lost(uint32_t cHandle, const char* trader)
{
	if (_cb_hft_chnl)
		_cb_hft_chnl(cHandle, trader, CHNL_EVENT_LOST);
}

void WtRtRunner::hft_on_channel_ready(uint32_t cHandle, const char* trader)
{
	if (_cb_hft_chnl)
		_cb_hft_chnl(cHandle, trader, CHNL_EVENT_READY);
}

void WtRtRunner::hft_on_entrust(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool bSuccess, const char* message, const char* userTag)
{
	if (_cb_hft_entrust)
		_cb_hft_entrust(cHandle, localid, stdCode, bSuccess, message, userTag);
}

void WtRtRunner::hft_on_order(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double totalQty, double leftQty, double price, bool isCanceled, const char* userTag)
{
	if (_cb_hft_ord)
		_cb_hft_ord(cHandle, localid, stdCode, isBuy, totalQty, leftQty, price, isCanceled, userTag);
}

void WtRtRunner::hft_on_trade(uint32_t cHandle, WtUInt32 localid, const char* stdCode, bool isBuy, double vol, double price, const char* userTag)
{
	if (_cb_hft_trd)
		_cb_hft_trd(cHandle, localid, stdCode, isBuy, vol, price, userTag);
}

bool WtRtRunner::config(const char* cfgFile, bool isFile /* = true */)
{
	_config = WTSVariant::createObject();
	{
		std::string json;
		if (isFile)
			StdFile::read_file_content(cfgFile, json);
		else
			json = cfgFile;

		rj::Document document;
		document.Parse(json.c_str());
		jsonToVariant(document, _config);
	}

	//基础数据文件
	WTSVariant* cfgBF = _config->get("basefiles");
	if (cfgBF->get("session"))
	{
		_bd_mgr.loadSessions(cfgBF->getCString("session"));
		WTSLogger::info("Trading sessions loaded");
	}

	if (cfgBF->get("commodity"))
	{
		_bd_mgr.loadCommodities(cfgBF->getCString("commodity"));
		WTSLogger::info("Commodities loaded");
	}

	if (cfgBF->get("contract"))
	{
		_bd_mgr.loadContracts(cfgBF->getCString("contract"));
		WTSLogger::info("Contracts loaded");
	}

	if (cfgBF->get("holiday"))
	{
		_bd_mgr.loadHolidays(cfgBF->getCString("holiday"));
		WTSLogger::info("Holidays loaded");
	}

	if (cfgBF->get("hot"))
	{
		_hot_mgr.loadHots(cfgBF->getCString("hot"));
		WTSLogger::info("Hot rules loades");
	}

	if (cfgBF->get("second"))
	{
		_hot_mgr.loadSeconds(cfgBF->getCString("second"));
		WTSLogger::info("Second rules loades");
	}

	//初始化运行环境
	initEngine();

	//初始化数据管理
	initDataMgr();

	//初始化开平策略
	if (!initActionPolicy())
		return false;

	//初始化行情通道
	const char* cfgParser = _config->getCString("parsers");
	if(StdFile::exists(cfgParser))
	{
		WTSLogger::info("Reading parser configuration from %s……", cfgParser);
		std::string json;
		StdFile::read_file_content(cfgParser, json);

		rj::Document document;
		document.Parse(json.c_str());
		WTSVariant* var = WTSVariant::createObject();
		jsonToVariant(document, var);

		if (!initParsers(var))
			WTSLogger::error("Loading parsers failed");
		var->release();
	}
	else
	{
		WTSLogger::error("Parser configuration %s not exists", cfgParser);
	}

	//初始化交易通道
	const char* cfgTraders = _config->getCString("traders");
	if (StdFile::exists(cfgTraders))
	{
		WTSLogger::info("Reading trader configuration from %s……", cfgTraders);
		std::string json;
		StdFile::read_file_content(cfgTraders, json);

		rj::Document document;
		document.Parse(json.c_str());
		WTSVariant* var = WTSVariant::createObject();
		jsonToVariant(document, var);

		if (!initTraders(var))
			WTSLogger::error("Loading traders failed");
		var->release();
	}
	else
	{
		WTSLogger::error("Trader configuration %s not exists", cfgTraders);
	}

	//初始化事件推送器
	initEvtNotifier();

	//如果不是高频引擎,则需要配置执行模块
	if (!_is_hft)
		initExecuters();

	if (!_is_hft)
		initCtaStrategies();
	else
		initHftStrategies();
	
	return true;
}

bool WtRtRunner::initCtaStrategies()
{
	WTSVariant* cfg = _config->get("strategies");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)
		return false;

	cfg = cfg->get("cta");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)
		return false;

	for (uint32_t idx = 0; idx < cfg->size(); idx++)
	{
		WTSVariant* cfgItem = cfg->get(idx);
		const char* id = cfgItem->getCString("id");
		const char* name = cfgItem->getCString("name");
		CtaStrategyPtr stra = _cta_mgr.createStrategy(name, id);
		stra->self()->init(cfgItem->get("params"));
		CtaStraContext* ctx = new CtaStraContext(&_cta_engine, id);
		ctx->set_strategy(stra->self());
		_cta_engine.addContext(CtaContextPtr(ctx));
	}

	return true;
}

bool WtRtRunner::initSelStrategies()
{
	WTSVariant* cfg = _config->get("strategies");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)
		return false;

	cfg = cfg->get("cta");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)
		return false;

	for (uint32_t idx = 0; idx < cfg->size(); idx++)
	{
		WTSVariant* cfgItem = cfg->get(idx);
		const char* id = cfgItem->getCString("id");
		const char* name = cfgItem->getCString("name");

		uint32_t date = cfgItem->getUInt32("date");
		uint32_t time = cfgItem->getUInt32("time");
		const char* period = cfgItem->getCString("period");

		TaskPeriodType ptype;
		if (my_stricmp(period, "d") == 0)
			ptype = TPT_Daily;
		else if (my_stricmp(period, "w") == 0)
			ptype = TPT_Weekly;
		else if (my_stricmp(period, "m") == 0)
			ptype = TPT_Monthly;
		else if (my_stricmp(period, "y") == 0)
			ptype = TPT_Yearly;
		else
			ptype = TPT_None;

		SelStrategyPtr stra = _sel_mgr.createStrategy(name, id);
		stra->self()->init(cfgItem->get("params"));
		SelStraContext* ctx = new SelStraContext(&_sel_engine, id);
		ctx->set_strategy(stra->self());
		_sel_engine.addContext(SelContextPtr(ctx), date, time, ptype);
	}

	return true;
}

bool WtRtRunner::initHftStrategies()
{
	WTSVariant* cfg = _config->get("strategies");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)
		return false;

	cfg = cfg->get("hft");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)
		return false;

	for (uint32_t idx = 0; idx < cfg->size(); idx++)
	{
		WTSVariant* cfgItem = cfg->get(idx);
		const char* id = cfgItem->getCString("id");
		const char* name = cfgItem->getCString("name");
		bool bAgent = cfgItem->getBoolean("agent");
		HftStrategyPtr stra = _hft_mgr.createStrategy(name, id);
		if (stra == NULL)
			continue;

		stra->self()->init(cfgItem->get("params"));
		HftStraContext* ctx = new HftStraContext(&_hft_engine, id, bAgent);
		ctx->set_strategy(stra->self());

		const char* traderid = cfgItem->getCString("trader");
		TraderAdapterPtr trader = _traders.getAdapter(traderid);
		if (trader)
		{
			ctx->setTrader(trader.get());
			trader->addSink(ctx);
		}
		else
		{
			WTSLogger::error("Trader %s not exists, Binding trader to HFT strategy failed", traderid);
		}

		_hft_engine.addContext(HftContextPtr(ctx));
	}

	return true;
}

bool WtRtRunner::initEngine()
{
	WTSVariant* cfg = _config->get("env");
	if (cfg == NULL)
		return false;

	const char* name = cfg->getCString("name");

	if (strlen(name) == 0 || my_stricmp(name, "cta") == 0)
	{
		_is_hft = false;
		_is_sel = false;
	}
	else if (my_stricmp(name, "sel") == 0)
	{
		_is_sel = true;
	}
	else //if (my_stricmp(name, "hft") == 0)
	{
		_is_hft = true;
	}

	if (_is_hft)
	{
		WTSLogger::info("Trading environment initialized, engine name: HFT");
		_hft_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr);
		_engine = &_hft_engine;
	}
	else if (_is_sel)
	{
		WTSLogger::info("Trading environment initialized, engine name: SEL");
		_sel_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr);
		_engine = &_sel_engine;
	}
	else
	{
		WTSLogger::info("Trading environment initialized, engine name: CTA");
		_cta_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr);
		_engine = &_cta_engine;
	}

	_engine->set_adapter_mgr(&_traders);
	
	return true;
}

bool WtRtRunner::initDataMgr()
{
	WTSVariant* cfg = _config->get("data");
	if (cfg == NULL)
		return false;

	_data_mgr.init(cfg, _engine);

	WTSLogger::info("Data manager initialized");
	return true;
}

bool WtRtRunner::initParsers(WTSVariant* cfgParser)
{
	WTSVariant* cfg = cfgParser->get("parsers");
	if (cfg == NULL)
		return false;

	uint32_t count = 0;
	for (uint32_t idx = 0; idx < cfg->size(); idx++)
	{
		WTSVariant* cfgItem = cfg->get(idx);
		if (!cfgItem->getBoolean("active"))
			continue;

		const char* id = cfgItem->getCString("id");

		ParserAdapterPtr adapter(new ParserAdapter);
		adapter->init(id, cfgItem, _engine, _engine->get_basedata_mgr(), _engine->get_hot_mgr());

		_parsers.addAdapter(id, adapter);

		count++;
	}

	WTSLogger::info("%u parsers loaded", count);

	return true;
}

bool WtRtRunner::initExecuters()
{
	WTSVariant* cfg = _config->get("executers");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)
		return false;

	//先加载自带的执行器工厂
	std::string path = WtHelper::getInstDir() + "executer//";
	_exe_factory.loadFactories(path.c_str());

	uint32_t count = 0;
	for (uint32_t idx = 0; idx < cfg->size(); idx++)
	{
		WTSVariant* cfgItem = cfg->get(idx);
		if (!cfgItem->getBoolean("active"))
			continue;

		const char* id = cfgItem->getCString("id");
		bool bLocal = cfgItem->getBoolean("local");

		if(bLocal)
		{
			WtLocalExecuter* executer = new WtLocalExecuter(&_exe_factory, id, &_data_mgr);
			if (!executer->init(cfgItem))
				return false;

			TraderAdapterPtr trader = _traders.getAdapter(cfgItem->getCString("trader"));
			executer->setTrader(trader.get());
			trader->addSink(executer);

			_cta_engine.addExecuter(ExecCmdPtr(executer));
		}
		else
		{
			WtDistExecuter* executer = new WtDistExecuter(id);
			if (!executer->init(cfgItem))
				return false;

			_cta_engine.addExecuter(ExecCmdPtr(executer));
		}
		
		count++;
	}

	WTSLogger::info("%u executers loaded", count);

	return true;
}

bool WtRtRunner::initEvtNotifier()
{
	WTSVariant* cfg = _config->get("notifier");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object)
		return false;

	_notifier.init(cfg);

	return true;
}

bool WtRtRunner::initTraders(WTSVariant* cfgTrader)
{
	WTSVariant* cfg = cfgTrader->get("traders");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)
		return false;
	
	uint32_t count = 0;
	for (uint32_t idx = 0; idx < cfg->size(); idx++)
	{
		WTSVariant* cfgItem = cfg->get(idx);
		if (!cfgItem->getBoolean("active"))
			continue;

		const char* id = cfgItem->getCString("id");

		TraderAdapterPtr adapter(new TraderAdapter(&_notifier));
		adapter->init(id, cfgItem, &_bd_mgr, &_act_policy);

		_traders.addAdapter(id, adapter);
		count++;
	}

	WTSLogger::info("%u traders loaded", count);

	return true;
}

void WtRtRunner::run(bool bAsync /* = false */)
{
	try
	{
		_parsers.run();
		_traders.run();

		_engine->run(bAsync);
	}
	catch (...)
	{
		print_stack_trace([](const char* message) {
			WTSLogger::error(message);
		});
	}
}

void WtRtRunner::release()
{
	WTSLogger::stop();
}

void WtRtRunner::dump_bars(const char* stdCode, const char* period, FuncDumpBarsCallback cb, FuncCountDataCallback cbCnt)
{
	WTSCommodityInfo* cInfo = _bd_mgr.getCommodity(CodeHelper::stdCodeToStdCommID(stdCode).c_str());
	if (cInfo == NULL)
	{
		cbCnt(0);
		return;
	}

	WTSSessionInfo* sInfo = _bd_mgr.getSession(cInfo->getSession());
	if (sInfo == NULL)
	{
		cbCnt(0);
		return;
	}

	std::string basePeriod = "";
	uint32_t times = 1;
	if (strlen(period) > 1)
	{
		basePeriod.append(period, 1);
		times = strtoul(period + 1, NULL, 10);
	}
	else
	{
		basePeriod = period;
	}

	WTSKlinePeriod kp;
	if (strcmp(basePeriod.c_str(), "m") == 0)
	{
		if (times % 5 == 0)
		{
			kp = KP_Minute5;
			times /= 5;
		}
		else
			kp = KP_Minute1;
	}
	else
	{
		kp = KP_DAY;
	}

	WTSKlineSlice* kline = _data_mgr.get_kline_slice(stdCode, kp, times, UINT32_MAX, 0);
	if (kline)
	{
		cbCnt(kline->size());
		for (int32_t idx = 0; idx < kline->size(); idx++)
		{
			WTSBarStruct* bs = kline->at(idx);
			cb(bs, (idx == kline->size() - 1));
		}
		kline->release();
	}
	else
	{
		cbCnt(0);
	}
}


bool WtRtRunner::initActionPolicy()
{
	const char* action_file = _config->getCString("bspolicy");
	if (strlen(action_file) <= 0)
		return false;

	bool ret = _act_policy.init(action_file);
	WTSLogger::info("Action policies initialized");
	return ret;
}

bool WtRtRunner::addCtaFactories(const char* folder)
{
	return _cta_mgr.loadFactories(folder);
}

bool WtRtRunner::addSelFactories(const char* folder)
{
	return _sel_mgr.loadFactories(folder);
}

bool WtRtRunner::addExeFactories(const char* folder)
{
	return _exe_factory.loadFactories(folder);
}

bool WtRtRunner::addHftFactories(const char* folder)
{
	return _hft_mgr.loadFactories(folder);
}

void WtRtRunner::hft_on_order_queue(uint32_t id, const char* stdCode, WTSOrdQueData* newOrdQue)
{
	if (_cb_hft_ordque)
		_cb_hft_ordque(id, stdCode, &newOrdQue->getOrdQueStruct());
}

void WtRtRunner::hft_on_order_detail(uint32_t id, const char* stdCode, WTSOrdDtlData* newOrdDtl)
{
	if (_cb_hft_orddtl)
		_cb_hft_orddtl(id, stdCode, &newOrdDtl->getOrdDtlStruct());
}

void WtRtRunner::hft_on_transaction(uint32_t id, const char* stdCode, WTSTransData* newTrans)
{
	if (_cb_hft_trans)
		_cb_hft_trans(id, stdCode, &newTrans->getTransStruct());
}

#pragma region "Extended Parser"
void WtRtRunner::parser_init(const char* id)
{
	if (_cb_parser_evt)
		_cb_parser_evt(EVENT_PARSER_INIT, id);
}

void WtRtRunner::parser_connect(const char* id)
{
	if (_cb_parser_evt)
		_cb_parser_evt(EVENT_PARSER_CONNECT, id);
}

void WtRtRunner::parser_disconnect(const char* id)
{
	if (_cb_parser_evt)
		_cb_parser_evt(EVENT_PARSER_DISCONNECT, id);
}

void WtRtRunner::parser_release(const char* id)
{
	if (_cb_parser_evt)
		_cb_parser_evt(EVENT_PARSER_RELEASE, id);
}

void WtRtRunner::parser_subscribe(const char* id, const char* code)
{
	if (_cb_parser_sub)
		_cb_parser_sub(id, code, true);
}

void WtRtRunner::parser_unsubscribe(const char* id, const char* code)
{
	if (_cb_parser_sub)
		_cb_parser_sub(id, code, false);
}

void WtRtRunner::on_parser_quote(const char* id, WTSTickStruct* curTick, bool bNeedSlice /* = true */)
{
	ParserAdapterPtr adapter = _parsers.getAdapter(id);
	if (adapter)
	{
		WTSTickData* newTick = WTSTickData::create(*curTick);
		adapter->handleQuote(newTick, bNeedSlice);
		newTick->release();
	}
}

#pragma endregion 

#pragma region "Extended Executer"
void WtRtRunner::executer_init(const char* id)
{
	if (_cb_exec_init)
		_cb_exec_init(id);
}

void WtRtRunner::executer_set_position(const char* id, const char* stdCode, double target)
{
	if (_cb_exec_cmd)
		_cb_exec_cmd(id, stdCode, target);
}
#pragma endregion
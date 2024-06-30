/*!
 * /file WtRunner.cpp
 * /project	WonderTrader
 *
 * /author Wesley
 * /date 2020/03/30
 * 
 * /brief 
 */
#include "WtRunner.h"

#include "../WtCore/WtHelper.h"
#include "../WtCore/HftStraContext.h"

#include "../Includes/WTSVariant.hpp"
#include "../WTSTools/WTSLogger.h"
#include "../Share/StdUtils.hpp"
#include "../Share/StrUtil.hpp"
#include "../Share/JsonToVariant.hpp"

#include "../WTSUtils/SignalHook.hpp"

#include <boost/circular_buffer.hpp>


#ifdef _WIN32
#define my_stricmp _stricmp
#else
#define my_stricmp strcasecmp
#endif


const char* getBinDir() {
	static std::string basePath;

	if (basePath.empty()) {
		basePath = boost::filesystem::initial_path<boost::filesystem::path>().string();
		basePath = StrUtil::standardisePath(basePath);
	}

	return basePath.c_str();
}

WtRunner::WtRunner(): _data_store(NULL), _is_hft(false), _is_sel(false) {
	install_signal_hooks([](const char* message) {
		WTSLogger::error(message);
	});
}

WtRunner::~WtRunner() {}


bool WtRunner::init() {
	std::string path = WtHelper::getCWD() + "logcfg.json";
	WTSLogger::init(path.c_str());

	WtHelper::setInstDir(getBinDir());

	return true;
}

bool WtRunner::config(const char* cfgFile) {
	std::string json;
	BoostFile::read_file_contents(cfgFile, json);
	rj::Document document;
	document.Parse(json.c_str());

	if (document.HasParseError()) {
		auto ec = document.GetParseError();
		WTSLogger::error("Configuration file parsing failed");
		return false;
	}

	_config = WTSVariant::createObject();
	jsonToVariant(document, _config);

	// strategies json part
	std::string strategiesString;
	BoostFile::read_file_contents("strategies.json", strategiesString);
	rj::Document strategiesDocument;
	strategiesDocument.Parse(strategiesString.c_str());

	if (strategiesDocument.HasParseError()) {
		auto ec = strategiesDocument.GetParseError();
		WTSLogger::error("strategies file parsing failed");
		return false;
	}

	this->_strategy_content = WTSVariant::createObject();
	jsonToVariant(strategiesDocument, this->_strategy_content);

	//基础数据文件
	WTSVariant* cfgBF = _config->get("basefiles");
	if (cfgBF->get("session"))
		_bd_mgr.loadSessions(cfgBF->getCString("session"));

	if (cfgBF->get("commodity"))
		_bd_mgr.loadCommodities(cfgBF->getCString("commodity"));

	if (cfgBF->get("contract"))
		_bd_mgr.loadContracts(cfgBF->getCString("contract"));

	if (cfgBF->get("holiday"))
		_bd_mgr.loadHolidays(cfgBF->getCString("holiday"));

	//初始化运行环境
	initEngine();

	//初始化数据管理
	initDataMgr();

	if (!initActionPolicy())
		return false;

	//初始化行情通道
	initParsers();

	//初始化交易通道
	initTraders();

	initHftStrategies();

	return true;
}


bool WtRunner::initHftStrategies() {
	WTSVariant* cfg = this->_strategy_content->get("strategies"); // tqz modify.

	if (cfg == NULL || cfg->type() != WTSVariant::VT_Object) // value of strategies.
		return false;

	cfg = cfg->get("hft");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array) // value of hft.
		return false;

	std::string path = WtHelper::getCWD() + "hft/";
	_hft_stra_mgr.loadFactories(path.c_str());

	for (uint32_t idx = 0; idx < cfg->size(); idx++) {
		WTSVariant* cfgItem = cfg->get(idx);
		if (!cfgItem->getBoolean("active"))
			continue;

		const char* id = cfgItem->getCString("id");
		const char* name = cfgItem->getCString("name");
		bool agent = cfgItem->getBoolean("agent");
		HftStrategyPtr stra = _hft_stra_mgr.createStrategy(name, id);
		if (stra == NULL)
			continue;

		stra->self()->init(cfgItem->get("params"));
		HftStraContext* ctx = new HftStraContext(&_hft_engine, id, agent);
		ctx->set_strategy(stra->self());

		const char* traderid = cfgItem->getCString("trader");
		TraderAdapterPtr trader = _traders.getAdapter(traderid);
		if (trader) {
			ctx->setTrader(trader.get());
			trader->addSink(ctx);
		} else {
			WTSLogger::error("Trader %s not exists, binding trader to HFT strategy failed", traderid);
		}

		_hft_engine.addContext(HftContextPtr(ctx));
	}

	return true;
}


bool WtRunner::initEngine() {
	WTSVariant* cfg = _config->get("env");
	if (cfg == NULL)
		return false;

	_is_hft = true;

	WTSLogger::info("Trading enviroment initialzied with engine: HFT");
	_hft_engine.init(cfg, &_bd_mgr, &_data_mgr, &_hot_mgr);
	_engine = &_hft_engine;

	_engine->set_adapter_mgr(&_traders);

	return true;
}

bool WtRunner::initActionPolicy() {
	return _act_policy.init(_config->getCString("bspolicy"));
}

bool WtRunner::initDataMgr() {
	WTSVariant* cfg = _config->get("data");
	if (cfg == NULL)
		return false;

	_data_mgr.init(cfg, _engine);
	WTSLogger::info("Data manager initialized");

	return true;
}

bool WtRunner::initParsers() {
	WTSVariant* cfg = _config->get("parsers");
	if (cfg == NULL)
		return false;

	uint32_t count = 0;
	for (uint32_t idx = 0; idx < cfg->size(); idx++) {
		WTSVariant* cfgItem = cfg->get(idx);
		if(!cfgItem->getBoolean("active"))
			continue;

		const char* id = cfgItem->getCString("id");

		ParserAdapterPtr adapter(new ParserAdapter);
		adapter->init(id, cfgItem, _engine, &_bd_mgr, &_hot_mgr);

		_parsers.addAdapter(id, adapter);

		count++;
	}

	WTSLogger::info("%u parsers loaded", count);
	return true;
}


bool WtRunner::initTraders() {
	WTSVariant* cfg = _config->get("traders");
	if (cfg == NULL || cfg->type() != WTSVariant::VT_Array)
		return false;
	
	uint32_t count = 0;
	for (uint32_t idx = 0; idx < cfg->size(); idx++) {
		WTSVariant* cfgItem = cfg->get(idx);
		if (!cfgItem->getBoolean("active"))
			continue;

		const char* id = cfgItem->getCString("id");

		TraderAdapterPtr adapter(new TraderAdapter);
		adapter->init(id, cfgItem, &_bd_mgr, &_act_policy);

		_traders.addAdapter(id, adapter);

		count++;
	}

	WTSLogger::info("%u traders loaded", count);

	return true;
}


void WtRunner::run(bool bAsync /* = false */) {
	try {
		_parsers.run();
		_traders.run();

		_engine->run(bAsync);
	} catch (...) {
		print_stack_trace([](const char* message) {
			WTSLogger::error(message);
		});
	}
}
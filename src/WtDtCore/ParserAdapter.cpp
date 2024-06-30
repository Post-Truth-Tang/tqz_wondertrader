/*!
 * \file ParserAdapter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#include "ParserAdapter.h"
#include "DataManager.h"
#include "StateMonitor.h"

#include "../Share/TimeUtils.hpp"
#include "../Share/StrUtil.hpp"
#include "../Includes/WTSParams.hpp"
#include "../Includes/WTSContractInfo.hpp"
#include "../Includes/WTSDataDef.hpp"

#include "../WTSTools/WTSBaseDataMgr.h"
#include "../WTSTools/WTSLogger.h"


//////////////////////////////////////////////////////////////////////////
//ParserAdapter
ParserAdapter::ParserAdapter(WTSBaseDataMgr * bgMgr, DataManager* dtMgr)
	: m_pParser(NULL)
	, m_funcCreate(NULL)
	, m_funcDelete(NULL)
	, m_bStopped(false)
	, m_bdMgr(bgMgr)
	, m_dtMgr(dtMgr)
{
}


ParserAdapter::~ParserAdapter()
{
}

bool ParserAdapter::initAdapter(WTSParams* params, FuncCreateParser funcCreate, FuncDeleteParser funcDelete)
{
	m_funcCreate = funcCreate;
	m_funcDelete = funcDelete;

	std::string strFilter = params->getString("filter");
	if(!strFilter.empty())
	{
		const StringVector &ayFilter = StrUtil::split(strFilter, ",");
		auto it = ayFilter.begin();
		for(; it != ayFilter.end(); it++)
		{
			m_codeFilters.insert(*it);
		}
	}

	m_pParser = m_funcCreate();
	if(m_pParser)
	{
		m_pParser->registerSpi(this);

		if(m_pParser->init(params))
		{
			ContractSet contractSet;
			WTSArray* ayContract = m_bdMgr->getContracts();
			WTSArray::Iterator it = ayContract->begin();
			for (; it != ayContract->end(); it++)
			{
				WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);

				const char * fullCode = contract->getFullCode();
				//��������������Ϊ�գ�����й���
				if (!m_codeFilters.empty())
				{
					for (auto& fItem : m_codeFilters)
					{
						//���fullcode����ʽ��CFFEX.IF2106�������չ�������ĳ��Ƚ��бȽϣ����ƥ�䣬����
						if (strncmp(fullCode, fItem.c_str(), fItem.size()) == 0)
							contractSet.insert(contract->getFullCode());
					}
				}
				else
				{
					contractSet.insert(contract->getFullCode());
				}
			}

			ayContract->release();

			m_pParser->subscribe(contractSet);
			m_pParser->connect();
			contractSet.clear();			
		}
		else
		{
			//WTSLogger::info("����ģ���ʼ��ʧ��,ģ��ӿڳ�ʼ��ʧ��...");
			WTSLogger::error("Initializing of market data parser failed...");
		}
	}
	else
	{
		//WTSLogger::info("����ģ���ʼ��ʧ��,��ȡģ��ӿ�ʧ��...");
		WTSLogger::error("Creation of market data parser failed...");
	}

	return true;
}

void ParserAdapter::release()
{
	m_bStopped = true;
	if (m_pParser)
	{
		m_pParser->release();
	}

	m_funcDelete(m_pParser);
}

void ParserAdapter::handleSymbolList( const WTSArray* aySymbols )
{
	
}

void ParserAdapter::handleTransaction(WTSTransData* transData)
{
	if (m_bStopped)
		return;


	if (transData->actiondate() == 0 || transData->tradingdate() == 0)
		return;

	WTSContractInfo* contract = m_bdMgr->getContract(transData->code(), transData->exchg());
	if (contract == NULL)
		return;

	m_dtMgr->writeTransaction(transData);
}

void ParserAdapter::handleOrderDetail(WTSOrdDtlData* ordDetailData)
{
	if (m_bStopped)
		return;

	if (ordDetailData->actiondate() == 0 || ordDetailData->tradingdate() == 0)
		return;

	WTSContractInfo* contract = m_bdMgr->getContract(ordDetailData->code(), ordDetailData->exchg());
	if (contract == NULL)
		return;

	m_dtMgr->writeOrderDetail(ordDetailData);
}

void ParserAdapter::handleOrderQueue(WTSOrdQueData* ordQueData)
{
	if (m_bStopped)
		return;

	if (ordQueData->actiondate() == 0 || ordQueData->tradingdate() == 0)
		return;

	WTSContractInfo* contract = m_bdMgr->getContract(ordQueData->code(), ordQueData->exchg());
	if (contract == NULL)
		return;
		
	m_dtMgr->writeOrderQueue(ordQueData);
}

void ParserAdapter::handleQuote( WTSTickData *quote, bool bNeedSlice )
{
	if (m_bStopped)
		return;

	if (quote->actiondate() == 0 || quote->tradingdate() == 0)
		return;

	WTSContractInfo* contract = m_bdMgr->getContract(quote->code(), quote->exchg());
	if (contract == NULL)
		return;

	if (!m_dtMgr->writeTick(quote, bNeedSlice))
		return;
}

void ParserAdapter::handleParserLog( WTSLogLevel ll, const char* format, ... )
{
	if (m_bStopped)
		return;

	char szBuf[2048] = {0};
	va_list args;
	va_start(args, format);        
	vsprintf(szBuf, format, args);
	va_end(args);

	WTSLogger::log2("parser", ll, szBuf);
}

IBaseDataMgr* ParserAdapter::getBaseDataMgr()
{
	return m_bdMgr;
}


//////////////////////////////////////////////////////////////////////////
//ParserAdapterMgr
ParserAdapterVec ParserAdapterMgr::m_ayAdapters;

void ParserAdapterMgr::releaseAdapters()
{
	ParserAdapterVec::iterator it = m_ayAdapters.begin();
	for(; it != m_ayAdapters.end(); it++)
	{
		(*it)->release();
	}

	m_ayAdapters.clear();
}

void ParserAdapterMgr::addAdapter(ParserAdapterPtr& adapter)
{
	m_ayAdapters.emplace_back(adapter);
}

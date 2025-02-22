#pragma once
#include "DataDefine.h"
#include "MysqlDB.hpp"

#include "../Includes/FasterDefs.h"
#include "../Includes/IDataWriter.h"
#include "../Share/StdUtils.hpp"
#include "../Share/BoostMappingFile.hpp"

#include <queue>

typedef std::shared_ptr<BoostMappingFile> BoostMFPtr;
typedef std::shared_ptr<MysqlDb>	MysqlDbPtr;

NS_OTP_BEGIN
class WTSContractInfo;
NS_OTP_END

USING_NS_OTP;

class WtDataWriter : public IDataWriter
{
public:
	WtDataWriter();
	~WtDataWriter();	

private:
	template<typename HeaderType, typename T>
	void* resizeRTBlock(BoostMFPtr& mfPtr, uint32_t nCount);

	void  proc_loop();

	void  check_loop();

	void  init_db();

	uint32_t  dump_hisdata_to_file(WTSContractInfo* ct);

	uint32_t  dump_hisdata_to_db(WTSContractInfo* ct);


public:
	virtual bool init(WTSVariant* params, IDataWriterSink* sink) override;
	virtual void release() override;

	virtual bool writeTick(WTSTickData* curTick, bool bNeedSlice = true) override;

	virtual bool writeOrderQueue(WTSOrdQueData* curOrdQue) override;

	virtual bool writeOrderDetail(WTSOrdDtlData* curOrdDetail) override;

	virtual bool writeTransaction(WTSTransData* curTrans) override;

	virtual void transHisData(const char* sid) override;
	
	virtual bool isSessionProceeded(const char* sid) override;

	virtual WTSTickData* getCurTick(const char* code, const char* exchg = "") override;

private:
	IDataWriterSink*	_sink;
	IBaseDataMgr*		_bd_mgr;

	typedef struct _DBConfig
	{
		bool	_active;
		char	_host[64];
		int32_t	_port;
		char	_dbname[32];
		char	_user[32];
		char	_pass[32];

		_DBConfig() { memset(this, 0, sizeof(_DBConfig)); }
	} DBConfig;

	DBConfig	_db_conf;
	MysqlDbPtr	_db_conn;

	typedef struct _KBlockPair
	{
		RTKlineBlock*	_block;
		BoostMFPtr		_file;
		StdUniqueMutex	_mutex;
		uint64_t		_lasttime;

		_KBlockPair()
		{
			_block = NULL;
			_file = NULL;
			_lasttime = 0;
		}

	} KBlockPair;
	typedef faster_hashmap<std::string, KBlockPair*>	KBlockFilesMap;

	typedef struct _TickBlockPair
	{
		RTTickBlock*	_block;
		BoostMFPtr		_file;
		StdUniqueMutex	_mutex;
		uint64_t		_lasttime;

		std::shared_ptr< std::ofstream>	_fstream;

		_TickBlockPair()
		{
			_block = NULL;
			_file = NULL;
			_fstream = NULL;
			_lasttime = 0;
		}
	} TickBlockPair;
	typedef faster_hashmap<std::string, TickBlockPair*>	TickBlockFilesMap;

	typedef struct _TransBlockPair
	{
		RTTransBlock*	_block;
		BoostMFPtr		_file;
		StdUniqueMutex	_mutex;
		uint64_t		_lasttime;

		std::shared_ptr< std::ofstream>	_fstream;

		_TransBlockPair()
		{
			_block = NULL;
			_file = NULL;
			_fstream = NULL;
			_lasttime = 0;
		}
	} TransBlockPair;
	typedef faster_hashmap<std::string, TransBlockPair*>	TransBlockFilesMap;

	typedef struct _OdeDtlBlockPair
	{
		RTOrdDtlBlock*	_block;
		BoostMFPtr		_file;
		StdUniqueMutex	_mutex;
		uint64_t		_lasttime;

		std::shared_ptr< std::ofstream>	_fstream;

		_OdeDtlBlockPair()
		{
			_block = NULL;
			_file = NULL;
			_fstream = NULL;
			_lasttime = 0;
		}
	} OrdDtlBlockPair;
	typedef faster_hashmap<std::string, OrdDtlBlockPair*>	OrdDtlBlockFilesMap;

	typedef struct _OdeQueBlockPair
	{
		RTOrdQueBlock*	_block;
		BoostMFPtr		_file;
		StdUniqueMutex	_mutex;
		uint64_t		_lasttime;

		std::shared_ptr< std::ofstream>	_fstream;

		_OdeQueBlockPair()
		{
			_block = NULL;
			_file = NULL;
			_fstream = NULL;
			_lasttime = 0;
		}
	} OrdQueBlockPair;
	typedef faster_hashmap<std::string, OrdQueBlockPair*>	OrdQueBlockFilesMap;
	

	KBlockFilesMap	_rt_min1_blocks;
	KBlockFilesMap	_rt_min5_blocks;

	TickBlockFilesMap	_rt_ticks_blocks;
	TransBlockFilesMap	_rt_trans_blocks;
	OrdDtlBlockFilesMap _rt_orddtl_blocks;
	OrdQueBlockFilesMap _rt_ordque_blocks;

	StdUniqueMutex	_mtx_tick_cache;
	faster_hashmap<std::string, uint32_t> _tick_cache_idx;
	BoostMFPtr		_tick_cache_file;
	RTTickCache*	_tick_cache_block;

	typedef std::function<void()> TaskInfo;
	std::queue<TaskInfo>	_tasks;
	StdThreadPtr			_task_thrd;
	StdUniqueMutex			_task_mtx;
	StdCondVariable			_task_cond;

	std::string		_base_dir;
	std::string		_cache_file;
	uint32_t		_log_group_size;
	bool			_async_proc;

	StdCondVariable	 _proc_cond;
	StdUniqueMutex _proc_mtx;
	std::queue<std::string> _proc_que;
	StdThreadPtr	_proc_thrd;
	StdThreadPtr	_proc_chk;
	bool			_terminated;

	bool			_save_tick_log;

	bool			_disable_tick;
	bool			_disable_min1;
	bool			_disable_min5;
	bool			_disable_day;

	bool			_disable_trans;
	bool			_disable_ordque;
	bool			_disable_orddtl;
	
	std::map<std::string, uint32_t> _proc_date;

private:
	void loadCache();

	bool updateCache(WTSContractInfo* ct, WTSTickData* curTick, bool bNeedSlice = true);

	void pipeToTicks(WTSContractInfo* ct, WTSTickData* curTick);

	void pipeToKlines(WTSContractInfo* ct, WTSTickData* curTick);

	KBlockPair* getKlineBlock(WTSContractInfo* ct, WTSKlinePeriod period, bool bAutoCreate = true);

	TickBlockPair* getTickBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate = true);
	TransBlockPair* getTransBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate = true);
	OrdDtlBlockPair* getOrdDtlBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate = true);
	OrdQueBlockPair* getOrdQueBlock(WTSContractInfo* ct, uint32_t curDate, bool bAutoCreate = true);

	template<typename T>
	void	releaseBlock(T* block);

	void pushTask(TaskInfo task);
};


/*!
 * \file WtDtPorter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#include "WtDtHelper.h"
#include "../Share/StrUtil.hpp"
#include "../Share/TimeUtils.hpp"
#include "../Share/BoostFile.hpp"

#include "../Includes/WTSTypes.h"
#include "../WtDataWriter/DataDefine.h"
#include "../WTSTools/WTSCmpHelper.hpp"

#include "../Includes/WTSDataDef.hpp"
#include "../WTSTools/WTSDataFactory.h"

#include "../Includes/WTSSessionInfo.hpp"

#include <rapidjson/document.h>

namespace rj = rapidjson;

USING_NS_OTP;

#ifdef _WIN32
#define my_stricmp _stricmp
#else
#define my_stricmp strcasecmp
#endif

uint32_t strToTime(const char* strTime, bool bHasSec = false)
{
	std::string str;
	const char *pos = strTime;
	while (strlen(pos) > 0)
	{
		if (pos[0] != ':')
		{
			str.append(pos, 1);
		}
		pos++;
	}

	uint32_t ret = strtoul(str.c_str(), NULL, 10);
	if (ret > 10000 && !bHasSec)
		ret /= 100;

	return ret;
}

uint32_t strToDate(const char* strDate)
{
	StringVector ay = StrUtil::split(strDate, "/");
	if (ay.size() == 1)
		ay = StrUtil::split(strDate, "-");
	std::stringstream ss;
	if (ay.size() > 1)
	{
		auto pos = ay[2].find(" ");
		if (pos != std::string::npos)
			ay[2] = ay[2].substr(0, pos);
		ss << ay[0] << (ay[1].size() == 1 ? "0" : "") << ay[1] << (ay[2].size() == 1 ? "0" : "") << ay[2];
	}
	else
		ss << ay[0];

	return strtoul(ss.str().c_str(), NULL, 10);
}

void dump_bars(WtString binFolder, WtString csvFolder, WtString strFilter /* = "" */, FuncLogCallback cbLogger /* = NULL */)
{
	std::string srcFolder = StrUtil::standardisePath(binFolder);
	if (!BoostFile::exists(srcFolder.c_str()))
	{
		if (cbLogger)
			cbLogger(StrUtil::printf("目录%s不存在", binFolder).c_str());
		return;
	}

	if (!BoostFile::exists(csvFolder))
		BoostFile::create_directories(csvFolder);

	boost::filesystem::path myPath(srcFolder);
	boost::filesystem::directory_iterator endIter;
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)
	{
		if (boost::filesystem::is_directory(iter->path()))
			continue;

		if (iter->path().extension() != ".dsb")
			continue;

		const std::string& path = iter->path().string();

		std::string fileCode = iter->path().stem().string();

		if (cbLogger)
			cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());

		std::string buffer;
		BoostFile::read_file_contents(path.c_str(), buffer);
		if (buffer.size() < sizeof(HisKlineBlock))
		{
			if (cbLogger)
				cbLogger(StrUtil::printf("文件%s头部校验失败", binFolder).c_str());
			continue;
		}

		HisKlineBlock* tBlock = (HisKlineBlock*)buffer.c_str();
		if (tBlock->_version == BLOCK_VERSION_CMP)
		{
			//压缩版本,要重新检查文件大小
			HisKlineBlockV2* kBlockV2 = (HisKlineBlockV2*)buffer.c_str();

			if (buffer.size() != (sizeof(HisKlineBlockV2) + kBlockV2->_size))
			{
				if (cbLogger)
					cbLogger(StrUtil::printf("文件%s头部校验失败", binFolder).c_str());
				continue;
			}

			//需要解压
			if (cbLogger)
				cbLogger(StrUtil::printf("正在解压数据...").c_str());
			std::string buf = WTSCmpHelper::uncompress_data(kBlockV2->_data, (uint32_t)kBlockV2->_size);

			//将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
			buffer.resize(sizeof(HisTickBlock));
			buffer.append(buf);
			kBlockV2 = (HisKlineBlockV2*)buffer.c_str();
			kBlockV2->_version = BLOCK_VERSION_RAW;
		}

		HisKlineBlock* klineBlk = (HisKlineBlock*)buffer.c_str();

		auto kcnt = (buffer.size() - sizeof(HisKlineBlock)) / sizeof(WTSBarStruct);
		if (kcnt <= 0)
			continue;

		std::string filename = StrUtil::standardisePath(csvFolder);
		filename += fileCode;
		filename += ".csv";

		if (cbLogger)
			cbLogger(StrUtil::printf("正在写入%s...", filename.c_str()).c_str());

		std::stringstream ss;
		ss << "date,time,open,high,low,close,settle,turnover,volume,open_interest,diff_interest" << std::endl;

		for (uint32_t i = 0; i < kcnt; i++)
		{
			const WTSBarStruct& curBar = klineBlk->_bars[i];
			ss << curBar.date << ","
				<< curBar.time << ","
				<< curBar.high << ","
				<< curBar.low << ","
				<< curBar.close << ","
				<< curBar.vol << ","
				<< curBar.money << ","
				<< curBar.hold << ","
				<< curBar.add << ","
				<< curBar.settle << std::endl;
		}

		BoostFile::write_file_contents(filename.c_str(), ss.str().c_str(), (uint32_t)ss.str().size());

		if (cbLogger)
			cbLogger(StrUtil::printf("%s写入完成,共%u条bar", filename.c_str(), kcnt).c_str());
	}

	if (cbLogger)
		cbLogger(StrUtil::printf("目录%s全部导出完成...", binFolder).c_str());
}

void dump_ticks(WtString binFolder, WtString csvFolder, WtString strFilter /* = "" */, FuncLogCallback cbLogger /* = NULL */)
{
	std::string srcFolder = StrUtil::standardisePath(binFolder);
	if (!BoostFile::exists(srcFolder.c_str()))
	{
		if (cbLogger)
			cbLogger(StrUtil::printf("目录%s不存在", binFolder).c_str());
		return;
	}

	if (!BoostFile::exists(csvFolder))
		BoostFile::create_directories(csvFolder);

	boost::filesystem::path myPath(srcFolder);
	boost::filesystem::directory_iterator endIter;
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)
	{
		if (boost::filesystem::is_directory(iter->path()))
			continue;

		if (iter->path().extension() != ".dsb")
			continue;

		const std::string& path = iter->path().string();

		std::string fileCode = iter->path().stem().string();

		if (cbLogger)
			cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());

		std::string buffer;
		BoostFile::read_file_contents(path.c_str(), buffer);
		if (buffer.size() < sizeof(HisTickBlock))
		{
			if (cbLogger)
				cbLogger(StrUtil::printf("文件%s头部校验失败", binFolder).c_str());
			continue;
		}

		HisTickBlock* tBlock = (HisTickBlock*)buffer.c_str();
		if (tBlock->_version == BLOCK_VERSION_CMP)
		{
			//压缩版本,要重新检查文件大小
			HisTickBlockV2* tBlockV2 = (HisTickBlockV2*)buffer.c_str();

			if (buffer.size() != (sizeof(HisTickBlockV2) + tBlockV2->_size))
			{
				if (cbLogger)
					cbLogger(StrUtil::printf("文件%s头部校验失败", binFolder).c_str());
				continue;
			}

			//需要解压
			if (cbLogger)
				cbLogger(StrUtil::printf("正在解压数据...").c_str());
			std::string buf = WTSCmpHelper::uncompress_data(tBlockV2->_data, (uint32_t)tBlockV2->_size);

			//将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
			buffer.resize(sizeof(HisTickBlock));
			buffer.append(buf);
			tBlockV2 = (HisTickBlockV2*)buffer.c_str();
			tBlockV2->_version = BLOCK_VERSION_RAW;
		}

		HisTickBlock* tickBlk = (HisTickBlock*)buffer.c_str();

		auto tcnt = (buffer.size() - sizeof(HisTickBlock)) / sizeof(WTSTickStruct);
		if (tcnt <= 0)
			continue;

		std::string filename = StrUtil::standardisePath(csvFolder);
		filename += fileCode;
		filename += ".csv";

		if (cbLogger)
			cbLogger(StrUtil::printf("正在写入%s...", filename.c_str()).c_str());

		std::stringstream ss;
		ss.setf(std::ios::fixed, std::ios::floatfield);
		ss.precision(3);
		ss << "code,tradingdate,actiondate,actiontime,price,open,high,low,settle,preclose,"
			<< "presettle,preinterest,total_volume,total_turnover,open_interest,volume,turnover,additional,";
		for (int i = 0; i < 10; i++)
		{
			bool hasTail = (i != 9);
			ss << "bidprice" << i + 1 << "," << "bidqty" << i + 1 << "," << "askprice" << i + 1 << "," << "askqty" << i + 1 << (hasTail ? "," : "");
		}
		ss << std::endl;

		for (uint32_t i = 0; i < tcnt; i++)
		{
			const WTSTickStruct& curTick = tickBlk->_ticks[i];
			ss << curTick.code << ","
				<< curTick.trading_date << ","
				<< curTick.action_date << ","
				<< curTick.action_time << ","
				<< curTick.price << ","
				<< curTick.open << ","
				<< curTick.high << ","
				<< curTick.low << ","
				<< curTick.settle_price << ","
				<< curTick.pre_close << ","
				<< curTick.pre_settle << ","
				<< curTick.pre_interest << ","
				<< curTick.total_volume << ","
				<< curTick.total_turnover << ","
				<< curTick.open_interest << ","
				<< curTick.volume << ","
				<< curTick.turn_over << ","
				<< curTick.diff_interest << ",";

			for (int j = 0; j < 10; j++)
			{
				bool hasTail = (j != 9);
				ss << curTick.bid_prices[j] << "," << curTick.bid_qty[j] << "," << curTick.ask_prices[j] << "," << curTick.ask_qty[j] << (hasTail ? "," : "");
			}
			ss << std::endl;
		}

		BoostFile::write_file_contents(filename.c_str(), ss.str().c_str(), (uint32_t)ss.str().size());

		if (cbLogger)
			cbLogger(StrUtil::printf("%s写入完成,共%u条tick数据", filename.c_str(), tcnt).c_str());
	}

	if (cbLogger)
		cbLogger(StrUtil::printf("目录%s全部导出完成...", binFolder).c_str());
}

void trans_csv_bars(WtString csvFolder, WtString binFolder, WtString period, FuncLogCallback cbLogger /* = NULL */)
{
	if (!BoostFile::exists(csvFolder))
		return;

	if (!BoostFile::exists(binFolder))
		BoostFile::create_directories(binFolder);

	WTSKlinePeriod kp = KP_DAY;
	if (my_stricmp(period, "m1") == 0)
		kp = KP_Minute1;
	else if (my_stricmp(period, "m5") == 0)
		kp = KP_Minute5;
	else
		kp = KP_DAY;

	boost::filesystem::path myPath(csvFolder);
	boost::filesystem::directory_iterator endIter;
	for (boost::filesystem::directory_iterator iter(myPath); iter != endIter; iter++)
	{
		if (boost::filesystem::is_directory(iter->path()))
			continue;

		if (iter->path().extension() != ".csv")
			continue;

		const std::string& path = iter->path().string();

		std::ifstream ifs;
		ifs.open(path.c_str());

		if(cbLogger)
			cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());

		char buffer[512];
		bool headerskipped = false;
		std::vector<WTSBarStruct> bars;
		while (!ifs.eof())
		{
			ifs.getline(buffer, 512);
			if (strlen(buffer) == 0)
				continue;

			//跳过头部
			if (!headerskipped)
			{
				headerskipped = true;
				continue;
			}

			//逐行读取
			StringVector ay = StrUtil::split(buffer, ",");
			WTSBarStruct bs;
			bs.date = strToDate(ay[0].c_str());
			bs.time = TimeUtils::timeToMinBar(bs.date, strToTime(ay[1].c_str()));
			bs.open = strtod(ay[2].c_str(), NULL);
			bs.high = strtod(ay[3].c_str(), NULL);
			bs.low = strtod(ay[4].c_str(), NULL);
			bs.close = strtod(ay[5].c_str(), NULL);
			bs.vol = strtoul(ay[6].c_str(), NULL, 10);
			if (ay.size() > 7)
				bs.money = strtod(ay[7].c_str(), NULL);
			if (ay.size() > 8)
				bs.hold = (uint32_t)strtod(ay[8].c_str(), NULL);
			if (ay.size() > 9)
				bs.add = (int32_t)strtod(ay[9].c_str(), NULL);
			if (ay.size() > 10)
				bs.settle = strtod(ay[10].c_str(), NULL);
			bars.emplace_back(bs);

			if (bars.size() % 1000 == 0)
			{
				if (cbLogger)
					cbLogger(StrUtil::printf("已读取数据%u条", bars.size()).c_str());
			}
		}
		ifs.close();
		if (cbLogger)
			cbLogger(StrUtil::printf("数据文件%s全部读取完成,共%u条", path.c_str(), bars.size()).c_str());

		BlockType btype;
		switch (kp)
		{
		case KP_Minute1: btype = BT_HIS_Minute1; break;
		case KP_Minute5: btype = BT_HIS_Minute5; break;
		default: btype = BT_HIS_Day; break;
		}

		HisKlineBlockV2 kBlock;
		strcpy(kBlock._blk_flag, BLK_FLAG);
		kBlock._type = btype;
		kBlock._version = BLOCK_VERSION_CMP;

		std::string cmprsData = WTSCmpHelper::compress_data(bars.data(), (uint32_t)(sizeof(WTSBarStruct)*bars.size()));
		kBlock._size = cmprsData.size();

		std::string filename = StrUtil::standardisePath(binFolder);
		filename += iter->path().stem().string();
		filename += ".dsb";

		BoostFile bf;
		if (bf.create_new_file(filename.c_str()))
		{
			bf.write_file(&kBlock, sizeof(HisKlineBlockV2));
		}
		bf.write_file(cmprsData);
		bf.close_file();
		if (cbLogger)
			cbLogger(StrUtil::printf("数据已转储至%s", filename.c_str()).c_str());
	}
}

bool trans_bars(WtString barFile, FuncGetBarItem getter, int count, WtString period, FuncLogCallback cbLogger /* = NULL */)
{
	if (count == 0)
	{
		if (cbLogger)
			cbLogger("K线数据条数为0");
		return false;
	}

	BlockType bType = BT_HIS_Day;
	if (my_stricmp(period, "m1") == 0)
		bType = BT_HIS_Minute1;
	else if (my_stricmp(period, "m5") == 0)
		bType = BT_HIS_Minute5;
	else if(my_stricmp(period, "d") == 0)
		bType = BT_HIS_Day;
	else
	{
		if (cbLogger)
			cbLogger("周期只能为m1、m5或d");
		return false;
	}

	std::string buffer;
	buffer.resize(sizeof(WTSBarStruct)*count);
	WTSBarStruct* bars = (WTSBarStruct*)buffer.c_str();
	for(int i = 0; i < count; i++)
	{
		bool bSucc = getter(&bars[i], i);
		if (!bSucc)
			break;
	}

	if (cbLogger)
		cbLogger("K线数据已经读取完成，准备写入文件");

	std::string content;
	content.resize(sizeof(HisKlineBlockV2));
	HisKlineBlockV2* block = (HisKlineBlockV2*)content.data();
	strcpy(block->_blk_flag, BLK_FLAG);
	block->_version = BLOCK_VERSION_CMP;
	block->_type = bType;
	std::string cmp_data = WTSCmpHelper::compress_data(bars, buffer.size());
	block->_size = cmp_data.size();
	content.append(cmp_data);

	BoostFile bf;
	if (bf.create_new_file(barFile))
	{
		bf.write_file(content);
	}
	bf.close_file();

	if (cbLogger)
		cbLogger("K线数据写入文件成功");
	return true;
}

bool trans_ticks(WtString tickFile, FuncGetTickItem getter, int count, FuncLogCallback cbLogger/* = NULL*/)
{
	if (count == 0)
	{
		if (cbLogger)
			cbLogger("Tick数据条数为0");
		return false;
	}

	std::string buffer;
	buffer.resize(sizeof(WTSTickStruct)*count);
	WTSTickStruct* ticks = (WTSTickStruct*)buffer.c_str();
	for (int i = 0; i < count; i++)
	{
		bool bSucc = getter(&ticks[i], i);
		if (!bSucc)
			break;
	}

	if (cbLogger)
		cbLogger("Tick数据已经读取完成，准备写入文件");

	std::string content;
	content.resize(sizeof(HisKlineBlockV2));
	HisKlineBlockV2* block = (HisKlineBlockV2*)content.data();
	strcpy(block->_blk_flag, BLK_FLAG);
	block->_version = BLOCK_VERSION_CMP;
	block->_type = BT_HIS_Ticks;
	std::string cmp_data = WTSCmpHelper::compress_data(ticks, buffer.size());
	block->_size = cmp_data.size();
	content.append(cmp_data);

	BoostFile bf;
	if (bf.create_new_file(tickFile))
	{
		bf.write_file(content);
	}
	bf.close_file();

	if (cbLogger)
		cbLogger("Tick数据写入文件成功");

	return true;
}

WtUInt32 read_dsb_ticks(WtString tickFile, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger /* = NULL */)
{
	std::string path = tickFile;

	if (cbLogger)
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());

	std::string buffer;
	BoostFile::read_file_contents(path.c_str(), buffer);
	if (buffer.size() < sizeof(HisTickBlock))
	{
		if (cbLogger)
			cbLogger(StrUtil::printf("文件%s头部校验失败", tickFile).c_str());
		return 0;
	}

	HisTickBlock* tBlock = (HisTickBlock*)buffer.c_str();
	if (tBlock->_version == BLOCK_VERSION_CMP)
	{
		//压缩版本,要重新检查文件大小
		HisTickBlockV2* tBlockV2 = (HisTickBlockV2*)buffer.c_str();

		if (buffer.size() != (sizeof(HisTickBlockV2) + tBlockV2->_size))
		{
			if (cbLogger)
				cbLogger(StrUtil::printf("文件%s头部校验失败", tickFile).c_str());
			return 0;
		}

		//需要解压
		if (cbLogger)
			cbLogger(StrUtil::printf("正在解压数据...").c_str());
		std::string buf = WTSCmpHelper::uncompress_data(tBlockV2->_data, (uint32_t)tBlockV2->_size);

		//将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
		buffer.resize(sizeof(HisTickBlock));
		buffer.append(buf);
		tBlockV2 = (HisTickBlockV2*)buffer.c_str();
		tBlockV2->_version = BLOCK_VERSION_RAW;
	}

	HisTickBlock* tickBlk = (HisTickBlock*)buffer.c_str();

	auto tcnt = (buffer.size() - sizeof(HisTickBlock)) / sizeof(WTSTickStruct);
	if (tcnt <= 0)
	{
		cbCnt(0);
		return 0;
	}

	cbCnt(tcnt);
	for (uint32_t i = 0; i < tcnt; i++)
	{
		WTSTickStruct& curTick = tickBlk->_ticks[i];
		cb(&curTick, i==tcnt-1);
	}

	if (cbLogger)
		cbLogger(StrUtil::printf("%s读取完成,共%u条tick数据", tickFile, tcnt).c_str());

	return (WtUInt32)tcnt;
}

WtUInt32 read_dsb_bars(WtString barFile, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger )
{
	std::string path = barFile;
	if (cbLogger)
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());

	std::string buffer;
	BoostFile::read_file_contents(path.c_str(), buffer);
	if (buffer.size() < sizeof(HisKlineBlock))
	{
		if (cbLogger)
			cbLogger(StrUtil::printf("文件%s头部校验失败", barFile).c_str());
		return 0;
	}

	HisKlineBlock* tBlock = (HisKlineBlock*)buffer.c_str();
	if (tBlock->_version == BLOCK_VERSION_CMP)
	{
		//压缩版本,要重新检查文件大小
		HisKlineBlockV2* kBlockV2 = (HisKlineBlockV2*)buffer.c_str();

		if (buffer.size() != (sizeof(HisKlineBlockV2) + kBlockV2->_size))
		{
			if (cbLogger)
				cbLogger(StrUtil::printf("文件%s头部校验失败", barFile).c_str());
			return 0;
		}

		//需要解压
		if (cbLogger)
			cbLogger(StrUtil::printf("正在解压数据...").c_str());
		std::string buf = WTSCmpHelper::uncompress_data(kBlockV2->_data, (uint32_t)kBlockV2->_size);

		//将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
		buffer.resize(sizeof(HisTickBlock));
		buffer.append(buf);
		kBlockV2 = (HisKlineBlockV2*)buffer.c_str();
		kBlockV2->_version = BLOCK_VERSION_RAW;
	}

	HisKlineBlock* klineBlk = (HisKlineBlock*)buffer.c_str();

	auto kcnt = (buffer.size() - sizeof(HisKlineBlock)) / sizeof(WTSBarStruct);
	if (kcnt <= 0)
	{
		cbCnt(0);
		return 0;
	}

	cbCnt(kcnt);

	for (uint32_t i = 0; i < kcnt; i++)
	{
		WTSBarStruct& curBar = klineBlk->_bars[i];
		cb(&curBar, i==kcnt-1);
	}

	if (cbLogger)
		cbLogger(StrUtil::printf("%s读取完成,共%u条bar", barFile, kcnt).c_str());

	return (WtUInt32)kcnt;
}

WtUInt32 read_dmb_bars(WtString barFile, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger)
{
	std::string path = barFile;

	std::string buffer;
	BoostFile::read_file_contents(path.c_str(), buffer);
	if (buffer.size() < sizeof(RTKlineBlock))
	{
		if (cbLogger)
			cbLogger(StrUtil::printf("文件%s头部校验失败", barFile).c_str());
		return 0;
	}

	RTKlineBlock* tBlock = (RTKlineBlock*)buffer.c_str();
	auto kcnt = tBlock->_size;
	if (kcnt <= 0)
	{
		cbCnt(0);
		return 0;
	}

	cbCnt(kcnt);

	for (uint32_t i = 0; i < kcnt; i++)
	{
		WTSBarStruct& curBar = tBlock->_bars[i];
		cb(&curBar, i == kcnt - 1);
	}

	if (cbLogger)
		cbLogger(StrUtil::printf("%s读取完成,共%u条bar", barFile, kcnt).c_str());

	return (WtUInt32)kcnt;
}

WtUInt32 read_dmb_ticks(WtString tickFile, FuncGetTicksCallback cb, FuncCountDataCallback cbCnt, FuncLogCallback cbLogger /* = NULL */)
{
	std::string path = tickFile;

	if (cbLogger)
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());

	std::string buffer;
	BoostFile::read_file_contents(path.c_str(), buffer);
	if (buffer.size() < sizeof(RTTickBlock))
	{
		if (cbLogger)
			cbLogger(StrUtil::printf("文件%s头部校验失败", tickFile).c_str());
		return 0;
	}

	RTTickBlock* tBlock = (RTTickBlock*)buffer.c_str();
	auto tcnt = tBlock->_size;
	if (tcnt <= 0)
	{
		cbCnt(0);
		return 0;
	}

	cbCnt(tcnt);

	for (uint32_t i = 0; i < tcnt; i++)
	{
		WTSTickStruct& curTick = tBlock->_ticks[i];
		cb(&curTick, i == tcnt - 1);
	}

	if (cbLogger)
		cbLogger(StrUtil::printf("%s读取完成,共%u条tick数据", tickFile, tcnt).c_str());

	return (WtUInt32)tcnt;
}

WtUInt32 resample_bars(WtString barFile, FuncGetBarsCallback cb, FuncCountDataCallback cbCnt, WtUInt64 fromTime, WtUInt64 endTime,
	WtString period, WtUInt32 times, WtString sessInfo, FuncLogCallback cbLogger /* = NULL */)
{
	WTSKlinePeriod kp;
	if(my_stricmp(period, "m1") == 0)
	{
		kp = KP_Minute1;
	}
	else if (my_stricmp(period, "m5") == 0)
	{
		kp = KP_Minute5;
	}
	else if (my_stricmp(period, "d") == 0)
	{
		kp = KP_DAY;
	}
	else
	{
		if (cbLogger)
			cbLogger(StrUtil::printf("周期%s不是基础周期...", period).c_str());
		return 0;
	}

	bool isDay = (kp == KP_DAY);

	if(isDay)
	{
		if(fromTime >= 100000000 || endTime > 100000000)
		{
			if (cbLogger)
				cbLogger("日线基础数据的开始时间结束时间应为日期，格式如yyyymmdd");
			return 0;
		}
	}
	else
	{
		if (fromTime < 100000000 || endTime < 100000000)
		{
			if (cbLogger)
				cbLogger("分钟线基础数据的开始时间结束时间应为日期，格式如yyyymmddHHMMSS");
			return 0;
		}
	}

	if(fromTime > endTime)
	{
		std::swap(fromTime, endTime);
	}

	WTSSessionInfo* sInfo = NULL;
	{
		rj::Document root;
		if (root.Parse(sessInfo).HasParseError())
		{
			if (cbLogger)
				cbLogger("交易时间模板解析失败");
			return 0;
		}

		int32_t offset = root["offset"].GetInt();

		sInfo = WTSSessionInfo::create("tmp", "tmp", offset);

		if (!root["auction"].IsNull())
		{
			const rj::Value& jAuc = root["auction"];
			sInfo->setAuctionTime(jAuc["from"].GetUint(), jAuc["to"].GetUint());
		}

		const rj::Value& jSecs = root["sections"];
		if (jSecs.IsNull() || !jSecs.IsArray())
		{
			if (cbLogger)
				cbLogger("交易时间模板格式错误");
			return 0;
		}

		for (const rj::Value& jSec : jSecs.GetArray())
		{
			sInfo->addTradingSection(jSec["from"].GetUint(), jSec["to"].GetUint());
		}
	}

	std::string path = barFile;
	if (cbLogger)
		cbLogger(StrUtil::printf("正在读取数据文件%s...", path.c_str()).c_str());

	std::string buffer;
	BoostFile::read_file_contents(path.c_str(), buffer);
	if (buffer.size() < sizeof(HisKlineBlock))
	{
		if (cbLogger)
			cbLogger(StrUtil::printf("文件%s头部校验失败", barFile).c_str());
		return 0;
	}

	HisKlineBlock* tBlock = (HisKlineBlock*)buffer.c_str();
	if (tBlock->_version == BLOCK_VERSION_CMP)
	{
		//压缩版本,要重新检查文件大小
		HisKlineBlockV2* kBlockV2 = (HisKlineBlockV2*)buffer.c_str();

		if (buffer.size() != (sizeof(HisKlineBlockV2) + kBlockV2->_size))
		{
			if (cbLogger)
				cbLogger(StrUtil::printf("文件%s头部校验失败", barFile).c_str());
			return 0;
		}

		//需要解压
		if (cbLogger)
			cbLogger(StrUtil::printf("正在解压数据...").c_str());
		std::string buf = WTSCmpHelper::uncompress_data(kBlockV2->_data, (uint32_t)kBlockV2->_size);

		//将原来的buffer只保留一个头部,并将所有tick数据追加到尾部
		buffer.resize(sizeof(HisTickBlock));
		buffer.append(buf);
		kBlockV2 = (HisKlineBlockV2*)buffer.c_str();
		kBlockV2->_version = BLOCK_VERSION_RAW;
	}

	HisKlineBlock* klineBlk = (HisKlineBlock*)buffer.c_str();

	auto kcnt = (buffer.size() - sizeof(HisKlineBlock)) / sizeof(WTSBarStruct);
	if (kcnt <= 0)
	{
		if (cbLogger)
			cbLogger(StrUtil::printf("%s数据为空", barFile).c_str());
		return 0;
	}

	//确定第一条K线的位置
	WTSBarStruct bar;
	if (isDay)
		bar.date = (uint32_t)fromTime;
	else
	{

		bar.time = fromTime % 100000000 + ((fromTime / 100000000) - 1990) * 100000000;
	}

	WTSBarStruct* pBar = std::lower_bound(klineBlk->_bars, klineBlk->_bars + (kcnt - 1), bar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
		if (isDay)
			return a.date < b.date;
		else
			return a.time < b.time;
	});


	uint32_t sIdx = pBar - klineBlk->_bars;
	if((isDay && pBar->date < bar.date) || (!isDay && pBar->time < bar.time))
	{
		//如果返回的K线的时间小于要查找的时间，说明没有符合条件的数据
		if (cbLogger)
			cbLogger("没有找到指定时间范围的K线");
		return 0;
	}
	else if (sIdx != 0 && ((isDay && pBar->date > bar.date) || (!isDay && pBar->time > bar.time)))
	{
		pBar--;
		sIdx--;
	}

	//确定最后一条K线的位置
	if (isDay)
		bar.date = (uint32_t)endTime;
	else
	{

		bar.time = endTime % 100000000 + ((endTime / 100000000) - 1990) * 100000000;
	}
	pBar = std::lower_bound(klineBlk->_bars, klineBlk->_bars + (kcnt - 1), bar, [isDay](const WTSBarStruct& a, const WTSBarStruct& b) {
		if (isDay)
			return a.date < b.date;
		else
			return a.time < b.time;
	});

	uint32_t eIdx = 0;
	if (pBar == NULL)
		eIdx = kcnt - 1;
	else
		eIdx = pBar - klineBlk->_bars;

	if (eIdx != 0 && ((isDay && pBar->date > bar.date) || (!isDay && pBar->time > bar.time)))
	{
		pBar--;
		eIdx--;
	}

	uint32_t hitCnt = eIdx - sIdx + 1;
	WTSKlineSlice* slice = WTSKlineSlice::create("", kp, 1, &klineBlk->_bars[sIdx], hitCnt);
	WTSDataFactory fact;
	WTSKlineData* kline = fact.extractKlineData(slice, kp, times, sInfo);
	if(kline == NULL)
	{
		if (cbLogger)
			cbLogger("K线重采样失败");
		return 0;
	}

	uint32_t newCnt = kline->size();
	cbCnt(newCnt);

	for (uint32_t i = 0; i < newCnt; i++)
	{
		WTSBarStruct* curBar = kline->at(i);
		cb(curBar, i == newCnt - 1);
	}

	if (cbLogger)
		cbLogger(StrUtil::printf("%s重采样完成,共将%u条bar重采样为%u条新bar", barFile, hitCnt, newCnt).c_str());

	
	kline->release();
	sInfo->release();
	slice->release();

	return (WtUInt32)newCnt;
}
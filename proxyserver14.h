#ifndef PROXYSERVER_H
#define PROXYSERVER_H

#include "concurrentqueue.h"
#include "mh/mhd/MHD.h"
#include "transaction.h"

//*************************** MODIFIED HERE!!! ****************************
#include <signal.h>
#include <mutex>
#include "xrenoIP.h"

//#define HAVE_28_CORES 1 
//#define HAVE_48_CORES 1
//#define HAVE_32_CORES 1
#define HAVE_2_CORES 1

#define HAVE_HT 1
#define USE_HT 1

#if defined(HAVE_28_CORES)
	static const uint cpuIrqCnt = 2;		// Mode 1, 2, 3: count of network IRQ CPUs
	static const uint cpuShowCols = 14;		// Count of columns of CPU stats in log file
#elif defined(HAVE_48_CORES)
	static const uint cpuIrqCnt = 5;		// Mode 1, 2, 3: count of network IRQ CPUs
	static const uint cpuShowCols = 24;		// Count of columns of CPU stats in log file
#elif defined(HAVE_32_CORES)
	static const uint cpuIrqCnt = 4;		// Mode 1, 2, 3: count of network IRQ CPUs
	static const uint cpuShowCols = 16;		// Count of columns of CPU stats in log file	
#elif defined(HAVE_2_CORES)
	static const uint cpuIrqCnt = 2;		// Mode 1, 2, 3: count of network IRQ CPUs
	static const uint cpuShowCols = 2;		// Count of columns of CPU stats in log file
#endif

#if defined(HAVE_HT)
	#if defined(USE_HT)
		static const uint cpuMapMode = 2;		// Mode 2: HT, first half CPUs are physical, second half are logical (based on same physical cores), second half CPUs also used for listen threads, for service & sending used first (0) CPU
	#else
		static const uint cpuMapMode = 1;		// Mode 1: HT, first half CPUs are physical, second half are logical (based on same physical cores), second half CPUs not used for listen threads, for service & sending used first (0) CPU
	#endif
#else
	static const uint cpuMapMode = 3;			// Mode 3: No HT, all CPUs are physical, all used for listen threads, for service & sending used first (0) CPU
#endif
										

static const uint cpuServSend = 0;		// Mode 1, 2, 3: CPU number for service & sending threads

static const uint sleepHeat = 1;		// Heater: heat threads sleep time in heat mode (milliseconds)
static const uint heatCirclesCnt = 300;	// Heater: count of calculation circles per heat step

static const uint sleepSend = 100;		// sleep time between sending sessions (milliseconds)

//*************************** MODIFIED END ********************************

const uint VESION_MAJOR = 1;
const uint VESION_MINOR = 4;

struct KeyManager {
    std::vector<char> PrivKey;
    std::vector<char> PubKey;
    std::vector<char> Bin_addr;
    std::string Text_PubKey;
    std::string Text_addres;

    bool parse(const std::string& line);

    std::string make_req_url(std::string& data);
};

struct Counters {
    std::atomic<uint64_t> qps = 0;
    char buff1[1024];
    std::atomic<uint64_t> qps_trash = 0;
    char buff2[1024];
    std::atomic<uint64_t> qps_no_req = 0;
    char buff3[1024];
    std::atomic<uint64_t> qps_inv = 0;
    char buff4[1024];
    std::atomic<uint64_t> qps_inv_sign = 0;
    char buff5[1024];
    std::atomic<uint64_t> qps_success = 0;

//	*************************** MODIFIED HERE!!! ****************************

	std::atomic<bool> getTx = false;
	
	std::atomic<bool> needSuicide = false;
	std::atomic<bool> needHeat = true;
	
	pid_t heaterPid = 0;
	uint sleepNoHeat = 250;							// Node: check period for no real tasks (milliseconds)
	
	std::atomic<uint> cpuBeg = 0;
	std::atomic<uint> cpuCnt = 0;
	
	std::atomic<uint> thrdServ = 0;
	std::atomic<uint> thrdSend = 0;
	std::atomic<uint> thrdHeat = 0;
	std::atomic<uint> thrdListen = 0;
	
	std::atomic<uint> coreStat[1024];
	std::atomic<uint> coreListenMap[1024];			// [listen thread number] => CPU number
	std::atomic<uint> coreHeaterMap[1024];			// [heater thread number] => CPU number
	
	std::atomic<uint> cpuHeaterBeg = 0;
	
	std::atomic<uint> heatSignalsCnt = 0;			// Heater: counter signals: SIGUSR1 increments, SIGUSR2 decrements 
	
	std::mutex coutLocker;
	std::mutex cntLocker;
	
//	*************************** MODIFIED END ********************************	

};

class PROXY_SERVER : public mh::mhd::MHD {
private:
    moodycamel::ConcurrentQueue<TX*>& send_message_queue;
    uint64_t pool_size;
    Counters& counters;
	KeyManager& key_manager;

public:
    PROXY_SERVER(int _port, moodycamel::ConcurrentQueue<TX*>& _send_message_queue, uint64_t _pool_size, Counters& _counter, KeyManager& key_holder);

    virtual ~PROXY_SERVER();

//	virtual bool run(int thread_number, Request& mhd_req, Response& mhd_resp);
    virtual bool run(const MHD_ConnectionInfo *client_addr, int thread_number, Request& mhd_req, Response& mhd_resp);		//*************************** MODIFIED SINGLE STRING HERE!!! ****************************

protected:
    virtual bool init();
};

#endif // PROXYSERVER_H

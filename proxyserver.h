#ifndef PROXYSERVER_H
#define PROXYSERVER_H

#include "concurrentqueue.h"
#include "mh/mhd/MHD.h"
#include "transaction.h"

//*************************** MODIFIED HERE!!! ****************************

#include <mutex>
#include "xrenoIP.h"

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
	
	std::mutex coutLocker;							//*************************** MODIFIED SINGLE STRING HERE!!! ****************************
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

    virtual bool run(int thread_number, Request& mhd_req, Response& mhd_resp);
	
protected:
    virtual bool init();
};

#endif // PROXYSERVER_H

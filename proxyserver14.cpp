#include "proxyserver.h"
#include "concurrentqueue.h"
#include "open_ssl_decor.h"
#include "rapidjson_new/document.h"

#include <iostream>

#include <arpa/inet.h>				//*************************** SINGLE STRING ADDED HERE!!! ****************************
#include <thread>					//*************************** SINGLE STRING ADDED HERE!!! ****************************

static const char P_TO[] = "p_to";
static const char P_PUBK[] = "p_pubk";
static const char P_SIGN[] = "p_sign";
static const char P_VALUE[] = "p_value";
static const char P_NO[] = "p_no";
static const char P_FEE[] = "p_fee";
static const char P_DATA[] = "p_data";

static const char T_CREATE[] = "addWallet";
static const char T_SEND[] = "send";

bool *thrdListenIsAff;				//*************************** SINGLE STRING ADDED HERE!!! ****************************

PROXY_SERVER::PROXY_SERVER(int _port, moodycamel::ConcurrentQueue<TX*>& _send_message_queue, uint64_t _pool_size, Counters& _counter, KeyManager& key_holder)
    : mh::mhd::MHD()
    , send_message_queue(_send_message_queue)
    , pool_size(_pool_size)
    , counters(_counter)
	, key_manager(key_holder)
{
    set_port(_port);
	
//*************************** MODIFIED HERE!!! ****************************

    set_host(XRENO_IP);
	
	thrdListenIsAff = new bool[counters.thrdListen];
	
	for (uint i = 0; i < counters.thrdListen; i++) {
		thrdListenIsAff[i] = false;
	}

	set_threads(counters.thrdListen);
		
//*************************** MODIFIED END ********************************	

}

PROXY_SERVER::~PROXY_SERVER() = default;	// ******************** DIFF FROM 1.3 !!! **************************************

//*************************** MODIFIED HERE!!! ****************************

bool PROXY_SERVER::run(const MHD_ConnectionInfo *caddr, int thread_number, mh::mhd::MHD::Request& mhd_req, mh::mhd::MHD::Response& mhd_resp) {
	if (counters.needSuicide) {
		return true;
	}

	uint cpuNum = counters.coreListenMap[thread_number];
			
	if (!thrdListenIsAff[thread_number]) {
		thrdListenIsAff[thread_number] = true;

		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(cpuNum, &cpuset);

		if (int ar = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) {
			counters.coutLocker.lock();
			std::cout << "Listen thread " << thread_number << " affinity to CPU " << cpuNum << " error: " << ar << std::endl;
			counters.coutLocker.unlock();
		} else {
//			counters.coutLocker.lock();
//			std::cout << "Listen thread " << thread_number << " (ID " << std::this_thread::get_id() << ") affinity to CPU " << cpuNum << " suss" << std::endl;
//			counters.coutLocker.unlock();		
		}
	}

	counters.coreStat[cpuNum]++;
	
//*************************** MODIFIED END ********************************		
	
    counters.qps++;

    mhd_resp.headers["Access-Control-Allow-Origin"] = "*";

    if (send_message_queue.size_approx() > pool_size) {
        mhd_resp.data = "Queue full<BR/>" + std::to_string(thread_number);
        return true;
    }

    if (mhd_req.params.find("act") != mhd_req.params.end()) {
        std::string& act = mhd_req.params["act"];

        if (act == T_SEND) {
            if (
                mhd_req.params.find(P_TO) != mhd_req.params.end() 
				&& mhd_req.params.find(P_PUBK) != mhd_req.params.end() 
				&& mhd_req.params.find(P_VALUE) != mhd_req.params.end() 
				&& mhd_req.params.find(P_SIGN) != mhd_req.params.end() 
				&& mhd_req.params.find(P_NO) != mhd_req.params.end() 
				&& mhd_req.params.find(P_FEE) != mhd_req.params.end() 
				&& mhd_req.params.find(P_DATA) != mhd_req.params.end()) 
			{
                TX* p_tx = new TX;

                if (p_tx->fill_from_strings(
                        mhd_req.params[P_TO],
                        mhd_req.params[P_VALUE],
                        mhd_req.params[P_FEE],
                        mhd_req.params[P_NO],
                        mhd_req.params[P_DATA],
                        mhd_req.params[P_SIGN],
                        mhd_req.params[P_PUBK])) {
                    std::string tx_hash_hex = bin2hex(p_tx->hash);
                    counters.qps_success++;
                    mhd_resp.data += "Transaction accapted.<BR/>" + tx_hash_hex;

                    send_message_queue.enqueue(p_tx);

                    return true;
                } else {
                    counters.qps_inv++;
                    mhd_resp.data += "Invalid transaction<BR/>";

                    delete p_tx;

                    return true;
                }
            } else {
                counters.qps_no_req++;
                mhd_resp.data += "Some fields not present<BR/>";
                return true;
            }
        } else if (act == T_CREATE) {
        }
    } else {
        rapidjson::Document req_json;
        if (!req_json.Parse(mhd_req.post.c_str()).HasParseError()) {
            std::string s_id = "";
            if (req_json.HasMember("id") && req_json["id"].IsString()) {
                s_id = ",\"id\":\"" + std::string(req_json["id"].GetString()) + "\"";
            }
            if (req_json.HasMember("id") && req_json["id"].IsInt()) {
                s_id = ",\"id\":" + std::to_string(req_json["id"].GetInt());
            }

            if (req_json.HasMember("method") && req_json["method"].IsString()) {

//	*************************** MODIFIED HERE!!! ****************************

                bool real_tx = (std::string(req_json["method"].GetString()) == "mhc_send");
                bool test_tx = (std::string(req_json["method"].GetString()) == "mhc_test_send");
				
				if (!test_tx) {
					counters.coutLocker.lock();
					std::cout << mhd_req.post << std::endl;	
					counters.coutLocker.unlock();
				}
		
//	*************************** MODIFIED END ********************************

                if (real_tx || test_tx) {
				
//	*************************** MODIFIED HERE!!! ****************************

					std::cout << thread_number << " * "<< mhd_req.post << std::endl;

					if (counters.heaterPid != 0 && counters.sleepNoHeat != 0) {
						counters.cntLocker.lock();
						
						if (!counters.getTx) {				
							counters.getTx = true;

							if (counters.needHeat) {
								counters.needHeat = false;
								kill(counters.heaterPid, SIGUSR1);
							}
						}	
					
						counters.cntLocker.unlock();
					}
					
//	*************************** MODIFIED END ********************************

                    if (req_json.HasMember("params") && req_json["params"].IsObject()) {
                        if (req_json["params"].HasMember("to") && req_json["params"]["to"].IsString()
                            && req_json["params"].HasMember("value") && req_json["params"]["value"].IsString()
                            && req_json["params"].HasMember("fee") && req_json["params"]["fee"].IsString()
                            && req_json["params"].HasMember("nonce") && req_json["params"]["nonce"].IsString()
                            && req_json["params"].HasMember("data") && req_json["params"]["data"].IsString()
                            && req_json["params"].HasMember("pubkey") && req_json["params"]["pubkey"].IsString()
                            && req_json["params"].HasMember("sign") && req_json["params"]["sign"].IsString()) {

                            std::string rto_addr(req_json["params"]["to"].GetString());
                            std::string rdata(req_json["params"]["data"].GetString());
                            std::string rsign(req_json["params"]["sign"].GetString());
                            std::string rpub_key(req_json["params"]["pubkey"].GetString());

                            TX* p_tx = new TX;

                            if (p_tx->fill_from_strings(
                                    rto_addr,
                                    std::string(req_json["params"]["value"].GetString()),
                                    std::string(req_json["params"]["fee"].GetString()),
                                    std::string(req_json["params"]["nonce"].GetString()),
                                    rdata, rsign, rpub_key)) {

//	*************************** MODIFIED HERE!!! ****************************
							
								counters.qps_success++;
                                mhd_resp.data += "{\"result\":\"ok\",\"params\":\"" + bin2hex(p_tx->hash) + "\"" + s_id + "}";
								
//	*************************** MODIFIED END ********************************

                                if (real_tx) {
                                    send_message_queue.enqueue(p_tx);
                                } else {
                                    delete p_tx;
                                }
                            } else {
                                counters.qps_inv++;
                                mhd_resp.data += "{\"result\":\"ok\",\"error\":\"Invalid transaction\"" + s_id + "}";

                                delete p_tx;
                            }
                        } else {
                            counters.qps_no_req++;
                            mhd_resp.data += "{\"result\":\"ok\",\"error\":\"no required params or bad type\"" + s_id + "}";
                        }
                    } else {
                        counters.qps_inv++;
                        mhd_resp.data += "{\"result\":\"ok\",\"error\":\"unsupported params type\"" + s_id + "}";
                    }
                } else if (std::string(req_json["method"].GetString()) == "getinfo") {
                    counters.qps_trash++;
                    mhd_resp.data += std::string("{\"result\":{")
                        + "\"version\":\"" + std::to_string(VESION_MAJOR) + "." + std::to_string(VESION_MINOR) + "\", "
                        + "\"mh_addr\":\"" + key_manager.Text_addres + "\"},\"error\":null" + s_id + "}";
						
//	*************************** MODIFIED HERE!!! ****************************
					
//					std::cout << "Curr thread ID: \t" << std::this_thread::get_id() << std::endl;

					struct sockaddr* addr = (struct sockaddr*) caddr->client_addr;

					sockaddr_in *sin = reinterpret_cast<sockaddr_in*>(addr);

					char *ip = NULL;
					ip = (char *) malloc(INET_ADDRSTRLEN);
					
					const char* p = inet_ntop(AF_INET, &(sin->sin_addr), ip, 100);

					counters.coutLocker.lock();
					
					if (p != nullptr) {
						std::cout << "getinfo IP: \t\t\t\t\t" << std::string(ip) << std::endl;
					} else {
						std::cout << "getinfo IP: \t\t\t\t\tundefined" << std::endl;
					}
					
					counters.coutLocker.unlock();
					
					delete ip;
					
//	*************************** MODIFIED END ********************************					
	
                } else {
                    counters.qps_inv++;
                    mhd_resp.data += "{\"result\":\"ok\",\"error\":\"unsupported method\"" + s_id + "}";
                }
            } else {
                counters.qps_trash++;
                mhd_resp.data += "{\"result\":\"ok\",\"error\":\"unsupported method\"" + s_id + "}";
            }
        } else {
            counters.qps_trash++;
            mhd_resp.data += "{\"result\":\"ok\",\"error\":\"json parse error\"}";
        }
    }
    return true;
}

bool PROXY_SERVER::init()
{
    return true;
}

bool KeyManager::parse(const std::string& line)
{

    std::vector<unsigned char> priv_k = hex2bin(line);
    PrivKey.insert(PrivKey.end(), priv_k.begin(), priv_k.end());
    if (!generate_public_key(PubKey, PrivKey)) {
        return false;
    }

    Text_PubKey = "0x" + bin2hex(PubKey);

    std::array<char, 25> addres = get_address(PubKey);
    Bin_addr.insert(Bin_addr.end(), addres.begin(), addres.end());

    Text_addres = "0x" + bin2hex(Bin_addr);

    return true;
}

std::string KeyManager::make_req_url(std::string& data)
{
    std::vector<char> sign;
    sign_data(data, sign, PrivKey);

    return "/?pubk=" + Text_PubKey + "&sign=" + bin2hex(sign);
}

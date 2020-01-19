#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <mh/libevent/LibEvent.h>

#include "concurrentqueue.h"
#include "proxyserver.h"
#include "transaction.h"

//	*************************** MODIFIED HERE!!! ****************************

#include "open_ssl_decor.h"

uint sendCnt = 0;
uint heatCnt = 0;
uint servCnt = 0;

std::string getMyIp() {
	return XRENO_IP;
}

bool checkCores(ulong num, uint htServSend) {
	if (

#if defined(HAVE_28_CORES)

		num == 1 ||
		num == 2 ||
		
	#if defined(HAVE_HT)		
		#if defined(USE_HT)	
		
		num == 29 ||
		num == 30 ||
		
		#endif
	#endif

#elif defined(HAVE_48_CORES)

		num == 8 ||
		num == 16 ||
		num == 20 ||
		num == 24 ||
		num == 28 ||
		
	#if defined(HAVE_HT)		
		#if defined(USE_HT)	
		
		num == 56 ||
		num == 64 ||
		num == 68 ||
		num == 72 ||
		num == 76 ||
		
		#endif
	#endif
	
#elif defined(HAVE_32_CORES)

		num == 2 ||
		num == 4 ||
		num == 6 ||
		num == 8 ||
		
	#if defined(HAVE_HT)		
		#if defined(USE_HT)	
		
//		num == 56 ||
//		num == 64 ||
//		num == 68 ||
//		num == 72 ||
		
		#endif
	#endif	
#endif
		num == cpuServSend ||
		num == htServSend
	) {
		return false;
	}
	
	return true;
}

//	*************************** MODIFIED END ********************************

std::string getHostName()
{
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    return std::string(hostname);
}

bool check_addr(const std::string& addr) {
    if (addr[0] == '0' && addr[1] == 'x') {
        for (uint i = 2; i < addr.length(); i++) {
            if (!isxdigit(addr[i])) {
                std::cout << "Not hex digit: " << addr[i] << "  " << i << std::endl;
                return false;
            }
        }
    } else {
        return false;
    }
	
    return true;
}

//	*************************** MODIFIED HERE!!! ****************************

void libevent(moodycamel::ConcurrentQueue<TX*>& send_message, std::string host, int port, KeyManager& key_holder, Counters& counters)
{
		uint thrdNum;
		uint cpuNum = 0;
		
		counters.cntLocker.lock();
		thrdNum = counters.thrdSend;
		counters.thrdSend++;
		counters.cntLocker.unlock();
	
		if (cpuMapMode == 1 || cpuMapMode == 2 || cpuMapMode == 3) {
			cpuNum = cpuServSend;
		}
		
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(cpuNum, &cpuset);

		if (int ar = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) {
			counters.coutLocker.lock();
			std::cout << "Send thread " << thrdNum << " affinity to CPU " << cpuNum << " error: " << ar << std::endl;
			counters.coutLocker.unlock();
		}
		
//	*************************** MODIFIED END ********************************

    mh::libevent::LibEvent levent;
    std::string req_post;
	
    while (true) {
        TX* p_req_post;
        if (send_message.try_dequeue(p_req_post)) {
            req_post.insert(req_post.end(), p_req_post->raw_tx.begin(), p_req_post->raw_tx.end());
            std::string path = key_holder.make_req_url(req_post);

            std::string response;
			
//	*************************** MODIFIED HERE!!! ****************************					

			bool is_send = false;

            for (uint i = 0; i < 10; i++) {
                int status = levent.post_keep_alive(host, port, host, path, req_post, response, 500);

                if (status > 0) {
					is_send = true;
                    break;
				}
            }

            if (!is_send) {
                send_message.enqueue(p_req_post);
            } else {
                delete p_req_post;
            }

            req_post.clear();
        } else {
			if (counters.needSuicide) {
				counters.coutLocker.lock();
				std::cout << "Sending thread " << thrdNum << " finished" << std::endl;
				counters.coutLocker.unlock();
				
				counters.cntLocker.lock();
				counters.thrdSend--;				
				counters.cntLocker.unlock();
				
				break;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(sleepSend));
			
//	*************************** MODIFIED END ********************************			
        }
    }
}

void SIGPIPE_handler(int s)
{
    printf("Caught SIGPIPE(%d)\n", s);
}

int main(int argc, char** argv)
{
    moodycamel::ConcurrentQueue<TX*> send_message_queue;

    std::cout << "Version:\t" << VESION_MAJOR << "." << VESION_MINOR << "." << std::endl;
//	std::cout << "Main thread ID: \t" << std::this_thread::get_id() << std::endl;

    int listen_port = 0;
    int pool_size = 0;
    std::string network;
    KeyManager key_holder;
    std::vector<std::thread> sender;
	
	ulong i;
	ulong j;
	
//	*************************** MODIFIED HERE!!! ****************************
	
	Counters counters;
		
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	uint cpuNum = 0;
	uint cpuServSendHT = 0;
	
	if (0 == sched_getaffinity(getpid(), sizeof(cpu_set_t), &cpuset)) {
		const uint nCores = sysconf( _SC_NPROCESSORS_ONLN );
		uint minCore = 1000000;
		uint maxCore = 0;
		
		for (i = 0; i < nCores; i++) {
			if (CPU_ISSET(i, &cpuset)) {
				if (i < minCore) minCore = i;
				if (i > maxCore) maxCore = i;

				counters.coreStat[i] = 0;
				counters.coreListenMap[i] = 0;
			}
		}
		
		if (cpuMapMode == 1 || cpuMapMode == 2 || cpuMapMode == 3) {
			if (cpuMapMode == 1) {
				counters.cpuCnt = nCores / 2;
				counters.thrdListen = counters.cpuCnt - (1 + cpuIrqCnt);
				cpuServSendHT = nCores / 2;
			} else if (cpuMapMode == 2) {
				counters.cpuCnt = nCores;
				counters.thrdListen = counters.cpuCnt - (1 + cpuIrqCnt) * 2;
				cpuServSendHT = nCores / 2;
			} else if (cpuMapMode == 3) {
				counters.cpuCnt = nCores;
				counters.thrdListen = counters.cpuCnt - (1 + cpuIrqCnt);
				cpuServSendHT = cpuServSend;
			}
			
			j = 0;
			for (i = 0; i < counters.thrdListen; i++) {
				while (!checkCores(j, cpuServSendHT)) {
					j++;
				}
				
				counters.coreListenMap[i] = j;
				j++;
			}
		}
		
		std::cout << "CPU cores found: " << nCores << " (from " << minCore << " to " << maxCore << ")" << std::endl;
		std::cout << "CPU cores will be used: " << counters.cpuCnt << " (for listening " << counters.thrdListen << ")" << std::endl;
		
// affinity of main thread		
		if (cpuMapMode == 1 || cpuMapMode == 2 || cpuMapMode == 3) {
			cpuNum = cpuServSend;
		}
		
		CPU_ZERO(&cpuset);
		CPU_SET(cpuNum, &cpuset);

		if (int ar = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) {
			counters.coutLocker.lock();
			std::cout << "Main thread affinity to CPU " << cpuNum << " error: " << ar << std::endl;
			counters.coutLocker.unlock();
		}
		
	} else {
		std::cerr << "sched_getaffinity() failed: " << strerror(errno) << std::endl;
	}	
	
//	*************************** MODIFIED END ********************************	

    if (argc > 6) {
		counters.heaterPid = std::atoi(argv[5]);
		counters.sleepNoHeat = std::atoi(argv[6]);
		
        {
            std::ifstream file(argv[1]);
            std::string line;

            if (std::getline(file, line)) {
                if (!key_holder.parse(line)) {
                    std::cerr << "Error while parsing Private key" << std::endl;
                    std::cerr << "Probably ivalid Private Key File" << std::endl;
                    exit(1);
                }
				
				std::cout << "got key for address:\t" << key_holder.Text_addres << std::endl;
            } else {
                std::cerr << "Ivalid Private Key File" << std::endl;
                exit(1);
            }
        }

        {
            std::ifstream file(argv[2]);
            std::string line;
            bool first_line = true;

            while (std::getline(file, line)) {
                std::stringstream linestream(line);
                std::string host;
                int port = 0;
                ulong conn = 0;

                linestream >> host >> port >> conn;
                std::cout << "Conn\t" << host << "\t" << port << "\t" << conn << "\n";

                if (first_line) {
                    if (port == 0 && conn == 0) {
                        std::stringstream netwok_line(line);
                        netwok_line >> network;
                        std::cout << "network\t" << network << std::endl;
                        first_line = false;
                        continue;
                    } else {
                        std::cerr << "seems to be old config" << std::endl;
                        std::cerr << "first line must be network" << std::endl;
                        std::cerr << "check documentation" << std::endl;
                        exit(1);
                    }
                }

                for (i = 0; i < conn; i++) {
				
//	*************************** MODIFIED HERE!!! ****************************

					sendCnt++;
                    sender.push_back(std::thread(libevent, std::ref(send_message_queue), host, port, std::ref(key_holder), std::ref(counters)));
					
//	*************************** MODIFIED END ********************************	
					
                }
            }
        }

        {
            listen_port = std::atoi(argv[3]);
            pool_size = std::atoi(argv[4]);
        }
    } else {
        std::cerr << "Ivalid command line parameters" << std::endl;
        std::cerr << "Usage:" << std::endl;
        std::cerr << "app [private key file] [config file] [listen port] [pool limit]" << std::endl;

        exit(1);
    }

    if (sender.size() == 0 || network.empty()) {
        std::cerr << "Ivalid configuration file" << std::endl;
        exit(1);
    }

//	*************************** MODIFIED HERE!!! ****************************

		servCnt++;
	
		std::thread heat([&counters]() {
			uint cpuNum = 0;
			uint thrdNum = 0;
		
			counters.cntLocker.lock();
			thrdNum = counters.thrdServ;
			counters.thrdServ++;
			counters.cntLocker.unlock();

			if (cpuMapMode == 1 || cpuMapMode == 2 || cpuMapMode == 3) {
				cpuNum = cpuServSend;
			}

			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(cpuNum, &cpuset);

			if (int ar = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) {
				counters.coutLocker.lock();
				std::cout << "Service thread " << thrdNum << " affinity to CPU " << cpuNum << " error: " << ar << std::endl;
				counters.coutLocker.unlock();
			}
		
			while (true) {
				if (counters.heaterPid != 0 && counters.sleepNoHeat != 0) {
					counters.cntLocker.lock();
			
					if (!counters.getTx) {
						if (counters.needHeat == false) {
							counters.needHeat = true;
							kill(counters.heaterPid, SIGUSR2);
						}
					} else {
						counters.getTx = false;
					}

					counters.cntLocker.unlock();
			
					std::this_thread::sleep_for(std::chrono::milliseconds(counters.sleepNoHeat));
				} else {
					std::this_thread::sleep_for(std::chrono::seconds(3600));
				}
			}
		});
	
	servCnt++;
	
    std::thread stat([&counters, &send_message_queue, &key_holder, network]() {
        mh::libevent::LibEvent levent;
        char printbuf[100000];

		uint qpsMax = 0;
		uint qpsCoreStatLimit = 1000;
		uint qpsSuicideLimit = 0;
		
		uint i;
		uint j;

		uint cpuNum = 0;
		uint thrdNum = 0;
		
		counters.cntLocker.lock();
		thrdNum = counters.thrdServ;
		counters.thrdServ++;
		counters.cntLocker.unlock();
		
		if (cpuMapMode == 1 || cpuMapMode == 2  || cpuMapMode == 3) {
			cpuNum = cpuServSend;
		}

		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(cpuNum, &cpuset);

		if (int ar = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) {
			counters.coutLocker.lock();
			std::cout << "Service thread " << thrdNum << " affinity to CPU " << cpuNum << " error: " << ar << std::endl;
			counters.coutLocker.unlock();
		}		
		
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

			if (counters.qps.load() == 0) {
				if (qpsSuicideLimit && qpsMax >= qpsSuicideLimit && !counters.needSuicide) {
					counters.coutLocker.lock();
					std::cout << "Suicide init" << std::endl;
					counters.needSuicide = true;
					counters.coutLocker.unlock();					
				}
				
				if (counters.needSuicide) {
					if (counters.thrdSend == 0 && counters.thrdHeat == 0) {
						counters.coutLocker.lock();
						std::cout << "Suicide done" << std::endl;
						counters.coutLocker.unlock();
						exit(1);
					}
				}
				
				if (qpsMax > qpsCoreStatLimit) {
					counters.coutLocker.lock();
					
					for(i = 0; i < counters.cpuCnt; ) {
						for(j = i; j < (i + cpuShowCols) && j < counters.cpuCnt; j++) {
							std::cout << "c";
							
							if (j < 10) {
								std::cout << "0";
							}
							
							std::cout << j << "\t";
						}
						
						std::cout << std::endl;
						
						for(j = i; j < (i + cpuShowCols) && j < counters.cpuCnt; j++) {
							std::cout << counters.coreStat[j] << "\t";
							counters.coreStat[j] = 0;
						}
						
						std::cout << std::endl;
						std::cout << std::endl;

						i = j;
					}
				
					counters.coutLocker.unlock();
					
					qpsMax = 0;
				}
			
				continue;
			}
			
			if (counters.qps.load() > qpsMax) {
				qpsMax = counters.qps.load();
			}
			
	
//	*************************** MODIFIED END ********************************
			
            std::string ip = getMyIp();
            std::string host = getHostName();

            uint64_t timestamp = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();
            memset(printbuf, 0, 100000);
            snprintf(
                printbuf,
                100000,
                "{\"params\": \n"
                "   {\"network\":\"%s\", \"group\": \"proxy\", \"server\": \"%s\", \"timestamp_ms\": %ld,\n"
                "   \"metrics\": [\n"
                "        {\"metric\": \"qps\", \"type\": \"sum\", \"value\": %ld},\n"
                "        {\"metric\": \"qps_trash\", \"type\": \"sum\", \"value\": %ld},\n"
                "        {\"metric\": \"qps_no_req\", \"type\": \"sum\", \"value\": %ld},\n"
                "        {\"metric\": \"qps_inv\", \"type\": \"sum\", \"value\": %ld},\n"
                "        {\"metric\": \"qps_inv_sign\", \"type\": \"sum\", \"value\": %ld},\n"
                "        {\"metric\": \"qps_success\", \"type\": \"sum\", \"value\": %ld},\n"
                "        {\"metric\": \"queue\", \"type\": \"sum\", \"value\": %ld},\n"
                "        {\"metric\": \"ip\", \"type\": \"none\", \"value\": \"%s\"},\n"
                "        {\"metric\": \"mh_addr\", \"type\": \"none\", \"value\": \"%s\"},\n"
                "        {\"metric\": \"version\", \"type\": \"none\", \"value\": \"%d.%d\"}\n"
                "    ]},\n"
                "\"id\": 1}",
                network.c_str(),
                host.c_str(), timestamp,
                counters.qps.load(),
                counters.qps_trash.load(),
                counters.qps_no_req.load(),
                counters.qps_inv.load(),
                counters.qps_inv_sign.load(),
                counters.qps_success.load(),
                send_message_queue.size_approx(),
                ip.c_str(),
                key_holder.Text_addres.c_str(),
                VESION_MAJOR,
                VESION_MINOR);

            std::string req_post(printbuf);
            std::string response;
            levent.post_keep_alive("172.104.236.166", 5797, "172.104.236.166", "/save-metrics", req_post, response);
			
//	*************************** MODIFIED HERE!!! ****************************

			std::cout << timestamp << " qps :\t" << counters.qps_success.load() << " / " << counters.qps.load() << std::endl;
			
//	*************************** MODIFIED END ********************************	

            counters.qps.store(0);
            counters.qps_trash.store(0);
            counters.qps_no_req.store(0);
            counters.qps_inv.store(0);
            counters.qps_inv_sign.store(0);
            counters.qps_success.store(0);
        }
    });

	while(sendCnt != counters.thrdSend || servCnt != counters.thrdServ) {
		usleep(100000);
	}
	
//	std::cout << "used CPUs:\t\t" << counters.cpuCnt << std::endl;
	std::cout << std::endl;
	std::cout << "service threads:\t" << counters.thrdServ << std::endl;
	std::cout << "sending threads:\t" << counters.thrdSend << std::endl;
	std::cout << "listen threads:\t\t" << counters.thrdListen << std::endl;

//	*************************** MODIFIED END ********************************	

    PROXY_SERVER PS(listen_port, send_message_queue, pool_size, counters, key_holder);    
	PS.start();

    return 0;
}

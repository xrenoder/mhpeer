#!/bin/bash

nodenum=

name=node$nodenum
heat=heater
workdir=~/peers/$name

sleepNoHeat=$1
port=9999
num=100000
log=log
key=main_key
conf=main_conf

function startProxy() {
    status
    if [ $res -eq 0 ]
    then
        echo Proxy already running, pid $pid
    else
        ulimit -c unlimited
        if [ -d $workdir ]
        then
           cd $workdir

           if [ ! -f $key ]
           then
               echo "No key file $key found, exiting."
               exit 2
           elif [ ! -f $conf ]
           then
               echo "No config file $conf found, exiting."
                          exit 2
           fi
		   
		   if [ -f $log ]
		   then
				i=1
				newlog=$log$i
				
				while [ -f $newlog ]
				do
					i=$(($i+1));
					newlog=$log$i
				done

				echo Copy log to $newlog			
				cp $log $newlog
		   fi
		   
			pid=`pidof $heat`
			rs=$?

			if [ $rs -ne 0 ]
			then
				echo Heater not running
				pid=0
				sleepNoHeat=0
			fi
		   
           echo Starting $name "$workdir/$name $key $conf $port $num $pid $sleepNoHeat"
           $workdir/$name $key $conf $port $num $pid $sleepNoHeat > $log &
        else
           echo "workdir $workdir doesn't exists"
           exit 2;
        fi
    fi
}


function stopProxy() {


    status
    if [ $res -ne 0 ]
       then
               echo Proxy not running
    else

        echo Stopping proxy, pid $pid
        kill $pid
        sleep 2
        status

        if [ $res -eq 0 ]
        then
            echo Stop failed. Proxy still alive, pid $pid. Please check manually
        fi

    fi
}

function status() {

    pid=`pidof $name`
    res=$?
}


case "$2" in
start)
    startProxy
    ;;

stop)
    stopProxy
    ;;

restart)
    stopProxy
    startProxy
    ;;

status)
     status

    if [ $res -ne 0 ]
       then
               echo -e "Proxy \e[31mnot running\e[0m"
       else
               echo -e "Proxy is \e[32mrunning\e[0m. Pid $pid"
       fi
     ;;

 *)
     echo "Usage: $0 [sleepNoHeat in ms] start|stop|status|restart"
     exit 1
    ;;
esac

# APP: COPSS-lite Version 2	2017-02-02
# Paper: Jiachen Chen, Mayutan Arumaithurai, Lei Jiao, Xiaoming Fu, K.K. Ramakrishnan (2011). COPSS: An Efficient Content Oriented Publish/Subscribe System. IEEE
# Based on CCN-Lite on RIOT library

This application named COPSS-lite demonstrates how to use COPSS (http://www.ccn-lite.net/) on RIOT. 

## The shell commands

RIOT provides three shell to interact with the COPSS-Lite:

COPSS-lite commands usage:

* link <ipv6-addr> <port>
% link fe80::207e:b9ff:fe04:9ff0 9696
 	link to a node on address:port.

* copss_fib <URI> <ipv6-addr> <port>
% copss_fib /peter/schmerzl fe80::207e:b9ff:fe04:9ff0 9696
	add an FIB entry name->address:port

* copss_RP <RP_name(without "/" inside) >
% copss_RP RP
	starts an RP module using RPName

* copss_client
% copss_client
 	start copss client on default port 9696

After start the copss client, these commands are available:

* copss sub/unsub <URI,URI,......>
% copss sub /sports/football
% copss sub /sports/football,/news/bbc
% copss unsub /sports/football
	subscribe or unsubscribe to a set of CDs

* copss_pub <URI,URI,......> <content>
% copss_pub /sports/football Hello
% copss_pub /sports/football,/news/bbc Hello COPSS
	publish a message


simulation and evaluation commands:

PUB/SUB

* simpub
	this command will open the auto-publish simulator. you could prepare the dataset in the file named pub.data.the format is strict as "prefix+space+data" for each line,e.g."/copss/a1/temp 25"

* simsuball
	this command will open the auto-sub simulator. you could prepare the prefixes in the file named sub.data.the format is strict as one single "prefix" for each line,e.g."/copss/a1/temp"

Query and Response

* sim_set_ccnl_cache
	with this command you could set the max cache of CS in ccn-lite relay

* sim_ccnl_int
	this command will open the auto-generate interst simulator. you could prepare the dataset in the file named ccnl_int.data.the format is strict as "prefix" for each line,e.g."/copss/a1/temp"

* sim_ccnl_cont
	this command will open the auto-publish simulator. you could prepare the dataset in the file named ccnl_cont.data.the format is strict as "prefix+space+data" for each line,e.g."/copss/a1/temp 25"



COPSS-lite also support the default CCN-Lite and RIOT commands
* `ccnl_int`  - generates and sends out an Interest. The command expects one
                mandatory and one optional parameter. The first parameter
                specifies the exact name (or a prefix) to request, the second
                parameter specifies the link-layer address of the relay to use.
                If the second parameter is omitted, the Interest will be
                broadcasted. You may call it like this:
                `ccnl_int /riot/peter/schmerzl b6:e5:94:26:ab:da`
* `ccnl_cont` - generates and populates content. The command expects one
                mandatory and one optional parameter. The first parameter
                specifies the name of the content to be created, the second
                parameter specifies the content itself. The second parameter
                may include spaces, e.g. you can call:
                `ccnl_cont /riot/peter/schmerzl Hello World! Hello RIOT!`
* `ccnl_fib`  - modifies the FIB or shows its current state. If the command is
                called without parameters, it will print the current state of
                the FIB. It can also be called with the action parameters `add`
                or `del` to add or delete an entry from the FIB, e.g.
                `ccnl_fib add /riot/peter/schmerzl ab:cd:ef:01:23:45:67:89`
                will add an entry for `/riot/peter/schmerzl` with
                `ab:cd:ef:01:23:45:67:89` as a next hop and
                `ccnl_fib del /riot/peter/schmerzl`
                will remove the entry for `/riot/peter/schmerzl` and
                `ccnl_fib del ab:cd:ef:01:23:45:67:89`
                will remove all entries with `ab:cd:ef:01:23:45:67:89` as a
                next hop.

Examples:
Steps:
1. create 10 (or any number you want) virtual taps in linux
./../dist/tools/tapsetup/tapsetup --create 10

2. compile COPSS-lite
make all term
make term


3. Query and Response in ccnlie with COPSS wapper
1) start COPSS in two instances (two IOT devices ), COPSS sever is running and listening to the port:

2) use 'Link <IPv6 address > <port>' command link the two nodes each other. 

3) in device 1: use 'copss_fib <prefix> <IPv6 address > <port>'to add a fib entry to ccnlite fib table. 
E.g. copss_fib /test Device2.Ipv6 port. 

4) in device 2 : use ccnlite-riot command 'ccnl_cont <prefix> <content>' to generate a content in Content store. 
E.g. Ccnl_cont /test hellocopss

5) in device 1 . use ccnlie-riot commands 'ccnl_int  <prefix>' to send out a interest. 
E.g. Ccnl_int /test

6) then the device 1 will get the corresponding content from device 2 with the help of COPSS. 


4. Subscribe and Publish

start COPSS client 
copss_client

some examples commands for sub and pub
note: prefix should start with /copss/

copss sub /copss/sports
copss sub /copss/news
copss sub /copss/ads

copss unsub /copss/news
copss unsub /copss/sports
copss unsub /copss/ads

copss_pub /copss/sports NBA
copss_pub /copss/news news bbc NBA
copss_pub /copss/sports,/copss/news news bbc NBA
copss_pub /copss/ads ads NBA



4. some scenarios
use "ifconfig" command to get IPv6 address for each node.

two instances commands example:

tap0    	fe80::90a4:6ff:fe80:874d

 		link fe80::a485:93ff:fe19:1b60 9696
		copss_fib /RP fe80::a485:93ff:fe19:1b60 9696
		copss_client

		copss sub /copss/sports


tap1(RP)  	fe80::a485:93ff:fe19:1b606	

		link fe80::90a4:6ff:fe80:874d 9696
		copss_RP RP
		copss_client

		copss_pub /copss/sports NBA


three instances example

tap0    fe80::60d3:49ff:fe9f:5ec9

 	link fe80::5c86:a9ff:fe42:8f6 9696
	copss_fib /RP fe80::5c86:a9ff:fe42:8f6 9696
	copss_client

	copss sub /copss/sports


tap1(RP)  fe80::5c86:a9ff:fe42:8f6	

	link fe80::60d3:49ff:fe9f:5ec9 9696
	link fe80::a466:efff:fed5:93c3 9696
	copss_RP RP
	copss_client


tap2	fe80::a466:efff:fed5:93c3

	link fe80::5c86:a9ff:fe42:8f6 9696
	copss_fib /RP fe80::5c86:a9ff:fe42:8f6 9696
	copss_client

	copss_pub /copss/sports this is copss




four instances example

tap0    fe80::a01c:89ff:fe51:d7fc

 	link fe80::b4d2:2fff:fec1:bc0c 9696
	copss_fib /RP fe80::b4d2:2fff:fec1:bc0c 9696
	copss_client

	copss sub /copss/sports


tap1(RP)  fe80::b4d2:2fff:fec1:bc0c	

	link fe80::a01c:89ff:fe51:d7fc 9696
	link fe80::a8cd:a8ff:fea4:c622 9696
	link fe80::dcf1:cbff:fec9:2bd2 9696
	copss_RP RP
	copss_client


tap2	fe80::a8cd:a8ff:fea4:c622

	link fe80::b4d2:2fff:fec1:bc0c 9696
	copss_fib /RP fe80::b4d2:2fff:fec1:bc0c 9696
	copss_client

	copss_pub /copss/sports this is copss


tap3	fe80::dcf1:cbff:fec9:2bd2

        link fe80::b4d2:2fff:fec1:bc0c 9696
	copss_fib /RP fe80::b4d2:2fff:fec1:bc0c 9696
	copss_client


COPSS-lite with RPL(please see RPL description in RIOT)

ifconfig 5 add 2001:db8::1

rpl init 5

rpl root 1 2001:db8::1


four instances example

tap0    2001:db8::a01c:89ff:fe51:d7fc

 	link 2001:db8::b4d2:2fff:fec1:bc0c 9696
	copss_fib /RP 2001:db8::b4d2:2fff:fec1:bc0c 9696
	copss_client

	copss sub /copss/sports


tap1(RP)  2001:db8::b4d2:2fff:fec1:bc0c	

	link 2001:db8::a01c:89ff:fe51:d7fc 9696
	link 2001:db8::a8cd:a8ff:fea4:c622 9696
	link 2001:db8::dcf1:cbff:fec9:2bd2 9696
	copss_RP RP
	copss_client


tap2	2001:db8::a8cd:a8ff:fea4:c622

	link 2001:db8::b4d2:2fff:fec1:bc0c 9696
	copss_fib /RP 2001:db8::b4d2:2fff:fec1:bc0c 9696
	copss_client

	copss_pub /copss/sports this is copss


tap3	2001:db8::dcf1:cbff:fec9:2bd2

        link 2001:db8::b4d2:2fff:fec1:bc0c 9696
	copss_fib /RP 2001:db8::b4d2:2fff:fec1:bc0c 9696
	copss_client

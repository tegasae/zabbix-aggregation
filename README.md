    Aggregation for zabbix server 4.*

    Description
	The module: aggregation.so
	Item: aggregation.triggers_lld
	The module counts the number of triggers from discovery rule in the specified state.
	This module is only for zabbix server.


    Build from source 
	cd <source_zabbix>
	./configure
	cd <source_zabbix>/src/modules/
	git clone https://github.com/tegasae/zabbix-aggregation.git
	cd zabbix_aggregation

	a) Linux
	    make
	b) FreeBSD
	gmake 
	or 
	make -f Makefile.bsd


    Сompiled
	linux/aggregation.so
	freebsd/aggregation.so


    Install
	Copy file aggregation.so to the directory specified in the option LoadModulePath.

	Add to zabbix_server.conf
	LoadModule=aggregation.so
	
	Restart zabbix server

    
    Сompiled
	linux/aggregation.so
	freebsd/aggregation.so


    Item aggregation.triggers_lld 
	aggregation.triggers_lld[host,discovery rule,trigger state, trigger prototype name1, trigger prototype name2, etc]
	host - hostname. Can use {HOST.HOST}
	trigger state:
	    1 - trigger is up
	    0 - trgger is down
	
	E.g.:
	aggregation.triggers_lld[{HOST.HOST},"vfs.fs.discovery",0,"{#FSNAME}: Disk space is critically low (used > {\$VFS.FS.PUSED.MAX.CRIT:\"{#FSNAME}\"}%)", "{#FSNAME}: Disk space is low (used > {\$VFS.FS.PUSED.MAX.WARN:\"{#FSNAME}\"}%)"]
	or
	aggregation.triggers_lld["my host","vfs.fs.discovery",1,"{#FSNAME}: Disk space is critically low (used > {\$VFS.FS.PUSED.MAX.CRIT:\"{#FSNAME}\"}%)"]
	For escaping " and $ use backslash .

	Item aggregation.triggers_lld returns the number of all triggers in the specified state.


    When using the zabbix proxy
	If a host is monitored by proxy, then aggregation.triggers_lld will return nothing. Since triggers are  in a proxy.
	In this case neccessary to add host and add item. In the element specify the host name that is necessary.
	The host mustn't monitored through a proxy.
	
	fake_my_host:
	    aggregation.triggers_lld["host that is monitored through a proxy","vfs.fs.discovery",1,"{#FSNAME}: Disk space is critically low (used > {\$VFS.FS.PUSED.MAX.CRIT:\"{#FSNAME}\"}%)"]
	
    Testing
	Zabbix 4.0 и zabbix 4.4
	Linux SMP Debian 4.19.67-2 (2019-08-28) x86_64 GNU/Linux
	FreeBSD 11.2-RELEASE-p4



/*
** Zabbix
** Copyright (C) 2001-2019 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/
#include "sysinc.h"

#include <stdlib.h>
#include <stdint.h>

#include "module.h"




#include "db.h"
#include "log.h"



#define QUERY_HOSTS "select items.itemid from items left join hosts on items.hostid=hosts.hostid where hosts.name='%s' and key_='%s'"
#define QUERY_TRIGGERS "select count(value) from triggers where value=%d and status=0 and triggerid in (select triggerid from trigger_discovery where parent_triggerid in (select triggerid from triggers where triggerid in (select triggerid from functions where itemid in (select itemid from item_discovery where parent_itemid=%d)) and description in (%s)))"

#define STR_DELIMITER "'%s', "

/* the variable keeps timeout setting for item processing */
static int	item_timeout = 0;


static char *query_hosts=NULL;
static char *query_triggers=NULL;



static int	aggregation_triggers_lld(AGENT_REQUEST *request, AGENT_RESULT *result);

static ZBX_METRIC keys[] =
/*	KEY			FLAG		FUNCTION	TEST PARAMETERS */
{
	{"aggregation.triggers_lld",	CF_HAVEPARAMS,	aggregation_triggers_lld, NULL},
	{NULL}
};



static char *get_str(const char *str)
{
	char 	*str_dst=NULL;
	size_t str_alloc=0;
	size_t str_offset=0;

	zbx_strncpy_alloc(&str_dst, &str_alloc, &str_offset, str, zbx_strlen_utf8(str));
	return str_dst;
}


static char *unescape_str(const char *str, char escape, const char *symbols_list)
{
	size_t len=zbx_strlen_utf8(str);
	size_t i, j=0, k;


	char *unescaped=NULL;

	unescaped=zbx_calloc(NULL,1,len+1);
	if (len==1)
	{
		unescaped[0]=str[0];
		return unescaped;
	}

	j=0;
	for (i=0;i<=len-2;i++)
	{
		if (str[i]==escape)
		{
			k=0;
			for (k=0;k<zbx_strlen_utf8(symbols_list);k++)
			{
				if (str[i+1]==symbols_list[k])
				{
					i++;
					break;
				}
			}
		}
		unescaped[j]=str[i];
		j++;
	}
	unescaped[j]=str[i];
	return unescaped;
}


/******************************************************************************
 *                                                                            *
 * Function: zbx_module_api_version                                           *
 *                                                                            *
 * Purpose: returns version number of the module interface                    *
 *                                                                            *
 * Return value: ZBX_MODULE_API_VERSION - version of module.h module is       *
 *               compiled with, in order to load module successfully Zabbix   *
 *               MUST be compiled with the same version of this header file   *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_api_version(void)
{
	return ZBX_MODULE_API_VERSION;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_timeout                                          *
 *                                                                            *
 * Purpose: set timeout value for processing of items                         *
 *                                                                            *
 * Parameters: timeout - timeout in seconds, 0 - no timeout set               *
 *                                                                            *
 ******************************************************************************/
void	zbx_module_item_timeout(int timeout)
{
	item_timeout = timeout;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_list                                             *
 *                                                                            *
 * Purpose: returns list of item keys supported by the module                 *
 *                                                                            *
 * Return value: list of item keys                                            *
 *                                                                            *
 ******************************************************************************/
ZBX_METRIC	*zbx_module_item_list(void)
{
	return keys;
}




static int	aggregation_triggers_lld(AGENT_REQUEST *request, AGENT_RESULT *result)
{

	char 		*hostname=NULL;
	char		*key;

	long unsigned int itemid=0;
	long unsigned int trigger_value=0;

	DB_RESULT sql_result;
	DB_ROW sql_row;


	long unsigned int trigger_count=0;

	char 	*triggers_str=NULL;
	size_t 	triggers_alloc=0, triggers_offset=0;

	char	*quotes_str=NULL;
	size_t	quotes_alloc=0, quotes_offset=0;

	char *unescape;
	int i;


	zabbix_log(LOG_LEVEL_DEBUG, "Module: %s raw hostname: %s, raw key: %s \n",__FILE__,request->params[0], request->params[1]);
	
	if (request->nparam<4)
	{
		zabbix_log(LOG_LEVEL_ERR, "Module: %s. Invalid number of parameters.\n",__FILE__);
		SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
		return SYSINFO_RET_FAIL;
	}

	hostname=DBdyn_escape_string(request->params[0]);
	zbx_lrtrim(hostname," ");

	key=DBdyn_escape_string(request->params[1]);
	zbx_lrtrim(key," ");

	trigger_value=atol(request->params[2]);

	zabbix_log(LOG_LEVEL_DEBUG, "Module: %s hostname: %s, LLD key: %s, trigger state: %lu\n",__FILE__,hostname,key,trigger_value);

	sql_result = DBselect(query_hosts, hostname,key);

	if ((sql_row=DBfetch(sql_result)))
		ZBX_STR2UINT64(itemid, sql_row[0]);

	zabbix_log(LOG_LEVEL_DEBUG, "Module: %s itemid: %lu\n",__FILE__,itemid);

	DBfree_result(sql_result);
	zbx_free(hostname);
	zbx_free(key);

	if (itemid==0)
	{
		zabbix_log(LOG_LEVEL_INFORMATION, "Module: %s itemid not found for host: %s, key %s\n",__FILE__,hostname,key);
		SET_MSG_RESULT(result, strdup("Item not found"));
		return SYSINFO_RET_FAIL;
	}


	for (i=3;i<=request->nparam-2;i++)
	{
		unescape=DBdyn_escape_string(unescape_str(request->params[i],'\\',"$"));
		zbx_snprintf_alloc(&quotes_str, &quotes_alloc, &quotes_offset,STR_DELIMITER,unescape);
		zbx_strncpy_alloc(&triggers_str,&triggers_alloc,&triggers_offset,quotes_str,zbx_strlen_utf8(quotes_str));

		zbx_free(unescape);
		zbx_free(quotes_str);
		quotes_alloc=quotes_offset=0;
	}
	
	zabbix_log(LOG_LEVEL_DEBUG, "Module: %s unescape %s quotes_str %s\n",__FILE__,unescape,quotes_str);

	unescape=DBdyn_escape_string(unescape_str(request->params[i],'\\',"$"));
	zbx_snprintf_alloc(&quotes_str, &quotes_alloc, &quotes_offset,"'%s'",unescape);
	zbx_strncpy_alloc(&triggers_str,&triggers_alloc,&triggers_offset,quotes_str,zbx_strlen_utf8(quotes_str));

	zbx_free(unescape);
	zbx_free(quotes_str);

	zabbix_log(LOG_LEVEL_DEBUG, "Module: %s triggers %s\n",__FILE__,triggers_str);


	sql_result = DBselect(query_triggers, trigger_value,itemid,triggers_str);

	if ((sql_row=DBfetch(sql_result)))
		ZBX_STR2UINT64(trigger_count, sql_row[0]);

	zabbix_log(LOG_LEVEL_DEBUG, "Module: %s count %lu\n",__FILE__,trigger_count);

	DBfree_result(sql_result);
	zbx_free(triggers_str);

	SET_UI64_RESULT(result, trigger_count);

	return SYSINFO_RET_OK;
}



/******************************************************************************
 *                                                                            *
 * Function: zbx_module_init                                                  *
 *                                                                            *
 * Purpose: the function is called on agent startup                           *
 *          It should be used to call any initialization routines             *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - module initialization failed               *
 *                                                                            *
 * Comment: the module won't be loaded in case of ZBX_MODULE_FAIL             *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_init(void)
{

	zabbix_log(LOG_LEVEL_DEBUG, "Module: %s is being initializated",__FILE__);
	query_hosts=get_str(QUERY_HOSTS);
	query_triggers=get_str(QUERY_TRIGGERS);

	zabbix_log(LOG_LEVEL_DEBUG, "Module: %s query_hosts %s\n",__FILE__,query_hosts);
	zabbix_log(LOG_LEVEL_DEBUG, "Module: %s query_triggers %s\n",__FILE__,query_triggers);
	zabbix_log(LOG_LEVEL_INFORMATION, "Module: %s had been initializated",__FILE__);

	return ZBX_MODULE_OK;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_uninit                                                *
 *                                                                            *
 * Purpose: the function is called on agent shutdown                          *
 *          It should be used to cleanup used resources if there are any      *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - function failed                            *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_uninit(void)
{
	zbx_free(query_hosts);
	zbx_free(query_triggers);
	return ZBX_MODULE_OK;
}

static void	aggregation_history_float_cb(const ZBX_HISTORY_FLOAT *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

static void	aggregation_history_integer_cb(const ZBX_HISTORY_INTEGER *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

static void	aggregation_history_string_cb(const ZBX_HISTORY_STRING *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

static void	aggregation_history_text_cb(const ZBX_HISTORY_TEXT *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

static void	aggregation_history_log_cb(const ZBX_HISTORY_LOG *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_history_write_cbs                                     *
 *                                                                            *
 * Purpose: returns a set of module functions Zabbix will call to export      *
 *          different types of historical data                                *
 *                                                                            *
 * Return value: structure with callback function pointers (can be NULL if    *
 *               module is not interested in data of certain types)           *
 *                                                                            *
 ******************************************************************************/
ZBX_HISTORY_WRITE_CBS	zbx_module_history_write_cbs(void)
{
	static ZBX_HISTORY_WRITE_CBS	aggregation_callbacks =
	{
			aggregation_history_float_cb,
			aggregation_history_integer_cb,
			aggregation_history_string_cb,
			aggregation_history_text_cb,
			aggregation_history_log_cb,
	};

	return aggregation_callbacks;
}

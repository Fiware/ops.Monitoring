/*
 * Copyright 2013 Telefónica I+D
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "config.h"
#include "nebstructs.h"
#include "nebmodules.h"
#include "nebcallbacks.h"
#include "neberrors.h"
#include "broker.h"
#include "argument_parser.h"
#include "ngsi_event_broker_common.h"


/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION)


/* event broker module identification */
#define MODULE_NAME			PACKAGE_NAME "_xifi"	/* from config.h  */
#define MODULE_VERSION			PACKAGE_VERSION		/* from config.h  */


/* custom variables in service definitions */
#define CUSTOM_VAR_ENTITY_TYPE		"ENTITY_TYPE"		/* = _entity_type */


/* [DEM monitoring] default entity type */
#define DEM_ENTITY_TYPE_LOCAL		"host"
#define DEM_ENTITY_TYPE_REMOTE		"vm"
#define DEM_DEFAULT_ENTITY_TYPE		DEM_ENTITY_TYPE_REMOTE


/* [DEM monitoring] adapter request query string fields (id = region:hostaddr) */
#define DEM_ADAPTER_REQUEST_FORMAT	ADAPTER_REQUEST_FORMAT


/* [NPM monitoring] default entity type */
#define NPM_DEFAULT_ENTITY_TYPE		"interface"


/* [NPM monitoring] adapter request query string fields (id = region:hostaddr/port) */
#define NPM_ADAPTER_REQUEST_FORMAT	"%s/%s" \
					"?" ADAPTER_QUERY_FIELD_ID "=%s:%s/%d" \
					"&" ADAPTER_QUERY_FIELD_TYPE "=%s"


/* [host service monitoring] default entity type */
#define SRV_DEFAULT_ENTITY_TYPE		"host_service"


/* [host service monitoring] adapter request query string fields (id = region:hostname:servname) */
#define SRV_ADAPTER_REQUEST_FORMAT	"%s/%s" \
					"?" ADAPTER_QUERY_FIELD_ID "=%s:%s:%s" \
					"&" ADAPTER_QUERY_FIELD_TYPE "=%s"


/* define module constants and variables */
char* const module_name			= MODULE_NAME;
char* const module_version		= MODULE_VERSION;
void*       module_handle		= NULL;


/* initializes module handle and info (name and version) */
int init_module_handle_info(void* handle)
{
	int result = NEB_OK;

	module_handle = handle;
	logging("info", "%s - Starting up (version %s)...", module_name, module_version);
	neb_set_module_info(module_handle, NEBMODULE_MODINFO_TITLE,   module_name);
	neb_set_module_info(module_handle, NEBMODULE_MODINFO_VERSION, module_version);

	return result;
}


/* gets adapter request URL including query fields */
char* get_adapter_request(nebstruct_service_check_data* data)
{
	char* result = NULL;
	char* name   = NULL;
	char* args   = NULL;
	int   nrpe   = 0;

	/* XIMM module-specific functions */
	char* srv_get_adapter_request(char* name, char* args, const char* type, const service* serv);
	char* dem_get_adapter_request(char* name, char* args, const char* type, int nrpe);
	char* npm_get_adapter_request(char* name, char* args, const char* type);

	/* Build request according to plugin details */
	const service* serv;
	if ((name = find_plugin_command_name(data, &args, &nrpe, &serv)) == NULL) {
		logging("error", "%s - Cannot get plugin command name", module_name);
		result = ADAPTER_REQUEST_INVALID;
	} else if (serv == NULL) {
		logging("error", "%s - Cannot get plugin service details", module_name);
		result = ADAPTER_REQUEST_INVALID;
	} else {
		const char*		type = NULL;
		customvariablesmember*	vars = serv->custom_variables;

		/* get entity type */
		if ((vars != NULL) && !strcmp(vars->variable_name, CUSTOM_VAR_ENTITY_TYPE)) {
			type = vars->variable_value;
		} else if (!strcmp(name, SNMP_PLUGIN)) {
			type = NPM_DEFAULT_ENTITY_TYPE;
		} else if (nrpe) {
			type = DEM_ENTITY_TYPE_REMOTE;
		} else {
			type = DEM_ENTITY_TYPE_LOCAL;
		}

		/* get request URL */
		if (!strcmp(type, SRV_DEFAULT_ENTITY_TYPE)) {
			result = srv_get_adapter_request(name, args, type, serv);
		} else if (!strcmp(type, NPM_DEFAULT_ENTITY_TYPE)) {
			result = npm_get_adapter_request(name, args, type);
		} else {
			result = dem_get_adapter_request(name, args, type, nrpe);
		}
	}

	free(args);
	args = NULL;
	free(name);
	name = NULL;
	return result;
}


/* [NPM monitoring] gets adapter request URL */
char* npm_get_adapter_request(char* name, char* args, const char* type)
{
	char*		result = NULL;
	option_list_t	opts   = NULL;

	/* Take adapter query fields from plugin arguments */
	if ((opts = parse_args(args, ":H:C:o:m:")) == NULL) {
		logging("error", "%s - Cannot get plugin options", module_name);
		result = ADAPTER_REQUEST_INVALID;
	} else {
		char*	host = NULL;
		int	port = -1;
		size_t	i;

		/* For interface 10.11.100.80/20, check_snmp plugin options should
		   look like "-H 10.11.100.80 -o .1.3.6.1.2.1.2.2.1.8.21", where
		   last number of -o option is port number + 1 */
		for (i = 0; opts[i].opt != -1; i++) {
			switch(opts[i].opt) {
				case 'H': {
					host = (char*) opts[i].val;
					break;
				}
				case 'o': {
					char* ptr = strrchr(opts[i].val, '.');
					port = (ptr) ? (atoi(++ptr) - 1) : -1;
					break;
				}
			}
		}
		if ((host == NULL) || (port == -1)) {
			logging("error", "%s - Missing plugin options", module_name);
			result = ADAPTER_REQUEST_INVALID;
		} else {
			char buffer[MAXBUFLEN];
			snprintf(buffer, sizeof(buffer)-1, NPM_ADAPTER_REQUEST_FORMAT,
			         adapter_url, name, region_id, host, port, type);
			buffer[sizeof(buffer)-1] = '\0';
			result = strdup(buffer);
		}
	}

	free(opts);
	opts = NULL;
	return result;
}


/* [DEM monitoring] gets adapter request URL */
char* dem_get_adapter_request(char* name, char* args, const char* type, int nrpe)
{
	char		buffer[MAXBUFLEN];
	char*		result = NULL;
	option_list_t	opts   = NULL;

	/* Take adapter query fields from plugin arguments, distinguishing
	   between local executions (host_addr as identifier) and NRPE plugin
	   executions (-H plugin argument as identifier) */
	if (!nrpe) {
		char* addr = host_addr;
		snprintf(buffer, sizeof(buffer)-1, DEM_ADAPTER_REQUEST_FORMAT,
		         adapter_url, name, region_id, addr, type);
		buffer[sizeof(buffer)-1] = '\0';
		result = strdup(buffer);
	} else if ((opts = parse_args(args, ":H:c:t:")) == NULL) {
		logging("error", "%s - Cannot get NRPE plugin options", module_name);
		result = ADAPTER_REQUEST_INVALID;
	} else {
		char        addr[INET_ADDRSTRLEN];
		const char* host = NULL;
		size_t      i;

		for (i = 0; opts[i].opt != -1; i++) {
			switch(opts[i].opt) {
				case 'H': {
					host = opts[i].val;
					break;
				}
			}
		}
		if (host == NULL) {
			logging("error", "%s - Missing NRPE plugin options", module_name);
			result = ADAPTER_REQUEST_INVALID;
		} else if (resolve_address(host, addr, INET_ADDRSTRLEN)) {
			logging("error", "%s - Cannot resolve remote address for %s", module_name, host);
			result = ADAPTER_REQUEST_INVALID;
		} else {
			snprintf(buffer, sizeof(buffer)-1, DEM_ADAPTER_REQUEST_FORMAT,
			         adapter_url, name, region_id, addr, type);
			buffer[sizeof(buffer)-1] = '\0';
			result = strdup(buffer);
		}
	}

	free(opts);
	opts = NULL;
	return result;
}


/* [host service monitoring] gets adapter request URL */
char* srv_get_adapter_request(char* name, char* args, const char* type, const service* serv)
{
	char* result = NULL;
	char  buffer[MAXBUFLEN];

	snprintf(buffer, sizeof(buffer)-1, SRV_ADAPTER_REQUEST_FORMAT,
	         adapter_url, name, region_id, serv->host_name, serv->description, type);
	buffer[sizeof(buffer)-1] = '\0';

	result = strdup(buffer);
	return result;
}

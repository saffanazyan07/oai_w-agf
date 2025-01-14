/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*****************************************************************************

Source    network_api.h

Version   0.1

Date    2012/03/01

Product   NAS stack

Subsystem Application Programming Interface

Author    Frederic Maurel

Description Implements the API used by the NAS layer to send/receive
    message to/from the network layer

*****************************************************************************/
#ifndef __NETWORK_API_H__
#define __NETWORK_API_H__

/****************************************************************************/
/*********************  G L O B A L    C O N S T A N T S  *******************/
/****************************************************************************/

/****************************************************************************/
/************************  G L O B A L    T Y P E S  ************************/
/****************************************************************************/

/****************************************************************************/
/********************  G L O B A L    V A R I A B L E S  ********************/
/****************************************************************************/

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/
#include "openair2/COMMON/as_message.h"
int network_api_initialize(const char* host, const char* port);

int network_api_get_fd(void);

int network_api_read_data(int fd, char *_network_api_recv_buffer);
void network_api_close(int fd);

int network_api_decode_data(char *_network_api_recv_buffer, int length, as_message_t *_as_data);

#endif /* __NETWORK_API_H__*/

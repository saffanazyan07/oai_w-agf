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

#ifndef OAI_MPLANE_H
#define OAI_MPLANE_H

#include "mplane-params.h"

#include <stdio.h>

typedef struct{
  /* add all relevant configs,
    ru-delay-profile,
    names for carriers and their values,
    compression,
    names for low-level-tx/rx-endpoints and their values,... 
    
    or maybe create separate structs, one for ru-delay-profile and
    the other for radio config */
} ru_config_t;

int init_mplane(void);

ru_config_t get_ru_config_mplane(void);

int edit_ru_config_mplane(void);

int free_mplane(void);

#endif /* OAI_MPLANE_H */

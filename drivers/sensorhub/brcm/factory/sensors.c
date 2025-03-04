
/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include "sensors.h"

void push_back(struct sensor_manager *sm, const char* key, void* item) {
    if(sm->size < ITEM_MAX){
        sm->item[sm->size] = item;
        memcpy(sm->key[sm->size], key, strlen(key)); 
        sm->size++;
    }
}

void* get_sensor_ptr(struct ssp_data *data, struct sensor_manager *sm, int type) { 
    //struct ssp_data *data = dev_get_drvdata(dev);
    int i = 0;

    if (sm->found == true) {
        goto default_get_sensor_ptr;
    }

    if(data->sensor_name[type][0] == 0){
        return sm->item[0];
    }

    for (i = 0; i < sm->size; i++){
            if(strcmp(data->sensor_name[type], sm->key[i]) == 0) {
                sm->index = i;
                sm->found = true;
                pr_info("[SSP] %s: index: %d sensor_name: %s\n", __func__, sm->index, data->sensor_name[type]);          
                break;
            }
    }

default_get_sensor_ptr:
    return sm->item[sm->index];
}

ssize_t sensor_default_show(struct device *dev, char *buf) { return 0; }
ssize_t sensor_default_store(struct device *dev, const char *buf, size_t size) { return size; }
void sensor_default_initialize(struct ssp_data *data) {}
int sensor_default_get_data(void) { return 0; }
int sensor_default_check_func(struct ssp_data *data, int sensor_type) { return 0; }
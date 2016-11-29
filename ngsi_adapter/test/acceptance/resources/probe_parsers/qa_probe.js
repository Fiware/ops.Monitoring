/*
 * Copyright 2016 Telefónica I+D
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */


'use strict';


var parser = Object.create(null);


parser.parseRequest = function (reqdomain) {
    var entityData = { data: reqdomain.body.split('\n')[0] };
    return entityData;
};


parser.getContextAttrs = function(probeEntityData) {
    var data = probeEntityData.data.split('\n')[0];     // only consider first line of probe data, discard perfData
    var attrs = { users: NaN };

    var items = data.split('-');
    if (items[1]) {
        attrs.users = parseFloat(items[1].trim().split(' ')[0]);
    }

    if (isNaN(attrs.users)) {
        throw new Error('No valid users data found');
    }

    return attrs;
};


exports.parser = parser;

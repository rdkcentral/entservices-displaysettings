/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/
#pragma once

#define RDK_PROFILE "RDK_PROFILE="
#define PROFILE_TV "TV"
#define PROFILE_STB "STB"

typedef enum profile {
    NOT_FOUND = -1,
    STB = 0,
    TV,
    MAX
} profile_t;

// External declaration - actual definition in UtilsSearchRDKProfile.cpp
extern profile_t profileType;

// Function declaration - actual definition in UtilsSearchRDKProfile.cpp  
profile_t searchRdkProfile(void);

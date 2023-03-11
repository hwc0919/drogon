/**
*
*  @file ControllerMacro.h
*  @author Nitromelon
*
*  Copyright 2023, Nitromelon.  All rights reserved.
*  https://github.com/drogonframework/drogon
*  Use of this source code is governed by a MIT license
*  that can be found in the License file.
*
*  Drogon
*
*/

#pragma once

#define PATH_LIST_BEGIN           \
    static void initPathRouting() \
    {
#define PATH_ADD(path, ...) registerSelf__(path, {__VA_ARGS__})
#define PATH_LIST_END }

/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#pragma once

#include <future>
#include <unordered_map>

namespace Mayo {

using TaskId = uint64_t;
enum class TaskAutoDestroy { On, Off };

} // namespace Mayo

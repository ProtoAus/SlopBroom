/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Result.h"

#include <string>
#include <string_view>

namespace tb::mdl
{

/**
 * Convert a J.A.C.K. / Jackhammer "JHMF" (.jmf) binary map into TrenchBroom Valve-format
 * `.map` text, so the result can be opened through the normal load pipeline. World/entity
 * geometry comes across, and JMF visgroups (with their colors + multi-membership) are emitted
 * as the `_tb_visgroup_*` properties our loader reads. The byte layout was reverse-engineered
 * + validated against real files (version 121).
 */
Result<std::string> convertJmfToMapText(std::string_view jmfBytes);

} // namespace tb::mdl

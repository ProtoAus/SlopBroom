/*
 Copyright (C) 2026 Kristian Duske

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

#include "mdl/CatchConfig.h"
#include "mdl/VmfImport.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
// A minimal VMF: one worldspawn solid with 6 sides (a cube), plus a stray skyname keyvalue.
const auto cubeVmf = std::string{R"VMF(versioninfo
{
	"editorversion" "400"
}
world
{
	"id" "1"
	"classname" "worldspawn"
	"skyname" "sky_day01_01"
	solid
	{
		"id" "2"
		side { "id" "3" "plane" "(-32 -32 32) (32 -32 32) (32 32 32)" "material" "DEV/DEV_MEASUREGENERIC01" "uaxis" "[1 0 0 0] 0.25" "vaxis" "[0 -1 0 0] 0.25" "rotation" "0" }
		side { "id" "4" "plane" "(-32 -32 -32) (-32 32 -32) (32 32 -32)" "material" "DEV/DEV_MEASUREGENERIC01" "uaxis" "[1 0 0 0] 0.25" "vaxis" "[0 -1 0 0] 0.25" "rotation" "0" }
		side { "id" "5" "plane" "(-32 32 32) (-32 32 -32) (32 32 -32)" "material" "DEV/DEV_MEASUREGENERIC01" "uaxis" "[1 0 0 0] 0.25" "vaxis" "[0 0 -1 0] 0.25" "rotation" "0" }
		side { "id" "6" "plane" "(-32 -32 32) (32 -32 32) (32 -32 -32)" "material" "DEV/DEV_MEASUREGENERIC01" "uaxis" "[1 0 0 0] 0.25" "vaxis" "[0 0 -1 0] 0.25" "rotation" "0" }
		side { "id" "7" "plane" "(32 -32 32) (32 32 32) (32 32 -32)" "material" "DEV/DEV_MEASUREGENERIC01" "uaxis" "[0 1 0 0] 0.25" "vaxis" "[0 0 -1 0] 0.25" "rotation" "0" }
		side { "id" "8" "plane" "(-32 -32 32) (-32 32 32) (-32 32 -32)" "material" "DEV/DEV_MEASUREGENERIC01" "uaxis" "[0 1 0 0] 0.25" "vaxis" "[0 0 -1 0] 0.25" "rotation" "0" }
	}
}
)VMF"};
} // namespace

TEST_CASE("VmfImport")
{
  SECTION("parseVmf builds the block tree")
  {
    auto result = parseVmf(cubeVmf);
    REQUIRE(result.is_success());
    const auto& roots = result.value();

    // versioninfo + world
    REQUIRE(roots.size() == 2u);
    CHECK(roots[0].name == "versioninfo");
    CHECK(roots[1].name == "world");

    const auto& world = roots[1];
    REQUIRE(world.property("classname") != nullptr);
    CHECK(*world.property("classname") == "worldspawn");
    REQUIRE(world.property("skyname") != nullptr);
    CHECK(*world.property("skyname") == "sky_day01_01");

    const auto solids = world.childrenNamed("solid");
    REQUIRE(solids.size() == 1u);
    const auto sides = solids[0]->childrenNamed("side");
    REQUIRE(sides.size() == 6u);
    REQUIRE(sides[0]->property("plane") != nullptr);
    CHECK(*sides[0]->property("material") == "DEV/DEV_MEASUREGENERIC01");
  }

  SECTION("repeated keys are preserved (multi-membership groundwork)")
  {
    auto result = parseVmf(std::string{R"VMF(editor
{
	"visgroupid" "1"
	"visgroupid" "7"
}
)VMF"});
    REQUIRE(result.is_success());
    const auto values = result.value()[0].propertyValues("visgroupid");
    CHECK(values == std::vector<std::string>{"1", "7"});
  }

  SECTION("convertVmfToMapText emits a Valve worldspawn brush")
  {
    auto result = convertVmfToMapText(cubeVmf);
    REQUIRE(result.is_success());
    const auto& mapText = result.value();

    CHECK(mapText.find("// Format: Valve") != std::string::npos);
    CHECK(mapText.find("\"classname\" \"worldspawn\"") != std::string::npos);
    CHECK(mapText.find("\"skyname\" \"sky_day01_01\"") != std::string::npos);
    CHECK(mapText.find("DEV/DEV_MEASUREGENERIC01") != std::string::npos);
    // a Valve-format face: three plane points then bracketed texture axes
    CHECK(mapText.find("( -32 -32 32 ) ( 32 -32 32 ) ( 32 32 32 )") != std::string::npos);
    CHECK(mapText.find("[ 1 0 0 0 ]") != std::string::npos);
  }

  SECTION("malformed VMF fails gracefully")
  {
    CHECK(parseVmf(std::string{"world { "}).is_error()); // unterminated block
  }
}

} // namespace tb::mdl

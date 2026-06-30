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

#include "mdl/JmfImport.h"

#include "Error.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace tb::mdl
{
namespace
{

struct Vec3
{
  float x = 0, y = 0, z = 0;
};

Vec3 cross(const Vec3& a, const Vec3& b)
{
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 sub(const Vec3& a, const Vec3& b)
{
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
float dot(const Vec3& a, const Vec3& b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Format a float like a .map writer: whole numbers as integers, else %.6g.
std::string num(const float x)
{
  if (std::isfinite(x) && x == std::trunc(x) && std::fabs(x) < 1e15f)
  {
    return std::to_string(static_cast<long long>(x));
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.6g", static_cast<double>(x));
  return std::string{buf};
}

// Little-endian binary cursor over the JMF bytes; sets a fail flag on overrun (never throws).
class Reader
{
private:
  std::string_view m_data;
  size_t m_pos = 0;
  bool m_ok = true;

public:
  explicit Reader(const std::string_view data)
    : m_data{data}
  {
  }

  bool ok() const { return m_ok; }
  bool atEnd() const { return m_pos >= m_data.size(); }

  bool ensure(const size_t n)
  {
    if (m_pos + n > m_data.size())
    {
      m_ok = false;
      return false;
    }
    return true;
  }

  void skip(const size_t n)
  {
    if (ensure(n))
    {
      m_pos += n;
    }
  }

  int32_t i32()
  {
    if (!ensure(4))
    {
      return 0;
    }
    int32_t v = 0;
    std::memcpy(&v, m_data.data() + m_pos, 4);
    m_pos += 4;
    return v;
  }

  uint8_t u8()
  {
    if (!ensure(1))
    {
      return 0;
    }
    return static_cast<uint8_t>(m_data[m_pos++]);
  }

  float f32()
  {
    if (!ensure(4))
    {
      return 0;
    }
    float v = 0;
    std::memcpy(&v, m_data.data() + m_pos, 4);
    m_pos += 4;
    return v;
  }

  Vec3 vec3()
  {
    const auto x = f32();
    const auto y = f32();
    const auto z = f32();
    return {x, y, z};
  }

  std::array<uint8_t, 4> rgba()
  {
    return {u8(), u8(), u8(), u8()};
  }

  // szp: int32 length + that many bytes (no trailing null; we still trim any nulls).
  std::string szp()
  {
    const auto n = i32();
    if (n <= 0 || !ensure(static_cast<size_t>(n)))
    {
      return {};
    }
    auto s = std::string{m_data.substr(m_pos, static_cast<size_t>(n))};
    m_pos += static_cast<size_t>(n);
    if (const auto z = s.find('\0'); z != std::string::npos)
    {
      s.resize(z);
    }
    return s;
  }

  // fixed-size null-padded ASCII buffer (e.g. the 64-byte texture name).
  std::string fixedString(const size_t n)
  {
    if (!ensure(n))
    {
      return {};
    }
    auto s = std::string{m_data.substr(m_pos, n)};
    m_pos += n;
    if (const auto z = s.find('\0'); z != std::string::npos)
    {
      s.resize(z);
    }
    return s;
  }
};

struct VisGroup
{
  std::string name;
  std::array<uint8_t, 4> color{};
  bool visible = true;
};

struct Face
{
  Vec3 uAxis, vAxis, planeNormal;
  float uShift = 0, vShift = 0, uScale = 1, vScale = 1, rotation = 0;
  std::string texture;
  std::vector<Vec3> vertices;
};

struct Solid
{
  std::vector<int> visGroupIds;
  std::vector<Face> faces;
};

struct Entity
{
  std::string classname;
  std::vector<std::pair<std::string, std::string>> properties;
  std::vector<int> visGroupIds;
  std::vector<Solid> solids;
  Vec3 origin;
};

// Emit one Valve-format .map face from a JMF face. Returns false if it has < 3 vertices.
bool emitFace(std::ostream& out, const Face& f)
{
  if (f.vertices.size() < 3)
  {
    return false;
  }
  auto p0 = f.vertices[0];
  auto p1 = f.vertices[1];
  auto p2 = f.vertices[2];
  // order the 3 points so TrenchBroom's plane normal (cross(p0-p1, p2-p1)) matches the JMF
  // outward normal — otherwise the brush is inside-out and gets rejected.
  if (dot(cross(sub(p0, p1), sub(p2, p1)), f.planeNormal) < 0)
  {
    std::swap(p1, p2);
  }

  const auto& tex = f.texture.empty() ? std::string{"SKIP"} : f.texture;
  const auto pt = [](const Vec3& q) {
    return "( " + num(q.x) + " " + num(q.y) + " " + num(q.z) + " )";
  };

  out << pt(p0) << " " << pt(p1) << " " << pt(p2) << " " << tex << " "
      << "[ " << num(f.uAxis.x) << " " << num(f.uAxis.y) << " " << num(f.uAxis.z) << " "
      << num(f.uShift) << " ] "
      << "[ " << num(f.vAxis.x) << " " << num(f.vAxis.y) << " " << num(f.vAxis.z) << " "
      << num(f.vShift) << " ] " << num(f.rotation) << " " << num(f.uScale) << " "
      << num(f.vScale) << "\n";
  return true;
}

std::string joinIds(const std::vector<int>& ids)
{
  auto out = std::string{};
  for (size_t i = 0; i < ids.size(); ++i)
  {
    out += (i ? " " : "") + std::to_string(ids[i]);
  }
  return out;
}

std::string emitMap(
  const std::vector<Entity>& ents, const std::map<int, VisGroup>& visGroups)
{
  auto out = std::ostringstream{};
  out << "// Game: Half-Life\n// Format: Valve\n";

  const auto& world = ents.front();

  out << "{\n\"classname\" \"worldspawn\"\n";
  for (const auto& [key, value] : world.properties)
  {
    if (key != "classname")
    {
      out << "\"" << key << "\" \"" << value << "\"\n";
    }
  }
  // visgroup definitions
  for (const auto& [id, vg] : visGroups)
  {
    char hex[8];
    std::snprintf(hex, sizeof(hex), "%02x%02x%02x", vg.color[0], vg.color[1], vg.color[2]);
    out << "\"_tb_visgroup_def_" << id << "\" \"" << (vg.visible ? 1 : 0) << " " << hex << " "
        << vg.name << "\"\n";
  }
  // raw world-brush membership table (cid 0 = default layer; ord = brush order)
  auto brushTable = std::vector<std::string>{};
  auto ord = 0;
  for (const auto& solid : world.solids)
  {
    if (!solid.visGroupIds.empty())
    {
      brushTable.push_back("0/" + std::to_string(ord) + "=" + joinIds(solid.visGroupIds));
    }
    ++ord;
  }
  if (!brushTable.empty())
  {
    out << "\"_tb_visgroup_brushes\" \"";
    for (size_t i = 0; i < brushTable.size(); ++i)
    {
      out << (i ? ";" : "") << brushTable[i];
    }
    out << "\"\n";
  }
  // world brushes
  for (const auto& solid : world.solids)
  {
    out << "{\n";
    for (const auto& face : solid.faces)
    {
      emitFace(out, face);
    }
    out << "}\n";
  }
  out << "}\n";

  // other entities
  for (size_t i = 1; i < ents.size(); ++i)
  {
    const auto& e = ents[i];
    out << "{\n\"classname\" \"" << e.classname << "\"\n";
    auto hasOrigin = false;
    for (const auto& [key, value] : e.properties)
    {
      if (key != "classname")
      {
        out << "\"" << key << "\" \"" << value << "\"\n";
      }
      if (key == "origin")
      {
        hasOrigin = true;
      }
    }
    // point entities (no brushes) get their position from the binary origin field
    if (e.solids.empty() && !hasOrigin)
    {
      out << "\"origin\" \"" << num(e.origin.x) << " " << num(e.origin.y) << " "
          << num(e.origin.z) << "\"\n";
    }
    if (!e.visGroupIds.empty())
    {
      out << "\"_tb_visgroups\" \"" << joinIds(e.visGroupIds) << "\"\n";
    }
    for (const auto& solid : e.solids)
    {
      out << "{\n";
      for (const auto& face : solid.faces)
      {
        emitFace(out, face);
      }
      out << "}\n";
    }
    out << "}\n";
  }

  return out.str();
}

} // namespace

Result<std::string> convertJmfToMapText(const std::string_view jmfBytes)
{
  if (jmfBytes.size() < 8 || jmfBytes.substr(0, 4) != "JHMF")
  {
    return Error{"Not a JMF file (missing JHMF signature)"};
  }

  auto r = Reader{jmfBytes};
  r.skip(4);          // "JHMF"
  const auto unused = r.i32(); // version (121/122)
  (void)unused;

  for (auto exportCount = r.i32(); exportCount > 0; --exportCount)
  {
    r.szp();
  }
  for (auto groupCount = r.i32(); groupCount > 0; --groupCount)
  {
    r.i32(); // id
    r.i32(); // parent id
    r.i32(); // flags
    r.i32(); // num objects
    r.rgba();
  }

  auto visGroups = std::map<int, VisGroup>{};
  for (auto vgCount = r.i32(); vgCount > 0; --vgCount)
  {
    auto name = r.szp();
    const auto id = r.i32();
    const auto color = r.rgba();
    const auto visible = r.u8();
    visGroups[id] = VisGroup{std::move(name), color, visible != 0};
  }

  r.vec3(); // cordon min
  r.vec3(); // cordon max
  for (auto camCount = r.i32(); camCount > 0; --camCount)
  {
    r.vec3(); // eye
    r.vec3(); // lookat
    r.i32();  // flags
    r.rgba();
  }
  for (auto pathCount = r.i32(); pathCount > 0; --pathCount)
  {
    r.szp(); // classname
    r.szp(); // name
    r.i32(); // direction
    r.i32(); // flags
    r.rgba();
    for (auto nodeCount = r.i32(); nodeCount > 0; --nodeCount)
    {
      r.szp();
      r.szp();
      r.vec3();
      r.vec3();
      r.i32();
      r.rgba();
      for (auto propCount = r.i32(); propCount > 0; --propCount)
      {
        r.szp();
        r.szp();
      }
    }
  }

  auto ents = std::vector<Entity>{};
  while (!r.atEnd() && r.ok())
  {
    auto e = Entity{};
    e.classname = r.szp();
    e.origin = r.vec3();
    r.i32(); // flags
    r.i32(); // group id
    r.i32(); // root group id
    r.rgba();
    for (auto i = 0; i < 13; ++i)
    {
      r.szp(); // 13 hardcoded attribute strings
    }
    r.i32();  // spawnflags
    r.vec3(); // angles
    r.i32();  // rendering
    r.rgba(); // fx color
    r.i32();  // render mode
    r.i32();  // render fx
    r.skip(4);   // body(i2) + skin(i2)
    r.i32();  // sequence
    r.f32();  // framerate
    r.f32();  // scale
    r.f32();  // radius
    r.skip(28);  // unknown
    for (auto propCount = r.i32(); propCount > 0; --propCount)
    {
      auto key = r.szp();
      auto value = r.szp();
      e.properties.emplace_back(std::move(key), std::move(value));
    }
    for (auto vgc = r.i32(); vgc > 0; --vgc)
    {
      e.visGroupIds.push_back(r.i32());
    }

    const auto solidCount = r.i32();
    if (!r.ok() || solidCount < 0 || solidCount > 1'000'000)
    {
      return Error{"JMF parse error: implausible solid count"};
    }
    for (auto s = 0; s < solidCount; ++s)
    {
      auto solid = Solid{};
      const auto patchCount = r.i32();
      if (patchCount != 0)
      {
        return Error{"JMF contains Quake III patches, which are not supported"};
      }
      r.i32(); // flags
      r.i32(); // group id
      r.i32(); // root group id
      r.rgba();
      for (auto vgc = r.i32(); vgc > 0; --vgc)
      {
        solid.visGroupIds.push_back(r.i32());
      }
      const auto faceCount = r.i32();
      if (!r.ok() || faceCount < 0 || faceCount > 1'000'000)
      {
        return Error{"JMF parse error: implausible face count"};
      }
      for (auto fi = 0; fi < faceCount; ++fi)
      {
        auto f = Face{};
        r.i32(); // render flags
        const auto vertexCount = r.i32();
        f.uAxis = r.vec3();
        f.uShift = r.f32();
        f.vAxis = r.vec3();
        f.vShift = r.f32();
        f.uScale = r.f32();
        f.vScale = r.f32();
        f.rotation = r.f32();
        r.i32();     // texture alignment
        r.skip(12);  // unknown (3x int32)
        r.i32();     // surface contents
        f.texture = r.fixedString(64);
        f.planeNormal = r.vec3();
        r.f32(); // plane distance
        r.i32(); // axis alignment
        if (!r.ok() || vertexCount < 0 || vertexCount > 1'000'000)
        {
          return Error{"JMF parse error: implausible vertex count"};
        }
        f.vertices.reserve(static_cast<size_t>(vertexCount));
        for (auto vi = 0; vi < vertexCount; ++vi)
        {
          const auto pos = r.vec3();
          r.f32(); // u
          r.f32(); // v
          r.i32(); // selection state
          f.vertices.push_back(pos);
        }
        solid.faces.push_back(std::move(f));
      }
      e.solids.push_back(std::move(solid));
    }
    ents.push_back(std::move(e));
  }

  if (!r.ok())
  {
    return Error{"JMF parse error: unexpected end of data"};
  }
  if (ents.empty())
  {
    return Error{"JMF contains no entities"};
  }

  return emitMap(ents, visGroups);
}

} // namespace tb::mdl

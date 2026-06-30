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

#include "mdl/VmfImport.h"

#include "Error.h"

#include <cctype>
#include <optional>
#include <sstream>

namespace tb::mdl
{

// ---------------------------------------------------------------------------
// VmfNode accessors
// ---------------------------------------------------------------------------

const std::string* VmfNode::property(const std::string_view key) const
{
  for (const auto& [k, v] : properties)
  {
    if (k == key)
    {
      return &v;
    }
  }
  return nullptr;
}

std::vector<std::string> VmfNode::propertyValues(const std::string_view key) const
{
  auto result = std::vector<std::string>{};
  for (const auto& [k, v] : properties)
  {
    if (k == key)
    {
      result.push_back(v);
    }
  }
  return result;
}

std::vector<const VmfNode*> VmfNode::childrenNamed(const std::string_view childName) const
{
  auto result = std::vector<const VmfNode*>{};
  for (const auto& child : children)
  {
    if (child.name == childName)
    {
      result.push_back(&child);
    }
  }
  return result;
}

// ---------------------------------------------------------------------------
// Parser (recursive descent over the VMF text)
// ---------------------------------------------------------------------------

namespace
{
class VmfParser
{
private:
  std::string_view m_text;
  size_t m_pos = 0;
  std::string m_error;

public:
  explicit VmfParser(const std::string_view text)
    : m_text{text}
  {
  }

  Result<std::vector<VmfNode>> parse()
  {
    auto nodes = std::vector<VmfNode>{};
    while (!atEnd())
    {
      auto node = VmfNode{};
      if (!parseBlock(node))
      {
        return Error{m_error};
      }
      nodes.push_back(std::move(node));
    }
    return nodes;
  }

private:
  void skipWhitespaceAndComments()
  {
    while (m_pos < m_text.size())
    {
      const auto c = m_text[m_pos];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      {
        ++m_pos;
      }
      else if (c == '/' && m_pos + 1 < m_text.size() && m_text[m_pos + 1] == '/')
      {
        while (m_pos < m_text.size() && m_text[m_pos] != '\n')
        {
          ++m_pos;
        }
      }
      else
      {
        break;
      }
    }
  }

  bool atEnd()
  {
    skipWhitespaceAndComments();
    return m_pos >= m_text.size();
  }

  char peek()
  {
    skipWhitespaceAndComments();
    return m_pos < m_text.size() ? m_text[m_pos] : '\0';
  }

  bool consume(const char expected)
  {
    skipWhitespaceAndComments();
    if (m_pos < m_text.size() && m_text[m_pos] == expected)
    {
      ++m_pos;
      return true;
    }
    return false;
  }

  std::optional<std::string> readQuoted()
  {
    skipWhitespaceAndComments();
    if (m_pos >= m_text.size() || m_text[m_pos] != '"')
    {
      return std::nullopt;
    }
    ++m_pos; // opening quote
    auto value = std::string{};
    while (m_pos < m_text.size() && m_text[m_pos] != '"')
    {
      value += m_text[m_pos++];
    }
    if (m_pos >= m_text.size())
    {
      return std::nullopt; // unterminated string
    }
    ++m_pos; // closing quote
    return value;
  }

  std::optional<std::string> readIdentifier()
  {
    skipWhitespaceAndComments();
    const auto start = m_pos;
    while (m_pos < m_text.size())
    {
      const auto c = m_text[m_pos];
      if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
      {
        ++m_pos;
      }
      else
      {
        break;
      }
    }
    if (m_pos == start)
    {
      return std::nullopt;
    }
    return std::string{m_text.substr(start, m_pos - start)};
  }

  bool parseBlock(VmfNode& node)
  {
    auto name = readIdentifier();
    if (!name)
    {
      m_error = "expected a VMF block name";
      return false;
    }
    if (!consume('{'))
    {
      m_error = "expected '{' after VMF block '" + *name + "'";
      return false;
    }
    node.name = std::move(*name);

    while (true)
    {
      const auto c = peek();
      if (c == '\0')
      {
        m_error = "unexpected end of file inside VMF block '" + node.name + "'";
        return false;
      }
      if (c == '}')
      {
        consume('}');
        return true;
      }
      if (c == '"')
      {
        auto key = readQuoted();
        auto value = readQuoted();
        if (!key || !value)
        {
          m_error = "malformed key/value in VMF block '" + node.name + "'";
          return false;
        }
        node.properties.emplace_back(std::move(*key), std::move(*value));
      }
      else
      {
        auto child = VmfNode{};
        if (!parseBlock(child))
        {
          return false;
        }
        node.children.push_back(std::move(child));
      }
    }
  }
};

// Split on whitespace after replacing ()[] with spaces — turns a VMF plane
// "(x y z) (x y z) (x y z)" into 9 number tokens and a uaxis "[x y z o] s" into 5.
std::vector<std::string> numberTokens(const std::string& s)
{
  auto cleaned = std::string{};
  cleaned.reserve(s.size());
  for (const auto c : s)
  {
    cleaned += (c == '(' || c == ')' || c == '[' || c == ']') ? ' ' : c;
  }

  auto tokens = std::vector<std::string>{};
  auto stream = std::istringstream{cleaned};
  auto token = std::string{};
  while (stream >> token)
  {
    tokens.push_back(token);
  }
  return tokens;
}

// Emit one Valve-format .map face line from a VMF `side` block. Returns false (skip the
// face) if the side is malformed. VMF number strings are passed through verbatim — no
// float parsing, so no precision loss.
bool emitFace(std::ostream& out, const VmfNode& side)
{
  const auto* plane = side.property("plane");
  const auto* material = side.property("material");
  const auto* uaxis = side.property("uaxis");
  const auto* vaxis = side.property("vaxis");
  if (!plane || !material || !uaxis || !vaxis)
  {
    return false;
  }

  const auto p = numberTokens(*plane);   // 9: three (x y z) points
  const auto u = numberTokens(*uaxis);   // 5: ux uy uz uoffset uscale
  const auto v = numberTokens(*vaxis);   // 5: vx vy vz voffset vscale
  if (p.size() < 9 || u.size() < 5 || v.size() < 5)
  {
    return false;
  }

  const auto* rotation = side.property("rotation");
  const auto rot = rotation ? *rotation : std::string{"0"};

  out << "( " << p[0] << " " << p[1] << " " << p[2] << " ) "
      << "( " << p[3] << " " << p[4] << " " << p[5] << " ) "
      << "( " << p[6] << " " << p[7] << " " << p[8] << " ) " << *material << " "
      << "[ " << u[0] << " " << u[1] << " " << u[2] << " " << u[3] << " ] "
      << "[ " << v[0] << " " << v[1] << " " << v[2] << " " << v[3] << " ] " << rot << " "
      << u[4] << " " << v[4] << "\n";
  return true;
}

// Emit a brush ("{ faces }") from a VMF `solid` block. Returns false if it has no faces.
bool emitBrush(std::ostream& out, const VmfNode& solid)
{
  const auto sides = solid.childrenNamed("side");
  if (sides.empty())
  {
    return false;
  }

  auto brush = std::ostringstream{};
  brush << "{\n";
  auto faceCount = 0;
  for (const auto* side : sides)
  {
    if (emitFace(brush, *side))
    {
      ++faceCount;
    }
  }
  brush << "}\n";

  if (faceCount < 4) // a valid convex brush needs at least 4 faces
  {
    return false;
  }
  out << brush.str();
  return true;
}
} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Result<std::vector<VmfNode>> parseVmf(const std::string_view text)
{
  return VmfParser{text}.parse();
}

Result<std::string> convertVmfToMapText(const std::string_view vmfText)
{
  return parseVmf(vmfText) | kdl::transform([](const auto& roots) {
           auto out = std::ostringstream{};
           // No "// Game:" line, so TrenchBroom prompts for the game on open; the format
           // is Valve because VMF uses uaxis/vaxis texture projection.
           out << "// Format: Valve\n";
           out << "// entity 0\n{\n\"classname\" \"worldspawn\"\n";

           for (const auto& root : roots)
           {
             if (root.name == "world")
             {
               // carry over worldspawn keyvalues (skip the editor id + classname we wrote)
               for (const auto& [key, value] : root.properties)
               {
                 if (key != "id" && key != "classname")
                 {
                   out << "\"" << key << "\" \"" << value << "\"\n";
                 }
               }
               for (const auto* solid : root.childrenNamed("solid"))
               {
                 emitBrush(out, *solid);
               }
             }
           }

           out << "}\n";
           return out.str();
         });
}

} // namespace tb::mdl

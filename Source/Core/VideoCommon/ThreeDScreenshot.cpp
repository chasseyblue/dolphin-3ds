// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/ThreeDScreenshot.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include "Core/Config/GraphicsSettings.h"

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace ThreeDScreenshot
{
namespace
{
struct Triangle
{
  std::array<Vertex, 3> vertices;
  u32 material_index;
};

struct Material
{
  std::string name;
  std::string texture_file_name;
};

std::mutex s_mutex;
std::atomic_bool s_requested = false;
std::string s_path;
std::vector<Triangle> s_triangles;
std::vector<Material> s_materials;
std::unordered_map<std::string, u32> s_material_indices;
u32 s_empty_end_frames = 0;
constexpr u32 EMPTY_END_FRAME_LIMIT = 120;
constexpr u32 VERTEX_COLOR_MATERIAL = 0;

std::string GetMaterialPath(std::string_view obj_path)
{
  std::string path(obj_path);
  const std::size_t extension = path.find_last_of('.');
  if (extension == std::string::npos)
    return path + ".mtl";

  path.replace(extension, std::string::npos, ".mtl");
  return path;
}

std::string GetFileName(std::string_view path)
{
  const std::size_t separator = path.find_last_of("/\\");
  if (separator == std::string_view::npos)
    return std::string(path);

  return std::string(path.substr(separator + 1));
}

std::string GetDirectory(std::string_view path)
{
  const std::size_t separator = path.find_last_of("/\\");
  if (separator == std::string_view::npos)
    return {};

  return std::string(path.substr(0, separator + 1));
}

std::string SanitizeFileName(std::string name)
{
  for (char& c : name)
  {
    if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' ||
        c == '?' || c == '*' || static_cast<unsigned char>(c) < 0x20)
    {
      c = '_';
    }
  }

  if (name.empty())
    return "texture";

  return name;
}

bool WriteCapture(std::string_view obj_path, std::string_view mtl_path,
                  const std::vector<Triangle>& triangles, const std::vector<Material>& materials)
{
  File::IOFile obj(std::string(obj_path), "w");
  File::IOFile mtl(std::string(mtl_path), "w");
  if (!obj || !mtl)
    return false;

  obj.WriteString("# Dolphin 3D screenshot\n");
  obj.WriteString("# Captured from Dolphin's native vertex stream before backend draw.\n");
  obj.WriteString(fmt::format("mtllib {}\n", GetFileName(mtl_path)));
  obj.WriteString("o dolphin_3d_screenshot\n");

  for (const Triangle& triangle : triangles)
  {
    for (const Vertex& vertex : triangle.vertices)
    {
      obj.WriteString(fmt::format("v {:.9g} {:.9g} {:.9g} {:.6f} {:.6f} {:.6f}\n",
                                  vertex.position.x, vertex.position.y, vertex.position.z,
                                  vertex.color[0] / 255.0f, vertex.color[1] / 255.0f,
                                  vertex.color[2] / 255.0f));
    }
  }

  for (const Triangle& triangle : triangles)
  {
    for (const Vertex& vertex : triangle.vertices)
    {
      obj.WriteString(
          fmt::format("vn {:.9g} {:.9g} {:.9g}\n", vertex.normal.x, vertex.normal.y,
                      vertex.normal.z));
    }
  }

  for (const Triangle& triangle : triangles)
  {
    for (const Vertex& vertex : triangle.vertices)
      obj.WriteString(fmt::format("vt {:.9g} {:.9g}\n", vertex.texcoord.x, 1.0f - vertex.texcoord.y));
  }

  for (std::size_t triangle_index = 0; triangle_index < triangles.size(); ++triangle_index)
  {
    if (triangle_index == 0 ||
        triangles[triangle_index].material_index != triangles[triangle_index - 1].material_index)
    {
      const u32 material_index = triangles[triangle_index].material_index;
      if (material_index < materials.size())
        obj.WriteString(fmt::format("usemtl {}\n", materials[material_index].name));
      else
        obj.WriteString("usemtl vertex_color\n");
    }

    const std::size_t first = triangle_index * 3 + 1;
    obj.WriteString(fmt::format("f {0}/{0}/{0} {1}/{1}/{1} {2}/{2}/{2}\n", first, first + 1,
                                first + 2));
  }

  mtl.WriteString("# Dolphin 3D screenshot material\n");
  for (const Material& material : materials)
  {
    mtl.WriteString(fmt::format("newmtl {}\n", material.name));
    mtl.WriteString("Ka 1.000000 1.000000 1.000000\n");
    mtl.WriteString("Kd 1.000000 1.000000 1.000000\n");
    mtl.WriteString("Ks 0.000000 0.000000 0.000000\n");
    mtl.WriteString("d 1.000000\n");
    mtl.WriteString("illum 1\n");
    if (!material.texture_file_name.empty())
      mtl.WriteString(fmt::format("map_Kd {}\n", material.texture_file_name));
    mtl.WriteString("\n");
  }

  return true;
}
}  // namespace

void Request(std::string path)
{
  std::lock_guard lock(s_mutex);
  s_path = std::move(path);
  s_triangles.clear();
  s_materials.clear();
  s_material_indices.clear();
  s_materials.push_back({"vertex_color", ""});
  s_empty_end_frames = 0;
  s_requested.store(true, std::memory_order_release);
}

bool IsRequested()
{
  return s_requested.load(std::memory_order_acquire);
}

u32 AddTextureMaterial(const AbstractTexture& texture, std::string texture_name)
{
  if (!IsRequested())
    return VERTEX_COLOR_MATERIAL;

  std::lock_guard lock(s_mutex);
  if (s_path.empty())
    return VERTEX_COLOR_MATERIAL;

  texture_name = SanitizeFileName(std::move(texture_name));
  const auto existing = s_material_indices.find(texture_name);
  if (existing != s_material_indices.end())
    return existing->second;

  const u32 material_index = static_cast<u32>(s_materials.size());
  const std::string material_name = fmt::format("tex_{:04}", material_index);
  const std::string texture_file_name = fmt::format("{}_{}.png", material_name, texture_name);
  const std::string texture_path = GetDirectory(s_path) + texture_file_name;

  if (!File::Exists(texture_path))
    texture.Save(texture_path, 0, Config::Get(Config::GFX_TEXTURE_PNG_COMPRESSION_LEVEL));

  s_material_indices.emplace(texture_name, material_index);
  s_materials.push_back({material_name, texture_file_name});
  return material_index;
}

void AddTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, u32 material_index)
{
  if (!IsRequested())
    return;

  std::lock_guard lock(s_mutex);
  if (material_index >= s_materials.size())
    material_index = VERTEX_COLOR_MATERIAL;
  s_triangles.push_back({{v0, v1, v2}, material_index});
  s_empty_end_frames = 0;
}

void EndFrame()
{
  if (!IsRequested())
    return;

  std::string path;
  std::vector<Triangle> triangles;
  std::vector<Material> materials;
  {
    std::lock_guard lock(s_mutex);
    if (s_triangles.empty())
    {
      ++s_empty_end_frames;
      if (s_empty_end_frames < EMPTY_END_FRAME_LIMIT)
        return;

      s_path.clear();
      s_requested.store(false, std::memory_order_release);
      s_empty_end_frames = 0;
      OSD::AddMessage("3D screenshot failed: no geometry was captured");
      return;
    }

    path = s_path;
    triangles = std::move(s_triangles);
    materials = std::move(s_materials);
    s_triangles.clear();
    s_materials.clear();
    s_material_indices.clear();
    s_path.clear();
    s_requested.store(false, std::memory_order_release);
    s_empty_end_frames = 0;
  }

  const std::string mtl_path = GetMaterialPath(path);
  if (WriteCapture(path, mtl_path, triangles, materials))
  {
    OSD::AddMessage(
        fmt::format("3D screenshot saved to {} ({} triangles)", path, triangles.size()));
  }
  else
  {
    OSD::AddMessage("3D screenshot failed: could not write OBJ/MTL files");
  }
}
}  // namespace ThreeDScreenshot

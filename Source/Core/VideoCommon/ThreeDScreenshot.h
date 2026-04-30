// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

class AbstractTexture;

namespace ThreeDScreenshot
{
struct Vertex
{
  Common::Vec3 position;
  Common::Vec3 normal;
  Common::Vec2 texcoord;
  std::array<u8, 4> color;
};

void Request(std::string path);
bool IsRequested();
u32 AddTextureMaterial(const AbstractTexture& texture, std::string texture_name);
void AddTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, u32 material_index = 0);
void EndFrame();
}  // namespace ThreeDScreenshot

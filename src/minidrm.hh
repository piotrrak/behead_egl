/*
 * SPDX-FileCopyrightText: 2020 Piotr Rak <piotr.rak@gamil.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "ufd.hh"

namespace behead_egl::internal {

enum class DrmNodeFlag : unsigned
{
   None    = 0,
   Primary = 1 << 0,
   Render  = 1 << 1,
// NB: We don't use that one for now control nodes
};

constexpr inline DrmNodeFlag
operator| (DrmNodeFlag e1, DrmNodeFlag e2) noexcept
{
   return DrmNodeFlag(unsigned(e1) | unsigned(e2));
}

constexpr inline DrmNodeFlag
operator& (DrmNodeFlag e1, DrmNodeFlag e2) noexcept
{
   return DrmNodeFlag(unsigned(e1) & unsigned(e2));
}

constexpr inline bool has(DrmNodeFlag e1, DrmNodeFlag e2) noexcept
{
   return (e1 & e2) == e2;
}

template <DrmNodeFlag e2>
constexpr inline bool has(DrmNodeFlag e1) noexcept
{
   return (e1 & e2) == e2;
}

inline constexpr DrmNodeFlag BothDrmNodes = DrmNodeFlag::Primary | DrmNodeFlag::Render;

const char *to_string(DrmNodeFlag);

struct DrmNodeFds
{
   unique_fd render_fd;
   unique_fd primary_fd;

   bool ok() const { return render_fd.ok() && primary_fd.ok(); }
};

// Open master and render node device file_descriptors
DrmNodeFds open_drm_nodes(const char *dev, DrmNodeFlag nodes = BothDrmNodes);

} // namespace behead_egl::internal

/*
 * SPDX-FileCopyrightText: 2020 Piotr Rak <piotr.rak@gamil.com>
 * SPDX-License-Identifier: MIT
 */
#include "ufd.hh"

namespace behead_egl::internal {

// Instantiate with posix_closer
template class basic_unique_fd<posix_closer>;

} // behead_egl::internal

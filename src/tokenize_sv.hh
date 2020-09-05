/*
 * SPDX-FileCopyrightText: 2020 Piotr Rak <piotr.rak@gamil.com>
 * SPDX-License-Identifier: MIT OR WTFPL
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace behead_egl::_impl {

// Helpers for iteration

// For all callback
//  - call it with argumens
//  - return false
template <bool BoolRet_> struct _quit_iter_ret final
{
   template <typename CbTy_, typename... ValTys_>
   static bool invoke(CbTy_&& cb, ValTys_... vals)
   {
      cb(std::forward<ValTys_>(vals)...);

      return false;
   }
};

// For decltype(cb) bool
//  - call it
//  - return its result
template <>
struct _quit_iter_ret<true> final
{
   template <typename CbTy_, typename... ValTys_>
   static bool invoke(CbTy_&& cb, ValTys_... vals)
   {
      return cb(std::forward<ValTys_>(vals)...);
   }
};

} // namespace bhd::_impl

namespace behead_egl::internal {

// For each token in string sv separated by delimiter delim calls cb(token);
// iff cb return type is bool, iteration will stop once cb returns true
//
// Returns number of tokens.
template <typename ChTy_, typename ChTraitsTy_, typename FnTy_> std::size_t
foreach_token_sv(std::basic_string_view<ChTy_, ChTraitsTy_> sv, ChTy_ delim, FnTy_ cb) noexcept
{
   // Is return type of cb bool?
   constexpr bool IS_BOOL_RET = std::is_same_v<decltype(cb(sv)), bool>;

   // Helper for invoking cb with bool or void return type.
   using quit_p = _impl::_quit_iter_ret<IS_BOOL_RET>;

   auto rest = sv;

   std::size_t token_count = 0;

   do {
      // Find position of not delimiter at begin rest of string
      std::size_t delim_to_trim =
         std::min(rest.find_first_not_of(delim), rest.size());

      // and remove it if needed
      rest.remove_prefix(delim_to_trim);

      // Handle one of three possible cases:
      // - <token> <delimiter>
      // - <token> <end of string>
      // - <end of string>
      std::size_t token_end = std::min(rest.find_first_of(delim), rest.size());

      // get token or empty string
      auto token = rest.substr(0, token_end);

      if (!token.empty())
      {
         ++token_count;
         if (quit_p::invoke(cb, token))
            break;

         rest.remove_prefix(token_end);
      }

      // And repeat until rest not empty

   } while (!rest.empty());

   return token_count;
}

} // namespace behead_egl::internal

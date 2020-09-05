/*
 * SPDX-FileCopyrightText: 2020 Piotr Rak <piotr.rak@gamil.com>
 * SPDX-License-Identifier: MIT OR WTFPL
 */

#pragma once

#include <utility>

#include <errno.h>
#include <unistd.h>

namespace behead_egl::internal {

template <typename CloserTy_>
class basic_unique_fd final
{
   constexpr static int INVALID_FD = -1;

   constexpr static bool if_closer_noexcept = noexcept(std::declval<CloserTy_>()(int()));

public:
   basic_unique_fd() noexcept = default;

   explicit basic_unique_fd(int fd) noexcept(if_closer_noexcept)
   {
      this->reset(fd);
   }

   ~basic_unique_fd()
   {
      this->reset();
   }

   basic_unique_fd(basic_unique_fd&& other) noexcept(if_closer_noexcept)
   {
      this->reset(other.release());
   }

   basic_unique_fd(const basic_unique_fd&) = delete;

   void operator=(const basic_unique_fd&) = delete;

   ///{{{ Observers

   int get() const noexcept
   { return _fd; }

   explicit operator int() const noexcept
   { return get(); }

   bool operator>=(int rhs) const noexcept
   { return get() >= rhs; }

   bool operator<(int rhs) const noexcept
   { return get() < rhs; }

   bool operator==(int rhs) const noexcept
   { return get() == rhs; }

   bool operator!=(int rhs) const noexcept
   { return get() != rhs; }

   bool operator==(const basic_unique_fd& rhs) const noexcept
   { return get() == rhs.get(); }

   bool operator!=(const basic_unique_fd& rhs) const noexcept
   { return get() != rhs.get(); }

   bool operator!() const = delete;

   bool ok() const noexcept
   { return get() > INVALID_FD; }

   /// }}}

   /// {{{ Mutators
   basic_unique_fd& operator=(basic_unique_fd&& other) noexcept(if_closer_noexcept)
   {
      int fd = other._fd;
      other._fd = INVALID_FD;

      this->reset(fd);

      return *this;
   }

   void reset(int fd = INVALID_FD) noexcept(if_closer_noexcept)
   {
      if (this->ok())
         this->close(_fd);

      _fd = fd;
   }

   [[nodiscard]] int release() noexcept
   {
      int ret = _fd;
      _fd = INVALID_FD;
      return ret;
   }

   /// }}}

private:
   int _fd = INVALID_FD;

   template <typename Ty_ = CloserTy_>
   static auto close(int fd) noexcept(if_closer_noexcept) ->
      decltype(std::declval<Ty_>()(fd), void())
   {
      Ty_ c;
      c(fd);
   }

};

struct posix_closer final
{
   int operator() (int fd) noexcept
   {
      int previous_errno = errno;
      int ret = ::close(fd);
      errno = previous_errno;
      return ret;
   }
};

using unique_fd = basic_unique_fd<posix_closer>;

extern template class basic_unique_fd<posix_closer>;

} // namespace behead_egl::internal


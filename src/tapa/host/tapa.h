#ifndef TAPA_HOST_TAPA_H_
#define TAPA_HOST_TAPA_H_

#include "tapa/base/tapa.h"

#include "tapa/host/coroutine.h"
#include "tapa/host/mmap.h"
#include "tapa/host/stream.h"
#include "tapa/host/task.h"
#include "tapa/host/traits.h"
#include "tapa/host/util.h"
#include "tapa/host/vec.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <frt.h>
#include <glog/logging.h>
#include <sys/types.h>

namespace tapa {

namespace internal {

struct seq {
  int pos = 0;
};

template <typename Param, typename Arg>
struct accessor {
  static Param access(Arg&& arg) { return arg; }
  static void access(fpga::Instance& instance, int& idx, Arg&& arg) {
    instance.SetArg(idx++, static_cast<Param>(arg));
  }
};

template <typename T>
struct accessor<T, seq> {
  static T access(seq&& arg) { return arg.pos++; }
  static void access(fpga::Instance& instance, int& idx, seq&& arg) {
    instance.SetArg(idx++, static_cast<T>(arg.pos++));
  }
};

}  // namespace internal

// Host-only invoke that takes path to a bistream file as an argument. Returns
// the kernel time in nanoseconds.
template <typename Func, typename... Args>
inline int64_t invoke(Func&& f, const std::string& bitstream, Args&&... args) {
  return internal::invoker<Func>::template invoke<Args...>(
      /*run_in_new_process*/ false, std::forward<Func>(f), bitstream,
      std::forward<Args>(args)...);
}

// Workaround for the fact that Xilinx's cosim cannot run for more than once in
// each process. The mmap pointers MUST be allocated via mmap, or the updates
// won't be seen by the caller process!
template <typename Func, typename... Args>
inline int64_t invoke_in_new_process(Func&& f, const std::string& bitstream,
                                     Args&&... args) {
  return internal::invoker<Func>::template invoke<Args...>(
      /*run_in_new_process*/ true, std::forward<Func>(f), bitstream,
      std::forward<Args>(args)...);
}

template <typename T>
struct aligned_allocator {
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  template <typename U>
  void construct(U* ptr) {
    ::new (static_cast<void*>(ptr)) U;
  }
  template <class U, class... Args>
  void construct(U* ptr, Args&&... args) {
    ::new (static_cast<void*>(ptr)) U(std::forward<Args>(args)...);
  }
  T* allocate(size_t count) {
    return reinterpret_cast<T*>(internal::allocate(count * sizeof(T)));
  }
  void deallocate(T* ptr, std::size_t count) {
    internal::deallocate(ptr, count * sizeof(T));
  }
};

}  // namespace tapa

#endif  // TAPA_HOST_TAPA_H_

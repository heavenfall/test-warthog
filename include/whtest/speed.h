#ifndef WHTEST_SPEED_H
#define WHTEST_SPEED_H

#include <warthog/util/timer.h>

#include <memory>
#include <random>
#include <cstddef>
#include <cstdint>

namespace whtest
{

// 2 * 4MB
constexpr size_t CACHE_SIZE = 4ull * 1024 * 1024;

using timer = ::warthog::util::timer;

class speed_manager
{
	std::unique_ptr<volatile uint16_t[]> cache_destoryer;
	std::minstd_rand rng;

public:
	speed_manager(std::size_t seed = 0);

	void clear_cache(int passes = 4);

	auto& rand() noexcept { return rng; }
};

} // namespace whtest

#endif // WHTEST_SPEED_H

#include <whtest/speed.h>

namespace whtest
{

speed_manager::speed_manager(std::size_t seed)
{
	cache_destoryer = std::make_unique<volatile uint16_t[]>(CACHE_SIZE);
	if (seed != 0) {
		std::seed_seq seq{seed};
		rng.seed(seq);
	} else {
		std::random_device rd;
		std::seed_seq seq{rd()};
		rng.seed(seq);
	}
}

void speed_manager::clear_cache(int passes)
{
	while (passes-- > 0) {
		volatile uint16_t* c = cache_destoryer.get();
		for (size_t i = 0; i < CACHE_SIZE; ++i) {
			c[i] = static_cast<uint16_t>(rng());
		}
		volatile uint16_t* m = cache_destoryer.get();
		for (size_t i = 0; i < CACHE_SIZE-1; ++i) {
			m[i+1] += m[i];
		}
	}
}

} // namespace whtest

#include <whtest/speed.h>
#include <warthog/data/hash_table.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>
#include <iostream>

namespace whtest
{

struct gen_ops
{
	int success = 0;
	int unsuccess = 0;
};

template <typename HT, typename R>
gen_ops hashtable_insert(HT& table, R&& range)
{
	gen_ops ops;
	for (auto& kv : range)
	{
		if (table.try_emplace(std::get<0>(kv), std::get<1>(kv)).second)
			ops.success++;
		else
			ops.unsuccess++;
	}
	return ops;
};

template <typename K, typename V, typename RNG>
std::vector<std::pair<K,V>> gen_keyvalue(RNG& rng, int amount, bool unique_key = true)
{
	std::unordered_set<K> keys;
	std::vector<std::pair<K,V>> keys_o;
	while (keys_o.size() < amount) {
		bool found = false;
		for (int i = 0; i < 32; ++i) {
			K k = static_cast<K>(rng());
			if (!unique_key || !keys.contains(k)) {
				keys.insert(k);
				keys_o.push_back({k,0});
				found = true;
				break;
			}
		}
		if (!found) {
			// insert in order until full
			for (K k = 0; keys_o.size() < amount; ++k) {
				if (!keys.contains(k)) {
					keys.insert(k);
					keys_o.push_back({k,0});
				}
			}
		}
	}

	std::shuffle(keys_o.begin(), keys_o.end(), rng);
	for (auto& kv : keys_o) {
		kv.second = static_cast<V>(rng());
	}

	return keys_o;
}

} // namespace whtest

int main()
{
	using K = uint32_t;
	using V = int32_t;

	whtest::speed_manager mgr;
	auto ins_values = whtest::gen_keyvalue<K, V>(mgr.rand(), 1'000'000);
	warthog::util::timer T;

	std::unordered_map<K, V> M(1ull * 1024 * 1024 * 2);

	mgr.clear_cache();
	T.start();
	whtest::hashtable_insert(M, ins_values);
	auto res_time = T.elapsed_time_nano();

	std::cout << res_time.count() << "ns\n";

	warthog::data::hash_table<K, V> M2;
	M2.factory_setup();
	M2.rehash(1ull * 1024 * 1024 * 2);

	mgr.clear_cache();
	T.start();
	whtest::hashtable_insert(M2, ins_values);
	auto res_time2 = T.elapsed_time_nano();

	std::cout << res_time2.count() << "ns\n";

	return 0;
}

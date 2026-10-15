#include <whtest/speed.h>
#include <warthog/data/hash_table.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>
#include <iostream>
#include <string>
#include <typeindex>
#include <format>
#include <memory_resource>

namespace whtest
{

namespace {

std::unordered_map<std::type_index, std::string> type_alias;
template <typename T>
std::string_view type_alias_get(std::string_view alt = std::string_view())
{
	if (auto it = type_alias.find(typeid(T)); it != type_alias.end()) {
		return it->second;
	} else if (!alt.empty()) {
		return alt;
	} else {
		return typeid(T).name();
	}
}
template <typename T>
void type_alias_set(std::string_view alias)
{
	type_alias[typeid(T)] = alias;
}

}

struct gen_ops
{
	int success = 0;
	int unsuccess = 0;

	operator bool() const noexcept
	{
		return unsuccess == 0;
	}
};

template <typename HT, typename R>
gen_ops hashtable_insert(HT& T, R&& range)
{
	gen_ops ops;
	for (auto& kv : range)
	{
		if (T.table.try_emplace(std::get<0>(kv), std::get<1>(kv)).second)
			ops.success++;
		else
			ops.unsuccess++;
	}
	return ops;
};

template <typename HT, typename R>
gen_ops hashtable_check(HT& T, R&& range)
{
	gen_ops ops;
	for (auto& kv : range)
	{
		if (T.table.at(std::get<0>(kv)) == std::get<1>(kv))
			ops.success++;
		else
			ops.unsuccess++;
	}
	return ops;
};

template <typename K, typename V>
std::vector<std::pair<K,V>> gen_keys_random(speed_manager& sm, int amount, bool unique_key = true)
{
	std::unordered_set<K> keys;
	std::vector<std::pair<K,V>> keys_o;
	auto& rng = sm.rand();
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

	return keys_o;
}

template <typename K, typename V>
std::vector<std::pair<K,V>> gen_keys_iota([[maybe_unused]] speed_manager& sm, int amount, int step = 1)
{
	if (amount < 0 || step < 1)
		throw std::out_of_range("amount|step");

	std::vector<std::pair<K,V>> keys_o(amount);
	for (int i = 0; i < amount; ++i) {
		keys_o[i].first = i;
	}

	std::shuffle(keys_o.begin(), keys_o.end(), sm.rand());

	return keys_o;
}

template <typename K, typename V>
void fill_values_rng(speed_manager& sm, std::vector<std::pair<K,V>>& values)
{
	auto& rng = sm.rand();
	for (auto& kv : values) {
		kv.second = static_cast<V>(rng());
	}
}

template <typename Key, typename Value, typename Hash, typename HT = std::unordered_map<Key, Value, Hash>, typename PMR = void>
class std_hash_table
{
public:
	static consteval bool is_pmr() noexcept { return !std::same_as<PMR,void>; }
	std::conditional_t<is_pmr(), PMR, int> pmr_res;
	HT table;
	using key = Key;
	using value = Value;
	using hash = Hash;

	std_hash_table(size_t buckets) requires(!is_pmr()) : table(buckets)
	{ }
	std_hash_table(size_t buckets) requires(is_pmr()) : table(buckets, &pmr_res)
	{ }
	std_hash_table(size_t buckets, auto pmrv) requires(is_pmr()) : pmr_res(pmrv), table(buckets, &pmr_res)
	{ }

	static std::string name()
	{
		using namespace std::string_view_literals;
		std::string pmr_name;
		if constexpr (is_pmr()) {
			pmr_name = std::string("-");
			pmr_name += type_alias_get<PMR>();
		}
		return std::format("{}-{}{}", (std::same_as<PMR,void> ? "std" : "pmr"), type_alias_get<Hash>("#"sv), pmr_name);
	}
};

template <typename Key, typename Value, typename Hash, typename Lookup, size_t BucketSize, size_t Rolling = 0, typename HT = warthog::data::hash_table<Key, Value, Hash, BucketSize, Lookup, Rolling>>
class wh_hash_table
{
public:
	HT table;
	using key = Key;
	using value = Value;

	wh_hash_table(size_t buckets)
	{
		table.factory_setup();
		table.rehash(buckets);
	}

	static std::string name()
	{
		using namespace std::string_view_literals;
		return std::format("wh{}-{}_{}|{}", type_alias_get<Lookup>("%"sv), type_alias_get<Hash>("#"sv), BucketSize, Rolling);
	}
};

struct hash_test_suite
{
	using K = uint32_t;
	using V = int32_t;
	using KV = std::pair<K, V>;
	using VKV = std::vector<KV>;

	std::string name;
	VKV values_rand;
	VKV values_inc;

	hash_test_suite(std::string l_name) : name(std::move(l_name))
	{ }

	void setup(whtest::speed_manager& sm, size_t size)
	{
		values_rand = whtest::gen_keys_random<K, V>(sm, size);
		whtest::fill_values_rng(sm, values_rand);
		values_inc = whtest::gen_keys_iota<K, V>(sm, size, 8);
		whtest::fill_values_rng(sm, values_inc);
	}

	template <typename HT>
	void test(whtest::speed_manager& sm, HT& T, std::ostream& out)
	{
		warthog::util::timer timer;
		std::chrono::nanoseconds rel;

		sm.clear_cache();
		rel = test_insert(T, values_rand);
		print_result(out, "insert", T, rel);

		sm.clear_cache();
		rel = test_check(T, values_rand);
		print_result(out, "check", T, rel);

		sm.clear_cache();
		rel = test_clear(T);
		print_result(out, "clear", T, rel);
		
		sm.clear_cache();
		rel = test_insert(T, values_inc);
		print_result(out, "insert-8step", T, rel);
		
		sm.clear_cache();
		rel = test_check(T, values_inc);
		print_result(out, "check-8step", T, rel);
	}
	
	void print_result(std::ostream& out, std::string_view test_name, auto& T, std::chrono::nanoseconds rel) const
	{
		out << std::format("{}\t{}\t{}\t{}\t{}\n",
			name, T.name(), test_name,
			rel.count(), static_cast<long double>(rel.count()) * 1e-9);
	}

	std::chrono::nanoseconds test_insert(auto& T, auto& values)
	{
		warthog::util::timer timer;

		timer.start();
		bool success = whtest::hashtable_insert(T, values);
		auto res_time = timer.elapsed_time_nano();
		if (!success)
			std::cerr << "fail " << T.name() << std::endl;

		return res_time;
	}

	std::chrono::nanoseconds test_check(auto& T, const auto& values)
	{
		warthog::util::timer timer;

		timer.start();
		bool success = whtest::hashtable_check(T, values);
		auto res_time = timer.elapsed_time_nano();
		if (!success)
			std::cerr << "fail " << T.name() << std::endl;

		return res_time;
	}
	
	std::chrono::nanoseconds test_clear(auto& T)
	{
		warthog::util::timer timer;

		timer.start();
		T.table.clear();
		auto res_time = timer.elapsed_time_nano();

		return res_time;
	}
};

} // namespace whtest

int main()
{
	using K = uint32_t;
	using V = int32_t;

	using namespace whtest;
	using namespace std::string_view_literals;

	type_alias_set<std::pmr::monotonic_buffer_resource>("mono"sv);
	type_alias_set<std::pmr::unsynchronized_pool_resource>("usync"sv);

	type_alias_set<warthog::data::bucket_lookup_pow2>("^"sv);
	type_alias_set<warthog::data::bucket_lookup_prime>("%"sv);

	type_alias_set<warthog::data::aes_hash_default>("aes"sv);

	speed_manager sm;
	hash_test_suite ts("S-4M");
	ts.setup(sm, 4'000'000);

	constexpr size_t S = 1ull * 1024 * 1024 * 8;
	constexpr size_t Swh = S / 2;

	std_hash_table<K,V,std::hash<K>>
	M1(S);
	ts.test(sm, M1, std::cout);

	std_hash_table<K,V,std::hash<K>,std::pmr::unordered_map<K,V>,std::pmr::unsynchronized_pool_resource>
	M2(S);
	ts.test(sm, M2, std::cout);

	std_hash_table<K,V,std::hash<K>,std::pmr::unordered_map<K,V>,std::pmr::monotonic_buffer_resource>
	M3(S,4*1024);
	ts.test(sm, M3, std::cout);

	wh_hash_table<K,V,warthog::data::identity_hash, warthog::data::bucket_lookup_pow2, 4, 0>
	W1(Swh);
	ts.test(sm, W1, std::cout);

	wh_hash_table<K,V,warthog::data::identity_hash, warthog::data::bucket_lookup_prime, 4, 0>
	W2(Swh);
	ts.test(sm, W2, std::cout);
	
	wh_hash_table<K,V,warthog::data::aes_hash_default, warthog::data::bucket_lookup_pow2, 4, 0>
	W3(Swh);
	ts.test(sm, W3, std::cout);
	
	wh_hash_table<K,V,warthog::data::aes_hash_default, warthog::data::bucket_lookup_pow2, 6, 0>
	W4(Swh);
	ts.test(sm, W4, std::cout);
	
	// wh_hash_table<K,V,warthog::data::aes_hash_default, warthog::data::bucket_lookup_pow2, 16, 0>
	// W5(Swh);
	// ts.test(sm, W5, std::cout);
	
	wh_hash_table<K,V,warthog::data::aes_hash_default, warthog::data::bucket_lookup_pow2, 2, 0>
	W6(Swh);
	ts.test(sm, W6, std::cout);
	
	// wh_hash_table<K,V,warthog::data::aes_hash_default, warthog::data::bucket_lookup_pow2, 1, 0>
	// W7(Swh);
	// ts.test(sm, W7, std::cout);
	
	// wh_hash_table<K,V,warthog::data::aes_hash_default, warthog::data::bucket_lookup_pow2, 2, 4>
	// W8(Swh);
	// ts.test(sm, W8, std::cout);

	return 0;
}

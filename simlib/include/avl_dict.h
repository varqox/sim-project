#pragma once

#include "debug.h"

#include <vector>

template<class size_type>
struct AVLNodeBase {
	static constexpr int L = 0; // left
	static constexpr int R = 1; // right

	std::array<size_type, 2> kid;
	uint8_t h;

	AVLNodeBase(size_type left_kid, size_type right_kid, uint8_t height)
		: kid{{left_kid, right_kid}}, h{height} {}
};

/// To use within AVLDictionary only
template<class T, class size_type = size_t>
class AVLPoolAllocator {
	using AVLNB = AVLNodeBase<size_type>;
	static_assert(std::is_base_of<AVLNB, T>::value,
		"T has to derive from AVLNodeBase");

	using Elem = typename std::aligned_storage<
		meta::max(sizeof(T), sizeof(size_type)),
		meta::max(alignof(T), alignof(size_type))>::type;

	std::unique_ptr<Elem[]> data;
	size_type capacity_ = 0;
	size_type head = 0;

	T& elem(size_type i) { return reinterpret_cast<T&>(data[i]); }

	const T& elem(size_type i) const { return reinterpret_cast<T&>(data[i]); }

	template<class Array>
	static size_type& ptr(Array& arr, size_type i) {
		return reinterpret_cast<size_type&>(arr[i]);
	}

	size_type& ptr(size_type i) { return ptr(data, i); }

	const size_type& ptr(size_type i) const { return ptr(data, i); }

public:
	struct AssertAllIsDeallocated {};

	AVLPoolAllocator() : data {new Elem[1]}, capacity_ {1} { ptr(0) = 1; }

	AVLPoolAllocator(const AVLPoolAllocator&) = delete;
	AVLPoolAllocator& operator=(const AVLPoolAllocator&) = delete;

	// After moving out @p apa should be reinitialized (e.g. apa = {}), before
	// using it in any way other than destructing
	AVLPoolAllocator(AVLPoolAllocator&& apa) noexcept
		: data(std::move(apa.data)), capacity_(apa.capacity_), head(apa.head)
	{
		apa.capacity_ = 0;
		// apa.head = 0; // It is not necessary to set head
	}

	// After moving out @p apa should be reinitialized (e.g. apa = {}), before
	// using it in any way other than destructing
	AVLPoolAllocator& move_assign(AVLPoolAllocator&& apa,
		AssertAllIsDeallocated) noexcept
	{
		// As there is nothing to destruct we can proceed to moving the data
		data = std::move(apa.data);
		capacity_ = apa.capacity_;
		head = apa.head;

		apa.capacity_ = 0;
		// apa.head = 0; // It is not necessary to set head
		return *this;
	}

	// After moving out @p apa should be reinitialized (e.g. apa = {}), before
	// using it in any way other than destructing
	AVLPoolAllocator& operator=(AVLPoolAllocator&& apa) {
		if (capacity() > 0) {
			// Destruct all previously held data
			// Mark allocated elements
			std::vector<bool> allocated(capacity_, true);
			for (size_type i = head; i < capacity(); i = ptr(i))
				allocated[i] = false;
			// Destruct them
			if (allocated[0]) // 0 is a special case
				reinterpret_cast<AVLNB&>(data[0]).~AVLNB();
			for (size_type i = 1; i < capacity(); ++i)
				if (allocated[i])
					elem(i).~T();
		}

		// All has been destructed, so we can proceed to moving the data
		return move_assign(apa, AssertAllIsDeallocated{});
	}

	explicit AVLPoolAllocator(size_type reverve_size)
		: data {new Elem[meta::max(1, reverve_size)]},
		capacity_ {meta::max(1, reverve_size)}
	{
		for (size_type i = 0; i < capacity(); ++i)
			ptr(i) = i + 1;
	}

	T& operator[](size_type n) noexcept { return elem(n); }

	const T& operator[](size_type n) const noexcept { return elem(n); }

	size_type capacity() const noexcept { return capacity_; }

	static constexpr size_type max_capacity() noexcept {
		return std::numeric_limits<size_type>::max();
	}

	void reserve_for(size_type n) {
		if (n < capacity())
			return;

		if (capacity() == max_capacity())
			THROW("The AVLpool is full");

		size_type new_capacity = meta::max(n, capacity() < (max_capacity() >> 1)
			? capacity() << 1 : max_capacity());
		std::unique_ptr<Elem[]> new_data {new Elem[new_capacity]};
		// Initialize new data
		for (size_type i = 0; i < capacity(); ++i)
			ptr(new_data, i) = new_capacity;
		for (size_type i = capacity(); i < new_capacity; ++i)
			ptr(new_data, i) = i + 1;

		// Move and mark unallocated cells (allocated cells will still have
		// new_capacity as value)
		for (size_type i = head; i < capacity(); i = ptr(i))
			ptr(new_data, i) = ptr(i);

		// Find allocated nodes and move them to the new_data
		// 0 element has to be moved in a different way
		if (ptr(new_data, 0) == new_capacity)
			new (&new_data[0])
				AVLNB{std::move(reinterpret_cast<AVLNB&>(data[0]))};

		for (size_type i = 1; i < capacity(); ++i)
			if (ptr(new_data, i) == new_capacity) {
				new (&new_data[i]) T{std::move(elem(i))};
				elem(i).~T();
			}

		data.reset(new_data.release());
		capacity_ = new_capacity;
	}

	size_type allocate() {
		if (head == capacity()) {
			if (capacity() == max_capacity())
				THROW("The AVLpool is full");

			size_type new_capacity = (capacity() < (max_capacity() >> 1) ?
				capacity() << 1 : max_capacity());
			std::unique_ptr<Elem[]> new_data {new Elem[new_capacity]};
			// Initialize new data (move old and initialize new cells)
			// 0 element has to be moved in a different way
			new (&new_data[0])
				AVLNB{std::move(reinterpret_cast<AVLNB&>(data[0]))};
			for (size_type i = 1; i < capacity(); ++i) {
				new (&new_data[i]) T{std::move(elem(i))};
				elem(i).~T();
			}

			for (size_type i = capacity(); i < new_capacity; ++i)
				ptr(new_data, i) = i + 1;

			data.reset(new_data.release());
			capacity_ = new_capacity;
		}

		size_type res = head;
		head = ptr(head);
		return res;
	}

	void deallocate(size_type n) noexcept {
		ptr(n) = head;
		head = n;
	}

	void destruct_and_deallocate(size_type n) noexcept {
		elem(n).~T();
		ptr(n) = head;
		head = n;
	}

	// Warning: works in O(capacity)
	void deallocate_all() noexcept {
		for (size_type i = 0; i < capacity(); ++i)
			ptr(i) = i + 1;
		head = 0;
	}

#if 0
	void cerr_info() const noexcept {
		for (size_type i = 0; i < capacity(); ++i)
			cerr << i << ": " << ptr(i) << endl;
	}
#endif

	~AVLPoolAllocator() {}  // It is user duty to destruct all the held objects
};

template<template<class...> class NodeT, class Comp = std::less<>,
	class size_type = size_t, class... NodeArgs>
class AVLDictionary {
protected:
	using Node = NodeT<size_type, NodeArgs...>;

	static_assert(std::is_base_of<AVLNodeBase<size_type>, Node>::value,
		"NodeT has to derive from AVLNodeBase");

	static constexpr int L = Node::L;
	static constexpr int R = Node::R;

	AVLPoolAllocator<Node, size_type> pool;

	const size_type nil = pool.allocate();
	size_type root = nil;
	size_type size_ = 0;
	Comp compare;

public:
	AVLDictionary(size_type reserve_n = 1, Comp cmp = {})
		: pool {reserve_n}, compare {std::move(cmp)}
	{
		throw_assert(nil == 0);
		pool[nil].kid[L] = nil;
		pool[nil].kid[R] = nil;
		pool[nil].h = 0;
	}

	AVLDictionary(Comp cmp) : AVLDictionary {1, std::move(cmp)} {}

	AVLDictionary(const AVLDictionary&) = delete;
	AVLDictionary& operator=(const AVLDictionary&) = delete;

	// After moving out @p avld should be reinitialized (e.g. avld = {}), before
	// using it in any way other than destructing
	AVLDictionary(AVLDictionary&& avld) noexcept
		: pool {std::move(avld.pool)}, nil {avld.nil}, root {avld.root},
			size_ {avld.size_}, compare {std::move(avld.compare)}
	{
		// Set avld to a clear state
		avld.root = nil;
	}

	// After moving out @p avld should be reinitialized (e.g. avld = {}), before
	// using it in any way other than destructing
	AVLDictionary& operator=(AVLDictionary&& avld) noexcept {
		delete_subtree(root);

		pool.move_assign(std::move(avld.pool),
			typename decltype(pool)::AssertAllIsDeallocated{});
		// nil = avld.nil; (nil is always equal to 0)
		root = avld.root;
		size_ = avld.size_;
		compare = std::move(avld.compare);

		// Set avld to a clear state
		avld.root = nil;
		return *this;
	}

	void deallocate_node(size_type x) noexcept {
		pool.destruct_and_deallocate(x);
		--size_;
	}

	template<class... Args>
	size_type allocate_node(Args&&... args) {
		size_t x = pool.allocate();

		try {
			new (&pool[x]) Node{std::forward<Args>(args)...};
		} catch (...) {

			pool.deallocate(x);
			throw;
		}

		++size_;
		return x;
	}


#if __cplusplus > 201402L
#warning "Since C++17 constexpr if is a better option below"
#endif
	template<class U = Node>
	std::enable_if_t<std::is_trivially_destructible<U>::value, void>
		delete_subtree(size_type) noexcept {}

	template<class U = Node>
	std::enable_if_t<!std::is_trivially_destructible<U>::value, void>
		delete_subtree(size_type x) noexcept
	{
		if (x == nil)
			return;

		delete_subtree(pool[x].kid[L]);
		delete_subtree(pool[x].kid[R]);

		deallocate_node(x);
	}

	~AVLDictionary() {
		delete_subtree(root);

		static_assert(
			std::is_trivially_destructible<AVLNodeBase<size_type>>::value,
			"Needed to avoid destructing nil and simplify move assignments");
	}

	void clear() {
		delete_subtree(root);
		root = nil;
	}

	static constexpr size_type max_capacity() noexcept {
		return decltype(pool)::max_capacity();
	}

	size_type capacity() const noexcept { return pool.capacity(); }

	void reserve_for(size_type n) { pool.reserve_for(n); }

	size_type size() const noexcept { return size_; }

	bool empty() const noexcept { return (size() == 0); }

	void seth(size_type x) noexcept {
		pool[x].h = uint8_t(1 +
			meta::max(pool[pool[x].kid[L]].h, pool[pool[x].kid[R]].h));
	}

	/// rotating nil node up is invalid
	size_type rotate(size_type x, int dir) noexcept {
		int revdir = dir ^ 1;
		size_type res = pool[x].kid[revdir];
		pool[x].kid[revdir] = pool[res].kid[dir];
		pool[res].kid[dir] = x;
		seth(x);
		// assert(nil->kid[L] == nil);
		// assert(nil->kid[R] == nil);
		return res;
	}

	/// rotating nil node up is invalid
	size_type rotate_and_seth(size_type x, int dir) noexcept {
		size_type res = rotate(x, dir);
		seth(res);
		return res;
	}

	size_type rebalance_and_seth(size_type x) noexcept {
		int b = (pool[pool[x].kid[L]].h - pool[pool[x].kid[R]].h) / 2;
		if (b) {
			int dir = (b + 1) >> 1, revdir = dir ^ 1;
			size_type aux_B = pool[x].kid[revdir];
			if (pool[pool[aux_B].kid[R]].h - pool[pool[aux_B].kid[L]].h == b) {
				size_type aux_C = pool[aux_B].kid[dir];
				pool[x].kid[revdir] = pool[aux_C].kid[dir];
				seth(x);
				pool[aux_B].kid[dir] = pool[aux_C].kid[revdir];
				seth(aux_B);
				pool[aux_C].kid[dir] = x;
				pool[aux_C].kid[revdir] = aux_B;
				seth(aux_C);
				return aux_C;
			}

			return rotate_and_seth(x, dir);
		}

		seth(x);
		return x;
	}

	template<class Func>
	void for_each(size_type x, Func&& func) {
		if (x == nil)
			return;

		for_each(pool[x].kid[L], func);
		func(pool[x]);
		for_each(pool[x].kid[R], func);
	}

	template<class Func>
	void for_each(size_type x, Func&& func) const {
		if (x == nil)
			return;

		for_each(pool[x].kid[L], func);
		func(pool[x]);
		for_each(pool[x].kid[R], func);
	}

	template<class Func>
	void for_each(Func&& func) { for_each(root, std::forward<Func>(func)); }

	template<class Func>
	void for_each(Func&& func) const {
		for_each(root, std::forward<Func>(func));
	}

	template<class T, class FuncNodeToRetVal>
	typename std::result_of<FuncNodeToRetVal(Node&)>::type find(const T& key,
		FuncNodeToRetVal&& to_ret_val = {})
	{
		for (size_type x = root; x != nil; ) {
			bool dir = compare(pool[x].key(), key);
			if (not dir and not compare(key, pool[x].key())) // we have a match
				return to_ret_val(pool[x]);

			x = pool[x].kid[dir];
		}

		return nullptr;
	}

	template<class T, class FuncNodeToRetVal>
	typename std::result_of<FuncNodeToRetVal(const Node&)>::type find(
		const T& key, FuncNodeToRetVal&& to_ret_val = {}) const
	{
		for (size_type x = root; x != nil; ) {
			bool dir = compare(pool[x].key(), key);
			if (not dir and not compare(key, pool[x].key())) // we have a match
				return to_ret_val(pool[x]);

			x = pool[x].kid[dir];
		}

		return nullptr;
	}

protected:
	void insert(size_type& x, size_type inserted) {
		if (x == nil) {
			x = inserted;
			return;
		}

		int dir = compare(pool[x].key(), pool[inserted].key());
		insert(pool[x].kid[dir], inserted);
		x = rebalance_and_seth(x);
	}

	/// Return value - @p inserted if the insertion took place, found node id
	/// otherwise
	size_type insert_if_not_exists(size_type& x, size_type inserted) {
		if (x == nil)
			return x = inserted;

		int dir = compare(pool[x].key(), pool[inserted].key());
		// if we have just found a duplicate
		if (not dir and not compare(pool[inserted].key(), pool[x].key()))
			return x;

		auto res = insert_if_not_exists(pool[x].kid[dir], inserted);
		x = rebalance_and_seth(x);
		return res;
	}

	/// Return value - a bool denoting whether the insertion took place
	bool insert_or_replace(size_type& x, size_type inserted) {
		if (x == nil) {
			x = inserted;
			return true;
		}

		int dir = compare(pool[x].key(), pool[inserted].key());
		// if we have just found a duplicate
		if (not dir and not compare(pool[inserted].key(), pool[x].key())) {
			pool[inserted].kid = pool[x].kid;
			deallocate_node(x);
			x = inserted;
			return false;
		}

		auto res = insert_or_replace(pool[x].kid[dir], inserted);
		x = rebalance_and_seth(x);
		return res;
	}

	/// @p direction: left - 0, right - 1
	Node* dirmost(size_type node, int direction) noexcept {
		while (node != nil) {
			auto next = pool[node].kid[direction];
			if (next == nil)
				return &pool[node];

			node = next;
		}

		return nullptr;
	}

	/// @p direction: left - 0, right - 1
	const Node* dirmost(size_type node, int direction) const noexcept {
		while (node != nil) {
			auto next = pool[node].kid[direction];
			if (next == nil)
				return &pool[node];

			node = next;
		}

		return nullptr;
	}

public:
	Node* insert(size_type node_id) {
		insert(root, node_id);
		return &pool[node_id];
	}


	/// Return value - @p node_id if the insertion took place, found node id
	/// otherwise (in this case you have to manually deallocate @p node_id)
	size_type insert_if_not_exists(size_type node_id) {
		return insert_if_not_exists(root, node_id);
	}

	std::pair<Node*, bool> insert_or_replace(size_type node_id) {
		auto b = insert_or_replace(root, node_id);
		return {&pool[node_id], b};
	}

	Node* front() noexcept { return dirmost(root, L); }

	const Node* front() const noexcept { return dirmost(root, L); }

	Node* back() noexcept { return dirmost(root, R); }

	const Node* back() const noexcept { return dirmost(root, R); }

	// return value - pulled out node; x is updated automatically
	size_type pull_out_rightmost(size_type& x) {
		if (pool[x].kid[R] == nil) {
			auto res = x;
			x = pool[x].kid[L];
			return res; // In this case tree is well-balanced
		}

		auto res = pull_out_rightmost(pool[x].kid[R]);
		x = rebalance_and_seth(x);
		return res;
	}

	/// Return value - a bool denoting whether the erasing took place
	bool erase(size_type& x, const typename Node::Key& k) {
		if (x == nil)
			return false;

		bool dir = compare(pool[x].key(), k);
		if (not dir and not compare(k, pool[x].key())) {
			if (pool[x].kid[L] == nil) {
				size_type res = pool[x].kid[R];

				deallocate_node(x);

				if (res != nil)
					x = rebalance_and_seth(res);
				else
					x = res;

				return true;

			} else {
				auto x_left = pool[x].kid[L];
				auto pulled = pull_out_rightmost(x_left);
				// pulled replaces x in the tree structure
				pool[pulled].kid[L] = x_left;
				pool[pulled].kid[R] = pool[x].kid[R];

				deallocate_node(x);

				x = rebalance_and_seth(pulled);
				return true;
			}
		}

		bool res = erase(pool[x].kid[dir], k);
		x = rebalance_and_seth(x);
		return res;
	}

	/// Return value - a bool denoting whether the erasing took place
	bool erase(const typename Node::Key& k) { return erase(root, k); }

	template<class FuncNodeToRetVal>
	typename std::result_of<FuncNodeToRetVal(Node&)>::type lower_bound(
		const typename Node::Key& k, FuncNodeToRetVal&& to_ret_val = {})
	{
		typename std::result_of<FuncNodeToRetVal(Node&)>::type res = nullptr;
		size_type x = root;
		while (x != nil) {
			if (compare(pool[x].key(), k))
				x = pool[x].kid[R];
			else {
				res = to_ret_val(pool[x]);
				x = pool[x].kid[L];
			}
		}

		return res;
	}

	template<class FuncNodeToRetVal>
	typename std::result_of<FuncNodeToRetVal(const Node&)>::type lower_bound(
		const typename Node::Key& k, FuncNodeToRetVal&& to_ret_val = {}) const
	{
		typename std::result_of<FuncNodeToRetVal(const Node&)>::type res =
			nullptr;
		size_type x = root;
		while (x != nil) {
			if (compare(pool[x].key(), k))
				x = pool[x].kid[R];
			else {
				res = to_ret_val(pool[x]);
				x = pool[x].kid[L];
			}
		}

		return res;
	}

	template<class FuncNodeToRetVal>
	typename std::result_of<FuncNodeToRetVal(Node&)>::type upper_bound(
		const typename Node::Key& k, FuncNodeToRetVal&& to_ret_val = {})
	{
		typename std::result_of<FuncNodeToRetVal(Node&)>::type res = nullptr;
		size_type x = root;
		while (x != nil) {
			if (compare(k, pool[x].key())) {
				res = to_ret_val(pool[x]);
				x = pool[x].kid[L];
			} else
				x = pool[x].kid[R];
		}

		return res;
	}

	template<class FuncNodeToRetVal>
	typename std::result_of<FuncNodeToRetVal(const Node&)>::type upper_bound(
		const typename Node::Key& k, FuncNodeToRetVal&& to_ret_val = {}) const
	{
		typename std::result_of<FuncNodeToRetVal(const Node&)>::type res =
			nullptr;
		size_type x = root;
		while (x != nil) {
			if (compare(k, pool[x].key())) {
				res = to_ret_val(pool[x]);
				x = pool[x].kid[L];
			} else
				x = pool[x].kid[R];
		}

		return res;
	}

#if 0
	void print(size_type x, int tabs = 0) const {
		if (x == nil)
			return;

		print(pool[x].kid[R], tabs + 1);

		cout << string(tabs * 3, ' ') << x <<  "- h: " << (int)pool[x].h << ", "
			<< pool[x].value() << '{' << pool[x].kid[L] << " " << pool[x].kid[R]
			<< '}' << endl;

		print(pool[x].kid[L], tabs + 1);
	}

	void print() const {
		cout << nl << nil << " NIL: "
			<< pool[nil].kid[L] << " " << pool[nil].kid[R] << endl;
		print(root);
		assert(pool[pool[nil].kid[L]].h == 0);
		assert(pool[nil].kid[L] == nil);
		assert(pool[nil].kid[R] == nil);
	}
#endif
};

template<template<class...> class NodeT, class... ArgsToForward>
class AVLDictContainer : protected AVLDictionary<NodeT, ArgsToForward...> {
protected:
	using AVLBase = AVLDictionary<NodeT, ArgsToForward...>;
	using typename AVLBase::Node;
	using AVLBase::pool;
	using AVLBase::nil;

public:
	using AVLBase::AVLBase;
	using AVLBase::clear;
	using AVLBase::size;
	using AVLBase::capacity;
	using AVLBase::max_capacity;
	using AVLBase::reserve_for;
	using AVLBase::empty;
	using AVLBase::erase;

	typename Node::Data* front() {
		auto x = AVLBase::front();
		return (x ? &x->data() : nullptr);
	}

	const typename Node::Data* front() const {
		auto x = AVLBase::front();
		return (x ? &x->data() : nullptr);
	}

	typename Node::Data* back() {
		auto x = AVLBase::back();
		return (x ? &x->data() : nullptr);
	}

	const typename Node::Data* back() const {
		auto x = AVLBase::back();
		return (x ? &x->data() : nullptr);
	}

	// Returns a pointer to the found value or nullptr if there is no such value
	template<class T>
	typename Node::Data* find(const T& key) {
		return AVLBase::find(key, [](Node& x) { return &x.data(); });
	}

	// Returns a pointer to the found value or nullptr if there is no such value
	template<class T>
	const typename Node::Data* find(const T& key) const {
		return AVLBase::find(key, [](const Node& x) { return &x.data(); });
	}

	template<class Func>
	void for_each(Func&& func) {
		AVLBase::for_each([&func](Node& node) { func(node.data()); });
	}

	template<class Func>
	void for_each(Func&& func) const {
		AVLBase::for_each([&func](const Node& node) {
			func(node.data());
		});
	}

	const typename Node::Data* lower_bound(const typename Node::Key& key) {
		return AVLBase::lower_bound(key, [](Node& x) { return &x.data(); });
	}

	const typename Node::Data* lower_bound(const typename Node::Key& key) const
	{
		return AVLBase::lower_bound(key, [](const Node& x) {
			return &x.data();
		});
	}

	const typename Node::Data* upper_bound(const typename Node::Key& key) {
		return AVLBase::upper_bound(key, [](Node& x) { return &x.data(); });
	}

	const typename Node::Data* upper_bound(const typename Node::Key& key) const
	{
		return AVLBase::upper_bound(key, [](const Node& x) {
			return &x.data();
		});
	}
};

template<class size_type, class ValueT>
class AVLSetNode : public AVLNodeBase<size_type> {
public:
	using Key = const ValueT;
	using Value = Key;
	using Data = Key;

	const Key key_;

	template<class... Args>
	AVLSetNode(Key key, Args&&... args)
		: AVLNodeBase<size_type>{std::forward<Args>(args)...}, key_{key} {}

	Key& key() const noexcept { return key_; }

	Data& data() const { return key(); }
};

template<class Value, class Comp = std::less<>, class size_type = size_t>
class AVLDictSet : public AVLDictContainer<AVLSetNode, Comp, size_type, Value> {
	using ADC = AVLDictContainer<AVLSetNode, Comp, size_type, Value>;
	using typename ADC::AVLBase;
	using typename ADC::Node;
	using ADC::nil;

public:
	using ADC::ADC;
	using ADC::operator=;

	/// Return value - a bool denoting whether the insertion took place
	template<class... Args>
	bool emplace(Args&&... args) {
		auto new_node =
			AVLBase::allocate_node(Value{std::forward<Args>(args)...}, nil, nil,
				uint8_t{0});
		auto x = AVLBase::insert_if_not_exists(new_node);
		if (x != new_node) {
			AVLBase::deallocate_node(new_node);
			return false;
		}

		return true;
	}

	/// Return value - a bool denoting whether the insertion took place
	bool insert(Value val) { return emplace(std::move(val)); }
};

template<class Value, class Comp = std::less<>, class size_type = size_t>
class AVLDictMultiset :
	public AVLDictContainer<AVLSetNode, Comp, size_type, Value>
{
	using ADC = AVLDictContainer<AVLSetNode, Comp, size_type, Value>;
	using typename ADC::AVLBase;
	using typename ADC::Node;
	using ADC::nil;

public:
	using ADC::ADC;
	using ADC::operator=;

	/// Return value - a bool denoting whether the insertion took place
	template<class... Args>
	void emplace(Args&&... args) {
		AVLBase::insert(
			AVLBase::allocate_node(Value{std::forward<Args>(args)...}, nil, nil,
				uint8_t{0}));
	}

	/// Return value - a bool denoting whether the insertion took place
	void insert(Value val) { emplace(std::move(val)); }
};

template<class size_type, class KeyT, class ValueT>
class AVLMapNode : public AVLNodeBase<size_type> {
public:
	using Key = KeyT;
	using Value = ValueT;
	using Data = std::pair<const Key, Value>;

	Data data_;

	template<class... Args>
	AVLMapNode(Data data, Args&&... args)
		: AVLNodeBase<size_type>{std::forward<Args>(args)...}, data_{data} {}

	Data& data() noexcept { return data_; }

	const Data& data() const noexcept { return data_; }

	const Key& key() const noexcept { return data_.first; }

	Value& value() noexcept { return data_.second; }

	const Value& value() const noexcept { return data_.second; }
};

template<class Key, class Value, class Comp = std::less<>,
	class size_type = size_t>
class AVLDictMap :
	public AVLDictContainer<AVLMapNode, Comp, size_type, Key, Value>
{
	using ADC = AVLDictContainer<AVLMapNode, Comp, size_type, Key, Value>;
	using typename ADC::AVLBase;
	using typename ADC::Node;
	using KeyValPair = typename ADC::Node::Data;
	using ADC::pool;
	using ADC::nil;

public:
	using ADC::ADC;
	using ADC::operator=;

	/// Return value - a pair of a pointer to pair (key, value) and a bool
	/// denoting whether the insertion took place
	template<class... Args>
	std::pair<KeyValPair*, bool> emplace(Args&&... args) {
		auto new_node =
			AVLBase::allocate_node(KeyValPair{std::forward<Args>(args)...}, nil,
				nil, uint8_t{0});

		auto x = AVLBase::insert_or_replace(new_node);
		return {&x.first->data(), x.second};
	}

	/// Return value - a pair of a pointer to pair (key, value) and a bool
	/// denoting whether the insertion took place
	std::pair<KeyValPair*, bool> insert(KeyValPair kvp) {
		return emplace(std::move(kvp));
	}

	Value& operator[](Key key) {
		auto new_node =
			AVLBase::allocate_node(KeyValPair{std::move(key), {}}, nil,
				nil, uint8_t{0});
		auto x = AVLBase::insert_if_not_exists(new_node);
		if (x != new_node)
			AVLBase::deallocate_node(new_node);

		return pool[x].value();
	}
};

template<class Key, class Value, class Comp = std::less<>,
	class size_type = size_t>
class AVLDictMultimap :
	public AVLDictContainer<AVLMapNode, Comp, size_type, Key, Value>
{
	using ADC = AVLDictContainer<AVLMapNode, Comp, size_type, Key, Value>;
	using typename ADC::AVLBase;
	using typename ADC::Node;
	using KeyValPair = typename ADC::Node::Data;
	using ADC::pool;
	using ADC::nil;

public:
	using ADC::ADC;
	using ADC::operator=;

	/// Return value - a pointer to pair (key, value)
	template<class... Args>
	KeyValPair* emplace(Args&&... args) {
		auto new_node =
			AVLBase::allocate_node(KeyValPair{std::forward<Args>(args)...}, nil,
				nil, uint8_t{0});

		AVLBase::insert(new_node);
		return &pool[new_node].data();
	}

	/// Return value - a pointer to pair (key, value)
	KeyValPair* insert(KeyValPair kvp) { return emplace(std::move(kvp)); }
};

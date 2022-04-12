#ifndef SIVYY_PUBLISHER_HPP
#define SIVYY_PUBLISHER_HPP


#include <functional>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <tuple>

namespace detail {
	struct class_id {};

	template<typename T>
	struct class_registry {
	public:
		static class_id * get_id() noexcept  {
			static class_id id;
			return std::addressof(id);
		}
	};


	template<class ForwardIterator>
	ForwardIterator 
	shift_item(ForwardIterator item, ForwardIterator boundary) {
		ForwardIterator next_position = item;

		while (++next_position != boundary)
			std::swap(*next_position, *item++);

		return item;
	}

	template<class ForwardIterator, class T, class Comparator>
	std::pair<ForwardIterator, bool> 
	binary_search(ForwardIterator left_boundary, ForwardIterator right_boundary, T const& value, Comparator cmp) {
		left_boundary = std::lower_bound(left_boundary, right_boundary, value, cmp);

		bool found = !(left_boundary == right_boundary) and
					 !(cmp(value, *left_boundary));

		return std::make_pair(std::move(left_boundary), found);
	}

}


class Publisher {
	using handler_t = std::function<void(void*)>;

	struct handler_slot {
		std::uint64_t handler_id;
		handler_t handler;
	};

	template<typename T, typename Fn>
	struct publisher_lambda {
		Fn handler_;
	public:
		publisher_lambda(Fn&& handler)
		: handler_(std::forward<Fn>(handler)) 
		{

		}

		void operator()(void* data) {
			handler_(*static_cast<T*>(data));
		}
	};

public:

	class handler_token {
		friend class Publisher;

		std::uint64_t handler_id;
		detail::class_id* class_id;
	};

	struct handler_comparator {
		bool operator()(handler_slot const & slot, std::uint64_t handler_id) const noexcept {
			return slot.handler_id < handler_id;
		}

		bool operator()(std::uint64_t handler_id, handler_slot const& slot) const noexcept {
			return handler_id < slot.handler_id;
		}
	};

public:
	template<typename TEvent, typename Fn>
	handler_token const subscribe(Fn&& handler) {
		auto wrapper = publisher_lambda<TEvent, Fn>(std::forward<Fn>(handler));
		handler_slot slot{ token_counter_, std::move(wrapper) };

		auto const class_id = detail::class_registry<TEvent>::get_id();
		slots_[class_id].push_back(std::move(slot));

		handler_token token;
		token.handler_id = slot.handler_id;
		token.class_id = class_id;

		token_counter_ += 1;
		return token;
	}


	bool unsubscribe(handler_token token) {
		if (not slots_.count(token.class_id))
			return false;

		auto& entry = slots_[token.class_id];

		bool found;
		std::vector<handler_slot>::iterator iter;		
		std::tie(iter, found) = detail::binary_search(std::begin(entry), std::end(entry), 
												   token.handler_id, handler_comparator());

		if (found == false)
			return false;

		entry.erase(detail::shift_item(iter, std::end(entry)));

		token.handler_id = 0; // invalidation 

		return true;
	}

	template<typename TEvent>
	void publish(TEvent&& event) {
		using key = detail::class_registry<typename std::remove_reference<TEvent>::type>;
		auto const class_id = key::get_id();

		if (slots_.count(class_id)) 
			for (auto const& slot : slots_[class_id])
				slot.handler(&event);        
	}

private:
	std::uint64_t token_counter_ = 1;
	std::unordered_map<detail::class_id const*, std::vector<handler_slot>> slots_;
};



#endif // SIVYY_PUBLISHER_HPP
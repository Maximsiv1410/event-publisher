#ifndef SIVYY_PUBLISHER_HPP
#define SIVYY_PUBLISHER_HPP



#include <functional>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace detail {
    struct class_id {};

    template<typename T>
    struct class_registry {
    public:
        static class_id* get_id() {
            static class_id id;
            return std::addressof(id);
        }
    };
}


class Publisher {
    template<typename T, typename Fn>
    struct publisher_lambda {
        Fn handler_;

        publisher_lambda(Fn&& handler)
            : handler_(std::forward<Fn>(handler)) {

        }

        void operator()(void* data) {
            handler_(*static_cast<T*>(data));
        }
    };

    struct handler_slot {
        using handler_t = std::function<void(void*)>;

        std::uint64_t handler_id;
        handler_t handler;
    };
public:

    class handler_token {
        friend class Publisher;

        std::uint64_t handler_id;
        detail::class_id* class_id;
    };

public:
    template<typename TEvent, typename Fn>
    handler_token const subscribe(Fn&& handler) {
        auto wrapper = publisher_lambda<TEvent, Fn>(std::forward<Fn>(handler));

        handler_slot slot{ token_counter_, std::move(wrapper) };

        auto const class_id = detail::class_registry<TEvent>::get_id();

        slots_[class_id].push_back(std::move(slot));

        token_counter_ += 1;

        handler_token token;
        token.handler_id = slot.handler_id;
        token.class_id = class_id;

        return token;
    }

    /* TODO: improve storage to be always sorted

    */
    bool unsubscribe(handler_token const token) {
        if (not slots_.count(token.class_id))
            return false;

        auto& entry = slots_[token.class_id];

        auto const position = std::remove_if(std::begin(entry), std::end(entry),
                                             [&token](handler_slot const& item) { return item.handler_id == token.handler_id; });

        if (position == std::end(entry))
            return false;

        entry.erase(position);

        return true;
    }

    template<typename TEvent>
    void publish(TEvent&& event) {
        using key = detail::class_registry<typename std::remove_reference<TEvent>::type>;
        auto const class_id = key::get_id();

        if (slots_.count(class_id)) {
            for (auto const& slot : slots_[class_id])
                slot.handler(&event);
        }
    }

private:
    std::uint64_t token_counter_ = 0;
    std::unordered_map<detail::class_id*, std::vector<handler_slot>> slots_;
};




#endif // SIVYY_PUBLISHER_HPP
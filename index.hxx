#ifndef __EVENTS_H_
#define __EVENTS_H_

#include <iostream>
#include <functional>
#include <string>
#include <map>
#include <vector>
#include <any>
#include <tuple>
#include <utility>
#include <type_traits>
#include <algorithm>
#include <mutex>

class EventEmitter {
  struct ListenerWrapper {
    std::any callback;
    bool is_once;
  };

  std::map<std::string, std::vector<ListenerWrapper>> events;
  int _listeners = 0;
  mutable std::mutex mtx_;

  template <typename Callable>
  struct traits : public traits<decltype(&std::decay_t<Callable>::operator())> {};

  template <typename ClassType, typename R, typename... Args>
  struct traits<R(ClassType::*)(Args...) const> {
    using ReturnType = R; using ArgumentTypesAsTuple = std::tuple<Args...>;
    using OriginalFunctionType = std::function<R(Args...)>;
    using StoredFunctionType = std::function<void(std::decay_t<Args>...)>;
  };
  template <typename ClassType, typename R, typename... Args>
  struct traits<R(ClassType::*)(Args...) const noexcept> {
    using ReturnType = R; using ArgumentTypesAsTuple = std::tuple<Args...>;
    using OriginalFunctionType = std::function<R(Args...)>;
    using StoredFunctionType = std::function<void(std::decay_t<Args>...)>;
  };
  template <typename ClassType, typename R, typename... Args>
  struct traits<R(ClassType::*)(Args...) > {
    using ReturnType = R; using ArgumentTypesAsTuple = std::tuple<Args...>;
    using OriginalFunctionType = std::function<R(Args...)>;
    using StoredFunctionType = std::function<void(std::decay_t<Args>...)>;
  };
  template <typename ClassType, typename R, typename... Args>
  struct traits<R(ClassType::*)(Args...) noexcept> {
    using ReturnType = R; using ArgumentTypesAsTuple = std::tuple<Args...>;
    using OriginalFunctionType = std::function<R(Args...)>;
    using StoredFunctionType = std::function<void(std::decay_t<Args>...)>;
  };
  template <typename R, typename... Args>
  struct traits<R(*)(Args...)> {
    using ReturnType = R; using ArgumentTypesAsTuple = std::tuple<Args...>;
    using OriginalFunctionType = std::function<R(Args...)>;
    using StoredFunctionType = std::function<void(std::decay_t<Args>...)>;
  };
  template <typename R, typename... Args>
  struct traits<R(*)(Args...) noexcept> {
    using ReturnType = R; using ArgumentTypesAsTuple = std::tuple<Args...>;
    using OriginalFunctionType = std::function<R(Args...)>;
    using StoredFunctionType = std::function<void(std::decay_t<Args>...)>;
  };
  template <typename R, typename... Args>
  struct traits<std::function<R(Args...)>> {
    using ReturnType = R; using ArgumentTypesAsTuple = std::tuple<Args...>;
    using OriginalFunctionType = std::function<R(Args...)>;
    using StoredFunctionType = std::function<void(std::decay_t<Args>...)>;
  };

  template <typename Callback>
  static typename traits<std::decay_t<Callback>>::OriginalFunctionType
  to_original_function(Callback&& cb) {
    return typename traits<std::decay_t<Callback>>::OriginalFunctionType(std::forward<Callback>(cb));
  }

  template <typename Callback>
  void add_listener(const std::string& name, Callback&& cb, bool is_once_flag) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (++this->_listeners > this->maxListeners) {
      std::cout
        << "warning: possible EventEmitter memory leak detected. "
        << this->_listeners
        << " listeners added (max is " << this->maxListeners
        << "). For event: " << name
        << std::endl;
    }

    auto original_func = to_original_function(std::forward<Callback>(cb));
    typename traits<std::decay_t<Callback>>::StoredFunctionType storable_func = original_func;

    events[name].push_back({storable_func, is_once_flag});
  }

public:
  int maxListeners = 10;

  EventEmitter() = default;
  ~EventEmitter() = default;

  EventEmitter(const EventEmitter&) = delete;
  EventEmitter& operator=(const EventEmitter&) = delete;
  EventEmitter(EventEmitter&&) = delete;
  EventEmitter& operator=(EventEmitter&&) = delete;


  int listeners() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return this->_listeners;
  }

  template <typename Callback>
  void on(const std::string& name, Callback&& cb) {
    add_listener(name, std::forward<Callback>(cb), false /*is_once_flag*/);
  }

  template <typename Callback>
  void once(const std::string& name, Callback&& cb) {
    add_listener(name, std::forward<Callback>(cb), true /*is_once_flag*/);
  }

  void off() {
    std::lock_guard<std::mutex> lock(mtx_);
    events.clear();
    this->_listeners = 0;
  }

  void off(const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = events.find(name);
    if (it != events.end()) {
      this->_listeners -= it->second.size();
      events.erase(it);
    }
  }

  template <typename... EmitArgs>
  void emit(const std::string& name, EmitArgs&&... args) {
    std::vector<ListenerWrapper> current_call_list;
    bool has_any_once_listener_in_list = false;

    {
      std::lock_guard<std::mutex> lock(mtx_);
      auto map_it = events.find(name);
      if (map_it == events.end() || map_it->second.empty()) {
        return;
      }
      current_call_list = map_it->second;

      for(const auto& wrapper : current_call_list) {
        if (wrapper.is_once) {
          has_any_once_listener_in_list = true;
          break;
        }
      }
    }

    std::tuple<EmitArgs...> captured_args_tuple(std::forward<EmitArgs>(args)...);

    for (const auto& listener_entry : current_call_list) {
      try {
        using TargetFunctionType = std::function<void(std::decay_t<EmitArgs>...)>;
        const auto& storable_func_any = listener_entry.callback;
        const auto& storable_func = std::any_cast<const TargetFunctionType&>(storable_func_any);

        std::apply([&](auto&&... tuple_args) {
          storable_func(std::forward<decltype(tuple_args)>(tuple_args)...);
        }, captured_args_tuple);

      } catch (const std::bad_any_cast& e) {
        std::cerr << "Emit error for event '" << name << "': "
                  << "Callback signature mismatch. Details: " << e.what()
                  << std::endl;
      } catch (const std::bad_function_call& e) {
         std::cerr << "Emit error for event '" << name << "': "
                   << "Bad function call (e.g. empty std::function). Details: " << e.what()
                   << std::endl;
      }
    }

    if (has_any_once_listener_in_list) {
      std::lock_guard<std::mutex> lock(mtx_);
      auto map_it = events.find(name);
      if (map_it != events.end()) {
        std::vector<ListenerWrapper>& original_listeners_ref = map_it->second;
        std::size_t original_size = original_listeners_ref.size();

        original_listeners_ref.erase(
          std::remove_if(original_listeners_ref.begin(), original_listeners_ref.end(),
                         [](const ListenerWrapper& entry) {
                           return entry.is_once;
                         }),
          original_listeners_ref.end()
        );

        std::size_t removed_count = original_size - original_listeners_ref.size();
        this->_listeners -= removed_count;

        if (original_listeners_ref.empty()) {
          events.erase(map_it);
        }
      }
    }
  }
};

#endif // __EVENTS_H_


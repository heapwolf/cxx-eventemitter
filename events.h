#ifndef __EVENTS_H_
#define __EVENTS_H_

#include <functional>
#include <unordered_map>

namespace Events {

  class EventEmitter {
    public:

      typedef const std::type_info* EventType;
      typedef std::function<void (void*)> Callback;
      typedef std::unordered_multimap<EventType, Callback> Listeners;

      Listeners listeners;

      template<typename TEvent, typename TListener>
      void on(const TListener function) 
      {
        auto deferred = [=](void* ev) {
          function(*static_cast<TEvent*>(ev));
        };

        auto event = &typeid(std::decay<TEvent>);
        auto pair = std::make_pair(event, deferred);
        listeners.insert(pair);
      }

      template<typename TEvent>
      void off()
      {
        auto type = &typeid(std::decay<TEvent>);
        auto range = listeners.equal_range(type);
        listeners.erase(range.first, range.second);
      }

      template<typename TEvent>
      void emit(TEvent ev)
      {
        auto range = listeners.equal_range(&typeid(std::decay<TEvent>));
        for (auto itr = range.first; itr != range.second; ++itr)
        {
          (itr->second)(static_cast<void*>(&ev));
        }
      }
  };
}

#endif


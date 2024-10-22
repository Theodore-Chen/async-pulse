#pragma once

#include <functional>

template <typename Event>
using Action = std::function<void(Event event)>;

template <typename State, typename Event>
struct FsmState {
    State state_;
    Action<Event> entry_{nullptr};
    Action<Event> exit_{nullptr};
    Action<Event> callback_{nullptr};
};

// Only the declaration is provided
// the definition needs to be provided by user
template <typename State, typename Event>
using StateTable = std::map<State, FsmState<State, Event>>;

template <typename State,
          typename Event,
          typename std::enable_if<std::is_enum<State>::value, bool>::type = true,
          typename std::enable_if<std::is_enum<Event>::value, bool>::type = true>
class FsmStateTable {
   public:
    FsmStateTable() {}
    explicit FsmStateTable(StateTable<State, Event>* table) : table_(table) {}
    void Entry(State state, Event event) {
        if (Valid(state)) {
            table_->find(state)->second.entry_(event);
        }
    }
    void Exit(State state, Event event) {
        if (Valid(state)) {
            table_->find(state)->second.exit_(event);
        }
    }
    void Callback(State state, Event event) {
        if (Valid(state)) {
            table_->find(state)->second.callback_(event);
        }
    }

   private:
    bool Valid(State state) {
        if (table_ && table_->find(state) != table_->end()) {
            return true;
        }
        return false;
    }

   private:
    StateTable<State, Event>* table_{nullptr};
};

// Only the declaration is provided
// the definition needs to be provided by user
template <typename State, typename Event>
using StateChangeTable = std::map<State, std::map<Event, State>>;

template <typename State,
          typename Event,
          typename std::enable_if<std::is_enum<State>::value, bool>::type = true,
          typename std::enable_if<std::is_enum<Event>::value, bool>::type = true>
class FsmStateChangeTable {
   public:
    FsmStateChangeTable() {}
    explicit FsmStateChangeTable(StateChangeTable<State, Event>* table) : table_(table) {}
    bool Valid(State state, Event event) {
        if (table_ && table_->find(state) != table_->end()) {
            const std::map<Event, State>& eventTable = (*table_)[state];
            if (eventTable.find(event) != eventTable.end()) {
                return true;
            }
        }
        return false;
    }
    bool GetTostate(State state, Event event, State& toState) {
        if (Valid(state, event)) {
            toState = (*table_)[state][event];
            return true;
        }
        return false;
    }

   private:
    StateChangeTable<State, Event>* table_{nullptr};
};

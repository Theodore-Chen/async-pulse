#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <thread>

#include "queue/lock_queue.h"

using namespace std::chrono_literals;

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

// Only the declaration is provided
// the definition needs to be provided by user
template <typename State, typename Event>
using StateChangeTable = std::map<State, std::map<Event, State>>;

template <typename State, typename Event>
class FsmStateTable {
   public:
    FsmStateTable() {}
    explicit FsmStateTable(StateTable<State, Event>* table) : table_(table) {}
    bool Valid(State state) { return (table_ && table_->find(state) != table_->end()) ? true : false; }
    FsmState<State, Event>* GetFsmState(State state) { return Valid(state) ? &(*table_)[state] : nullptr; }

   private:
    StateTable<State, Event>* table_{nullptr};
};

template <typename State, typename Event>
class FsmStateChangeTable {
   public:
    FsmStateChangeTable() {}
    explicit FsmStateChangeTable(StateChangeTable<State, Event>* table) : table_(table) {}
    bool Valid(State state, Event event) {
        if (table_ && table_->find(state) != table_->end() && (*table_)[state].find(event) != (*table_)[state].end()) {
            return true;
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

template <typename State, typename Event>
class FSM {
   public:
    FSM(StateTable<State, Event>* stateTable, StateChangeTable<State, Event>* changeTable, State initial) {
        stateTable_ = FsmStateTable<State, Event>(stateTable);
        changeTable_ = FsmStateChangeTable<State, Event>(changeTable);
        ready_.store(true);
        pause_.store(false);
        pooling_ = std::async(std::launch::async, Process, this);
        curState_.store(stateTable_.GetFsmState(initial));
    }
    ~FSM() { ready_.store(false); }
    void Submit(Event event) { queue_.Enqueue(event); }
    State GetState() { return curState_.load()->state_; }

   private:
    static void Process(FSM* fsm) {
        Event event;
        while (fsm->ready_.load() == true) {
            if (fsm->pause_.load() == false && fsm->queue_.Dequeue(event)) {
                fsm->curState_.load()->callback_(event);
                State toState;
                if (fsm->changeTable_.GetTostate(fsm->curState_.load()->state_, event, toState)) {
                    ChangeState(fsm, event, toState);
                }
            }
            std::this_thread::sleep_for(10ms);
        }
    }
    static void ChangeState(FSM* fsm, Event event, State toState) {
        FsmState<State, Event>* toFsm = fsm->stateTable_.GetFsmState(toState);
        if (toState == fsm->curState_.load()->state_ || toFsm == nullptr) {
            return;
        }
        fsm->curState_.load()->exit_(event);
        fsm->curState_.exchange(toFsm);
        fsm->curState_.load()->entry_(event);
        fsm->curState_.load()->callback_(event);
    }

   private:
    std::atomic<FsmState<State, Event>*> curState_{nullptr};
    FsmStateTable<State, Event> stateTable_;
    FsmStateChangeTable<State, Event> changeTable_;
    std::future<void> pooling_;
    LockQueue<Event> queue_;
    std::atomic<bool> pause_{true};
    std::atomic<bool> ready_{false};
};
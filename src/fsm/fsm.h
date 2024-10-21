#include <array>
#include <atomic>
#include <functional>
#include <future>
#include <map>

#include <queue/lock_queue.h>

template <typename Event>
using Action = std::function<void(Event event)>;

template <typename State, typename Event>
struct FsmState {
    const State state_;
    Action<Event> entry_{nullptr};
    Action<Event> exit_{nullptr};
    Action<Event> callback_{nullptr};
};

template <typename State, typename Event>
using StateTable = std::map<State, FsmState<State, Event>>;

template <typename State, typename Event>
class FsmStateTable {
    FsmStateTable(StateTable<State, Event>* table) : table_(table) {}
    bool Valid(State state) { return (table_ != nullptr && table_.find(state) != table_.end()) ? true : false; }
    FsmState<State, Event>* GetFsmState(State state) { return Valid(state) ? &table[state] : nullptr }

   private:
    StateTable<State, Event>* table_{nullptr};
};

template <typename State, typename Event>
using StateChangeTable = std::map<State, std::map<Event, State>>;

template <typename State, typename Event>
class FsmStateChangeTable {
   public:
    FsmStateChangeTable(StateChangeTable<State, Event>* table) : table_(table) {}
    bool Valid(State state, Event event) {
        if (table_ && table_.find(state) != table_.end() && table_[state].find(event) != table_[state].end()) {
            return true;
        }
        return false;
    }
    std::pair<bool, State> GetTostate(State state, Event event) {
        if (Valid(state, event)) {
            return std::make_pair(true, table_[state][event]);
        }
        return std::make_pair(false, State());
    }

   private:
    StateChangeTable<State, Event>* table_{nullptr};
};

template <typename State, typename Event>
class FSM {
   public:
    FSM() {
        pooling_ = std::async(std::launch::async, Process, this);
        ready_.store(true);
        pause_.store(true);
    }
    void Submit(Event event) { queue_.Enqueue(event); }
    static void Process(FSM* fsm) {
        Event event;
        while (fsm->ready_ == true && fsm->pause_ != true) {
            if (fsm->queue_.Dequeue(event)) {
                curState_.callback_(event);
                auto [valid, toState] = changeTable_->GetTostate(curState_->state_, event);
                if (valid) {
                    ChangeState(fsm, toState);
                }
            }
        }
        static void ChangeState(FSM * fsm, State toState) {
            FsmState<State, Event>* toFsm = stateTable_->GetFsmState(toState);
            if (toState == curState_->state_ || toFsm == nullptr) {
                return;
            }
            curState_->exit_(event);
            curState_.exchange(toFsm);
            curState_.load()->entry(event);
            curState_.load()->callback_(event);
        }
    }

   private:
    std::atomic<FsmState<State, Event>*> curState_{nullptr};
    const FsmStateTable<State, Event>* stateTable_{nullptr};
    const FsmStateChangeTable<State, Event>* changeTable_{nullptr};
    std::future<void> pooling_;
    LockQueue<Event> queue_;
    std::atomic<bool> pause_{false};
    std::atomic<bool> ready_{false};
};
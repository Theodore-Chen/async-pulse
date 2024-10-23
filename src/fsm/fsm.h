#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <thread>

#include "fsm/state_table.h"
#include "queue/lock_queue.h"

template <typename State,
          typename Event,
          typename std::enable_if<std::is_enum<State>::value, bool>::type = true,
          typename std::enable_if<std::is_enum<Event>::value, bool>::type = true>
class FSM {
   public:
    FSM(StateTable<State, Event>* stateTable, StateChangeTable<State, Event>* changeTable, State initial)
        : stateTable_(FsmStateTable<State, Event>(stateTable)),
          changeTable_(FsmStateChangeTable<State, Event>(changeTable)),
          curState_(initial) {
        ready_.store(true);
        pause_.store(false);
        pooling_ = std::async(std::launch::async, Process, this);
    }
    ~FSM() {
        ready_.store(false);
        pooling_.wait();
    }
    FSM(const FSM&) = delete;
    FSM(FSM&&) = delete;
    FSM& operator=(const FSM&) = delete;
    FSM& operator=(FSM&&) = delete;

    std::future<void> Submit(Event event) {
        Handle handle(event, std::promise<void>());
        std::future<void> fut = handle.second.get_future();
        queue_.Enqueue(std::move(handle));
        return fut;
    }
    State GetState() { return curState_; }

   private:
    static void Process(FSM* fsm) {
        while (fsm->ready_.load() == true) {
            Handle handle;
            if (fsm->pause_.load() == false && fsm->queue_.Dequeue(handle)) {
                fsm->stateTable_.Callback(fsm->curState_, handle.first);
                ChangeState(fsm, handle.first);
                handle.second.set_value();
            }
        }
    }
    static void ChangeState(FSM* fsm, Event event) {
        State toState;
        if (fsm->changeTable_.GetTostate(fsm->curState_, event, toState)) {
            fsm->stateTable_.Exit(fsm->curState_, event);
            fsm->stateTable_.Entry(toState, event);
            fsm->stateTable_.Callback(toState, event);
            fsm->curState_ = toState;
        }
    }

   private:
    using Handle = std::pair<Event, std::promise<void>>;

   private:
    State curState_;
    FsmStateTable<State, Event> stateTable_;
    FsmStateChangeTable<State, Event> changeTable_;
    std::future<void> pooling_;
    LockQueue<Handle> queue_;
    std::atomic<bool> pause_{true};
    std::atomic<bool> ready_{false};
};
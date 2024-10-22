#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <thread>

#include "fsm/state_table.h"
#include "queue/lock_queue.h"

template <typename State, typename Event>
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
    ~FSM() { ready_.store(false); }
    FSM(const FSM&) = delete;
    FSM& operator=(const FSM&) = delete;
    FSM(FSM&&) = delete;
    FSM& operator=(FSM&&) = delete;

    void Submit(Event event) { queue_.Enqueue(event); }
    State GetState() { return curState_; }

   private:
    static void Process(FSM* fsm) {
        Event event;
        while (fsm->ready_.load() == true) {
            if (fsm->pause_.load() == false && fsm->queue_.Dequeue(event)) {
                fsm->stateTable_.Callback(fsm->curState_, event);

                State toState;
                if (fsm->changeTable_.GetTostate(fsm->curState_, event, toState)) {
                    fsm->stateTable_.Exit(fsm->curState_, event);
                    fsm->stateTable_.Entry(toState, event);
                    fsm->stateTable_.Callback(toState, event);
                    fsm->curState_ = toState;
                }
            }
        }
    }

   private:
    State curState_;
    FsmStateTable<State, Event> stateTable_;
    FsmStateChangeTable<State, Event> changeTable_;
    std::future<void> pooling_;
    LockQueue<Event> queue_;
    std::atomic<bool> pause_{true};
    std::atomic<bool> ready_{false};
};
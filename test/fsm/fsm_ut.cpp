#include <gtest/gtest.h>

#include <atomic>
#include <mutex>
#include <vector>

#include "fsm/fsm.h"

enum class PlayerState : uint32_t { RAW, INIT, PLAY, PAUSE, STOP };
enum class PlayerEvent : uint32_t { INIT, PLAY, PAUSE, STOP, DESTROY };

struct TestCallbacks {
    std::atomic<int> entryCount{0};
    std::atomic<int> exitCount{0};
    std::atomic<int> callbackCount{0};
    std::vector<PlayerEvent> entryEvents;
    std::vector<PlayerEvent> exitEvents;
    std::mutex mtx;

    void Reset() {
        entryCount = 0;
        exitCount = 0;
        callbackCount = 0;
        entryEvents.clear();
        exitEvents.clear();
    }

    Action<PlayerEvent> Entry() {
        return [this](PlayerEvent e) {
            entryCount++;
            std::lock_guard lock(mtx);
            entryEvents.push_back(e);
        };
    }

    Action<PlayerEvent> Exit() {
        return [this](PlayerEvent e) {
            exitCount++;
            std::lock_guard lock(mtx);
            exitEvents.push_back(e);
        };
    }

    Action<PlayerEvent> Callback() {
        return [this](PlayerEvent e) { callbackCount++; };
    }
};

TestCallbacks g_callbacks;

StateTable<PlayerState, PlayerEvent> g_playerStateTable{
    {PlayerState::RAW, {g_callbacks.Entry(), g_callbacks.Exit(), g_callbacks.Callback()}},
    {PlayerState::INIT, {g_callbacks.Entry(), g_callbacks.Exit(), g_callbacks.Callback()}},
    {PlayerState::PLAY, {g_callbacks.Entry(), g_callbacks.Exit(), g_callbacks.Callback()}},
    {PlayerState::PAUSE, {g_callbacks.Entry(), g_callbacks.Exit(), g_callbacks.Callback()}},
    {PlayerState::STOP, {g_callbacks.Entry(), g_callbacks.Exit(), g_callbacks.Callback()}},
};

StateChangeTable<PlayerState, PlayerEvent> g_playerStateChangeTable{
    {PlayerState::RAW, {{PlayerEvent::INIT, PlayerState::INIT}}},
    {PlayerState::INIT,
     {{PlayerEvent::DESTROY, PlayerState::RAW},
      {PlayerEvent::PLAY, PlayerState::PLAY},
      {PlayerEvent::PAUSE, PlayerState::PAUSE},
      {PlayerEvent::STOP, PlayerState::STOP}}},
    {PlayerState::PLAY,
     {{PlayerEvent::DESTROY, PlayerState::RAW},
      {PlayerEvent::PAUSE, PlayerState::PAUSE},
      {PlayerEvent::STOP, PlayerState::STOP}}},
    {PlayerState::PAUSE,
     {{PlayerEvent::DESTROY, PlayerState::RAW},
      {PlayerEvent::PLAY, PlayerState::PLAY},
      {PlayerEvent::STOP, PlayerState::STOP}}},
    {PlayerState::STOP,
     {{PlayerEvent::DESTROY, PlayerState::RAW},
      {PlayerEvent::PLAY, PlayerState::PLAY},
      {PlayerEvent::PAUSE, PlayerState::STOP}}},
};

class FsmUt : public ::testing::Test {
   protected:
    void SetUp() override {
        g_callbacks.Reset();
    }
};

TEST_F(FsmUt, Create) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    EXPECT_EQ(player.GetState(), PlayerState::RAW);
}

TEST_F(FsmUt, SingleTransition) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    auto f = player.Submit(PlayerEvent::INIT);
    f.wait();
    EXPECT_EQ(player.GetState(), PlayerState::INIT);
    EXPECT_EQ(g_callbacks.entryCount, 1);
    EXPECT_EQ(g_callbacks.exitCount, 1);
}

TEST_F(FsmUt, CallbackOrder) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    player.Submit(PlayerEvent::INIT).wait();
    EXPECT_EQ(g_callbacks.exitEvents.back(), PlayerEvent::INIT);
    EXPECT_EQ(g_callbacks.entryEvents.back(), PlayerEvent::INIT);
}

TEST_F(FsmUt, MultipleTransitions) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    player.Submit(PlayerEvent::INIT).wait();
    player.Submit(PlayerEvent::PLAY).wait();
    player.Submit(PlayerEvent::PAUSE).wait();
    EXPECT_EQ(player.GetState(), PlayerState::PAUSE);
}

TEST_F(FsmUt, RunPause) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    player.Submit(PlayerEvent::INIT).wait();
    player.Submit(PlayerEvent::PLAY).wait();
    player.Submit(PlayerEvent::PAUSE).wait();
    EXPECT_EQ(player.GetState(), PlayerState::PAUSE);
}

TEST_F(FsmUt, RunStop) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    player.Submit(PlayerEvent::INIT).wait();
    player.Submit(PlayerEvent::PLAY).wait();
    player.Submit(PlayerEvent::STOP).wait();
    EXPECT_EQ(player.GetState(), PlayerState::STOP);
}

TEST_F(FsmUt, RunStopToPlay) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    player.Submit(PlayerEvent::INIT).wait();
    player.Submit(PlayerEvent::STOP).wait();
    player.Submit(PlayerEvent::PLAY).wait();
    EXPECT_EQ(player.GetState(), PlayerState::PLAY);
}

TEST_F(FsmUt, ConcurrentSubmit) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100; i++) {
        futures.push_back(player.Submit(PlayerEvent::INIT));
        futures.push_back(player.Submit(PlayerEvent::DESTROY));
    }
    for (auto& f : futures)
        f.wait();
    EXPECT_EQ(player.GetState(), PlayerState::RAW);
}

TEST_F(FsmUt, InvalidTransition_NoStateChange) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    player.Submit(PlayerEvent::PLAY).wait();
    EXPECT_EQ(player.GetState(), PlayerState::RAW);
}

TEST_F(FsmUt, Destroy) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    player.Submit(PlayerEvent::INIT).wait();
    player.Submit(PlayerEvent::DESTROY).wait();
    EXPECT_EQ(player.GetState(), PlayerState::RAW);
}

TEST_F(FsmUt, CallbackExecutionOrder) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    player.Submit(PlayerEvent::INIT).wait();

    EXPECT_EQ(player.GetState(), PlayerState::INIT);

    EXPECT_EQ(g_callbacks.callbackCount, 2);
    EXPECT_EQ(g_callbacks.exitCount, 1);
    EXPECT_EQ(g_callbacks.entryCount, 1);

    EXPECT_EQ(g_callbacks.exitEvents[0], PlayerEvent::INIT);
    EXPECT_EQ(g_callbacks.entryEvents[0], PlayerEvent::INIT);
}

TEST_F(FsmUt, InvalidTransition_CallbackStillCalled) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    player.Submit(PlayerEvent::PLAY).wait();

    EXPECT_EQ(player.GetState(), PlayerState::RAW);

    EXPECT_EQ(g_callbacks.callbackCount, 1);
    EXPECT_EQ(g_callbacks.exitCount, 0);
    EXPECT_EQ(g_callbacks.entryCount, 0);
}

#include <gtest/gtest.h>
#include <functional>

#include "fsm/fsm.h"

using namespace std::chrono_literals;

enum class PlayerState : uint32_t {
    RAW,
    INIT,
    PLAY,
    PAUSE,
    STOP,
};

enum class PlayerEvent : uint32_t {
    INIT,
    PLAY,
    PAUSE,
    STOP,
    DESTROY,
};

Action<PlayerEvent> doNothing = [](PlayerEvent event) -> void {};

StateTable<PlayerState, PlayerEvent> g_playerStateTable{
    {PlayerState::RAW,
     {
         .state_ = PlayerState::RAW,
         .entry_ = doNothing,
         .exit_ = doNothing,
         .callback_ = doNothing,
     }},
    {PlayerState::INIT,
     {
         .state_ = PlayerState::INIT,
         .entry_ = doNothing,
         .exit_ = doNothing,
         .callback_ = doNothing,
     }},
    {PlayerState::PLAY,
     {
         .state_ = PlayerState::PLAY,
         .entry_ = doNothing,
         .exit_ = doNothing,
         .callback_ = doNothing,
     }},
    {PlayerState::PAUSE,
     {
         .state_ = PlayerState::PAUSE,
         .entry_ = doNothing,
         .exit_ = doNothing,
         .callback_ = doNothing,
     }},
    {PlayerState::STOP,
     {
         .state_ = PlayerState::STOP,
         .entry_ = doNothing,
         .exit_ = doNothing,
         .callback_ = doNothing,
     }},
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

TEST(FsmUt, Create) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    EXPECT_EQ(player.GetState(), PlayerState::RAW);
}

TEST(FsmUt, Init) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    std::future<void> handle = player.Submit(PlayerEvent::INIT);
    handle.wait();
    EXPECT_EQ(player.GetState(), PlayerState::INIT);
}

TEST(FsmUt, Destroy) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    std::future<void> handle;
    handle = player.Submit(PlayerEvent::INIT);
    handle = player.Submit(PlayerEvent::DESTROY);
    handle.wait();
    EXPECT_EQ(player.GetState(), PlayerState::RAW);
}

TEST(FsmUt, RunPlay) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    std::future<void> handle;
    handle = player.Submit(PlayerEvent::INIT);
    handle = player.Submit(PlayerEvent::PLAY);
    handle.wait();
    EXPECT_EQ(player.GetState(), PlayerState::PLAY);
}

TEST(FsmUt, RunPause) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    std::future<void> handle;
    handle = player.Submit(PlayerEvent::INIT);
    handle = player.Submit(PlayerEvent::PLAY);
    handle = player.Submit(PlayerEvent::PAUSE);
    handle.wait();
    EXPECT_EQ(player.GetState(), PlayerState::PAUSE);
}

TEST(FsmUt, RunStop) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    std::future<void> handle;
    handle = player.Submit(PlayerEvent::INIT);
    handle = player.Submit(PlayerEvent::PLAY);
    handle = player.Submit(PlayerEvent::STOP);
    handle.wait();
    EXPECT_EQ(player.GetState(), PlayerState::STOP);
}

TEST(FsmUt, RunStopToPlay) {
    FSM<PlayerState, PlayerEvent> player(&g_playerStateTable, &g_playerStateChangeTable, PlayerState::RAW);
    std::future<void> handle;
    handle = player.Submit(PlayerEvent::INIT);
    handle = player.Submit(PlayerEvent::STOP);
    handle = player.Submit(PlayerEvent::PLAY);
    handle.wait();
    EXPECT_EQ(player.GetState(), PlayerState::PLAY);
}

#include <gtest/gtest.h>
#include <functional>
#include <iostream>

#include "fsm/fsm.h"

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
    {PlayerState::RAW, {PlayerState::RAW, doNothing, doNothing, doNothing}},
    {PlayerState::INIT, {PlayerState::INIT, doNothing, doNothing, doNothing}},
    {PlayerState::PLAY, {PlayerState::PLAY, doNothing, doNothing, doNothing}},
    {PlayerState::PAUSE, {PlayerState::PAUSE, doNothing, doNothing, doNothing}},
    {PlayerState::STOP, {PlayerState::STOP, doNothing, doNothing, doNothing}},
};

StateChangeTable<PlayerState, PlayerEvent> g_playerStateChangeTable{
    {PlayerState::RAW, {{PlayerEvent::INIT, PlayerState::INIT}}},
    {PlayerState::INIT, {{PlayerEvent::DESTROY, PlayerState::RAW}, {PlayerEvent::PLAY, PlayerState::PLAY}}},
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
    player.Submit(PlayerEvent::INIT);
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(player.GetState(), PlayerState::INIT);
}

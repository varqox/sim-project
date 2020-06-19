#pragma once

enum class repeating { STOP, CONTINUE };

constexpr auto stop_repeating = repeating::STOP;
constexpr auto continue_repeating = repeating::CONTINUE;

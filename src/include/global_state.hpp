#ifndef GLOBAL_STATE_HPP
#define GLOBAL_STATE_HPP

#include <atomic>

inline std::atomic_bool programRunning{true};

void handleSignal(int signal);

#endif // GLOBAL_STATE_HPP

#pragma once

// Optional local override for developer builds.
// Create `local/Tester.local.h` (ignored by git) and define LIGHTQUEST_ENABLE_TESTER 1 there.
#if __has_include("../../local/Tester.local.h")
#include "../../local/Tester.local.h"
#endif

#ifndef LIGHTQUEST_ENABLE_TESTER
#define LIGHTQUEST_ENABLE_TESTER 0
#endif

inline constexpr bool kTesterEnabledBuild = (LIGHTQUEST_ENABLE_TESTER != 0);

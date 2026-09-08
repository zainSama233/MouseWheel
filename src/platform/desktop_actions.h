#pragma once
#include "platform/windows_injection.h"
namespace wheel::win {
bool executeDesktopAction(const Action& action,HWND target,const KeyState& physical,QString& error);
}

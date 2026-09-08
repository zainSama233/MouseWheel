#pragma once
#include <QString>
#include <Windows.h>
namespace wheel::win {
QString processExecutable(HWND window);
bool isFullscreen(HWND window);
}

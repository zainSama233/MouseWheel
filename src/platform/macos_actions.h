#pragma once
#include "core/model.h"
namespace wheel::mac {
bool fullscreen(int pid);
bool execute(const Action& action,int pid,QString& error);
}

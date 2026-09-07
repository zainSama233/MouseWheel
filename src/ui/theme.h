#pragma once
#include "core/model.h"
#include <QColor>
namespace wheel {
struct ThemeColors {
    QColor background, surface, text, muted, accent, selected, selectedText;
};
ThemeColors themeColors(Theme theme);
QString settingsStyle(Theme theme);
}

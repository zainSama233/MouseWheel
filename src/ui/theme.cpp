#include "ui/theme.h"
#include <QGuiApplication>
#include <QStyleHints>
namespace wheel {
ThemeColors themeColors(Theme theme) {
    if(theme==Theme::System)theme=QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark?Theme::Dark:Theme::Light;
    switch(theme) {
    case Theme::Morandi:return {"#e6e3de","#f0ece6","#555b58","#878c85","#8e9b91","#cbd4ca","#424d44"};
    case Theme::Ocean:return {"#e9f2f4","#f5fbfc","#234b59","#658b98","#217c96","#c5e7ee","#145367"};
    case Theme::Warm: return {"#f5efdf","#fff9ec","#393b2b","#72705f","#65804d","#e3edcf","#344821"};
    case Theme::Dark: return {"#151922","#222936","#edf2f9","#a3afc2","#9bb6ff","#344b77","#ffffff"};
    default: return {"#f4f6fa","#ffffff","#202b3c","#788496","#476dec","#e6edff","#244cbd"};
    }
}
QString settingsStyle(Theme theme) {
    const auto c = themeColors(theme);
    return QStringLiteral(
        "QWidget { background: %1; color: %2; font-family: 'Microsoft YaHei UI'; font-size: 13px; }"
        "QLabel#title { font-size: 22px; font-weight: 600; }"
        "QLabel#section { font-size: 16px; font-weight: 600; }"
        "QLabel#muted,QLabel#save-status { color: %3; }"
        "QLabel#save-status[failed=true] { color: #cf625d; }"
        "QLineEdit,QKeySequenceEdit,QComboBox,QAbstractSpinBox,QPlainTextEdit { background: %4; border: none; border-radius: 7px; padding: 8px; min-height: 20px; }"
        "QComboBox::drop-down { border: none; width: 26px; }"
        "QComboBox::down-arrow { image: url(:/icons/chevron-down.svg); width: 16px; height: 16px; }"
        "QLineEdit:focus,QKeySequenceEdit:focus { background: %5; }"
        "QListWidget { background: %4; alternate-background-color: %1; border: none; }"
        "QListWidget::item { padding: 4px; border-radius: 5px; }"
        "QListWidget::item:selected { background: %5; color: %2; }"
        "QPushButton { background: %4; border: none; border-radius: 7px; padding: 8px 13px; }"
        "QPushButton:hover,QToolButton:hover { background: %5; }"
        "QPushButton:disabled { color: %3; background: transparent; }"
        "QPushButton[navigation=true] { background: transparent; padding: 10px 20px; font-weight: 600; }"
        "QPushButton[navigation=true]:checked { background: %5; color: %2; }"
        "QPushButton:focus,QComboBox:focus,QToolButton:focus { outline: 1px solid %6; }"
        "QToolButton { border: none; border-radius: 6px; padding: 7px; }"
        "QCheckBox { spacing: 8px; min-height: 28px; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %3; min-height: 32px; border-radius: 4px; }"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background: transparent; }"
        "QPushButton#primary { background: %6; color: %1; }"
        "QToolTip { background: %4; color: %2; }")
        .arg(c.background.name(),c.text.name(),c.muted.name(),c.surface.name(),c.selected.name(),c.accent.name());
}
}

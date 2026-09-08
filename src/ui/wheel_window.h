#pragma once
#include <QWidget>
#include <QVariantAnimation>
#include <QIcon>
#include "core/model.h"
namespace wheel {
class WheelWindow final : public QWidget {
    Q_OBJECT
public:
    explicit WheelWindow(bool overlay = true, QWidget* parent = nullptr);
    void present(quint64 session, Config config, Geometry geometry, const QString& screen);
    void changeLevel(quint64 session,int group);
    void select(quint64 session, int index);
    void dismiss(quint64 session);
    void preview(const Config& config,int group=-1);
Q_SIGNALS:
    void slotClicked(int index);
    void slotsSwapped(int source,int target);
    void hidden(quint64 session);
    void firstPaint(quint64 session, qint64 nanoseconds);
protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void paintEvent(QPaintEvent*) override;
    bool nativeEvent(const QByteArray&, void*, qintptr*) override;
private:
    void applyLevel();
    void applyConfig(const Config& config);
    int previewSlotAt(QPointF position) const;
    QVariantAnimation opening_;
    double opacity_=1;
    QList<QIcon> icons_, selectedIcons_;
    int group_=-1;
    int cachedGroup_=-1;
    Config rootConfig_;
    QIcon cancelIcon_;
    QPixmap centerImage_;
    QPointF dragStart_;
    int dragSource_=-1;
    bool cached_=false;
    bool overlay_;
    bool painted_ = false;
    quint64 session_ = 0;
    int selection_ = -1;
    Config config_ = defaultConfig();
};
}

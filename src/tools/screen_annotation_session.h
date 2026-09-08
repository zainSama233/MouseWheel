#pragma once
#include <QObject>
#include <QPointer>
#include <memory>
#include "core/model.h"
#include "tools/annotation_document.h"
class QToolBar;
class QInputDialog;
namespace wheel {
class ScreenOverlay;
class ScreenAnnotationSession final : public QObject {
    Q_OBJECT
public:
    explicit ScreenAnnotationSession(QObject* parent=nullptr);
    ~ScreenAnnotationSession() override;
    void start(Theme theme);
    void stop();
    bool active() const { return !toolbar_.isNull(); }
    bool drawing() const { return active() && drawing_; }
    void setDrawing(bool drawing);
    AnnotationDocument& document() { return *document_; }
Q_SIGNALS:
    void stateChanged();
protected:
    bool eventFilter(QObject*,QEvent*) override;
private:
    void cancelTextInput();
    std::shared_ptr<AnnotationDocument> document_;
    std::shared_ptr<Annotation> style_;
    QList<ScreenOverlay*> overlays_;
    QPointer<QToolBar> toolbar_;
    QPointer<QInputDialog> textInput_;
    bool drawing_=false;
};
}

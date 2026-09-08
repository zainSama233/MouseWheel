#pragma once
#include <QImage>
#include <QString>
namespace wheel {
struct OcrResult { QString text,error; };
OcrResult recognizeLocal(const QImage& image);
}

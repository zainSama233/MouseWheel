#include "tools/local_ocr.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>
namespace wheel {
OcrResult recognizeLocal(const QImage& source) {
    using namespace winrt::Windows;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        struct Apartment { ~Apartment(){winrt::uninit_apartment();} } apartment;
        auto engine=Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();
        if(!engine) return {{},QStringLiteral("未安装可用的 Windows OCR 语言，请在系统语言设置中安装对应语言的文字识别组件。")};
        auto image=source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const int limit=int(Media::Ocr::OcrEngine::MaxImageDimension());
        if(image.width()>limit || image.height()>limit) image=image.scaled(limit,limit,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        if(image.isNull()) return {{},QStringLiteral("识别图片为空。")};
        Storage::Streams::DataWriter writer;
        writer.WriteBytes(winrt::array_view<const uint8_t>(image.constBits(),image.constBits()+image.sizeInBytes()));
        Graphics::Imaging::SoftwareBitmap bitmap(Graphics::Imaging::BitmapPixelFormat::Bgra8,image.width(),image.height(),Graphics::Imaging::BitmapAlphaMode::Premultiplied);
        bitmap.CopyFromBuffer(writer.DetachBuffer());
        const auto result=engine.RecognizeAsync(bitmap).get();
        QStringList lines; for(const auto& line:result.Lines()) lines.append(QString::fromWCharArray(line.Text().c_str()));
        return {lines.join('\n'),{}};
    } catch(const winrt::hresult_error& error) {
        return {{},QStringLiteral("本地 OCR 失败：%1").arg(QString::fromWCharArray(error.message().c_str()))};
    }
}
}

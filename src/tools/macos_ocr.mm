#include "tools/local_ocr.h"
#include <QBuffer>
#import <Vision/Vision.h>
namespace wheel {
OcrResult recognizeLocal(const QImage& image) {
    @autoreleasepool {QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);if(!image.save(&buffer,"PNG"))return {{},QStringLiteral("无法读取识别图像")};
        NSData* data=[NSData dataWithBytes:bytes.constData() length:bytes.size()];
        VNRecognizeTextRequest* request=[[VNRecognizeTextRequest alloc] init];request.recognitionLevel=VNRequestTextRecognitionLevelAccurate;request.usesLanguageCorrection=YES;
        VNImageRequestHandler* handler=[[VNImageRequestHandler alloc] initWithData:data options:@{}];NSError* error=nil;
        if(![handler performRequests:@[request] error:&error])return {{},QString::fromNSString(error.localizedDescription)};
        QStringList lines;for(VNRecognizedTextObservation* observation in request.results){VNRecognizedText* text=[observation topCandidates:1].firstObject;if(text)lines.append(QString::fromNSString(text.string));}
        return {lines.join("\n"),{}};}
}
}

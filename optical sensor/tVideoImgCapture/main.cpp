#include "tvideoimgcapture.h"
#include <QApplication>
#include <QTextCodec>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));

    QFont font;
    font.setPointSize(16);

    font.setFamily("wenquanyi");
//  font.setFamily("WenQuanYi Micro Hei");
    a.setFont(font);

    TVideoImgCapture w;
    w.showMaximized();

    return a.exec();
}

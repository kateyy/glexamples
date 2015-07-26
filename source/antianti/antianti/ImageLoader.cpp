#include "ImageLoader.h"

#include <QGLWidget>
#include <QString>

QImage loadImage(std::string path)
{
    QImage image = QImage(QString(path.c_str()));
    if (image.isNull())
        return{};

    return QGLWidget::convertToGLFormat(image);
}
#pragma once

#include <string>

#include <QImage>

// this file exist to avoid namespace clashes between glbinding and QGLWidget.

QImage loadImage(std::string path);

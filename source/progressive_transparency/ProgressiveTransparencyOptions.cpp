#include "ProgressiveTransparencyOptions.h"

#include <glm/common.hpp>

#include <glbinding/gl/enum.h>

#include <globjects/globjects.h>

#include "ProgressiveTransparency.h"


ProgressiveTransparencyOptions::ProgressiveTransparencyOptions(ProgressiveTransparency & painter)
:   m_painter(painter)
,   m_transparency(0.5f)
,   m_backFaceCulling(false)
{   
    painter.addProperty<float>("transparency", this,
        &ProgressiveTransparencyOptions::transparency, 
        &ProgressiveTransparencyOptions::setTransparency)->setOptions({
        { "minimum", 0.0f },
        { "maximum", 1.0f },
        { "step", 0.1f }});
    
    painter.addProperty<bool>("back_face_culling", this,
        &ProgressiveTransparencyOptions::backFaceCulling, 
        &ProgressiveTransparencyOptions::setBackFaceCulling);
}

ProgressiveTransparencyOptions::~ProgressiveTransparencyOptions() = default;

float ProgressiveTransparencyOptions::transparency() const
{
    return m_transparency;
}

void ProgressiveTransparencyOptions::setTransparency(float transparency)
{
    m_transparency = transparency;
}

bool ProgressiveTransparencyOptions::backFaceCulling() const
{
    return m_backFaceCulling;
}

void ProgressiveTransparencyOptions::setBackFaceCulling(bool b)
{
    m_backFaceCulling = b;
}

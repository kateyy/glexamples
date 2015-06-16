#pragma once

#include <cstdint>

#include <reflectionzeug/PropertyGroup.h>


class ProgressiveTransparency;

class ProgressiveTransparencyOptions
{
public:
    ProgressiveTransparencyOptions(ProgressiveTransparency & painter);
    ~ProgressiveTransparencyOptions();

    float transparency() const;
    void setTransparency(float transparency);
    
    bool backFaceCulling() const;
    void setBackFaceCulling(bool b);

private:
    ProgressiveTransparency & m_painter;

    float m_transparency;
    bool m_backFaceCulling;
};

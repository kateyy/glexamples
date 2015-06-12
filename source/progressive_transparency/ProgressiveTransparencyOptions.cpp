#include "ProgressiveTransparencyOptions.h"

#include <glm/common.hpp>

#include <glbinding/gl/enum.h>

#include <globjects/globjects.h>

#include "ProgressiveTransparency.h"


ProgressiveTransparencyOptions::ProgressiveTransparencyOptions(ProgressiveTransparency & painter)
:   m_painter(painter)
,   m_transparency(160u)
,   m_optimization(StochasticTransparencyOptimization::NoOptimization)
,   m_backFaceCulling(false)
,   m_numSamples(8u)
,   m_numSamplesChanged(true)
{   
    painter.addProperty<unsigned char>("transparency", this,
        &ProgressiveTransparencyOptions::transparency, 
        &ProgressiveTransparencyOptions::setTransparency)->setOptions({
        { "minimum", 0 },
        { "maximum", 255 },
        { "step", 1 }});
    
    painter.addProperty<StochasticTransparencyOptimization>("optimization", this,
        &ProgressiveTransparencyOptions::optimization,
        &ProgressiveTransparencyOptions::setOptimization)->setStrings({
        { StochasticTransparencyOptimization::NoOptimization, "NoOptimization" },
        { StochasticTransparencyOptimization::AlphaCorrection, "AlphaCorrection" },
        { StochasticTransparencyOptimization::AlphaCorrectionAndDepthBased, "AlphaCorrectionAndDepthBased" }});
    
    painter.addProperty<bool>("back_face_culling", this,
        &ProgressiveTransparencyOptions::backFaceCulling, 
        &ProgressiveTransparencyOptions::setBackFaceCulling);
    
    painter.addProperty<uint16_t>("num_samples", this,
        &ProgressiveTransparencyOptions::numSamples,
        &ProgressiveTransparencyOptions::setNumSamples)->setOptions({
        { "minimum", 1u }});
}

ProgressiveTransparencyOptions::~ProgressiveTransparencyOptions() = default;

unsigned char ProgressiveTransparencyOptions::transparency() const
{
    return m_transparency;
}

void ProgressiveTransparencyOptions::setTransparency(unsigned char transparency)
{
    m_transparency = transparency;
}

StochasticTransparencyOptimization ProgressiveTransparencyOptions::optimization() const
{
    return m_optimization;
}

void ProgressiveTransparencyOptions::setOptimization(StochasticTransparencyOptimization optimization)
{
    m_optimization = optimization;
}

bool ProgressiveTransparencyOptions::backFaceCulling() const
{
    return m_backFaceCulling;
}

void ProgressiveTransparencyOptions::setBackFaceCulling(bool b)
{
    m_backFaceCulling = b;
}

uint16_t ProgressiveTransparencyOptions::numSamples() const
{
    return m_numSamples;
}

void ProgressiveTransparencyOptions::setNumSamples(uint16_t numSamples)
{
    m_numSamples = numSamples;
    m_numSamplesChanged = true;
}

bool ProgressiveTransparencyOptions::numSamplesChanged() const
{
    const auto changed = m_numSamplesChanged;
    m_numSamplesChanged = false;
    return changed;
}

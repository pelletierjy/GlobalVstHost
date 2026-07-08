// =====================================================================
// REALTIME CONSTRAINTS (Constitution §V)
// =====================================================================
//   • This file is NOT reached from the audio callback in the current
//     build configuration; it runs exclusively on UI / control threads.
//   • Kept under src/audio-engine/ for cohesion; header satisfies T107.
// =====================================================================
// src/audio-engine/vst-host/plugin_instance.cpp
//
// T036 — PluginInstance implementation.

#include "plugin_instance.h"

#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4996)
#endif

namespace jyglobalvst::engine {

float PluginInstance::getParameter(ParamId param_id) const noexcept
{
    if (!processor_)
        return 0.0f;

    // JUCE's parameter ID mapping is opaque; for now we do a linear search.
    // Future: cache parameter descriptor by ID for performance.
    const auto num_params = processor_->getNumParameters();
    for (int i = 0; i < num_params; ++i)
    {
        // Assume param_id can be cast to int (depends on data-model.md definition).
        if (i == static_cast<int>(param_id))
        {
            return processor_->getParameter(i);
        }
    }
    return 0.0f;
}

void PluginInstance::setParameter(ParamId param_id, float value) noexcept
{
    if (!processor_)
        return;

    const int num_params = processor_->getNumParameters();
    const int idx = static_cast<int>(param_id);
    if (idx >= 0 && idx < num_params)
    {
        processor_->setParameter(idx, value);
    }
}

}  // namespace jyglobalvst::engine

#if defined(_MSC_VER)
#    pragma warning(pop)
#endif

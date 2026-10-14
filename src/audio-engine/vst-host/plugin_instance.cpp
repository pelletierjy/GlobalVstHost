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

    // ParamId is the parameter index (see setParameter); a direct bounds
    // check is all that is needed — the previous linear scan compared the
    // loop counter against the same index and added nothing.
    const int idx = static_cast<int>(param_id);
    const int num_params = processor_->getNumParameters();
    if (idx >= 0 && idx < num_params)
    {
        return processor_->getParameter(idx);
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

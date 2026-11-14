#include "ei_bridge.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

static const float *g_features = 0;

static int get_signal_data(size_t offset, size_t length, float *out_ptr)
{
    if (g_features == 0 || out_ptr == 0) {
        return -1;
    }

    for (size_t i = 0; i < length; ++i) {
        out_ptr[i] = g_features[offset + i];
    }

    return 0;
}

extern "C" int LS_AI_Classify(const float *features, float *sitting, float *standing)
{
    if (features == 0 || sitting == 0 || standing == 0) {
        return -1;
    }

    g_features = features;

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &get_signal_data;

    ei_impulse_result_t result = {};
    EI_IMPULSE_ERROR status = run_classifier(&signal, &result, false);

    if (status != EI_IMPULSE_OK) {
        return static_cast<int>(status);
    }

    *sitting = result.classification[0].value;
    *standing = result.classification[1].value;
    return 0;
}

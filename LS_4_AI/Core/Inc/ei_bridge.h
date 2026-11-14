#ifndef LS_AI_EI_BRIDGE_H
#define LS_AI_EI_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 0 on success; otherwise returns the Edge Impulse error code. */
int LS_AI_Classify(const float *features, float *sitting, float *standing);

#ifdef __cplusplus
}
#endif

#endif

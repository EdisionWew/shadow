#ifndef SHADOW_ACTIVATIONS_HPP
#define SHADOW_ACTIVATIONS_HPP

#include "shadow.pb.h"

#include <string>

static float Activate(float x, shadow::ActivateType a) {
  switch (a) {
  case shadow::ActivateType::Linear:
    return x;
  case shadow::ActivateType::Relu:
    return x * (x > 0);
  case shadow::ActivateType::Leaky:
    return (x > 0) ? x : .1f * x;
  }
  return x;
}

class Activations {
public:
  static void ActivateArray(int N, shadow::ActivateType a, float *out_data) {
    for (int i = 0; i < N; ++i) {
      out_data[i] = Activate(out_data[i], a);
    }
  }
};

#endif // SHADOW_ACTIVATIONS_HPP

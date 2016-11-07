#ifndef SHADOW_LAYERS_ACTIVATE_LAYER_HPP
#define SHADOW_LAYERS_ACTIVATE_LAYER_HPP

#include "shadow/layers/layer.hpp"

class ActivateLayer : public Layer {
 public:
  ActivateLayer() {}
  explicit ActivateLayer(const shadow::LayerParameter &layer_param)
      : Layer(layer_param) {}
  ~ActivateLayer() { Release(); }

  void Setup(VecBlob *blobs);
  void Reshape();
  void Forward();
  void Release();

 private:
  shadow::ActivateParameter::ActivateType activate_type_;
};

#endif  // SHADOW_ACTIVATE_LAYER_HPP
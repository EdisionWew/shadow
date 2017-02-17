#ifndef SHADOW_LAYERS_FLATTEN_LAYER_HPP
#define SHADOW_LAYERS_FLATTEN_LAYER_HPP

#include "shadow/layer.hpp"

class FlattenLayer : public Layer {
 public:
  FlattenLayer() {}
  explicit FlattenLayer(const shadow::LayerParameter &layer_param)
      : Layer(layer_param) {}
  ~FlattenLayer() { Release(); }

  void Setup(VecBlob *blobs);
  void Reshape();
  void Forward();
  void Release();

 private:
  int axis_, end_axis_;
};

#endif  // SHADOW_LAYERS_FLATTEN_LAYER_HPP

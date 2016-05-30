#ifndef SHADOW_NETWORK_HPP
#define SHADOW_NETWORK_HPP

#include "layer.hpp"

#include <string>
#include <vector>

class Network {
public:
  int batch_;
  int in_c_, in_h_, in_w_;
  int in_num_, out_num_;

  int class_num_, grid_size_, sqrt_box_, box_num_;

  int num_layers_;
  std::vector<Layer *> layers_;

  shadow::NetParameter net_param_;

  void LoadModel(std::string cfg_file, std::string weight_file, int batch = 1);

  float *PredictNetwork(float *in_data);
  void ReleaseNetwork();

  int GetNetworkOutputSize();

private:
  float *GetNetworkOutput();

  void ForwardNetwork(float *in_data);

#ifdef USE_CUDA
  void CUDAForwardNetwork(float *in_data);
#endif

#ifdef USE_CL
  void CLForwardNetwork(float *in_data);
#endif
};

#endif // SHADOW_NETWORK_H

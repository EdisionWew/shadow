#include "shadow/util/parser.hpp"

#include "shadow/layers/connected_layer.hpp"
#include "shadow/layers/conv_layer.hpp"
#include "shadow/layers/data_layer.hpp"
#include "shadow/layers/dropout_layer.hpp"
#include "shadow/layers/flatten_layer.hpp"
#include "shadow/layers/permute_layer.hpp"
#include "shadow/layers/pooling_layer.hpp"

#include <google/protobuf/text_format.h>

using google::protobuf::TextFormat;

void Parser::ParseNetworkProtoTxt(Network *net, const std::string proto_txt,
                                  int batch) {
  std::string proto_str = Util::read_text_from_file(proto_txt);
  ParseNetworkProtoStr(net, proto_str, batch);
}

void Parser::ParseNetworkProtoStr(Network *net, const std::string proto_str,
                                  int batch) {
  bool success = TextFormat::ParseFromString(proto_str, &net->net_param_);

  if (!proto_str.compare("") || !success) Fatal("Parse configure file error");

  ParseNet(net);
  std::vector<int> shape(1, batch);
  for (int i = 1; i < net->in_shape_.dim_size(); ++i) {
    shape.push_back(net->in_shape_.dim(i));
  }

  Blob<float> *in_blob = new Blob<float>("in_blob");

  in_blob->set_shape(shape);
  in_blob->allocate_data(in_blob->count());
  net->blobs_.push_back(in_blob);

  for (int i = 0; i < net->num_layers_; ++i) {
    shadow::LayerParameter layer_param = net->net_param_.layer(i);
    Layer *layer = LayerFactory(layer_param, &net->blobs_);
    net->layers_.push_back(layer);
  }
}

void Parser::LoadWeights(Network *net, const std::string weight_file) {
  LoadWeightsUpto(net, weight_file, net->num_layers_);
}

void Parser::LoadWeights(Network *net, const float *weights) {
  LoadWeightsUpto(net, weights, net->num_layers_);
}

void Parser::ParseNet(Network *net) {
  net->num_layers_ = net->net_param_.layer_size();
  net->layers_.reserve(net->num_layers_);
  net->blobs_.reserve(net->num_layers_);

  net->in_shape_ = net->net_param_.input_shape();
  if (!(net->in_shape_.dim(1) && net->in_shape_.dim(2) &&
        net->in_shape_.dim(3)))
    Fatal("No input parameters supplied!");
}

Layer *Parser::LayerFactory(const shadow::LayerParameter &layer_param,
                            VecBlob *blobs) {
  shadow::LayerType layer_type = layer_param.type();
  Layer *layer = nullptr;
  if (layer_type == shadow::LayerType::Data) {
    layer = new DataLayer(layer_param);
  } else if (layer_type == shadow::LayerType::Convolution) {
    layer = new ConvLayer(layer_param);
  } else if (layer_type == shadow::LayerType::Pooling) {
    layer = new PoolingLayer(layer_param);
  } else if (layer_type == shadow::LayerType::Connected) {
    layer = new ConnectedLayer(layer_param);
  } else if (layer_type == shadow::LayerType::Dropout) {
    layer = new DropoutLayer(layer_param);
  } else if (layer_type == shadow::LayerType::Permute) {
    layer = new PermuteLayer(layer_param);
  } else if (layer_type == shadow::LayerType::Flatten) {
    layer = new FlattenLayer(layer_param);
  } else {
    Fatal("Type not recognized!");
  }
  if (layer != nullptr) {
    layer->Setup(blobs);
    layer->Reshape();
  } else {
    Fatal("Error when making layer: " + layer_param.name());
  }
  return layer;
}

void Parser::LoadWeightsUpto(Network *net, const std::string weight_file,
                             int cut_off) {
  DInfo("Load model from " + weight_file + " ... ");

  std::ifstream file(weight_file, std::ios::binary);
  if (!file.is_open()) Fatal("Load weight file error!");

  file.seekg(sizeof(char) * 16, std::ios::beg);

  for (int i = 0; i < net->num_layers_ && i < cut_off; ++i) {
    Layer *layer = net->layers_[i];
    if (layer->layer_param_.type() == shadow::LayerType::Convolution) {
      ConvLayer *l = reinterpret_cast<ConvLayer *>(layer);
      int in_c = l->bottom_[0]->shape(1), out_c = l->top_[0]->shape(1);
      int num = out_c * in_c * l->kernel_size() * l->kernel_size();
      float *biases = new float[out_c], *filters = new float[num];
      file.read(reinterpret_cast<char *>(biases), sizeof(float) * out_c);
      file.read(reinterpret_cast<char *>(filters), sizeof(float) * num);
      l->set_biases(biases);
      l->set_filters(filters);
      delete[] biases;
      delete[] filters;
    }
    if (layer->layer_param_.type() == shadow::LayerType::Connected) {
      ConnectedLayer *l = reinterpret_cast<ConnectedLayer *>(layer);
      int out_num = l->top_[0]->num(), num = l->bottom_[0]->num() * out_num;
      float *biases = new float[out_num], *weights = new float[num];
      file.read(reinterpret_cast<char *>(biases), sizeof(float) * out_num);
      file.read(reinterpret_cast<char *>(weights), sizeof(float) * num);
      l->set_biases(biases);
      l->set_weights(weights);
      delete[] biases;
      delete[] weights;
    }
  }

  file.close();
}

void Parser::LoadWeightsUpto(Network *net, const float *weights, int cut_off) {
  float *index = const_cast<float *>(weights);
  for (int i = 0; i < net->num_layers_ && i < cut_off; ++i) {
    Layer *layer = net->layers_[i];
    if (layer->layer_param_.type() == shadow::LayerType::Convolution) {
      ConvLayer *l = reinterpret_cast<ConvLayer *>(layer);
      int in_c = l->bottom_[0]->shape(1), out_c = l->top_[0]->shape(1);
      int num = out_c * in_c * l->kernel_size() * l->kernel_size();
      l->set_biases(index);
      index += out_c;
      l->set_filters(index);
      index += num;
    }
    if (layer->layer_param_.type() == shadow::LayerType::Connected) {
      ConnectedLayer *l = reinterpret_cast<ConnectedLayer *>(layer);
      int out_num = l->top_[0]->num(), num = l->bottom_[0]->num() * out_num;
      l->set_biases(index);
      index += out_num;
      l->set_weights(index);
      index += num;
    }
  }
}

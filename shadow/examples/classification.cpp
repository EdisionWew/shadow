#include "classification.hpp"

namespace Shadow {

void Classification::Setup(const std::string &model_file) {
  net_.Setup();

#if defined(USE_Protobuf)
  shadow::MetaNetParam meta_net_param;
  CHECK(IO::ReadProtoFromBinaryFile(model_file, &meta_net_param))
      << "Error when loading proto binary file: " << model_file;

  net_.LoadModel(meta_net_param.network(0));

#else
  LOG(FATAL) << "Unsupported load binary model, recompiled with USE_Protobuf";
#endif

  const auto &in_blob = net_.in_blob();
  CHECK_EQ(in_blob.size(), 1);
  in_str_ = in_blob[0];

  const auto &out_blob = net_.out_blob();
  CHECK_EQ(out_blob.size(), 1);
  prob_str_ = out_blob[0];

  auto data_shape = net_.GetBlobByName<float>(in_str_)->shape();
  CHECK_EQ(data_shape.size(), 4);

  batch_ = data_shape[0];
  in_c_ = data_shape[1];
  in_h_ = data_shape[2];
  in_w_ = data_shape[3];
  in_num_ = in_c_ * in_h_ * in_w_;

  in_data_.resize(batch_ * in_num_);

  num_classes_ = net_.get_single_argument<int>("num_classes", 1000);
}

void Classification::Predict(
    const JImage &im_src, const VecRectF &rois,
    std::vector<std::map<std::string, VecFloat>> *scores) {
  CHECK_LE(rois.size(), batch_);
  for (int b = 0; b < rois.size(); ++b) {
    ConvertData(im_src, in_data_.data() + b * in_num_, rois[b], in_c_, in_h_,
                in_w_);
  }

  Process(in_data_, scores);

  CHECK_EQ(scores->size(), rois.size());
}

#if defined(USE_OpenCV)
void Classification::Predict(
    const cv::Mat &im_mat, const VecRectF &rois,
    std::vector<std::map<std::string, VecFloat>> *scores) {
  CHECK_LE(rois.size(), batch_);
  for (int b = 0; b < rois.size(); ++b) {
    ConvertData(im_mat, in_data_.data() + b * in_num_, rois[b], in_c_, in_h_,
                in_w_);
  }

  Process(in_data_, scores);

  CHECK_EQ(scores->size(), rois.size());
}
#endif

void Classification::Release() { net_.Release(); }

void Classification::Process(
    const VecFloat &in_data,
    std::vector<std::map<std::string, VecFloat>> *scores) {
  std::map<std::string, float *> data_map;
  data_map[in_str_] = const_cast<float *>(in_data.data());

  net_.Forward(data_map);

  const auto *prob_data = net_.GetBlobDataByName<float>(prob_str_);

  scores->clear();
  for (int b = 0; b < batch_; ++b) {
    int offset = b * num_classes_;
    std::map<std::string, VecFloat> score_map;
    score_map["score"] = VecFloat(prob_data + offset, prob_data + num_classes_);
    scores->push_back(score_map);
  }
}

}  // namespace Shadow

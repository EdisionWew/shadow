#include "roi_pooling_op.hpp"
#include "core/vision.hpp"

namespace Shadow {

void ROIPoolingOp::Setup() {
  pooled_h_ = get_single_argument<int>("pooled_h", 0);
  pooled_w_ = get_single_argument<int>("pooled_w", 0);
  CHECK_GT(pooled_h_, 0) << "pooled_h must be > 0";
  CHECK_GT(pooled_w_, 0) << "pooled_w must be > 0";
  spatial_scale_ = get_single_argument<float>("spatial_scale", 1.f / 16);
  CHECK_EQ(bottoms_size(), 2);
}

void ROIPoolingOp::Reshape() {
  const auto *bottom_fea = bottoms<float>(0);
  const auto *bottom_roi = bottoms<float>(1);
  auto *top = mutable_tops<float>(0);

  CHECK_NE(bottom_fea, top);

  int in_c = bottom_fea->shape(1), num_rois = bottom_roi->shape(0);

  top->reshape({num_rois, in_c, pooled_h_, pooled_w_});

  VecString str;
  for (int i = 0; i < bottoms_size(); ++i) {
    const auto *bottom = bottoms<float>(i);
    str.push_back(
        Util::format_vector(bottom->shape(), ",", bottom->name() + "(", ")"));
  }
  DLOG(INFO) << op_name_ << "(" << op_type_
             << "): " << Util::format_vector(str, " + ") << " -> "
             << top->name() << Util::format_vector(top->shape(), ",", "(", ")");
}

void ROIPoolingOp::Forward() {
  const auto *bottom_fea = bottoms<float>(0);
  const auto *bottom_roi = bottoms<float>(1);
  auto *top = mutable_tops<float>(0);

  int num_rois = bottom_roi->shape(0);

  Vision::ROIPooling(bottom_fea->data(), bottom_fea->shape(),
                     bottom_roi->data(), num_rois, pooled_h_, pooled_w_,
                     spatial_scale_, top->mutable_data());
}

void ROIPoolingOp::Release() {
  // DLOG(INFO) << "Free ROIPoolingOp!";
}

REGISTER_OPERATOR(ROIPooling, ROIPoolingOp);

}  // namespace Shadow

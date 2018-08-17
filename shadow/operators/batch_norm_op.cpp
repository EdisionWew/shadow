#include "batch_norm_op.hpp"

namespace Shadow {

void BatchNormOp::Forward() {
  const auto *bottom = bottoms<float>(0);
  auto *top = mutable_tops<float>(0);

  int batch = bottom->shape(0), channels, spatial_dim = bottom->count(2);

  if (bottom->num_axes() == 1) {
    channels = 1;
  } else {
    channels = bottom->shape(1);
  }

  if (bottom != top) {
    top->reshape(bottom->shape());
    Blas::BlasScopy(bottom->count(), bottom->data(), 0, top->mutable_data(), 0);
  }

  mean_ = op_ws_->CreateBlob<float>(op_name_ + "_mean");
  variance_ = op_ws_->CreateBlob<float>(op_name_ + "_variance");
  temp_ = op_ws_->CreateBlob<float>(op_name_ + "_temp");
  batch_by_channel_ = op_ws_->CreateBlob<float>(op_name_ + "_batch_by_channel");
  sum_batch_multiplier_ =
      op_ws_->CreateBlob<float>(op_name_ + "_sum_batch_multiplier");
  sum_spatial_multiplier_ =
      op_ws_->CreateBlob<float>(op_name_ + "_sum_spatial_multiplier");

  mean_->reshape({1, channels});
  variance_->reshape({1, channels});
  temp_->reshape(bottom->shape());
  batch_by_channel_->reshape({batch, channels});

  sum_batch_multiplier_->reshape({batch});
  Blas::Set(batch, 1, sum_batch_multiplier_->mutable_data(), 0);

  sum_spatial_multiplier_->reshape({1, 1, spatial_dim});
  Blas::Set(spatial_dim, 1, sum_spatial_multiplier_->mutable_data(), 0);

  if (use_global_stats_) {
    CHECK_EQ(blobs_size(), 3);
    CHECK_EQ(blobs<float>(2)->count(), 1);
    float scale = 1;
    blobs<float>(2)->read_data(&scale, 1);
    float scale_factor = scale == 0 ? 0 : 1 / scale;
    Blas::Mul(mean_->count(), blobs<float>(0)->data(), 0, scale_factor,
              mean_->mutable_data(), 0);
    Blas::Mul(variance_->count(), blobs<float>(1)->data(), 0, scale_factor,
              variance_->mutable_data(), 0);
  }

  if (!use_global_stats_) {
    Blas::BlasSgemv(0, batch * channels, spatial_dim,
                    1.f / (batch * spatial_dim), bottom->data(), 0,
                    sum_spatial_multiplier_->data(), 0, 0,
                    batch_by_channel_->mutable_data(), 0);
    Blas::BlasSgemv(1, batch, channels, 1, batch_by_channel_->data(), 0,
                    sum_batch_multiplier_->data(), 0, 0, mean_->mutable_data(),
                    0);
  }
  Blas::BlasSgemm(0, 0, batch, channels, 1, 1, sum_batch_multiplier_->data(), 0,
                  mean_->data(), 0, 0, batch_by_channel_->mutable_data(), 0);
  Blas::BlasSgemm(0, 0, batch * channels, spatial_dim, 1, -1,
                  batch_by_channel_->data(), 0, sum_spatial_multiplier_->data(),
                  0, 1, top->mutable_data(), 0);

  if (!use_global_stats_) {
    Blas::Pow(top->count(), top->data(), 0, 2, temp_->mutable_data(), 0);
    Blas::BlasSgemv(0, batch * channels, spatial_dim,
                    1.f / (batch * spatial_dim), temp_->data(), 0,
                    sum_spatial_multiplier_->data(), 0, 0,
                    batch_by_channel_->mutable_data(), 0);
    Blas::BlasSgemv(1, batch, channels, 1, batch_by_channel_->data(), 0,
                    sum_batch_multiplier_->data(), 0, 0,
                    variance_->mutable_data(), 0);
  }
  Blas::Add(variance_->count(), variance_->data(), 0, eps_,
            variance_->mutable_data(), 0);
  Blas::Pow(variance_->count(), variance_->data(), 0, 0.5,
            variance_->mutable_data(), 0);
  Blas::BlasSgemm(0, 0, batch, channels, 1, 1, sum_batch_multiplier_->data(), 0,
                  variance_->data(), 0, 0, batch_by_channel_->mutable_data(),
                  0);
  Blas::BlasSgemm(0, 0, batch * channels, spatial_dim, 1, 1,
                  batch_by_channel_->data(), 0, sum_spatial_multiplier_->data(),
                  0, 0, temp_->mutable_data(), 0);
  Blas::Div(top->count(), top->data(), 0, temp_->data(), 0, top->mutable_data(),
            0);

  DLOG(INFO) << debug_log();
}

REGISTER_OPERATOR(BatchNorm, BatchNormOp);

}  // namespace Shadow

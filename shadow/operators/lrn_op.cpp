#include "lrn_op.hpp"

namespace Shadow {

void LRNOp::Forward() {
  const auto bottom = bottoms(0);
  auto top = tops(0);

  if (bottom != top) {
    top->reshape(bottom->shape());
  }

  ws_->GrowTempBuffer(bottom->raw_size());
  auto scale = ws_->CreateTempBlob(bottom->shape(), DataType::kF32);

  Vision::LRN(bottom->data<float>(), bottom->shape(), size_, alpha_, beta_, k_,
              scale->mutable_data<float>(), top->mutable_data<float>(),
              ws_->Ctx());
}

REGISTER_OPERATOR(LRN, LRNOp);

namespace Vision {

#if !defined(USE_CUDA)
template <typename T>
void LRN(const T *in_data, const VecInt &in_shape, int size, float alpha,
         float beta, float k, T *scale_data, T *out_data, Context *context) {
  int batch = in_shape[0], in_c = in_shape[1];
  int in_h = in_shape[2], in_w = in_shape[3];
  int step = in_h * in_w, count = batch * in_c * step;
  int pre_pad = (size - 1) / 2, post_pad = size - pre_pad - 1;
  float alpha_over_size = alpha / size;
  for (int b = 0; b < batch; ++b) {
    for (int h = 0; h < in_h; ++h) {
      for (int w = 0; w < in_w; ++w) {
        int offset = (b * in_c * in_h + h) * in_w + w, head = 0;
        const T *in_off = in_data + offset;
        T *scale_off = scale_data + offset;
        auto accum_scale = T(0);
        while (head < post_pad && head < in_c) {
          accum_scale += in_off[head * step] * in_off[head * step];
          head++;
        }
        while (head < in_c) {
          accum_scale += in_off[head * step] * in_off[head * step];
          if (head - size >= 0) {
            accum_scale -=
                in_off[(head - size) * step] * in_off[(head - size) * step];
          }
          scale_off[(head - post_pad) * step] =
              k + accum_scale * alpha_over_size;
          head++;
        }
        while (head < in_c + post_pad) {
          if (head - size >= 0) {
            accum_scale -=
                in_off[(head - size) * step] * in_off[(head - size) * step];
          }
          scale_off[(head - post_pad) * step] =
              k + accum_scale * alpha_over_size;
          head++;
        }
      }
    }
  }
  for (int i = 0; i < count; ++i) {
    out_data[i] = in_data[i] * std::pow(scale_data[i], -beta);
  }
}

template void LRN(const float *, const VecInt &, int, float, float, float,
                  float *, float *, Context *);
#endif

}  // namespace Vision

}  // namespace Shadow

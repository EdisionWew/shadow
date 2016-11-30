#include "shadow/util/jimage_proc.hpp"
#include "shadow/util/util.hpp"

namespace JImageProc {

VecPointI GetLinePoints(const PointI &start, const PointI &end, const int step,
                        const int slice_axis) {
  PointI start_p(start), end_p(end);

  float delta_x = end_p.x - start_p.x, delta_y = end_p.y - start_p.y;

  if (std::abs(delta_x) < 1 && std::abs(delta_y) < 1)
    return VecPointI(1, start_p);

  bool steep = false;
  if (slice_axis == 1 ||
      (slice_axis == -1 && std::abs(delta_y) > std::abs(delta_x))) {
    steep = true;
    int t = start_p.x;
    start_p.x = start_p.y;
    start_p.y = t;
    t = end_p.x;
    end_p.x = end_p.y;
    end_p.y = t;
  }

  if (start_p.x > end_p.x) {
    PointI t = start_p;
    start_p = end_p;
    end_p = t;
  }

  delta_x = end_p.x - start_p.x, delta_y = end_p.y - start_p.y;

  float step_y = delta_y / delta_x;
  VecPointI points;
  for (int x = start_p.x; x <= end_p.x; x += step) {
    int y = Util::round(start_p.y + (x - start_p.x) * step_y);
    if (steep) {
      points.push_back(PointI(y, x));
    } else {
      points.push_back(PointI(x, y));
    }
  }

  return points;
}

template <typename Dtype>
void Line(JImage *im, const Point<Dtype> &start, const Point<Dtype> &end,
          const Scalar &scalar) {
  CHECK_NOTNULL(im);
  CHECK_NOTNULL(im->data());

  int c_ = im->c_, h_ = im->h_, w_ = im->w_;
  const auto &order_ = im->order();

  if (order_ != kGray && order_ != kRGB && order_ != kBGR) {
    LOG(FATAL) << "Unsupported format to resize!";
  }

  unsigned char *data = im->data();

  int loc_r = 0, loc_g = 1, loc_b = 2;
  if (order_ == kRGB) {
    loc_r = 0;
    loc_g = 1;
    loc_b = 2;
  } else if (order_ == kBGR) {
    loc_r = 2;
    loc_g = 1;
    loc_b = 0;
  }

  unsigned char gray = std::max(std::max(scalar.r, scalar.g), scalar.b);

  const auto &points = GetLinePoints(PointI(start), PointI(end));

  int offset, x, y;
  for (const auto &point : points) {
    x = point.x, y = point.y;
    if (x < 0 || y < 0 || x >= w_ || y >= h_) continue;
    offset = (w_ * y + x) * c_;
    if (order_ == kGray) {
      data[offset] = gray;
    } else {
      data[offset + loc_r] = scalar.r;
      data[offset + loc_g] = scalar.g;
      data[offset + loc_b] = scalar.b;
    }
  }
}

template <typename Dtype>
void Rectangle(JImage *im, const Rect<Dtype> &rect, const Scalar &scalar) {
  CHECK_NOTNULL(im);
  CHECK_NOTNULL(im->data());

  int h_ = im->h_, w_ = im->w_;
  const auto &order_ = im->order();

  if (order_ != kGray && order_ != kRGB && order_ != kBGR) {
    LOG(FATAL) << "Unsupported format to resize!";
  }

  RectI rectI(rect);

  int x1 = Util::constrain(0, w_ - 1, rectI.x);
  int y1 = Util::constrain(0, h_ - 1, rectI.y);
  int x2 = Util::constrain(x1, w_ - 1, x1 + rectI.w);
  int y2 = Util::constrain(y1, h_ - 1, y1 + rectI.h);

  Line(im, PointI(x1, y1), PointI(x2, y1), scalar);
  Line(im, PointI(x1, y1), PointI(x1, y2), scalar);
  Line(im, PointI(x1, y2), PointI(x2, y2), scalar);
  Line(im, PointI(x2, y1), PointI(x2, y2), scalar);
}

void Color2Gray(const JImage &im_src, JImage *im_gray) {
  CHECK_NOTNULL(im_src.data());
  CHECK_NOTNULL(im_gray);

  int c_ = im_src.c_, h_ = im_src.h_, w_ = im_src.w_;
  const auto &order_ = im_src.order();

  im_gray->Reshape(1, h_, w_, kGray);

  const unsigned char *data_src = im_src.data();
  unsigned char *data_gray = im_gray->data();

  if (order_ == kRGB || order_ == kBGR) {
    int index;
    for (int h = 0; h < h_; ++h) {
      for (int w = 0; w < w_; ++w) {
        index = (w_ * h + w) * c_;
        float sum =
            data_src[index + 0] + data_src[index + 1] + data_src[index + 2];
        *data_gray++ = static_cast<unsigned char>(sum / 3.f);
      }
    }
  } else if (order_ == kGray) {
    memcpy(data_gray, data_src, h_ * w_ * sizeof(unsigned char));
  } else {
    LOG(FATAL) << "Unsupported format to convert color to gray!";
  }
}

// Resize and Crop.
void Resize(const JImage &im_src, JImage *im_res, int height, int width) {
  CHECK_NOTNULL(im_src.data());
  CHECK_NOTNULL(im_res);

  int c_ = im_src.c_, h_ = im_src.h_, w_ = im_src.w_;
  const auto &order_ = im_src.order();

  if (order_ != kGray && order_ != kRGB && order_ != kBGR) {
    LOG(FATAL) << "Unsupported format to resize!";
  }

  im_res->Reshape(c_, height, width, order_);

  const unsigned char *data_src = im_src.data();
  unsigned char *data_gray = im_res->data();

  float step_h = static_cast<float>(h_) / im_res->h_;
  float step_w = static_cast<float>(w_) / im_res->w_;
  int s_h, s_w, src_offset, dst_offset;
  int src_step = w_ * c_, dst_step = im_res->w_ * c_;
  for (int c = 0; c < c_; ++c) {
    for (int h = 0; h < im_res->h_; ++h) {
      for (int w = 0; w < im_res->w_; ++w) {
        s_h = static_cast<int>(step_h * h);
        s_w = static_cast<int>(step_w * w);
        src_offset = s_h * src_step + s_w * c_;
        dst_offset = h * dst_step + w * c_;
        data_gray[dst_offset + c] = data_src[src_offset + c];
      }
    }
  }
}

template <typename Dtype>
void Crop(const JImage &im_src, JImage *im_crop, const Rect<Dtype> &crop) {
  CHECK_NOTNULL(im_src.data());
  CHECK_NOTNULL(im_crop);

  int c_ = im_src.c_, h_ = im_src.h_, w_ = im_src.w_;
  const auto &order_ = im_src.order();

  if (order_ != kGray && order_ != kRGB && order_ != kBGR) {
    LOG(FATAL) << "Unsupported format to crop!";
  }
  if (crop.w <= 1 && crop.h <= 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > 1 ||
        crop.y + crop.h > 1) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else if (crop.w > 1 && crop.h > 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > w_ ||
        crop.y + crop.h > h_) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else {
    LOG(FATAL) << "Crop scale must be the same!";
  }

  int height = crop.h <= 1 ? static_cast<int>(crop.h * h_) : crop.h;
  int width = crop.w <= 1 ? static_cast<int>(crop.w * w_) : crop.w;
  im_crop->Reshape(c_, height, width, order_);

  const unsigned char *data_src;
  unsigned char *data_crop;

  int w_off = crop.w <= 1 ? static_cast<int>(crop.x * w_) : crop.x;
  int h_off = crop.h <= 1 ? static_cast<int>(crop.y * h_) : crop.y;
  int src_step = w_ * c_, dst_step = width * c_;
  for (int h = 0; h < im_crop->h_; ++h) {
    data_src = im_src.data() + (h + h_off) * src_step + w_off * c_;
    data_crop = im_crop->data() + h * dst_step;
    memcpy(data_crop, data_src, dst_step * sizeof(unsigned char));
  }
}

template <typename Dtype>
void CropResize(const JImage &im_src, JImage *im_res, const Rect<Dtype> &crop,
                int height, int width) {
  CHECK_NOTNULL(im_src.data());
  CHECK_NOTNULL(im_res);

  int c_ = im_src.c_, h_ = im_src.h_, w_ = im_src.w_;
  const auto &order_ = im_src.order();

  if (order_ != kGray && order_ != kRGB && order_ != kBGR) {
    LOG(FATAL) << "Unsupported format to crop and resize!";
  }
  if (crop.w <= 1 && crop.h <= 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > 1 ||
        crop.y + crop.h > 1) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else if (crop.w > 1 && crop.h > 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > w_ ||
        crop.y + crop.h > h_) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else {
    LOG(FATAL) << "Crop scale must be the same!";
  }

  im_res->Reshape(c_, height, width, order_);

  const unsigned char *data_src = im_src.data();
  unsigned char *data_res = im_res->data();

  float step_h =
      (crop.h <= 1 ? crop.h * h_ : crop.h) / static_cast<float>(height);
  float step_w =
      (crop.w <= 1 ? crop.w * w_ : crop.w) / static_cast<float>(width);
  float h_off = crop.h <= 1 ? crop.y * h_ : crop.y;
  float w_off = crop.w <= 1 ? crop.x * w_ : crop.x;
  int s_h, s_w, src_offset, dst_offset;
  int src_step = w_ * c_, dst_step = width * c_;
  for (int c = 0; c < c_; ++c) {
    for (int h = 0; h < height; ++h) {
      for (int w = 0; w < width; ++w) {
        s_h = static_cast<int>(h_off + step_h * h);
        s_w = static_cast<int>(w_off + step_w * w);
        src_offset = s_h * src_step + s_w * c_;
        dst_offset = h * dst_step + w * c_;
        data_res[dst_offset + c] = data_src[src_offset + c];
      }
    }
  }
}

template <typename Dtype>
void CropResize2Gray(const JImage &im_src, JImage *im_gray,
                     const Rect<Dtype> &crop, int height, int width) {
  CHECK_NOTNULL(im_src.data());
  CHECK_NOTNULL(im_gray);

  int c_ = im_src.c_, h_ = im_src.h_, w_ = im_src.w_;
  const auto &order_ = im_src.order();

  if (order_ != kGray && order_ != kRGB && order_ != kBGR) {
    LOG(FATAL) << "Unsupported format to crop and resize!";
  }
  if (crop.w <= 1 && crop.h <= 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > 1 ||
        crop.y + crop.h > 1) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else if (crop.w > 1 && crop.h > 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > w_ ||
        crop.y + crop.h > h_) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else {
    LOG(FATAL) << "Crop scale must be the same!";
  }

  im_gray->Reshape(1, height, width, kGray);

  const unsigned char *data_src = im_src.data();
  unsigned char *data_gray = im_gray->data();

  float step_h =
      (crop.h <= 1 ? crop.h * h_ : crop.h) / static_cast<float>(height);
  float step_w =
      (crop.w <= 1 ? crop.w * w_ : crop.w) / static_cast<float>(width);
  float h_off = crop.h <= 1 ? crop.y * h_ : crop.y;
  float w_off = crop.w <= 1 ? crop.x * w_ : crop.x;
  float sum;
  int s_h, s_w, src_offset, src_step = w_ * c_;
  if (order_ == kGray) {
    for (int h = 0; h < height; ++h) {
      for (int w = 0; w < width; ++w) {
        s_h = static_cast<int>(h_off + step_h * h);
        s_w = static_cast<int>(w_off + step_w * w);
        src_offset = s_h * src_step + s_w * c_;
        sum = data_src[src_offset];
        *data_gray++ = static_cast<unsigned char>(sum);
      }
    }
  } else {
    for (int h = 0; h < height; ++h) {
      for (int w = 0; w < width; ++w) {
        s_h = static_cast<int>(h_off + step_h * h);
        s_w = static_cast<int>(w_off + step_w * w);
        src_offset = s_h * src_step + s_w * c_;
        sum = data_src[src_offset + 0] + data_src[src_offset + 1] +
              data_src[src_offset + 2];
        *data_gray++ = static_cast<unsigned char>(sum / 3);
      }
    }
  }
}

#ifdef USE_ArcSoft
template <typename Dtype>
void CropResize(const ASVLOFFSCREEN &im_arc, float *batch_data,
                const Rect<Dtype> &crop, int height, int width) {
  CHECK_NOTNULL(batch_data);

  int h_ = im_arc.i32Height, w_ = im_arc.i32Width;

  if (crop.w <= 1 && crop.h <= 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > 1 ||
        crop.y + crop.h > 1) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else if (crop.w > 1 && crop.h > 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > w_ ||
        crop.y + crop.h > h_) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else {
    LOG(FATAL) << "Crop scale must be the same!";
  }

  const unsigned char *src_y = im_arc.ppu8Plane[0];
  const unsigned char *src_u = im_arc.ppu8Plane[1];
  const unsigned char *src_v = im_arc.ppu8Plane[2];
  float *dst_b = batch_data;
  float *dst_g = batch_data + height * width;
  float *dst_r = batch_data + height * width * 2;

  float step_h =
      (crop.h <= 1 ? crop.h * h_ : crop.h) / static_cast<float>(height);
  float step_w =
      (crop.w <= 1 ? crop.w * w_ : crop.w) / static_cast<float>(width);
  float h_off = crop.h <= 1 ? crop.y * h_ : crop.y;
  float w_off = crop.w <= 1 ? crop.x * w_ : crop.x;
  int s_h, s_w, src_step = im_arc.pi32Pitch[0];
  int y, u, v, r, g, b;
  for (int h = 0; h < height; ++h) {
    for (int w = 0; w < width; ++w) {
      s_h = static_cast<int>(h_off + step_h * h);
      s_w = static_cast<int>(w_off + step_w * w);

      y = src_y[s_h * src_step + s_w];
      u = src_u[(s_h >> 1) * (src_step >> 1) + (s_w >> 1)];
      v = src_v[(s_h >> 1) * (src_step >> 1) + (s_w >> 1)];
      u -= 128;
      v -= 128;
      r = y + v + ((v * 103) >> 8);
      g = y - ((u * 88) >> 8) + ((v * 183) >> 8);
      b = y + u + ((u * 198) >> 8);

      *dst_r++ = Util::constrain(0, 255, r);
      *dst_g++ = Util::constrain(0, 255, g);
      *dst_b++ = Util::constrain(0, 255, b);
    }
  }
}

template <typename Dtype>
void CropResize2Gray(const ASVLOFFSCREEN &im_arc, JImage *im_gray,
                     const Rect<Dtype> &crop, int height, int width) {
  CHECK_NOTNULL(im_gray);

  int h_ = im_arc.i32Height, w_ = im_arc.i32Width;

  if (crop.w <= 1 && crop.h <= 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > 1 ||
        crop.y + crop.h > 1) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else if (crop.w > 1 && crop.h > 1) {
    if (crop.x < 0 || crop.y < 0 || crop.x + crop.w > w_ ||
        crop.y + crop.h > h_) {
      LOG(FATAL) << "Crop region overflow!";
    }
  } else {
    LOG(FATAL) << "Crop scale must be the same!";
  }

  im_gray->Reshape(1, height, width, kGray);

  const unsigned char *data_src = im_arc.ppu8Plane[0];
  unsigned char *data_gray = im_gray->data();

  float step_h =
      (crop.h <= 1 ? crop.h * h_ : crop.h) / static_cast<float>(height);
  float step_w =
      (crop.w <= 1 ? crop.w * w_ : crop.w) / static_cast<float>(width);
  float h_off = crop.h <= 1 ? crop.y * h_ : crop.y;
  float w_off = crop.w <= 1 ? crop.x * w_ : crop.x;
  int s_h, s_w, src_step = im_arc.pi32Pitch[0];
  for (int h = 0; h < height; ++h) {
    for (int w = 0; w < width; ++w) {
      s_h = static_cast<int>(h_off + step_h * h);
      s_w = static_cast<int>(w_off + step_w * w);
      *data_gray++ = data_src[s_h * src_step + s_w];
    }
  }
}
#endif

// Filter, Gaussian Blur and Canny.
void Filter1D(const JImage &im_src, JImage *im_filter, const float *kernel,
              int kernel_size, int direction) {
  CHECK_NOTNULL(im_src.data());
  CHECK_NOTNULL(im_filter);

  int c_ = im_src.c_, h_ = im_src.h_, w_ = im_src.w_;
  const auto &order_ = im_src.order();

  im_filter->Reshape(c_, h_, w_, order_);

  const unsigned char *data_src = im_src.data();
  unsigned char *data_filter = im_filter->data();

  float val_c0, val_c1, val_c2, val_kernel;
  int p, p_loc, l_, im_index, center = kernel_size >> 1;

  for (int h = 0; h < h_; ++h) {
    for (int w = 0; w < w_; ++w) {
      val_c0 = 0.f, val_c1 = 0.f, val_c2 = 0.f;
      l_ = !direction ? w_ : h_;
      p = !direction ? w : h;
      for (int i = 0; i < kernel_size; ++i) {
        p_loc = std::abs(p - center + i);
        p_loc = p_loc < l_ ? p_loc : ((l_ << 1) - 1 - p_loc) % l_;
        im_index = !direction ? (w_ * h + p_loc) * c_ : (w_ * p_loc + w) * c_;
        val_kernel = kernel[i];
        val_c0 += data_src[im_index + 0] * val_kernel;
        if (order_ != kGray) {
          val_c1 += data_src[im_index + 1] * val_kernel;
          val_c2 += data_src[im_index + 2] * val_kernel;
        }
      }
      *data_filter++ =
          (unsigned char)Util::constrain(0, 255, static_cast<int>(val_c0));
      if (order_ != kGray) {
        *data_filter++ =
            (unsigned char)Util::constrain(0, 255, static_cast<int>(val_c1));
        *data_filter++ =
            (unsigned char)Util::constrain(0, 255, static_cast<int>(val_c2));
      }
    }
  }
}

void Filter2D(const JImage &im_src, JImage *im_filter, const float *kernel,
              int height, int width) {
  CHECK_NOTNULL(im_src.data());
  CHECK_NOTNULL(im_filter);

  int c_ = im_src.c_, h_ = im_src.h_, w_ = im_src.w_;
  const auto &order_ = im_src.order();

  im_filter->Reshape(c_, h_, w_, order_);

  const unsigned char *data_src = im_src.data();
  unsigned char *data_filter = im_filter->data();

  float val_c0, val_c1, val_c2, val_kernel;
  int im_h, im_w, im_index;

  for (int h = 0; h < h_; ++h) {
    for (int w = 0; w < w_; ++w) {
      val_c0 = 0.f, val_c1 = 0.f, val_c2 = 0.f;
      for (int k_h = 0; k_h < height; ++k_h) {
        for (int k_w = 0; k_w < width; ++k_w) {
          im_h = std::abs(h - (height >> 1) + k_h);
          im_w = std::abs(w - (width >> 1) + k_w);
          im_h = im_h < h_ ? im_h : ((h_ << 1) - 1 - im_h) % h_;
          im_w = im_w < w_ ? im_w : ((w_ << 1) - 1 - im_w) % w_;
          im_index = (w_ * im_h + im_w) * c_;
          val_kernel = kernel[k_h * width + k_w];
          val_c0 += data_src[im_index + 0] * val_kernel;
          if (order_ != kGray) {
            val_c1 += data_src[im_index + 1] * val_kernel;
            val_c2 += data_src[im_index + 2] * val_kernel;
          }
        }
      }
      *data_filter++ =
          (unsigned char)Util::constrain(0, 255, static_cast<int>(val_c0));
      if (order_ != kGray) {
        *data_filter++ =
            (unsigned char)Util::constrain(0, 255, static_cast<int>(val_c1));
        *data_filter++ =
            (unsigned char)Util::constrain(0, 255, static_cast<int>(val_c2));
      }
    }
  }
}

void GaussianBlur(const JImage &im_src, JImage *im_blur, int kernel_size,
                  float sigma) {
  CHECK_NOTNULL(im_src.data());
  CHECK_NOTNULL(im_blur);

  int c_ = im_src.c_, h_ = im_src.h_, w_ = im_src.w_;
  const auto &order_ = im_src.order();

  im_blur->Reshape(c_, h_, w_, order_);

  const unsigned char *data_src = im_src.data();
  unsigned char *data_blur = im_blur->data();

  float *kernel = new float[kernel_size];
  GetGaussianKernel(kernel, kernel_size, sigma);

  float val_c0, val_c1, val_c2, val_kernel;
  int im_h, im_w, im_index, center = kernel_size >> 1;

  float *data_w = new float[c_ * h_ * w_];
  float *data_w_index = data_w;
  for (int h = 0; h < h_; ++h) {
    for (int w = 0; w < w_; ++w) {
      val_c0 = 0.f, val_c1 = 0.f, val_c2 = 0.f;
      for (int k_w = 0; k_w < kernel_size; ++k_w) {
        im_w = std::abs(w - center + k_w);
        im_w = im_w < w_ ? im_w : ((w_ << 1) - 1 - im_w) % w_;
        im_index = (w_ * h + im_w) * c_;
        val_kernel = kernel[k_w];
        val_c0 += data_src[im_index + 0] * val_kernel;
        if (order_ != kGray) {
          val_c1 += data_src[im_index + 1] * val_kernel;
          val_c2 += data_src[im_index + 2] * val_kernel;
        }
      }
      *data_w_index++ = val_c0;
      if (order_ != kGray) {
        *data_w_index++ = val_c1;
        *data_w_index++ = val_c2;
      }
    }
  }

  for (int h = 0; h < h_; ++h) {
    for (int w = 0; w < w_; ++w) {
      val_c0 = 0.f, val_c1 = 0.f, val_c2 = 0.f;
      for (int k_h = 0; k_h < kernel_size; ++k_h) {
        im_h = std::abs(h - center + k_h);
        im_h = im_h < h_ ? im_h : ((h_ << 1) - 1 - im_h) % h_;
        im_index = (w_ * im_h + w) * c_;
        val_kernel = kernel[k_h];
        val_c0 += data_w[im_index + 0] * val_kernel;
        if (order_ != kGray) {
          val_c1 += data_w[im_index + 1] * val_kernel;
          val_c2 += data_w[im_index + 2] * val_kernel;
        }
      }
      *data_blur++ =
          (unsigned char)Util::constrain(0, 255, static_cast<int>(val_c0));
      if (order_ != kGray) {
        *data_blur++ =
            (unsigned char)Util::constrain(0, 255, static_cast<int>(val_c1));
        *data_blur++ =
            (unsigned char)Util::constrain(0, 255, static_cast<int>(val_c2));
      }
    }
  }

  delete[] data_w;
  delete[] kernel;
}

void Canny(const JImage &im_src, JImage *im_canny, float thresh_low,
           float thresh_high, bool L2) {
  CHECK_NOTNULL(im_src.data());

  im_src.CopyTo(im_canny);

  if (im_canny->order() != kGray) im_canny->Color2Gray();

  int h_ = im_canny->h_, w_ = im_canny->w_;
  unsigned char *data_ = im_canny->data();

  int *grad_x = new int[h_ * w_], *grad_y = new int[h_ * w_];
  int *magnitude = new int[h_ * w_];
  Gradient(*im_canny, grad_x, grad_y, magnitude, L2);

  if (L2) {
    if (thresh_low > 0) thresh_low *= thresh_low;
    if (thresh_high > 0) thresh_high *= thresh_high;
  }

//   0 - the pixel might belong to an edge
//   1 - the pixel can not belong to an edge
//   2 - the pixel does belong to an edge

#define CANNY_SHIFT 15
  const int TG22 = static_cast<int>(
      0.4142135623730950488016887242097 * (1 << CANNY_SHIFT) + 0.5);

  memset(im_canny->data(), 0, h_ * w_ * sizeof(unsigned char));
  int index, val, g_x, g_y, x, y, prev_flag, tg22x, tg67x;
  for (int h = 1; h < h_ - 1; ++h) {
    prev_flag = 0;
    for (int w = 1; w < w_ - 1; ++w) {
      index = h * w_ + w;
      val = magnitude[index];
      if (val > thresh_low) {
        g_x = grad_x[index];
        g_y = grad_y[index];
        x = std::abs(g_x);
        y = std::abs(g_y) << CANNY_SHIFT;

        tg22x = x * TG22;
        if (y < tg22x) {
          if (val > magnitude[index - 1] && val >= magnitude[index + 1])
            goto __canny_set;
        } else {
          tg67x = tg22x + (x << (CANNY_SHIFT + 1));
          if (y > tg67x) {
            if (val > magnitude[index - w_] && val >= magnitude[index + w_])
              goto __canny_set;
          } else {
            int s = (g_x ^ g_y) < 0 ? -1 : 1;
            if (val > magnitude[index - w_ - s] &&
                val > magnitude[index + w_ + s])
              goto __canny_set;
          }
        }
      }
      prev_flag = 0;
      data_[index] = 1;
      continue;
    __canny_set:
      if (!prev_flag && val > thresh_high && data_[index - w_] != 2) {
        data_[index] = 2;
        prev_flag = 1;
      } else {
        data_[index] = 0;
      }
    }
  }

  for (int h = 1; h < h_ - 1; ++h) {
    for (int w = 1; w < w_ - 1; ++w) {
      index = h * w_ + w;
      if (data_[index] == 2) {
        if (!data_[index - w_ - 1]) data_[index - w_ - 1] = 2;
        if (!data_[index - w_]) data_[index - w_] = 2;
        if (!data_[index - w_ + 1]) data_[index - w_ + 1] = 2;
        if (!data_[index - 1]) data_[index - 1] = 2;
        if (!data_[index + 1]) data_[index + 1] = 2;
        if (!data_[index + w_ - 1]) data_[index + w_ - 1] = 2;
        if (!data_[index + w_]) data_[index + w_] = 2;
        if (!data_[index + w_ + 1]) data_[index + w_ + 1] = 2;
      }
    }
  }

  unsigned char *data_index = data_;
  for (int i = 0; i < h_ * w_; ++i, ++data_index) {
    *data_index = (unsigned char)-(*data_index >> 1);
  }

  delete[] grad_x;
  delete[] grad_y;
  delete[] magnitude;
}

void GetGaussianKernel(float *kernel, int n, float sigma) {
  const int SMALL_GAUSSIAN_SIZE = 7;
  static const float small_gaussian_tab[][SMALL_GAUSSIAN_SIZE] = {
      {1.f},
      {0.25f, 0.5f, 0.25f},
      {0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f},
      {0.03125f, 0.109375f, 0.21875f, 0.28125f, 0.21875f, 0.109375f, 0.03125f}};

  const float *fixed_kernel =
      n % 2 == 1 && n <= SMALL_GAUSSIAN_SIZE && sigma <= 0
          ? small_gaussian_tab[n >> 1]
          : 0;

  float sigmaX = sigma > 0 ? sigma : ((n - 1) * 0.5f - 1) * 0.3f + 0.8f;
  float scale2X = -0.5f / (sigmaX * sigmaX);

  float sum = 0;
  for (int i = 0; i < n; ++i) {
    float x = i - (n - 1) * 0.5f;
    float t = fixed_kernel ? fixed_kernel[i] : std::exp(scale2X * x * x);
    kernel[i] = t;
    sum += kernel[i];
  }

  sum = 1.f / sum;
  for (int i = 0; i < n; ++i) {
    kernel[i] = kernel[i] * sum;
  }
}

void Gradient(const JImage &im_src, int *grad_x, int *grad_y, int *magnitude,
              bool L2) {
  CHECK_NOTNULL(im_src.data());

  const unsigned char *data_src = im_src.data();
  int h_ = im_src.h_, w_ = im_src.w_;

  JImage *im_gray = nullptr;
  if (im_src.order() != kGray) {
    im_gray = new JImage();
    im_src.CopyTo(im_gray);
    data_src = im_gray->data();
  }

  memset(grad_x, 0, h_ * w_ * sizeof(int));
  memset(grad_y, 0, h_ * w_ * sizeof(int));
  memset(magnitude, 0, h_ * w_ * sizeof(int));

  int index;
  for (int h = 1; h < h_ - 1; ++h) {
    for (int w = 1; w < w_ - 1; ++w) {
      index = h * w_ + w;
      // grad_x[index] = data_src[index + 1] - data_src[index - 1];
      // grad_y[index] = data_src[index + w_] - data_src[index - w_];
      grad_x[index] = data_src[index + w_ + 1] + data_src[index - w_ + 1] +
                      (data_src[index + 1] << 1) - data_src[index + w_ - 1] -
                      data_src[index - w_ - 1] - (data_src[index - 1] << 1);
      grad_y[index] = data_src[index - w_ - 1] + data_src[index - w_ + 1] +
                      (data_src[index - w_] << 1) - data_src[index + w_ - 1] -
                      data_src[index + w_ + 1] - (data_src[index + w_] << 1);
      if (L2) {
        magnitude[index] = static_cast<int>(std::sqrt(
            grad_x[index] * grad_x[index] + grad_y[index] * grad_y[index]));
      } else {
        magnitude[index] = std::abs(grad_x[index]) + std::abs(grad_y[index]);
      }
    }
  }
  if (im_gray != nullptr) {
    im_gray->Release();
  }
}

// Explicit instantiation
template void Line(JImage *im, const PointI &start, const PointI &end,
                   const Scalar &scalar);
template void Line(JImage *im, const PointF &start, const PointF &end,
                   const Scalar &scalar);

template void Rectangle(JImage *im, const RectI &rect, const Scalar &scalar);
template void Rectangle(JImage *im, const RectF &rect, const Scalar &scalar);

template void Crop(const JImage &im_src, JImage *im_crop, const RectI &crop);
template void Crop(const JImage &im_src, JImage *im_crop, const RectF &crop);

template void CropResize(const JImage &im_src, JImage *im_res,
                         const RectI &crop, int height, int width);
template void CropResize(const JImage &im_src, JImage *im_res,
                         const RectF &crop, int height, int width);

template void CropResize2Gray(const JImage &im_src, JImage *im_gray,
                              const RectI &crop, int height, int width);
template void CropResize2Gray(const JImage &im_src, JImage *im_gray,
                              const RectF &crop, int height, int width);
#ifdef USE_ArcSoft
template void CropResize(const ASVLOFFSCREEN &im_arc, float *batch_data,
                         const RectI &crop, int height, int width);
template void CropResize(const ASVLOFFSCREEN &im_arc, float *batch_data,
                         const RectF &crop, int height, int width);
template void CropResize2Gray(const ASVLOFFSCREEN &im_arc, JImage *im_gray,
                              const RectI &crop, int height, int width);
template void CropResize2Gray(const ASVLOFFSCREEN &im_arc, JImage *im_gray,
                              const RectF &crop, int height, int width);
#endif

}  // namespace JImageProc

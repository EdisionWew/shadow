#include "jimage.h"
#include "util.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

struct Scalar {
  unsigned char r, g, b;
};

JImage::JImage() { data_ = nullptr; }

JImage::JImage(int channel, int height, int width, Order order) {
  c_ = channel;
  h_ = height;
  w_ = width;
  order_ = order;
  data_ = new unsigned char[c_ * h_ * w_];
}

JImage::~JImage() { delete[] data_; }

void JImage::Read(std::string im_path) {
  if (data_ != nullptr)
    error("JImage data must be uninitialized!");
  unsigned char *im_data = stbi_load(im_path.c_str(), &w_, &h_, &c_, 3);
  if (im_data == nullptr)
    error("Failed to read image " + im_path);
  order_ = kRGB;
  data_ = new unsigned char[c_ * h_ * w_];
  memcpy(data_, im_data, c_ * h_ * w_);
  delete im_data;
}

void JImage::Write(std::string im_path) {
  if (data_ == nullptr)
    error("JImage data is NULL!");
  int is_ok = -1;
  int step = w_ * c_;
  if (order_ == kRGB) {
    is_ok = stbi_write_png(im_path.c_str(), w_, h_, c_, data_, step);
  } else if (order_ == kBGR) {
    unsigned char *data_inv = new unsigned char[c_ * h_ * w_];
    GetInv(data_inv);
    is_ok = stbi_write_png(im_path.c_str(), w_, h_, c_, data_inv, step);
    delete[] data_inv;
  } else {
    error("Unsupported format to disk!");
  }
  if (!is_ok)
    error("Failed to write image to " + im_path);
}

void JImage::Show(std::string show_name) {
  if (data_ == nullptr)
    error("JImage data is NULL!");
#ifdef USE_OpenCV
  if (order_ == kRGB) {
    unsigned char *data_inv = new unsigned char[c_ * h_ * w_];
    GetInv(data_inv);
    cv::Mat im_mat(h_, w_, CV_8UC3, data_inv);
    cv::imshow(show_name, im_mat);
    cv::waitKey(0);
    delete[] data_inv;
  } else if (order_ == kBGR) {
    cv::Mat im_mat(h_, w_, CV_8UC3, data_);
    cv::imshow(show_name, im_mat);
    cv::waitKey(0);
  } else {
    error("Unsupported format to show!");
  }
#else
  warn("Not compiled with OpenCV, saving image to " + show_name + ".png");
  Write(show_name + ".png");
#endif
}

void JImage::CopyTo(JImage *im_copy) {
  if (data_ == nullptr)
    error("JImage data is NULL!");
  if (im_copy->data_ != nullptr)
    error("Target copy data must be uninitialized!");
  im_copy->c_ = c_;
  im_copy->h_ = h_;
  im_copy->w_ = w_;
  im_copy->order_ = order_;
  im_copy->data_ = new unsigned char[c_ * h_ * w_];
  memcpy(im_copy->data_, data_, c_ * h_ * w_);
}

void JImage::Release() { delete[] data_; }

void JImage::Resize(JImage *im_res, int height, int width) {
  if (data_ == nullptr)
    error("JImage data is NULL!");
  if (im_res->data_ != nullptr && im_res->h_ * im_res->w_ < height * width)
    error("Resize data is initialized and memory is insufficient");
  im_res->c_ = c_;
  im_res->h_ = height;
  im_res->w_ = width;
  im_res->order_ = kRGB;
  if (im_res->data_ == nullptr)
    im_res->data_ = new unsigned char[c_ * height * width];
  if (order_ == kRGB) {
    stbir_resize_uint8(data_, w_, h_, w_ * c_, im_res->data_, im_res->w_,
                       im_res->h_, im_res->w_ * im_res->c_, c_);
  } else if (order_ == kBGR) {
    unsigned char *data_inv = new unsigned char[c_ * h_ * w_];
    GetInv(data_inv);
    stbir_resize_uint8(data_inv, w_, h_, w_ * c_, im_res->data_, im_res->w_,
                       im_res->h_, im_res->w_ * im_res->c_, c_);
    delete[] data_inv;
  } else {
    error("Unsupported format resize!");
  }
}

void JImage::Crop(JImage *im_crop, Box roi) {
  if (data_ == nullptr)
    error("JImage data is NULL!");
  if (im_crop->data_ != nullptr && im_crop->h_ * im_crop->w_ < roi.h * roi.w)
    error("Resize data is initialized and memory is insufficient");
  if (roi.x < 0 || roi.y < 0 || roi.x + roi.w > w_ || roi.y + roi.h > h_)
    error("Crop region overflow!");
  im_crop->c_ = c_;
  im_crop->h_ = static_cast<int>(roi.h);
  im_crop->w_ = static_cast<int>(roi.w);
  im_crop->order_ = order_;
  if (im_crop->data_ == nullptr)
    im_crop->data_ = new unsigned char[c_ * im_crop->h_ * im_crop->w_];
  if (order_ != kRGB && order_ != kBGR) {
    error("Unsupported format to crop!");
  }
  int step_src = w_ * c_;
  int step_crop = im_crop->w_ * c_;
  int w_off = static_cast<int>(roi.x);
  int h_off = static_cast<int>(roi.y);
  for (int c = 0; c < c_; ++c) {
    for (int h = 0; h < im_crop->h_; ++h) {
      for (int w = 0; w < im_crop->w_; ++w) {
        im_crop->data_[h * step_crop + w * c_ + c] =
            data_[(h + h_off) * step_src + (w + w_off) * c_ + c];
      }
    }
  }
}

void JImage::FromI420(unsigned char *src_y, unsigned char *src_u,
                      unsigned char *src_v, int src_h, int src_w,
                      int src_stride) {
  if (data_ != nullptr && h_ * w_ < src_h * src_w)
    error("JImage data is initialized and memory is insufficient");
  c_ = 3;
  h_ = src_h;
  w_ = src_w;
  order_ = kRGB;
  if (data_ == nullptr)
    data_ = new unsigned char[c_ * h_ * w_];
  for (int h = 0; h < h_; ++h) {
    for (int w = 0; w < w_; ++w) {
      int y = src_y[h * src_stride + w];
      int u = src_u[(h >> 1) * (src_stride >> 1) + (w >> 1)];
      int v = src_v[(h >> 1) * (src_stride >> 1) + (w >> 1)];
      u -= 128;
      v -= 128;
      int r = y + v + ((v * 103) >> 8);
      int g = y - ((u * 88) >> 8) + ((v * 183) >> 8);
      int b = y + u + ((u * 198) >> 8);

      int offset = (w_ * h + w) * c_;
      data_[offset + 0] = (unsigned char)constrain(0, 255, r);
      data_[offset + 1] = (unsigned char)constrain(0, 255, g);
      data_[offset + 2] = (unsigned char)constrain(0, 255, b);
    }
  }
}

#ifdef USE_OpenCV
void JImage::FromMat(cv::Mat &im_mat) {
  if (data_ != nullptr && h_ * w_ < im_mat.rows * im_mat.cols)
    error("JImage data is initialized and memory is insufficient");
  c_ = im_mat.channels();
  h_ = im_mat.rows;
  w_ = im_mat.cols;
  order_ = kBGR;
  if (data_ == nullptr)
    data_ = new unsigned char[c_ * h_ * w_];
  memcpy(data_, im_mat.data, c_ * h_ * w_);
}
#endif

void JImage::Rectangle(VecBox &boxes, bool console_show) {
  for (int b = 0; b < boxes.size(); ++b) {
    if (boxes[b].class_index == -1)
      continue;

    Box box = boxes[b];
    int x1 = constrain(0, w_ - 1, static_cast<int>(box.x));
    int y1 = constrain(0, h_ - 1, static_cast<int>(box.y));
    int x2 = constrain(x1, w_ - 1, static_cast<int>(x1 + box.w));
    int y2 = constrain(y1, h_ - 1, static_cast<int>(y1 + box.h));

    Scalar scalar;
    if (box.class_index == 0)
      scalar = {0, 255, 0};
    else
      scalar = {0, 0, 255};

    for (int i = x1; i <= x2; ++i) {
      int offset = (w_ * y1 + i) * c_;
      data_[offset + 0] = scalar.r;
      data_[offset + 1] = scalar.g;
      data_[offset + 2] = scalar.b;
      offset = (w_ * y2 + i) * c_;
      data_[offset + 0] = scalar.r;
      data_[offset + 1] = scalar.g;
      data_[offset + 2] = scalar.b;
    }
    for (int i = y1; i <= y2; ++i) {
      int offset = (w_ * i + x1) * c_;
      data_[offset + 0] = scalar.r;
      data_[offset + 1] = scalar.g;
      data_[offset + 2] = scalar.b;
      offset = (w_ * i + x2) * c_;
      data_[offset + 0] = scalar.r;
      data_[offset + 1] = scalar.g;
      data_[offset + 2] = scalar.b;
    }
    if (console_show) {
      std::cout << "x = " << box.x << ", y = " << box.y << ", w = " << box.w
                << ", h = " << box.h << ", score = " << box.score << std::endl;
    }
  }
}

void JImage::GetBatchData(float *batch_data) {
  if (data_ == nullptr)
    error("JImage data is NULL!");
  int step = w_ * c_;
  int count = 0;
  for (int c = 0; c < c_; ++c) {
    for (int h = 0; h < h_; ++h) {
      for (int w = 0; w < w_; ++w) {
        batch_data[count++] = data_[h * step + w * c_ + c];
      }
    }
  }
}

void JImage::GetInv(unsigned char *im_inv) {
  bool is_rgb2bgr = false;
  if (order_ == kRGB) {
    is_rgb2bgr = true;
  } else if (order_ == kBGR) {
    is_rgb2bgr = false;
  } else {
    error("Unsupported format to inverse!");
  }
  int ch_src, ch_inv;
  int step = w_ * c_;
  for (int c = 0; c < c_; ++c) {
    for (int h = 0; h < h_; ++h) {
      for (int w = 0; w < w_; ++w) {
        ch_src = is_rgb2bgr ? c : c_ - c - 1;
        ch_inv = is_rgb2bgr ? c_ - c - 1 : c;
        im_inv[h * step + w * c_ + ch_inv] = data_[h * step + w * c_ + ch_src];
      }
    }
  }
}

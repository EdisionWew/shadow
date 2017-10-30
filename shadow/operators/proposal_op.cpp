#include "proposal_op.hpp"
#include "core/vision.hpp"

namespace Shadow {

struct RectInfo {
  float xmin, ymin, xmax, ymax, score;
};

inline VecFloat generate_anchors(int base_size, const VecFloat &ratios,
                                 const VecFloat &scales) {
  auto num_ratio = static_cast<int>(ratios.size());
  auto num_scale = static_cast<int>(scales.size());
  VecFloat anchors(4 * num_ratio * num_scale, 0);
  float cx = (base_size - 1) * 0.5f, cy = (base_size - 1) * 0.5f;
  for (int i = 0; i < num_ratio; ++i) {
    float ar = ratios[i];
    float r_w = Util::round(std::sqrt(base_size * base_size / ar));
    float r_h = Util::round(r_w * ar);
    for (int j = 0; j < num_scale; ++j) {
      float scale = scales[j];
      float rs_w = r_w * scale, rs_h = r_h * scale;
      float *anchor = anchors.data() + (i * num_scale + j) * 4;
      anchor[0] = cx - (rs_w - 1) * 0.5f;
      anchor[1] = cy - (rs_h - 1) * 0.5f;
      anchor[2] = cx + (rs_w - 1) * 0.5f;
      anchor[3] = cy + (rs_h - 1) * 0.5f;
    }
  }
  return anchors;
}

inline bool compare_descend(const RectInfo &rect_a, const RectInfo &rect_b) {
  return rect_a.score > rect_b.score;
}

inline float intersection_area(const RectInfo &a, const RectInfo &b) {
  if (a.xmin > b.xmax || a.xmax < b.xmin || a.ymin > b.ymax ||
      a.ymax < b.ymin) {
    return 0.f;
  }
  float inter_width = std::min(a.xmax, b.xmax) - std::max(a.xmin, b.xmin);
  float inter_height = std::min(a.ymax, b.ymax) - std::max(a.ymin, b.ymin);
  return inter_width * inter_height;
}

inline VecInt nms_sorted(const std::vector<RectInfo> &rects,
                         float nms_threshold) {
  const auto num_rects = rects.size();
  VecFloat areas(num_rects, 0);
  for (int n = 0; n < num_rects; ++n) {
    const auto &rect = rects[n];
    areas[n] = (rect.xmax - rect.xmin + 1) * (rect.ymax - rect.ymin + 1);
  }
  VecInt picked;
  for (int n = 0; n < num_rects; ++n) {
    bool keep = true;
    const auto &rect_n = rects[n];
    for (const auto &p : picked) {
      const auto &rect_p = rects[p];
      float inter_area = intersection_area(rect_n, rect_p);
      float union_area = areas[n] + areas[p] - inter_area;
      if (inter_area / union_area > nms_threshold) {
        keep = false;
      }
    }
    if (keep) {
      picked.push_back(n);
    }
  }
  return picked;
}

void ProposalOp::Setup() {
  feat_stride_ = get_single_argument<int>("feat_stride", 16);
  pre_nms_topN_ = 6000;
  post_nms_topN_ = 300;
  min_size_ = 16;
  base_size_ = 16;
  nms_thresh_ = 0.7;
  ratios_ = {0.5f, 1.f, 2.f};
  scales_ = {8.f, 16.f, 32.f};
  const auto &anchors = generate_anchors(base_size_, ratios_, scales_);
  num_anchors_ = static_cast<int>(ratios_.size() * scales_.size());
  anchors_ =
      op_ws_->CreateBlob<float>({num_anchors_, 4}, op_name_ + "_anchors");
  anchors_->set_data(anchors.data(), anchors_->count());
}

void ProposalOp::Reshape() {
  const auto *bottom_score = bottoms<float>(0);
  const auto *bottom_delta = bottoms<float>(1);
  const auto *bottom_info = bottoms<float>(2);
  auto *top = mutable_tops<float>(0);

  int batch = bottom_score->shape(0), in_h = bottom_score->shape(2),
      in_w = bottom_score->shape(3);
  int num_scores = bottom_score->shape(1), num_regs = bottom_delta->shape(1),
      num_info = bottom_info->shape(1);

  CHECK_EQ(batch, 1) << "Only single item batches are supported";
  CHECK_EQ(num_scores, 2 * num_anchors_);
  CHECK_EQ(num_regs, 4 * num_anchors_);
  CHECK_EQ(num_info, 3);

  top->reshape({post_nms_topN_, 5});

  proposals_ = op_ws_->CreateBlob<float>(op_name_ + "_proposals");
  proposals_->reshape({in_h * in_w * num_anchors_, 6});

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

void ProposalOp::Forward() {
  const auto *bottom_score = bottoms<float>(0);
  const auto *bottom_delta = bottoms<float>(1);
  const auto *bottom_info = bottoms<float>(2);
  auto *top = mutable_tops<float>(0);

  Vision::Proposal(anchors_->data(), bottom_score->data(), bottom_delta->data(),
                   bottom_info->data(), bottom_score->shape(), num_anchors_,
                   feat_stride_, min_size_, proposals_->mutable_data());

  const auto *proposal_data = proposals_->cpu_data();

  std::vector<RectInfo> rectangles;
  for (int n = 0; n < proposals_->shape(0); ++n) {
    const auto *proposal_ptr = proposal_data + n * 6;
    if (proposal_ptr[5] > 0) {
      RectInfo rect = {};
      rect.xmin = proposal_ptr[0];
      rect.ymin = proposal_ptr[1];
      rect.xmax = proposal_ptr[2];
      rect.ymax = proposal_ptr[3];
      rect.score = proposal_ptr[4];
      rectangles.push_back(rect);
    }
  }

  std::stable_sort(rectangles.begin(), rectangles.end(), compare_descend);

  if (pre_nms_topN_ > 0 && pre_nms_topN_ < rectangles.size()) {
    rectangles.resize(pre_nms_topN_);
  }

  const auto &picked = nms_sorted(rectangles, nms_thresh_);

  int picked_count = std::min(static_cast<int>(picked.size()), post_nms_topN_);

  selected_rois_.resize(picked_count * 5, 0);
  for (int n = 0; n < picked_count; ++n) {
    int selected_offset = n * 5;
    const auto &rect = rectangles[picked[n]];
    selected_rois_[selected_offset + 0] = 0;
    selected_rois_[selected_offset + 1] = rect.xmin;
    selected_rois_[selected_offset + 2] = rect.ymin;
    selected_rois_[selected_offset + 3] = rect.xmax;
    selected_rois_[selected_offset + 4] = rect.ymax;
  }

  top->reshape({picked_count, 5});
  top->set_data(selected_rois_.data(), selected_rois_.size());
}

void ProposalOp::Release() {
  // DLOG(INFO) << "Free ProposalOp!";
}

REGISTER_OPERATOR(Proposal, ProposalOp);

}  // namespace Shadow
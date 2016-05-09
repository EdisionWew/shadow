#include "boxes.hpp"

float BoxArea(const Box box) { return box.w * box.h; }

float BoxesOverlap(float x1, float w1, float x2, float w2) {
  float left = x1 > x2 ? x1 : x2;
  float right = x1 + w1 < x2 + w2 ? x1 + w1 : x2 + w2;
  return right - left;
}

float BoxesIntersection(const Box box_a, const Box box_b) {
  float width = BoxesOverlap(box_a.x, box_a.w, box_b.x, box_b.w);
  float height = BoxesOverlap(box_a.y, box_a.h, box_b.y, box_b.h);
  if (width < 0 || height < 0)
    return 0;
  return width * height;
}

float BoxesUnion(const Box box_a, const Box box_b) {
  return BoxArea(box_a) + BoxArea(box_b) - BoxesIntersection(box_a, box_b);
}

float Boxes::BoxesIoU(const Box &box_a, const Box &box_b) {
  return BoxesIntersection(box_a, box_b) / BoxesUnion(box_a, box_b);
}

VecBox Boxes::BoxesNMS(const std::vector<VecBox> &Bboxes, float iou_threshold) {
  VecBox boxes;
  for (int i = 0; i < Bboxes.size(); ++i) {
    for (int j = 0; j < Bboxes[i].size(); ++j) {
      boxes.push_back(Bboxes[i][j]);
    }
  }
  for (int i = 0; i < boxes.size(); ++i) {
    if (boxes[i].class_index == -1)
      continue;
    for (int j = i + 1; j < boxes.size(); ++j) {
      if (boxes[j].class_index == -1 ||
          boxes[i].class_index != boxes[j].class_index)
        continue;
      if (BoxesIoU(boxes[i], boxes[j]) > iou_threshold) {
        if (boxes[i].score < boxes[j].score)
          boxes[i].class_index = -1;
        else
          boxes[j].class_index = -1;
        continue;
      }
      float in = BoxesIntersection(boxes[i], boxes[j]);
      float cover_i = in / BoxArea(boxes[i]);
      float cover_j = in / BoxArea(boxes[j]);
      if (cover_i > cover_j && cover_i > 0.7) {
        boxes[i].class_index = -1;
      }
      if (cover_i < cover_j && cover_j > 0.7) {
        boxes[j].class_index = -1;
      }
    }
  }
  return boxes;
}

void MergeBoxes(const Box &oldBox, Box *newBox, float smooth) {
  newBox->x = oldBox.x + (newBox->x - oldBox.x) * smooth;
  newBox->y = oldBox.y + (newBox->y - oldBox.y) * smooth;
  newBox->w = oldBox.w + (newBox->w - oldBox.w) * smooth;
  newBox->h = oldBox.h + (newBox->h - oldBox.h) * smooth;
}

void Boxes::SmoothBoxes(const VecBox &oldBoxes, VecBox *newBoxes,
                        float smooth) {
  for (int i = 0; i < newBoxes->size(); ++i) {
    Box &newBox = (*newBoxes)[i];
    for (int j = 0; j < oldBoxes.size(); ++j) {
      Box oldBox = oldBoxes[j];
      if (BoxesIoU(oldBox, newBox) > 0.7) {
        MergeBoxes(oldBox, &newBox, smooth);
        break;
      }
    }
  }
}

void Boxes::AmendBoxes(std::vector<VecBox> *boxes, int height, int width,
                       VecBox rois) {
  for (int i = 0; i < rois.size(); ++i) {
    for (int b = 0; b < (*boxes)[i].size(); ++b) {
      (*boxes)[i][b].x += rois[i].x * width;
      (*boxes)[i][b].y += rois[i].y * height;
    }
  }
}

void Boxes::SelectRoI(int event, int x, int y, int flags, void *roi) {
  Box *roi_ = reinterpret_cast<Box *>(roi);
  switch (event) {
  case cv::EVENT_LBUTTONDOWN: {
    std::cout << "Mouse Pressed" << std::endl;
    roi_->x = x;
    roi_->y = y;
  }
  case cv::EVENT_LBUTTONUP: {
    std::cout << "Mouse LBUTTON Released" << std::endl;
    roi_->w = x - roi_->x;
    roi_->h = y - roi_->y;
  }
  default:
    return;
  }
}

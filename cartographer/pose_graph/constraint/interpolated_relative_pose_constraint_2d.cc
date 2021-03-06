/*
 * Copyright 2018 The Cartographer Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cartographer/pose_graph/constraint/interpolated_relative_pose_constraint_2d.h"

#include "cartographer/common/make_unique.h"
#include "cartographer/common/utils.h"

namespace cartographer {
namespace pose_graph {
namespace {

void AddPose3DParameters(Pose3D* pose, ceres::Problem* problem) {
  auto transation = pose->mutable_translation();
  auto rotation = pose->mutable_rotation();
  problem->AddParameterBlock(transation->data(), transation->size());
  problem->AddParameterBlock(rotation->data(), rotation->size());
  if (pose->constant()) {
    problem->SetParameterBlockConstant(transation->data());
    problem->SetParameterBlockConstant(rotation->data());
  }
}

void AddPose2DParameters(Pose2D* pose, ceres::Problem* problem) {
  auto pose_2d = pose->mutable_pose_2d();
  problem->AddParameterBlock(pose_2d->data(), pose_2d->size());
  if (pose->constant()) {
    problem->SetParameterBlockConstant(pose_2d->data());
  }
}

}  // namespace

InterpolatedRelativePoseConstraint2D::InterpolatedRelativePoseConstraint2D(
    const ConstraintId& id, const proto::LossFunction& loss_function_proto,
    const proto::InterpolatedRelativePose2D& proto)
    : Constraint(id, loss_function_proto),
      first_start_(proto.first_start()),
      first_end_(proto.first_end()),
      second_(proto.second()),
      cost_(new InterpolatedRelativePoseCost2D(proto.parameters())),
      ceres_cost_(common::make_unique<AutoDiffFunction>(cost_)) {}

void InterpolatedRelativePoseConstraint2D::AddToOptimizer(
    Nodes* nodes, ceres::Problem* problem) const {
  auto first_node_start =
      common::FindOrNull(nodes->pose_2d_nodes, first_start_);
  if (first_node_start == nullptr) {
    LOG(INFO) << "First node (start) was not found in pose_2d_nodes.";
    return;
  }

  auto first_node_end = common::FindOrNull(nodes->pose_2d_nodes, first_end_);
  if (first_node_end == nullptr) {
    LOG(INFO) << "First node (end) was not found in pose_2d_nodes.";
    return;
  }

  auto second_node = common::FindOrNull(nodes->pose_3d_nodes, second_);
  if (second_node == nullptr) {
    LOG(INFO) << "Second node was not found in pose_3d_nodes.";
    return;
  }

  if (first_node_start->constant() && first_node_end->constant() &&
      second_node->constant()) {
    LOG(INFO) << "All nodes are constant, skipping the constraint.";
    return;
  }

  AddPose2DParameters(first_node_start, problem);
  AddPose2DParameters(first_node_end, problem);
  AddPose3DParameters(second_node, problem);
  problem->AddResidualBlock(ceres_cost_.get(), ceres_loss(),
                            first_node_start->mutable_pose_2d()->data(),
                            first_node_end->mutable_pose_2d()->data(),
                            second_node->mutable_translation()->data(),
                            second_node->mutable_rotation()->data());
}

proto::CostFunction InterpolatedRelativePoseConstraint2D::ToCostFunctionProto()
    const {
  proto::CostFunction cost_function;
  auto* interpolated_relative_pose_2d =
      cost_function.mutable_interpolated_relative_pose_2d();
  *interpolated_relative_pose_2d->mutable_first_start() =
      first_start_.ToProto();
  *interpolated_relative_pose_2d->mutable_first_end() = first_end_.ToProto();
  *interpolated_relative_pose_2d->mutable_second() = second_.ToProto();
  *interpolated_relative_pose_2d->mutable_parameters() = cost_->ToProto();
  return cost_function;
}

}  // namespace pose_graph
}  // namespace cartographer

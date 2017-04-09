/**
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "gloo/cuda_allreduce_halving_doubling.h"

#include "gloo/cuda_private.h"
#include "gloo/cuda_workspace.h"

namespace gloo {

template <typename T, typename W>
CudaAllreduceHalvingDoubling<T, W>::CudaAllreduceHalvingDoubling(
    const std::shared_ptr<Context>& context,
    const std::vector<T*>& ptrs,
    const int count,
    const std::vector<cudaStream_t>& streams)
    : Algorithm(context),
      count_(count),
      bytes_(count_ * sizeof(T)),
      chunks_(this->contextSize_),
      chunkSize_((count_ + chunks_ - 1) / chunks_),
      chunkBytes_(chunkSize_ * sizeof(T)),
      steps_(log2(this->contextSize_)),
      fn_(CudaReductionFunction<T>::sum),
      sendOffsets_(steps_),
      recvOffsets_(steps_),
      sendDataBufs_(steps_),
      recvDataBufs_(steps_) {
  auto newStream = true;
  if (streams.size() > 0) {
    GLOO_ENFORCE_EQ(streams.size(), ptrs.size());
    newStream = false;
  }

  for (auto i = 0; i < ptrs.size(); i++) {
    auto ptr = CudaDevicePointer<T>::create(ptrs[i], count_);
    if (newStream) {
      streams_.push_back(CudaStream(ptr.getDeviceID()));
    } else {
      streams_.push_back(CudaStream(ptr.getDeviceID(), streams[i]));
    }
    devicePtrs_.push_back(std::move(ptr));
  }

  // Workspace specific initialization
  init();

  size_t bitmask = 1;
  size_t stepChunkSize = chunkSize_ << (steps_ - 1);
  size_t stepChunkBytes = stepChunkSize * sizeof(T);
  size_t sendOffset = 0;
  size_t recvOffset = 0;
  size_t bufferOffset = 0; // offset into recvBuf_
  for (int i = 0; i < steps_; i++) {
    const auto destRank = (this->context_->rank) ^ bitmask;
    commPairs_.push_back(this->context_->getPair(destRank));
    const auto slot = this->context_->nextSlot();
    sendOffsets_[i] = sendOffset + ((destRank & bitmask) ? stepChunkSize : 0);
    recvOffsets_[i] =
        recvOffset + ((this->context_->rank & bitmask) ? stepChunkSize : 0);
    if (sendOffsets_[i] < count_) {
      sendDataBufs_[i] =
          commPairs_[i].get()->createSendBuffer(slot, *scratch_, bytes_);
    }
    if (recvOffsets_[i] < count_) {
      recvDataBufs_[i] = commPairs_[i].get()->createRecvBuffer(
          slot, &recvBuf_[bufferOffset], stepChunkBytes);
    }
    bufferOffset += stepChunkSize;
    if (this->context_->rank & bitmask) {
      sendOffset += stepChunkSize;
      recvOffset += stepChunkSize;
    }
    bitmask <<= 1;
    stepChunkSize >>= 1;
    stepChunkBytes >>= 1;

    auto notificationSlot = this->context_->nextSlot();
    sendNotificationBufs_.push_back(commPairs_[i].get()->createSendBuffer(
        notificationSlot, &dummy_, sizeof(dummy_)));
    recvNotificationBufs_.push_back(commPairs_[i].get()->createRecvBuffer(
        notificationSlot, &dummy_, sizeof(dummy_)));
  }
}

template <typename T, typename W>
void CudaAllreduceHalvingDoubling<T, W>::run() {
  CudaDeviceGuard guard;
  CudaStream& stream = streams_[0];

  if (localReduceOp_) {
    localReduceOp_->run();
  }

  size_t bufferOffset = 0;
  size_t numItems = chunkSize_ << (steps_ - 1);
  size_t numSending;
  size_t numReceiving;

  // Reduce-scatter
  for (int i = 0; i < steps_; i++) {
    if (sendOffsets_[i] < count_) {
      numSending = sendOffsets_[i] + numItems > count_
          ? count_ - sendOffsets_[i]
          : numItems;
      sendDataBufs_[i]->send(
          sendOffsets_[i] * sizeof(T), numSending * sizeof(T));
    }
    if (recvOffsets_[i] < count_) {
      recvDataBufs_[i]->waitRecv();
      numReceiving = recvOffsets_[i] + numItems > count_
          ? count_ - recvOffsets_[i]
          : numItems;
      auto recvBufAtOffset = recvBuf_.range(bufferOffset, numReceiving);
      auto scratchAtOffset = scratch_.range(recvOffsets_[i], numReceiving);
      fn_->call(scratchAtOffset, recvBufAtOffset, numReceiving, stream);
      stream.wait();
    }
    bufferOffset += numItems;
    sendNotificationBufs_[i]->send();
    numItems >>= 1;
  }

  numItems = chunkSize_;

  // Allgather
  for (int i = steps_ - 1; i >= 0; i--) {
    // verify that destination rank has received and processed this rank's
    // message during the reduce-scatter phase
    recvNotificationBufs_[i]->waitRecv();
    if (recvOffsets_[i] < count_) {
      numSending = recvOffsets_[i] + numItems > count_
          ? count_ - recvOffsets_[i]
          : numItems;
      sendDataBufs_[i]->send(
          recvOffsets_[i] * sizeof(T), numSending * sizeof(T));
    }
    bufferOffset -= numItems;
    if (sendOffsets_[i] < count_) {
      recvDataBufs_[i]->waitRecv();
      numReceiving = sendOffsets_[i] + numItems > count_
          ? count_ - sendOffsets_[i]
          : numItems;
      auto recvBufAtOffset = recvBuf_.range(bufferOffset, numReceiving);
      auto scratchAtOffset = scratch_.range(sendOffsets_[i], numReceiving);
      stream.copyAsync(scratchAtOffset, recvBufAtOffset);
      stream.wait();
    }
    numItems <<= 1;
  }

  if (localBroadcastOp_) {
    localBroadcastOp_->runAsync();
    localBroadcastOp_->wait();
  }
}

template <typename T, typename W>
template <typename U>
void CudaAllreduceHalvingDoubling<T, W>::init(
    typename std::enable_if<
        std::is_same<U, CudaHostWorkspace<T>>::value,
        typename U::Pointer>::type*) {
  // Since reduction is executed on the CPU, the scratch space
  // where they are accumulated is a new host side buffer.
  scratch_ = W::Pointer::alloc(count_);

  // Execute local reduction and broadcast from host.
  // If devicePtrs_.size() == 1 these functions construct an op that
  // executes a memcpy such that scratch_ always holds the result.
  localReduceOp_ = cudaHostReduce(streams_, devicePtrs_, scratch_, fn_);
  localBroadcastOp_ = cudaHostBroadcast(streams_, devicePtrs_, scratch_);

  recvBuf_ = W::Pointer::alloc(count_);
}

template <typename T, typename W>
template <typename U>
void CudaAllreduceHalvingDoubling<T, W>::init(
    typename std::enable_if<
        std::is_same<U, CudaDeviceWorkspace<T>>::value,
        typename U::Pointer>::type*) {
  // Since reduction is executed on the GPU, the scratch space
  // can use an existing input buffer to accumulate.
  auto& ptr = devicePtrs_[0];
  auto count = ptr.getCount();
  scratch_ = CudaDevicePointer<T>::create(*ptr, count);

  // Run local reduction and broadcast on device.
  // When running with a device workspace we intend to never leave the device.
  if (devicePtrs_.size() > 1) {
    localReduceOp_ =
      cudaDeviceReduce(streams_, devicePtrs_, scratch_, fn_, 0, count_);
    localBroadcastOp_ =
      cudaDeviceBroadcast(streams_, devicePtrs_, scratch_, 0, count_);
  }

  // Inbox/outbox must be colocated with scratch buffer to avoid
  // cross device copies while accumulating the reduction.
  {
    CudaDeviceScope scope(scratch_.getDeviceID());
    recvBuf_ = W::Pointer::alloc(count);
  }
}

// Instantiate templates
template class CudaAllreduceHalvingDoubling<float, CudaHostWorkspace<float> >;
template class CudaAllreduceHalvingDoubling<float, CudaDeviceWorkspace<float> >;

} // namespace gloo
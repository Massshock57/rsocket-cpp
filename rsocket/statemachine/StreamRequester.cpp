// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/StreamRequester.h"

namespace rsocket {

void StreamRequester::setRequested(size_t n) {
  VLOG(3) << "Setting allowance to " << n;
  requested_ = true;
  addImplicitAllowance(n);
}

void StreamRequester::request(int64_t signedN) {
  if (signedN <= 0 || consumerClosed()) {
    return;
  }

  const size_t n = signedN;

  if (requested_) {
    generateRequest(n);
    return;
  }

  requested_ = true;

  // We must inform ConsumerBase about an implicit allowance we have requested
  // from the remote end.
  auto const initial = std::min<uint32_t>(n, kMaxRequestN);
  addImplicitAllowance(initial);
  newStream(StreamType::STREAM, initial, std::move(initialPayload_));

  // Pump the remaining allowance into the ConsumerBase _after_ sending the
  // initial request.
  if (n > initial) {
    generateRequest(n - initial);
  }
}

void StreamRequester::cancel() {
  VLOG(5) << "StreamRequester::cancel(requested_=" << requested_ << ")";
  if (consumerClosed()) {
    return;
  }
  cancelConsumer();
  if (requested_) {
    writeCancel();
  }
  removeFromWriter();
}

void StreamRequester::handlePayload(
    Payload&& payload,
    bool complete,
    bool next,
    bool follows) {
  if (!requested_) {
    handleError(std::runtime_error("Haven't sent REQUEST_STREAM yet"));
    return;
  }
  bool finalComplete =
      processFragmentedPayload(std::move(payload), next, complete, follows);

  if (finalComplete) {
    completeConsumer();
    removeFromWriter();
  }
}

void StreamRequester::handleError(folly::exception_wrapper ew) {
  errorConsumer(std::move(ew));
  removeFromWriter();
}

} // namespace rsocket

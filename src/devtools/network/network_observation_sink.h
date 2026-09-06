#pragma once

#include "devtools/network/network_trace.h"

namespace openbrowser::devtools::network {

class NetworkObservationSink {
public:
    virtual ~NetworkObservationSink() = default;

    virtual void OnNetworkEvent(NetworkEvent event) = 0;
};

}  // namespace openbrowser::devtools::network

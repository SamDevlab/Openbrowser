#pragma once

namespace openbrowser::devtools::network {

struct NetworkEvent;

class NetworkObservationSink {
public:
    virtual ~NetworkObservationSink() = default;

    virtual void OnNetworkEvent(NetworkEvent event) = 0;
};

}  // namespace openbrowser::devtools::network

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <iostream>
#include <string>

// Include WebRTC headers
#include <api/peer_connection_interface.h>
#include <api/create_peerconnection_factory.h>
#include <api/data_channel_interface.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <rtc_base/logging.h>

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class DummySetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
public:
    static rtc::scoped_refptr<DummySetSessionDescriptionObserver> Create() {
        return rtc::scoped_refptr<DummySetSessionDescriptionObserver>(
            new rtc::RefCountedObject<DummySetSessionDescriptionObserver>());
    }
    virtual void OnSuccess() { std::cout << "Set session description success" << std::endl; }
    virtual void OnFailure(webrtc::RTCError error) { std::cout << "Set session description error: " << error.message() << std::endl; }
protected:
    DummySetSessionDescriptionObserver() {}
    ~DummySetSessionDescriptionObserver() {}
};

class DataChannelObserver : public webrtc::DataChannelObserver {
public:
    void OnStateChange() override {
        std::cout << "Data channel state changed" << std::endl;
    }

    void OnMessage(const webrtc::DataBuffer& buffer) override {
        std::string message(buffer.data.data<char>(), buffer.data.size());
        std::cout << "Received message from data channel: " << message << std::endl;
    }

    void OnBufferedAmountChange(uint64_t previous_amount) override {}

    void SetDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
        data_channel_ = data_channel;
    }

    void SendMessage(const std::string& message) {
        if (data_channel_ && data_channel_->state() == webrtc::DataChannelInterface::kOpen) {
            webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(message.c_str(), message.length()), true);
            data_channel_->Send(buffer);
            std::cout << "Sent message through data channel: " << message << std::endl;
        } else {
            std::cerr << "Data channel is not open. Cannot send message." << std::endl;
        }
    }

private:
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
};

class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
public:
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {
        std::cout << "Signaling state changed to: " << new_state << std::endl;
    }

    void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {
        std::cout << "Stream added" << std::endl;
    }

    void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {
        std::cout << "Stream removed" << std::endl;
    }

    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {
        std::cout << "Data channel created" << std::endl;
    }

    void OnRenegotiationNeeded() override {
        std::cout << "Renegotiation needed" << std::endl;
    }

    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
        std::cout << "ICE connection state changed to: " << new_state << std::endl;
    }

    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
        std::cout << "ICE gathering state changed to: " << new_state << std::endl;
    }

    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
        std::cout << "New ICE candidate" << std::endl;
    }
};

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;
std::unique_ptr<DataChannelObserver> data_channel_observer;
std::unique_ptr<PeerConnectionObserver> peer_connection_observer;

// Callback for when the connection is opened
void on_open(client* c, websocketpp::connection_hdl hdl) {
    // Initialize SSL
    rtc::InitializeSSL();

    // Set WebRTC logging
    rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

    // Create threads
    std::unique_ptr<rtc::Thread> network_thread = rtc::Thread::CreateWithSocketServer();
    network_thread->Start();
    std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
    worker_thread->Start();
    std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
    signaling_thread->Start();

    // Create PeerConnectionFactory
    peer_connection_factory = webrtc::CreatePeerConnectionFactory(
        network_thread.get(),
        worker_thread.get(),
        signaling_thread.get(),
        nullptr,  // default_adm
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        nullptr,  // video encoder factory
        nullptr,  // video decoder factory
        nullptr,  // audio_mixer
        nullptr   // audio_processing
    );

    if (!peer_connection_factory) {
        std::cerr << "Failed to create peer connection factory" << std::endl;
        return;
    }

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

    // Add ICE servers (STUN/TURN) to the configuration
    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun.l.google.com:19302";
    config.servers.push_back(ice_server);

    // Create PeerConnectionObserver
    peer_connection_observer = std::make_unique<PeerConnectionObserver>();

    // Create PeerConnectionDependencies with the observer
    webrtc::PeerConnectionDependencies dependencies(peer_connection_observer.get());

    // Create the peer connection with the updated configuration and dependencies
    auto result = peer_connection_factory->CreatePeerConnectionOrError(
        config,
        std::move(dependencies)
    );

    if (!result.ok()) {
        std::cerr << "Failed to create peer connection with ICE servers: " << result.error().message() << std::endl;
        rtc::CleanupSSL();
        return;
    }

    peer_connection = result.value();

    std::cout << "Peer connection created successfully" << std::endl;
  

    // Create data channel observer
    data_channel_observer = std::make_unique<DataChannelObserver>();

    // Create an SDP offer
    class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
    public:
        void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
            std::string sdp;
            desc->ToString(&sdp);
            peer_connection->SetLocalDescription(DummySetSessionDescriptionObserver::Create().get(), desc);
            c->send(hdl, sdp, websocketpp::frame::opcode::text);
            std::cout << "Sent SDP offer: " << sdp << std::endl;
        }

        void OnFailure(webrtc::RTCError error) override {
            std::cerr << "Error creating offer: " << error.message() << std::endl;
        }

        client* c;
        websocketpp::connection_hdl hdl;
    };

    rtc::scoped_refptr<CreateSessionDescriptionObserver> observer(new rtc::RefCountedObject<CreateSessionDescriptionObserver>());
    observer->c = c;
    observer->hdl = hdl;

    peer_connection->CreateOffer(
        observer.get(),
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions()
    );

    // Add a delay to ensure the SDP offer is created and sent
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check if the SDP offer was sent
    if (observer->c == nullptr || observer->hdl.expired()) {
        std::cerr << "SDP offer was not sent. Connection might be closed or invalid." << std::endl;
    }
}

// Callback for when a message is received
void on_message(websocketpp::connection_hdl, client::message_ptr msg) {
    std::string payload = msg->get_payload();
    std::cout << "Received SDP answer: " << payload << std::endl;

    // Handle the SDP answer
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, payload, &error);

    if (!session_description) {
        std::cerr << "Failed to parse SDP answer: " << error.description << std::endl;
        return;
    }

    // Set the remote description on the WebRTC peer connection
    peer_connection->SetRemoteDescription(
        DummySetSessionDescriptionObserver::Create().get(),
        session_description.release());

    // After setting remote description, we can access the data channel
    auto data_channel = peer_connection->CreateDataChannel("data_channel", nullptr);
    if (data_channel) {
        data_channel_observer->SetDataChannel(data_channel);
        data_channel->RegisterObserver(data_channel_observer.get());

        // Send a test message through the data channel
        data_channel_observer->SendMessage("Hello from client!");
    } else {
        std::cerr << "Failed to create data channel" << std::endl;
    }
}

int main() {
    client c;

    try {
        // Set logging to be pretty verbose (everything except message payloads)
        c.set_access_channels(websocketpp::log::alevel::all);
        c.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize ASIO
        c.init_asio();

        // Register our message handler
        c.set_message_handler(bind(&on_message, ::_1, ::_2));

        // Create a connection to the given URI
        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection("ws://localhost:9002", ec);
        if (ec) {
            std::cout << "could not create connection because: " << ec.message() << std::endl;
            return 1;
        }

        // Note that connect here only requests a connection. No network messages are
        // exchanged until the event loop starts running in the next line.
        c.connect(con);

        // Set the open handler
        con->set_open_handler(bind(&on_open, &c, ::_1));

        // Start the ASIO io_service run loop
        c.run();
    } catch (websocketpp::exception const & e) {
        std::cout << e.what() << std::endl;
    }

    return 0;
}

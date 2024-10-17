#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>
#include <string>
#include <functional>

// Include WebRTC headers
#include <api/peer_connection_interface.h>
#include <api/create_peerconnection_factory.h>
#include <api/data_channel_interface.h>
#include <rtc_base/thread.h>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
public:
    static rtc::scoped_refptr<SetSessionDescriptionObserver> Create() {
        return rtc::scoped_refptr<SetSessionDescriptionObserver>(new rtc::RefCountedObject<SetSessionDescriptionObserver>());
    }
    virtual void OnSuccess() { std::cout << "Set session description success" << std::endl; }
    virtual void OnFailure(webrtc::RTCError error) { std::cout << "Set session description error: " << error.message() << std::endl; }
protected:
    SetSessionDescriptionObserver() {}
    ~SetSessionDescriptionObserver() {}
};

class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
public:
    static rtc::scoped_refptr<CreateSessionDescriptionObserver> Create(
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc,
        server* s,
        websocketpp::connection_hdl hdl) {
        return rtc::scoped_refptr<CreateSessionDescriptionObserver>(
            new rtc::RefCountedObject<CreateSessionDescriptionObserver>(pc, s, hdl));
    }
    virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
        std::string sdp;
        desc->ToString(&sdp);
        pc_->SetLocalDescription(SetSessionDescriptionObserver::Create().get(), desc);
        s_->send(hdl_, sdp, websocketpp::frame::opcode::text);
        std::cout << "Sent SDP answer to client" << std::endl;
    }
    virtual void OnFailure(webrtc::RTCError error) override {
        std::cout << "Create session description error: " << error.message() << std::endl;
    }
protected:
    CreateSessionDescriptionObserver(
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc,
        server* s,
        websocketpp::connection_hdl hdl)
        : pc_(pc), s_(s), hdl_(hdl) {}
private:
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_;
    server* s_;
    websocketpp::connection_hdl hdl_;
};

class DataChannelObserver : public webrtc::DataChannelObserver {
public:
    void OnStateChange() override {
        std::cout << "Data channel state changed" << std::endl;
    }

    void OnMessage(const webrtc::DataBuffer& buffer) override {
        std::string message(buffer.data.data<char>(), buffer.data.size());
        std::cout << "Received message from data channel: " << message << std::endl;
        
        // Echo the message back
        webrtc::DataBuffer response(rtc::CopyOnWriteBuffer(message.c_str(), message.length()), true);
        data_channel_->Send(response);
    }

    void OnBufferedAmountChange(uint64_t previous_amount) override {}

    void SetDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
        data_channel_ = data_channel;
    }

private:
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
};

// Define a callback to handle incoming messages
void on_message(server* s, websocketpp::connection_hdl hdl, server::message_ptr msg) {
    std::string payload = msg->get_payload();
    std::cout << "Received SDP offer from client: " << payload << std::endl;

    // Create PeerConnectionFactory
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory =
        webrtc::CreatePeerConnectionFactory(
            nullptr /* network_thread */,
            nullptr /* worker_thread */,
            nullptr /* signaling_thread */,
            nullptr /* default_adm */,
            nullptr /* audio_encoder_factory */,
            nullptr /* audio_decoder_factory */,
            nullptr /* video_encoder_factory */,
            nullptr /* video_decoder_factory */,
            nullptr /* audio_mixer */,
            nullptr /* audio_processing */
        );

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection =
        peer_connection_factory->CreatePeerConnection(config, nullptr, nullptr, nullptr);

    if (!peer_connection) {
        std::cerr << "Failed to create peer connection" << std::endl;
        return;
    }

    // Create and register data channel observer
    DataChannelObserver* data_observer = new DataChannelObserver();

    // Create data channel
    webrtc::DataChannelInit data_channel_config;
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel =
        peer_connection->CreateDataChannel("ServerDataChannel", &data_channel_config);
    data_observer->SetDataChannel(data_channel);
    data_channel->RegisterObserver(data_observer);

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, payload, &error);

    if (!session_description) {
        std::cerr << "Failed to parse SDP offer: " << error.description << std::endl;
        return;
    }

    peer_connection->SetRemoteDescription(
        SetSessionDescriptionObserver::Create().get(),
        session_description.release());

    std::cout << "Set remote description (offer) successfully" << std::endl;

    peer_connection->CreateAnswer(
        CreateSessionDescriptionObserver::Create(peer_connection, s, hdl).get(),
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());

    std::cout << "Creating answer..." << std::endl;
}

int main() {
    // Create a server endpoint
    server echo_server;

    try {
        // Set logging settings
        echo_server.set_access_channels(websocketpp::log::alevel::all);
        echo_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize Asio
        echo_server.init_asio();

        // Register our message handler
        echo_server.set_message_handler(bind(&on_message, &echo_server, ::_1, ::_2));

        // Listen on port 9002
        echo_server.listen(9002);

        std::cout << "WebSocket server listening on port 9002" << std::endl;

        // Start the server accept loop
        echo_server.start_accept();

        // Start the ASIO io_service run loop
        echo_server.run();
    } catch (websocketpp::exception const & e) {
        std::cout << "WebSocket error: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unknown error occurred" << std::endl;
    }

    return 0;
}

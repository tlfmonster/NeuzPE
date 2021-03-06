#include "GUI.h"

#include <iostream>
#include "util.h"
#include "Net.h"
#include "PacketTemplate.h"


void do_send_packet(GUI* gui) {
	auto optstr = gui->pfm.packet_textbox.getline(0);
	if (optstr.has_value()) {
		auto pt = PacketTemplate(optstr.value());
		auto pkt_to_send = pt.GenerateBinary();
		auto sock = Net::GetCClientSockForName(PacketTemplate::ServerToString(pt.server));
		if (sock != nullptr) {
#ifdef NEUZPE_DEBUG_LOG
			std::cout << "Inject button clicked, sending: " << Util::bytes_to_hex_string(pkt_to_send) << std::endl;
#endif
			Net::original_dosend(sock, pkt_to_send.data(), pkt_to_send.size(), 1);
		}
	}
}

void do_spam_packet(GUI* gui) {
	int delay_ms = 1000;
	try {
		delay_ms = gui->pfm.spam_delay_textbox.to_int();
	}
	catch (...) {
		gui->pfm.spam_delay_textbox.reset(std::to_string(delay_ms));
	}

	while (gui->spamming_packet) {
		do_send_packet(gui);
		std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
	}
}

GUI::GUI()
{
	spamming_packet = false;

	// Select packet from list
;	pfm.packet_listbox.events().selected([this](const arg_listbox& arg) {
		if(!spamming_packet) {
			const PacketTemplate* ptr = arg.item.value_ptr<PacketTemplate>();
			this->pfm.packet_textbox.reset(const_cast<PacketTemplate*>(ptr)->GenerateString());
		}
	});

	// Clear button
	pfm.clear_button.events().click([this] {
		this->pfm.packet_listbox.clear();
	});

	// Send
	pfm.inject_button.events().click([this] {
		do_send_packet(this);
	});

	// Spam checkbox clicked
	pfm.spam_checkbox.events().click([this] {
		if (this->pfm.spam_checkbox.checked()) {
			this->spamming_packet = true;
			this->pfm.packet_textbox.editable(false);
			this->pfm.spam_delay_textbox.editable(false);

			auto t = std::thread(do_spam_packet, this);
			t.detach(); // You're free, go!
		} else {
			this->spamming_packet = false;
			this->pfm.packet_textbox.editable(true);
			this->pfm.spam_delay_textbox.editable(true);
		}
	});

	pfm.show();
}

GUI* GUI::instance = nullptr;
GUI* GUI::Get()
{
	if(instance == nullptr){
		instance = new GUI();
	}
	return instance;
}


void GUI::Exec() {
	exec();
}

// Checks if a nana::listbox is scrolled to the bottom
// From "Error Flynn" @ http://nanapro.org/en-us/forum/index.php?u=/topic/1098/ggget-listbox-current-scroll
bool scroll_at_bottom(const nana::listbox &lb)
{
	auto cat_proxy = lb.at(lb.size_categ() - 1);
	if (cat_proxy.size() > 0)
	{
		return cat_proxy.back().displayed();
	}
	else
	{
		return true;
	}
}

void GUI::LogPacket(Net::CClientSock * sock, std::vector<uint8_t> pkt) {


	auto pfm = &GUI::Get()->pfm;

	// Check if we should log this packet, early return if not.
	auto servType = PacketTemplate::StringToServer(Net::GetNameForCClientSock(sock));
	switch (servType) {
	case PacketTemplate::Server::AUTH:
		if (!pfm->log_server_auth.checked()) {
			return;
		}
		break;
	case PacketTemplate::Server::LOGIN:
		if (!pfm->log_server_login.checked()) {
			return;
		}
		break;
	case PacketTemplate::Server::WORLD:
		if (!pfm->log_server_world.checked()) {
			return;
		}
		break;
	}

	bool previously_at_bottom = scroll_at_bottom(pfm->packet_listbox);

	// Make my object
	PacketTemplate pt = PacketTemplate(sock, pkt, PacketTemplate::Direction::SEND);

	// Append object to the listbox
	auto cat = pfm->packet_listbox.at(0);
	cat.append(pt, true);

	// Scroll to bottom only if we were previously at the bottom
	if (previously_at_bottom) {
		pfm->packet_listbox.scroll(true);
	}

}
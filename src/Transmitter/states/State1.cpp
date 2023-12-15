#include <states/base/State.hpp>

#include <chrono>
#include <thread>

#include <Transmitter.hpp>

namespace states
{

bool State1::can_transition_to(AppState state)
{
	switch (state) {
	case AppState::STATE_1:
	case AppState::STATE_2:
	case AppState::STATE_3:
		return true;

	default:
		return false;
	}
}

void State1::on_enter() {}

void State1::on_exit() {}

void State1::run()
{
	LOG("Sending BT5 message pack");
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	struct ODID_UAS_Data uasData = {};
	struct ODID_MessagePack_encoded encoded = {};

	// Testing
	uasData.BasicID[0].UAType = ODID_UATYPE_CAPTIVE_BALLOON;

	create_message_pack(&uasData, &encoded);

	int msg_counter = 1;
	int set = 1; // set=1 BT5
	_parent->bluetooth()->hci_le_set_extended_advertising_data_pack(set, &encoded, msg_counter);
}

void State1::create_message_pack(struct ODID_UAS_Data* uasData, struct ODID_MessagePack_encoded* pack_enc)
{
	union ODID_Message_encoded encoded = { 0 };
	ODID_MessagePack_data pack_data = { 0 };
	pack_data.SingleMessageSize = ODID_MESSAGE_SIZE;
	pack_data.MsgPackSize = 9;

	if (encodeBasicIDMessage((ODID_BasicID_encoded*) &encoded, &uasData->BasicID[0]) != ODID_SUCCESS)
		printf("Error: Failed to encode Basic ID\n");

	memcpy(&pack_data.Messages[0], &encoded, ODID_MESSAGE_SIZE);

	if (encodeBasicIDMessage((ODID_BasicID_encoded*) &encoded, &uasData->BasicID[1]) != ODID_SUCCESS)
		printf("Error: Failed to encode Basic ID\n");

	memcpy(&pack_data.Messages[1], &encoded, ODID_MESSAGE_SIZE);

	if (encodeLocationMessage((ODID_Location_encoded*) &encoded, &uasData->Location) != ODID_SUCCESS)
		printf("Error: Failed to encode Location\n");

	memcpy(&pack_data.Messages[2], &encoded, ODID_MESSAGE_SIZE);

	if (encodeAuthMessage((ODID_Auth_encoded*) &encoded, &uasData->Auth[0]) != ODID_SUCCESS)
		printf("Error: Failed to encode Auth 0\n");

	memcpy(&pack_data.Messages[3], &encoded, ODID_MESSAGE_SIZE);

	if (encodeAuthMessage((ODID_Auth_encoded*) &encoded, &uasData->Auth[1]) != ODID_SUCCESS)
		printf("Error: Failed to encode Auth 1\n");

	memcpy(&pack_data.Messages[4], &encoded, ODID_MESSAGE_SIZE);

	if (encodeAuthMessage((ODID_Auth_encoded*) &encoded, &uasData->Auth[2]) != ODID_SUCCESS)
		printf("Error: Failed to encode Auth 2\n");

	memcpy(&pack_data.Messages[5], &encoded, ODID_MESSAGE_SIZE);

	if (encodeSelfIDMessage((ODID_SelfID_encoded*) &encoded, &uasData->SelfID) != ODID_SUCCESS)
		printf("Error: Failed to encode Self ID\n");

	memcpy(&pack_data.Messages[6], &encoded, ODID_MESSAGE_SIZE);

	if (encodeSystemMessage((ODID_System_encoded*) &encoded, &uasData->System) != ODID_SUCCESS)
		printf("Error: Failed to encode System\n");

	memcpy(&pack_data.Messages[7], &encoded, ODID_MESSAGE_SIZE);

	if (encodeOperatorIDMessage((ODID_OperatorID_encoded*) &encoded, &uasData->OperatorID) != ODID_SUCCESS)
		printf("Error: Failed to encode Operator ID\n");

	memcpy(&pack_data.Messages[8], &encoded, ODID_MESSAGE_SIZE);

	if (encodeMessagePack(pack_enc, &pack_data) != ODID_SUCCESS)
		printf("Error: Failed to encode message pack_data\n");
}


} // end namespace states

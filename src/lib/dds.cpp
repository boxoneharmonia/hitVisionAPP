#include "dds.hpp"

#define SYNC_HEAD 0xEB90
#define DATA_LENGTH 33
#define APP_MAIN_ID 0x01
#define APP_SUB_ID 0x11
#define SYS_MAIN_ID 0x01
#define SYS_SUB_ID 0x10
#define TELEMETRY_TYPE 0x02
#define TELECONTROL_TYPE 0x01

#define BIT(x) (1U << (x))

using namespace std;

const float expTable[16] = {
    5000.f,  // 0x00 (default)
    2500.f,  // 0x01
    5000.f,  // 0x02
    7500.f,  // 0x03
    10000.f, // 0x04
    12500.f, // 0x05
    15000.f, // 0x06
    17500.f, // 0x07
    20000.f, // 0x08
    22500.f, // 0x09
    25000.f, // 0x0A
    27500.f, // 0x0B
    30000.f, // 0x0C
    32500.f, // 0x0D
    35000.f, // 0x0E
    37500.f, // 0x0F
};

const float gainTable[4] = {
    0.f,  // 0x00 (default)
    5.f,  // 0x01
    10.f,  // 0x02
    20.f,  // 0x03
};

const float frameRateTable[4] = {
    10.f,  // 0x00 (default)
    5.f,  // 0x01
    15.f,  // 0x02
    20.f,  // 0x03
};

float getExposureTime(uint8_t dExpEx)
{
    uint8_t exp = dExpEx & 0x0F;
    return expTable[exp];
}

float getGain(uint8_t dGainEx)
{
    uint8_t g = dGainEx & 0x03;
    return gainTable[g];
}

float getFrameRate(uint8_t dFrameRateEx)
{
    uint8_t fr = dFrameRateEx & 0x03;
    return frameRateTable[fr];
}

uint8_t checksum(uint8_t data[], int len)
{
	unsigned int sum = 0;
	for (int i = 0; i < len; i++) {
		sum += data[i];
	}
	return sum & 0xFF;  // 只取低8位
}

void getArray(cJSON* data_message, uint8_t* dataArray)
{
    int size = cJSON_GetArraySize(data_message);
    // if (size < 4) {
    //     printf("data_message array size less than 4\n");
    //     return;
    // }

    for (int i = 0; i < 4; i++) {
        cJSON* item = cJSON_GetArrayItem(data_message, i);
        if (item && cJSON_IsNumber(item)) {
            dataArray[i] = item->valueint & 0xFF;
        } else {
            printf("Invalid number at index %d\n", i);
            dataArray[i] = 0;
        }
    }
}

void myCallbackFunction(int type, const void* data)
{
	bool flag1 = 0;
	bool flag2 = 0;
	bool flag3 = 0;
	bool flag4 = 0;
	bool flag5 = 0;
	bool flag6 = 0;
	bool flag7 = 0;
	bool flag8 = 0;

	uint8_t temp[18];

	const char* json_str = (const char*)data;

	printf("Received JSON: %s\n", json_str);

	cJSON* root = cJSON_Parse(json_str);
	if (!root) {
		printf("Failed to parse JSON\n");
		return;
	}

	cJSON* sync_head = cJSON_GetObjectItem(root, "sync_head");
	if (sync_head && cJSON_IsNumber(sync_head)) {
		printf("sync_head: %x\n", sync_head->valueint);

		temp[0] = (sync_head->valueint >> 8) & 0xFF; // 高字节
		temp[1] = sync_head->valueint & 0xFF; // 低字节

		if (sync_head->valueint == SYNC_HEAD)
		{
			flag1 = 1;
		}
		else
		{
			flag1 = 0;
		}
	}

	cJSON* data_length = cJSON_GetObjectItem(root, "data_length");
	if (data_length && cJSON_IsNumber(data_length)) {
		printf("data_length: %d\n", data_length->valueint);

		temp[2] = (data_length->valueint >> 8) & 0xFF; // 高字节
		temp[3] = data_length->valueint & 0xFF; // 低字节
	}

	cJSON* send_payload_main_id = cJSON_GetObjectItem(root, "send_payload_main_id");
	if (send_payload_main_id && cJSON_IsNumber(send_payload_main_id)) {
		printf("send_payload_main_id: %x\n", send_payload_main_id->valueint);

		temp[4] = send_payload_main_id->valueint & 0xFF;

		if (send_payload_main_id->valueint == SYS_MAIN_ID)
		{
			flag2 = 1;
		}
		else
		{
			flag2 = 0;
		}
	}

	cJSON* send_payload_sub_id = cJSON_GetObjectItem(root, "send_payload_sub_id");
	if (send_payload_sub_id && cJSON_IsNumber(send_payload_sub_id)) {
		printf("send_payload_sub_id: %x\n", send_payload_sub_id->valueint);

		temp[5] = send_payload_sub_id->valueint & 0xFF;

		if (send_payload_sub_id->valueint == SYS_SUB_ID)
		{
			flag3 = 1;
		}
		else
		{
			flag3 = 0;
		}
	}

	cJSON* receive_payload_main_id = cJSON_GetObjectItem(root, "receive_payload_main_id");
	if (receive_payload_main_id && cJSON_IsNumber(receive_payload_main_id)) {
		printf("receive_payload_main_id: %x\n", receive_payload_main_id->valueint);

		temp[6] = receive_payload_main_id->valueint & 0xFF;

		if (receive_payload_main_id->valueint == APP_MAIN_ID)
		{
			flag4 = 1;
		}
		else
		{
			flag4 = 0;
		}
	}

	cJSON* receive_payload_sub_id = cJSON_GetObjectItem(root, "receive_payload_sub_id");
	if (receive_payload_sub_id && cJSON_IsNumber(receive_payload_sub_id)) {
		printf("receive_payload_sub_id: %x\n", receive_payload_sub_id->valueint);

		temp[7] = receive_payload_sub_id->valueint & 0xFF;

		if (receive_payload_sub_id->valueint == APP_SUB_ID)
		{
			flag5 = 1;
		}
		else
		{
			flag5 = 0;
		}
	}

	cJSON* data_type = cJSON_GetObjectItem(root, "data_type");
	if (data_type && cJSON_IsNumber(data_type)) {
		printf("data_type: %x\n", data_type->valueint);

		temp[8] = data_type->valueint & 0xFF;

		if (data_type->valueint == TELECONTROL_TYPE)
		{
			flag6 = 1;
		}
		else
		{
			flag6 = 0;
		}
	}

	cJSON* send_frame_count = cJSON_GetObjectItem(root, "send_frame_count");
	if (send_frame_count && cJSON_IsNumber(send_frame_count)) {
		printf("send_frame_count: %d\n", send_frame_count->valueint);

		temp[9] = send_frame_count->valueint & 0xFF;
	}

	cJSON* time_of_send = cJSON_GetObjectItem(root, "time_of_send");
	if (time_of_send && cJSON_IsNumber(time_of_send)) {
		printf("time_of_send: %d\n", time_of_send->valueint);

		temp[10] = (time_of_send->valueint >> 24) & 0xFF;  // 高
		temp[11] = (time_of_send->valueint >> 16) & 0xFF;
		temp[12] = (time_of_send->valueint >> 8) & 0xFF;
		temp[13] = time_of_send->valueint & 0xFF;          // 低
	}

	cJSON* data_message = cJSON_GetObjectItem(root, "data_message");
	if (data_message && cJSON_IsArray(data_message)) {
		uint8_t dataArray[4];
		getArray(data_message, dataArray);
		printf("data_message: %x, %x, %x, %x\n",dataArray[0],dataArray[1],dataArray[2],dataArray[3]);
		temp[14] = dataArray[0];  // 高
		temp[15] = dataArray[1];
		temp[16] = dataArray[2];
		temp[17] = dataArray[3];  // 低
		if ((dataArray[0] == 0x00) && (dataArray[1] == 0x02))
		{
			flag7 = 1;
		}
		else
		{
			flag7 = 0;
		}
	}

	cJSON* sum_of_check = cJSON_GetObjectItem(root, "sum_of_check");
	if (sum_of_check && cJSON_IsNumber(sum_of_check)) {
		printf("sum_of_check: %x\n", sum_of_check->valueint);

		if (sum_of_check->valueint == checksum(temp, sizeof(temp)))
		{
			flag8 = 1;
		}
		else
		{
			flag8 = 1;
			printf("Checksum false but forced true.\n");
		}
	}

	if (flag1 && flag2 && flag3 && flag4 && flag5 && flag6 && flag7 && flag8)
	{
		std::lock_guard<std::mutex> lock(ddsMutex);
		printf("sub_succed\n");
		TM_receive_cnt++;
		// 第1个字节
		imageBit = (temp[16] & BIT(7)) ? 1 : 0;
		dataTransBit = (temp[16] & BIT(6)) ? 1 : 0;
		poseBit = (temp[16] & BIT(5)) ? 1 : 0;
		controlBit = (temp[16] & BIT(4)) ? 1 : 0;
		dExpBit = (temp[16] & BIT(3)) ? 1 : 0;
		dGainBit = (temp[16] & BIT(2)) ? 1 : 0;
		dFrameRateBit = (temp[16] & BIT(1)) ? 1 : 0;

		// 第2个字节
		dExp = (temp[17] >> 4) & 0x0F;
		dGain = (temp[17] >> 2) & 0x03;
		dFrameRate = temp[17] & 0x03;
	}
	else
	{
		printf("flag1~8=%d, %d, %d, %d, %d, %d, %d, %d\n",flag1,flag2,flag3,flag4,flag5,flag6,flag7,flag8);
		printf("sub_fail\n");
	}
	cJSON_Delete(root);
}

void buildArrayString(const uint8_t* data, int size, char* out)
{
    if (!data || !out || size <= 0) {
        printf("Invalid input to buildArrayString\n");
        return;
    }
    strcat(out, "[");
    for (int i = 0; i < size; ++i) {
        char temp_buf[8] = {0};
        sprintf(temp_buf, "%u", data[i]);
        strcat(out, temp_buf);

        if (i != size - 1) {
            strcat(out, ",");
        }
    }
    strcat(out, "]");
}

void DDSPub(uint8_t data_message[], uint8_t receive_cnt, DDSPub_t* pub, int topic)
{
	static uint8_t send_frame_count = 0;
	static uint8_t send_cnt = 0;
	time_t t = time(NULL);
	uint32_t time_of_send = (uint32_t)t;
	static int8_t receive_cnt_old = 0;
	if(receive_cnt != receive_cnt_old || true)
	{
		uint8_t temp[54] = { 0 };

		temp[0] = (SYNC_HEAD >> 8) & 0xFF;
		temp[1] = SYNC_HEAD & 0xFF;
		temp[2] = (DATA_LENGTH >> 8) & 0xFF;
		temp[3] = DATA_LENGTH & 0xFF;
		temp[4] = SYS_MAIN_ID;
		temp[5] = SYS_SUB_ID;
		temp[6] = APP_MAIN_ID;
		temp[7] = APP_SUB_ID;
		temp[8] = TELECONTROL_TYPE;
		temp[9] = send_frame_count;
		temp[10] = (time_of_send >> 24) & 0xFF;  // 最高8位
		temp[11] = (time_of_send >> 16) & 0xFF;  // 次高8位
		temp[12] = (time_of_send >> 8) & 0xFF;   // 次低8位
		temp[13] = time_of_send & 0xFF;          // 最低8位

		// temp[5] = 0x00;
		// temp[6] = 0x02;

		int i = 0;
		for (i = 0;i < 38;i++)
		{
			temp[14 + i] = data_message[i];
		}
		temp[52] = receive_cnt;
		send_cnt++;
		temp[53] = send_cnt;

		char json[1024] = {0};  
		char data_message_json[512] = {0};
		buildArrayString(temp + 14, 40, data_message_json);
		uint8_t sum = checksum(temp, sizeof(temp));

		sprintf(json, 
			"{\"sync_head\":%u,"
			"\"data_length\":%u,"
			"\"send_payload_main_id\":%u,"
			"\"send_payload_sub_id\":%u,"
			"\"receive_payload_main_id\":%u,"
			"\"receive_payload_sub_id\":%u,"
			"\"data_type\":%u,"
			"\"send_frame_count\":%u,"
			"\"time_of_send\":%u,"
			"\"data_message\":%s,"
			"\"sum_of_check\":%u}",
			SYNC_HEAD, 
			DATA_LENGTH, 
			APP_MAIN_ID, 
			APP_SUB_ID, 
			SYS_MAIN_ID, 
			SYS_SUB_ID, 
			TELEMETRY_TYPE, 
			send_frame_count, 
			time_of_send, 
			data_message_json, 
			sum);

		// 打印
		// printf("%s\n", json);

		send_frame_count++;

		const char* message = json;

		if (DDSPub_sendMsg(pub, topic, message))
		{
			// printf("[Publisher] Sent message\n");
		}
		else
		{
			printf("[Publisher] Failed to send message\n");
		}
	}
	receive_cnt_old = receive_cnt;
}

void DDSSubThread()
{
	DDSSub_t *sub = DDSSub_creat();
    DDSSub_set_domain_id(sub, 2);
    DDSSub_registerCallback(myCallbackFunction);
    int subAddr[1] ={TELEMETRY_DATA_INDEX};
    DDSSub_init(sub, subAddr, 1);
	printf("DDSSub exited");
}

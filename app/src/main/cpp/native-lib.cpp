#include <jni.h>
#include <string>
#include <android/log.h>
#include <utility>
#include <vector>
#include <cstdint>


#define TAG "H265_SPS_Cpp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

// 定义一个全局变量，用于记录当前码流解析到第几个字节，以便下一次解析时使用。
int offsetIndex = 0;
// 码流总长度
int totalSize;
// 码流数据
jbyte *h265Data;
// SPS NAL 单元索引
int nalBitIndex = 0;


int findSeparator();

std::vector<uint8_t> getNal(int offsetIndex, int length);

void parseSPS(std::vector<uint8_t> nalData);

int u(int length, std::vector<uint8_t> nalData);

int ue(const std::vector<uint8_t> nalData);

int se(const std::vector<uint8_t> nalData);

void profile_tier_level(int profilePresentFlag, int sps_max_sub_layers_minus1, std::vector<uint8_t> nalData);



extern "C"
JNIEXPORT void JNICALL
Java_com_zhanghao_h265_1sps_1analyze_MainActivity_analyzeSps(JNIEnv *env, jobject thiz, jbyteArray data_byte_array) {
    h265Data = env->GetByteArrayElements(data_byte_array, nullptr);
    totalSize = env->GetArrayLength(data_byte_array);
    LOGI("file length = %d", totalSize);

    // 找出NAL单元
    int nal_start;
    int nal_end;

    while (1) {
        if (totalSize == 0 || offsetIndex >= totalSize) {
            break;
        }

        nal_start = offsetIndex;
        int separatorStartIndex = findSeparator();
        nal_end = separatorStartIndex;
        if (separatorStartIndex == 0) {
            continue;
        }

        if (separatorStartIndex == -1) {
            // 只关注SPS的解析， 因此最后一段数据暂不处理
            break;
        }

        // 获取单个NAL单元数据
        std::vector<uint8_t> nalData = getNal(nal_start, nal_end - nal_start);

        // 取NAL type 详见Readme.md
        int nalType = (nalData[0] & 0x7E) >> 1;

        // SPS
        if (nalType == 33) {
            parseSPS(nalData);
        }
    }

    env->ReleaseByteArrayElements(data_byte_array, h265Data, JNI_ABORT);
    h265Data = nullptr;

}

/**
* 寻找分隔符坐标 :
* 0x 00 00 01
* 0x 00 00 00 01
* 找到后更新 offsetIndex(跳过分隔符)
*/
int findSeparator() {
    for (int i = offsetIndex; i < totalSize - 4; i++) {
        if (h265Data[i] == 0x00 && h265Data[i + 1] == 0x00 && h265Data[i + 2] == 0x01) {
            offsetIndex = i + 3;
            return i;
        }
        if (h265Data[i] == 0x00 && h265Data[i + 1] == 0x00 && h265Data[i + 2] == 0x00 && h265Data[i + 3] == 0x01) {
            offsetIndex = i + 4;
            return i;
        }
    }
    return -1;
}

/**
 * 获取一个NALU单元字节数组
 * @param offsetIndex 起始索引
 * @param length      长度
 * @return  NAL单元数组
 */
std::vector<uint8_t> getNal(int offsetIndex, int length) {
    std::vector<uint8_t> naluByteArray(length);
    for (int i = offsetIndex; i < offsetIndex + length; i++) {
        naluByteArray[i - offsetIndex] = h265Data[i];
    }
    return naluByteArray;
}

/**
 * 解析SPS
 * @param nalData
 */
void parseSPS(std::vector<uint8_t> nalData) {
    // 跳过NALU头（占2个字节）
    nalBitIndex = 16;

    // 标识特定的视频参数集
    int sps_video_parameter_set_id = u(4, nalData);

    int sps_max_sub_layers_minus1 = u(3, nalData);
    int sps_temporal_id_nesting_flag = u(1, nalData);

    profile_tier_level(1, sps_max_sub_layers_minus1, nalData);

    /**
     * 注： 应该是 profile_tier_level 中的多读取了一个字节的长度， 暂不知道原因
     * 由于此处目的是解析分辨率，因此无需关注， NAL 索引 -1 ，修正即可
     */
    nalBitIndex -= 8;

    int sps_seq_parameter_set_id = ue(nalData);

    int chroma_format_idc = ue(nalData);
    if (chroma_format_idc == 3) {
        int separate_colour_plane_flag = u(1, nalData);
    }

    // 宽高
    int pic_width_in_luma_samples = ue(nalData);
    int pic_height_in_luma_samples = ue(nalData);
    LOGI("pic_width_in_luma_samples = %d", pic_width_in_luma_samples);
    LOGI("pic_height_in_luma_samples = %d", pic_height_in_luma_samples);
}

/**
 * 获取指定长度bit的值；
 * 获取完毕后，将NAL索引移动到读取后的位置。
 *
 * @param length 需要读取的bit位长度
 * @param nalData    NALU单元
 * @return 读取到的值, 以整数返回
 */
int u(int length, std::vector<uint8_t> nalData) {
    int data = 0;
    for (int i = nalBitIndex; i < nalBitIndex + length; i++) {
        data <<= 1;
        int bitValue = (nalData[i / 8] >> (7 - i % 8)) & 0x01;
        data |= bitValue;
    }
    nalBitIndex += length;
    return data;
}


/**
    * 辅助方法，用于解析 ue(v) 类型的 Exp-Golomb 编码
    * 解析无符号数
    *
    * @param nalData NAL 单元
    * @return 解析Exp-Golomb编码后的值
    */
int ue(const std::vector<uint8_t> nalData) {
    int leadingZeros = 0;
    while (u(1, nalData) == 0 && leadingZeros < 32) {
        leadingZeros++;
    }
    return (1 << leadingZeros) - 1 + u(leadingZeros, nalData);
}

/**
 * 辅助方法，用于解析 se(v) 类型的 Exp-Golomb 编码
 * 解析有符号数
 *
 * @param nalData NAL 单元
 * @return 解析Exp-Golomb编码后的值
 */
int se(const std::vector<uint8_t> nalData) {
    int value = ue(nalData);
    return (value % 2 == 0) ? -(value / 2) : (value + 1) / 2;
}

/**
 * 是一个结构，用于表示视频编码的配置信息，包括编码配置的profile、tier和level等信息
 * 目前无需重点关注
 */
void profile_tier_level(int profilePresentFlag, int sps_max_sub_layers_minus1, std::vector<uint8_t> nalData) {
    if (profilePresentFlag) {
        int general_profile_space = u(2, nalData);
        int general_tier_flag = u(1, nalData);
        int general_profile_idc = u(5, nalData);
        int general_profile_compatibility_flag[32];
        for (int j = 0; j < 32; j++) {
            general_profile_compatibility_flag[j] = u(1, nalData);
        }
        int general_progressive_source_flag = u(1, nalData);
        int general_interlaced_source_flag = u(1, nalData);
        int general_non_packed_constraint_flag = u(1, nalData);
        int general_frame_only_constraint_flag = u(1, nalData);

        if (general_profile_idc == 4 || general_profile_compatibility_flag[4] ||
            general_profile_idc == 5 || general_profile_compatibility_flag[5] ||
            general_profile_idc == 6 || general_profile_compatibility_flag[6] ||
            general_profile_idc == 7 || general_profile_compatibility_flag[7] ||
            general_profile_idc == 8 || general_profile_compatibility_flag[8] ||
            general_profile_idc == 9 || general_profile_compatibility_flag[9] ||
            general_profile_idc == 10 || general_profile_compatibility_flag[10] ||
            general_profile_idc == 11 || general_profile_compatibility_flag[11]) {

            int general_max_12bit_constraint_flag = u(1, nalData);
            int general_max_10bit_constraint_flag = u(1, nalData);
            int general_max_8bit_constraint_flag = u(1, nalData);
            int general_max_422chroma_constraint_flag = u(1, nalData);
            int general_max_420chroma_constraint_flag = u(1, nalData);
            int general_max_monochrome_constraint_flag = u(1, nalData);
            int general_intra_constraint_flag = u(1, nalData);
            int general_one_picture_only_constraint_flag = u(1, nalData);
            int general_lower_bit_rate_constraint_flag = u(1, nalData);

            if (general_profile_idc == 5 || general_profile_compatibility_flag[5] ||
                general_profile_idc == 9 || general_profile_compatibility_flag[9] ||
                general_profile_idc == 10 || general_profile_compatibility_flag[10] ||
                general_profile_idc == 11 || general_profile_compatibility_flag[11]) {

                int general_max_14bit_constraint_flag = u(1, nalData);
                int general_reserved_zero_33bits = u(33, nalData);
            } else {
                int general_reserved_zero_34bits = u(34, nalData);
            }

        } else if (general_profile_idc == 2 || general_profile_compatibility_flag[2]) {
            int general_reserved_zero_7bits = u(7, nalData);
            int general_one_picture_only_constraint_flag = u(1, nalData);
            int general_reserved_zero_35bits = u(35, nalData);
        } else {
            int general_reserved_zero_43bits = u(43, nalData);
        }

        if (general_profile_idc == 1 || general_profile_compatibility_flag[1] ||
            general_profile_idc == 2 || general_profile_compatibility_flag[2] ||
            general_profile_idc == 3 || general_profile_compatibility_flag[3] ||
            general_profile_idc == 4 || general_profile_compatibility_flag[4] ||
            general_profile_idc == 5 || general_profile_compatibility_flag[5] ||
            general_profile_idc == 9 || general_profile_compatibility_flag[9] ||
            general_profile_idc == 11 || general_profile_compatibility_flag[11]) {

            int general_inbld_flag = u(1, nalData);
        } else {
            int general_reserved_zero_bit = u(1, nalData);
        }
    }
    int general_level_idc1 = u(8, nalData);
    int sub_layer_profile_present_flag[sps_max_sub_layers_minus1];
    int sub_layer_level_present_flag[sps_max_sub_layers_minus1];
    for (int i = 0; i < sps_max_sub_layers_minus1; ++i) {
        sub_layer_profile_present_flag[i] = u(1, nalData);
        sub_layer_level_present_flag[i] = u(1, nalData);
    }

    if (sps_max_sub_layers_minus1 > 0) {
        int reserved_zero_2bits[8];
        for (int i = sps_max_sub_layers_minus1; i < 8; i++) {
            reserved_zero_2bits[i] = u(2, nalData);
        }
    }

    int sub_layer_profile_space[sps_max_sub_layers_minus1];
    int sub_layer_tier_flag[sps_max_sub_layers_minus1];
    int sub_layer_profile_idc[sps_max_sub_layers_minus1];
    int sub_layer_profile_compatibility_flag[sps_max_sub_layers_minus1][32];
    int sub_layer_progressive_source_flag[sps_max_sub_layers_minus1];
    int sub_layer_interlaced_source_flag[sps_max_sub_layers_minus1];
    int sub_layer_non_packed_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_frame_only_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_max_12bit_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_max_10bit_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_max_8bit_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_max_422chroma_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_max_420chroma_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_max_monochrome_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_intra_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_one_picture_only_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_lower_bit_rate_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_max_14bit_constraint_flag[sps_max_sub_layers_minus1];
    int sub_layer_reserved_zero_33bits[sps_max_sub_layers_minus1];
    int sub_layer_reserved_zero_34bits[sps_max_sub_layers_minus1];
    int sub_layer_reserved_zero_7bits[sps_max_sub_layers_minus1];
    int sub_layer_reserved_zero_35bits[sps_max_sub_layers_minus1];
    int sub_layer_reserved_zero_43bits[sps_max_sub_layers_minus1];
    int sub_layer_level_idc[sps_max_sub_layers_minus1];
    for (int i = 0; i < sps_max_sub_layers_minus1; i++) {
        if (sub_layer_profile_present_flag[i]) {
            sub_layer_profile_space[i] = u(2, nalData);
            sub_layer_tier_flag[i] = u(1, nalData);
            sub_layer_profile_idc[i] = u(5, nalData);
            for (int j = 0; j < 32; j++) {
                sub_layer_profile_compatibility_flag[i][j] = u(1, nalData);
            }
            sub_layer_progressive_source_flag[i] = u(1, nalData);
            sub_layer_interlaced_source_flag[i] = u(1, nalData);
            sub_layer_non_packed_constraint_flag[i] = u(1, nalData);
            sub_layer_frame_only_constraint_flag[i] = u(1, nalData);

            if (sub_layer_profile_idc[i] == 4 || sub_layer_profile_compatibility_flag[i][4] ||
                sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[i][5] ||
                sub_layer_profile_idc[i] == 6 || sub_layer_profile_compatibility_flag[i][6] ||
                sub_layer_profile_idc[i] == 7 || sub_layer_profile_compatibility_flag[i][7] ||
                sub_layer_profile_idc[i] == 8 || sub_layer_profile_compatibility_flag[i][8] ||
                sub_layer_profile_idc[i] == 9 || sub_layer_profile_compatibility_flag[i][9] ||
                sub_layer_profile_idc[i] == 10 || sub_layer_profile_compatibility_flag[i][10] ||
                sub_layer_profile_idc[i] == 11 || sub_layer_profile_compatibility_flag[i][11]) {

                sub_layer_max_12bit_constraint_flag[i] = u(1, nalData);
                sub_layer_max_10bit_constraint_flag[i] = u(1, nalData);
                sub_layer_max_8bit_constraint_flag[i] = u(1, nalData);
                sub_layer_max_422chroma_constraint_flag[i] = u(1, nalData);
                sub_layer_max_420chroma_constraint_flag[i] = u(1, nalData);
                sub_layer_max_monochrome_constraint_flag[i] = u(1, nalData);
                sub_layer_intra_constraint_flag[i] = u(1, nalData);
                sub_layer_one_picture_only_constraint_flag[i] = u(1, nalData);
                sub_layer_lower_bit_rate_constraint_flag[i] = u(1, nalData);

                if (sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[i][5] ||
                    sub_layer_profile_idc[i] == 9 || sub_layer_profile_compatibility_flag[i][9] ||
                    sub_layer_profile_idc[i] == 10 || sub_layer_profile_compatibility_flag[i][10] ||
                    sub_layer_profile_idc[i] == 11 || sub_layer_profile_compatibility_flag[i][11]) {

                    sub_layer_max_14bit_constraint_flag[i] = u(1, nalData);
                    sub_layer_reserved_zero_33bits[i] = u(33, nalData);
                } else {
                    sub_layer_reserved_zero_34bits[i] = u(34, nalData);
                }

            } else if (sub_layer_profile_idc[i] == 2 || sub_layer_profile_compatibility_flag[i][2]) {
                sub_layer_reserved_zero_7bits[i] = u(7, nalData);
                sub_layer_one_picture_only_constraint_flag[i] = u(1, nalData);
                sub_layer_reserved_zero_35bits[i] = u(35, nalData);
            } else {
                sub_layer_reserved_zero_43bits[i] = u(43, nalData);
            }

            if (sub_layer_profile_idc[i] == 1 ||
                sub_layer_profile_compatibility_flag[i][1] ||
                sub_layer_profile_idc[i] == 2 ||
                sub_layer_profile_compatibility_flag[i][2] ||
                sub_layer_profile_idc[i] == 3 ||
                sub_layer_profile_compatibility_flag[i][3] ||
                sub_layer_profile_idc[i] == 4 ||
                sub_layer_profile_compatibility_flag[i][4] ||
                sub_layer_profile_idc[i] == 5 ||
                sub_layer_profile_compatibility_flag[i][5] ||
                sub_layer_profile_idc[i] == 9 ||
                sub_layer_profile_compatibility_flag[i][9] ||
                sub_layer_profile_idc[i] == 11 ||
                sub_layer_profile_compatibility_flag[i][11]) {

                int sub_layer_inbld_flag = u(1, nalData);
            } else {
                int sub_layer_reserved_zero_bit = u(1, nalData);
            }
        }
        if (sub_layer_level_present_flag[i]) {
            sub_layer_level_idc[i] = u(8, nalData);
        }
    }
}

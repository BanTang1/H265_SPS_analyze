目的：H265 从SPS中解析出分辨率

H265文档在项目根目录中：
Sequence parameter set RBSP syntax 在文档的 35页
NAL unit type 在文档的66页

简单说明：
在H.264编码中，NAL头部只有一个字节，包含以下字段：
    forbidden_zero_bit：这是一个1位的字段，总是为0。
    nal_ref_idc：这是一个2位的字段，表示NAL单元的重要性。
    nal_unit_type：这是一个5位的字段，表示NAL单元的类型。

而在H.265编码中，NAL头部扩展到了两个字节，包含以下字段：
    forbidden_zero_bit：这是一个1位的字段，总是为0。
    nal_unit_type：这是一个6位的字段，表示NAL单元的类型。
    nuh_layer_id：这是一个6位的字段，表示NAL单元的层ID。
    nuh_temporal_id_plus1：这是一个3位的字段，表示NAL单元的时间ID

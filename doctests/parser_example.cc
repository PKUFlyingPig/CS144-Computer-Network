const uint32_t val1 = 0xdeadbeef;
const uint16_t val2 = 0xc0c0;
const uint8_t val3 = 0xff;
const uint32_t val4 = 0x0c05fefe;

// first, let's serialize it
std::string buffer;
buffer.push_back(0x32);  // manually added to beginning of string
{
    NetUnparser p;
    p.u32(buffer, val1);
    p.u16(buffer, val2);
    p.u8(buffer, val3);
    p.u32(buffer, val4);
}  // p goes out of scope, data is in buffer

// now let's deserialize it
uint8_t out0, out3;
uint32_t out1, out4;
uint16_t out2;
{
    NetParser p{std::string(buffer)};  // NOTE: starting at offset 0
    out0 = p.u8();                     // buffer[0], which we manually set to 0x32 above
    out1 = p.u32();                    // parse out val1
    out2 = p.u16();                    // val2
    out3 = p.u8();                     // val3
    out4 = p.u32();                    // val4
}  // p goes out of scope

if (out0 != 0x32 || out1 != val1 || out2 != val2 || out3 != val3 || out4 != val4) {
    throw std::runtime_error("bad parse");
}

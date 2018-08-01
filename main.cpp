#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <functional>
using namespace std;

union bmpheader {
  uint8_t raw[54];
  struct alignas(2) {
    //char bfType[2];         // exclude to align following data
    uint32_t  bfSize;
    uint16_t  bfReserved1;
    uint16_t  bfReserved2;
    uint32_t  bfOffBits;

    uint32_t  bcSize;         // must be 40
    uint32_t  bcWidth;
    int32_t   bcHeight;
    uint16_t  bcPlanes;
    uint16_t  bcBitCount;

    uint32_t  biCompression;  // must be 0
    uint32_t  biSizeImage;
    uint32_t  biXPixPerMeter;
    uint32_t  biYPixPerMeter;
    uint32_t  biClrUsed;
    uint32_t  biCirImportant;
  } data;
};
using coeff_t = vector<vector<int8_t>>;

struct YCbCr {
  static constexpr int scale = 1<<8;
  static constexpr int32_t cnv[3][4] = {
    {(int32_t)( scale*0.299),   (int32_t)( scale*0.587),   (int32_t)( scale*0.114),   scale*0   },
    {(int32_t)(-scale*0.168736),(int32_t)(-scale*0.331264),(int32_t)( scale*0.5),     scale*128 },
    {(int32_t)( scale*0.5),     (int32_t)(-scale*0.418688),(int32_t)(-scale*0.081312),scale*128 }
  };
public:
  uint8_t Y, Cb, Cr;

  YCbCr(const uint8_t bgr[3]) {
    uint8_t* const trg[3] = {&Y, &Cb, &Cr};
    for (int i = 0; i < 3; i++) {
      uint32_t tmp = (
        cnv[i][0]*bgr[2] +
        cnv[i][1]*bgr[1] +
        cnv[i][2]*bgr[0] +
        cnv[i][3]
      );
      *trg[i] = tmp / scale;
    }
    //cout << cnv[0][0] << endl << cnv[0][1] << endl << cnv[1][0] << endl;
  }
};

struct zigzag {
  int walk[64][2];
  constexpr zigzag() : walk() {
    int y=0, x=0;
    int rightup=1;
    for (int i = 0; i < 64; i++) {
      walk[i][0] = y;
      walk[i][1] = x;
      x += rightup ?  1 : -1;
      y += rightup ? -1 :  1;
      if(y>7) {y = 7; rightup = 1; x += 2;}
      if(x>7) {x = 7; rightup = 0; y += 2;}
      if(y<0) {y = 0; rightup = 0;}
      if(x<0) {x = 0; rightup = 1;}
    }
  }
};

// cos((2*x+1)*u*PI/16)
struct costable {
  constexpr static double PI = 3.141592653589793;
  double v[8][8];
  constexpr costable() : v() {
    for (int j = 0; j < 8; j++)
    for (int i = 0; i < 8; i++) {
      v[j][i] = cos((2*i+1)*j*PI/16);
    }
  }
};

constexpr uint8_t qtable_y[8][8] = {
  {3, 3, 3, 4, 4, 5, 6, 6},
  {3, 3, 4, 4, 5, 6, 6, 7},
  {3, 4, 4, 5, 6, 6, 7, 7},
  {4, 4, 5, 6, 6, 7, 7, 8},
  {4, 5, 6, 6, 7, 7, 8, 8},
  {5, 6, 6, 7, 7, 8, 8, 8},
  {6, 6, 7, 7, 8, 8, 8, 8},
  {6, 7, 7, 8, 8, 8, 8, 8}
};
constexpr uint8_t qtable_c[8][8] = {
  {4, 4, 5, 6, 7, 8, 8, 8},
  {4, 5, 6, 7, 8, 8, 8, 8},
  {5, 6, 7, 8, 8, 8, 8, 8},
  {6, 7, 8, 8, 8, 8, 8, 8},
  {7, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8}
};

constexpr uint16_t dc_y_hufftable[][2] = {
//{code len, code}
  { 2, 0x0000},
  { 3, 0x0002},
  { 3, 0x0003},
  { 3, 0x0004},
  { 3, 0x0005},
  { 3, 0x0006},
  { 4, 0x000e},
  { 5, 0x001e},
  { 6, 0x003e},
  { 7, 0x007e},
  { 8, 0x00fe},
  { 9, 0x01fe}
};

constexpr uint16_t dc_c_hufftable[][2] = {
//{code len, code}
  { 2, 0x0000},
  { 2, 0x0001},
  { 2, 0x0002},
  { 3, 0x0006},
  { 4, 0x000e},
  { 5, 0x001e},
  { 6, 0x003e},
  { 7, 0x007e},
  { 8, 0x00fe},
  { 9, 0x01fe},
  {10, 0x03fe},
  {11, 0x07fe}
};

constexpr uint16_t ac_y_hufftable[][11][2] = {
//{code len, code}
//v Run length    > Code length
  {{ 4,0x000a}, { 2,0x0000}, { 2,0x0001}, { 3,0x0004}, { 4,0x000b}, { 5,0x001a}, { 7,0x0078}, { 8,0x00f8}, {10,0x03f6}, {16,0xff82}, {16,0xff83}},
  {{ 0,0xdead}, { 4,0x000c}, { 5,0x001b}, { 7,0x0079}, { 9,0x01f6}, {11,0x07f6}, {16,0xff84}, {16,0xff85}, {16,0xff86}, {16,0xff87}, {16,0xff88}},
  {{ 0,0xdead}, { 5,0x001c}, { 8,0x00f9}, {10,0x03f7}, {12,0x0ff4}, {16,0xff89}, {16,0xff8a}, {16,0xff8b}, {16,0xff8c}, {16,0xff8d}, {16,0xff8e}},
  {{ 0,0xdead}, { 6,0x003a}, { 9,0x01f7}, {12,0x0ff5}, {16,0xff8f}, {16,0xff90}, {16,0xff91}, {16,0xff92}, {16,0xff93}, {16,0xff94}, {16,0xff95}},
  {{ 0,0xdead}, { 6,0x003b}, {10,0x03f8}, {16,0xff96}, {16,0xff97}, {16,0xff98}, {16,0xff99}, {16,0xff9a}, {16,0xff9b}, {16,0xff9c}, {16,0xff9d}},
  {{ 0,0xdead}, { 7,0x007a}, {11,0x07f7}, {16,0xff9e}, {16,0xff9f}, {16,0xffa0}, {16,0xffa1}, {16,0xffa2}, {16,0xffa3}, {16,0xffa4}, {16,0xffa5}},
  {{ 0,0xdead}, { 7,0x007b}, {12,0x0ff6}, {16,0xffa6}, {16,0xffa7}, {16,0xffa8}, {16,0xffa9}, {16,0xffaa}, {16,0xffab}, {16,0xffac}, {16,0xffad}},
  {{ 0,0xdead}, { 8,0x00fa}, {12,0x0ff7}, {16,0xffae}, {16,0xffaf}, {16,0xffb0}, {16,0xffb1}, {16,0xffb2}, {16,0xffb3}, {16,0xffb4}, {16,0xffb5}},
  {{ 0,0xdead}, { 9,0x01f8}, {15,0x7fc0}, {16,0xffb6}, {16,0xffb7}, {16,0xffb8}, {16,0xffb9}, {16,0xffba}, {16,0xffbb}, {16,0xffbc}, {16,0xffbd}},
  {{ 0,0xdead}, { 9,0x01f9}, {16,0xffbe}, {16,0xffbf}, {16,0xffc0}, {16,0xffc1}, {16,0xffc2}, {16,0xffc3}, {16,0xffc4}, {16,0xffc5}, {16,0xffc6}},
  {{ 0,0xdead}, { 9,0x01fa}, {16,0xffc7}, {16,0xffc8}, {16,0xffc9}, {16,0xffca}, {16,0xffcb}, {16,0xffcc}, {16,0xffcd}, {16,0xffce}, {16,0xffcf}},
  {{ 0,0xdead}, {10,0x03f9}, {16,0xffd0}, {16,0xffd1}, {16,0xffd2}, {16,0xffd3}, {16,0xffd4}, {16,0xffd5}, {16,0xffd6}, {16,0xffd7}, {16,0xffd8}},
  {{ 0,0xdead}, {10,0x03fa}, {16,0xffd9}, {16,0xffda}, {16,0xffdb}, {16,0xffdc}, {16,0xffdd}, {16,0xffde}, {16,0xffdf}, {16,0xffe0}, {16,0xffe1}},
  {{ 0,0xdead}, {11,0x07f8}, {16,0xffe2}, {16,0xffe3}, {16,0xffe4}, {16,0xffe5}, {16,0xffe6}, {16,0xffe7}, {16,0xffe8}, {16,0xffe9}, {16,0xffea}},
  {{ 0,0xdead}, {16,0xffeb}, {16,0xffec}, {16,0xffed}, {16,0xffee}, {16,0xffef}, {16,0xfff0}, {16,0xfff1}, {16,0xfff2}, {16,0xfff3}, {16,0xfff4}},
  {{11,0x07f9}, {16,0xfff5}, {16,0xfff6}, {16,0xfff7}, {16,0xfff8}, {16,0xfff9}, {16,0xfffa}, {16,0xfffb}, {16,0xfffc}, {16,0xfffd}, {16,0xfffe}}
};

constexpr uint16_t ac_c_hufftable[][11][2] = {
//{code len, code}
//v Run length    > Code length
  {{ 2,0x0000}, { 2,0x0001}, { 3,0x0004}, { 4,0x000a}, { 5,0x0018}, { 5,0x0019}, { 6,0x0038}, { 7,0x0078}, { 9,0x01f4}, {10,0x03f6}, {12,0x0ff4}},
  {{ 0,0xdead}, { 4,0x000b}, { 6,0x0039}, { 8,0x00f6}, { 9,0x01f5}, {11,0x07f6}, {12,0x0ff5}, {16,0xff88}, {16,0xff89}, {16,0xff8a}, {16,0xff8b}},
  {{ 0,0xdead}, { 5,0x001a}, { 8,0x00f7}, {10,0x03f7}, {12,0x0ff6}, {15,0x7fc2}, {16,0xff8c}, {16,0xff8d}, {16,0xff8e}, {16,0xff8f}, {16,0xff90}},
  {{ 0,0xdead}, { 5,0x001b}, { 8,0x00f8}, {10,0x03f8}, {12,0x0ff7}, {16,0xff91}, {16,0xff92}, {16,0xff93}, {16,0xff94}, {16,0xff95}, {16,0xff96}},
  {{ 0,0xdead}, { 6,0x003a}, { 9,0x01f6}, {16,0xff97}, {16,0xff98}, {16,0xff99}, {16,0xff9a}, {16,0xff9b}, {16,0xff9c}, {16,0xff9d}, {16,0xff9e}},
  {{ 0,0xdead}, { 6,0x003b}, {10,0x03f9}, {16,0xff9f}, {16,0xffa0}, {16,0xffa1}, {16,0xffa2}, {16,0xffa3}, {16,0xffa4}, {16,0xffa5}, {16,0xffa6}},
  {{ 0,0xdead}, { 7,0x0079}, {11,0x07f7}, {16,0xffa7}, {16,0xffa8}, {16,0xffa9}, {16,0xffaa}, {16,0xffab}, {16,0xffac}, {16,0xffad}, {16,0xffae}},
  {{ 0,0xdead}, { 7,0x007a}, {11,0x07f8}, {16,0xffaf}, {16,0xffb0}, {16,0xffb1}, {16,0xffb2}, {16,0xffb3}, {16,0xffb4}, {16,0xffb5}, {16,0xffb6}},
  {{ 0,0xdead}, { 8,0x00f9}, {16,0xffb7}, {16,0xffb8}, {16,0xffb9}, {16,0xffba}, {16,0xffbb}, {16,0xffbc}, {16,0xffbd}, {16,0xffbe}, {16,0xffbf}},
  {{ 0,0xdead}, { 9,0x01f7}, {16,0xffc0}, {16,0xffc1}, {16,0xffc2}, {16,0xffc3}, {16,0xffc4}, {16,0xffc5}, {16,0xffc6}, {16,0xffc7}, {16,0xffc8}},
  {{ 0,0xdead}, { 9,0x01f8}, {16,0xffc9}, {16,0xffca}, {16,0xffcb}, {16,0xffcc}, {16,0xffcd}, {16,0xffce}, {16,0xffcf}, {16,0xffd0}, {16,0xffd1}},
  {{ 0,0xdead}, { 9,0x01f9}, {16,0xffd2}, {16,0xffd3}, {16,0xffd4}, {16,0xffd5}, {16,0xffd6}, {16,0xffd7}, {16,0xffd8}, {16,0xffd9}, {16,0xffda}},
  {{ 0,0xdead}, { 9,0x01fa}, {16,0xffdb}, {16,0xffdc}, {16,0xffdd}, {16,0xffde}, {16,0xffdf}, {16,0xffe0}, {16,0xffe1}, {16,0xffe2}, {16,0xffe3}},
  {{ 0,0xdead}, {11,0x07f9}, {16,0xffe4}, {16,0xffe5}, {16,0xffe6}, {16,0xffe7}, {16,0xffe8}, {16,0xffe9}, {16,0xffea}, {16,0xffeb}, {16,0xffec}},
  {{ 0,0xdead}, {14,0x3fe0}, {16,0xffed}, {16,0xffee}, {16,0xffef}, {16,0xfff0}, {16,0xfff1}, {16,0xfff2}, {16,0xfff3}, {16,0xfff4}, {16,0xfff5}},
  {{10,0x03fa}, {15,0x7fc3}, {16,0xfff6}, {16,0xfff7}, {16,0xfff8}, {16,0xfff9}, {16,0xfffa}, {16,0xfffb}, {16,0xfffc}, {16,0xfffd}, {16,0xfffe}}
};


struct jpegheader {
  static constexpr uint8_t SOI[] = {0xff, 0xd8};
  static constexpr uint8_t DQT[] = {0xff, 0xdb, 0x00, 0x84};
  static constexpr uint8_t DHT[] = {0xff, 0xc4, 0x01, 0xa2,
    0x00, // DC - y
    0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x01, // DC - c
    0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x10, // AC - y
    0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D,
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 
    0xF9, 0xFA,
    0x11, // AC - c
    0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77,
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 
    0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 
    0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 
    0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 
    0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 
    0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 
    0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 
    0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 
    0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 
    0xF9, 0xFA
  };
  static constexpr uint8_t SOF1[] = {0xff, 0xc0, 0x00, 8+3*3, 0x08};
  static constexpr uint8_t SOF2[] = {
    3,
    0x01, 0x11, 0x00,
    0x02, 0x11, 0x01,
    0x03, 0x11, 0x01
  };
  static constexpr uint8_t SOS[] = {
    0xff, 0xda,
    0x00, 6+2*3,
    0x03,
    0x01, 0x00,
    0x02, 0x11,
    0x03, 0x11,
    0x00, 0x3f, 0x00
  };
  static constexpr uint8_t EOI[] = {0xff, 0xD9};
  int   len = 0;
  char  h[4096];
  jpegheader(uint16_t y, uint16_t x) : h() {
    int acc = 0;
    const auto zz = zigzag().walk;
    for (int i = 0; i < sizeof(SOI); i++) h[acc++] = SOI[i];
    for (int i = 0; i < sizeof(DQT); i++) h[acc++] = DQT[i];
    h[acc++] = 0;
    for (int i = 0; i < 64; i++) {
      uint16_t q = 1 << qtable_y[zz[i][0]][zz[i][1]];
      h[acc++] = q>255 ? 255 : q;
    }
    h[acc++] = 1;
    for (int i = 0; i < 64; i++) {
      uint16_t q = 1 << qtable_c[zz[i][0]][zz[i][1]];
      h[acc++] = q>255 ? 255 : q;
    }
    //for (int i = 0; i < sizeof(DHT); i++) h[acc++] = DHT[i];
    for (int i = 0; i < sizeof(SOF1); i++) h[acc++] = SOF1[i];
    h[acc++] = y >> 8;
    h[acc++] = y & 0xff;
    h[acc++] = x >> 8;
    h[acc++] = x & 0xff;
    for (int i = 0; i < sizeof(SOF2); i++) h[acc++] = SOF2[i];
    for (int i = 0; i < sizeof(SOS); i++) h[acc++] = SOS[i];
    len = acc;
  }
};


bool load_header(uint8_t const* bmp, const int len, bmpheader& bh) {
  constexpr int len_head = sizeof(bmpheader::raw);
  if(len < len_head) return false;
  for(int i = 0; i < len_head; i++) bh.raw[i] = bmp[i+2];
  return
    bmp[0]=='B' && bmp[1]=='M' &&
    bh.data.bfReserved1 == 0 &&
    bh.data.bfReserved2 == 0 &&
    bh.data.bcSize > 12 &&
    bh.data.bcPlanes == 1 &&
    bh.data.bcBitCount == 24 &&
    bh.data.biCompression == 0;
}
static inline int padnum(int b, int align) {
  return b%align ? align-b%align : 0;
}

template<typename fsxy>
void dct_q(coeff_t& coeff, fsxy sxy, const uint8_t (&q)[8][8]) {
  constexpr auto   costbl = costable();
  constexpr double isqrt2 = 1/sqrt(2);
  for (int j = 0; j < coeff.size();    j += 8)
  for (int i = 0; i < coeff[0].size(); i += 8) {
    //cout << "(" << j << "," << i << ")\n";
    for (int v = 0; v < 8; v++)
    for (int u = 0; u < 8; u++) {
      if(q[v][u] >= 8) {coeff[j+v][i+u] = 0; break;}
      int a = 0;
      for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++) {
        a += (sxy(j+y, i+x)-128) *
          costbl.v[u][x] *
          costbl.v[v][y];
      }
      double ce =
        0.25 * a * (
        (u && v) ? 1.0    :
        (u || v) ? isqrt2 : 0.5);
      ce /= (1<<q[v][u]);
      assert(-128<=ce && ce<128);
      coeff[j+v][i+u] = ce;
    }
  }
}

class bitstream {
private:
  constexpr static char zero = 0;
  int       len_acc = 0;
  uint8_t   acc = 0;
  ofstream& o;
public:
  bitstream(ofstream& out) : o(out) {}
  void append(const int len, const uint32_t val) {
    assert(0<=len && len<=32 && len_acc<8);
    if(len <= 0) return;
    int rshift = len - (8 - len_acc);
    uint8_t valmask = ~(0xFF<<(8-len_acc));
    if(rshift>=0) acc |= valmask & (val>> rshift);
    else          acc |= valmask & (val<<-rshift);
    if(len+len_acc < 8) {
      len_acc += len;
      return;
    }
    // write and continue
    o.write((char*)&acc, 1);
    if(acc==0xFF) o.write(&zero, 1);
    acc = 0;
    int len_wrote_val = 8 - len_acc;
    len_acc = 0;
    append(len-len_wrote_val, val);
  }
  void append(const uint16_t hufftable[2]) {
    append(hufftable[0], hufftable[1]);
  }
  void finish() { append(8-len_acc, ~0); }
};

struct mcu_encoder {
private:
  int8_t last_dc = 0;
  const uint16_t (&dc_hufftable)[12][2];
  const uint16_t (&ac_hufftable)[16][11][2];
public:
  mcu_encoder(bool is_y) :
    dc_hufftable(is_y ? dc_y_hufftable : dc_c_hufftable),
    ac_hufftable(is_y ? ac_y_hufftable : ac_c_hufftable) {}
  static int bitlen (const int16_t v) {
    for (int i = 0; i < 15; i++) {
      if(-(1<<i)<v && v<(1<<i)) return  i;
    }
    assert(0 && "invalid bitlen");
  }
  void encode (
      bitstream& bs,
      const coeff_t& coeff, int offy, int offx) {
    // dc
    const int8_t    dc = coeff[offy][offx];
    const int16_t   ddc = dc - last_dc;
    const int dbl = bitlen(ddc);
    bs.append(dc_hufftable[dbl]);
    bs.append(dbl, ddc<0 ? ddc-1 : ddc);
    last_dc = dc;

    // ac
    int runlen = 0;
    const auto zz = zigzag().walk;
    for (int i = 1; i < 64; i++) {
      const auto idx = zz[i];
      const int8_t ac = coeff[offy+idx[0]][offx+idx[1]];
      printf("%2x,", coeff[offy+idx[0]][offx+idx[1]]);
      if(ac==0) { runlen++; continue; }
      // ac is nonzero
      const int abl = bitlen(ac);
      assert(abl>0);
      while(runlen>15) {
        // insert ZRL
        bs.append(ac_hufftable[15][0]);
        runlen -= 16;
      }
      bs.append(ac_hufftable[runlen][abl]);
      bs.append(abl, ac<0 ? ac-1 : ac);
      runlen = 0;
    }
    printf("\n");
    if(!coeff[7][7]) {
      // add EOB
      bs.append(ac_hufftable[0][0]);
    }
  }
};

void bitlentest() {
  assert(mcu_encoder::bitlen( 0) == 0);
  assert(mcu_encoder::bitlen( 1) == 1);
  assert(mcu_encoder::bitlen(-1) == 1);
  assert(mcu_encoder::bitlen( 2) == 2);
  assert(mcu_encoder::bitlen( 3) == 2);
  assert(mcu_encoder::bitlen(-2) == 2);
  assert(mcu_encoder::bitlen(-3) == 2);
  assert(mcu_encoder::bitlen( 4) == 3);
  assert(mcu_encoder::bitlen( 7) == 3);
  assert(mcu_encoder::bitlen(-4) == 3);
  assert(mcu_encoder::bitlen(-7) == 3);
  assert(mcu_encoder::bitlen( 8) == 4);
  assert(mcu_encoder::bitlen(-8) == 4);
  cout << "OK\n";
}


int main(int argc, char const* argv[]) {
  ios::sync_with_stdio(false);
  if(argc!=2) {
    cout << "usage: bmp2jpeg filename" << endl;
    return 1;
  }

  // Read bmp
  ifstream bmps(argv[1], ios::in | ios::binary | ios::ate);
  if(!bmps.is_open()) {
    cout << "failed to read " << argv[1] << endl;
    return 2;
  }

  const streampos size = bmps.tellg();
  auto* const bmp = new uint8_t[size];
  bmps.seekg(0, ios::beg);
  bmps.read((char*)bmp, size);
  bmps.close();

  bmpheader bh;
  if(!load_header(bmp, size, bh)) {
    cout << "invalid or unexpected bmp" << endl;
    return 2;
  }

  cout << dec << bh.data.bcWidth << 'x' << dec << bh.data.bcHeight << endl;

  vector<vector<YCbCr>> plane(bh.data.bcHeight/2, vector<YCbCr>());
  const uint8_t* line = bmp+bh.data.bfOffBits;
  for (int y = 0; y < bh.data.bcHeight; y+=2) {
    for (int x = 0; x < bh.data.bcWidth; x+=2) {
      plane[y/2].push_back(YCbCr(line + 3*x));
    }
    const int w3 = bh.data.bcWidth*3*2;
    line = line + w3 + padnum(w3,4);
  }

  // Align plane
  const int ypad = padnum(plane.size(), 8);
  const int xpad = padnum(plane[0].size(), 8);
  for (auto&& l : plane) {
    for (int i = 0; i < xpad; i++) l.push_back(l.back());
  }
  for (int i = 0; i < ypad; i++) {
    plane.push_back(plane.back());
  }

  // DCT and Quantize
  const int
    height= plane.size(),
    width = plane[0].size();
  coeff_t
    ycoeff(height, vector<int8_t>(width)),
    cbcoeff(height, vector<int8_t>(width)),
    crcoeff(height, vector<int8_t>(width));
  auto sxy_y  = [&](int y,int x) {return plane[y][x].Y;};
  auto sxy_cb = [&](int y,int x) {return plane[y][x].Cb;};
  auto sxy_cr = [&](int y,int x) {return plane[y][x].Cr;};
  dct_q( ycoeff, sxy_y,  qtable_y);
  dct_q(cbcoeff, sxy_cb, qtable_c);
  dct_q(crcoeff, sxy_cr, qtable_c);

  // Output jpeg header
  ofstream jpeg("/tmp/po.jpg", ios::out | ios::binary | ios::trunc);
  if(!jpeg.is_open()) {cout << "dame" << endl; return 4;}
  const auto header = jpegheader(bh.data.bcHeight/2, bh.data.bcWidth/2);
  jpeg.write(header.h, header.len);

  // huffman encoding
  auto bs = bitstream(jpeg);
  auto y_enc  = mcu_encoder(true);
  auto cb_enc = mcu_encoder(false);
  auto cr_enc = mcu_encoder(false);
  for (int j = 0; j < ycoeff.size();    j += 8)
  for (int i = 0; i < ycoeff[0].size(); i += 8) {
    printf("[%6d-%6d, %6d-%6d]\n", j, j+7, i, i+7);
    y_enc.encode (bs, ycoeff, j, i);
    cb_enc.encode(bs, cbcoeff, j, i);
    cr_enc.encode(bs, crcoeff, j, i);
  }
  bs.finish();

  // Output jpeg footer
  jpeg.write((char*)header.EOI, sizeof(header.EOI));



  return 0;
}

void appendtest(bitstream& bs) {
  bs.append( 8, 0x10);
  bs.append( 8, 0x11);
  bs.append( 8, 0x10);
  bs.append( 8, 0x11);
  bs.append( 8, 0xFF); // added 00
  bs.append(16, 0x1010);
  bs.append(16, 0x1111);
  bs.append(16, 0x1010);
  bs.append(16, 0x1111);
  bs.append(16, 0xFFFF); // added two 00
  // 01
  bs.append( 4, 0x0);
  bs.append( 4, 0xF1);
  // 81
  bs.append( 1, 0xF1);
  bs.append( 7, 0x01);
  // 81
  bs.append( 7, 0x40);
  bs.append( 1, 0xF1);
  // AA
  bs.append( 3, 0xF5);
  bs.append( 3, 0xF2);
  bs.append( 2, 0xF2);
  // 555A
  bs.append(12, 0x555);
  bs.append( 4, 0xFA);
  // A555
  bs.append( 4, 0xFA);
  bs.append(12, 0x555);

  // 77FF00 including padding
  bs.append(9, 0x0EF);
  // 0 1110 1111 111
  bs.finish();
}


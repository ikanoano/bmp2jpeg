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
  {4, 4, 5, 6, 7, 8, 8, 8},
  {4, 5, 6, 7, 8, 8, 8, 8},
  {5, 6, 7, 8, 8, 8, 8, 8},
  {6, 7, 8, 8, 8, 8, 8, 8},
  {7, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8}
};
constexpr uint8_t qtable_c[8][8] = {
  {4, 5, 6, 7, 8, 8, 8, 8},
  {5, 6, 7, 8, 8, 8, 8, 8},
  {6, 7, 8, 8, 8, 8, 8, 8},
  {7, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8},
  {8, 8, 8, 8, 8, 8, 8, 8}
};
constexpr uint8_t zigzag[][2] = { // saiaku
  {0, 1}, {1, 0},
  {2, 0}, {1, 1}, {0, 2},
  {0, 3}, {1, 2}, {2, 1}, {3, 0},
  {4, 0}, {3, 1}, {2, 2}, {1, 3}, {0, 4},
  {0, 5}
};

constexpr uint16_t dc_y_hufftable[][2] = {
//{len, code}
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
//{len, code}
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
//{len, code}
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
//{len, code}
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

template<typename fsxy, typename fq>
void dct_q(coeff_t& coeff, fsxy sxy, fq q) {
  constexpr auto   costbl = costable();
  constexpr double isqrt2 = 1/sqrt(2);
  for (int j = 0; j < coeff.size();    j += 8)
  for (int i = 0; i < coeff[0].size(); i += 8) {
    cout << "(" << j << "," << i << ")\n";
    for (int v = 0; v < 8; v++)
    for (int u = 0; u < 8; u++) {
      int a = 0;
      for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++) {
        a += sxy(j+y, i+x) *
          costbl.v[u][x] *
          costbl.v[v][y];
      }
      double ce =
        0.25 * a * (
        (u && v) ? 1.0    :
        (u || v) ? isqrt2 : 0.5);
      ce /= (1<<q(v,u));
      assert(-128<=ce && ce<128);
      coeff[j+v][i+u] = ce;
    }
  }
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

  vector<vector<YCbCr>> plane(bh.data.bcHeight, vector<YCbCr>());
  const uint8_t* line = bmp+bh.data.bfOffBits;
  for (int y = 0; y < bh.data.bcHeight; y++) {
    for (int x = 0; x < bh.data.bcWidth; x++) {
      plane[y].push_back(YCbCr(line + 3*x));
    }
    const int w3 = bh.data.bcWidth*3;
    line = line + w3 + padnum(w3,4);
  }

  // Align plane
  const int ypad = padnum(plane.size(), 16);
  const int xpad = padnum(plane[0].size(), 16);
  for (auto&& l : plane) {
    for (int i = 0; i < xpad; i++) l.push_back(l.back());
  }
  for (int i = 0; i < ypad; i++) {
    plane.push_back(plane.back());
  }

  // DCT and Quantize
  const int
    y_height= plane.size(),
    y_width = plane[0].size(),
    c_height= plane.size()/2,
    c_width = plane[0].size()/2;
  coeff_t
    ycoeff(y_height, vector<int8_t>(y_width)),
    cbcoeff(c_height, vector<int8_t>(c_width)),
    crcoeff(c_height, vector<int8_t>(c_width));
  auto sxy_y  = [&](int y,int x) {return plane[y][x].Y;};
  auto sxy_cb = [&](int y,int x) {return plane[y][x].Cb;};
  auto sxy_cr = [&](int y,int x) {return plane[y][x].Cr;};
  auto q_y    = [&](int y,int x) {return qtable_y[y][x];};
  auto q_c    = [&](int y,int x) {return qtable_c[y][x];};
  dct_q( ycoeff, sxy_y,  q_y);
  //dct_q(cbcoeff, sxy_cb, q_c);
  //dct_q(crcoeff, sxy_cr, q_c);

  ofstream test("/tmp/po.bmp", ios::out | ios::binary | ios::trunc);
  if(!test.is_open()) {cout << "dame" << endl; return 3;}
  test.write((char*)bmp, bh.data.bfOffBits);

  for (int y = 0; y < bh.data.bcHeight; y++) {
    for (int x = 0; x < bh.data.bcWidth; x++) {
      int8_t dd = ycoeff[y][x];
      if(dd<0 && y && x) dd = -dd; // ac component
      char d = dd;
      test.write(&d, 1);
      test.write(&d, 1);
      test.write(&d, 1);
    }
    const int w3 = bh.data.bcWidth*3;
    test.write("\0\0\0", padnum(w3,4));
  }



  return 0;
}

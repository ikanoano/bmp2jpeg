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

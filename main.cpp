#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>
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
int padnum(int b) {
  return b%4 ? 4-b%4 : 0;
}

int main(int argc, char const* argv[]) {
  if(argc!=2) {
    cout << "usage: bmp2jpeg filename" << endl;
    return 1;
  }

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
    line = line + w3 + padnum(w3);
  }

  ofstream test("/tmp/po.bmp", ios::out | ios::binary | ios::trunc);
  if(!test.is_open()) {cout << "dame" << endl; return 3;}
  test.write((char*)bmp, bh.data.bfOffBits);

  for (int y = 0; y < bh.data.bcHeight; y++) {
    for (int x = 0; x < bh.data.bcWidth; x++) {
      const char d = plane[y][x].Cb;
      test.write(&d, 1);
      test.write(&d, 1);
      test.write(&d, 1);
    }
    const int w3 = bh.data.bcWidth*3;
    test.write("\0\0\0", padnum(w3));
  }


  return 0;
}

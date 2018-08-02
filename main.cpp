#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <functional>
#include "consts.hpp"
using namespace std;

struct bmpheader {
  union {
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

  bool read_header(uint8_t const* bmp, const int len) {
    constexpr int len_head = sizeof(raw);
    if(len < len_head) return false;
    for(int i = 0; i < len_head; i++) raw[i] = bmp[i+2];
    return
      bmp[0]=='B' && bmp[1]=='M' &&
      data.bfReserved1 == 0 &&
      data.bfReserved2 == 0 &&
      data.bcSize > 12 &&
      data.bcPlanes == 1 &&
      data.bcBitCount == 24 &&
      data.biCompression == 0;
  }
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
  constexpr static int    scale = 7;
  int8_t v[8][8];
  constexpr costable() : v() {
    for (int j = 0; j < 8; j++)
    for (int i = 0; i < 8; i++) {
      int16_t tmp = cos((2*i+1)*j*PI/16) * (1<<scale);
      v[j][i] = tmp==128 ? 127 : tmp;
    }
  }
};

struct jpegheader {
  static constexpr uint8_t SOI[] = {0xff, 0xd8};
  static constexpr uint8_t DQT[] = {0xff, 0xdb, 0x00, 0x84};
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
    static constexpr  auto  zzc = zigzag();
    const auto  zz = zzc.walk;
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


template<bool is_y, int dct_th>
class component_encoder {
private:
  int16_t                 last_dc = 0;
  const int               h_mcu;
  vector<array<int16_t,dct_th>> sum_acc;
  const uint8_t   (&qtable)[8][8]           = is_y ? qtable_y       : qtable_c;
  const uint16_t  (&dc_hufftable)[12][2]    = is_y ? dc_y_hufftable : dc_c_hufftable;
  const uint16_t  (&ac_hufftable)[16][11][2]= is_y ? ac_y_hufftable : ac_c_hufftable;
  static int bitlen (const int16_t v) {
    for (int i = 0; i < 15; i++) {
      if(-(1<<i)<v && v<(1<<i)) return  i;
    }
    assert(0 && "invalid bitlen");
  }
public:
  component_encoder(int width) :
    h_mcu(width / 8),
    sum_acc(vector<array<int16_t,dct_th>>(h_mcu))
    {}
  void encode(
      const uint8_t pix, bitstream& bs,
      int x_mcu, int y_in_mcu, int x_in_mcu) {
    assert(x_mcu < h_mcu);

    // level shift
    const int16_t sxy = (int16_t)pix - 128;

    // dct
    static constexpr  auto  costblc = costable();
    const auto  costbl = costblc.v;
    static constexpr  auto  zzc = zigzag();
    const auto  zz = zzc.walk;
    for (int i = 0; i < dct_th; i++) {
      const auto v = zz[i][0];
      const auto u = zz[i][1];
      // reset sum when y==x==0
      const int16_t s = y_in_mcu+x_in_mcu ? sum_acc[x_mcu][i] : 0;
      const int16_t m = (int32_t)sxy *
        costbl[v][y_in_mcu] * costbl[u][x_in_mcu] >> costable::scale*2;
      sum_acc[x_mcu][i] = s + m + (m<0 ? 1 : 0);
    }

    if(y_in_mcu<7 || x_in_mcu<7) return;
    //printf("mcu[%3d] is filled\n", x_mcu);

    static const int isqrt_scale = 4;
    static const int16_t isqrt2 = 1/sqrt(2) * (1<<isqrt_scale);
    int runlen = 0;
    for (int i = 0; i < dct_th && runlen < 16; i++) {
      const auto v = zz[i][0];
      const auto u = zz[i][1];

      // rest of dct and quantize
      int16_t sq =
        (u && v) ? (int32_t)sum_acc[x_mcu][i]          >> (2+qtable[v][u]) :
        (u || v) ? (int32_t)(sum_acc[x_mcu][i]*isqrt2) >> (2+qtable[v][u]+isqrt_scale) :
                   (int32_t)sum_acc[x_mcu][i]          >> (3+qtable[v][u]);
      if(sq<0) sq++;
      // equivalent to:
      //double sq =
      //  0.25 * sum_acc[x_mcu][i] * (
      //      (u && v) ? 1.0    :
      //      (u || v) ? isqrt2 : 0.5);
      //sq /= (1<<qtable[v][u]);
      assert(-256<=sq && sq<256);

      // huffman encode
      if(u || v) {
        // ac
        const int16_t ac = sq;
        if(ac==0) { runlen++; continue; }
        // ac is nonzero
        const int abl = bitlen(ac);
        assert(abl>0);
        assert(runlen<16);
        bs.append(ac_hufftable[runlen][abl]);
        bs.append(abl, ac<0 ? ac-1 : ac);
        runlen = 0;
      } else {
        // dc
        const int16_t   dc  = sq;
        const int16_t   ddc = dc - last_dc;
        const int dbl = bitlen(ddc);
        bs.append(dc_hufftable[dbl]);
        bs.append(dbl, ddc<0 ? ddc-1 : ddc);
        last_dc = dc;
      }
    }
    if(dct_th<64 || runlen) {
      // add EOB
      bs.append(ac_hufftable[0][0]);
    }
  }
};

static inline int padnum(int b, int align) {return b%align ? align-b%align : 0; }
int main(int argc, char const* argv[]) {
  ios::sync_with_stdio(false);
  if(argc!=2) {
    cout << "usage: bmp2jpeg filename" << endl;
    return 1;
  }

  // read bmp
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
  if(!bh.read_header(bmp, size)) {
    cout << "invalid or unexpected bmp" << endl;
    return 3;
  }

  cout << bh.data.bcHeight << 'x' << bh.data.bcWidth << endl;

  // convert color space, vertically invert
  vector<vector<YCbCr>> plane(bh.data.bcHeight/2, vector<YCbCr>());
  const uint8_t* line = bmp+bh.data.bfOffBits;
  for (int y = 0; y < bh.data.bcHeight; y+=2) {
    for (int x = 0; x < bh.data.bcWidth; x+=2) {
      plane[(bh.data.bcHeight-y-1)/2].push_back(YCbCr(line + 3*x));
    }
    const int w3 = bh.data.bcWidth*3*2;
    line = line + w3 + padnum(w3,4);
  }

  // padding
  const int ypad = padnum(plane.size(), 8);
  const int xpad = padnum(plane[0].size(), 8);
  for (auto&& l : plane) { l.insert(l.end(), xpad, l.back()); }
  plane.insert(plane.end(), ypad, plane.back());

  cout << plane.size() << 'x' << plane[0].size() << endl;

  // output jpeg header
  ofstream jpeg("/tmp/po.jpg", ios::out | ios::binary | ios::trunc);
  if(!jpeg.is_open()) {cout << "dame" << endl; return 4;}
  const auto header = jpegheader(bh.data.bcHeight/2, bh.data.bcWidth/2);
  jpeg.write(header.h, header.len);

  // output jpeg entropy-coded data
  auto      bs    = bitstream(jpeg);
  const int ysize = plane.size();
  const int xsize = plane[0].size();
  auto   yenc = component_encoder<true,  49>(xsize);
  auto  cbenc = component_encoder<false, 10>(xsize);
  auto  crenc = component_encoder<false, 10>(xsize);
  for (int y = 0; y < ysize; y++)
  for (int x = 0; x < xsize; x++) {
     yenc.encode(plane[y][x].Y,  bs, x>>3, y&7, x&7);
    cbenc.encode(plane[y][x].Cb, bs, x>>3, y&7, x&7);
    crenc.encode(plane[y][x].Cr, bs, x>>3, y&7, x&7);
  }

  // output jpeg footer
  bs.finish();
  jpeg.write((char*)header.EOI, sizeof(header.EOI));

  return 0;
}


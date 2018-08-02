constexpr uint8_t qtable_y[8][8] = {
  {2, 2, 2, 3, 3, 3, 3, 4},
  {2, 2, 3, 3, 3, 3, 4, 4},
  {2, 3, 3, 3, 3, 4, 4, 4},
  {3, 3, 3, 3, 4, 4, 4, 5},
  {3, 3, 3, 4, 4, 4, 5, 5},
  {3, 3, 4, 4, 4, 5, 5, 5},
  {3, 4, 4, 4, 5, 5, 5, 5},
  {4, 4, 4, 5, 5, 5, 5, 5}
};

constexpr uint8_t qtable_c[8][8] = {
  {3, 4, 5, 6, 7, 7, 7, 7},
  {4, 5, 6, 7, 7, 7, 7, 8},
  {5, 6, 7, 7, 7, 7, 8, 8},
  {6, 7, 7, 7, 7, 8, 8, 8},
  {7, 7, 7, 7, 8, 8, 8, 8},
  {7, 7, 7, 8, 8, 8, 8, 8},
  {7, 7, 8, 8, 8, 8, 8, 8},
  {7, 8, 8, 8, 8, 8, 8, 8}
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

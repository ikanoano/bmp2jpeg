
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

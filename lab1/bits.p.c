#line 31 "<command-line>"
#include "/usr/include/stdc-predef.h"
#line 14 "bits.c"
#include "btest.h"
#include "/usr/lib/gcc/x86_64-linux-gnu/6/include-fixed/limits.h"
#line 22
team_struct team=
{
#line 28
    "516072910016",

   "Liu Qingyuan",

   "516072910016",
#line 36
   "",

   ""};
#line 177
int bang(int x) {
#line 184
  int judge=(  x |( ~x+1));
  judge = judge>>31;
  return judge+1;
}
#line 195
int bitCount(int x) {

  int mask=  0x11 |( 0x11<<8);
  int cnt;
  mask = mask |( mask<<16);
  cnt = x & mask;
  cnt += x>>1 & mask;
  cnt += x>>2 & mask;
  cnt += x>>3 & mask;

  cnt = cnt +( cnt >> 16);


  mask = 0xf +( 0xf<<8);
  cnt =( cnt & mask) +(( cnt>>4) & mask);
  cnt = cnt +( cnt>>8);
  return cnt & 0x3f;
}
#line 220
int copyLSB(int x) {

  int judge=  x & 0x1;
  return (~judge+1);
}
#line 233
int divpwr2(int x, int n) {
  int sign=(  x>>31) & 0x1;
  int modified_x= x+(sign<<n)+(~sign+1);
  return modified_x>>n;
}
#line 244
int evenBits(void) {

  int mask=(0x55<<8)|0x55;
  mask=(mask<<16)|mask;
  return mask;
}
#line 259
int fitsBits(int x, int n) {
#line 264
  int mask=  x >>( n +( ~0));
  return !((x>>31)^mask);
}
#line 275
int getByte(int x, int n) {
  int mask=0xff;
  return (x>>( n<<3)) & mask;
}
#line 286
int isGreater(int x, int y) {
#line 290
  int judge=  x +( ~(1 + y) + 1);
  int xm=(  x>>31) + 1;
  int ym=(  y>>31) + 1;
  judge = judge &( 0x1<<31);

  return (xm &( ~ym)) |(( xm |( ~ym)) &( !(judge>>31)));
}
#line 304
int isNonNegative(int x) {
  return (x>>31) + 1;
}
#line 314
int isNotEqual(int x, int y) {
  int judge=  x +( ~y + 1);
  return !!judge;
}
#line 326
int isPower2(int x) {

  int xm=  1 +( x>>31);
  int mask=  0x11 |( 0x11<<8);
  int cnt;
  mask = mask |( mask<<16);
  cnt = x & mask;
  cnt += x>>1 & mask;
  cnt += x>>2 & mask;
  cnt += x>>3 & mask;

  cnt = cnt +( cnt >> 16);

  mask = 0xf +( 0xf<<8);
  cnt =( cnt & mask) +(( cnt>>4) & mask);
  cnt = cnt +( cnt>>8);
  cnt = cnt & 0x3f;
  return ((!(cnt>>1)) & xm & cnt);

}
#line 354
int leastBitPos(int x) {
  return (x ^( x +( ~0))) & x;
}
#line 365
int logicalShift(int x, int n) {
  int mask=(  ~0)<<(33 +( ~n));
  mask = ~mask;
  return (x>>n) & mask;
}
#line 380
int satAdd(int x, int y) {

  int xm=(x>>31) & 1;
  int ym=(y>>31) & 1;
  int sum=x+y;
  int xym=(sum>>31) & 1;
  int overflow=(  !(xm ^ ym)) &( xm ^ xym);
  int shift=(  overflow<<5)+(~overflow+1);
  return (sum>>shift);
}
#line 399
int tc2sm(int x) {
  return 2;
}

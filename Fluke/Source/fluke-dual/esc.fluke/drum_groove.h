/* Original Fluke rhythm sketches. Inspired by the shared-map/strength principle
 * described in Grids; no Grids firmware or pattern tables are used here. */
#ifndef FLUKE_DRUM_GROOVE_H
#define FLUKE_DRUM_GROOVE_H
/* Four related groove corners, six complementary roles, strong/medium/detail. */
static const unsigned short drum_sketch[4][6][3]={
 {{0x0101,0x1010,0x4044},{0x1010,0x8080,0x0404},{0x5555,0xaaaa,0x0000},{0x4444,0x2222,0x8888},{0x1249,0x8080,0x2424},{0x8000,0x5000,0x2a00}},
 {{0x0401,0x1040,0x8084},{0x1010,0x4080,0x0204},{0x5155,0x84a0,0x2a0a},{0x4244,0x2820,0x8088},{0x1449,0x8200,0x2824},{0x4000,0xa000,0x1500}},
 {{0x1001,0x0440,0x8280},{0x1020,0x8080,0x0408},{0x5555,0x8822,0x2288},{0x2492,0x8040,0x1100},{0x4921,0x8200,0x1440},{0x8000,0x2800,0x5400}},
 {{0x0101,0x4040,0x1210},{0x1010,0x8400,0x0084},{0xaaaa,0x5555,0x0000},{0x8888,0x2222,0x4444},{0x0925,0x4040,0x9290},{0x8080,0x6000,0x1e00}}
};
static int drum_role(uint32_t seed){return (int)((seed>>13)&7u)%6;}
static uint32_t drum_groove_seed(uint32_t groove,int role,uint32_t detail){return (groove&0xffff0000u)|((unsigned)role<<13)|(detail&8191u);}
static int drum_corner(int corner,int role,int step){
 const unsigned short *m=drum_sketch[corner][role];unsigned bit=1u<<step;
 return m[0]&bit?240:m[1]&bit?145:m[2]&bit?70:8;
}
static int drum_map_level(uint32_t seed,int step){
 int x=(seed>>24)&255,y=(seed>>16)&255,role=drum_role(seed);
 int a=drum_corner(0,role,step)*(255-x)+drum_corner(1,role,step)*x;
 int b=drum_corner(2,role,step)*(255-x)+drum_corner(3,role,step)*x;
 return (a*(255-y)+b*y+32512)/65025;
}
#endif

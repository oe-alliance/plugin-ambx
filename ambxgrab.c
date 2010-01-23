/* 
Based on
AiO Dreambox Screengrabber v0.81
*/

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include <linux/fb.h>
#include "ambxlib.h"
#include "colorproc.h"

#define CLAMP(x)    ((x < 0) ? 0 : ((x > 255) ? 255 : x))
#define SWAP(x,y)	{ x ^= y; y ^= x; x ^= y; }

#define RED565(x)    ((((x) >> (11 )) & 0x1f) << 3)
#define GREEN565(x)  ((((x) >> (5 )) & 0x3f) << 2)
#define BLUE565(x)   ((((x) >> (0)) & 0x1f) << 3)

#define YFB(x)    ((((x) >> (10)) & 0x3f) << 2)
#define CBFB(x)  ((((x) >> (6)) & 0xf) << 4)
#define CRFB(x)   ((((x) >> (2)) & 0xf) << 4)
#define BFFB(x)   ((((x) >> (0)) & 0x3) << 6)

#define VIDEO_DEV "/dev/video"

// dont change SPARE_RAM and DMA_BLOCKSIZE until you really know what you are doing !!!
#define SPARE_RAM 252*1024*1024 // the last 4 MB is enough...
#define DMA_BLOCKSIZE 0x3FF000 // should be big enough to hold a complete YUV 1920x1080 HD picture, otherwise it will not work properly on DM8000

// static lookup tables for faster yuv2rgb conversion
static const int yuv2rgbtable_y[256] = {
0xFFED5EA0, 0xFFEE88B6, 0xFFEFB2CC, 0xFFF0DCE2, 0xFFF206F8, 0xFFF3310E, 0xFFF45B24, 0xFFF5853A, 0xFFF6AF50, 0xFFF7D966, 0xFFF9037C, 0xFFFA2D92, 0xFFFB57A8, 0xFFFC81BE, 0xFFFDABD4, 0xFFFED5EA, 0x0, 0x12A16, 0x2542C, 0x37E42, 0x4A858, 0x5D26E, 0x6FC84, 0x8269A, 0x950B0, 0xA7AC6, 0xBA4DC, 0xCCEF2, 0xDF908, 0xF231E, 0x104D34, 0x11774A, 0x12A160, 0x13CB76, 0x14F58C, 0x161FA2, 0x1749B8, 0x1873CE, 0x199DE4, 0x1AC7FA, 0x1BF210, 0x1D1C26, 0x1E463C, 0x1F7052, 0x209A68, 0x21C47E, 0x22EE94, 0x2418AA, 0x2542C0, 0x266CD6, 0x2796EC, 0x28C102, 0x29EB18, 0x2B152E, 0x2C3F44, 0x2D695A, 0x2E9370, 0x2FBD86, 0x30E79C, 0x3211B2, 0x333BC8, 0x3465DE, 0x358FF4, 0x36BA0A, 0x37E420, 0x390E36, 0x3A384C, 0x3B6262, 0x3C8C78, 0x3DB68E, 0x3EE0A4, 0x400ABA, 0x4134D0, 0x425EE6, 0x4388FC, 0x44B312, 0x45DD28, 0x47073E, 0x483154, 0x495B6A, 0x4A8580, 0x4BAF96, 0x4CD9AC, 0x4E03C2, 0x4F2DD8, 0x5057EE, 0x518204, 0x52AC1A, 0x53D630, 0x550046, 0x562A5C, 0x575472, 0x587E88, 0x59A89E, 0x5AD2B4, 0x5BFCCA, 0x5D26E0, 0x5E50F6, 0x5F7B0C, 0x60A522, 0x61CF38, 0x62F94E, 0x642364, 0x654D7A, 0x667790, 0x67A1A6, 0x68CBBC, 0x69F5D2, 0x6B1FE8, 0x6C49FE, 0x6D7414, 0x6E9E2A, 0x6FC840, 0x70F256, 0x721C6C, 0x734682, 0x747098, 0x759AAE, 0x76C4C4, 0x77EEDA, 0x7918F0, 0x7A4306, 0x7B6D1C, 0x7C9732, 0x7DC148, 0x7EEB5E, 0x801574, 0x813F8A, 0x8269A0, 0x8393B6, 0x84BDCC, 0x85E7E2, 0x8711F8, 0x883C0E, 0x896624, 0x8A903A, 0x8BBA50, 0x8CE466, 0x8E0E7C, 0x8F3892, 0x9062A8, 0x918CBE, 0x92B6D4, 0x93E0EA, 0x950B00, 0x963516, 0x975F2C, 0x988942, 0x99B358, 0x9ADD6E, 0x9C0784, 0x9D319A, 0x9E5BB0, 0x9F85C6, 0xA0AFDC, 0xA1D9F2, 0xA30408, 0xA42E1E, 0xA55834, 0xA6824A, 0xA7AC60, 0xA8D676, 0xAA008C, 0xAB2AA2, 0xAC54B8, 0xAD7ECE, 0xAEA8E4, 0xAFD2FA, 0xB0FD10, 0xB22726, 0xB3513C, 0xB47B52, 0xB5A568, 0xB6CF7E, 0xB7F994, 0xB923AA, 0xBA4DC0, 0xBB77D6, 0xBCA1EC, 0xBDCC02, 0xBEF618, 0xC0202E, 0xC14A44, 0xC2745A, 0xC39E70, 0xC4C886, 0xC5F29C, 0xC71CB2, 0xC846C8, 0xC970DE, 0xCA9AF4, 0xCBC50A, 0xCCEF20, 0xCE1936, 0xCF434C, 0xD06D62, 0xD19778, 0xD2C18E, 0xD3EBA4, 0xD515BA, 0xD63FD0, 0xD769E6, 0xD893FC, 0xD9BE12, 0xDAE828, 0xDC123E, 0xDD3C54, 0xDE666A, 0xDF9080, 0xE0BA96, 0xE1E4AC, 0xE30EC2, 0xE438D8, 0xE562EE, 0xE68D04, 0xE7B71A, 0xE8E130, 0xEA0B46, 0xEB355C, 0xEC5F72, 0xED8988, 0xEEB39E, 0xEFDDB4, 0xF107CA, 0xF231E0, 0xF35BF6, 0xF4860C, 0xF5B022, 0xF6DA38, 0xF8044E, 0xF92E64, 0xFA587A, 0xFB8290, 0xFCACA6, 0xFDD6BC, 0xFF00D2, 0x1002AE8, 0x10154FE, 0x1027F14, 0x103A92A, 0x104D340, 0x105FD56, 0x107276C, 0x1085182, 0x1097B98, 0x10AA5AE, 0x10BCFC4, 0x10CF9DA, 0x10E23F0, 0x10F4E06, 0x110781C, 0x111A232, 0x112CC48, 0x113F65E, 0x1152074, 0x1164A8A
};
static const int yuv2rgbtable_ru[256] = {
0xFEFDA500, 0xFEFFA9B6, 0xFF01AE6C, 0xFF03B322, 0xFF05B7D8, 0xFF07BC8E, 0xFF09C144, 0xFF0BC5FA, 0xFF0DCAB0, 0xFF0FCF66, 0xFF11D41C, 0xFF13D8D2, 0xFF15DD88, 0xFF17E23E, 0xFF19E6F4, 0xFF1BEBAA, 0xFF1DF060, 0xFF1FF516, 0xFF21F9CC, 0xFF23FE82, 0xFF260338, 0xFF2807EE, 0xFF2A0CA4, 0xFF2C115A, 0xFF2E1610, 0xFF301AC6, 0xFF321F7C, 0xFF342432, 0xFF3628E8, 0xFF382D9E, 0xFF3A3254, 0xFF3C370A, 0xFF3E3BC0, 0xFF404076, 0xFF42452C, 0xFF4449E2, 0xFF464E98, 0xFF48534E, 0xFF4A5804, 0xFF4C5CBA, 0xFF4E6170, 0xFF506626, 0xFF526ADC, 0xFF546F92, 0xFF567448, 0xFF5878FE, 0xFF5A7DB4, 0xFF5C826A, 0xFF5E8720, 0xFF608BD6, 0xFF62908C, 0xFF649542, 0xFF6699F8, 0xFF689EAE, 0xFF6AA364, 0xFF6CA81A, 0xFF6EACD0, 0xFF70B186, 0xFF72B63C, 0xFF74BAF2, 0xFF76BFA8, 0xFF78C45E, 0xFF7AC914, 0xFF7CCDCA, 0xFF7ED280, 0xFF80D736, 0xFF82DBEC, 0xFF84E0A2, 0xFF86E558, 0xFF88EA0E, 0xFF8AEEC4, 0xFF8CF37A, 0xFF8EF830, 0xFF90FCE6, 0xFF93019C, 0xFF950652, 0xFF970B08, 0xFF990FBE, 0xFF9B1474, 0xFF9D192A, 0xFF9F1DE0, 0xFFA12296, 0xFFA3274C, 0xFFA52C02, 0xFFA730B8, 0xFFA9356E, 0xFFAB3A24, 0xFFAD3EDA, 0xFFAF4390, 0xFFB14846, 0xFFB34CFC, 0xFFB551B2, 0xFFB75668, 0xFFB95B1E, 0xFFBB5FD4, 0xFFBD648A, 0xFFBF6940, 0xFFC16DF6, 0xFFC372AC, 0xFFC57762, 0xFFC77C18, 0xFFC980CE, 0xFFCB8584, 0xFFCD8A3A, 0xFFCF8EF0, 0xFFD193A6, 0xFFD3985C, 0xFFD59D12, 0xFFD7A1C8, 0xFFD9A67E, 0xFFDBAB34, 0xFFDDAFEA, 0xFFDFB4A0, 0xFFE1B956, 0xFFE3BE0C, 0xFFE5C2C2, 0xFFE7C778, 0xFFE9CC2E, 0xFFEBD0E4, 0xFFEDD59A, 0xFFEFDA50, 0xFFF1DF06, 0xFFF3E3BC, 0xFFF5E872, 0xFFF7ED28, 0xFFF9F1DE, 0xFFFBF694, 0xFFFDFB4A, 0x0, 0x204B6, 0x4096C, 0x60E22, 0x812D8, 0xA178E, 0xC1C44, 0xE20FA, 0x1025B0, 0x122A66, 0x142F1C, 0x1633D2, 0x183888, 0x1A3D3E, 0x1C41F4, 0x1E46AA, 0x204B60, 0x225016, 0x2454CC, 0x265982, 0x285E38, 0x2A62EE, 0x2C67A4, 0x2E6C5A, 0x307110, 0x3275C6, 0x347A7C, 0x367F32, 0x3883E8, 0x3A889E, 0x3C8D54, 0x3E920A, 0x4096C0, 0x429B76, 0x44A02C, 0x46A4E2, 0x48A998, 0x4AAE4E, 0x4CB304, 0x4EB7BA, 0x50BC70, 0x52C126, 0x54C5DC, 0x56CA92, 0x58CF48, 0x5AD3FE, 0x5CD8B4, 0x5EDD6A, 0x60E220, 0x62E6D6, 0x64EB8C, 0x66F042, 0x68F4F8, 0x6AF9AE, 0x6CFE64, 0x6F031A, 0x7107D0, 0x730C86, 0x75113C, 0x7715F2, 0x791AA8, 0x7B1F5E, 0x7D2414, 0x7F28CA, 0x812D80, 0x833236, 0x8536EC, 0x873BA2, 0x894058, 0x8B450E, 0x8D49C4, 0x8F4E7A, 0x915330, 0x9357E6, 0x955C9C, 0x976152, 0x996608, 0x9B6ABE, 0x9D6F74, 0x9F742A, 0xA178E0, 0xA37D96, 0xA5824C, 0xA78702, 0xA98BB8, 0xAB906E, 0xAD9524, 0xAF99DA, 0xB19E90, 0xB3A346, 0xB5A7FC, 0xB7ACB2, 0xB9B168, 0xBBB61E, 0xBDBAD4, 0xBFBF8A, 0xC1C440, 0xC3C8F6, 0xC5CDAC, 0xC7D262, 0xC9D718, 0xCBDBCE, 0xCDE084, 0xCFE53A, 0xD1E9F0, 0xD3EEA6, 0xD5F35C, 0xD7F812, 0xD9FCC8, 0xDC017E, 0xDE0634, 0xE00AEA, 0xE20FA0, 0xE41456, 0xE6190C, 0xE81DC2, 0xEA2278, 0xEC272E, 0xEE2BE4, 0xF0309A, 0xF23550, 0xF43A06, 0xF63EBC, 0xF84372, 0xFA4828, 0xFC4CDE, 0xFE5194, 0x100564A
};
static const int yuv2rgbtable_gu[256] = {
0xFFCDD300, 0xFFCE375A, 0xFFCE9BB4, 0xFFCF000E, 0xFFCF6468, 0xFFCFC8C2, 0xFFD02D1C, 0xFFD09176, 0xFFD0F5D0, 0xFFD15A2A, 0xFFD1BE84, 0xFFD222DE, 0xFFD28738, 0xFFD2EB92, 0xFFD34FEC, 0xFFD3B446, 0xFFD418A0, 0xFFD47CFA, 0xFFD4E154, 0xFFD545AE, 0xFFD5AA08, 0xFFD60E62, 0xFFD672BC, 0xFFD6D716, 0xFFD73B70, 0xFFD79FCA, 0xFFD80424, 0xFFD8687E, 0xFFD8CCD8, 0xFFD93132, 0xFFD9958C, 0xFFD9F9E6, 0xFFDA5E40, 0xFFDAC29A, 0xFFDB26F4, 0xFFDB8B4E, 0xFFDBEFA8, 0xFFDC5402, 0xFFDCB85C, 0xFFDD1CB6, 0xFFDD8110, 0xFFDDE56A, 0xFFDE49C4, 0xFFDEAE1E, 0xFFDF1278, 0xFFDF76D2, 0xFFDFDB2C, 0xFFE03F86, 0xFFE0A3E0, 0xFFE1083A, 0xFFE16C94, 0xFFE1D0EE, 0xFFE23548, 0xFFE299A2, 0xFFE2FDFC, 0xFFE36256, 0xFFE3C6B0, 0xFFE42B0A, 0xFFE48F64, 0xFFE4F3BE, 0xFFE55818, 0xFFE5BC72, 0xFFE620CC, 0xFFE68526, 0xFFE6E980, 0xFFE74DDA, 0xFFE7B234, 0xFFE8168E, 0xFFE87AE8, 0xFFE8DF42, 0xFFE9439C, 0xFFE9A7F6, 0xFFEA0C50, 0xFFEA70AA, 0xFFEAD504, 0xFFEB395E, 0xFFEB9DB8, 0xFFEC0212, 0xFFEC666C, 0xFFECCAC6, 0xFFED2F20, 0xFFED937A, 0xFFEDF7D4, 0xFFEE5C2E, 0xFFEEC088, 0xFFEF24E2, 0xFFEF893C, 0xFFEFED96, 0xFFF051F0, 0xFFF0B64A, 0xFFF11AA4, 0xFFF17EFE, 0xFFF1E358, 0xFFF247B2, 0xFFF2AC0C, 0xFFF31066, 0xFFF374C0, 0xFFF3D91A, 0xFFF43D74, 0xFFF4A1CE, 0xFFF50628, 0xFFF56A82, 0xFFF5CEDC, 0xFFF63336, 0xFFF69790, 0xFFF6FBEA, 0xFFF76044, 0xFFF7C49E, 0xFFF828F8, 0xFFF88D52, 0xFFF8F1AC, 0xFFF95606, 0xFFF9BA60, 0xFFFA1EBA, 0xFFFA8314, 0xFFFAE76E, 0xFFFB4BC8, 0xFFFBB022, 0xFFFC147C, 0xFFFC78D6, 0xFFFCDD30, 0xFFFD418A, 0xFFFDA5E4, 0xFFFE0A3E, 0xFFFE6E98, 0xFFFED2F2, 0xFFFF374C, 0xFFFF9BA6, 0x0, 0x645A, 0xC8B4, 0x12D0E, 0x19168, 0x1F5C2, 0x25A1C, 0x2BE76, 0x322D0, 0x3872A, 0x3EB84, 0x44FDE, 0x4B438, 0x51892, 0x57CEC, 0x5E146, 0x645A0, 0x6A9FA, 0x70E54, 0x772AE, 0x7D708, 0x83B62, 0x89FBC, 0x90416, 0x96870, 0x9CCCA, 0xA3124, 0xA957E, 0xAF9D8, 0xB5E32, 0xBC28C, 0xC26E6, 0xC8B40, 0xCEF9A, 0xD53F4, 0xDB84E, 0xE1CA8, 0xE8102, 0xEE55C, 0xF49B6, 0xFAE10, 0x10126A, 0x1076C4, 0x10DB1E, 0x113F78, 0x11A3D2, 0x12082C, 0x126C86, 0x12D0E0, 0x13353A, 0x139994, 0x13FDEE, 0x146248, 0x14C6A2, 0x152AFC, 0x158F56, 0x15F3B0, 0x16580A, 0x16BC64, 0x1720BE, 0x178518, 0x17E972, 0x184DCC, 0x18B226, 0x191680, 0x197ADA, 0x19DF34, 0x1A438E, 0x1AA7E8, 0x1B0C42, 0x1B709C, 0x1BD4F6, 0x1C3950, 0x1C9DAA, 0x1D0204, 0x1D665E, 0x1DCAB8, 0x1E2F12, 0x1E936C, 0x1EF7C6, 0x1F5C20, 0x1FC07A, 0x2024D4, 0x20892E, 0x20ED88, 0x2151E2, 0x21B63C, 0x221A96, 0x227EF0, 0x22E34A, 0x2347A4, 0x23ABFE, 0x241058, 0x2474B2, 0x24D90C, 0x253D66, 0x25A1C0, 0x26061A, 0x266A74, 0x26CECE, 0x273328, 0x279782, 0x27FBDC, 0x286036, 0x28C490, 0x2928EA, 0x298D44, 0x29F19E, 0x2A55F8, 0x2ABA52, 0x2B1EAC, 0x2B8306, 0x2BE760, 0x2C4BBA, 0x2CB014, 0x2D146E, 0x2D78C8, 0x2DDD22, 0x2E417C, 0x2EA5D6, 0x2F0A30, 0x2F6E8A, 0x2FD2E4, 0x30373E, 0x309B98, 0x30FFF2, 0x31644C, 0x31C8A6
};
static const int yuv2rgbtable_gv[256] = {
0xFF97E900, 0xFF98B92E, 0xFF99895C, 0xFF9A598A, 0xFF9B29B8, 0xFF9BF9E6, 0xFF9CCA14, 0xFF9D9A42, 0xFF9E6A70, 0xFF9F3A9E, 0xFFA00ACC, 0xFFA0DAFA, 0xFFA1AB28, 0xFFA27B56, 0xFFA34B84, 0xFFA41BB2, 0xFFA4EBE0, 0xFFA5BC0E, 0xFFA68C3C, 0xFFA75C6A, 0xFFA82C98, 0xFFA8FCC6, 0xFFA9CCF4, 0xFFAA9D22, 0xFFAB6D50, 0xFFAC3D7E, 0xFFAD0DAC, 0xFFADDDDA, 0xFFAEAE08, 0xFFAF7E36, 0xFFB04E64, 0xFFB11E92, 0xFFB1EEC0, 0xFFB2BEEE, 0xFFB38F1C, 0xFFB45F4A, 0xFFB52F78, 0xFFB5FFA6, 0xFFB6CFD4, 0xFFB7A002, 0xFFB87030, 0xFFB9405E, 0xFFBA108C, 0xFFBAE0BA, 0xFFBBB0E8, 0xFFBC8116, 0xFFBD5144, 0xFFBE2172, 0xFFBEF1A0, 0xFFBFC1CE, 0xFFC091FC, 0xFFC1622A, 0xFFC23258, 0xFFC30286, 0xFFC3D2B4, 0xFFC4A2E2, 0xFFC57310, 0xFFC6433E, 0xFFC7136C, 0xFFC7E39A, 0xFFC8B3C8, 0xFFC983F6, 0xFFCA5424, 0xFFCB2452, 0xFFCBF480, 0xFFCCC4AE, 0xFFCD94DC, 0xFFCE650A, 0xFFCF3538, 0xFFD00566, 0xFFD0D594, 0xFFD1A5C2, 0xFFD275F0, 0xFFD3461E, 0xFFD4164C, 0xFFD4E67A, 0xFFD5B6A8, 0xFFD686D6, 0xFFD75704, 0xFFD82732, 0xFFD8F760, 0xFFD9C78E, 0xFFDA97BC, 0xFFDB67EA, 0xFFDC3818, 0xFFDD0846, 0xFFDDD874, 0xFFDEA8A2, 0xFFDF78D0, 0xFFE048FE, 0xFFE1192C, 0xFFE1E95A, 0xFFE2B988, 0xFFE389B6, 0xFFE459E4, 0xFFE52A12, 0xFFE5FA40, 0xFFE6CA6E, 0xFFE79A9C, 0xFFE86ACA, 0xFFE93AF8, 0xFFEA0B26, 0xFFEADB54, 0xFFEBAB82, 0xFFEC7BB0, 0xFFED4BDE, 0xFFEE1C0C, 0xFFEEEC3A, 0xFFEFBC68, 0xFFF08C96, 0xFFF15CC4, 0xFFF22CF2, 0xFFF2FD20, 0xFFF3CD4E, 0xFFF49D7C, 0xFFF56DAA, 0xFFF63DD8, 0xFFF70E06, 0xFFF7DE34, 0xFFF8AE62, 0xFFF97E90, 0xFFFA4EBE, 0xFFFB1EEC, 0xFFFBEF1A, 0xFFFCBF48, 0xFFFD8F76, 0xFFFE5FA4, 0xFFFF2FD2, 0x0, 0xD02E, 0x1A05C, 0x2708A, 0x340B8, 0x410E6, 0x4E114, 0x5B142, 0x68170, 0x7519E, 0x821CC, 0x8F1FA, 0x9C228, 0xA9256, 0xB6284, 0xC32B2, 0xD02E0, 0xDD30E, 0xEA33C, 0xF736A, 0x104398, 0x1113C6, 0x11E3F4, 0x12B422, 0x138450, 0x14547E, 0x1524AC, 0x15F4DA, 0x16C508, 0x179536, 0x186564, 0x193592, 0x1A05C0, 0x1AD5EE, 0x1BA61C, 0x1C764A, 0x1D4678, 0x1E16A6, 0x1EE6D4, 0x1FB702, 0x208730, 0x21575E, 0x22278C, 0x22F7BA, 0x23C7E8, 0x249816, 0x256844, 0x263872, 0x2708A0, 0x27D8CE, 0x28A8FC, 0x29792A, 0x2A4958, 0x2B1986, 0x2BE9B4, 0x2CB9E2, 0x2D8A10, 0x2E5A3E, 0x2F2A6C, 0x2FFA9A, 0x30CAC8, 0x319AF6, 0x326B24, 0x333B52, 0x340B80, 0x34DBAE, 0x35ABDC, 0x367C0A, 0x374C38, 0x381C66, 0x38EC94, 0x39BCC2, 0x3A8CF0, 0x3B5D1E, 0x3C2D4C, 0x3CFD7A, 0x3DCDA8, 0x3E9DD6, 0x3F6E04, 0x403E32, 0x410E60, 0x41DE8E, 0x42AEBC, 0x437EEA, 0x444F18, 0x451F46, 0x45EF74, 0x46BFA2, 0x478FD0, 0x485FFE, 0x49302C, 0x4A005A, 0x4AD088, 0x4BA0B6, 0x4C70E4, 0x4D4112, 0x4E1140, 0x4EE16E, 0x4FB19C, 0x5081CA, 0x5151F8, 0x522226, 0x52F254, 0x53C282, 0x5492B0, 0x5562DE, 0x56330C, 0x57033A, 0x57D368, 0x58A396, 0x5973C4, 0x5A43F2, 0x5B1420, 0x5BE44E, 0x5CB47C, 0x5D84AA, 0x5E54D8, 0x5F2506, 0x5FF534, 0x60C562, 0x619590, 0x6265BE, 0x6335EC, 0x64061A, 0x64D648, 0x65A676, 0x6676A4, 0x6746D2
};
static const int yuv2rgbtable_bv[256] = {
0xFF33A280, 0xFF353B3B, 0xFF36D3F6, 0xFF386CB1, 0xFF3A056C, 0xFF3B9E27, 0xFF3D36E2, 0xFF3ECF9D, 0xFF406858, 0xFF420113, 0xFF4399CE, 0xFF453289, 0xFF46CB44, 0xFF4863FF, 0xFF49FCBA, 0xFF4B9575, 0xFF4D2E30, 0xFF4EC6EB, 0xFF505FA6, 0xFF51F861, 0xFF53911C, 0xFF5529D7, 0xFF56C292, 0xFF585B4D, 0xFF59F408, 0xFF5B8CC3, 0xFF5D257E, 0xFF5EBE39, 0xFF6056F4, 0xFF61EFAF, 0xFF63886A, 0xFF652125, 0xFF66B9E0, 0xFF68529B, 0xFF69EB56, 0xFF6B8411, 0xFF6D1CCC, 0xFF6EB587, 0xFF704E42, 0xFF71E6FD, 0xFF737FB8, 0xFF751873, 0xFF76B12E, 0xFF7849E9, 0xFF79E2A4, 0xFF7B7B5F, 0xFF7D141A, 0xFF7EACD5, 0xFF804590, 0xFF81DE4B, 0xFF837706, 0xFF850FC1, 0xFF86A87C, 0xFF884137, 0xFF89D9F2, 0xFF8B72AD, 0xFF8D0B68, 0xFF8EA423, 0xFF903CDE, 0xFF91D599, 0xFF936E54, 0xFF95070F, 0xFF969FCA, 0xFF983885, 0xFF99D140, 0xFF9B69FB, 0xFF9D02B6, 0xFF9E9B71, 0xFFA0342C, 0xFFA1CCE7, 0xFFA365A2, 0xFFA4FE5D, 0xFFA69718, 0xFFA82FD3, 0xFFA9C88E, 0xFFAB6149, 0xFFACFA04, 0xFFAE92BF, 0xFFB02B7A, 0xFFB1C435, 0xFFB35CF0, 0xFFB4F5AB, 0xFFB68E66, 0xFFB82721, 0xFFB9BFDC, 0xFFBB5897, 0xFFBCF152, 0xFFBE8A0D, 0xFFC022C8, 0xFFC1BB83, 0xFFC3543E, 0xFFC4ECF9, 0xFFC685B4, 0xFFC81E6F, 0xFFC9B72A, 0xFFCB4FE5, 0xFFCCE8A0, 0xFFCE815B, 0xFFD01A16, 0xFFD1B2D1, 0xFFD34B8C, 0xFFD4E447, 0xFFD67D02, 0xFFD815BD, 0xFFD9AE78, 0xFFDB4733, 0xFFDCDFEE, 0xFFDE78A9, 0xFFE01164, 0xFFE1AA1F, 0xFFE342DA, 0xFFE4DB95, 0xFFE67450, 0xFFE80D0B, 0xFFE9A5C6, 0xFFEB3E81, 0xFFECD73C, 0xFFEE6FF7, 0xFFF008B2, 0xFFF1A16D, 0xFFF33A28, 0xFFF4D2E3, 0xFFF66B9E, 0xFFF80459, 0xFFF99D14, 0xFFFB35CF, 0xFFFCCE8A, 0xFFFE6745, 0x0, 0x198BB, 0x33176, 0x4CA31, 0x662EC, 0x7FBA7, 0x99462, 0xB2D1D, 0xCC5D8, 0xE5E93, 0xFF74E, 0x119009, 0x1328C4, 0x14C17F, 0x165A3A, 0x17F2F5, 0x198BB0, 0x1B246B, 0x1CBD26, 0x1E55E1, 0x1FEE9C, 0x218757, 0x232012, 0x24B8CD, 0x265188, 0x27EA43, 0x2982FE, 0x2B1BB9, 0x2CB474, 0x2E4D2F, 0x2FE5EA, 0x317EA5, 0x331760, 0x34B01B, 0x3648D6, 0x37E191, 0x397A4C, 0x3B1307, 0x3CABC2, 0x3E447D, 0x3FDD38, 0x4175F3, 0x430EAE, 0x44A769, 0x464024, 0x47D8DF, 0x49719A, 0x4B0A55, 0x4CA310, 0x4E3BCB, 0x4FD486, 0x516D41, 0x5305FC, 0x549EB7, 0x563772, 0x57D02D, 0x5968E8, 0x5B01A3, 0x5C9A5E, 0x5E3319, 0x5FCBD4, 0x61648F, 0x62FD4A, 0x649605, 0x662EC0, 0x67C77B, 0x696036, 0x6AF8F1, 0x6C91AC, 0x6E2A67, 0x6FC322, 0x715BDD, 0x72F498, 0x748D53, 0x76260E, 0x77BEC9, 0x795784, 0x7AF03F, 0x7C88FA, 0x7E21B5, 0x7FBA70, 0x81532B, 0x82EBE6, 0x8484A1, 0x861D5C, 0x87B617, 0x894ED2, 0x8AE78D, 0x8C8048, 0x8E1903, 0x8FB1BE, 0x914A79, 0x92E334, 0x947BEF, 0x9614AA, 0x97AD65, 0x994620, 0x9ADEDB, 0x9C7796, 0x9E1051, 0x9FA90C, 0xA141C7, 0xA2DA82, 0xA4733D, 0xA60BF8, 0xA7A4B3, 0xA93D6E, 0xAAD629, 0xAC6EE4, 0xAE079F, 0xAFA05A, 0xB13915, 0xB2D1D0, 0xB46A8B, 0xB60346, 0xB79C01, 0xB934BC, 0xBACD77, 0xBC6632, 0xBDFEED, 0xBF97A8, 0xC13063, 0xC2C91E, 0xC461D9, 0xC5FA94, 0xC7934F, 0xC92C0A, 0xCAC4C5
};

void getvideo(unsigned char *video, int *xres, int *yres);

enum {UNKNOWN,PALLAS,VULCAN,XILLEON,BRCM7401,BRCM4380};
char *stb_name[]={"unknown","Pallas","Vulcan","Xilleon","Brcm7401","Brcm4380"};
int stb_type=UNKNOWN;

static char* upcase(char* mixedstr)
{
	char* s;
        for (s = mixedstr; *s; ++s)
        {
                *s = toupper(*s);
        }
        return mixedstr;
}

// main program

int main(int argc, char **argv) {

	printf("amBX Screengrabber\n");

    if (ambx_init() < 0)
    {
        printf("ambx_init failed.\n");
        return -1;
    }
	int id = 0;
    if (ambx_open(id) < 0)
    {
        printf("ambx_open(%d) failed.\n", id);
        return -1;
    }
	

	int xres,yres;
	unsigned char *video;

	static const char* filename = "/tmp/screenshot.bmp";
	
	// detect STB
	char buf[256];
	FILE *pipe=fopen("/proc/fb","r");
	while (fgets(buf,sizeof(buf),pipe))
	{
		if (strstr(upcase(buf),"VULCAN")) {stb_type=VULCAN;}
		if (strstr(upcase(buf),"PALLAS")) {stb_type=PALLAS;}
		if (strstr(upcase(buf),"XILLEON")) {stb_type=XILLEON;}
		if (strstr(upcase(buf),"BCM7401") || strstr(upcase(buf),"BCMFB")) {stb_type=BRCM7401;}
	}
	fclose(pipe);
	if (stb_type == BRCM7401) // Bcrm7401 + Bcrm4380 use the same framebuffer string, so fall back to /proc/cpuinfO for detecting DM8000
	{
		pipe=fopen("/proc/cpuinfo","r");
		while (fgets(buf,sizeof(buf),pipe))
		{
			if (strstr(upcase(buf),"BRCM4380")) {stb_type=BRCM4380;}
		}
		fclose(pipe);
	}
	if (stb_type == UNKNOWN)
	{
		printf("Unknown STB .. quit.\n");
		return 1;
	} else
	{
		printf("Detected STB: %s\n",stb_name[stb_type]);
	}
	
	int mallocsize=1920*1080;
	if (stb_type == VULCAN || stb_type == PALLAS)
		mallocsize=720*576;
	
	video = (unsigned char *)malloc(mallocsize*3);

	static const int light[] = {0, 2, 3, 4, 1};
	for (;;)
	{
		getvideo(video, &xres, &yres);
		//printf("Grabbed: %d x %d\n", xres, yres);

		int colors[5];
		getcolors(colors, video, xres, yres);

		int i;
		for (i=0; i<5; ++i)
		{	
			//printf("%06x ", colors[i]);
			ambx_set_light(id, light[i], colors[i]);
		}	
		//printf("\n");
		
		//usleep(1000000/50);
	}
	
	// Thats all folks 
	//printf("... Done !\n");
	
	// clean up
	free(video);

	return 0;
}

// grabing the video picture

void getvideo(unsigned char *video, int *xres, int *yres)
{
	int mem_fd;
	//unsigned char *memory;
	void *memory;
	if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
		printf("Mainmemory: can't open /dev/mem \n");
		return;
	}

	unsigned char *luma, *chroma, *memory_tmp;
	luma = NULL; //(unsigned char *)malloc(1); // real malloc will be done later
	chroma = NULL; //(unsigned char *)malloc(1); // this is just to be sure it get initialized and free() will not segfaulting
	memory_tmp = NULL; //(unsigned char *)malloc(1);
	int t,stride,res;
	res = stride = 0;
	char buf[256];
	FILE *pipe;

	if (stb_type == BRCM7401 || stb_type == BRCM4380)
	{
		// grab brcm7401/4380 pic from decoder memory
		
		if(!(memory = (unsigned char*)mmap(0, 100, PROT_READ, MAP_SHARED, mem_fd, 0x10100000)))
		{
			printf("Mainmemory: <Memmapping failed>\n");
			return;
		}

		unsigned char data[100];

		int adr,adr2,ofs,ofs2,offset/*,vert_start,vert_end*/;
		int xtmp,xsub,ytmp,t2,dat1;
		
		memcpy(data,memory,100); 
		//vert_start=data[0x1B]<<8|data[0x1A];
		//vert_end=data[0x19]<<8|data[0x18];
		stride=data[0x15]<<8|data[0x14];	
		ofs=(data[0x28]<<8|data[0x27])>>4;
		ofs2=(data[0x2c]<<8|data[0x2b])>>4;
		adr=(data[0x1f]<<24|data[0x1e]<<16|data[0x1d]<<8|data[0x1c])&0xFFFFFF00;
		adr2=(data[0x23]<<24|data[0x22]<<16|data[0x21]<<8|data[0x20])&0xFFFFFF00;
		offset=adr2-adr;
		
		munmap(memory, 100);

		// printf("Stride: %d Res: %d\n",stride,res);
		// printf("Adr: %X Adr2: %X OFS: %d %d\n",adr,adr2,ofs,ofs2);

		pipe=fopen("/proc/stb/vmpeg/0/yres","r");
		while (fgets(buf,sizeof(buf),pipe))
			sscanf(buf,"%x",&res); 
		fclose(pipe);

		
		luma = (unsigned char *)malloc(stride*(ofs));
		chroma = (unsigned char *)malloc(stride*(ofs2+64));	
								
		// grabbing luma & chroma plane from the decoder memory
		if (stb_type == BRCM7401)
		{
			// on dm800 we have direct access to the decoder memory
			if(!(memory_tmp = (unsigned char*)mmap(0, offset + stride*(ofs2+64), PROT_READ, MAP_SHARED, mem_fd, adr)))
			{
				printf("Mainmemory: <Memmapping failed>\n");
				return;
			}
			
			usleep(50000); 	// we try to get a full picture, its not possible to get a sync from the decoder so we use a delay
							// and hope we get a good timing. dont ask me why, but every DM800 i tested so far produced a good
							// result with a 50ms delay
			
		} else if (stb_type == BRCM4380)
				{
			// on dm8000 we have to use dma, so dont change anything here until you really know what you are doing !
			
			if(!(memory_tmp = (unsigned char*)mmap(0, DMA_BLOCKSIZE + 0x1000, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, SPARE_RAM)))
			{
				printf("Mainmemory: <Memmapping failed>\n");
				return;
			}
			volatile unsigned long *mem_dma;
			if(!(mem_dma = (volatile unsigned long*)mmap(0, 0x1000, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, 0x10c02000)))
			{
				printf("Mainmemory: <Memmapping failed>\n");
				return;
			}

			int i = 0;
			int tmp_len = DMA_BLOCKSIZE;
			int tmp_size = offset + stride*(ofs2+64);
			for (i=0; i < tmp_size; i += DMA_BLOCKSIZE)
			{
				
				unsigned long *descriptor = (void*)memory_tmp;

				if (i + DMA_BLOCKSIZE > tmp_size)
					tmp_len = tmp_size - i;

				//printf("DMACopy: %x (%d) size: %d\n", adr+i, i, tmp_len);
				
				descriptor[0] = /* READ */ adr + i;
				descriptor[1] = /* WRITE */ SPARE_RAM + 0x1000;
				descriptor[2] = 0x40000000 | /* LEN */ tmp_len;
				descriptor[3] = 0;
				descriptor[4] = 0;
				descriptor[5] = 0;
				descriptor[6] = 0;
				descriptor[7] = 0;
				mem_dma[1] = /* FIRST_DESCRIPTOR */ SPARE_RAM;
				mem_dma[3] = /* DMA WAKE CTRL */ 3;
				mem_dma[2] = 1;
				while (mem_dma[5] == 1)
					usleep(2);
				mem_dma[2] = 0;
		
			}

			munmap((void *)mem_dma, 0x1000);
			memory_tmp+=0x1000;
		}

		t=t2=dat1=0;
		const int chr_luma_stride = 0x40;

		/*
		if (stb_type == BRCM7405)
			chr_luma_stride *= 2;
		*/

		// Scale down here, since it's hard to do early 
		xsub=chr_luma_stride;
		// decode luma & chroma plane or lets say sort it
		const int scaling = 16;
		// Scale down while getting 2 luma lines for each chroma line 
		for (xtmp=0; xtmp < stride; xtmp += chr_luma_stride)
		{
			if ((stride-xtmp) <= chr_luma_stride)
				xsub=stride-xtmp;

			dat1=xtmp;
			for (ytmp = 0; ytmp < ofs; ytmp+=scaling) 
			{
				memcpy(luma+dat1,memory_tmp+t,xsub); // luma0
				dat1 += stride;
				memcpy(luma+dat1,memory_tmp+t+chr_luma_stride,xsub); // luma1
				t += chr_luma_stride*scaling;
				dat1 += stride;
			}
		}
		// Hmm apparently lumastride == chromastride?
		xsub=chr_luma_stride;
		for (xtmp=0; xtmp < stride; xtmp += chr_luma_stride)
		{
			if ((stride-xtmp) <= chr_luma_stride)
				xsub=stride-xtmp;

			dat1=xtmp;
			for (ytmp = 0; ytmp < ofs2; ytmp += (scaling/2)) 
			{
				memcpy(chroma+dat1, memory_tmp+offset+t2, xsub); // chroma
				t2 += chr_luma_stride*(scaling/2);
				dat1 += stride;
			}
		}
		memory_tmp -= 0x1000; // There was a +=0x1000 further up we have to make up for.
		if (stb_type == BRCM7401 /* || stb_type == BRCM7405 */ )
			munmap(memory_tmp, offset + stride*(ofs2+64));
		else /* if (stb_type == BRCM7400) */
			munmap(memory_tmp, DMA_BLOCKSIZE + 0x1000);
		ofs /= (scaling/2);
		res /= (scaling/2);
		int count = (stride*ofs) >> 2;
		unsigned char* p = luma;
		for (t=count; t != 0; --t)
		{
			SWAP(p[0], p[3]);
			SWAP(p[1], p[2]);
			p += 4;
		}
		count = (stride*(ofs>>1)) >> 2;
		p = chroma;
                for (t=count; t != 0; --t)
                {
                        SWAP(p[0], p[3]);
                        SWAP(p[1], p[2]);
                        p += 4;
                }
	} else if (stb_type == XILLEON)
	{
		// grab xilleon pic from decoder memory
		pipe=fopen("/proc/stb/vmpeg/0/xres","r");
		while (fgets(buf,sizeof(buf),pipe))
		{
			sscanf(buf,"%x",&stride); 
		}
		fclose(pipe);
		pipe=fopen("/proc/stb/vmpeg/0/yres","r");
		while (fgets(buf,sizeof(buf),pipe))
		{
			sscanf(buf,"%x",&res); 
		}
		fclose(pipe);

		if(!(memory = (unsigned char*)mmap(0, 1920*1152*6, PROT_READ, MAP_SHARED, mem_fd, 0x6000000)))
		{
			printf("Mainmemory: <Memmapping failed>\n");
			return;
		}
		
		luma = (unsigned char *)malloc(stride*res);
		chroma = (unsigned char *)malloc(stride*(res+1)/2);
		
		int offset=1920*1152*5;	// offset for chroma buffer
		
		const unsigned char* frame_l = memory; // luma frame from video decoder
		const unsigned char* frame_c = memory + offset; // chroma frame from video decoder
		
		int xtmp,ytmp,ysub,xsub;
		const int ypart=32;
		const int xpart=128;
		int oe2=0;
		int ysubcount=res/32;
		int ysubchromacount=res/64;

		// "decode" luma/chroma, there are 128x32pixel blocks inside the decoder mem
		for (ysub=0; ysub<=ysubcount; ysub++) 
		{
			int extraoffset = stride*ysub;
			for (xsub=0; xsub<15; xsub++) // 1920/128=15
			{
				// Even lines
				{
					int destx = xsub*xpart;
					int overflow = (destx + xpart) - stride;
					if (overflow <= 0)
					{
						// We copy a bit too much...
						memcpy(luma + destx + extraoffset, frame_l, xpart);
					}
					else if (overflow < xpart)
					{
                                                memcpy(luma + destx + extraoffset, frame_l, overflow);
					}
				}
				frame_l += ypart*xpart;
			}
			++ysub; // dirty...
			extraoffset = stride*ysub;
			for (xsub=0; xsub<15; xsub++) // 1920/128=15
			{
				// Odd lines (reverts 64 byte block?)
				// Only luminance
				// for (ytmp=0; ytmp<ypart; ytmp++)
				{
					int destx = xsub*xpart;
					int overflow = (destx + xpart) - stride;
					if (overflow <= 0)
					{
						// We copy a bit too much...
						memcpy(luma + destx + extraoffset + 64, frame_l, 64);
						memcpy(luma + destx + extraoffset, frame_l + 64, 64);
					}
					else if (overflow < xpart)
					{
						if (overflow > 64)
						{
							memcpy(luma + destx + extraoffset + 64, frame_l, overflow-64);
							memcpy(luma + destx + extraoffset, frame_l + 64, 64);
						}
						else
						{
							memcpy(luma + destx + extraoffset, frame_l + 64, overflow);
						}
					}
				}
				frame_l += ypart*xpart;
			}
		}

		// Chrominance (half resolution)
		ysubcount /= 2;
		for (ysub=0; ysub<=ysubcount; ysub++) 
		{
			for (xsub=0; xsub<15; xsub++) // 1920/128=15
			{
				// Even lines
				for (ytmp=0; ytmp<ypart; ytmp+=16)
				{
					int extraoffset = (stride*(ytmp+ysub));
					int destx = xsub*xpart;
					int overflow = (destx + xpart) - stride;
					if (overflow <= 0)
					{
						memcpy(chroma + destx + extraoffset, frame_c, xpart);
					}
					else if (overflow < xpart)
					{
                                                memcpy(chroma + destx + extraoffset, frame_c, overflow);
					}
					frame_c += xpart*16;
				}
			}
			++ysub; // dirty...
			for (xsub=0; xsub<15; xsub++) // 1920/128=15
			{
				// Odd lines (reverts 64 byte block?)
				// Only luminance
				for (ytmp=0; ytmp<ypart; ytmp+=16)
				{
					int extraoffset = (stride*(ytmp+ysub));
					int destx = xsub*xpart;
					int overflow = (destx + xpart) - stride;
					if (overflow <= 0)
					{
						// We copy a bit too much...
						memcpy(chroma + destx + extraoffset + 64, frame_c, 64);
						memcpy(chroma + destx + extraoffset, frame_c + 64, 64);
					}
					else if (overflow < xpart)
					{
						if (overflow > 64)
						{
							memcpy(chroma + destx + extraoffset + 64, frame_c, overflow-64);
							memcpy(chroma + destx + extraoffset, frame_c + 64, 64);
						}
						else
						{
							memcpy(chroma + destx + extraoffset, frame_c + 64, overflow);
						}
					}
					frame_c += xpart*16;
				}
			}
		}
		
		res /= 32; // much much smaller...

		munmap(memory, 1920*1152*6);

	} else if (stb_type == VULCAN || stb_type == PALLAS)
	{
		// grab via v4l device (ppc boxes)
		
		memory_tmp = (unsigned char *)malloc(720 * 576 * 3 + 16);
		
		int fd_video = open(VIDEO_DEV, O_RDONLY);
		if (fd_video < 0)
		{
			printf("could not open /dev/video");
			return;
		}	 
		
		int r = read(fd_video, memory_tmp, 720 * 576 * 3 + 16);
		if (r < 16)
		{
			fprintf(stderr, "read failed\n");
			close(fd_video);
			return;
		}
		close(fd_video);
		
		int *size = (int*)memory_tmp;
		stride = size[0];
		res = size[1];
		
		luma = (unsigned char *)malloc(stride * res);
		chroma = (unsigned char *)malloc(stride * res);
		
		memcpy (luma, memory_tmp + 16, stride * res);
		memcpy (chroma, memory_tmp + 16 + stride * res, stride * res);
		
		free(memory_tmp);
	}

	close(mem_fd);	
	
	
	int Y, U, V, y ,x, out1, pos, RU, GU, GV, BV, rgbstride;
	Y=U=V=0;
		
	// yuv2rgb conversion (4:2:0)
	out1=pos=t=0;
	rgbstride=stride*3;
	
	for (y=0; y < res; y+=2)
	{
		for (x=0; x < stride; x+=2)
		{
			U=chroma[t++];
			V=chroma[t++];
			
			RU=yuv2rgbtable_ru[U]; // use lookup tables to speedup the whole thing
			GU=yuv2rgbtable_gu[U];
			GV=yuv2rgbtable_gv[V];
			BV=yuv2rgbtable_bv[V];
			
			if (stb_type == XILLEON) //on xilleon we use bgr instead of rgb so simply swap the coeffs
				SWAP(RU,BV);

			// now we do 4 pixels on each iteration this is more code but much faster 
			Y=yuv2rgbtable_y[luma[pos]]; 

			video[out1]=CLAMP((Y + RU)>>16);
			video[out1+1]=CLAMP((Y - GV - GU)>>16);
			video[out1+2]=CLAMP((Y + BV)>>16);
			
			Y=yuv2rgbtable_y[luma[stride+pos]];

			video[out1+rgbstride]=CLAMP((Y + RU)>>16);
			video[out1+1+rgbstride]=CLAMP((Y - GV - GU)>>16);
			video[out1+2+rgbstride]=CLAMP((Y + BV)>>16);

			pos++;
			out1+=3;
			
			Y=yuv2rgbtable_y[luma[pos]];

			video[out1]=CLAMP((Y + RU)>>16);
			video[out1+1]=CLAMP((Y - GV - GU)>>16);
			video[out1+2]=CLAMP((Y + BV)>>16);
			
			Y=yuv2rgbtable_y[luma[stride+pos]];

			video[out1+rgbstride]=CLAMP((Y + RU)>>16);
			video[out1+1+rgbstride]=CLAMP((Y - GV - GU)>>16);
			video[out1+2+rgbstride]=CLAMP((Y + BV)>>16);
			
			out1+=3;
			pos++;
		}
		out1+=rgbstride;
		pos+=stride;

	}
	
	*xres=stride;
	*yres=res;
	free(luma);
	free(chroma);
}


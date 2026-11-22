// npcore: numpy and Pillow primitives for the made-art ports, the same to
// the bit as NumPy 1.26.4 and Pillow 10.2.0 here. See npcore.h; proved by
// art/npcore_check.py (OPENC2E_NPCORE_CHECK=DIR, then the script).
//
// Each part follows the C (or Python) source it names. The RNG, Pillow's
// resampler, box blur, bicubic transform and premultiply steps come from
// the earlier ports (stream_bed, treelift_top, volcano_passage,
// volcano_trunk, cave_ramp), generalised; the ziggurat and tanh tables are
// numpy's own (numpy/random/src/distributions/ziggurat_constants.h,
// numpy/core/src/umath/loops_hyperbolic.dispatch.c.src; BSD licence).

#include "npcore.h"
#include "common/devenv.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

// numpy, Pillow and CPython were built without fused multiply-add
// contraction (generic x86-64); so is this, whatever the flags
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace MadeArt {
namespace np {

const double kPi = 3.14159265358979323846; // M_PI (Pillow, CPython, the recipes)

// ===========================================================================
// dtype conversions

uint8_t u8(double v) {
	// numpy's C cast: to int32 (cvttsd2si: out of range or NaN is INT_MIN),
	// then its low byte
	if (!(v > -2147483649.0 && v < 2147483648.0))
		return 0;
	return uint8_t(uint32_t(int32_t(v)));
}

float clip(float v, float lo, float hi) {
	if (!std::isnan(v))
		v = v > lo ? v : lo;
	if (!std::isnan(v))
		v = v < hi ? v : hi;
	return v;
}

double clip(double v, double lo, double hi) {
	if (!std::isnan(v))
		v = v > lo ? v : lo;
	if (!std::isnan(v))
		v = v < hi ? v : hi;
	return v;
}

template <typename T>
static Img8 toU8T(const Img<T>& a, bool clipFirst) {
	Img8 o(a.h, a.w, a.c);
	for (size_t i = 0; i < a.v.size(); i++)
		o.v[i] = u8(double(clipFirst ? clip(a.v[i], T(0), T(255)) : a.v[i]));
	return o;
}
Img8 toU8(const ImgF& a) { return toU8T(a, false); }
Img8 toU8(const ImgD& a) { return toU8T(a, false); }
Img8 clipToU8(const ImgF& a) { return toU8T(a, true); }
Img8 clipToU8(const ImgD& a) { return toU8T(a, true); }

template <typename D, typename S>
static Img<D> convertT(const Img<S>& a) {
	Img<D> o(a.h, a.w, a.c);
	for (size_t i = 0; i < a.v.size(); i++)
		o.v[i] = D(a.v[i]);
	return o;
}
ImgF toF32(const Img8& a) { return convertT<float>(a); }
ImgF toF32(const ImgD& a) { return convertT<float>(a); }
ImgD toF64(const Img8& a) { return convertT<double>(a); }
ImgD toF64(const ImgF& a) { return convertT<double>(a); }

Img8 fromRgba(const Rgba& im, int c) {
	Img8 o(im.h, im.w, c);
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++)
			for (int k = 0; k < c; k++)
				o(y, x, k) = im.at(x, y)[k];
	return o;
}

Rgba toRgba(const Img8& im) {
	Rgba o(im.w, im.h);
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++) {
			uint8_t* d = o.at(x, y);
			const uint8_t* s = im.px(y, x);
			if (im.c == 1)
				d[0] = d[1] = d[2] = s[0];
			else
				for (int k = 0; k < 3; k++)
					d[k] = s[k];
			d[3] = im.c == 4 ? s[3] : 255;
		}
	return o;
}

// ===========================================================================
// numpy.random: SeedSequence, PCG64 (XSL-RR), Generator

namespace {

// 128-bit arithmetic without __int128 (MSVC lacks it)
struct U128 {
	uint64_t hi = 0, lo = 0;
};

U128 add128(U128 a, U128 b) {
	U128 r;
	r.lo = a.lo + b.lo;
	r.hi = a.hi + b.hi + (r.lo < a.lo ? 1 : 0);
	return r;
}

U128 mul64(uint64_t a, uint64_t b) {
	uint64_t a0 = a & 0xffffffffu, a1 = a >> 32, b0 = b & 0xffffffffu, b1 = b >> 32;
	uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
	uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
	U128 r;
	r.lo = (mid << 32) | (p00 & 0xffffffffu);
	r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
	return r;
}

U128 mul128(U128 a, U128 b) {
	U128 r = mul64(a.lo, b.lo);
	r.hi += a.hi * b.lo + a.lo * b.hi;
	return r;
}

// numpy's ziggurat tables for doubles (ziggurat_constants.h, verbatim)
static const uint64_t ki_double[] = {
    0x000EF33D8025EF6AULL, 0x0000000000000000ULL, 0x000C08BE98FBC6A8ULL,
    0x000DA354FABD8142ULL, 0x000E51F67EC1EEEAULL, 0x000EB255E9D3F77EULL,
    0x000EEF4B817ECAB9ULL, 0x000F19470AFA44AAULL, 0x000F37ED61FFCB18ULL,
    0x000F4F469561255CULL, 0x000F61A5E41BA396ULL, 0x000F707A755396A4ULL,
    0x000F7CB2EC28449AULL, 0x000F86F10C6357D3ULL, 0x000F8FA6578325DEULL,
    0x000F9724C74DD0DAULL, 0x000F9DA907DBF509ULL, 0x000FA360F581FA74ULL,
    0x000FA86FDE5B4BF8ULL, 0x000FACF160D354DCULL, 0x000FB0FB6718B90FULL,
    0x000FB49F8D5374C6ULL, 0x000FB7EC2366FE77ULL, 0x000FBAECE9A1E50EULL,
    0x000FBDAB9D040BEDULL, 0x000FC03060FF6C57ULL, 0x000FC2821037A248ULL,
    0x000FC4A67AE25BD1ULL, 0x000FC6A2977AEE31ULL, 0x000FC87AA92896A4ULL,
    0x000FCA325E4BDE85ULL, 0x000FCBCCE902231AULL, 0x000FCD4D12F839C4ULL,
    0x000FCEB54D8FEC99ULL, 0x000FD007BF1DC930ULL, 0x000FD1464DD6C4E6ULL,
    0x000FD272A8E2F450ULL, 0x000FD38E4FF0C91EULL, 0x000FD49A9990B478ULL,
    0x000FD598B8920F53ULL, 0x000FD689C08E99ECULL, 0x000FD76EA9C8E832ULL,
    0x000FD848547B08E8ULL, 0x000FD9178BAD2C8CULL, 0x000FD9DD07A7ADD2ULL,
    0x000FDA9970105E8CULL, 0x000FDB4D5DC02E20ULL, 0x000FDBF95C5BFCD0ULL,
    0x000FDC9DEBB99A7DULL, 0x000FDD3B8118729DULL, 0x000FDDD288342F90ULL,
    0x000FDE6364369F64ULL, 0x000FDEEE708D514EULL, 0x000FDF7401A6B42EULL,
    0x000FDFF46599ED40ULL, 0x000FE06FE4BC24F2ULL, 0x000FE0E6C225A258ULL,
    0x000FE1593C28B84CULL, 0x000FE1C78CBC3F99ULL, 0x000FE231E9DB1CAAULL,
    0x000FE29885DA1B91ULL, 0x000FE2FB8FB54186ULL, 0x000FE35B33558D4AULL,
    0x000FE3B799D0002AULL, 0x000FE410E99EAD7FULL, 0x000FE46746D47734ULL,
    0x000FE4BAD34C095CULL, 0x000FE50BAED29524ULL, 0x000FE559F74EBC78ULL,
    0x000FE5A5C8E41212ULL, 0x000FE5EF3E138689ULL, 0x000FE6366FD91078ULL,
    0x000FE67B75C6D578ULL, 0x000FE6BE661E11AAULL, 0x000FE6FF55E5F4F2ULL,
    0x000FE73E5900A702ULL, 0x000FE77B823E9E39ULL, 0x000FE7B6E37070A2ULL,
    0x000FE7F08D774243ULL, 0x000FE8289053F08CULL, 0x000FE85EFB35173AULL,
    0x000FE893DC840864ULL, 0x000FE8C741F0CEBCULL, 0x000FE8F9387D4EF6ULL,
    0x000FE929CC879B1DULL, 0x000FE95909D388EAULL, 0x000FE986FB939AA2ULL,
    0x000FE9B3AC714866ULL, 0x000FE9DF2694B6D5ULL, 0x000FEA0973ABE67CULL,
    0x000FEA329CF166A4ULL, 0x000FEA5AAB32952CULL, 0x000FEA81A6D5741AULL,
    0x000FEAA797DE1CF0ULL, 0x000FEACC85F3D920ULL, 0x000FEAF07865E63CULL,
    0x000FEB13762FEC13ULL, 0x000FEB3585FE2A4AULL, 0x000FEB56AE3162B4ULL,
    0x000FEB76F4E284FAULL, 0x000FEB965FE62014ULL, 0x000FEBB4F4CF9D7CULL,
    0x000FEBD2B8F449D0ULL, 0x000FEBEFB16E2E3EULL, 0x000FEC0BE31EBDE8ULL,
    0x000FEC2752B15A15ULL, 0x000FEC42049DAFD3ULL, 0x000FEC5BFD29F196ULL,
    0x000FEC75406CEEF4ULL, 0x000FEC8DD2500CB4ULL, 0x000FECA5B6911F12ULL,
    0x000FECBCF0C427FEULL, 0x000FECD38454FB15ULL, 0x000FECE97488C8B3ULL,
    0x000FECFEC47F91B7ULL, 0x000FED1377358528ULL, 0x000FED278F844903ULL,
    0x000FED3B10242F4CULL, 0x000FED4DFBAD586EULL, 0x000FED605498C3DDULL,
    0x000FED721D414FE8ULL, 0x000FED8357E4A982ULL, 0x000FED9406A42CC8ULL,
    0x000FEDA42B85B704ULL, 0x000FEDB3C8746AB4ULL, 0x000FEDC2DF416652ULL,
    0x000FEDD171A46E52ULL, 0x000FEDDF813C8AD3ULL, 0x000FEDED0F909980ULL,
    0x000FEDFA1E0FD414ULL, 0x000FEE06AE124BC4ULL, 0x000FEE12C0D95A06ULL,
    0x000FEE1E579006E0ULL, 0x000FEE29734B6524ULL, 0x000FEE34150AE4BCULL,
    0x000FEE3E3DB89B3CULL, 0x000FEE47EE2982F4ULL, 0x000FEE51271DB086ULL,
    0x000FEE59E9407F41ULL, 0x000FEE623528B42EULL, 0x000FEE6A0B5897F1ULL,
    0x000FEE716C3E077AULL, 0x000FEE7858327B82ULL, 0x000FEE7ECF7B06BAULL,
    0x000FEE84D2484AB2ULL, 0x000FEE8A60B66343ULL, 0x000FEE8F7ACCC851ULL,
    0x000FEE94207E25DAULL, 0x000FEE9851A829EAULL, 0x000FEE9C0E13485CULL,
    0x000FEE9F557273F4ULL, 0x000FEEA22762CCAEULL, 0x000FEEA4836B42ACULL,
    0x000FEEA668FC2D71ULL, 0x000FEEA7D76ED6FAULL, 0x000FEEA8CE04FA0AULL,
    0x000FEEA94BE8333BULL, 0x000FEEA950296410ULL, 0x000FEEA8D9C0075EULL,
    0x000FEEA7E7897654ULL, 0x000FEEA678481D24ULL, 0x000FEEA48AA29E83ULL,
    0x000FEEA21D22E4DAULL, 0x000FEE9F2E352024ULL, 0x000FEE9BBC26AF2EULL,
    0x000FEE97C524F2E4ULL, 0x000FEE93473C0A3AULL, 0x000FEE8E40557516ULL,
    0x000FEE88AE369C7AULL, 0x000FEE828E7F3DFDULL, 0x000FEE7BDEA7B888ULL,
    0x000FEE749BFF37FFULL, 0x000FEE6CC3A9BD5EULL, 0x000FEE64529E007EULL,
    0x000FEE5B45A32888ULL, 0x000FEE51994E57B6ULL, 0x000FEE474A0006CFULL,
    0x000FEE3C53E12C50ULL, 0x000FEE30B2E02AD8ULL, 0x000FEE2462AD8205ULL,
    0x000FEE175EB83C5AULL, 0x000FEE09A22A1447ULL, 0x000FEDFB27E349CCULL,
    0x000FEDEBEA76216CULL, 0x000FEDDBE422047EULL, 0x000FEDCB0ECE39D3ULL,
    0x000FEDB964042CF4ULL, 0x000FEDA6DCE938C9ULL, 0x000FED937237E98DULL,
    0x000FED7F1C38A836ULL, 0x000FED69D2B9C02BULL, 0x000FED538D06AE00ULL,
    0x000FED3C41DEA422ULL, 0x000FED23E76A2FD8ULL, 0x000FED0A732FE644ULL,
    0x000FECEFDA07FE34ULL, 0x000FECD4100EB7B8ULL, 0x000FECB708956EB4ULL,
    0x000FEC98B61230C1ULL, 0x000FEC790A0DA978ULL, 0x000FEC57F50F31FEULL,
    0x000FEC356686C962ULL, 0x000FEC114CB4B335ULL, 0x000FEBEB948E6FD0ULL,
    0x000FEBC429A0B692ULL, 0x000FEB9AF5EE0CDCULL, 0x000FEB6FE1C98542ULL,
    0x000FEB42D3AD1F9EULL, 0x000FEB13B00B2D4BULL, 0x000FEAE2591A02E9ULL,
    0x000FEAAEAE992257ULL, 0x000FEA788D8EE326ULL, 0x000FEA3FCFFD73E5ULL,
    0x000FEA044C8DD9F6ULL, 0x000FE9C5D62F563BULL, 0x000FE9843BA947A4ULL,
    0x000FE93F471D4728ULL, 0x000FE8F6BD76C5D6ULL, 0x000FE8AA5DC4E8E6ULL,
    0x000FE859E07AB1EAULL, 0x000FE804F690A940ULL, 0x000FE7AB488233C0ULL,
    0x000FE74C751F6AA5ULL, 0x000FE6E8102AA202ULL, 0x000FE67DA0B6ABD8ULL,
    0x000FE60C9F38307EULL, 0x000FE5947338F742ULL, 0x000FE51470977280ULL,
    0x000FE48BD436F458ULL, 0x000FE3F9BFFD1E37ULL, 0x000FE35D35EEB19CULL,
    0x000FE2B5122FE4FEULL, 0x000FE20003995557ULL, 0x000FE13C82788314ULL,
    0x000FE068C4EE67B0ULL, 0x000FDF82B02B71AAULL, 0x000FDE87C57EFEAAULL,
    0x000FDD7509C63BFDULL, 0x000FDC46E529BF13ULL, 0x000FDAF8F82E0282ULL,
    0x000FD985E1B2BA75ULL, 0x000FD7E6EF48CF04ULL, 0x000FD613ADBD650BULL,
    0x000FD40149E2F012ULL, 0x000FD1A1A7B4C7ACULL, 0x000FCEE204761F9EULL,
    0x000FCBA8D85E11B2ULL, 0x000FC7D26ECD2D22ULL, 0x000FC32B2F1E22EDULL,
    0x000FBD6581C0B83AULL, 0x000FB606C4005434ULL, 0x000FAC40582A2874ULL,
    0x000F9E971E014598ULL, 0x000F89FA48A41DFCULL, 0x000F66C5F7F0302CULL,
    0x000F1A5A4B331C4AULL};

static const double wi_double[] = {
    8.68362706080130616677e-16, 4.77933017572773682428e-17,
    6.35435241740526230246e-17, 7.45487048124769627714e-17,
    8.32936681579309972857e-17, 9.06806040505948228243e-17,
    9.71486007656776183958e-17, 1.02947503142410192108e-16,
    1.08234302884476839838e-16, 1.13114701961090307945e-16,
    1.17663594570229211411e-16, 1.21936172787143633280e-16,
    1.25974399146370927864e-16, 1.29810998862640315416e-16,
    1.33472037368241227547e-16, 1.36978648425712032797e-16,
    1.40348230012423820659e-16, 1.43595294520569430270e-16,
    1.46732087423644219083e-16, 1.49769046683910367425e-16,
    1.52715150035961979750e-16, 1.55578181694607639484e-16,
    1.58364940092908853989e-16, 1.61081401752749279325e-16,
    1.63732852039698532012e-16, 1.66323990584208352778e-16,
    1.68859017086765964015e-16, 1.71341701765596607184e-16,
    1.73775443658648593310e-16, 1.76163319230009959832e-16,
    1.78508123169767272927e-16, 1.80812402857991522674e-16,
    1.83078487648267501776e-16, 1.85308513886180189386e-16,
    1.87504446393738816849e-16, 1.89668097007747596212e-16,
    1.91801140648386198029e-16, 1.93905129306251037069e-16,
    1.95981504266288244037e-16, 1.98031606831281739736e-16,
    2.00056687762733300198e-16, 2.02057915620716538808e-16,
    2.04036384154802118313e-16, 2.05993118874037063144e-16,
    2.07929082904140197311e-16, 2.09845182223703516690e-16,
    2.11742270357603418769e-16, 2.13621152594498681022e-16,
    2.15482589785814580926e-16, 2.17327301775643674990e-16,
    2.19155970504272708519e-16, 2.20969242822353175995e-16,
    2.22767733047895534948e-16, 2.24552025294143552381e-16,
    2.26322675592856786566e-16, 2.28080213834501706782e-16,
    2.29825145544246839061e-16, 2.31557953510408037008e-16,
    2.33279099280043561128e-16, 2.34989024534709550938e-16,
    2.36688152357916037468e-16, 2.38376888404542434981e-16,
    2.40055621981350627349e-16, 2.41724727046750252175e-16,
    2.43384563137110286400e-16, 2.45035476226149539878e-16,
    2.46677799523270498158e-16, 2.48311854216108767769e-16,
    2.49937950162045242375e-16, 2.51556386532965786439e-16,
    2.53167452417135826983e-16, 2.54771427381694417303e-16,
    2.56368581998939683749e-16, 2.57959178339286723500e-16,
    2.59543470433517070146e-16, 2.61121704706701939097e-16,
    2.62694120385972564623e-16, 2.64260949884118951286e-16,
    2.65822419160830680292e-16, 2.67378748063236329361e-16,
    2.68930150647261591777e-16, 2.70476835481199518794e-16,
    2.72019005932773206655e-16, 2.73556860440867908686e-16,
    2.75090592773016664571e-16, 2.76620392269639032183e-16,
    2.78146444075954410103e-16, 2.79668929362423005309e-16,
    2.81188025534502074329e-16, 2.82703906432447923059e-16,
    2.84216742521840606520e-16, 2.85726701075460149289e-16,
    2.87233946347097994381e-16, 2.88738639737848191815e-16,
    2.90240939955384233230e-16, 2.91741003166694553259e-16,
    2.93238983144718163965e-16, 2.94735031409293489611e-16,
    2.96229297362806647792e-16, 2.97721928420902891115e-16,
    2.99213070138601307081e-16, 3.00702866332133102993e-16,
    3.02191459196806151971e-16, 3.03678989421180184427e-16,
    3.05165596297821922381e-16, 3.06651417830895451744e-16,
    3.08136590840829717032e-16, 3.09621251066292253306e-16,
    3.11105533263689296831e-16, 3.12589571304399892784e-16,
    3.14073498269944617203e-16, 3.15557446545280064031e-16,
    3.17041547910402852545e-16, 3.18525933630440648871e-16,
    3.20010734544401137886e-16, 3.21496081152744704901e-16,
    3.22982103703941557538e-16, 3.24468932280169778077e-16,
    3.25956696882307838340e-16, 3.27445527514370671802e-16,
    3.28935554267536967851e-16, 3.30426907403912838589e-16,
    3.31919717440175233652e-16, 3.33414115231237245918e-16,
    3.34910232054077845412e-16, 3.36408199691876507948e-16,
    3.37908150518594979994e-16, 3.39410217584148914282e-16,
    3.40914534700312603713e-16, 3.42421236527501816058e-16,
    3.43930458662583133920e-16, 3.45442337727858401604e-16,
    3.46957011461378353333e-16, 3.48474618808741370700e-16,
    3.49995300016538099813e-16, 3.51519196727607440975e-16,
    3.53046452078274009054e-16, 3.54577210797743572160e-16,
    3.56111619309838843415e-16, 3.57649825837265051035e-16,
    3.59191980508602994994e-16, 3.60738235468235137839e-16,
    3.62288744989419151904e-16, 3.63843665590734438546e-16,
    3.65403156156136995766e-16, 3.66967378058870090021e-16,
    3.68536495289491401456e-16, 3.70110674588289834952e-16,
    3.71690085582382297792e-16, 3.73274900927794352614e-16,
    3.74865296456848868882e-16, 3.76461451331202869131e-16,
    3.78063548200896037651e-16, 3.79671773369794425924e-16,
    3.81286316967837738238e-16, 3.82907373130524317507e-16,
    3.84535140186095955858e-16, 3.86169820850914927119e-16,
    3.87811622433558721164e-16, 3.89460757048192620674e-16,
    3.91117441837820542060e-16, 3.92781899208054153270e-16,
    3.94454357072087711446e-16, 3.96135049107613542983e-16,
    3.97824215026468259474e-16, 3.99522100857856502444e-16,
    4.01228959246062907451e-16, 4.02945049763632792393e-16,
    4.04670639241074995115e-16, 4.06406002114225038723e-16,
    4.08151420790493873480e-16, 4.09907186035326643447e-16,
    4.11673597380302570170e-16, 4.13450963554423599878e-16,
    4.15239602940268833891e-16, 4.17039844056831587498e-16,
    4.18852026071011229572e-16, 4.20676499339901510978e-16,
    4.22513625986204937320e-16, 4.24363780509307796137e-16,
    4.26227350434779809917e-16, 4.28104737005311666397e-16,
    4.29996355916383230161e-16, 4.31902638100262944617e-16,
    4.33824030562279080411e-16, 4.35760997273684900553e-16,
    4.37714020125858747008e-16, 4.39683599951052137423e-16,
    4.41670257615420348435e-16, 4.43674535190656726604e-16,
    4.45696997211204306674e-16, 4.47738232024753387312e-16,
    4.49798853244554968009e-16, 4.51879501313005876278e-16,
    4.53980845187003400947e-16, 4.56103584156742206384e-16,
    4.58248449810956667052e-16, 4.60416208163115281428e-16,
    4.62607661954784567754e-16, 4.64823653154320737780e-16,
    4.67065065671263059081e-16, 4.69332828309332890697e-16,
    4.71627917983835129766e-16, 4.73951363232586715165e-16,
    4.76304248053313737663e-16, 4.78687716104872284247e-16,
    4.81102975314741720538e-16, 4.83551302941152515162e-16,
    4.86034051145081195402e-16, 4.88552653135360343280e-16,
    4.91108629959526955862e-16, 4.93703598024033454728e-16,
    4.96339277440398725619e-16, 4.99017501309182245754e-16,
    5.01740226071808946011e-16, 5.04509543081872748637e-16,
    5.07327691573354207058e-16, 5.10197073234156184149e-16,
    5.13120268630678373200e-16, 5.16100055774322824569e-16,
    5.19139431175769859873e-16, 5.22241633800023428760e-16,
    5.25410172417759732697e-16, 5.28648856950494511482e-16,
    5.31961834533840037535e-16, 5.35353631181649688145e-16,
    5.38829200133405320160e-16, 5.42393978220171234073e-16,
    5.46053951907478041166e-16, 5.49815735089281410703e-16,
    5.53686661246787600374e-16, 5.57674893292657647836e-16,
    5.61789555355541665830e-16, 5.66040892008242216739e-16,
    5.70440462129138908417e-16, 5.75001376891989523684e-16,
    5.79738594572459365014e-16, 5.84669289345547900201e-16,
    5.89813317647789942685e-16, 5.95193814964144415532e-16,
    6.00837969627190832234e-16, 6.06778040933344851394e-16,
    6.13052720872528159123e-16, 6.19708989458162555387e-16,
    6.26804696330128439415e-16, 6.34412240712750598627e-16,
    6.42623965954805540945e-16, 6.51560331734499356881e-16,
    6.61382788509766415145e-16, 6.72315046250558662913e-16,
    6.84680341756425875856e-16, 6.98971833638761995415e-16,
    7.15999493483066421560e-16, 7.37242430179879890722e-16,
    7.65893637080557275482e-16, 8.11384933765648418565e-16};

static const double fi_double[] = {
    1.00000000000000000000e+00, 9.77101701267671596263e-01,
    9.59879091800106665211e-01, 9.45198953442299649730e-01,
    9.32060075959230460718e-01, 9.19991505039347012840e-01,
    9.08726440052130879366e-01, 8.98095921898343418910e-01,
    8.87984660755833377088e-01, 8.78309655808917399966e-01,
    8.69008688036857046555e-01, 8.60033621196331532488e-01,
    8.51346258458677951353e-01, 8.42915653112204177333e-01,
    8.34716292986883434679e-01, 8.26726833946221373317e-01,
    8.18929191603702366642e-01, 8.11307874312656274185e-01,
    8.03849483170964274059e-01, 7.96542330422958966274e-01,
    7.89376143566024590648e-01, 7.82341832654802504798e-01,
    7.75431304981187174974e-01, 7.68637315798486264740e-01,
    7.61953346836795386565e-01, 7.55373506507096115214e-01,
    7.48892447219156820459e-01, 7.42505296340151055290e-01,
    7.36207598126862650112e-01, 7.29995264561476231435e-01,
    7.23864533468630222401e-01, 7.17811932630721960535e-01,
    7.11834248878248421200e-01, 7.05928501332754310127e-01,
    7.00091918136511615067e-01, 6.94321916126116711609e-01,
    6.88616083004671808432e-01, 6.82972161644994857355e-01,
    6.77388036218773526009e-01, 6.71861719897082099173e-01,
    6.66391343908750100056e-01, 6.60975147776663107813e-01,
    6.55611470579697264149e-01, 6.50298743110816701574e-01,
    6.45035480820822293424e-01, 6.39820277453056585060e-01,
    6.34651799287623608059e-01, 6.29528779924836690007e-01,
    6.24450015547026504592e-01, 6.19414360605834324325e-01,
    6.14420723888913888899e-01, 6.09468064925773433949e-01,
    6.04555390697467776029e-01, 5.99681752619125263415e-01,
    5.94846243767987448159e-01, 5.90047996332826008015e-01,
    5.85286179263371453274e-01, 5.80559996100790898232e-01,
    5.75868682972353718164e-01, 5.71211506735253227163e-01,
    5.66587763256164445025e-01, 5.61996775814524340831e-01,
    5.57437893618765945014e-01, 5.52910490425832290562e-01,
    5.48413963255265812791e-01, 5.43947731190026262382e-01,
    5.39511234256952132426e-01, 5.35103932380457614215e-01,
    5.30725304403662057062e-01, 5.26374847171684479008e-01,
    5.22052074672321841931e-01, 5.17756517229756352272e-01,
    5.13487720747326958914e-01, 5.09245245995747941592e-01,
    5.05028667943468123624e-01, 5.00837575126148681903e-01,
    4.96671569052489714213e-01, 4.92530263643868537748e-01,
    4.88413284705458028423e-01, 4.84320269426683325253e-01,
    4.80250865909046753544e-01, 4.76204732719505863248e-01,
    4.72181538467730199660e-01, 4.68180961405693596422e-01,
    4.64202689048174355069e-01, 4.60246417812842867345e-01,
    4.56311852678716434184e-01, 4.52398706861848520777e-01,
    4.48506701507203064949e-01, 4.44635565395739396077e-01,
    4.40785034665803987508e-01, 4.36954852547985550526e-01,
    4.33144769112652261445e-01, 4.29354541029441427735e-01,
    4.25583931338021970170e-01, 4.21832709229495894654e-01,
    4.18100649837848226120e-01, 4.14387534040891125642e-01,
    4.10693148270188157500e-01, 4.07017284329473372217e-01,
    4.03359739221114510510e-01, 3.99720314980197222177e-01,
    3.96098818515832451492e-01, 3.92495061459315619512e-01,
    3.88908860018788715696e-01, 3.85340034840077283462e-01,
    3.81788410873393657674e-01, 3.78253817245619183840e-01,
    3.74736087137891138443e-01, 3.71235057668239498696e-01,
    3.67750569779032587814e-01, 3.64282468129004055601e-01,
    3.60830600989648031529e-01, 3.57394820145780500731e-01,
    3.53974980800076777232e-01, 3.50570941481406106455e-01,
    3.47182563956793643900e-01, 3.43809713146850715049e-01,
    3.40452257044521866547e-01, 3.37110066637006045021e-01,
    3.33783015830718454708e-01, 3.30470981379163586400e-01,
    3.27173842813601400970e-01, 3.23891482376391093290e-01,
    3.20623784956905355514e-01, 3.17370638029913609834e-01,
    3.14131931596337177215e-01, 3.10907558126286509559e-01,
    3.07697412504292056035e-01, 3.04501391976649993243e-01,
    3.01319396100803049698e-01, 2.98151326696685481377e-01,
    2.94997087799961810184e-01, 2.91856585617095209972e-01,
    2.88729728482182923521e-01, 2.85616426815501756042e-01,
    2.82516593083707578948e-01, 2.79430141761637940157e-01,
    2.76356989295668320494e-01, 2.73297054068577072172e-01,
    2.70250256365875463072e-01, 2.67216518343561471038e-01,
    2.64195763997261190426e-01, 2.61187919132721213522e-01,
    2.58192911337619235290e-01, 2.55210669954661961700e-01,
    2.52241126055942177508e-01, 2.49284212418528522415e-01,
    2.46339863501263828249e-01, 2.43408015422750312329e-01,
    2.40488605940500588254e-01, 2.37581574431238090606e-01,
    2.34686861872330010392e-01, 2.31804410824338724684e-01,
    2.28934165414680340644e-01, 2.26076071322380278694e-01,
    2.23230075763917484855e-01, 2.20396127480151998723e-01,
    2.17574176724331130872e-01, 2.14764175251173583536e-01,
    2.11966076307030182324e-01, 2.09179834621125076977e-01,
    2.06405406397880797353e-01, 2.03642749310334908452e-01,
    2.00891822494656591136e-01, 1.98152586545775138971e-01,
    1.95425003514134304483e-01, 1.92709036903589175926e-01,
    1.90004651670464985713e-01, 1.87311814223800304768e-01,
    1.84630492426799269756e-01, 1.81960655599522513892e-01,
    1.79302274522847582272e-01, 1.76655321443734858455e-01,
    1.74019770081838553999e-01, 1.71395595637505754327e-01,
    1.68782774801211288285e-01, 1.66181285764481906364e-01,
    1.63591108232365584074e-01, 1.61012223437511009516e-01,
    1.58444614155924284882e-01, 1.55888264724479197465e-01,
    1.53343161060262855866e-01, 1.50809290681845675763e-01,
    1.48286642732574552861e-01, 1.45775208005994028060e-01,
    1.43274978973513461566e-01, 1.40785949814444699690e-01,
    1.38308116448550733057e-01, 1.35841476571253755301e-01,
    1.33386029691669155683e-01, 1.30941777173644358090e-01,
    1.28508722279999570981e-01, 1.26086870220185887081e-01,
    1.23676228201596571932e-01, 1.21276805484790306533e-01,
    1.18888613442910059947e-01, 1.16511665625610869035e-01,
    1.14145977827838487895e-01, 1.11791568163838089811e-01,
    1.09448457146811797824e-01, 1.07116667774683801961e-01,
    1.04796225622487068629e-01, 1.02487158941935246892e-01,
    1.00189498768810017482e-01, 9.79032790388624646338e-02,
    9.56285367130089991594e-02, 9.33653119126910124859e-02,
    9.11136480663737591268e-02, 8.88735920682758862021e-02,
    8.66451944505580717859e-02, 8.44285095703534715916e-02,
    8.22235958132029043366e-02, 8.00305158146630696292e-02,
    7.78493367020961224423e-02, 7.56801303589271778804e-02,
    7.35229737139813238622e-02, 7.13779490588904025339e-02,
    6.92451443970067553879e-02, 6.71246538277884968737e-02,
    6.50165779712428976156e-02, 6.29210244377581412456e-02,
    6.08381083495398780614e-02, 5.87679529209337372930e-02,
    5.67106901062029017391e-02, 5.46664613248889208474e-02,
    5.26354182767921896513e-02, 5.06177238609477817000e-02,
    4.86135532158685421122e-02, 4.66230949019303814174e-02,
    4.46465522512944634759e-02, 4.26841449164744590750e-02,
    4.07361106559409394401e-02, 3.88027074045261474722e-02,
    3.68842156885673053135e-02, 3.49809414617161251737e-02,
    3.30932194585785779961e-02, 3.12214171919203004046e-02,
    2.93659397581333588001e-02, 2.75272356696031131329e-02,
    2.57058040085489103443e-02, 2.39022033057958785407e-02,
    2.21170627073088502113e-02, 2.03510962300445102935e-02,
    1.86051212757246224594e-02, 1.68800831525431419000e-02,
    1.51770883079353092332e-02, 1.34974506017398673818e-02,
    1.18427578579078790488e-02, 1.02149714397014590439e-02,
    8.61658276939872638800e-03, 7.05087547137322242369e-03,
    5.52240329925099155545e-03, 4.03797259336302356153e-03,
    2.60907274610215926189e-03, 1.26028593049859797236e-03};

static const double ziggurat_nor_r = 3.6541528853610087963519472518;
static const double ziggurat_nor_inv_r =
    0.27366123732975827203338247596; // 1.0 / ziggurat_nor_r;


} // namespace

Generator::Generator(uint64_t seed) {
	// SeedSequence(seed).generate_state(4, np.uint64) (bit_generator.pyx)
	const uint32_t INIT_A = 0x43b0d7e5, MULT_A = 0x931e8875, INIT_B = 0x8b51f9dd, MULT_B = 0x58f38ded;
	const uint32_t MIX_MULT_L = 0xca01f9dd, MIX_MULT_R = 0x4973f715;
	std::vector<uint32_t> entropy;
	if (seed == 0)
		entropy.push_back(0);
	while (seed) {
		entropy.push_back(uint32_t(seed));
		seed >>= 32;
	}
	uint32_t hc = INIT_A;
	auto hashmix = [&](uint32_t v) {
		v ^= hc;
		hc *= MULT_A;
		v *= hc;
		v ^= v >> 16;
		return v;
	};
	auto mix = [&](uint32_t x, uint32_t y) {
		uint32_t r = MIX_MULT_L * x - MIX_MULT_R * y;
		r ^= r >> 16;
		return r;
	};
	uint32_t pool[4];
	for (size_t i = 0; i < 4; i++)
		pool[i] = hashmix(i < entropy.size() ? entropy[i] : 0);
	for (size_t s = 0; s < 4; s++)
		for (size_t d = 0; d < 4; d++)
			if (s != d)
				pool[d] = mix(pool[d], hashmix(pool[s]));
	for (size_t s = 4; s < entropy.size(); s++)
		for (size_t d = 0; d < 4; d++)
			pool[d] = mix(pool[d], hashmix(entropy[s]));
	uint32_t hb = INIT_B;
	uint32_t st[8];
	for (int i = 0; i < 8; i++) {
		uint32_t v = pool[i % 4];
		v ^= hb;
		hb *= MULT_B;
		v *= hb;
		v ^= v >> 16;
		st[i] = v;
	}
	uint64_t w[4];
	for (int k = 0; k < 4; k++)
		w[k] = uint64_t(st[2 * k]) | (uint64_t(st[2 * k + 1]) << 32);
	// pcg64_set_seed: state w0:w1 (high:low), increment from w2:w3
	U128 initseq{w[2], w[3]};
	incHi = (initseq.hi << 1) | (initseq.lo >> 63);
	incLo = (initseq.lo << 1) | 1;
	stHi = stLo = 0;
	step();
	U128 s = add128(U128{stHi, stLo}, U128{w[0], w[1]});
	stHi = s.hi;
	stLo = s.lo;
	step();
}

void Generator::step() {
	const U128 MULT{0x2360ed051fc65da4ull, 0x4385df649fccf645ull};
	U128 s = add128(mul128(U128{stHi, stLo}, MULT), U128{incHi, incLo});
	stHi = s.hi;
	stLo = s.lo;
}

uint64_t Generator::nextU64() {
	step();
	uint64_t x = stHi ^ stLo;
	unsigned rot = unsigned(stHi >> 58);
	return (x >> rot) | (x << ((64 - rot) & 63));
}

uint32_t Generator::nextU32() {
	if (hasU32) {
		hasU32 = false;
		return u32;
	}
	uint64_t n = nextU64();
	hasU32 = true;
	u32 = uint32_t(n >> 32);
	return uint32_t(n);
}

double Generator::nextDouble() {
	return double(nextU64() >> 11) * (1.0 / 9007199254740992.0);
}

double Generator::random() {
	return nextDouble();
}

std::vector<double> Generator::random(size_t n) {
	std::vector<double> o(n);
	for (auto& v : o)
		v = nextDouble();
	return o;
}

double Generator::uniform(double lo, double hi) {
	// random_uniform(lower, range): lower + range * next_double
	double range = hi - lo;
	return lo + range * nextDouble();
}

std::vector<double> Generator::uniform(double lo, double hi, size_t n) {
	std::vector<double> o(n);
	for (auto& v : o)
		v = uniform(lo, hi);
	return o;
}

int64_t Generator::integers(int64_t lo, int64_t hi) {
	// random_bounded_uint64_fill(off = lo, rng = hi - 1 - lo), Lemire
	uint64_t rng = uint64_t(hi) - 1 - uint64_t(lo);
	if (hi <= lo)
		throw std::invalid_argument("integers: high <= low");
	if (rng == 0)
		return lo;
	if (rng <= 0xffffffffu) {
		if (rng == 0xffffffffu)
			return int64_t(uint64_t(lo) + nextU32());
		const uint32_t excl = uint32_t(rng) + 1;
		uint64_t m = uint64_t(nextU32()) * excl;
		uint32_t leftover = uint32_t(m);
		if (leftover < excl) {
			const uint32_t threshold = (0xffffffffu - uint32_t(rng)) % excl;
			while (leftover < threshold) {
				m = uint64_t(nextU32()) * excl;
				leftover = uint32_t(m);
			}
		}
		return int64_t(uint64_t(lo) + (m >> 32));
	}
	if (rng == ~uint64_t(0))
		return int64_t(uint64_t(lo) + nextU64());
	const uint64_t excl = rng + 1;
	U128 m = mul64(nextU64(), excl);
	if (m.lo < excl) {
		const uint64_t threshold = (~uint64_t(0) - rng) % excl;
		while (m.lo < threshold)
			m = mul64(nextU64(), excl);
	}
	return int64_t(uint64_t(lo) + m.hi);
}

std::vector<int64_t> Generator::integers(int64_t lo, int64_t hi, size_t n) {
	std::vector<int64_t> o(n);
	for (auto& v : o)
		v = integers(lo, hi);
	return o;
}

double Generator::standardNormal() {
	// random_standard_normal (distributions.c)
	for (;;) {
		uint64_t r = nextU64();
		int idx = int(r & 0xff);
		r >>= 8;
		int sign = int(r & 0x1);
		uint64_t rabs = (r >> 1) & 0x000fffffffffffffull;
		double x = double(rabs) * wi_double[idx];
		if (sign & 0x1)
			x = -x;
		if (rabs < ki_double[idx])
			return x;
		if (idx == 0) {
			for (;;) {
				double xx = -ziggurat_nor_inv_r * std::log1p(-nextDouble());
				double yy = -std::log1p(-nextDouble());
				if (yy + yy > xx * xx)
					return ((rabs >> 8) & 0x1) ? -(ziggurat_nor_r + xx) : ziggurat_nor_r + xx;
			}
		} else {
			if (((fi_double[idx - 1] - fi_double[idx]) * nextDouble() + fi_double[idx]) < std::exp(-0.5 * x * x))
				return x;
		}
	}
}

double Generator::normal(double loc, double scale) {
	return loc + scale * standardNormal();
}

std::vector<double> Generator::normal(double loc, double scale, size_t n) {
	std::vector<double> o(n);
	for (auto& v : o)
		v = normal(loc, scale);
	return o;
}

// ===========================================================================
// Pillow: premultiplied alpha (Convert.c rgbA2rgba / rgba2rgbA)

static void premultiply(Img8& im) {
	for (size_t i = 0; i < im.v.size(); i += 4) {
		unsigned a = im.v[i + 3];
		for (int k = 0; k < 3; k++) {
			unsigned t = im.v[i + k] * a + 128;
			im.v[i + k] = uint8_t(((t >> 8) + t) >> 8);
		}
	}
}

static void unpremultiply(Img8& im) {
	for (size_t i = 0; i < im.v.size(); i += 4) {
		unsigned a = im.v[i + 3];
		if (a == 0 || a == 255)
			continue;
		for (int k = 0; k < 3; k++)
			im.v[i + k] = uint8_t(std::min(255u, 255u * im.v[i + k] / a));
	}
}

// ===========================================================================
// Pillow: Image.resize (Resample.c)

namespace {

double boxFilter(double x) {
	return (x > -0.5 && x <= 0.5) ? 1.0 : 0.0;
}
double bilinearFilter(double x) {
	if (x < 0.0)
		x = -x;
	return x < 1.0 ? 1.0 - x : 0.0;
}
double hammingFilter(double x) {
	if (x < 0.0)
		x = -x;
	if (x == 0.0)
		return 1.0;
	if (x >= 1.0)
		return 0.0;
	x = x * kPi;
	return std::sin(x) / x * (0.54f + 0.46f * std::cos(x));
}
double bicubicFilter(double x) {
	const double a = -0.5;
	if (x < 0.0)
		x = -x;
	if (x < 1.0)
		return ((a + 2.0) * x - (a + 3.0)) * x * x + 1;
	if (x < 2.0)
		return (((x - 5) * x + 8) * x - 4) * a;
	return 0.0;
}
double sincFilter(double x) {
	if (x == 0.0)
		return 1.0;
	x = x * kPi;
	return std::sin(x) / x;
}
double lanczosFilter(double x) {
	if (-3.0 <= x && x < 3.0)
		return sincFilter(x) * sincFilter(x / 3);
	return 0.0;
}

struct Filter {
	double (*f)(double);
	double support;
};

Filter filterFor(Resample r) {
	switch (r) {
		case Resample::Box: return {boxFilter, 0.5};
		case Resample::Bilinear: return {bilinearFilter, 1.0};
		case Resample::Hamming: return {hammingFilter, 1.0};
		case Resample::Bicubic: return {bicubicFilter, 2.0};
		case Resample::Lanczos: return {lanczosFilter, 3.0};
		default: throw std::invalid_argument("npcore resize: unsupported filter");
	}
}

const int PRECISION_BITS = 32 - 8 - 2;

struct Coeffs {
	int ksize = 0;
	std::vector<int> bounds; // xmin, count per output
	std::vector<int> kk; // fixed point
};

// precompute_coeffs + normalize_coeffs_8bpc
Coeffs coeffs(int inSize, float in0, float in1, int outSize, Filter fp) {
	Coeffs c;
	double scale = double(in1 - in0) / outSize, filterscale = scale;
	if (filterscale < 1.0)
		filterscale = 1.0;
	double support = fp.support * filterscale;
	c.ksize = int(std::ceil(support)) * 2 + 1;
	c.bounds.assign(size_t(outSize) * 2, 0);
	c.kk.assign(size_t(outSize) * c.ksize, 0);
	std::vector<double> k(size_t(c.ksize));
	for (int xx = 0; xx < outSize; xx++) {
		double center = in0 + (xx + 0.5) * scale, ww = 0.0, ss = 1.0 / filterscale;
		int xmin = int(center - support + 0.5);
		if (xmin < 0)
			xmin = 0;
		int xmax = int(center + support + 0.5);
		if (xmax > inSize)
			xmax = inSize;
		xmax -= xmin;
		for (int x = 0; x < xmax; x++) {
			double w = fp.f((x + xmin - center + 0.5) * ss);
			k[size_t(x)] = w;
			ww += w;
		}
		for (int x = 0; x < xmax; x++)
			if (ww != 0.0)
				k[size_t(x)] /= ww;
		for (int x = xmax; x < c.ksize; x++)
			k[size_t(x)] = 0;
		for (int x = 0; x < c.ksize; x++) {
			double v = k[size_t(x)];
			c.kk[size_t(xx) * c.ksize + x] = v < 0 ? int(-0.5 + v * (1 << PRECISION_BITS)) : int(0.5 + v * (1 << PRECISION_BITS));
		}
		c.bounds[size_t(xx) * 2] = xmin;
		c.bounds[size_t(xx) * 2 + 1] = xmax;
	}
	return c;
}

inline uint8_t clip8(int in) {
	in >>= PRECISION_BITS; // arithmetic, as Pillow's clip8_lookups index
	return uint8_t(in < 0 ? 0 : in > 255 ? 255 : in);
}

} // namespace

Img8 resize(const Img8& src, int W, int H, Resample r) {
	if (W < 1 || H < 1)
		throw std::invalid_argument("npcore resize: size must be > 0");
	if (W == src.w && H == src.h)
		return src;
	if (r == Resample::Nearest)
		throw std::invalid_argument("npcore resize: NEAREST is not ported");
	Img8 im = src;
	if (im.c == 4)
		premultiply(im);
	const int C = im.c;
	Filter fp = filterFor(r);
	bool needH = W != im.w, needV = H != im.h;
	Coeffs ch = coeffs(im.w, 0.0f, float(im.w), W, fp);
	Coeffs cv = coeffs(im.h, 0.0f, float(im.h), H, fp);
	int yFirst = cv.bounds[0];
	int yLast = cv.bounds[size_t(H) * 2 - 2] + cv.bounds[size_t(H) * 2 - 1];
	if (needH) {
		for (int i = 0; i < H; i++)
			cv.bounds[size_t(i) * 2] -= yFirst;
		Img8 tmp(yLast - yFirst, W, C);
		for (int yy = 0; yy < tmp.h; yy++)
			for (int xx = 0; xx < W; xx++) {
				int xmin = ch.bounds[size_t(xx) * 2], n = ch.bounds[size_t(xx) * 2 + 1];
				const int* k = &ch.kk[size_t(xx) * ch.ksize];
				for (int c = 0; c < C; c++) {
					int ss = 1 << (PRECISION_BITS - 1);
					for (int x = 0; x < n; x++)
						ss += im(yy + yFirst, x + xmin, c) * k[x];
					tmp(yy, xx, c) = clip8(ss);
				}
			}
		im = std::move(tmp);
	}
	if (needV) {
		Img8 out(H, im.w, C);
		for (int yy = 0; yy < H; yy++) {
			int ymin = cv.bounds[size_t(yy) * 2], n = cv.bounds[size_t(yy) * 2 + 1];
			const int* k = &cv.kk[size_t(yy) * cv.ksize];
			for (int xx = 0; xx < im.w; xx++)
				for (int c = 0; c < C; c++) {
					int ss = 1 << (PRECISION_BITS - 1);
					for (int y = 0; y < n; y++)
						ss += im(y + ymin, xx, c) * k[y];
					out(yy, xx, c) = clip8(ss);
				}
		}
		im = std::move(out);
	}
	if (im.c == 4)
		unpremultiply(im);
	return im;
}

// ===========================================================================
// Pillow: GaussianBlur / BoxBlur (BoxBlur.c)

namespace {

float gaussianBlurRadius(float radius, int passes) {
	float sigma2, L, l, a;
	sigma2 = radius * radius / passes;
	L = float(std::sqrt(12.0 * sigma2 + 1.0));
	l = float(std::floor((L - 1.0) / 2.0));
	a = (2 * l + 1) * (l * (l + 1) - 3 * sigma2);
	a /= 6 * (sigma2 - (l + 1) * (l + 1));
	return l + a;
}

// ImagingLineBoxBlur8 / 32 on one band of a line (stride: bytes per pixel)
void lineBoxBlur(uint8_t* out, const uint8_t* in, int stride, int lastx, int radius, int edgeA, int edgeB, uint32_t ww, uint32_t fw) {
	uint32_t acc, bulk;
	auto I = [&](int x) { return uint32_t(in[size_t(x) * stride]); };
	auto move = [&](int sub, int add) { acc += I(add) - I(sub); };
	auto far = [&](int left, int right) { bulk = (acc * ww) + (I(left) + I(right)) * fw; };
	auto save = [&](int x) { out[size_t(x) * stride] = uint8_t((bulk + (1 << 23)) >> 24); };
	acc = I(0) * (radius + 1);
	for (int x = 0; x < edgeA - 1; x++)
		acc += I(x);
	acc += I(lastx) * (radius - edgeA + 1);
	if (edgeA <= edgeB) {
		for (int x = 0; x < edgeA; x++) {
			move(0, x + radius);
			far(0, x + radius + 1);
			save(x);
		}
		for (int x = edgeA; x < edgeB; x++) {
			move(x - radius - 1, x + radius);
			far(x - radius - 1, x + radius + 1);
			save(x);
		}
		for (int x = edgeB; x <= lastx; x++) {
			move(x - radius - 1, lastx);
			far(x - radius - 1, lastx);
			save(x);
		}
	} else {
		for (int x = 0; x < edgeB; x++) {
			move(0, x + radius);
			far(0, lastx);
			save(x);
		}
		for (int x = edgeB; x < edgeA; x++) {
			move(0, lastx);
			far(0, lastx);
			save(x);
		}
		for (int x = edgeA; x <= lastx; x++) {
			move(x - radius - 1, lastx);
			far(x - radius - 1, lastx);
			save(x);
		}
	}
}

// ImagingHorizontalBoxBlur, in place (Pillow commits each line after it)
void horizontalBoxBlur(Img8& im, float floatRadius) {
	int radius = int(floatRadius);
	uint32_t ww = uint32_t(float(1 << 24) / (floatRadius * 2 + 1));
	uint32_t fw = ((1 << 24) - (radius * 2 + 1) * ww) / 2;
	int edgeA = std::min(radius + 1, im.w);
	int edgeB = std::max(im.w - radius - 1, 0);
	std::vector<uint8_t> line(size_t(im.w) * im.c);
	for (int y = 0; y < im.h; y++) {
		uint8_t* row = im.px(y, 0);
		for (int c = 0; c < im.c; c++)
			lineBoxBlur(line.data() + c, row + c, im.c, im.w - 1, radius, edgeA, edgeB, ww, fw);
		std::copy(line.begin(), line.end(), row);
	}
}

Img8 transposed(const Img8& im) {
	Img8 t(im.w, im.h, im.c);
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++)
			for (int c = 0; c < im.c; c++)
				t(x, y, c) = im(y, x, c);
	return t;
}

// ImagingBoxBlur(xradius = yradius = r, n passes)
Img8 boxBlurPasses(const Img8& src, float r, int n) {
	if (r < 0)
		throw std::invalid_argument("npcore blur: radius must be >= 0");
	if (r == 0)
		return src;
	Img8 im = src;
	for (int i = 0; i < n; i++)
		horizontalBoxBlur(im, r);
	Img8 t = transposed(im);
	for (int i = 0; i < n; i++)
		horizontalBoxBlur(t, r);
	return transposed(t);
}

} // namespace

Img8 gaussianBlur(const Img8& im, double radius) {
	if (radius == 0)
		return im;
	return boxBlurPasses(im, gaussianBlurRadius(float(radius), 3), 3);
}

Img8 boxBlur(const Img8& im, double radius) {
	if (radius == 0)
		return im;
	return boxBlurPasses(im, float(radius), 1);
}

// ===========================================================================
// Pillow: RankFilter (ImageFilter.py: expand by size // 2 with edge pixels;
// RankFilter.c: the rank-th smallest of each size x size window). Done with
// a sliding histogram per column, which gives the same order statistic.

Img8 rankFilter(const Img8& im, int size, int rank) {
	if (!(size & 1) || size < 1 || rank < 0 || rank >= size * size)
		throw std::invalid_argument("npcore rankFilter: bad size or rank");
	const int m = size / 2;
	Img8 out(im.h, im.w, im.c);
	auto at = [&](int y, int x, int c) {
		y = y < 0 ? 0 : y >= im.h ? im.h - 1 : y;
		x = x < 0 ? 0 : x >= im.w ? im.w - 1 : x;
		return im(y, x, c);
	};
	for (int c = 0; c < im.c; c++)
		for (int y = 0; y < im.h; y++) {
			int hist[256] = {0};
			for (int dy = -m; dy <= m; dy++)
				for (int dx = -m; dx <= m; dx++)
					hist[at(y + dy, dx, c)]++;
			for (int x = 0; x < im.w; x++) {
				if (x > 0)
					for (int dy = -m; dy <= m; dy++) {
						hist[at(y + dy, x - 1 - m, c)]--;
						hist[at(y + dy, x + m, c)]++;
					}
				int cum = 0, v = 0;
				for (; v < 256; v++) {
					cum += hist[v];
					if (cum > rank)
						break;
				}
				out(y, x, c) = uint8_t(v);
			}
		}
	return out;
}

Img8 minFilter(const Img8& im, int size) { return rankFilter(im, size, 0); }
Img8 maxFilter(const Img8& im, int size) { return rankFilter(im, size, size * size - 1); }
Img8 medianFilter(const Img8& im, int size) { return rankFilter(im, size, size * size / 2); }

// ===========================================================================
// Pillow: Image.rotate (Image.py) through the generic affine transform
// (Geometry.c: bilinear / bicubic filters)

namespace {

// Python's round(v, 15)
double round15(double v) {
	char buf[512];
	std::snprintf(buf, sizeof buf, "%.15f", v);
	return std::strtod(buf, nullptr);
}

// Geometry.c BICUBIC (a = -1 form, as Pillow writes it)
inline double cubic(double v1, double v2, double v3, double v4, double d) {
	double p1 = v2;
	double p2 = -v1 + v3;
	double p3 = 2 * (v1 - v2) + v3 - v4;
	double p4 = -v1 + v2 - v3 + v4;
	return p1 + d * (p2 + d * (p3 + d * p4));
}

inline int floorInt(double v) {
	return v < 0.0 ? int(std::floor(v)) : int(v);
}

// one output pixel (all bands) from (xin, yin); false when outside
bool sampleBicubic(const Img8& im, double xin, double yin, uint8_t* out) {
	if (xin < 0.0 || xin >= im.w || yin < 0.0 || yin >= im.h)
		return false;
	xin -= 0.5;
	yin -= 0.5;
	int x = floorInt(xin), y = floorInt(yin);
	double dx = xin - x, dy = yin - y;
	x--;
	y--;
	auto xc = [&](int v) { return v < 0 ? 0 : v < im.w ? v : im.w - 1; };
	int x0 = xc(x), x1 = xc(x + 1), x2 = xc(x + 2), x3 = xc(x + 3);
	int yc = y < 0 ? 0 : y < im.h ? y : im.h - 1;
	for (int c = 0; c < im.c; c++) {
		auto row = [&](int yy) { return cubic(im(yy, x0, c), im(yy, x1, c), im(yy, x2, c), im(yy, x3, c), dx); };
		double v1 = row(yc);
		double v2 = (y + 1 >= 0 && y + 1 < im.h) ? row(y + 1) : v1;
		double v3 = (y + 2 >= 0 && y + 2 < im.h) ? row(y + 2) : v2;
		double v4 = (y + 3 >= 0 && y + 3 < im.h) ? row(y + 3) : v3;
		double v = cubic(v1, v2, v3, v4, dy);
		out[c] = v <= 0.0 ? 0 : v >= 255.0 ? 255 : uint8_t(v);
	}
	return true;
}

bool sampleBilinear(const Img8& im, double xin, double yin, uint8_t* out) {
	if (xin < 0.0 || xin >= im.w || yin < 0.0 || yin >= im.h)
		return false;
	xin -= 0.5;
	yin -= 0.5;
	int x = floorInt(xin), y = floorInt(yin);
	double dx = xin - x, dy = yin - y;
	auto xc = [&](int v) { return v < 0 ? 0 : v < im.w ? v : im.w - 1; };
	int x0 = xc(x), x1 = xc(x + 1);
	int yc = y < 0 ? 0 : y < im.h ? y : im.h - 1;
	for (int c = 0; c < im.c; c++) {
		auto lerp = [](double a, double b, double d) { return a + (b - a) * d; };
		double v1 = lerp(im(yc, x0, c), im(yc, x1, c), dx);
		double v2 = (y + 1 >= 0 && y + 1 < im.h) ? lerp(im(y + 1, x0, c), im(y + 1, x1, c), dx) : v1;
		out[c] = uint8_t(lerp(v1, v2, dy));
	}
	return true;
}

} // namespace

Img8 rotate(const Img8& src, double angle, Resample r, bool expand) {
	if (r != Resample::Bicubic && r != Resample::Bilinear)
		throw std::invalid_argument("npcore rotate: only BILINEAR and BICUBIC are ported");
	const int w = src.w, h = src.h;
	angle = std::fmod(angle, 360.0); // Python's float %
	if (angle != 0 && angle < 0)
		angle += 360.0;
	if (angle == 0)
		return src;
	if (angle == 180 || ((angle == 90 || angle == 270) && (expand || w == h))) {
		// Image.transpose: exact pixel moves
		bool quarter = angle != 180;
		Img8 o(quarter ? w : h, quarter ? h : w, src.c);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				int ox, oy;
				if (angle == 180)
					ox = w - 1 - x, oy = h - 1 - y;
				else if (angle == 90)
					ox = y, oy = w - 1 - x;
				else
					ox = h - 1 - y, oy = x;
				std::memcpy(o.px(oy, ox), src.px(y, x), size_t(src.c));
			}
		return o;
	}
	double cx = w / 2.0, cy = h / 2.0;
	double rad = -(angle * (kPi / 180.0)); // -math.radians(angle)
	double m[6] = {round15(std::cos(rad)), round15(std::sin(rad)), 0.0, round15(-std::sin(rad)), round15(std::cos(rad)), 0.0};
	auto tf = [&](double x, double y, double& ox, double& oy) {
		double a = m[0] * x + m[1] * y + m[2];
		double b = m[3] * x + m[4] * y + m[5];
		ox = a;
		oy = b;
	};
	tf(-cx, -cy, m[2], m[5]);
	m[2] += cx;
	m[5] += cy;
	int nw = w, nh = h;
	if (expand) {
		double xs[4], ys[4];
		const double corners[4][2] = {{0, 0}, {double(w), 0}, {double(w), double(h)}, {0, double(h)}};
		for (int i = 0; i < 4; i++)
			tf(corners[i][0], corners[i][1], xs[i], ys[i]);
		nw = int(std::ceil(*std::max_element(xs, xs + 4)) - std::floor(*std::min_element(xs, xs + 4)));
		nh = int(std::ceil(*std::max_element(ys, ys + 4)) - std::floor(*std::min_element(ys, ys + 4)));
		tf(-(nw - w) / 2.0, -(nh - h) / 2.0, m[2], m[5]);
	}
	Img8 im = src;
	if (im.c == 4)
		premultiply(im);
	Img8 out(nh, nw, im.c);
	for (int y = 0; y < nh; y++)
		for (int x = 0; x < nw; x++) {
			double xin = x + 0.5, yin = y + 0.5;
			double sx = m[0] * xin + m[1] * yin + m[2];
			double sy = m[3] * xin + m[4] * yin + m[5];
			uint8_t* o = out.px(y, x);
			bool ok = r == Resample::Bicubic ? sampleBicubic(im, sx, sy, o) : sampleBilinear(im, sx, sy, o);
			if (!ok)
				std::memset(o, 0, size_t(im.c));
		}
	if (out.c == 4)
		unpremultiply(out);
	return out;
}

// ===========================================================================
// Pillow: ImageDraw on "L" (Draw.c, _imaging.c, ImageDraw.py)

namespace {

void hline8(Img8& im, int x0, int y0, int x1, uint8_t ink) {
	if (y0 < 0 || y0 >= im.h)
		return;
	if (x0 < 0)
		x0 = 0;
	else if (x0 >= im.w)
		return;
	if (x1 < 0)
		return;
	else if (x1 >= im.w)
		x1 = im.w - 1;
	for (int x = x0; x <= x1; x++)
		im.v[size_t(y0) * im.w + x] = ink;
}

struct Edge {
	int d = 0, x0 = 0, y0 = 0, xmin = 0, ymin = 0, xmax = 0, ymax = 0;
	float dx = 0;
};

void addEdge(Edge& e, int x0, int y0, int x1, int y1) {
	if (x0 <= x1)
		e.xmin = x0, e.xmax = x1;
	else
		e.xmin = x1, e.xmax = x0;
	if (y0 <= y1)
		e.ymin = y0, e.ymax = y1;
	else
		e.ymin = y1, e.ymax = y0;
	if (y0 == y1) {
		e.d = 0;
		e.dx = 0.0f;
	} else {
		e.dx = float(x1 - x0) / float(y1 - y0);
		e.d = y0 == e.ymin ? 1 : -1;
	}
	e.x0 = x0;
	e.y0 = y0;
}

inline int roundUpF(float f) {
	return int(f >= 0.0 ? std::floor(double(f + 0.5f)) : -std::floor(double(std::fabs(f) + 0.5f)));
}
inline int roundDownF(float f) {
	return int(f >= 0.0 ? std::ceil(double(f - 0.5f)) : -std::ceil(double(std::fabs(f) - 0.5f)));
}

// polygon_generic(..., eofill 0, hline8, hasAlpha 0)
void polygonGeneric(Img8& im, std::vector<Edge>& e, uint8_t ink) {
	const int n = int(e.size());
	if (n <= 0)
		return;
	std::vector<Edge*> table;
	int ymin = im.h - 1, ymax = 0;
	for (int i = 0; i < n; i++) {
		if (ymin > e[size_t(i)].ymin)
			ymin = e[size_t(i)].ymin;
		if (ymax < e[size_t(i)].ymax)
			ymax = e[size_t(i)].ymax;
		if (e[size_t(i)].ymin == e[size_t(i)].ymax) {
			hline8(im, e[size_t(i)].xmin, e[size_t(i)].ymin, e[size_t(i)].xmax, ink);
			continue;
		}
		table.push_back(&e[size_t(i)]);
	}
	if (ymin < 0)
		ymin = 0;
	if (ymax > im.h)
		ymax = im.h;
	const int count = int(table.size());
	std::vector<float> xx(size_t(count) * 2 + 1, 0.0f);
	for (; ymin <= ymax; ymin++) {
		int j = 0;
		for (int i = 0; i < count; i++) {
			Edge* cur = table[size_t(i)];
			if (ymin >= cur->ymin && ymin <= cur->ymax) {
				xx[size_t(j++)] = float(ymin - cur->y0) * cur->dx + float(cur->x0);
				if (ymin == cur->ymax && ymin < ymax) {
					xx[size_t(j)] = xx[size_t(j - 1)];
					j++;
				} else if (cur->dx != 0 && std::round(xx[size_t(j - 1)]) == xx[size_t(j - 1)]) {
					// Connect discontiguous corners (writes xx[k] by edge index, as Pillow does)
					for (int k = 0; k < i; k++) {
						Edge* other = table[size_t(k)];
						if ((cur->dx > 0 && other->dx <= 0) || (cur->dx < 0 && other->dx >= 0))
							continue;
						if (((ymin == cur->ymin && ymin == other->ymin) || (ymin == cur->ymax && ymin == other->ymax)) &&
							xx[size_t(j - 1)] == float(ymin - other->y0) * other->dx + float(other->x0)) {
							int offset = ymin == ymax ? -1 : 1;
							float adj = float(ymin + offset - cur->y0) * cur->dx + float(cur->x0);
							float adjOther = float(ymin + offset - other->y0) * other->dx + float(other->x0);
							if (ymin == cur->ymax) {
								if (cur->dx > 0)
									xx[size_t(k)] = float(std::fmax(adj, adjOther) + 1);
								else
									xx[size_t(k)] = float(std::fmin(adj, adjOther) - 1);
							} else {
								if (cur->dx > 0)
									xx[size_t(k)] = float(std::fmin(adj, adjOther));
								else
									xx[size_t(k)] = float(std::fmax(adj, adjOther) + 1);
							}
							break;
						}
					}
				}
			}
		}
		std::sort(xx.begin(), xx.begin() + j);
		for (int i = 1; i < j; i += 2)
			hline8(im, roundUpF(xx[size_t(i - 1)]), ymin, roundDownF(xx[size_t(i)]), ink);
	}
}

// ImagingDrawPolygon with fill
void drawPolygon(Img8& im, const std::vector<int>& xy, uint8_t ink) {
	const int count = int(xy.size() / 2);
	if (count <= 0)
		return;
	std::vector<Edge> e(static_cast<size_t>(count));
	int i, n;
	for (i = n = 0; i < count - 1; i++) {
		int x0 = xy[size_t(i) * 2], y0 = xy[size_t(i) * 2 + 1], x1 = xy[size_t(i) * 2 + 2], y1 = xy[size_t(i) * 2 + 3];
		if (y0 == y1 && i != 0 && y0 == xy[size_t(i) * 2 - 1]) {
			Edge& last = e[size_t(n - 1)];
			if (x1 > x0 && x0 > xy[size_t(i) * 2 - 2]) {
				last.xmax = x1;
				continue;
			} else if (x1 < x0 && x0 < xy[size_t(i) * 2 - 2]) {
				last.xmin = x1;
				continue;
			}
		}
		addEdge(e[size_t(n++)], x0, y0, x1, y1);
	}
	if (xy[size_t(i) * 2] != xy[0] || xy[size_t(i) * 2 + 1] != xy[1])
		addEdge(e[size_t(n++)], xy[size_t(i) * 2], xy[size_t(i) * 2 + 1], xy[0], xy[1]);
	e.resize(size_t(n));
	polygonGeneric(im, e, ink);
}

// --- ellipses (Draw.c quarter_* / ellipse_* / clip tree / pie) ---

struct Quarter {
	int32_t a = 0, b = 0, cx = 0, cy = 0, ex = 0, ey = 0;
	int64_t a2 = 0, b2 = 0, a2b2 = 0;
	bool finished = true;
	void init(int32_t a_, int32_t b_) {
		if (a_ < 0 || b_ < 0) {
			finished = true;
			return;
		}
		a = a_;
		b = b_;
		cx = a;
		cy = b % 2;
		ex = a % 2;
		ey = b;
		a2 = int64_t(a) * a; // (Pillow multiplies int32s; equal for the sizes drawn)
		b2 = int64_t(b) * b;
		a2b2 = a2 * b2;
		finished = false;
	}
	int64_t delta(int64_t x, int64_t y) const { return std::llabs(a2 * y * y + b2 * x * x - a2b2); }
	bool next(int32_t& rx, int32_t& ry) {
		if (finished)
			return false;
		rx = cx;
		ry = cy;
		if (cx == ex && cy == ey) {
			finished = true;
		} else {
			int32_t nx = cx, ny = cy + 2;
			int64_t nd = delta(nx, ny);
			if (nx > 1) {
				int64_t d2 = delta(cx - 2, cy + 2);
				if (nd > d2) {
					nx = cx - 2;
					ny = cy + 2;
					nd = d2;
				}
				d2 = delta(cx - 2, cy);
				if (nd > d2) {
					nx = cx - 2;
					ny = cy;
				}
			}
			cx = nx;
			cy = ny;
		}
		return true;
	}
};

struct EllipseState {
	Quarter o, in;
	int32_t py = 0, pl = 0, pr = 0;
	int32_t cy[4], cl[4], cr[4];
	int bufcnt = 0;
	bool finished = false;
	int32_t leftmost = 0;
	void init(int32_t a, int32_t b, int32_t w) {
		bufcnt = 0;
		leftmost = a % 2;
		o.init(a, b);
		if (w < 1 || !o.next(pr, py)) {
			finished = true;
		} else {
			finished = false;
			in.init(a - 2 * (w - 1), b - 2 * (w - 1));
			pl = leftmost;
		}
	}
	bool next(int32_t& rx0, int32_t& ry, int32_t& rx1) {
		if (bufcnt == 0) {
			if (finished)
				return false;
			int32_t y = py, l = pl, r = pr, cx = 0, cyv = 0;
			bool got;
			while ((got = o.next(cx, cyv)) && cyv <= y) {
			}
			if (!got)
				finished = true;
			else {
				pr = cx;
				py = cyv;
			}
			while ((got = in.next(cx, cyv)) && cyv <= y)
				l = cx;
			pl = !got ? leftmost : cx;
			if ((l > 0 || l < r) && y > 0) {
				cl[bufcnt] = l == 0 ? 2 : l;
				cy[bufcnt] = y;
				cr[bufcnt] = r;
				++bufcnt;
			}
			if (y > 0) {
				cl[bufcnt] = -r;
				cy[bufcnt] = y;
				cr[bufcnt] = -l;
				++bufcnt;
			}
			if (l > 0 || l < r) {
				cl[bufcnt] = l == 0 ? 2 : l;
				cy[bufcnt] = -y;
				cr[bufcnt] = r;
				++bufcnt;
			}
			cl[bufcnt] = -r;
			cy[bufcnt] = -y;
			cr[bufcnt] = -l;
			++bufcnt;
		}
		--bufcnt;
		rx0 = cl[bufcnt];
		ry = cy[bufcnt];
		rx1 = cr[bufcnt];
		return true;
	}
};

enum ClipType { CT_AND, CT_OR, CT_CLIP };
struct ClipNode {
	ClipType type = CT_CLIP;
	double a = 0, b = 0, c = 0;
	const ClipNode* l = nullptr;
	const ClipNode* r = nullptr;
};
struct ClipEvent {
	int32_t x;
	int type;
};

// clip_tree_do_clip: the clipped segment's ends, as ordered events
std::vector<ClipEvent> clipTree(const ClipNode* root, int32_t x0, int32_t y, int32_t x1) {
	if (!root)
		return {{x0, 1}, {x1, -1}};
	if (root->type == CT_CLIP) {
		const double eps = 1e-9;
		double A = root->a, B = root->b, C = root->c;
		if (std::fabs(A) < eps) {
			if (B * y + C < -eps) {
				x0 = 1;
				x1 = 0;
			}
		} else {
			double ix = -(B * y + C) / A;
			if (A * x0 + B * y + C < eps)
				x0 = int32_t(std::lround(std::fmax(x0, ix)));
			if (A * x1 + B * y + C < eps)
				x1 = int32_t(std::lround(std::fmin(x1, ix)));
		}
		if (x0 <= x1)
			return {{x0, 1}, {x1, -1}};
		return {};
	}
	std::vector<ClipEvent> l1 = clipTree(root->l, x0, y, x1), l2 = clipTree(root->r, x0, y, x1), ret;
	size_t i1 = 0, i2 = 0;
	int k1 = 0, k2 = 0;
	while (i1 < l1.size() || i2 < l2.size()) {
		ClipEvent t;
		if (i2 >= l2.size() || (i1 < l1.size() && (l1[i1].x < l2[i2].x || (l1[i1].x == l2[i2].x && l1[i1].type > l2[i2].type)))) {
			t = l1[i1++];
			k1 += t.type;
		} else {
			t = l2[i2++];
			k2 += t.type;
		}
		bool tailOpen = !ret.empty() && ret.back().type == 1;
		bool tailNoneOrClosed = ret.empty() || ret.back().type == -1;
		if ((root->type == CT_OR && ((t.type == 1 && tailNoneOrClosed) || (t.type == -1 && k1 == 0 && k2 == 0))) ||
			(root->type == CT_AND && ((t.type == 1 && tailNoneOrClosed && k1 > 0 && k2 > 0) || (t.type == -1 && tailOpen && (k1 == 0 || k2 == 0)))))
			ret.push_back(t);
	}
	return ret;
}

void normalizeAngles(float& al, float& ar) {
	if (ar - al >= 360) {
		al = 0;
		ar = 360;
	} else {
		al = float(std::fmod(al < 0 ? 360 - (std::fmod(-double(al), 360)) : double(al), 360));
		// (Draw.c subtracts the two floats in float, then fmods in double)
		ar = float(al + std::fmod(ar < al ? 360 - std::fmod(double(float(al - ar)), 360) : double(float(ar - al)), 360));
	}
}

// ellipseNew with fill
void ellipseFill(Img8& im, int x0, int y0, int x1, int y1, uint8_t ink) {
	int a = x1 - x0, b = y1 - y0;
	if (a < 0 || b < 0)
		return;
	EllipseState st;
	st.init(a, b, a + b);
	int32_t X0, Y, X1;
	while (st.next(X0, Y, X1))
		hline8(im, x0 + (X0 + a) / 2, y0 + (Y + b) / 2, x0 + (X1 + a) / 2, ink);
}

// pieNew (pie_init) with width a + b: a filled pie slice
void pieFill(Img8& im, int x0, int y0, int x1, int y1, float al, float ar, uint8_t ink) {
	int a = x1 - x0, b = y1 - y0;
	if (a < 0 || b < 0)
		return;
	EllipseState st;
	st.init(a, b, x1 + y1 - x0 - y0);
	double xl = a * std::cos(al * kPi / 180.0), xr = a * std::cos(ar * kPi / 180.0);
	double yl = b * std::sin(al * kPi / 180.0), yr = b * std::sin(ar * kPi / 180.0);
	ClipNode lc, rc, root, spike, root2;
	lc.type = rc.type = CT_CLIP;
	lc.a = -yl;
	lc.b = xl;
	lc.c = 0;
	rc.a = yr;
	rc.b = -xr;
	rc.c = 0;
	root.l = &lc;
	root.r = &rc;
	root.type = ar - al < 180 ? CT_AND : CT_OR;
	const ClipNode* top = &root;
	if (ar - al < 90) {
		spike.type = CT_CLIP;
		spike.a = (xl + xr) / 2.0;
		spike.b = (yl + yr) / 2.0;
		spike.c = 0;
		root2.type = CT_AND;
		root2.l = &root;
		root2.r = &spike;
		top = &root2;
	}
	int32_t X0, Y, X1;
	while (st.next(X0, Y, X1)) {
		std::vector<ClipEvent> ev = clipTree(top, X0, Y, X1);
		for (size_t i = 0; i + 1 < ev.size(); i += 2)
			hline8(im, x0 + (ev[i].x + a) / 2, y0 + (Y + b) / 2, x0 + (ev[i + 1].x + a) / 2, ink);
	}
}

// ImagingDrawRectangle with fill
void rectFill(Img8& im, int x0, int y0, int x1, int y1, uint8_t ink) {
	if (y0 > y1)
		std::swap(y0, y1);
	if (y0 < 0)
		y0 = 0;
	else if (y0 >= im.h)
		return;
	if (y1 < 0)
		return;
	else if (y1 > im.h)
		y1 = im.h;
	for (int y = y0; y <= y1; y++)
		hline8(im, x0, y, x1, ink);
}

void needL(const Img8& im) {
	if (im.c != 1)
		throw std::invalid_argument("npcore ImageDraw: only mode L is ported");
}

} // namespace

void polygon(Img8& im, const std::vector<std::pair<double, double>>& pts, uint8_t ink) {
	needL(im);
	if (pts.size() < 2)
		throw std::invalid_argument("npcore polygon: at least 2 points");
	std::vector<int> xy;
	for (auto& p : pts) {
		xy.push_back(int(p.first)); // _draw_polygon: (int) truncation
		xy.push_back(int(p.second));
	}
	drawPolygon(im, xy, ink);
}

void rectangle(Img8& im, int x0, int y0, int x1, int y1, uint8_t ink) {
	needL(im);
	if (x1 < x0 || y1 < y0)
		throw std::invalid_argument("npcore rectangle: corners out of order");
	rectFill(im, x0, y0, x1, y1, ink);
}

void ellipse(Img8& im, int x0, int y0, int x1, int y1, uint8_t ink) {
	needL(im);
	if (x1 < x0 || y1 < y0)
		throw std::invalid_argument("npcore ellipse: corners out of order");
	ellipseFill(im, x0, y0, x1, y1, ink);
}

void pieslice(Img8& im, int x0, int y0, int x1, int y1, float start, float end, uint8_t ink) {
	needL(im);
	normalizeAngles(start, end);
	if (start + 360 == end) {
		ellipseFill(im, x0, y0, x1, y1, ink);
		return;
	}
	if (start == end)
		return;
	pieFill(im, x0, y0, x1, y1, start, end, ink);
}

void roundedRectangle(Img8& im, int x0, int y0, int x1, int y1, int radius, uint8_t ink) {
	needL(im);
	if (x1 < x0 || y1 < y0)
		throw std::invalid_argument("npcore rounded_rectangle: corners out of order");
	int d = radius * 2;
	bool fullX = d >= x1 - x0 - 1;
	if (fullX)
		d = x1 - x0;
	bool fullY = d >= y1 - y0 - 1;
	if (fullY)
		d = y1 - y0;
	if (fullX && fullY) {
		ellipseFill(im, x0, y0, x1, y1, ink);
		return;
	}
	if (d == 0) {
		rectFill(im, x0, y0, x1, y1, ink);
		return;
	}
	int r = d / 2;
	if (fullX) {
		pieslice(im, x0, y0, x0 + d, y0 + d, 180, 360, ink);
		pieslice(im, x0, y1 - d, x0 + d, y1, 0, 180, ink);
	} else if (fullY) {
		pieslice(im, x0, y0, x0 + d, y0 + d, 90, 270, ink);
		pieslice(im, x1 - d, y0, x1, y0 + d, 270, 90, ink);
	} else {
		pieslice(im, x0, y0, x0 + d, y0 + d, 180, 270, ink);
		pieslice(im, x1 - d, y0, x1, y0 + d, 270, 360, ink);
		pieslice(im, x1 - d, y1 - d, x1, y1, 0, 90, ink);
		pieslice(im, x0, y1 - d, x0 + d, y1, 90, 180, ink);
	}
	if (fullX)
		rectFill(im, x0, y0 + r + 1, x1, y1 - r - 1, ink);
	else
		rectFill(im, x0 + r + 1, y0, x1 - r - 1, y1, ink);
	if (!fullX && !fullY) {
		rectFill(im, x0, y0 + r + 1, x0 + r, y1 - r - 1, ink);
		rectFill(im, x1 - r, y0 + r + 1, x1, y1 - r - 1, ink);
	}
}

// ===========================================================================
// numpy reductions

namespace {

// numpy's @TYPE@_pairwise_sum (loops_utils.h.src), stride 1
template <typename T>
T pairwise(const T* a, size_t n) {
	if (n < 8) {
		T res = T(-0.0);
		for (size_t i = 0; i < n; i++)
			res += a[i];
		return res;
	} else if (n <= 128) {
		T r[8];
		for (int j = 0; j < 8; j++)
			r[j] = a[j];
		size_t i;
		for (i = 8; i < n - (n % 8); i += 8)
			for (int j = 0; j < 8; j++)
				r[j] += a[i + size_t(j)];
		T res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
		for (; i < n; i++)
			res += a[i];
		return res;
	} else {
		size_t n2 = n / 2;
		n2 -= n2 % 8;
		return pairwise(a, n2) + pairwise(a + n2, n - n2);
	}
}

template <typename T>
T sumT(const T* a, size_t n) {
	// numpy's buffered reduction: the pairwise loop sees 8192 values at a time
	T acc = T(0);
	for (size_t i = 0; i < n; i += 8192)
		acc += pairwise(a + i, std::min<size_t>(8192, n - i));
	return acc;
}

template <typename T>
Img<T> sumLastT(const Img<T>& a) {
	Img<T> o(a.h, a.w, 1);
	for (size_t i = 0, m = size_t(a.h) * a.w; i < m; i++)
		o.v[i] = T(0) + pairwise(&a.v[i * a.c], size_t(a.c));
	return o;
}

template <typename T>
Img<T> meanLastT(const Img<T>& a) {
	Img<T> o = sumLastT(a);
	for (auto& v : o.v)
		v = v / T(a.c);
	return o;
}

template <typename T>
std::vector<T> sumAxis0T(const T* rows, size_t n, int c) {
	std::vector<T> acc(size_t(c), T(0));
	for (size_t i = 0; i < n; i++)
		for (int k = 0; k < c; k++)
			acc[size_t(k)] += rows[i * c + k];
	return acc;
}

template <typename T>
std::vector<T> meanAxis0T(const T* rows, size_t n, int c) {
	std::vector<T> acc = sumAxis0T(rows, n, c);
	for (auto& v : acc)
		v = v / T(n);
	return acc;
}

template <typename T>
Img<T> blockMeanT(const Img<T>& a, int ss) {
	const int h = a.h / ss, w = a.w / ss, C = a.c;
	Img<T> o(h, w, C);
	const T div = T(ss * ss);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			if (C == 1) {
				// (h, ss, w, ss): the inner ss along a row added pairwise, then the rows
				T acc = T(0);
				for (int i = 0; i < ss; i++)
					acc += pairwise(&a.v[size_t(y * ss + i) * a.w + size_t(x) * ss], size_t(ss));
				o(y, x) = acc / div;
			} else {
				for (int k = 0; k < C; k++) {
					T acc = T(0);
					for (int i = 0; i < ss; i++)
						for (int j = 0; j < ss; j++)
							acc += a(y * ss + i, x * ss + j, k);
					o(y, x, k) = acc / div;
				}
			}
		}
	return o;
}

} // namespace

float sum(const float* a, size_t n) { return sumT(a, n); }
double sum(const double* a, size_t n) { return sumT(a, n); }

float meanFlat(const float* a, size_t n) {
	return float(double(sumT(a, n)) / double(n));
}
double meanFlat(const double* a, size_t n) {
	return sumT(a, n) / double(n);
}

template <typename T>
static T stdT(const T* a, size_t n) {
	// _var: the mean kept as a 1-element array (divided in T), the squared
	// deviations summed, divided as numpy's scalar path does, sqrt in T
	T m = sumT(a, n) / T(n);
	std::vector<T> x(n);
	for (size_t i = 0; i < n; i++) {
		x[i] = a[i] - m;
		x[i] = x[i] * x[i];
	}
	T s = sumT(x.data(), n);
	T var = T(double(s) / double(n));
	return std::sqrt(var);
}
float stdFlat(const float* a, size_t n) { return stdT(a, n); }
double stdFlat(const double* a, size_t n) { return stdT(a, n); }

ImgF sumLast(const ImgF& a) { return sumLastT(a); }
ImgD sumLast(const ImgD& a) { return sumLastT(a); }
ImgF meanLast(const ImgF& a) { return meanLastT(a); }
ImgD meanLast(const ImgD& a) { return meanLastT(a); }
std::vector<float> meanAxis0(const float* rows, size_t n, int c) { return meanAxis0T(rows, n, c); }
std::vector<double> meanAxis0(const double* rows, size_t n, int c) { return meanAxis0T(rows, n, c); }
std::vector<float> sumAxis0(const float* rows, size_t n, int c) { return sumAxis0T(rows, n, c); }
std::vector<double> sumAxis0(const double* rows, size_t n, int c) { return sumAxis0T(rows, n, c); }
ImgF blockMean(const ImgF& a, int ss) { return blockMeanT(a, ss); }
ImgD blockMean(const ImgD& a, int ss) { return blockMeanT(a, ss); }

// ===========================================================================
// numpy functions

template <typename T>
static T medianT(std::vector<T> lane, bool scalar) {
	const size_t n = lane.size();
	if (!n)
		return T(NAN);
	if (n % 2) {
		std::nth_element(lane.begin(), lane.begin() + n / 2, lane.end());
		return T(0) + (T(-0.0) + lane[n / 2]); // mean of one: 0 + pairwise
	}
	std::nth_element(lane.begin(), lane.begin() + n / 2, lane.end());
	T b = lane[n / 2];
	T a = *std::max_element(lane.begin(), lane.begin() + n / 2);
	T s = T(0) + ((T(-0.0) + a) + b);
	return scalar ? T(double(s) / 2.0) : s / T(2);
}
float median(std::vector<float> lane, bool scalar) { return medianT(std::move(lane), scalar); }
double median(std::vector<double> lane, bool scalar) { return medianT(std::move(lane), scalar); }

template <typename T>
static double percentileT(std::vector<T> lane, double q) {
	const int64_t n = int64_t(lane.size());
	if (!n)
		return NAN;
	std::sort(lane.begin(), lane.end());
	double quant = q / 100.0; // np.true_divide(q, 100)
	double virt = double(n - 1) * quant; // linear: (n - 1) * quantiles
	double prevF = std::floor(virt);
	int64_t prev = int64_t(prevF), next = prev + 1;
	if (virt >= double(n - 1))
		prev = next = n - 1; // (-1: the last)
	if (virt < 0)
		prev = next = 0;
	T a = lane[size_t(prev)], b = lane[size_t(next)];
	double gamma = virt - double(virt >= double(n - 1) ? -1 : virt < 0 ? 0 : prev);
	T diff = T(b - a); // subtract(b, a) in the array's type
	if (gamma >= 0.5)
		return double(b) - double(diff) * (1 - gamma);
	return double(a) + double(diff) * gamma;
}
double percentile(std::vector<float> lane, double q) { return percentileT(std::move(lane), q); }
double percentile(std::vector<double> lane, double q) { return percentileT(std::move(lane), q); }

// binary_search_with_guess's answer (compiled_base.c): -1 below, len above
// xp[len-1], else the last j with xp[j] <= x
static int64_t interpIndex(double key, const std::vector<double>& xp) {
	const int64_t len = int64_t(xp.size());
	if (key > xp[size_t(len - 1)])
		return len;
	if (key < xp[0])
		return -1;
	return int64_t(std::upper_bound(xp.begin(), xp.end(), key) - xp.begin()) - 1;
}

double interp(double x, const std::vector<double>& xp, const std::vector<double>& fp) {
	const int64_t lenxp = int64_t(xp.size());
	if (lenxp == 0 || fp.size() != xp.size())
		throw std::invalid_argument("npcore interp: xp and fp must be the same, nonempty length");
	const double lval = fp[0], rval = fp[size_t(lenxp - 1)];
	if (lenxp == 1)
		return x < xp[0] ? lval : x > xp[0] ? rval : fp[0];
	if (std::isnan(x))
		return x;
	int64_t j = interpIndex(x, xp);
	if (j == -1)
		return lval;
	if (j == lenxp)
		return rval;
	if (j == lenxp - 1)
		return fp[size_t(j)];
	if (xp[size_t(j)] == x)
		return fp[size_t(j)];
	double slope = (fp[size_t(j + 1)] - fp[size_t(j)]) / (xp[size_t(j + 1)] - xp[size_t(j)]);
	double r = slope * (x - xp[size_t(j)]) + fp[size_t(j)];
	if (std::isnan(r)) {
		r = slope * (x - xp[size_t(j + 1)]) + fp[size_t(j + 1)];
		if (std::isnan(r) && fp[size_t(j)] == fp[size_t(j + 1)])
			r = fp[size_t(j)];
	}
	return r;
}

std::vector<double> interp(const std::vector<double>& x, const std::vector<double>& xp, const std::vector<double>& fp) {
	std::vector<double> o(x.size());
	for (size_t i = 0; i < x.size(); i++)
		o[i] = interp(x[i], xp, fp);
	return o;
}

std::vector<double> linspace(double start, double stop, int n) {
	std::vector<double> y(size_t(std::max(n, 0)));
	if (n <= 0)
		return y;
	const int div = n - 1;
	const double delta = stop - start;
	if (div > 0) {
		double step = delta / div;
		for (int i = 0; i < n; i++)
			y[size_t(i)] = step == 0 ? double(i) / div * delta : double(i) * step;
	} else {
		y[0] = 0.0 * delta;
	}
	for (auto& v : y)
		v += start;
	if (n > 1)
		y[size_t(n - 1)] = stop;
	return y;
}

std::vector<double> convolve(const std::vector<double>& a0, const std::vector<double>& v0, const std::string& mode) {
	// np.convolve: correlate(a, v[::-1]) (the longer one first), each
	// output a dot product added in order from 0 (reference BLAS ddot)
	const std::vector<double>* pa = &a0;
	const std::vector<double>* pv = &v0;
	if (pv->size() > pa->size())
		std::swap(pa, pv);
	if (pa->empty() || pv->empty())
		throw std::invalid_argument("npcore convolve: empty input");
	std::vector<double> ap2(pv->rbegin(), pv->rend());
	const std::vector<double>& ap1 = *pa;
	const int64_t n1 = int64_t(ap1.size()), n2 = int64_t(ap2.size());
	int64_t length = n1, n = n2, nLeft, nRight;
	if (mode == "valid") {
		length = length - n + 1;
		nLeft = nRight = 0;
	} else if (mode == "same") {
		nLeft = n / 2;
		nRight = n - nLeft - 1;
	} else if (mode == "full") {
		nRight = n - 1;
		nLeft = n - 1;
		length = length + n - 1;
	} else
		throw std::invalid_argument("npcore convolve: mode must be valid, same or full");
	std::vector<double> out;
	out.reserve(size_t(length));
	auto dot = [](const double* p1, const double* p2, int64_t m) {
		double s = 0.0, t = 0.0;
		for (int64_t i = 0; i < m; i++)
			t = t + p1[i] * p2[i];
		return s + t;
	};
	const double* ip1 = ap1.data();
	const double* ip2 = ap2.data() + nLeft;
	n = n - nLeft;
	for (int64_t i = 0; i < nLeft; i++) {
		out.push_back(dot(ip1, ip2, n));
		n++;
		ip2--;
	}
	for (int64_t i = 0; i < n1 - n2 + 1; i++) {
		out.push_back(dot(ip1, ip2, n));
		ip1++;
	}
	for (int64_t i = 0; i < nRight; i++) {
		n--;
		out.push_back(dot(ip1, ip2, n));
		ip1++;
	}
	return out;
}

void gradient(const ImgF& f, ImgF& gy, ImgF& gx) {
	if (f.c != 1 || f.h < 2 || f.w < 2)
		throw std::invalid_argument("npcore gradient: a 2-D plane of at least 2 x 2");
	gy = ImgF(f.h, f.w);
	gx = ImgF(f.h, f.w);
	for (int y = 0; y < f.h; y++)
		for (int x = 0; x < f.w; x++) {
			if (y == 0)
				gy(y, x) = (f(1, x) - f(0, x)) / 1.0f;
			else if (y == f.h - 1)
				gy(y, x) = (f(y, x) - f(y - 1, x)) / 1.0f;
			else
				gy(y, x) = (f(y + 1, x) - f(y - 1, x)) / 2.0f;
			if (x == 0)
				gx(y, x) = (f(y, 1) - f(y, 0)) / 1.0f;
			else if (x == f.w - 1)
				gx(y, x) = (f(y, x) - f(y, x - 1)) / 1.0f;
			else
				gx(y, x) = (f(y, x + 1) - f(y, x - 1)) / 2.0f;
		}
}

// --- tanh: numpy's simd_tanh_f64 / simd_tanh_f32 (from SVML), one lane ---

namespace {

// simd_tanh_f64's table (18 rows of 16: b, c0..c16; verbatim)
const uint64_t lut16x18[] = {
        // 0
        0x0ull,                0x3fcc000000000000ull, 0x3fd4000000000000ull, 0x3fdc000000000000ull,
        0x3fe4000000000000ull, 0x3fec000000000000ull, 0x3ff4000000000000ull, 0x3ffc000000000000ull,
        0x4004000000000000ull, 0x400c000000000000ull, 0x4014000000000000ull, 0x401c000000000000ull,
        0x4024000000000000ull, 0x402c000000000000ull, 0x4034000000000000ull, 0x0ull,
        // 1
        0x0ull,                0x3fcb8fd0416a7c92ull, 0x3fd35f98a0ea650eull, 0x3fda5729ee488037ull,
        0x3fe1bf47eabb8f95ull, 0x3fe686650b8c2015ull, 0x3feb2523bb6b2deeull, 0x3fee1fbf97e33527ull,
        0x3fef9258260a71c2ull, 0x3feff112c63a9077ull, 0x3fefff419668df11ull, 0x3feffffc832750f2ull,
        0x3feffffffdc96f35ull, 0x3fefffffffffcf58ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull,
        // 2
        0x3ff0000000000000ull, 0x3fee842ca3f08532ull, 0x3fed11574af58f1bull, 0x3fea945b9c24e4f9ull,
        0x3fe6284c3374f815ull, 0x3fe02500a09f8d6eull, 0x3fd1f25131e3a8c0ull, 0x3fbd22ca1c24a139ull,
        0x3f9b3afe1fba5c76ull, 0x3f6dd37d19b22b21ull, 0x3f27ccec13a9ef96ull, 0x3ecbe6c3f33250aeull,
        0x3e41b4865394f75full, 0x3d8853f01bda5f28ull, 0x3c73953c0197ef58ull, 0x0ull,
        // 3
        0xbbf0b3ea3fdfaa19ull, 0xbfca48aaeb53bc21ull, 0xbfd19921f4329916ull, 0xbfd5e0f09bef8011ull,
        0xbfd893b59c35c882ull, 0xbfd6ba7cb7576538ull, 0xbfce7291743d7555ull, 0xbfbb6d85a01efb80ull,
        0xbf9addae58c7141aull, 0xbf6dc59376c7aa19ull, 0xbf27cc5e74677410ull, 0xbecbe6c0e8b4cc87ull,
        0xbe41b486526b0565ull, 0xbd8853f01bef63a4ull, 0xbc73955be519be31ull, 0x0ull,
        // 4
        0xbfd5555555555555ull, 0xbfd183afc292ba11ull, 0xbfcc1a4b039c9bfaull, 0xbfc16e1e6d8d0be6ull,
        0xbf92426c751e48a2ull, 0x3fb4f152b2bad124ull, 0x3fbbba40cbef72beull, 0x3fb01ba038be6a3dull,
        0x3f916df44871efc8ull, 0x3f63c6869dfc8870ull, 0x3f1fb9aef915d828ull, 0x3ec299d1e27c6e11ull,
        0x3e379b5ddcca334cull, 0x3d8037f57bc62c9aull, 0x3c6a2d4b50a2cff7ull, 0x0ull,
        // 5
        0xbce6863ee44ed636ull, 0x3fc04dcd0476c75eull, 0x3fc43d3449a80f08ull, 0x3fc5c26f3699b7e7ull,
        0x3fc1a686f6ab2533ull, 0x3faf203c316ce730ull, 0xbf89c7a02788557cull, 0xbf98157e26e0d541ull,
        0xbf807b55c1c7d278ull, 0xbf53a18d5843190full, 0xbf0fb6bbc89b1a5bull, 0xbeb299c9c684a963ull,
        0xbe279b5dd4fb3d01ull, 0xbd7037f57ae72aa6ull, 0xbc5a2ca2bba78e86ull, 0x0ull,
        // 6
        0x3fc1111111112ab5ull, 0x3fb5c19efdfc08adull, 0x3fa74c98dc34fbacull, 0xbf790d6a8eff0a77ull,
        0xbfac3c021789a786ull, 0xbfae2196b7326859ull, 0xbf93a7a011ff8c2aull, 0x3f6e4709c7e8430eull,
        0x3f67682afa611151ull, 0x3f3ef2ee77717cbfull, 0x3ef95a4482f180b7ull, 0x3e9dc2c27da3b603ull,
        0x3e12e2afd9f7433eull, 0x3d59f320348679baull, 0x3c44b61d9bbcc940ull, 0x0ull,
        // 7
        0xbda1ea19ddddb3b4ull, 0xbfb0b8df995ce4dfull, 0xbfb2955cf41e8164ull, 0xbfaf9d05c309f7c6ull,
        0xbf987d27ccff4291ull, 0x3f8b2ca62572b098ull, 0x3f8f1cf6c7f5b00aull, 0x3f60379811e43dd5ull,
        0xbf4793826f78537eull, 0xbf2405695e36240full, 0xbee0e08de39ce756ull, 0xbe83d709ba5f714eull,
        0xbdf92e3fc5ee63e0ull, 0xbd414cc030f2110eull, 0xbc2ba022e8d82a87ull, 0x0ull,
        // 8
        0xbfaba1ba1990520bull, 0xbf96e37bba52f6fcull, 0x3ecff7df18455399ull, 0x3f97362834d33a4eull,
        0x3f9e7f8380184b45ull, 0x3f869543e7c420d4ull, 0xbf7326bd4914222aull, 0xbf5fc15b0a9d98faull,
        0x3f14cffcfa69fbb6ull, 0x3f057e48e5b79d10ull, 0x3ec33b66d7d77264ull, 0x3e66ac4e578b9b10ull,
        0x3ddcc74b8d3d5c42ull, 0x3d23c589137f92b4ull, 0x3c107f8e2c8707a1ull, 0x0ull,
        // 9
        0xbe351ca7f096011full, 0x3f9eaaf3320c3851ull, 0x3f9cf823fe761fc1ull, 0x3f9022271754ff1full,
        0xbf731fe77c9c60afull, 0xbf84a6046865ec7dull, 0xbf4ca3f1f2b9192bull, 0x3f4c77dee0afd227ull,
        0x3f04055bce68597aull, 0xbee2bf0cb4a71647ull, 0xbea31eaafe73efd5ull, 0xbe46abb02c4368edull,
        0xbdbcc749ca8079ddull, 0xbd03c5883836b9d2ull, 0xbbf07a5416264aecull, 0x0ull,
        // 10
        0x3f9664f94e6ac14eull, 0xbf94d3343bae39ddull, 0xbf7bc748e60df843ull, 0xbf8c89372b43ba85ull,
        0xbf8129a092de747aull, 0x3f60c85b4d538746ull, 0x3f5be9392199ec18ull, 0xbf2a0c68a4489f10ull,
        0xbf00462601dc2faaull, 0x3eb7b6a219dea9f4ull, 0x3e80cbcc8d4c5c8aull, 0x3e2425bb231a5e29ull,
        0x3d9992a4beac8662ull, 0x3ce191ba5ed3fb67ull, 0x3bc892450bad44c4ull, 0x0ull,
        // 11
        0xbea8c4c1fd7852feull, 0xbfccce16b1046f13ull, 0xbf81a16f224bb7b6ull, 0xbf62cbf00406bc09ull,
        0x3f75b29bb02cf69bull, 0x3f607df0f9f90c17ull, 0xbf4b852a6e0758d5ull, 0xbf0078c63d1b8445ull,
        0x3eec12eadd55be7aull, 0xbe6fa600f593181bull, 0xbe5a3c935dce3f7dull, 0xbe001c6d95e3ae96ull,
        0xbd74755a00ea1fd3ull, 0xbcbc1c6c063bb7acull, 0xbba3be9a4460fe00ull, 0x0ull,
        // 12
        0xbf822404577aa9ddull, 0x403d8b07f7a82aa3ull, 0xbf9f44ab92fbab0aull, 0x3fb2eac604473d6aull,
        0x3f45f87d903aaac8ull, 0xbf5e104671036300ull, 0x3f19bc98ddf0f340ull, 0x3f0d4304bc9246e8ull,
        0xbed13c415f7b9d41ull, 0xbe722b8d9720cdb0ull, 0x3e322666d739bec0ull, 0x3dd76a553d7e7918ull,
        0x3d4de0fa59416a39ull, 0x3c948716cf3681b4ull, 0x3b873f9f2d2fda99ull, 0x0ull,
        // 13
        0xbefdd99a221ed573ull, 0x4070593a3735bab4ull, 0xbfccab654e44835eull, 0x3fd13ed80037dbacull,
        0xbf6045b9076cc487ull, 0x3f2085ee7e8ac170ull, 0x3f23524622610430ull, 0xbeff12a6626911b4ull,
        0x3eab9008bca408afull, 0x3e634df71865f620ull, 0xbe05bb1bcf83ca73ull, 0xbdaf2ac143fb6762ull,
        0xbd23eae52a3dbf57ull, 0xbc6b5e3e9ca0955eull, 0xbb5eca68e2c1ba2eull, 0x0ull,
        // 14
        0x3f6e3be689423841ull, 0xc0d263511f5baac1ull, 0x40169f73b15ebe5cull, 0xc025c1dd41cd6cb5ull,
        0xbf58fd89fe05e0d1ull, 0x3f73f7af01d5af7aull, 0xbf1e40bdead17e6bull, 0x3ee224cd6c4513e5ull,
        0xbe24b645e68eeaa3ull, 0xbe4abfebfb72bc83ull, 0x3dd51c38f8695ed3ull, 0x3d8313ac38c6832bull,
        0x3cf7787935626685ull, 0x3c401ffc49c6bc29ull, 0xbabf0b21acfa52abull, 0x0ull,
        // 15
        0xbf2a1306713a4f3aull, 0xc1045e509116b066ull, 0x4041fab9250984ceull, 0xc0458d090ec3de95ull,
        0xbf74949d60113d63ull, 0x3f7c9fd6200d0adeull, 0x3f02cd40e0ad0a9full, 0xbe858ab8e019f311ull,
        0xbe792fa6323b7cf8ull, 0x3e2df04d67876402ull, 0xbd95c72be95e4d2cull, 0xbd55a89c30203106ull,
        0xbccad6b3bb9eff65ull, 0xbc12705ccd3dd884ull, 0xba8e0a4c47ae75f5ull, 0x0ull,
        // 16
        0xbf55d7e76dc56871ull, 0x41528c38809c90c7ull, 0xc076d57fb5190b02ull, 0x4085f09f888f8adaull,
        0x3fa246332a2fcba5ull, 0xbfb29d851a896fcdull, 0x3ed9065ae369b212ull, 0xbeb8e1ba4c98a030ull,
        0x3e6ffd0766ad4016ull, 0xbe0c63c29f505f5bull, 0xbd7fab216b9e0e49ull, 0x3d2826b62056aa27ull,
        0x3ca313e31762f523ull, 0x3bea37aa21895319ull, 0x3ae5c7f1fd871496ull, 0x0ull,
        // 17
        0x3f35e67ab76a26e7ull, 0x41848ee0627d8206ull, 0xc0a216d618b489ecull, 0x40a5b89107c8af4full,
        0x3fb69d8374520edaull, 0xbfbded519f981716ull, 0xbef02d288b5b3371ull, 0x3eb290981209c1a6ull,
        0xbe567e924bf5ff6eull, 0x3de3f7f7de6b0eb6ull, 0x3d69ed18bae3ebbcull, 0xbcf7534c4f3dfa71ull,
        0xbc730b73f1eaff20ull, 0xbbba2cff8135d462ull, 0xbab5a71b5f7d9035ull, 0x0ull
    };

// simd_tanh_f32's table (8 rows of 32: b, c0..c6; verbatim)
const uint32_t lut32x8[] = {
        // 0
        0x0,        0x3d700000, 0x3d900000, 0x3db00000, 0x3dd00000, 0x3df00000, 0x3e100000, 0x3e300000,
        0x3e500000, 0x3e700000, 0x3e900000, 0x3eb00000, 0x3ed00000, 0x3ef00000, 0x3f100000, 0x3f300000,
        0x3f500000, 0x3f700000, 0x3f900000, 0x3fb00000, 0x3fd00000, 0x3ff00000, 0x40100000, 0x40300000,
        0x40500000, 0x40700000, 0x40900000, 0x40b00000, 0x40d00000, 0x40f00000, 0x41100000, 0x0,
        // 1
        0x0,        0x3d6fb9c9, 0x3d8fc35f, 0x3daf9169, 0x3dcf49ab, 0x3deee849, 0x3e0f0ee8, 0x3e2e4984,
        0x3e4d2f8e, 0x3e6bb32e, 0x3e8c51cd, 0x3ea96163, 0x3ec543f1, 0x3edfd735, 0x3f028438, 0x3f18abf0,
        0x3f2bc480, 0x3f3bec1c, 0x3f4f2e5b, 0x3f613c53, 0x3f6ce37d, 0x3f743c4f, 0x3f7a5feb, 0x3f7dea85,
        0x3f7f3b3d, 0x3f7fb78c, 0x3f7fefd4, 0x3f7ffdd0, 0x3f7fffb4, 0x3f7ffff6, 0x3f7fffff, 0x3f800000,
        // 2
        0x3f800000, 0x3f7f1f84, 0x3f7ebd11, 0x3f7e1e5f, 0x3f7d609f, 0x3f7c842d, 0x3f7b00e5, 0x3f789580,
        0x3f75b8ad, 0x3f726fd9, 0x3f6cc59b, 0x3f63fb92, 0x3f59ff97, 0x3f4f11d7, 0x3f3d7573, 0x3f24f360,
        0x3f0cbfe7, 0x3eec1a69, 0x3eb0a801, 0x3e6753a2, 0x3e132f1a, 0x3db7e7d3, 0x3d320845, 0x3c84d3d4,
        0x3bc477b7, 0x3b10d3da, 0x3a01601e, 0x388c1a3b, 0x3717b0da, 0x35a43bce, 0x338306c6, 0x0,
        // 3
        0xb0343c7b, 0xbd6ee69d, 0xbd8f0da7, 0xbdae477d, 0xbdcd2a1f, 0xbdeba80d, 0xbe0c443b, 0xbe293cf3,
        0xbe44f282, 0xbe5f3651, 0xbe81c7c0, 0xbe96d7ca, 0xbea7fb8e, 0xbeb50e9e, 0xbec12efe, 0xbec4be92,
        0xbebce070, 0xbead510e, 0xbe8ef7d6, 0xbe4b8704, 0xbe083237, 0xbdaf7449, 0xbd2e1ec4, 0xbc83bf06,
        0xbbc3e0b5, 0xbb10aadc, 0xba0157db, 0xb88c18f2, 0xb717b096, 0xb5a43bae, 0xb383012c, 0x0,
        // 4
        0xbeaaaaa5, 0xbeab0612, 0xbea7f01f, 0xbea4e120, 0xbea387b7, 0xbea15962, 0xbe9d57f7, 0xbe976b5a,
        0xbe90230d, 0xbe880dff, 0xbe7479b3, 0xbe4c3d88, 0xbe212482, 0xbdeb8cba, 0xbd5e78ad, 0x3c6b5e6e,
        0x3d839143, 0x3dc21ee1, 0x3de347af, 0x3dcbec96, 0x3d99ef2d, 0x3d542ea1, 0x3cdde701, 0x3c2cca67,
        0x3b81cb27, 0x3ac073a1, 0x39ac3032, 0x383a94d9, 0x36ca081d, 0x355abd4c, 0x332b3cb6, 0x0,
        // 5
        0xb76dd6b9, 0xbe1c276d, 0x3c1dcf2f, 0x3dc1a78d, 0x3d96f985, 0x3da2b61b, 0x3dc13397, 0x3dd2f670,
        0x3df48a0a, 0x3e06c5a8, 0x3e1a3aba, 0x3e27c405, 0x3e2e78d0, 0x3e2c3e44, 0x3e1d3097, 0x3df4a8f4,
        0x3da38508, 0x3d31416a, 0x3b562657, 0xbcaeeac9, 0xbcce9419, 0xbcaaeac4, 0xbc49e7d0, 0xbba71ddd,
        0xbb003b0e, 0xba3f9a05, 0xb92c08a7, 0xb7ba9232, 0xb64a0b0f, 0xb4dac169, 0xb2ab78ac, 0x0,
        // 6
        0x3e0910e9, 0x43761143, 0x4165ecdc, 0xc190f756, 0xc08c097d, 0xc02ba813, 0xbf7f6bda, 0x3f2b1dc0,
        0x3ece105d, 0x3f426a94, 0xbadb0dc4, 0x3da43b17, 0xbd51ab88, 0xbcaea23d, 0xbd3b6d8d, 0xbd6caaad,
        0xbd795bed, 0xbd5fddda, 0xbd038f3b, 0xbc1cad63, 0x3abb4766, 0x3b95f10b, 0x3b825873, 0x3afaea66,
        0x3a49f878, 0x39996bf3, 0x388f3e6c, 0x371bb0e3, 0x35a8a5e6, 0x34369b17, 0x322487b0, 0x0,
        // 7
        0xbc0e2f66, 0x460bda12, 0x43d638ef, 0xc3e11c3e, 0xc2baa4e9, 0xc249da2d, 0xc1859b82, 0x40dd5b57,
        0x40494640, 0x40c730a8, 0xbf0f160e, 0x3e30e76f, 0xbea81387, 0xbdb26a1c, 0xbd351e57, 0xbb4c01a0,
        0x3c1d7bfb, 0x3c722cd1, 0x3c973f1c, 0x3c33a31b, 0x3b862ef4, 0x3a27b3d0, 0xba3b5907, 0xba0efc22,
        0xb97f9f0f, 0xb8c8af50, 0xb7bdddfb, 0xb64f2950, 0xb4e085b1, 0xb3731dfa, 0xb15a1f04, 0x0
    };

template <typename T, typename U>
T bitsTo(U u) {
	T t;
	std::memcpy(&t, &u, sizeof t);
	return t;
}

} // namespace

double tanh(double x) {
	uint64_t bits = bitsTo<uint64_t>(x);
	int64_t ndnan = int64_t(bits & 0x7ff8000000000000ull);
	bool special = !(ndnan <= 0x7fe0000000000000ll); // |x| > HUGE_THRESHOLD, inf, nan
	// the index from the high word, clamped as 32-bit lanes, then >> 51
	int32_t hi = int32_t(uint32_t(uint64_t(ndnan - 0x3fc0000000000000ll) >> 32));
	hi = std::min(std::max(hi, 0), 0x780000);
	unsigned idx = unsigned(uint32_t(hi) >> 19);
	auto L = [&](int k) { return bitsTo<double>(lut16x18[size_t(k) * 16 + idx]); };
	double b = L(0);
	double y = std::fabs(x) - b;
	double r = std::fma(L(17), y, L(16));
	for (int k = 15; k >= 1; k--)
		r = std::fma(r, y, L(k));
	if (special)
		r = 1.0;
	r = bitsTo<double>(bitsTo<uint64_t>(r) | (bits & 0x8000000000000000ull));
	if (std::isnan(x))
		r = std::numeric_limits<double>::quiet_NaN();
	return r;
}

float tanh(float x) {
	uint32_t bits = bitsTo<uint32_t>(x);
	int32_t ndnan = int32_t(bits & 0x7fe00000u);
	bool special = !(ndnan <= 0x7f000000);
	int32_t idxs = ndnan - 0x3d400000;
	idxs = std::min(std::max(idxs, 0), 0x3e00000);
	unsigned idx = uint32_t(idxs) >> 21;
	auto L = [&](int k) { return bitsTo<float>(lut32x8[size_t(k) * 32 + idx]); };
	float b = L(0);
	float y = std::fabs(x) - b;
	float r = std::fma(L(7), y, L(6));
	for (int k = 5; k >= 1; k--)
		r = std::fma(r, y, L(k));
	if (special)
		r = 1.0f;
	r = bitsTo<float>(bitsTo<uint32_t>(r) | (bits & 0x80000000u));
	if (std::isnan(x))
		r = std::numeric_limits<float>::quiet_NaN();
	return r;
}

// --- float32 exp, sin, cos: numpy's simd_exp_FLOAT (loops_exponent_log)
// and simd_sincos_f32 (loops_trigonometric), one lane ---

float exp(float xin) {
	const float xmax = 88.72283935546875f, xmin = -103.97208404541015625f;
	const float magic = 0x1.800000p+23f;
	bool nanM = std::isnan(xin);
	float x = nanM ? 0.0f : xin;
	bool maxM = x >= xmax, minM = x <= xmin;
	if (nanM || minM || maxM)
		x = 0.0f;
	float q = x * 1.442695040888963407359924681001892137f; // NPY_LOG2Ef
	q = q + magic; // round to nearest
	q = q - magic;
	x = std::fma(q, -6.93145752e-1f, x); // Cody-Waite
	x = std::fma(q, -1.42860677e-6f, x);
	x = std::fma(q, 0.0f, x);
	float num = std::fma(5.082762527590693718096e-04f, x, 6.757896990527504603057e-03f);
	num = std::fma(num, x, 5.114512081637298353406e-02f);
	num = std::fma(num, x, 2.473615434895520810817e-01f);
	num = std::fma(num, x, 7.257664613233124478488e-01f);
	num = std::fma(num, x, 9.999999999980870924916e-01f);
	float den = std::fma(2.159509375685829852307e-02f, x, -2.742335390411667452936e-01f);
	den = std::fma(den, x, 1.000000000000000000000e+00f);
	float poly = num / den;
	poly = std::ldexp(poly, int(q)); // _mm512_scalef_ps
	if (nanM)
		poly = std::numeric_limits<float>::quiet_NaN();
	if (maxM)
		poly = std::numeric_limits<float>::infinity();
	if (minM)
		poly = 0.0f;
	return poly;
}

static float sincosF32(float xin, bool cosine) {
	const float maxCody = cosine ? 71476.0625f : 117435.992f;
	if (!(std::fabs(xin) <= maxCody)) // large, inf or NaN: glibc, as numpy
		return cosine ? std::cos(xin) : std::sin(xin);
	const float magic = 0x1.800000p+23f;
	float q = xin * 0x1.45f306p-1f;
	q = q + magic;
	q = q - magic;
	float r = std::fma(q, -0x1.921fb0p+00f, xin);
	r = std::fma(q, -0x1.5110b4p-22f, r);
	r = std::fma(q, -0x1.846988p-48f, r);
	float r2 = r * r;
	float c = std::fma(0x1.98e616p-16f, r2, -0x1.6c06dcp-10f);
	c = std::fma(c, r2, 0x1.55553cp-05f);
	c = std::fma(c, r2, -0x1.000000p-01f);
	c = std::fma(c, r2, 0x1.000000p+00f);
	float s = std::fma(0x1.7d3bbcp-19f, r2, -0x1.a06bbap-13f);
	s = std::fma(s, r2, 0x1.11119ap-07f);
	s = std::fma(s, r2, -0x1.555556p-03f);
	s = std::fma(s, r2, 0.0f);
	s = std::fma(s, r, r);
	int32_t iq = int32_t(std::lrint(q));
	if (cosine)
		iq += 1;
	float v = (iq & 1) == 0 ? s : c;
	if ((iq & 2) == 2)
		v = 0.0f - v;
	return v;
}

float sin(float x) { return sincosF32(x, false); }
float cos(float x) { return sincosF32(x, true); }

// --- pw and ex: art/tube_walk_lib.py's deterministic power and exp ---

namespace {
const double LN2 = 0x1.62e42fefa39efp-1, INV_LN2 = 0x1.71547652b82fep+0, SQRT_HALF = 0x1.6a09e667f3bcdp-1;
const double LN2_HI = 0x1.62e42fee00000p-1, LN2_LO = 0x1.a39ef35793c76p-33;

// e^z for |z| <= ln2 / 2: Horner over 1/k!, k = 14 .. 0 (tube_walk_lib _EXP_C)
double expPoly(double z) {
	double fact[15];
	fact[0] = 1;
	for (int k = 1; k < 15; k++)
		fact[k] = fact[k - 1] * k;
	double q = 1.0 / fact[14];
	for (int k = 13; k >= 0; k--)
		q = q * z + 1.0 / fact[k];
	return q;
}

double ldexpClamped(double q, double n) {
	return std::ldexp(q, int(n < -2000 ? -2000 : n > 2000 ? 2000 : n));
}
} // namespace

double pw(double x, double e) {
	double ax = std::fabs(x);
	int k;
	double m = std::frexp(ax, &k); // ax = m 2^k, m in [0.5, 1)
	if (m < SQRT_HALF) {
		m = m * 2;
		k = k - 1;
	}
	double s = (m - 1) / (m + 1), s2 = s * s;
	double p = 1.0 / (2 * 11 + 1);
	for (int j = 10; j >= 0; j--)
		p = p * s2 + 1.0 / (2 * j + 1);
	double t = e * (double(k) + (2 * s * p) * INV_LN2); // e log2 |x|
	double n = std::nearbyint(t);
	double r = ldexpClamped(expPoly((t - n) * LN2), n);
	if (ax == 0)
		r = e > 0 ? 0.0 : e == 0 ? 1.0 : HUGE_VAL;
	if (e == double(int64_t(e))) {
		if (x < 0 && (int64_t(e) % 2 != 0))
			r = -r;
	} else if (x < 0)
		r = std::numeric_limits<double>::quiet_NaN();
	return r;
}

float pw(float x, double e) {
	return float(pw(double(x), e));
}

double ex(double x) {
	double n = std::nearbyint(x * INV_LN2);
	double z = (x - n * LN2_HI) - n * LN2_LO; // Cody-Waite
	return ldexpClamped(expPoly(z), n);
}

float ex(float x) {
	return float(ex(double(x)));
}

// --- math.hypot: CPython 3.12 vector_norm for two values ---

double hypot(double x, double y) {
	double vec[2] = {std::fabs(x), std::fabs(y)};
	double mx = std::max(vec[0], vec[1]);
	if (std::isinf(vec[0]) || std::isinf(vec[1]))
		return HUGE_VAL;
	if (std::isnan(vec[0]) || std::isnan(vec[1]))
		return NAN;
	if (mx == 0.0)
		return mx;
	int maxE;
	std::frexp(mx, &maxE);
	if (maxE < -1023) {
		// subnormals: scaled up first, as CPython does
		return DBL_MIN * hypot(vec[0] / DBL_MIN, vec[1] / DBL_MIN);
	}
	double scale = std::ldexp(1.0, -maxE);
	double csum = 1.0, frac1 = 0.0, frac2 = 0.0;
	auto dlMul = [](double a, double b, double& hi, double& lo) {
		hi = a * b;
		lo = std::fma(a, b, -hi);
	};
	auto dlFastSum = [](double a, double b, double& hi, double& lo) {
		hi = a + b;
		lo = (a - hi) + b;
	};
	for (double v : vec) {
		double xs = v * scale, ph, pl, sh, sl;
		dlMul(xs, xs, ph, pl);
		dlFastSum(csum, ph, sh, sl);
		csum = sh;
		frac1 += pl;
		frac2 += sl;
	}
	double h = std::sqrt(csum - 1.0 + (frac1 + frac2));
	double ph, pl, sh, sl;
	dlMul(-h, h, ph, pl);
	dlFastSum(csum, ph, sh, sl);
	csum = sh;
	frac1 += pl;
	frac2 += sl;
	double xx = csum - 1.0 + (frac1 + frac2);
	h += xx / (2.0 * h);
	return h / scale;
}

template <typename T>
static Img<T> normLastT(const Img<T>& a) {
	Img<T> o(a.h, a.w, 1);
	std::vector<T> s(size_t(a.c));
	for (size_t i = 0, m = size_t(a.h) * a.w; i < m; i++) {
		for (int k = 0; k < a.c; k++)
			s[size_t(k)] = a.v[i * a.c + k] * a.v[i * a.c + k];
		o.v[i] = std::sqrt(T(0) + pairwise(s.data(), s.size()));
	}
	return o;
}
ImgF normLast(const ImgF& a) { return normLastT(a); }
ImgD normLast(const ImgD& a) { return normLastT(a); }

double norm(const std::vector<double>& v) {
	double t = 0.0;
	for (double x : v)
		t = t + x * x;
	return std::sqrt(0.0 + t);
}

// ===========================================================================
// the self-check (developer aid): fixed inputs through every primitive,
// written to DIR as raw little-endian arrays for art/npcore_check.py

namespace {

// the fixed inputs (art/npcore_check.py makes the same)
uint32_t hash32(uint64_t i, uint32_t seed) {
	uint32_t h = uint32_t(i) * 2654435761u + seed * 40503u + 12345u;
	h ^= h >> 15;
	h *= 2246822519u;
	h ^= h >> 13;
	h *= 3266489917u;
	h ^= h >> 16;
	return h;
}

// kind 0 noise, 1 smooth with wrapping bands and a little noise, 2 a blob
// mask with specks; for c == 4 the last channel is an alpha ramp with clear
// and opaque ends
Img8 testImg(int h, int w, int c, uint32_t seed, int kind) {
	Img8 im(h, w, c);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			for (int k = 0; k < c; k++) {
				uint64_t i = (uint64_t(y) * w + x) * c + k;
				uint32_t hv = hash32(i, seed);
				int v;
				if (c == 4 && k == 3)
					v = x < w / 5 ? 0 : x > 4 * w / 5 ? 255 : ((x * 5 + y * 3) & 255);
				else if (kind == 0)
					v = int(hv & 255);
				else if (kind == 1)
					v = ((x * 256) / w + (y * 128) / h + k * 60 + int(hv & 15)) & 255;
				else {
					int dx = x - w / 3, dy = y - h / 2, r = std::min(w, h) / 3;
					v = (dx * dx + dy * dy < r * r || hv % 11 == 0) ? 255 : 0;
				}
				im(y, x, k) = uint8_t(v);
			}
	return im;
}

std::vector<float> testF32(size_t n, uint32_t seed, double scale, double offset) {
	std::vector<float> o(n);
	for (size_t i = 0; i < n; i++)
		o[i] = float(double(hash32(i, seed) & 0xffffff) / 16777216.0 * scale + offset);
	return o;
}

std::vector<double> testF64(size_t n, uint32_t seed, double scale, double offset) {
	std::vector<double> o(n);
	for (size_t i = 0; i < n; i++)
		o[i] = double(hash32(i, seed)) / 4294967296.0 * scale + offset;
	return o;
}

struct Out {
	std::filesystem::path dir;
	std::ofstream index;
	template <typename T>
	void put(const std::string& name, const char* dtype, const std::vector<T>& v) {
		std::ofstream f(dir / (name + ".bin"), std::ios::binary);
		f.write(reinterpret_cast<const char*>(v.data()), std::streamsize(v.size() * sizeof(T)));
		index << name << " " << dtype << " " << v.size() << "\n";
	}
	void img(const std::string& name, const Img8& im) {
		put(name, "u8", im.v);
	}
};

} // namespace

int selfCheck(const std::string& dirName) {
	Out o;
	o.dir = dirName;
	std::filesystem::create_directories(o.dir);
	o.index.open(o.dir / "index.txt");

	// --- the random streams ---
	{
		Generator g(8);
		std::vector<double> s;
		auto add = [&](const std::vector<double>& v) { s.insert(s.end(), v.begin(), v.end()); };
		add(g.random(37));
		for (int i = 0; i < 5; i++)
			s.push_back(double(g.integers(0, 3)));
		add(g.normal(0, 1, 50));
		s.push_back(g.uniform(0.3, 0.7));
		s.push_back(g.random());
		for (int i = 0; i < 4; i++)
			s.push_back(double(g.integers(-6, 7)));
		add(g.uniform(-5, 5, 20));
		for (int i = 0; i < 10; i++)
			s.push_back(g.normal(0, 0.6));
		add(g.random(63));
		for (int i = 0; i < 3; i++)
			s.push_back(double(g.integers(0, 1000)));
		for (int i = 0; i < 2; i++)
			s.push_back(double(g.integers(0, int64_t(1) << 40)));
		add(g.normal(128, 30, 24));
		for (int64_t v : g.integers(0, 3, 6))
			s.push_back(double(v));
		add(g.random(4));
		for (int i = 0; i < 3; i++)
			s.push_back(g.standardNormal());
		s.push_back(double(g.integers(0, 7)));
		s.push_back(g.uniform(-1, 1));
		o.put("rng_mixed_s8", "f8", s);
	}
	{
		Generator g(41);
		o.put("rng_normal_s41", "f8", g.normal(0, 1, 300000));
	}
	{
		std::vector<double> s;
		const uint64_t seeds[] = {0, 1, 3, 5, 7, 11, 12, 21, 31, 41, 52, 61, 62, 4294967301ull, 123456789012345ull};
		for (uint64_t seed : seeds) {
			Generator g(seed);
			for (double v : g.random(3))
				s.push_back(v);
			s.push_back(double(g.integers(0, 100)));
			s.push_back(double(g.integers(0, 100)));
			for (double v : g.normal(0, 1, 2))
				s.push_back(v);
		}
		o.put("rng_seeds", "f8", s);
	}
	{
		Generator g(99);
		std::vector<int64_t> s;
		for (int i = 0; i < 3; i++)
			s.push_back(g.integers(0, int64_t(1) << 32));
		for (int i = 0; i < 3; i++)
			s.push_back(g.integers(-(int64_t(1) << 62), int64_t(1) << 62));
		s.push_back(g.integers(5, 6));
		s.push_back(g.integers(0, 3));
		s.push_back(int64_t(g.nextU64()));
		for (int i = 0; i < 1000; i++)
			s.push_back(g.integers(0, 37));
		s.push_back(int64_t(g.nextU64()));
		o.put("rng_ints_s99", "i8", s);
	}

	// --- Pillow: resize ---
	struct RCase {
		const char* name;
		int h, w, c, kind, W, H;
		Resample r;
	};
	const RCase rc[] = {
		{"rs_L_noise_bicubic", 5, 7, 1, 0, 64, 40, Resample::Bicubic},
		{"rs_L_canvas_bicubic", 153, 282, 1, 0, 364, 672, Resample::Bicubic},
		{"rs_L_streaks_bicubic", 5, 14, 1, 0, 100, 80, Resample::Bicubic},
		{"rs_RGB_tp_bicubic", 24, 24, 3, 1, 91, 75, Resample::Bicubic},
		{"rs_L_vonly_bilinear", 90, 26, 1, 1, 100, 90, Resample::Bilinear},
		{"rs_L_down_bilinear", 90, 100, 1, 1, 100, 9, Resample::Bilinear},
		{"rs_L_up_bicubic_from9", 9, 100, 1, 1, 100, 90, Resample::Bicubic},
		{"rs_L_down_bicubic", 30, 40, 1, 0, 17, 11, Resample::Bicubic},
		{"rs_RGB_bilinear", 21, 33, 3, 1, 70, 50, Resample::Bilinear},
		{"rs_RGBA_bicubic", 16, 20, 4, 1, 45, 37, Resample::Bicubic},
		{"rs_RGBA_lanczos", 16, 20, 4, 1, 45, 37, Resample::Lanczos},
		{"rs_L_slow_bicubic", 2, 12, 1, 0, 138, 20, Resample::Bicubic},
		{"rs_L_mo_bicubic", 6, 16, 1, 0, 132, 42, Resample::Bicubic},
		{"rs_L_lanczos_down", 50, 61, 1, 0, 23, 19, Resample::Lanczos},
		{"rs_L_hamming", 20, 30, 1, 1, 47, 13, Resample::Hamming},
		{"rs_L_box", 20, 30, 1, 1, 47, 13, Resample::Box},
	};
	for (const RCase& c : rc)
		o.img(c.name, resize(testImg(c.h, c.w, c.c, 7, c.kind), c.W, c.H, c.r));

	// --- GaussianBlur ---
	const double radii[] = {0.6, 1.2, 2.5, 4, 5, 6, 7.5, 8, 9};
	const char* rn[] = {"0.6", "1.2", "2.5", "4", "5", "6", "7.5", "8", "9"};
	for (int i = 0; i < 9; i++) {
		o.img(std::string("gb_L_") + rn[i], gaussianBlur(testImg(61, 97, 1, 11, 1), radii[i]));
		o.img(std::string("gb_RGB_") + rn[i], gaussianBlur(testImg(61, 97, 3, 12, 0), radii[i]));
	}
	o.img("gb_L_tiny_8", gaussianBlur(testImg(5, 7, 1, 13, 0), 8));
	o.img("gb_L_mask_8", gaussianBlur(testImg(90, 120, 1, 14, 2), 8));
	o.img("gb_RGBA_2.5", gaussianBlur(testImg(30, 40, 4, 15, 1), 2.5));
	o.img("bb_L_3", boxBlur(testImg(30, 40, 1, 16, 0), 3));

	// --- rank filters ---
	{
		Img8 mask = testImg(60, 80, 1, 21, 2), noise = testImg(40, 50, 1, 22, 0);
		o.img("rk_min3_mask", minFilter(mask, 3));
		o.img("rk_min5_mask", minFilter(mask, 5));
		o.img("rk_max3_mask", maxFilter(mask, 3));
		o.img("rk_max5_mask", maxFilter(mask, 5));
		o.img("rk_max9_mask", maxFilter(mask, 9));
		o.img("rk_min5_noise", minFilter(noise, 5));
		o.img("rk_max9_noise", maxFilter(noise, 9));
		o.img("rk_median7_noise", medianFilter(noise, 7));
		o.img("rk_median7_mask", medianFilter(mask, 7));
		o.img("rk_median7_RGB", medianFilter(testImg(30, 33, 3, 23, 1), 7));
		Img8 cur = mask;
		for (int k = 0; k < 6; k++)
			cur = minFilter(cur, 5);
		o.img("rk_min5x6_mask", cur);
	}

	// --- rotate ---
	{
		// the recipe's angle (tube_walk.py: start_top, slope, ang)
		double startTop = 1438 + double((4780 - 4835) * (1496 - 1438)) / (5935 - 4835);
		double slope = (1798 - 134 - startTop) / ((6113 - 110) + 20 - 4780);
		double ang = std::atan(slope) * (180.0 / kPi);
		o.put("rot_angles", "f8", std::vector<double>{ang});
		Img8 rgba = testImg(40, 60, 4, 31, 1);
		const double angles[] = {10.434190, -10.434190, ang, -ang, 33.3, 90, 180, -90};
		const char* an[] = {"p10.434190", "m10.434190", "pang", "mang", "p33.3", "p90", "p180", "m90"};
		for (int i = 0; i < 8; i++) {
			Img8 r = rotate(rgba, angles[i]);
			o.put(std::string("rot_RGBA_") + an[i] + "_size", "i8", std::vector<int64_t>{r.h, r.w});
			o.img(std::string("rot_RGBA_") + an[i], r);
		}
		Img8 strip = testImg(170, 300, 4, 32, 1);
		for (size_t i = 0; i < strip.v.size(); i += 4)
			strip.v[i + 3] = strip.v[i + 3] > 100 ? 255 : 0; // as the tube's piece: alpha 0 or 255
		Img8 rs = rotate(strip, -ang);
		o.put("rot_strip_mang_size", "i8", std::vector<int64_t>{rs.h, rs.w});
		o.img("rot_strip_mang", rs);
		Img8 rl = rotate(testImg(40, 60, 1, 33, 1), -10.434190);
		o.put("rot_L_m10.434190_size", "i8", std::vector<int64_t>{rl.h, rl.w});
		o.img("rot_L_m10.434190", rl);
		Img8 rr = rotate(testImg(40, 60, 3, 34, 0), -10.434190);
		o.put("rot_RGB_m10.434190_size", "i8", std::vector<int64_t>{rr.h, rr.w});
		o.img("rot_RGB_m10.434190", rr);
		Img8 rb = rotate(rgba, -10.434190, Resample::Bilinear);
		o.put("rot_RGBA_bilinear_size", "i8", std::vector<int64_t>{rb.h, rb.w});
		o.img("rot_RGBA_bilinear", rb);
		Img8 rn2 = rotate(rgba, 25.0, Resample::Bicubic, false);
		o.img("rot_RGBA_noexpand_25", rn2);
	}

	// --- ImageDraw ---
	{
		// the recipe's branch circles, from (4730, 1360) on a 520 x 260 canvas
		const int C0x = 4780, C0y = 1380;
		const std::vector<std::vector<std::pair<int, int>>> br = {
			{{190, 70}, {215, 5}, {232, 10}, {226, 90}, {252, 180}, {225, 188}, {205, 140}},
			{{308, 150}, {345, 122}, {378, 128}, {362, 188}, {318, 188}},
			{{388, 70}, {405, 8}, {422, 14}, {452, 100}, {457, 182}, {442, 222}, {408, 218}, {414, 150}}};
		Img8 m(260, 520, 1);
		for (auto& p : br) {
			std::vector<std::pair<double, double>> pts;
			for (auto& q : p)
				pts.push_back({double(q.first + C0x - 4730), double(q.second + C0y - 1360)});
			polygon(m, pts, 255);
		}
		o.img("pg_branches", m);
		// a jittered blob with fractional points, at 2x (as the rock's outline)
		Img8 b(300, 340, 1);
		std::vector<std::pair<double, double>> pts;
		for (int k = 0; k < 90; k++) {
			double t = k * (2 * kPi / 90);
			double rr = 60 + 25 * std::sin(3 * t) + 10 * std::sin(7 * t + 1) + double(hash32(uint64_t(k), 41) & 1023) / 1023.0 * 9 - 4.5;
			pts.push_back({(170 + rr * std::cos(t) * 1.3 - 3.25) * 2 - 170, (150 + rr * std::sin(t)) * 2 - 150});
		}
		polygon(b, pts, 255);
		o.img("pg_blob", b);
		// concave, horizontal runs, out of bounds, exact slopes
		Img8 s(80, 90, 1);
		polygon(s, {{-10, 5}, {30, 5}, {50, 5}, {45, 30}, {88, 40}, {95, 79}, {60, 60}, {40, 85}, {20, 60}, {20, 60}, {5, 70}, {0, 40}}, 200);
		polygon(s, {{10, 10}, {20, 20}, {10, 30}, {0, 20}}, 255);
		polygon(s, {{60, 10}, {70, 10}, {80, 10}, {80, 20}, {70, 20}, {60, 20}}, 128);
		polygon(s, {{30.7, 40.2}, {44.9, 44.99}, {36.5, 58.5}, {29.999, 52.1}}, 77);
		o.img("pg_shapes", s);
		struct RR {
			int H, W, x0, y0, x1, y1, r;
		};
		const RR rr[] = {{173, 35, 0, 0, 34, 172, 10}, {169, 32, 0, 0, 31, 168, 9}, {30, 50, 2, 3, 47, 26, 20},
			{20, 20, 0, 0, 19, 19, 15}, {40, 40, 3, 3, 30, 35, 0}, {45, 60, 5, 4, 50, 40, 7}, {30, 60, 0, 0, 59, 12, 9}};
		int i = 0;
		for (const RR& q : rr) {
			Img8 im(q.H, q.W, 1);
			roundedRectangle(im, q.x0, q.y0, q.x1, q.y1, q.r, 255);
			o.img("rr_" + std::to_string(i++), im);
		}
		Img8 e(50, 70, 1);
		ellipse(e, 3, 4, 60, 40, 255);
		ellipse(e, 50, 30, 69, 49, 128);
		pieslice(e, 10, 10, 40, 45, 30, 200, 60);
		pieslice(e, 0, 0, 25, 25, -45, 45, 90);
		rectangle(e, 60, 2, 66, 8, 33);
		o.img("dr_misc", e);
	}

	// --- numpy: reductions ---
	{
		std::vector<float> s32, m32;
		std::vector<double> s64, m64;
		std::vector<size_t> ns;
		for (size_t n = 1; n <= 300; n++)
			ns.push_back(n);
		for (size_t n : {1000, 8191, 8192, 8193, 20000, 100000})
			ns.push_back(n);
		for (size_t n : ns) {
			std::vector<float> a = testF32(n, uint32_t(n), 255, 0);
			std::vector<double> d = testF64(n, uint32_t(n) + 7, 2, -1);
			s32.push_back(sum(a.data(), n));
			m32.push_back(meanFlat(a.data(), n));
			s64.push_back(sum(d.data(), n));
			m64.push_back(meanFlat(d.data(), n));
		}
		o.put("red_sum_f32", "f4", s32);
		o.put("red_mean_f32", "f4", m32);
		o.put("red_sum_f64", "f8", s64);
		o.put("red_mean_f64", "f8", m64);
		// strided views big[:n, 7] of a (3000, 40, 3) float32 array
		std::vector<float> big = testF32(3000 * 40 * 3, 5, 255, 0);
		std::vector<float> vs, vm, vsd;
		for (int n : {100, 2730, 2731, 2800, 2999}) {
			std::vector<float> v;
			for (int r = 0; r < n; r++)
				for (int k = 0; k < 3; k++)
					v.push_back(big[(size_t(r) * 40 + 7) * 3 + k]);
			vs.push_back(sum(v.data(), v.size()));
			vm.push_back(meanFlat(v.data(), v.size()));
			vsd.push_back(stdFlat(v.data(), v.size()));
		}
		o.put("red_view_sum", "f4", vs);
		o.put("red_view_mean", "f4", vm);
		o.put("red_view_std", "f4", vsd);
		std::vector<float> sd;
		for (size_t n : {5, 37, 1000, 9000}) {
			std::vector<float> a = testF32(n, uint32_t(n) + 3, 40, -20);
			sd.push_back(stdFlat(a.data(), n));
		}
		o.put("red_std_f32", "f4", sd);
		ImgF a3(70, 90, 3);
		a3.v = testF32(70 * 90 * 3, 6, 200, -100);
		o.put("red_sumlast_f32", "f4", sumLast(a3).v);
		o.put("red_meanlast_f32", "f4", meanLast(a3).v);
		ImgD d3(70, 90, 3);
		d3.v = testF64(70 * 90 * 3, 8, 200, -100);
		o.put("red_sumlast_f64", "f8", sumLast(d3).v);
		o.put("red_meanlast_f64", "f8", meanLast(d3).v);
		o.put("red_normlast_f32", "f4", normLast(a3).v);
		o.put("red_normlast_f64", "f8", normLast(d3).v);
		std::vector<float> ax;
		for (size_t n : {5, 100, 1000, 50000}) {
			std::vector<float> r = testF32(n * 3, uint32_t(n) + 9, 255, 0);
			for (float v : meanAxis0(r.data(), n, 3))
				ax.push_back(v);
			for (float v : sumAxis0(r.data(), n, 3))
				ax.push_back(v);
		}
		o.put("red_axis0_f32", "f4", ax);
		for (int ss : {2, 3}) {
			ImgF p(60 * ss, 70 * ss, 1);
			p.v = testF32(p.v.size(), uint32_t(ss) + 10, 255, 0);
			o.put("red_block_L_" + std::to_string(ss), "f4", blockMean(p, ss).v);
			ImgF q(60 * ss, 70 * ss, 3);
			q.v = testF32(q.v.size(), uint32_t(ss) + 20, 255, 0);
			o.put("red_block_RGB_" + std::to_string(ss), "f4", blockMean(q, ss).v);
			ImgD pd(60 * ss, 70 * ss, 1);
			pd.v = testF64(pd.v.size(), uint32_t(ss) + 30, 1, 0);
			o.put("red_block_Ld_" + std::to_string(ss), "f8", blockMean(pd, ss).v);
			ImgD qd(60 * ss, 70 * ss, 3);
			qd.v = testF64(qd.v.size(), uint32_t(ss) + 40, 255, 0);
			o.put("red_block_RGBd_" + std::to_string(ss), "f8", blockMean(qd, ss).v);
		}
	}

	// --- numpy: functions ---
	{
		std::vector<float> med32;
		std::vector<double> med64;
		for (size_t n : {1, 2, 7, 100, 101, 1000, 1001}) {
			std::vector<float> a = testF32(n, uint32_t(n) + 40, 255, 0);
			med32.push_back(median(a));
			med32.push_back(median(a, true));
			std::vector<double> d = testF64(n, uint32_t(n) + 41, 1, 0);
			med64.push_back(median(d));
			med64.push_back(median(d, true));
		}
		o.put("fn_median_f32", "f4", med32);
		o.put("fn_median_f64", "f8", med64);
		// percentile(prof[16:132], 35, axis=0) of a (170, 3) float32 table, and other q
		std::vector<float> prof = testF32(170 * 3, 50, 90, 3);
		std::vector<double> pc;
		for (int k = 0; k < 3; k++) {
			std::vector<float> lane;
			for (int r = 16; r < 132; r++)
				lane.push_back(prof[size_t(r) * 3 + k]);
			pc.push_back(percentile(lane, 35));
		}
		std::vector<float> l37 = testF32(37, 51, 255, 0);
		for (double q : {0.0, 10.0, 35.0, 50.0, 62.5, 99.9, 100.0})
			pc.push_back(percentile(l37, q));
		std::vector<double> d37 = testF64(37, 52, 1, 0);
		for (double q : {0.0, 10.0, 35.0, 50.0, 99.9, 100.0})
			pc.push_back(percentile(d37, q));
		o.put("fn_percentile", "f8", pc);
		// sort, searchsorted (quantized values, so duplicates)
		std::vector<float> src = testF32(1000, 53, 255, 0);
		for (auto& v : src)
			v = std::floor(v * 4) / 4;
		std::vector<float> ss = sorted(src);
		o.put("fn_sort", "f4", ss);
		std::vector<float> q = testF32(2000, 54, 270, -5);
		for (size_t i = 0; i < 500; i++)
			q[i] = ss[(i * 7) % ss.size()];
		std::vector<int64_t> sl, sr;
		for (float v : q) {
			sl.push_back(searchsorted(ss, v));
			sr.push_back(searchsorted(ss, v, true));
		}
		o.put("fn_searchsorted_left", "i8", sl);
		o.put("fn_searchsorted_right", "i8", sr);
		// interp(clip(x), linspace(0, 1, n), sorted float32 as float64)
		std::vector<double> fp(ss.begin(), ss.end());
		std::vector<double> xp = linspace(0, 1, int(fp.size()));
		std::vector<double> xs = testF64(5000, 55, 1.2, -0.1);
		for (size_t i = 0; i < 50; i++)
			xs[i] = xp[i * 13];
		o.put("fn_interp", "f8", interp(xs, xp, fp));
		std::vector<double> ls;
		for (int n : {1, 2, 3, 7, 24, 4440})
			for (double v : linspace(0, 1, n))
				ls.push_back(v);
		for (double v : linspace(0.95, 0.7, 6))
			ls.push_back(v);
		for (double v : linspace(1, 0, 16))
			ls.push_back(v);
		for (double v : linspace(-3.5, 7.25, 13))
			ls.push_back(v);
		o.put("fn_linspace", "f8", ls);
		// convolve: an edge-padded float32 column with a normalised Gaussian
		std::vector<double> k81, k51;
		for (int i = -40; i <= 40; i++) {
			double t = i / 16.0;
			k81.push_back(std::exp(-0.5 * (t * t)));
		}
		for (int i = -25; i <= 25; i++) {
			double t = i / 10.0;
			k51.push_back(std::exp(-0.5 * (t * t)));
		}
		double s81 = sum(k81.data(), k81.size()), s51 = sum(k51.data(), k51.size());
		for (auto& v : k81)
			v /= s81;
		for (auto& v : k51)
			v /= s51;
		std::vector<float> col = testF32(300, 56, 255, 0);
		std::vector<double> colD(col.begin(), col.end());
		std::vector<double> cv = convolve(padEdge(colD, 40), k81, "valid");
		std::vector<double> c2 = convolve(padEdge(colD, 25), k51, "valid");
		cv.insert(cv.end(), c2.begin(), c2.end());
		std::vector<double> shortA(colD.begin(), colD.begin() + 30);
		for (const char* mode : {"same", "full", "valid"}) {
			std::vector<double> c3 = convolve(shortA, k51, mode);
			cv.insert(cv.end(), c3.begin(), c3.end());
			std::vector<double> c4 = convolve(shortA, {0.25, 0.5, 0.25}, mode);
			cv.insert(cv.end(), c4.begin(), c4.end());
		}
		o.put("fn_convolve", "f8", cv);
		// gradient
		ImgF g(23, 37);
		g.v = testF32(23 * 37, 57, 100, -20);
		ImgF gy, gx;
		gradient(g, gy, gx);
		o.put("fn_gradient_y", "f4", gy.v);
		o.put("fn_gradient_x", "f4", gx.v);
		// tanh
		std::vector<double> tx = testF64(200000, 58, 16, -8);
		for (double v : {0.0, -0.0, 1e-300, -1e-300, 5e-324, 1e-9, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 19.0, 19.5, 30.0, 1e10, -1e10, 1e300})
			tx.push_back(v);
		tx.push_back(HUGE_VAL);
		tx.push_back(-HUGE_VAL);
		std::vector<double> ty;
		for (double v : tx)
			ty.push_back(tanh(v));
		o.put("fn_tanh_f64", "f8", ty);
		std::vector<float> tfx = testF32(200000, 59, 20, -10);
		for (float v : {0.0f, -0.0f, 1e-30f, 1e-45f, 0.5f, 1.0f, 3.0f, 8.0f, 9.0f, 9.5f, 20.0f, 1e20f, -1e20f})
			tfx.push_back(v);
		std::vector<float> tfy;
		for (float v : tfx)
			tfy.push_back(tanh(v));
		o.put("fn_tanh_f32", "f4", tfy);
		// float32 exp, sin, cos
		std::vector<float> expIn = testF32(300000, 65, 240, -120);
		std::vector<float> sx = testF32(300000, 66, 400, -200);
		std::vector<float> bx = testF32(20000, 67, 300000, -150000);
		for (float v : {0.0f, -0.0f, 88.7f, 88.72283935546875f, 89.0f, -103.9f, -103.97208404541015625f, -110.0f, 1e-40f, -87.5f})
			expIn.push_back(v);
		expIn.push_back(std::numeric_limits<float>::infinity());
		expIn.push_back(-std::numeric_limits<float>::infinity());
		expIn.push_back(std::numeric_limits<float>::quiet_NaN());
		std::vector<float> eo, so, co;
		for (float v : expIn)
			eo.push_back(exp(v));
		sx.insert(sx.end(), bx.begin(), bx.end());
		for (float v : {0.0f, -0.0f, 1e-30f, 71476.0625f, 71476.07f, 117435.992f, 117436.0f, 1e30f})
			sx.push_back(v);
		sx.push_back(std::numeric_limits<float>::infinity());
		sx.push_back(std::numeric_limits<float>::quiet_NaN());
		for (float v : sx) {
			so.push_back(sin(v));
			co.push_back(cos(v));
		}
		o.put("fn_exp_f32", "f4", eo);
		o.put("fn_sin_f32", "f4", so);
		o.put("fn_cos_f32", "f4", co);
		// pw and ex (tube_walk_lib.py)
		{
			std::vector<double> xr = testF64(200000, 70, 1.2, 0), xw = testF64(50000, 71, 0.5, 0.5);
			for (size_t i = 0; i < xw.size(); i++)
				xw[i] = std::ldexp(xw[i], int(hash32(i, 72) % 2100) - 1074);
			for (double v : {0.0, 1.0, 0.5, 1.2, 5e-324, 1e-310, 0x1p-1022, 1e300, 0.7071067811865476, 0.7071067811865475})
				xw.push_back(v);
			std::vector<double> po;
			for (double e : {0.85, 1.6, 1.1, 0.4, 0.8, 1.5, 3.0, 12.0, 20.0, 24.0, 26.0, 30.0, -0.7, 2.5, 0.0, 1.0 / 3}) {
				for (double v : xr)
					po.push_back(pw(v, e));
				for (double v : xw)
					po.push_back(pw(v, e));
			}
			std::vector<double> xn = testF64(20000, 73, 4, -2);
			for (double e : {3.0, 4.0})
				for (double v : xn)
					po.push_back(pw(v, e));
			o.put("fn_pw_f64", "f8", po);
			std::vector<float> x32 = testF32(200000, 74, 1.2, 0), p32;
			for (float v : {0.0f, 1.0f, 1e-45f, 1e-38f, 0.5f, 1.2f})
				x32.push_back(v);
			for (double e : {0.85, 1.6, 1.1, 0.4, 0.8, 1.5, 3.0})
				for (float v : x32)
					p32.push_back(pw(v, e));
			o.put("fn_pw_f32", "f4", p32);
			std::vector<double> xe = testF64(300000, 75, 80, -70), eo64;
			for (auto hd : {std::make_pair(40, 16.0), std::make_pair(25, 10.0), std::make_pair(8, 1.6)})
				for (int i = -hd.first; i <= hd.first; i++) {
					double t = i / hd.second;
					xe.push_back(-0.5 * (t * t));
				}
			for (double v : {0.0, -0.0, 1.0, -745.0, 709.0, 710.0, -1000.0})
				xe.push_back(v);
			for (double v : xe)
				eo64.push_back(ex(v));
			o.put("fn_ex_f64", "f8", eo64);
			std::vector<float> xf = testF32(100000, 76, 60, -50), eo32;
			for (float v : xf)
				eo32.push_back(ex(v));
			o.put("fn_ex_f32", "f4", eo32);
		}
		// hypot
		std::vector<double> ha = testF64(100000, 60, 4000, -2000), hb = testF64(100000, 61, 300, -150), hy;
		for (size_t i = 0; i < ha.size(); i++)
			hy.push_back(hypot(ha[i], hb[i]));
		for (size_t i = 0; i < 1000; i++)
			hy.push_back(hypot(std::floor(ha[i]), std::floor(hb[i])));
		for (size_t i = 0; i < 1000; i++)
			hy.push_back(hypot(ha[i] * 1e-310, hb[i] * 1e-310));
		hy.push_back(hypot(0, 0));
		hy.push_back(hypot(3, 4));
		hy.push_back(hypot(1e308, 1e308));
		o.put("fn_hypot", "f8", hy);
		// norm of a 1-D float64 vector
		std::vector<double> nv;
		nv.push_back(norm({-0.55, -0.55, 0.63}));
		nv.push_back(norm({0.9, -0.1, 0.25}));
		nv.push_back(norm({-0.6, -0.45, 0.66}));
		for (size_t n : {3, 5, 7, 20, 101}) {
			std::vector<double> v = testF64(n, uint32_t(n) + 62, 2, -1);
			nv.push_back(norm(v));
		}
		o.put("fn_norm", "f8", nv);
		// astype(uint8) and clip
		std::vector<float> uf = testF32(5000, 63, 700, -200);
		for (float v : {255.9f, 256.0f, 300.5f, -1.0f, -0.5f, -200.7f, 1e10f, -0.0f})
			uf.push_back(v);
		std::vector<uint8_t> uo, uc;
		for (float v : uf) {
			uo.push_back(u8(v));
			uc.push_back(u8(clip(v, 0.0f, 255.0f)));
		}
		o.put("fn_u8_f32", "u8", uo);
		o.put("fn_u8clip_f32", "u8", uc);
		std::vector<double> ud = testF64(3000, 64, 700, -200);
		std::vector<uint8_t> udo;
		for (double v : ud)
			udo.push_back(u8(v));
		o.put("fn_u8_f64", "u8", udo);
		std::vector<float> cl;
		for (float v : uf)
			cl.push_back(clip(v, -3.5f, 100.25f));
		o.put("fn_clip_f32", "f4", cl);
	}
	o.index.close();
	return 0;
}

namespace {
// OPENC2E_NPCORE_CHECK=DIR: write the self-check at startup and exit
struct SelfCheckAtStartup {
	SelfCheckAtStartup() {
		const char* dir = devenv("OPENC2E_NPCORE_CHECK");
		if (!dir || !*dir)
			return;
		try {
			selfCheck(dir);
			std::fprintf(stderr, "npcore: self-check written to %s\n", dir);
			std::exit(0);
		} catch (const std::exception& e) {
			std::fprintf(stderr, "npcore: self-check failed: %s\n", e.what());
			std::exit(1);
		}
	}
} selfCheckAtStartup;
} // namespace


} // namespace np
} // namespace MadeArt

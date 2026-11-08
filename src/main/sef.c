#include "common.h"
#include "shared.h"
#include "sef.h"

typedef struct SefActor {
    unsigned int flags;
    unsigned char unmodeled_004[0xc];
    Vector4 position;
    unsigned char unmodeled_20[0x30];
    Vector4 rotation;
    Vector4 scale;
    unsigned char unmodeled_70[0x86 - 0x70];
    short scheduleState;             /* +0x86 */
    unsigned char unmodeled_88[0x510 - 0x88];
    Vector4 light_parameters[15];    /* +0x510 */
    unsigned char unmodeled_600[0x824 - 0x600];
    Matrix4 *accessoryMatrix;
} SefActor;

#define SEF_ACTOR_LIGHT_FLAG 0x00008000u
#define SEF_BATTLE_ACTOR_MAX 24
typedef struct SefBattleActor {
    unsigned char unmodeled_000[0x20];
    Vector4 light_direction;         /* +0x20 */
    unsigned char unmodeled_030[0x20];
    Vector4 light_accumulator;       /* +0x50 */
    Vector4 light_position;          /* +0x60 */
    Vector4 actor_position;          /* +0x70 */
    SefActor *actor;
    unsigned char unmodeled_084[0]; /* +0x84: zero-length alias of the former padding member, kept for saved bodies */
    short excluded;
    short lightCount;
    short hitSignal;
    short schedulerLightCount;
    short seSignal;
    unsigned char unmodeled_08e[2];
} SefBattleActor;

typedef struct SefBattleActorTbl {
    SefBattleActor actors[SEF_BATTLE_ACTOR_MAX];
    unsigned char unmodeled_d80[0]; /* +0xd80: zero-length alias of the former padding member, kept for saved bodies */
    Vector4 center;
    int count;
    unsigned char unmodeled_d94[0x0c];
} SefBattleActorTbl;
typedef struct SefSavePrm {
    Vector4 position;
    Vector4 orientation;
    Vector4 saved_vector;
    int active;
    int saved_words[5];
    short effect_no;
    unsigned char saved_flags;
    unsigned char unmodeled_4b[5];
} SefSavePrm;
#define SEF_SAVE_PRM_COUNT 0x80

/* Arctangent lookup over [0, 1] in 1/1024 steps (srsAtan2 reads it through the uncached mirror 0x2040FAC0). */
float sefAtanTbl[1025] __attribute__((aligned(64))) = {
    0.0f, 0.000977f, 0.001953f, 0.00293f, 0.003906f, 0.004883f,
    0.005859f, 0.006836f, 0.007812f, 0.008789f, 0.009765f, 0.010742f,
    0.011718f, 0.012695f, 0.013671f, 0.014647f, 0.015624f, 0.0166f,
    0.017576f, 0.018553f, 0.019529f, 0.020505f, 0.021481f, 0.022457f,
    0.023433f, 0.024409f, 0.025385f, 0.026361f, 0.027337f, 0.028313f,
    0.029288f, 0.030264f, 0.03124f, 0.032215f, 0.033191f, 0.034166f,
    0.035142f, 0.036117f, 0.037092f, 0.038068f, 0.039043f, 0.040018f,
    0.040993f, 0.041968f, 0.042942f, 0.043917f, 0.044892f, 0.045866f,
    0.046841f, 0.047815f, 0.048789f, 0.049764f, 0.050738f, 0.051712f,
    0.052686f, 0.053659f, 0.054633f, 0.055607f, 0.05658f, 0.057554f,
    0.058527f, 0.0595f, 0.060473f, 0.061446f, 0.062419f, 0.063392f,
    0.064364f, 0.065337f, 0.066309f, 0.067281f, 0.068253f, 0.069225f,
    0.070197f, 0.071169f, 0.07214f, 0.073112f, 0.074083f, 0.075054f,
    0.076025f, 0.076996f, 0.077967f, 0.078937f, 0.079908f, 0.080878f,
    0.081848f, 0.082818f, 0.083788f, 0.084757f, 0.085727f, 0.086696f,
    0.087665f, 0.088634f, 0.089603f, 0.090572f, 0.09154f, 0.092509f,
    0.093477f, 0.094445f, 0.095413f, 0.09638f, 0.097348f, 0.098315f,
    0.099282f, 0.100249f, 0.101215f, 0.102182f, 0.103148f, 0.104114f,
    0.10508f, 0.106046f, 0.107012f, 0.107977f, 0.108942f, 0.109907f,
    0.110872f, 0.111836f, 0.1128f, 0.113764f, 0.114728f, 0.115692f,
    0.116655f, 0.117619f, 0.118582f, 0.119544f, 0.120507f, 0.121469f,
    0.122431f, 0.123393f, 0.124355f, 0.125316f, 0.126278f, 0.127239f,
    0.128199f, 0.12916f, 0.13012f, 0.13108f, 0.13204f, 0.132999f,
    0.133959f, 0.134918f, 0.135876f, 0.136835f, 0.137793f, 0.138751f,
    0.139709f, 0.140666f, 0.141624f, 0.142581f, 0.143537f, 0.144494f,
    0.14545f, 0.146406f, 0.147361f, 0.148317f, 0.149272f, 0.150227f,
    0.151181f, 0.152136f, 0.15309f, 0.154043f, 0.154997f, 0.15595f,
    0.156903f, 0.157855f, 0.158808f, 0.15976f, 0.160711f, 0.161663f,
    0.162614f, 0.163565f, 0.164515f, 0.165465f, 0.166415f, 0.167365f,
    0.168314f, 0.169263f, 0.170212f, 0.17116f, 0.172108f, 0.173056f,
    0.174004f, 0.174951f, 0.175898f, 0.176844f, 0.17779f, 0.178736f,
    0.179682f, 0.180627f, 0.181572f, 0.182516f, 0.18346f, 0.184404f,
    0.185348f, 0.186291f, 0.187234f, 0.188177f, 0.189119f, 0.190061f,
    0.191002f, 0.191943f, 0.192884f, 0.193825f, 0.194765f, 0.195705f,
    0.196644f, 0.197583f, 0.198522f, 0.199461f, 0.200399f, 0.201336f,
    0.202274f, 0.203211f, 0.204147f, 0.205083f, 0.206019f, 0.206955f,
    0.20789f, 0.208825f, 0.209759f, 0.210693f, 0.211627f, 0.21256f,
    0.213493f, 0.214426f, 0.215358f, 0.216289f, 0.217221f, 0.218152f,
    0.219083f, 0.220013f, 0.220943f, 0.221872f, 0.222801f, 0.22373f,
    0.224658f, 0.225586f, 0.226514f, 0.227441f, 0.228367f, 0.229294f,
    0.23022f, 0.231145f, 0.23207f, 0.232995f, 0.233919f, 0.234843f,
    0.235767f, 0.23669f, 0.237612f, 0.238535f, 0.239456f, 0.240378f,
    0.241299f, 0.242219f, 0.24314f, 0.244059f, 0.244979f, 0.245898f,
    0.246816f, 0.247734f, 0.248652f, 0.249569f, 0.250486f, 0.251402f,
    0.252318f, 0.253233f, 0.254149f, 0.255063f, 0.255977f, 0.256891f,
    0.257804f, 0.258717f, 0.25963f, 0.260542f, 0.261453f, 0.262364f,
    0.263275f, 0.264185f, 0.265095f, 0.266004f, 0.266913f, 0.267821f,
    0.268729f, 0.269637f, 0.270544f, 0.27145f, 0.272357f, 0.273262f,
    0.274167f, 0.275072f, 0.275976f, 0.27688f, 0.277784f, 0.278687f,
    0.279589f, 0.280491f, 0.281392f, 0.282293f, 0.283194f, 0.284094f,
    0.284994f, 0.285893f, 0.286791f, 0.28769f, 0.288587f, 0.289485f,
    0.290381f, 0.291278f, 0.292173f, 0.293069f, 0.293963f, 0.294858f,
    0.295752f, 0.296645f, 0.297538f, 0.29843f, 0.299322f, 0.300214f,
    0.301105f, 0.301995f, 0.302885f, 0.303774f, 0.304663f, 0.305552f,
    0.30644f, 0.307327f, 0.308214f, 0.3091f, 0.309986f, 0.310872f,
    0.311757f, 0.312641f, 0.313525f, 0.314409f, 0.315291f, 0.316174f,
    0.317056f, 0.317937f, 0.318818f, 0.319698f, 0.320578f, 0.321458f,
    0.322336f, 0.323215f, 0.324092f, 0.32497f, 0.325846f, 0.326723f,
    0.327598f, 0.328474f, 0.329348f, 0.330222f, 0.331096f, 0.331969f,
    0.332842f, 0.333714f, 0.334585f, 0.335456f, 0.336327f, 0.337197f,
    0.338066f, 0.338935f, 0.339803f, 0.340671f, 0.341538f, 0.342405f,
    0.343271f, 0.344137f, 0.345002f, 0.345867f, 0.346731f, 0.347594f,
    0.348457f, 0.34932f, 0.350182f, 0.351043f, 0.351904f, 0.352764f,
    0.353624f, 0.354483f, 0.355342f, 0.3562f, 0.357057f, 0.357914f,
    0.358771f, 0.359627f, 0.360482f, 0.361337f, 0.362191f, 0.363045f,
    0.363898f, 0.36475f, 0.365602f, 0.366454f, 0.367305f, 0.368155f,
    0.369005f, 0.369854f, 0.370703f, 0.371551f, 0.372398f, 0.373245f,
    0.374092f, 0.374938f, 0.375783f, 0.376628f, 0.377472f, 0.378316f,
    0.379159f, 0.380001f, 0.380843f, 0.381684f, 0.382525f, 0.383365f,
    0.384205f, 0.385044f, 0.385883f, 0.386721f, 0.387558f, 0.388395f,
    0.389231f, 0.390067f, 0.390902f, 0.391736f, 0.39257f, 0.393403f,
    0.394236f, 0.395068f, 0.3959f, 0.396731f, 0.397562f, 0.398391f,
    0.399221f, 0.40005f, 0.400878f, 0.401705f, 0.402532f, 0.403359f,
    0.404184f, 0.40501f, 0.405834f, 0.406658f, 0.407482f, 0.408305f,
    0.409127f, 0.409949f, 0.41077f, 0.41159f, 0.41241f, 0.41323f,
    0.414049f, 0.414867f, 0.415684f, 0.416501f, 0.417318f, 0.418134f,
    0.418949f, 0.419764f, 0.420578f, 0.421391f, 0.422204f, 0.423016f,
    0.423828f, 0.424639f, 0.42545f, 0.42626f, 0.427069f, 0.427878f,
    0.428686f, 0.429493f, 0.4303f, 0.431107f, 0.431912f, 0.432717f,
    0.433522f, 0.434326f, 0.435129f, 0.435932f, 0.436734f, 0.437536f,
    0.438337f, 0.439137f, 0.439937f, 0.440736f, 0.441534f, 0.442332f,
    0.44313f, 0.443926f, 0.444722f, 0.445518f, 0.446313f, 0.447107f,
    0.447901f, 0.448694f, 0.449486f, 0.450278f, 0.45107f, 0.45186f,
    0.45265f, 0.45344f, 0.454229f, 0.455017f, 0.455805f, 0.456592f,
    0.457378f, 0.458164f, 0.458949f, 0.459734f, 0.460518f, 0.461301f,
    0.462084f, 0.462866f, 0.463648f, 0.464429f, 0.465209f, 0.465989f,
    0.466768f, 0.467546f, 0.468324f, 0.469101f, 0.469878f, 0.470654f,
    0.47143f, 0.472204f, 0.472979f, 0.473752f, 0.474525f, 0.475298f,
    0.476069f, 0.47684f, 0.477611f, 0.478381f, 0.47915f, 0.479919f,
    0.480687f, 0.481454f, 0.482221f, 0.482988f, 0.483753f, 0.484518f,
    0.485283f, 0.486046f, 0.486809f, 0.487572f, 0.488334f, 0.489095f,
    0.489856f, 0.490616f, 0.491375f, 0.492134f, 0.492893f, 0.49365f,
    0.494407f, 0.495164f, 0.495919f, 0.496674f, 0.497429f, 0.498183f,
    0.498936f, 0.499689f, 0.500441f, 0.501192f, 0.501943f, 0.502693f,
    0.503443f, 0.504192f, 0.50494f, 0.505688f, 0.506435f, 0.507181f,
    0.507927f, 0.508673f, 0.509417f, 0.510161f, 0.510905f, 0.511647f,
    0.512389f, 0.513131f, 0.513872f, 0.514612f, 0.515352f, 0.516091f,
    0.516829f, 0.517567f, 0.518304f, 0.519041f, 0.519777f, 0.520512f,
    0.521247f, 0.521981f, 0.522715f, 0.523447f, 0.52418f, 0.524911f,
    0.525642f, 0.526373f, 0.527102f, 0.527832f, 0.52856f, 0.529288f,
    0.530015f, 0.530742f, 0.531468f, 0.532193f, 0.532918f, 0.533642f,
    0.534366f, 0.535089f, 0.535811f, 0.536533f, 0.537254f, 0.537975f,
    0.538694f, 0.539414f, 0.540132f, 0.54085f, 0.541568f, 0.542284f,
    0.543001f, 0.543716f, 0.544431f, 0.545145f, 0.545859f, 0.546572f,
    0.547284f, 0.547996f, 0.548707f, 0.549418f, 0.550128f, 0.550837f,
    0.551546f, 0.552254f, 0.552962f, 0.553668f, 0.554375f, 0.55508f,
    0.555785f, 0.55649f, 0.557194f, 0.557897f, 0.558599f, 0.559301f,
    0.560003f, 0.560703f, 0.561403f, 0.562103f, 0.562802f, 0.5635f,
    0.564198f, 0.564895f, 0.565591f, 0.566287f, 0.566982f, 0.567676f,
    0.56837f, 0.569064f, 0.569756f, 0.570449f, 0.57114f, 0.571831f,
    0.572521f, 0.573211f, 0.5739f, 0.574588f, 0.575276f, 0.575963f,
    0.57665f, 0.577336f, 0.578021f, 0.578706f, 0.57939f, 0.580073f,
    0.580756f, 0.581439f, 0.58212f, 0.582801f, 0.583482f, 0.584162f,
    0.584841f, 0.58552f, 0.586198f, 0.586875f, 0.587552f, 0.588228f,
    0.588903f, 0.589578f, 0.590253f, 0.590927f, 0.5916f, 0.592272f,
    0.592944f, 0.593615f, 0.594286f, 0.594956f, 0.595626f, 0.596295f,
    0.596963f, 0.597631f, 0.598298f, 0.598964f, 0.59963f, 0.600295f,
    0.60096f, 0.601624f, 0.602287f, 0.60295f, 0.603612f, 0.604274f,
    0.604935f, 0.605595f, 0.606255f, 0.606914f, 0.607573f, 0.608231f,
    0.608888f, 0.609545f, 0.610201f, 0.610857f, 0.611512f, 0.612166f,
    0.61282f, 0.613473f, 0.614126f, 0.614778f, 0.615429f, 0.61608f,
    0.61673f, 0.61738f, 0.618029f, 0.618677f, 0.619325f, 0.619972f,
    0.620619f, 0.621265f, 0.62191f, 0.622555f, 0.623199f, 0.623843f,
    0.624486f, 0.625128f, 0.62577f, 0.626411f, 0.627052f, 0.627692f,
    0.628332f, 0.62897f, 0.629609f, 0.630246f, 0.630883f, 0.63152f,
    0.632156f, 0.632791f, 0.633426f, 0.63406f, 0.634694f, 0.635327f,
    0.635959f, 0.636591f, 0.637222f, 0.637852f, 0.638482f, 0.639112f,
    0.639741f, 0.640369f, 0.640996f, 0.641623f, 0.64225f, 0.642876f,
    0.643501f, 0.644126f, 0.64475f, 0.645373f, 0.645996f, 0.646619f,
    0.647241f, 0.647862f, 0.648482f, 0.649102f, 0.649722f, 0.650341f,
    0.650959f, 0.651577f, 0.652194f, 0.65281f, 0.653426f, 0.654042f,
    0.654657f, 0.655271f, 0.655884f, 0.656497f, 0.65711f, 0.657722f,
    0.658333f, 0.658944f, 0.659554f, 0.660164f, 0.660773f, 0.661381f,
    0.661989f, 0.662596f, 0.663203f, 0.663809f, 0.664415f, 0.66502f,
    0.665624f, 0.666228f, 0.666831f, 0.667434f, 0.668036f, 0.668638f,
    0.669239f, 0.669839f, 0.670439f, 0.671038f, 0.671637f, 0.672235f,
    0.672833f, 0.67343f, 0.674026f, 0.674622f, 0.675217f, 0.675812f,
    0.676406f, 0.677f, 0.677593f, 0.678185f, 0.678777f, 0.679368f,
    0.679959f, 0.680549f, 0.681139f, 0.681728f, 0.682317f, 0.682904f,
    0.683492f, 0.684079f, 0.684665f, 0.685251f, 0.685836f, 0.68642f,
    0.687005f, 0.687588f, 0.688171f, 0.688753f, 0.689335f, 0.689916f,
    0.690497f, 0.691077f, 0.691657f, 0.692236f, 0.692814f, 0.693392f,
    0.693969f, 0.694546f, 0.695122f, 0.695698f, 0.696273f, 0.696848f,
    0.697422f, 0.697995f, 0.698568f, 0.699141f, 0.699712f, 0.700284f,
    0.700854f, 0.701425f, 0.701994f, 0.702563f, 0.703132f, 0.7037f,
    0.704267f, 0.704834f, 0.7054f, 0.705966f, 0.706532f, 0.707096f,
    0.70766f, 0.708224f, 0.708787f, 0.70935f, 0.709912f, 0.710473f,
    0.711034f, 0.711594f, 0.712154f, 0.712713f, 0.713272f, 0.71383f,
    0.714388f, 0.714945f, 0.715502f, 0.716058f, 0.716613f, 0.717168f,
    0.717723f, 0.718277f, 0.71883f, 0.719383f, 0.719935f, 0.720487f,
    0.721038f, 0.721589f, 0.722139f, 0.722689f, 0.723238f, 0.723786f,
    0.724334f, 0.724882f, 0.725429f, 0.725975f, 0.726521f, 0.727066f,
    0.727611f, 0.728156f, 0.728699f, 0.729243f, 0.729785f, 0.730328f,
    0.730869f, 0.731411f, 0.731951f, 0.732491f, 0.733031f, 0.73357f,
    0.734109f, 0.734646f, 0.735184f, 0.735721f, 0.736257f, 0.736793f,
    0.737329f, 0.737864f, 0.738398f, 0.738932f, 0.739465f, 0.739998f,
    0.74053f, 0.741062f, 0.741593f, 0.742124f, 0.742654f, 0.743184f,
    0.743713f, 0.744242f, 0.74477f, 0.745298f, 0.745825f, 0.746352f,
    0.746878f, 0.747403f, 0.747928f, 0.748453f, 0.748977f, 0.749501f,
    0.750024f, 0.750546f, 0.751068f, 0.75159f, 0.752111f, 0.752631f,
    0.753151f, 0.753671f, 0.75419f, 0.754708f, 0.755226f, 0.755744f,
    0.756261f, 0.756777f, 0.757293f, 0.757809f, 0.758324f, 0.758838f,
    0.759352f, 0.759865f, 0.760378f, 0.760891f, 0.761403f, 0.761914f,
    0.762425f, 0.762936f, 0.763446f, 0.763955f, 0.764464f, 0.764973f,
    0.76548f, 0.765988f, 0.766495f, 0.767001f, 0.767507f, 0.768013f,
    0.768518f, 0.769022f, 0.769526f, 0.77003f, 0.770533f, 0.771036f,
    0.771538f, 0.772039f, 0.77254f, 0.773041f, 0.773541f, 0.774041f,
    0.77454f, 0.775038f, 0.775537f, 0.776034f, 0.776531f, 0.777028f,
    0.777524f, 0.77802f, 0.778515f, 0.77901f, 0.779504f, 0.779998f,
    0.780491f, 0.780984f, 0.781477f, 0.781968f, 0.78246f, 0.782951f,
    0.783441f, 0.783931f, 0.784421f, 0.78491f, 0.785398f
};

static unsigned char _eftBuffer[0xD4800];
static unsigned char _scheduler[0xAB0 * 0x80];
static unsigned char _lineData[0x41000];
static unsigned char _ptAlloc[0xA0810];
static SefSavePrm _savePrm[SEF_SAVE_PRM_COUNT];
static unsigned char _battleData[0x230];
static unsigned char _battlePrm[0x40];
static SefBattleActorTbl _battleActor[1];

static SchedulerState *_nowScheduler;
static int _nowScriptAdr;
/* 8 bytes in the original (cc1 emits each .lcomm label before its .align, so the
 * unused word at 0x004DC67C is the tail of this object); only word 0 is used. */
static int _nowParentLocal[2];
static int _sefLoadEftQue;

#define SEF_LINE_DATA_RECORD_SIZE 0x820
#define SEF_LINE_DATA_COUNT 0x80
typedef struct SefLineDataEntry {
    unsigned char unmodeled_000[2];
    unsigned short inUse;
    unsigned char unmodeled_004[SEF_LINE_DATA_RECORD_SIZE - 4];
} SefLineDataEntry;
typedef struct SefLineDataTable {
    unsigned char unmodeled_000[0x810];
    SefLineDataEntry entries[SEF_LINE_DATA_COUNT];
} SefLineDataTable;

typedef struct GameLoopFlagsView {
    unsigned char unmodeled_00[0x10];
    int flags;
} GameLoopFlagsView;
extern GameLoopFlagsView GameLoopState;
#define SEF_DRAW_FLAG_PENDING 0x04000000

/* The freed-scheduler helper is declared after an assembly placeholder below. */
extern void sefFreeScheduler(unsigned int scheduler_index);

typedef struct SefKey3 {
    short frame;
    short value;
    short jump;
} SefKey3;

typedef struct SefProgressState {
    int key_index;
    int frame;
    int value;
    unsigned char unmodeled_0c[4];
    Vector4 vector;
} SefProgressState;

float MMathCalcLength(float *vec);

static int _parentLine = -1;
static float _gravity = 0.0f;
static float _colision = 0.0f;
static float _height = 0.0f;
static short _revDirZ = 0;
static short _initialize = 0;
short _hitFlag = 0;
short _hitSignal = 0;
short _seSignal = 0;
short _sefBattleMode = 0;

float srsAtan2(float x, float y)
{
    static float *atanTbl_0 __asm__("atanTbl.0") = (float *)0x2040FAC0;
    int y_is_positive;
    int x_is_positive;
    float result = 0.0f;
    float angle;

    y_is_positive = y >= 0.0f;
    x_is_positive = x >= 0.0f;

    if (y == 0.0f) {
        if (x != 0.0f) {
            result = 1.5707964f;
            if (!x_is_positive) {
                result = -1.5707964f;
            }
        }
        return result;
    }

    if (x == 0.0f) {
        if (!y_is_positive) {
            result = 3.1415927f;
        }
        return result;
    }

    if (!y_is_positive) {
        y = -y;
    }
    if (!x_is_positive) {
        x = -x;
    }

    if (x <= y) {
        angle = atanTbl_0[(int)(x * 1024.0f / y)];

        if (y_is_positive) {
            if (!x_is_positive) {
                angle = -angle;
            }
        } else if (x_is_positive) {
            angle = 3.1415927f - angle;
        } else {
            angle += 3.1415927f;
        }
    } else {
        angle = atanTbl_0[(int)(y * 1024.0f / x)];

        if (y_is_positive) {
            if (x_is_positive) {
                angle = 1.5707964f - angle;
            } else {
                angle = -(1.5707964f - angle);
            }
        } else if (x_is_positive) {
            angle += 1.5707964f;
        } else {
            angle = 4.712389f - angle;
        }
    }

    return angle;
}

static int sefRandSeed = 0x12345678;

static float sefRandf(void)
{
    int bits;

    sefRandSeed = sefRandSeed * 0x41C64E6D + 0x3039;
    bits = ((unsigned int)sefRandSeed >> 16) & 0x7FFF;
    return (float)bits * 3.051851e-05f;
}

/*
 * sefIsBossID (main:0x002e11e8): true when character_id falls in the boss ID
 * range [0x97, 0xba] (sefCnvDeathEffectNo, sefSetupEnemy, this TU).
 */
static int sefIsBossID(int character_id)
{
    return (unsigned int)(character_id - 0x97) < 0x24;
}

/*
 * GNU EE native TI storage/copy type (docs/native-ti.md), used only for the
 * 16-byte aligned zero-fill blocks sefMemZero clears with por+sq; no wide
 * arithmetic is done on it.
 */
typedef unsigned int Quadword __attribute__((mode(TI)));

typedef union SefQuadBlock {
    Quadword quad;
} SefQuadBlock;

/*
 * sefMemZero (main:0x002e11f8): clears `size` bytes at `dest` in aligned
 * 16-byte blocks, then any remaining 4-byte word. sefInitLineData,
 * sefCreateParticle, sefInitScheduler, sefPushEffect, sefPopEffect,
 * sdvInitAlter and sdvDestroyAlter (all still assembly in this TU) are its
 * callers.
 */
void sefMemZero(void *dest, int size)
{
    SefQuadBlock *blocks = (SefQuadBlock *)dest;
    int block_count = size >> 4;
    int word_count = (size & 0xf) >> 2;
    int *words = (int *)(blocks + block_count);
    int i;

    for (i = 0; i < block_count; i++) {
        blocks[i].quad = 0;
    }
    for (i = 0; i < word_count; i++) {
        words[i] = 0;
    }
}

extern Matrix4 *MMathRotateMatrixXYZ(Matrix4 *out, Matrix4 *matrix, Vector4 *angles);
extern Matrix4 *MMathScaleMatrix(Matrix4 *out, Matrix4 *matrix, Vector4 *scale);

/*
 * sefCalcRotTransSMatrix (main:0x002e1270): builds `matrix` as a
 * translation-only matrix from `translation` (VU0 macro-mode broadcast of
 * vf0's own one-hot rows for the identity part, the same idiom
 * sefCalcInvView below uses, with vf1's own W lane forced to vf0's own W
 * and its XYZ lanes loaded from `translation`), then rotates it in place by
 * `angles` and scales it by `scale`. sefGetMotionNullMatrix,
 * sefGetParentMatrix and sefExecScheduler are its callers.
 */
void sefCalcRotTransSMatrix(Vector4 *angles, Vector4 *translation, Vector4 *scale, Matrix4 *matrix)
{
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(translation) : "memory");
    __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf2, vf0" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf3, vf2" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf4, vf3" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf2, 32(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf3, 16(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf4, 0(%0)" : : "r"(matrix) : "memory");
    MMathRotateMatrixXYZ(matrix, matrix, angles);
    MMathScaleMatrix(matrix, matrix, scale);
}

extern StudioCamera *xglStudioGetActiveCamera(void);
extern Matrix4 *MMathRotateMatrixYXZ(Matrix4 *out, Matrix4 *matrix, Vector4 *angles);

/*
 * sefCalcInvView (main:0x002e12e0): writes the identity matrix into `matrix`
 * (VU0 macro-mode broadcast of vf0's own one-hot rows, the same idiom
 * sefCalcRotChange's own identity block below uses), then rotates it in
 * place by the active studio camera's own rotation (StudioCamera::rotation,
 * include/shared.h, at +0xA0) in YXZ order. sefDrawEffect is its only
 * caller.
 */
static void sefCalcInvView(Matrix4 *matrix) {
    StudioCamera *camera = xglStudioGetActiveCamera();

    __asm__ __volatile__("vmr32.xyzw vf1, vf0" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf2, vf1" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf3, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf0, 48(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf1, 32(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf2, 16(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("sqc2 vf3, 0(%0)" : : "r"(matrix) : "memory");
    MMathRotateMatrixYXZ(matrix, matrix, &camera->rotation);
}

typedef struct {
    unsigned char unmodeled_00[0x8];
    int wflag[9];                       /* +0x08 */
    unsigned char unmodeled_2c[0x40 - 0x2c];
    int flag[0x18];                     /* +0x40 */
    unsigned char unmodeled_a0[0xf4 - 0xa0];
    short wid[9];                       /* +0xf4 */
    unsigned char unmodeled_106[0x110 - 0x106];
    short id[0x18];                     /* +0x110 */
} SefMapTbl__df;
typedef struct {
    short pid[3];
    short id[3];
} SefBattlePrmView;
extern short srsEftNoWeaponEffectID(int effect_id);
extern short D_00794120[];

int sefSearchMapperIndex2(int effect_id, int number, int category) {
    extern SefMapTbl__df _srsMemRes;
    extern unsigned char _battlePrm[];
    int o, i, n, j;
    int w;

    if (category == 0xE) {
        return 0xE;
    }
    if (category == 0xF) {
        return 0xF;
    }
    if ((unsigned)(category - 0x10) < 0x18) {
        for (i = 0; i < 24; i++) {
            if (_srsMemRes.flag[i] != 0 && _srsMemRes.id[i] == effect_id) {
                return 0x10 + i;
            }
        }
        return 0;
    }
    if ((unsigned)(effect_id - 0xAF0) < 0xC8u) {
        w = srsEftNoWeaponEffectID(effect_id);
        if (w > 0) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    n = i * 3 + j;
                    if (_srsMemRes.wflag[n] != 0 && _srsMemRes.wid[n] == w) {
                        return i * 3 + j + 2;
                    }
                }
            }
        }
        return 0;
    }
    if ((unsigned)effect_id < 0x64 || (unsigned)(effect_id - 0xF0) < 0x9D ||
        (unsigned)(effect_id - 0x9C4) < 0x64 || (unsigned)(effect_id - 0xA78) < 0x6A) {
        for (i = 0; i < 3; i++) {
            if (D_00794120[0] != 0 && ((short *)_battlePrm)[i] == D_00794120[0]) {
                return 3 * i + 2;
            }
        }
        for (i = 0; i < 3; i++) {
            if (D_00794120[0] != 0 && ((SefBattlePrmView *)_battlePrm)->id[i] == D_00794120[0]) {
                return 0xB + i;
            }
        }
    }
    return 0;
}

extern int sefSearchMapperIndex2(int effect_id, int number, int category);
extern int srsAnalyzeEftNo(int effect_id, int *number, int *category);

/*
 * sefSearchMapperIndex (main:0x002e1540): decomposes the effect id into a
 * number/category pair via srsAnalyzeEftNo, then feeds them to the mapper
 * lookup sefSearchMapperIndex2, discarding its result.
 */
int sefSearchMapperIndex(int effect_id) {
    int number;
    int category;

    srsAnalyzeEftNo(effect_id, &number, &category);
    return sefSearchMapperIndex2(effect_id, number, category);
}


/*
 * sevInitPtAllocator (main:0x002e1580): clears the whole _ptAlloc particle
 * allocator table and _battleData, then rebuilds _ptAlloc's own trailing
 * free-index queue (1024 entries, main VA 0x00794110's 0xa07fe-byte offset)
 * with the descending indices 0x3ff..0. sefInitEffect is its only caller.
 */
static void sevInitPtAllocator(void)
{
    short *slot;
    int index;

    memset(_ptAlloc, 0, 0xA0810);
    slot = (short *)(_ptAlloc + 0xA07FE);
    memset(_battleData, 0, 0x230);
    index = 0x3FF;
    do {
        *slot = (short)index;
        index--;
        slot--;
    } while (index >= 0);
}

typedef struct {
    unsigned char records[1024][640];
    short queue[1024];
    int head;
    int tail;
} SefPtAllocTable;

static int sevAllocPtAllocator(void)
{
    SefPtAllocTable *t = (SefPtAllocTable *)_ptAlloc;
    int head = t->head;
    int next = (head + 1) & 0x3FF;

    if (next == t->tail) {
        return -1;
    }
    t->head = next;
    return t->queue[head];
}

static void sevFreePtAllocator(unsigned int handle)
{
    SefPtAllocTable *t = (SefPtAllocTable *)_ptAlloc;
    int tail = t->tail;

    if (handle < 0x400 && t->head != tail) {
        t->queue[tail] = handle;
        t->tail = (tail + 1) & 0x3FF;
    }
}

static void sefProgressKey3(SefKey3 *keys, SefProgressState *state)
{
    int frame = state->frame + 1;

    state->frame = frame;
    if (frame < 1024) {
        SefKey3 *key = keys + state->key_index;

        if (key->frame != 1024 && frame >= key[1].frame) {
            if (key[1].jump >= 0) {
                state->key_index = key[1].jump;
                key = &keys[state->key_index];
                state->frame = key->frame;
            } else {
                state->key_index++;
            }
        }
    }
}

/*
 * A 10-byte keyed-animation record (config/units, sefProgressKey5): the
 * end_frame this function tests against 1024 and the following record's own
 * jump index are the only fields any accepted function of this TU touches;
 * the six bytes between them are not yet evidenced.
 */
typedef struct SefKey5 {
    short end_frame;
    short vector[3];
    short jump;
} SefKey5;

/*
 * sefProgressKey5 (main:0x002e1740): like sefProgressKey3 above, but on the
 * wider 10-byte SefKey5 keys sefLerpVector/sefLerpIVector/sefLerpIVector2
 * (all still assembly in this TU) share: advances `state`'s frame, and once
 * it reaches the current key's own end_frame either follows the next key's
 * jump index or moves to the next key in sequence, returning the active key.
 */
static SefKey5 *sefProgressKey5(SefKey5 *keys, SefProgressState *state)
{
    int key_index = state->key_index;
    int frame = state->frame + 1;
    SefKey5 *key = keys + key_index;
    short jump;

    state->frame = frame;
    if (frame < 1024) {
        if (key->end_frame != 1024 && frame >= key[1].end_frame) {
            key++;
            jump = key->jump;
            if (jump >= 0) {
                state->key_index = jump;
                key = keys + jump;
                state->frame = key->end_frame;
            } else {
                state->key_index = key_index + 1;
            }
        }
    }
    return key;
}

/*
 * sefLerpVectorA (main:0x002e17c8), re-treated from the accepted assembly
 * group src/main/sef/sef_lerp_vector_a.s toward readable C with constrained
 * ee-vu-cop2 inline assembly (route ee-vu-cop2, docs/ps2-capabilities.md).
 *
 * Reads the packed signed halfword key at `keys` with the R5900 unaligned
 * doubleword ldl/ldr pair, discards the low halfword with dsrl, widens the
 * remaining three signed halfwords to words with pcgth/pextlh (the one
 * evidenced MMI packed-halfword widening idiom, user-authorized 2026-09-13:
 * config/compiler-patterns.json CP-0169 -- cc1 2.96 emits no MMI for any C
 * spelling, so this reproduces the original's own hand-written idiom, not
 * compiler output), converts the widened lanes to float with vitof0.xyz and
 * forces the resulting vector's W lane to the VU0 architectural constant
 * VF0.W=1.0 with vmove.w before storing the full quadword at `dest`. The
 * original has no .globl, so the emitted binding stays LOCAL.
 */
static void sefLerpVectorA(void *keys, Vector4 *dest)
{
    __asm__ __volatile__(
        "ldl $9, 7(%0)\n\t"
        "ldr $9, 0(%0)\n\t"
        "dsrl $9, $9, 0x10\n\t"
        "vmove.w $vf2w, $vf0w\n\t"
        "pcgth $10, $0, $9\n\t"
        "pextlh $10, $10, $9\n\t"
        "qmtc2 $10, $vf1\n\t"
        "vitof0.xyz $vf2xyz, $vf1xyz\n\t"
        "sqc2 $vf2, 0(%1)\n\t"
        "nop"
        :
        : "r"(keys), "r"(dest)
        : "$9", "$10", "memory"
    );
}

int sefLerpVector(SefKey5 *keys, SefProgressState *state)
{
    Vector4 *dest = (Vector4 *)((char *)state + 0x10);
    SefKey5 *key;
    float fraction;

    key = keys + state->key_index;
    if (key[1].end_frame == 1024) {
        return 0;
    }
    key = sefProgressKey5(keys, state);
    if (key->end_frame != key[1].end_frame) {
        fraction = (float)(state->frame - key->end_frame) / (float)(key[1].end_frame - key->end_frame);
        __asm__ __volatile__(
            "mfc1 $9, %2\n\t"
            "qmtc2.ni $9, $vf3\n\t"
            "ldl $8, 7(%0)\n\t"
            "ldr $8, 0(%0)\n\t"
            "ldl $9, 7(%1)\n\t"
            "ldr $9, 0(%1)\n\t"
            "dsrl $8, $8, 16\n\t"
            "dsrl $9, $9, 16\n\t"
            "pcgth $10, $0, $8\n\t"
            "pcgth $11, $0, $9\n\t"
            "pextlh $10, $10, $8\n\t"
            "pextlh $11, $11, $9\n\t"
            "qmtc2.ni $10, $vf1\n\t"
            "qmtc2.ni $11, $vf2\n\t"
            "vitof0.xyzw $vf1, $vf1\n\t"
            "vitof0.xyzw $vf2, $vf2\n\t"
            "vsub.xyz $vf2, $vf2, $vf1\n\t"
            "vmulx.xyz $vf2, $vf2, $vf3x\n\t"
            "vadd.xyz $vf2, $vf2, $vf1\n\t"
            "sqc2 $vf2, 0(%3)"
            :
            : "r"(key), "r"(key + 1), "f"(fraction), "r"(dest)
            : "$8", "$9", "$10", "$11", "memory");
        dest->w = 1.0f;
    } else {
        sefLerpVectorA(key, dest);
    }
    return 1;
}

/*
 * sefLerpVectorSC (main:0x002e1908), re-treated from the accepted assembly
 * group src/main/sef/sef_lerp_vector_sc.s toward readable C with constrained
 * ee-vu-cop2 inline assembly (route ee-vu-cop2, docs/ps2-capabilities.md).
 *
 * Forwards its own arguments to the still-unrecovered sefLerpVector, then
 * when its status is nonzero scales the XYZ lanes of the second four-slot
 * vector (the caller's own vectors pointer advanced by one slot) in place by
 * the shared 0.1 effect-scale literal (config/units/main-sef-lerp-vector-sc.json,
 * effect_scale_literal_0_1 @ 0x004d82dc), leaving its W lane untouched. The
 * original has no .globl, so the emitted binding stays LOCAL.
 *
 * The scale read is placed unconditionally, before the status test, matching
 * the original's own instruction order (lwc1 before beq). 2.96's scheduler
 * otherwise sinks that load past the branch into the taken arm since nothing
 * else forces it to stay put; an explicit empty compiler barrier right after
 * the read (before the branch) blocks that motion without changing any value
 * (the instruction-free scheduling barrier, user-authorized 2026-09-13:
 * config/compiler-patterns.json CP-0171 -- an empty `__asm__ __volatile__`
 * template with no output and no clobber emits no instruction, so it cannot
 * hide a hardware access; it only fixes the read's position for the
 * scheduler).
 */
static int sefLerpVectorSC(void *keys, Vector4 *vectors)
{
    int status = sefLerpVector(keys, vectors);
    float scale = 0.1f;

    __asm__ __volatile__("" : : "f"(scale));

    if (status != 0) {
        vectors++;

        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "mfc1 $8, %1\n\t"
            "qmtc2 $8, $vf2\n\t"
            "vmulx.xyz $vf1xyz, $vf1xyz, $vf2x\n\t"
            "sqc2 $vf1, 0(%0)"
            :
            : "r"(vectors), "f"(scale)
            : "$8", "memory"
        );
    }

    return status;
}

void sefLerpIVectorA(void *keys, Vector4 *dest)
{
    __asm__ __volatile__(
        "ldl $9, 7(%0)\n\t"
        "ldr $9, 0(%0)\n\t"
        "pcgth $10, $0, $9\n\t"
        "pextlh $10, $10, $9\n\t"
        "sq $10, 0(%1)\n\t"
        "nop"
        :
        : "r"(keys), "r"(dest)
        : "$9", "$10", "memory"
    );
}

void sefLerpIVector(SefKey5 *keys, SefProgressState *state)
{
    Vector4 *dest = (Vector4 *)((char *)state + 0x10);
    SefKey5 *key;
    float fraction;

    key = keys + state->key_index;
    if (key[1].end_frame == 1024) {
        return;
    }
    key = sefProgressKey5(keys, state);
    if (key->end_frame != key[1].end_frame) {
        fraction = (float)(state->frame - key->end_frame) / (float)(key[1].end_frame - key->end_frame);
        __asm__ __volatile__(
            "ldl $8, 7(%0)\n\t"
            "ldr $8, 0(%0)\n\t"
            "ldl $9, 7(%1)\n\t"
            "ldr $9, 0(%1)\n\t"
            "mfc1 $10, %2\n\t"
            "qmtc2.ni $10, $vf3\n\t"
            "pcgth $10, $0, $8\n\t"
            "pcgth $11, $0, $9\n\t"
            "pextlh $10, $10, $8\n\t"
            "pextlh $11, $11, $9\n\t"
            "qmtc2.ni $10, $vf1\n\t"
            "qmtc2.ni $11, $vf2\n\t"
            "vitof0.xyzw $vf1, $vf1\n\t"
            "vitof0.xyzw $vf2, $vf2\n\t"
            "vsub.xyz $vf2, $vf2, $vf1\n\t"
            "vmulx.xyz $vf2, $vf2, $vf3x\n\t"
            "vadd.xyz $vf2, $vf2, $vf1\n\t"
            "vftoi0.xyzw $vf2, $vf2\n\t"
            "sqc2 $vf2, 0(%3)"
            :
            : "r"((char *)key + 2), "r"((char *)key + 12), "f"(fraction), "r"(dest)
            : "$8", "$9", "$10", "$11", "memory");
        return;
    }
    sefLerpIVectorA((char *)key + 2, dest);
}

#define SK(k, n) (((short *)(k))[n])

void sefLerpIVector2(SefKey5 *keys, SefProgressState *state)
{
    union { int i; } *dest = (void *)((char *)state + 0x10);
    SefKey5 *key;
    int t;
    int w;

    key = keys + state->key_index;
    if (key[1].end_frame != 1024
        && (key = sefProgressKey5(keys, state), key->end_frame != key[1].end_frame)) {
        t = ((state->frame - key->end_frame) << 16) / (key[1].end_frame - key->end_frame);
        dest[0].i = ((SK(key, 6) - SK(key, 1)) * t >> 16) + SK(key, 1);
        dest[1].i = ((SK(key, 7) - SK(key, 2)) * t >> 16) + SK(key, 2);
        dest[2].i = ((SK(key, 8) - SK(key, 3)) * t >> 16) + SK(key, 3);
        w = ((SK(key, 9) - SK(key, 4)) * t >> 16) + SK(key, 4);
        w = -w;
        dest[3].i = w;
    } else {
        dest[0].i = SK(key, 1);
        dest[1].i = SK(key, 2);
        dest[2].i = SK(key, 3);
        dest[3].i = -SK(key, 4);
    }
}

void sefLerpFloat(SefKey3 *keys, SefProgressState *state)
{
    SefKey3 *key;
    int frame_delta;
    int frame_offset;
    float fraction;

    key = (SefKey3 *)((char *)keys + state->key_index * 6);
    if (((SefKey3 *)((char *)key + 6))->frame != 1024) {
        sefProgressKey3(keys, state);
        key = (SefKey3 *)((char *)keys + state->key_index * 6);
        if (key->frame != ((SefKey3 *)((char *)key + 6))->frame) {
            frame_delta = ((SefKey3 *)((char *)key + 6))->frame - key->frame;
            frame_offset = state->frame - key->frame;
            fraction = (float)frame_offset / (float)frame_delta;
            *(float *)&state->value = ((float)(((SefKey3 *)((char *)key + 6))->value - key->value) * fraction + (float)key->value) * 0.01f;
            return;
        }
    }
    *(float *)&state->value = (float)key->value * 0.01f;
}

void sefLerpInt(SefKey3 *keys, SefProgressState *state)
{
    SefKey3 *key;
    int frame_delta;
    int frame_offset;
    int key_index;
    float fraction;

    key = (SefKey3 *)((char *)keys + state->key_index * 6);
    if (key->frame == 1024) {
        state->value = key->value;
    } else if (((SefKey3 *)((char *)key + 6))->frame == 1024) {
        state->value = key->value;
    } else {
        sefProgressKey3(keys, state);
        key_index = state->key_index;
        key = (SefKey3 *)((char *)keys + key_index * 6);
        if (key->frame != ((SefKey3 *)((char *)key + 6))->frame) {
            frame_delta = ((SefKey3 *)((char *)key + 6))->frame - key->frame;
            frame_offset = state->frame - key->frame;
            fraction = (float)frame_offset / (float)frame_delta;
            state->value = (short)((float)(((SefKey3 *)((char *)key + 6))->value - key->value) * fraction + (float)key->value);
        } else {
            state->value = key->value;
        }
    }
}

static void sefProgressInt(SefKey3 *keys, SefProgressState *state)
{
    sefProgressKey3(keys, state);
    state->value = ((SefKey3 *)((char *)keys + state->key_index * 6))->value;
}

extern int ACT_jointGetAccessories(void *actor, unsigned int id);
static inline int sefMatrixHasOffset(unsigned char *source)
{
    return (*(int *)(source + 8) | *(long long *)source) != 0;
}

/*
 * sefGetMotionNullMatrix (main:0x002e1de0): copies the actor's accessory
 * joint matrix (`id` + 8) into `matrix`; falls back to the actor's own
 * rotation/translation/scale when there is no joint matrix or its
 * translation row is zero. A null actor gives the identity matrix.
 */
void sefGetMotionNullMatrix(Matrix4 *matrix, unsigned char *actor, unsigned int id)
{
    int joint;
    int mats;

    if (actor != 0) {
        joint = ACT_jointGetAccessories(actor, id + 8);
        if (joint >= 0) {
            mats = *(int *)(actor + 0x824);
            if (mats != 0) {
                __asm__ __volatile__(
                    "lq $8, 0(%0)\n\t"
                    "lq $9, 16(%0)\n\t"
                    "lq $10, 32(%0)\n\t"
                    "lq $11, 48(%0)\n\t"
                    "sq $8, 0(%1)\n\t"
                    "sq $9, 16(%1)\n\t"
                    "sq $10, 32(%1)\n\t"
                    "sq $11, 48(%1)"
                    :
                    : "r"((joint << 6) + mats), "r"(matrix)
                    : "$8", "$9", "$10", "$11", "memory");
                if (sefMatrixHasOffset((unsigned char *)matrix + 0x30)) {
                    return;
                }
            }
        }
        sefCalcRotTransSMatrix((Vector4 *)(actor + 0x50), (Vector4 *)(actor + 0x10),
                               (Vector4 *)(actor + 0x60), matrix);
        return;
    }
    /* Identity: rotate $vf0 = (0,0,0,1) one lane at a time, rows back to front. */
    __asm__ __volatile__(
        "vmr32.xyzw $vf1xyzw, $vf0xyzw\n\t"
        "vmr32.xyzw $vf2xyzw, $vf1xyzw\n\t"
        "vmr32.xyzw $vf3xyzw, $vf2xyzw\n\t"
        "sqc2 $vf0, 48(%0)\n\t"
        "sqc2 $vf1, 32(%0)\n\t"
        "sqc2 $vf2, 16(%0)\n\t"
        "sqc2 $vf3, 0(%0)"
        :
        : "r"(matrix)
        : "memory");
}

extern int ACT_jointGetAccessories(void *actor, unsigned int id);

void sefGetNullPosition(SefQuadBlock *out, void *actor, unsigned int id, int flag)
{
    int joint;
    int mats;

    if (actor != 0 && id < 16) {
        joint = ACT_jointGetAccessories(actor, id + 8);
        mats = *(int *)((char *)actor + 0x824);
        if (joint < 0 || mats == 0) {
            ((float *)out)[1] = 20.0f;
        } else {
            __asm__ __volatile__(
                "lq $8, 0(%0)\n\t"
                "sq $8, 0(%1)"
                :
                : "r"((joint << 6) + mats + 0x30), "r"(out)
                : "$8", "memory");
        }
    }
}

void sefGetWeaponPosition(SefQuadBlock *out, void *actor, unsigned int id)
{
    int joint;
    int mats;

    if (actor != 0 && id < 8) {
        joint = ACT_jointGetAccessories(actor, id);
        mats = *(int *)((char *)actor + 0x824);
        if (joint < 0 || mats == 0) {
            ((float *)out)[1] = 20.0f;
        } else {
            __asm__ __volatile__(
                "lq $8, 0(%0)\n\t"
                "sq $8, 0(%1)"
                :
                : "r"((joint << 6) + mats + 0x30), "r"(out)
                : "$8", "memory");
        }
    }
}

extern void *MMathApplyMatrix(Vector4 *destination, void *matrix, Vector4 *point);
extern unsigned char D_00795100[];
static inline short sefPointHasOffset(unsigned char *source)
{
    return (*(int *)(source + 8) | *(long long *)source) != 0;
}
static inline void sefLoadPointMatrix(Vector4 *dst, unsigned char *rows, Vector4 *position)
{
    __asm__ __volatile__(
        "lq $8, 0(%0)\n\t"
        "lq $9, 0x10(%0)\n\t"
        "lq $10, 0x20(%0)\n\t"
        "lq $11, 0(%1)\n\t"
        "sq $8, 0(%2)\n\t"
        "sq $9, 0x10(%2)\n\t"
        "sq $10, 0x20(%2)\n\t"
        "sq $11, 0x30(%2)"
        : : "r"(rows), "r"(position), "r"(dst) : "$8", "$9", "$10", "$11", "memory");
}

void sefGetPoint(Vector4 *position, unsigned char *source, int target)
{
    Vector4 local[4] __attribute__((aligned(16)));
    unsigned char *matrix;
    unsigned char *battle;
    short offset;

    matrix = 0;
    offset = sefPointHasOffset(source);
    __asm__ __volatile__("sqc2 $vf0, 0(%0)" : : "r"(position) : "memory");
    battle = _battleData;
    if (*(int *)(battle + 0x224) & 2) {
        if ((unsigned int)(target - 0x101) < 0x10) {
            target += 0x700;
        }
    }
    if (target == 0x211) {
        target = (*(short *)(battle + 0xC) >= 2) ? 0x20F : 0x210;
    }
    if (target >= 4 && target != 0x800) {
        if (target == 0x100) {
            if (*(int *)battle != 0) {
                matrix = battle + 0x80;
            }
        } else if (target == 0x200) {
            if (*(int *)(battle + 4) != 0) {
                matrix = battle + 0xC0;
            }
        } else if (target & 0x100) {
            sefGetNullPosition((SefQuadBlock *)position, *(void **)battle, target - 0x101, 1);
            sefLoadPointMatrix(local, battle + 0x80, position);
            matrix = (unsigned char *)local;
        } else if (target & 0x200) {
            sefGetNullPosition((SefQuadBlock *)position, *(void **)(battle + 4), target - 0x201, 0);
            sefLoadPointMatrix(local, battle + 0xC0, position);
            matrix = (unsigned char *)local;
        } else if (!(target & 0x1000)) {
            if (target == 4) {
                unsigned char *parent;

                if (_parentLine >= 0 && _nowParentLocal[0] >= 0) {
                    parent = &_ptAlloc[_nowParentLocal[0] * 640];
                } else {
                    parent = 0;
                }
                if (parent != 0) {
                    __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(parent + 0xC0), "r"(position) : "$8", "memory");
                }
            } else if (target & 0x400) {
                sefGetWeaponPosition((SefQuadBlock *)position, *(void **)(battle + 8), target - 0x400);
                sefLoadPointMatrix(local, battle + 0x80, position);
                matrix = (unsigned char *)local;
            } else if (target == 5) {
                __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(D_00795100), "r"(position) : "$8", "memory");
            } else if (target & 0x800) {
                sefGetNullPosition((SefQuadBlock *)position, *(void **)battle, target - 0x801, 1);
                MMathApplyMatrix(position, battle + 0x80, position);
            }
        }
    }
    if (matrix != 0) {
        if (offset != 0) {
            MMathApplyMatrix(position, matrix, (Vector4 *)source);
        } else {
            __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(matrix + 0x30), "r"(position) : "$8", "memory");
        }
    } else {
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "lqc2 $vf2, 0(%1)\n\t"
            "vadd.xyz $vf1, $vf1, $vf2\n\t"
            "sqc2 $vf1, 0(%0)"
            : : "r"(position), "r"(source) : "memory");
    }
}

extern void sefGetPoint(Vector4 *position, unsigned char *source, int target);
static const float _zeroPos[4] __attribute__((aligned(16))) = {0.0f, 0.0f, 0.0f, 1.0f};
static const float _zeroAng[4] = {0.0f, 0.0f, 0.0f, 0.0f};

/*
 * sefGetPosition (main:0x002e2240): fills `position`'s XYZ via sefGetPoint
 * from `source` (or the shared zero-position table when source is NULL),
 * then forces the resulting vector's W lane to 1.0f.
 */
void sefGetPosition(Vector4 *position, unsigned char *source, int target) {
    unsigned char *point = source;

    if (point == 0) {
        point = (unsigned char *)_zeroPos;
    }
    sefGetPoint(position, point, target);
    position->w = 1.0f;
}

/*
 * sefGetCirclePos (main:0x002e2280): draws one random unit scalar, scales it
 * into a full-circle angle in radians (randf() * 360deg * (pi/180)), and asks
 * the VU0 macro-mode circle microprogram for that angle's two position
 * components (vcallms 0xe8, then vcallms 0x20 on the same angle), each
 * scaled by the caller's radius. Y is exact zero and W is exact 1.0f; there
 * is only one random draw, reused for both VU calls. Each block moves the
 * angle into the VU0 broadcast register and the result back out through
 * COP1 (mfc1/mtc1) around the qmtc2/vcallms/qmfc2.i transfer, exactly the
 * hardware handoff the original performs; the VU microprogram body itself is
 * outside this EE-side caller.
 */
static void sefGetCirclePos(Vector4 *pos, float radius)
{
    float angle = sefRandf() * 360.0f * 0.017453292f;
    float x_unit;
    float z_unit;

    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "qmtc2 $8, $vf4\n\t"
        "vcallms 0xe8\n\t"
        "qmfc2.i $8, $vf1\n\t"
        "mtc1 $8, %0\n\t"
        : "=f"(x_unit)
        : "f"(angle)
        : "$8", "memory"
    );
    pos->x = x_unit * radius;

    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "qmtc2 $8, $vf4\n\t"
        "vcallms 0x20\n\t"
        "qmfc2.i $8, $vf1\n\t"
        "mtc1 $8, %0\n\t"
        : "=f"(z_unit)
        : "f"(angle)
        : "$8", "memory"
    );

    pos->y = 0.0f;
    pos->w = 1.0f;
    pos->z = z_unit * radius;
}


static void sefGetSpherePos(Vector4 *pos, float radius)
{
    int vals[4];
    unsigned int t2;
    unsigned int t3;
    unsigned int t5;
    float half = 16384.0f;

    t2 = (unsigned int)sefRandSeed * 0x41C64E6D + 0x3039;
    vals[0] = (t2 >> 16) & 0x7FFF;
    t3 = t2 * 0x41C64E6D + 0x3039;
    vals[1] = (t3 >> 16) & 0x7FFF;
    t5 = t3 * 0x41C64E6D + 0x3039;
    sefRandSeed = (int)t5;
    vals[2] = (t5 >> 16) & 0x7FFF;

    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n\t"
        "mfc1 $2, %2\n\t"
        "vitof0.xyzw $vf1, $vf1\n\t"
        "qmtc2.ni $2, $vf2\n\t"
        "vsubx.xyz $vf1, $vf1, $vf2x\n\t"
        "vmul.xyz $vf2, $vf1, $vf1\n\t"
        "vaddy.x $vf2, $vf2, $vf2y\n\t"
        "vaddz.x $vf2, $vf2, $vf2z\n\t"
        "vsqrt Q, $vf2x\n\t"
        "vwaitq\n\t"
        "vaddq.x $vf2, $vf0, Q\n\t"
        "vdiv Q, $vf0w, $vf2x\n\t"
        "vmove.w $vf2, $vf0\n\t"
        "vwaitq\n\t"
        "vmulq.xyz $vf2, $vf1, Q\n\t"
        "mfc1 $2, %3\n\t"
        "qmtc2.ni $2, $vf1\n\t"
        "vmulx.xyz $vf2, $vf2, $vf1x\n\t"
        "sqc2 $vf2, 0(%0)\n\t"
        :
        : "r"(pos), "r"(vals), "f"(half), "f"(radius)
        : "$2", "memory");
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct SefBossDeathEffect {
    short character_id;
    unsigned short effect_no;
} SefBossDeathEffect;

typedef struct SefEffectControl {
    unsigned char unmodeled_00[4];
    short character_id;
    unsigned char unmodeled_06[2];
    short effect_no;
    unsigned char unmodeled_0a[0x40 - 0x0a];
} SefEffectControl;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct SefEffectScheduler {
    unsigned char unmodeled_000[0x6b0];
    int inUse;
    unsigned char unmodeled_6b4[0xa70 - 0x6b4];
    SefEffectControl effect;
} SefEffectScheduler;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct SefFloatProgressState {
    int key_index;
    int frame;
    float value;
} SefFloatProgressState;

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct SefPtAllocatorState {
    unsigned char records[0xA0000];
    short freeSlots[0x400];
    unsigned int head;
    unsigned int tail;
    unsigned char unmodeled_a0808[8];
} SefPtAllocatorState;

typedef struct SefBattleParameters {
    short player_character_ids[3];
    short enemy_character_ids[3];
    short enemy_effect_numbers[3];
    short player_setup_parameters[3][3];
    short player_active[3];
    short enemy_active[3];
    int has_boss;
} SefBattleParameters;

typedef struct {
    struct SefActor *targetActor;    /* +0x00 */
    struct SefActor *sourceActor;    /* +0x04 */
    unsigned char unmodeled_08[0x0c - 0x08];
    short phase;
    unsigned char unmodeled_0e[0x14 - 0x0e];
    short effectNo;                  /* +0x14 */
    short nusumeResult;              /* +0x16 */
    unsigned char unmodeled_18[0x70 - 0x18];
    struct SefParentRecord *parentRecord; /* +0x70 */
    int savedParameter;              /* +0x74 */
    unsigned char unmodeled_78[0x80 - 0x78];
    Matrix4 targetMatrix;            /* +0x80, identity, then the target actor's transform */
    Matrix4 sourceMatrix;            /* +0xc0, identity, then the source actor's transform */
    unsigned char unmodeled_100[0x214 - 0x100];
    int vectorActive;                /* +0x214 */
    unsigned char unmodeled_218[0x21c - 0x218];
    float effectSpeed;               /* +0x21c */
    unsigned char unmodeled_220[0x224 - 0x220];
    int flags;                       /* +0x224 */
    unsigned char unmodeled_228[0x22c - 0x228];
    int running;
} SefBattleStatus;

typedef struct {
    unsigned char unmodeled_00[0xf0];
    unsigned long long offsetXYBits;
    float offsetZ;
    float offsetW;
} SefLocalOffset;

typedef struct {
    unsigned char unmodeled_00[0x6b0];
    int inUse;
    unsigned char unmodeled_6b4[0xa74 - 0x6b4];
    short characterId;
    unsigned char unmodeled_a76[2];
    short effectNo;
    unsigned char unmodeled_a7a[0xab0 - 0xa7a];
} SefSchedulerRecord;

static static void sefGetCubePosBtm(Vector4 *pos, const int *extent)
{
    Vector4 extent_vector;
    Vector4 random_vector;
    Vector4 *random_position;
    unsigned int seed_1;
    unsigned int seed_2;
    unsigned int seed_3;
    int random_x;
    int random_y;
    int random_z;
    float random_scale;
    float final_scale;
    float half_scale;

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(extent) : "memory");
    __asm__ __volatile__("vitof0.xyzw vf1, vf1" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");
    random_vector.w = 0.0f;
    random_position = &random_vector;

    random_scale = 3.051851e-05f;
    seed_1 = sefRandSeed * 0x41C64E6Du + 0x3039u;
    seed_2 = seed_1 * 0x41C64E6Du + 0x3039u;
    seed_3 = seed_2 * 0x41C64E6Du + 0x3039u;
    sefRandSeed = seed_3;
    random_x = (int)((seed_1 >> 16) & 0x7FFF);
    random_y = (int)((seed_2 >> 16) & 0x7FFF);
    random_z = (int)((seed_3 >> 16) & 0x7FFF);
    random_vector.x = (float)random_x * random_scale;
    random_vector.y = (float)random_y * random_scale;
    random_vector.z = (float)random_z * random_scale;

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("vmul.xyz vf1, vf1, vf2\n\tsqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");
    extent_vector.y = 0.0f;

    half_scale = 0.5f;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("mfc1 $8, %0\n\tqmtc2 $8, vf2" : : "f"(half_scale) : "$8", "memory");
    __asm__ __volatile__("vmulx.xyz vf1, vf1, vf2x" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("vsub.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");

    final_scale = 0.1f;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(random_position) : "memory");
    __asm__ __volatile__("mfc1 $8, %0\n\tqmtc2 $8, vf2" : : "f"(final_scale) : "$8", "memory");
    __asm__ __volatile__("vmulx.xyz vf1, vf1, vf2x" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(pos) : "memory");
}

/*
 * sefGetCubePosBtm (main:0x002e23c8) is still INCLUDE_ASM in this TU;
 * declared here so sefGetCubePosTop (main:0x002e24c0) can call it. Both are
 * LOCAL in the original, so both stay static. `extent` is the cube's
 * integer extent vector (four ints, loaded with lqc2 and converted with
 * vitof0); the Top variant forwards it untouched.
 */
static void sefGetCubePosBtm(Vector4 *pos, const int *extent);

/*
 * sefGetCubePosTop (main:0x002e24c0): fills `pos` with the bottom corner via
 * sefGetCubePosBtm, then negates its Y component to mirror it to the top.
 */
static void sefGetCubePosTop(Vector4 *pos, const int *extent)
{
    sefGetCubePosBtm(pos, extent);
    pos->y = -pos->y;
}


static void sefGetCubePos(Vector4 *pos, const int *extent)
{
    Vector4 extent_vector;
    Vector4 random_vector;
    unsigned int seed_1;
    unsigned int seed_2;
    unsigned int seed_3;
    int random_x;
    int random_y;
    int random_z;
    float random_scale;
    float final_scale;
    float half_scale;

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(extent) : "memory");
    __asm__ __volatile__("vitof0.xyzw vf1, vf1" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");
    random_vector.w = 0.0f;

    random_scale = 3.051851e-05f;
    seed_1 = sefRandSeed * 0x41C64E6Du + 0x3039u;
    seed_2 = seed_1 * 0x41C64E6Du + 0x3039u;
    seed_3 = seed_2 * 0x41C64E6Du + 0x3039u;
    sefRandSeed = seed_3;
    random_x = (int)((seed_1 >> 16) & 0x7FFF);
    random_y = (int)((seed_2 >> 16) & 0x7FFF);
    random_z = (int)((seed_3 >> 16) & 0x7FFF);
    random_vector.x = (float)random_x * random_scale;
    random_vector.y = (float)random_y * random_scale;
    random_vector.z = (float)random_z * random_scale;

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("vmul.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");

    half_scale = 0.5f;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("mfc1 $8, %0\n\tqmtc2 $8, vf2" : : "f"(half_scale) : "$8", "memory");
    __asm__ __volatile__("vmulx.xyz vf1, vf1, vf2x" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&extent_vector) : "memory");

    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(&extent_vector) : "memory");
    __asm__ __volatile__("vsub.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");

    final_scale = 0.1f;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&random_vector) : "memory");
    __asm__ __volatile__("mfc1 $8, %0\n\tqmtc2 $8, vf2" : : "f"(final_scale) : "$8", "memory");
    __asm__ __volatile__("vmulx.xyz vf1, vf1, vf2x" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(pos) : "memory");
}

extern Matrix4 *MMathRotateMatrixX(Matrix4 *out, Matrix4 *matrix, float angle);
extern Matrix4 *MMathRotateMatrixZ(Matrix4 *out, Matrix4 *matrix, float angle);

static void sefGetOfsRange(Vector4 *position, const void *offset_table, int index)
{
    const int *offsets = offset_table;

    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(position) : "memory");

    if (index == 0) {
        float radius;

        if (!__builtin_expect(offsets[0], 0)) {
            return;
        }
        radius = (float)offsets[0] * 0.1f;
        sefGetSpherePos(position, radius * sefRandf());
        return;
    }
    if (index == 1) {
        if (!__builtin_expect(offsets[0], 0)) {
            return;
        }
        sefGetSpherePos(position, (float)offsets[0] * 0.1f);
        return;
    }
    if (index == 2) {
        sefGetCubePos(position, offsets);
        return;
    }
    if (index == 3) {
        sefGetCubePosTop(position, offsets);
        return;
    }
    if (index == 4) {
        sefGetCubePosBtm(position, offsets);
        return;
    }
    if (index == 5) {
        Matrix4 rotation;
        Vector4 circle_position;

        sefGetCirclePos(&circle_position, (float)offsets[0] * 0.1f);
        __asm__ __volatile__(
            "vmr32.xyzw vf1, vf0\n\t"
            "vmr32.xyzw vf2, vf1\n\t"
            "vmr32.xyzw vf3, vf2\n\t"
            "sqc2 vf0, 48(%0)\n\t"
            "sqc2 vf1, 32(%0)\n\t"
            "sqc2 vf2, 16(%0)\n\t"
            "sqc2 vf3, 0(%0)"
            :
            : "r"(&rotation)
            : "memory");

        if (offsets[1] != 0) {
            MMathRotateMatrixX(&rotation, &rotation, (float)offsets[1] * 0.017453292f);
        }
        if (offsets[2] != 0) {
            MMathRotateMatrixZ(&rotation, &rotation, (float)offsets[2] * 0.017453292f);
        }
        MMathApplyMatrix(position, &rotation, &circle_position);
        return;
    }
    if (index == 6) {
        unsigned int seed = sefRandSeed * 0x41C64E6Du;
        float range = (float)offsets[0];
        int sample;

        seed += 0x3039u;
        sample = (int)((seed >> 16) & 0x7FFFu);
        sefRandSeed = seed;
        sefGetCirclePos(position,
                        (float)sample * 3.051851e-05f * range * 0.1f);
        position->y = sefRandf() * (float)offsets[1] * 0.1f;
        return;
    }
    tracePrint("bad range prm : %d", index);
}

static void sefGetDirMatrix(Matrix4 *dest, Vector4 *dir)
{
    Vector4 neg;
    float angle[4];
    float len;

    __asm__ __volatile__(
        "lqc2 $vf1, 0(%0)\n\t"
        "vsub.xyz $vf2, $vf0, $vf1\n\t"
        "vmul.xz $vf4, $vf2, $vf2\n\t"
        "sqc2 $vf0, 0(%1)\n\t"
        "vaddz.x $vf3, $vf4, $vf4z\n\t"
        "vmove.y $vf2, $vf1\n\t"
        "vsqrt Q, $vf3x\n\t"
        "sqc2 $vf2, 0(%2)"
        :
        : "r"(dir), "r"(angle), "r"(&neg)
        : "memory");
    angle[1] = srsAtan2(neg.x, neg.z);
    __asm__ __volatile__(
        "vwaitq\n\t"
        "vaddq.x $vf1, $vf0, Q\n\t"
        "qmfc2.ni %0, $vf1"
        : "=r"(len)
        :
        : "memory");
    angle[0] = srsAtan2(neg.y, len);
    __asm__ __volatile__(
        "lqc2 $vf7, 0(%1)\n\t"
        "vaddx.x $vf4, $vf0, $vf7x\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.x $vf5, $vf0, $vf1x\n\t"
        "vaddy.x $vf4, $vf0, $vf7y\n\t"
        "vcallms 0xe8\n\t"
        "vaddx.y $vf5, $vf0, $vf1x\n\t"
        "vaddx.x $vf4, $vf0, $vf7x\n\t"
        "vcallms 0x20\n\t"
        "vaddx.x $vf6, $vf0, $vf1x\n\t"
        "vaddy.x $vf4, $vf0, $vf7y\n\t"
        "vcallms 0x20\n\t"
        "vaddx.y $vf6, $vf0, $vf1x\n\t"
        "vmove.xyzw $vf4, $vf0\n\t"
        "vmr32.xyzw $vf3, $vf4\n\t"
        "vmr32.xyzw $vf2, $vf3\n\t"
        "vmr32.xyzw $vf1, $vf2\n\t"
        "vmulay.xyzw ACC, $vf1, $vf5y\n\t"
        "vmsuby.xyzw $vf7, $vf3, $vf6y\n\t"
        "vmulay.xyzw ACC, $vf3, $vf5y\n\t"
        "vmaddy.xyzw $vf3, $vf1, $vf6y\n\t"
        "vmulax.xyzw ACC, $vf2, $vf5x\n\t"
        "vmaddx.xyzw $vf8, $vf3, $vf6x\n\t"
        "vmulax.xyzw ACC, $vf3, $vf5x\n\t"
        "vmsubx.xyzw $vf3, $vf2, $vf6x\n\t"
        "sqc2 $vf7, 0(%0)\n\t"
        "sqc2 $vf8, 0x10(%0)\n\t"
        "sqc2 $vf3, 0x20(%0)\n\t"
        "sqc2 $vf4, 0x30(%0)"
        :
        : "r"(dest), "r"(angle)
        : "memory");
}

/*
 * sefGetVecMatrix (main:0x002e2908): subtracts source vector B from source
 * vector A into a temporary direction vector (VU0 macro-mode vsub.xyz writes
 * only the XYZ lanes, so the temporary keeps source A's own W lane), then
 * builds the destination orientation matrix from that temporary via the
 * still-unrecovered sefGetDirMatrix helper. Only the subtraction is genuine
 * COP2/VU0 hardware; the call is ordinary C.
 */
void sefGetVecMatrix(Matrix4 *dest, Vector4 *source_a, Vector4 *source_b)
{
    Vector4 dir;

    __asm__ __volatile__(
        "lqc2 $vf1, 0(%0)\n\t"
        "lqc2 $vf2, 0(%1)\n\t"
        "vsub.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
        "sqc2 $vf1, 0(%2)\n\t"
        :
        : "r"(source_a), "r"(source_b), "r"(&dir)
        : "memory"
    );
    sefGetDirMatrix(dest, &dir);
}

/*
 * sefGetDirVector (main:0x002e2938): a single VU0 macro-mode sequence with
 * no general-purpose instruction of its own (all 7 hardware operations,
 * leaf, no calls; sefGetSpeed is the only caller). Negates `source`'s Z
 * lane against the VU0 architectural constant vf0 (0,0,0,1), accumulates it
 * against the caller's own vf7/vf8/vf3/vf4 basis (ACC = vf7*x + vf8*y +
 * vf3*(-z), then vf1 = ACC + vf4*w) and stores the four-lane result to
 * `dest`. vf7/vf8/vf3/vf4 are whatever sefGetSpeed already loaded into VU0
 * before this call; this function neither reads nor writes them as C state.
 */
static void sefGetDirVector(void *dest, void *source) {
    __asm__ __volatile__("lqc2 vf1, 0(%0)\n\tvsub.z vf1, vf0, vf1\n\tvmulax.xyzw ACC, vf7, vf1x\n\tvmadday.xyzw ACC, vf8, vf1y\n\tvmaddaz.xyzw ACC, vf3, vf1z\n\tvmaddw.xyzw vf1, vf4, vf1w\n\tsqc2 vf1, 0(%1)\n\tnop" :  : "r"(source), "r"(dest) : "memory");
}

static void sefGetOfsRange(Vector4 *position, const void *offset_table, int index);

/*
 * SefParticleOffset (sefGetStartPos/sefGetTargetPos, main:0x002e2960 and
 * main:0x002e29b0): the only fields sefCreateParticle's effect record these
 * two functions evidence -- a start and a target position vector, and a
 * start and a target offset-table/index pair consumed opaquely by
 * sefGetOfsRange (still assembly). The bytes between and around them are
 * not evidenced by any accepted function in this TU.
 */
typedef struct SefParticleOffset {
    unsigned char unmodeled_00[0x30];
    Vector4 start_position;                  /* +0x030 */
    unsigned char unmodeled_40[0x10];
    Vector4 target_position;                 /* +0x050 */
    unsigned char unmodeled_60[0x110];
    unsigned char start_offset_table[0x20];  /* +0x170 */
    unsigned char target_offset_table[0x20]; /* +0x190 */
    unsigned char unmodeled_1b0[0x24];
    short *start_index;                      /* +0x1D4 */
    unsigned char unmodeled_1d8[4];
    short *target_index;                     /* +0x1DC */
} SefParticleOffset;

/*
 * sefGetStartPos (main:0x002e2960): looks up the configured start offset
 * through sefGetOfsRange using the record's own start offset table and the
 * index its start_index points to, then adds the record's own
 * start_position vector into the caller's own position accumulator.
 * sefCreateParticle is its only caller.
 */
static void sefGetStartPos(Vector4 *position, SefParticleOffset *record) {
    sefGetOfsRange(position, record->start_offset_table, *record->start_index);
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&record->start_position) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(position) : "memory");
    __asm__ __volatile__("vadd.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(position) : "memory");
}

/*
 * sefGetTargetPos (main:0x002e29b0): identical in shape to sefGetStartPos
 * above, at the record's target fields instead of its start fields.
 * sefCreateParticle is also its only caller.
 */
static void sefGetTargetPos(Vector4 *position, SefParticleOffset *record) {
    sefGetOfsRange(position, record->target_offset_table, *record->target_index);
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&record->target_position) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(position) : "memory");
    __asm__ __volatile__("vadd.xyz vf1, vf1, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(position) : "memory");
}

extern void MMathSpecialVectorSub(Vector4 *destination, Vector4 *left, Vector4 *right, float scale);
extern void *MMathApplyMatrix(Vector4 *destination, void *matrix, Vector4 *point);

static void sefGetSpeed(Vector4 *speed_out, unsigned char *record, int type, int frame)
{
    Vector4 dir __attribute__((aligned(16)));
    Vector4 *velocity;
    float speed;

    speed = *(float *)(record + 0x270) + *(float *)(record + 0x150);
    velocity = (Vector4 *)(record + 0x200);
    if (speed == 0.0f) {
        __asm__ __volatile__("sqc2 $vf0, 0(%0)" : : "r"(speed_out) : "memory");
        return;
    }
    if (frame != 0 && type != 0) {
        MMathSpecialVectorSub(&dir, velocity, velocity, (float)frame);
    } else {
        __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(velocity), "r"(&dir) : "$8", "memory");
    }
    if (type >= 5) {
        sefGetVecMatrix((Matrix4 *)record, (Vector4 *)(record + 0xD0), (Vector4 *)(record + 0xC0));
        sefGetDirVector(&dir, &dir);
    } else if (type == 4) {
        unsigned char *parent;

        if (_parentLine >= 0 && _nowParentLocal[0] >= 0) {
            parent = &_ptAlloc[_nowParentLocal[0] * 640];
        } else {
            parent = 0;
        }
        if (parent != 0) {
            sefGetDirMatrix((Matrix4 *)record, (Vector4 *)(parent + 0x120));
            sefGetDirVector(&dir, &dir);
        }
    } else if (type == 3) {
        speed *= 10.0f;
        __asm__ __volatile__("lq $8, 0(%0)\n\tsq $8, 0(%1)" : : "r"(&dir), "r"(&dir) : "$8", "memory");
    } else if (type == 1) {
        if (*(int *)_battleData != 0) {
            MMathApplyMatrix(&dir, _battleData + 0x80, &dir);
        }
    }
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%0)\n\t"
        "mfc1 $8, %2\n\t"
        "qmtc2.ni $8, $vf2\n\t"
        "sqc2 $vf1, 0(%1)\n\t"
        "vmulx.xyz $vf1, $vf1, $vf2x\n\t"
        "sqc2 $vf1, 0(%3)"
        :
        : "r"(&dir), "r"(record + 0x120), "f"(speed), "r"(speed_out)
        : "$8", "memory");
}


void sefMoveParticle(unsigned char *particle, unsigned char *line, int frame)
{
    Vector4 speed __attribute__((aligned(16)));
    int target = **(short **)(line + 0x10);
    unsigned char *battle;
    float height;
    float scale;

    if (target == 0x211) {
        unsigned char *b = _battleData;

        target = (*(short *)(b + 0xC) >= 2) ? 0x20F : 0x210;
    }
    battle = _battleData;
    if (*(int *)(battle + 0x22C) != 0) {
        if ((unsigned int)(target - 0x101) < 0x10) {
            target += 0x700;
        }
        if ((_nowScheduler->flags & 0x1400) == 0x1000) {
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "lq $9, 16(%1)\n\t"
                "lq $10, 32(%1)\n\t"
                "lq $11, 48(%1)\n\t"
                "sq $8, 0(%0)\n\t"
                "sq $9, 16(%0)\n\t"
                "sq $10, 32(%0)\n\t"
                "sq $11, 48(%0)"
                :
                : "r"(particle + 0x40), "r"(battle + 0x180)
                : "$8", "$9", "$10", "$11", "memory");
            *(float *)(particle + 0x154) = *(float *)(battle + 0x220);
        }
    }
    if (*(signed char *)(particle + 0x163) == 0) {
        sefGetSpeed(&speed, particle, target, frame);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "lqc2 $vf2, 0(%1)\n\t"
            "vadd.xyz $vf1, $vf1, $vf2\n\t"
            "sqc2 $vf1, 0(%0)"
            :
            : "r"(particle + 0xC0), "r"(&speed)
            : "memory");
    }
    *(float *)(particle + 0x114) -= _gravity;
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%0)\n\t"
        "lqc2 $vf2, 0(%1)\n\t"
        "vadd.xyz $vf1, $vf1, $vf2\n\t"
        "sqc2 $vf1, 0(%0)"
        :
        : "r"(particle + 0xC0), "r"(particle + 0x110)
        : "memory");
    if (_colision != 0.0f && *(float *)(particle + 0xC4) <= _height) {
        if (*(signed char *)(particle + 0x163) == 0) {
            __asm__ __volatile__(
                "lqc2 $vf1, 0(%0)\n\t"
                "lqc2 $vf2, 0(%1)\n\t"
                "vadd.xyz $vf1, $vf1, $vf2\n\t"
                "sqc2 $vf1, 0(%1)"
                :
                : "r"(&speed), "r"(particle + 0x110)
                : "memory");
        }
        *(signed char *)(particle + 0x163) = 1;
        height = _height;
        scale = _colision;
        *(float *)(particle + 0xC4) = height;
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf2\n\t"
            "vmulx.xyz $vf1, $vf1, $vf2x\n\t"
            "sqc2 $vf1, 0(%0)"
            :
            : "r"(particle + 0x110), "f"(scale)
            : "$8", "memory");
        if (*(float *)(particle + 0x114) < 0.0f) {
            *(float *)(particle + 0x114) = -*(float *)(particle + 0x114);
        }
    }
}

/*
 * sefCalcLocalMatrix (main:0x002e2d48), re-treated from the accepted assembly
 * group src/main/sef/sefCalcLocalMatrix.s toward readable C with constrained
 * ee-vu-cop2 inline assembly (route ee-vu-cop2, docs/ps2-capabilities.md).
 * The second function of that group, sefCalcRotChange, converted the same
 * way further down this file; the group now holds no tracked assembly.
 *
 * Names recovered from config/symbols/main.txt: _battleData (0x00794110),
 * _parentLine, _nowParentLocal, _ptAlloc. The battle state block and the
 * scheduler local record are read through narrowly evidenced scalar accesses
 * rather than through structs with invented members: only the fields below
 * are evidenced, the bytes between them are not, and docs/naming.md forbids
 * inventing padding members to close the gaps.
 *   _battleData + 0x00c  short  battle phase compared against 2
 *   _battleData + 0x22c  int    "battle is running" flag
 *   _battleData + 0x200         the matrix source both callees take
 *   record     + 0x000         the local matrix this call produces
 *   record     + 0x080         the base matrix it is multiplied by
 *   record     + 0x0c0         the rotation vector
 *   record     + 0x0f0/0x0f8   the offset the multiply is gated on
 *
 * record is kept as unsigned char* (its full layout is not recovered, only
 * these offsets are evidenced), and sefGetDirMatrix/sefGetVecMatrix are
 * called through Matrix4 and Vector4 pointer casts of record's own address to match
 * their published prototypes (src/main/sef.h): both take the destination
 * matrix at offset 0, so the cast is exactly what an already-Matrix4-typed
 * `record` would give the callee, with no different bytes read or written.
 */
#define SEF_BATTLE_PHASE         0x00c
#define SEF_BATTLE_MATRIX_SOURCE 0x200
#define SEF_BATTLE_RUNNING       0x22c
#define SEF_LOCAL_MATRIX         0x000
#define SEF_LOCAL_BASE_MATRIX    0x080
#define SEF_LOCAL_ROTATION       0x0c0
#define SEF_LOCAL_OFFSET         0x0f0
#define SEF_LOCAL_OFFSET_Z       0x0f8
#define SEF_PARENT_DIR_MATRIX    0x120

/* _battleData + 0x200, still owned by the scaffold, so it keeps the original
 * object's own name for the address (docs/naming.md). The two callees below
 * materialise the same address differently, which is why both spellings are
 * here: the flag path reaches it from the block base it already holds. */
extern unsigned char D_00794310[];

extern void MMathMulMatrix(void *destination, void *left, void *right);

static void sefCalcLocalMatrix(unsigned char *record, int target, int previous_target)
{
    unsigned char *battle = _battleData;
    int in_battle = *(int *)(battle + SEF_BATTLE_RUNNING);

    target &= ~0xe000;
    if (in_battle != 0 && (unsigned int)(target - 257) < 16) {
        target += 1792;
    }
    if (target == 529) {
        target = (*(short *)(battle + SEF_BATTLE_PHASE) >= 2) ? 527 : 528;
    }

    if (target == 4) {
        unsigned char *parent;

        if (_parentLine >= 0 && _nowParentLocal[0] >= 0) {
            parent = &_ptAlloc[_nowParentLocal[0] * 640];
        } else {
            parent = 0;
        }
        if (parent != 0 && target != previous_target) {
            sefGetDirMatrix((Matrix4 *)record, (Vector4 *)(parent + SEF_PARENT_DIR_MATRIX));
        }
    } else if (target == 1) {
        /* The base matrix becomes the local one: the four quadword moves the
         * original performs inline through the EE's own scratch GPRs. */
        __asm__ __volatile__(
            "lq $8, 0(%0)\n\t"
            "lq $9, 16(%0)\n\t"
            "lq $10, 32(%0)\n\t"
            "lq $11, 48(%0)\n\t"
            "sq $8, 0(%1)\n\t"
            "sq $9, 16(%1)\n\t"
            "sq $10, 32(%1)\n\t"
            "sq $11, 48(%1)"
            :
            : "r"(record + SEF_LOCAL_BASE_MATRIX), "r"(record)
            : "$8", "$9", "$10", "$11", "memory");
        return;
    } else if (in_battle != 0 && (target & 0x200) != 0) {
        Vector4 direction;

        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "lqc2 $vf2, 0(%1)\n\t"
            "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
            "sqc2 $vf1, 0(%2)"
            :
            : "r"(record + SEF_LOCAL_ROTATION), "r"((unsigned char *)_nowScheduler + 0x80),
              "r"(&direction)
            : "memory");
        sefGetVecMatrix((Matrix4 *)record, (Vector4 *)(battle + SEF_BATTLE_MATRIX_SOURCE),
                        &direction);
    } else if (target >= 5) {
        sefGetVecMatrix((Matrix4 *)record, (Vector4 *)D_00794310,
                        (Vector4 *)(record + SEF_LOCAL_ROTATION));
    } else {
        /* The identity matrix, built by rotating $vf0 = (0,0,0,1) one lane at
         * a time and storing the rows back to front. */
        __asm__ __volatile__(
            "vmr32.xyzw $vf1xyzw, $vf0xyzw\n\t"
            "vmr32.xyzw $vf2xyzw, $vf1xyzw\n\t"
            "vmr32.xyzw $vf3xyzw, $vf2xyzw\n\t"
            "sqc2 $vf0, 48(%0)\n\t"
            "sqc2 $vf1, 32(%0)\n\t"
            "sqc2 $vf2, 16(%0)\n\t"
            "sqc2 $vf3, 0(%0)"
            :
            : "r"(record)
            : "memory");
    }

    if (*(long long *)(record + SEF_LOCAL_OFFSET) != 0 ||
            *(float *)(record + SEF_LOCAL_OFFSET_Z) != 0.0f) {
        MMathMulMatrix(record, record, record + SEF_LOCAL_BASE_MATRIX);
    }
}

/*
 * sefCalcRotChange (main:0x002e2f00), re-treated from the accepted assembly
 * group src/main/sef/sefCalcRotChange.s (itself the re-cut remainder of
 * src/main/sef/sefCalcLocalMatrix.s once its sibling sefCalcLocalMatrix
 * converted, config/units/math-correction13-5.json) toward readable C with
 * constrained ee-vu-cop2 inline assembly (route ee-vu-cop2,
 * docs/ps2-capabilities.md). The group now holds no assembly of its own.
 *
 * record keeps the offsets sefCalcLocalMatrix's own comment already
 * evidences, plus what this function itself reads and writes:
 *   record + 0x040   the source matrix MMathRotateMatrixXYZ multiplies by
 *                    when the masked mode selects it
 *   record + 0x080   SEF_LOCAL_BASE_MATRIX, the matrix every path here
 *                    rotates in place and sefCalcLocalMatrix later reads
 *   record + 0x0e0   the accumulated offset before this frame's change
 *   record + 0x0f0/0x0f4/0x0f8
 *                    SEF_LOCAL_OFFSET, the combined offset this call
 *                    produces and sefCalcLocalMatrix gates its multiply on.
 *                    The three components are the x/y/z of one Vector4, so
 *                    the single-axis rotations below read them as fields of
 *                    `(Vector4 *)(record + SEF_LOCAL_OFFSET)` rather than as
 *                    three separate offsets; SEF_LOCAL_OFFSET_Z stays for
 *                    sefCalcLocalMatrix, which reads +0x0f8 on its own
 *   record + 0x1c0   the per-frame rotation delta: degrees on entry,
 *                    converted to radians in place by the first block
 *
 * flags selects the rotation order: masking off bits 0x6000 and comparing to
 * 1 picks the XYZ path (Y/Z/X one axis at a time when bit 0x4000 is also set,
 * or one MMathRotateMatrixXYZ call over the source matrix otherwise); with
 * that not selected, bit 0x6000 clear takes MMathRotateMatrixYXZ, and
 * otherwise bit 0x4000 takes the same Y/Z/X order and bit 0x2000 takes
 * MMathRotateMatrixXYZ again, this time from the identity default and the
 * combined offset. update disables all of it: only the vector scale and
 * offset accumulation happen. MMathRotateMatrixX/Y/Z/XYZ/YXZ are main/tu219
 * (src/main/m_math.c); a null matrix argument here reaches their own
 * null-default idiom (docs/tu-worker.md), not a call site concern.
 *
 * The degrees-to-radians factor is D_004D8318, a second occurrence of the
 * same .lit4 constant sefDeg2RadVector reads as lit4_004d8354 above: EE GCC
 * 2.96 does not merge identical .lit4 words across call sites, so each user
 * gets its own address and its own extern (docs/naming.md, "Scaffold-owned
 * data keeps its splat name").
 *
 * MMathRotateMatrixX/Y/Z/XYZ/YXZ are declared with a non-const Matrix4 *
 * `matrix` parameter, matching MMathMulMatrix's own cross-TU declaration
 * above rather than m_math.c's `const Matrix4 *`: EE GCC 2.96 warns passing
 * a plain Matrix4 pointer (or a literal 0) into a `const Matrix4 *` array
 * parameter, and dropping the const here does not change what is read.
 */
#define SEF_ROTCHANGE_SRC_MATRIX  0x040
#define SEF_ROTCHANGE_OFFSET_BASE 0x0e0
#define SEF_ROTCHANGE_DELTA       0x1c0

extern Matrix4 *MMathRotateMatrixX(Matrix4 *out, Matrix4 *matrix, float angle);
extern Matrix4 *MMathRotateMatrixY(Matrix4 *out, Matrix4 *matrix, float angle);
extern Matrix4 *MMathRotateMatrixZ(Matrix4 *out, Matrix4 *matrix, float angle);
extern Matrix4 *MMathRotateMatrixXYZ(Matrix4 *out, Matrix4 *matrix, Vector4 *angles);
extern Matrix4 *MMathRotateMatrixYXZ(Matrix4 *out, Matrix4 *matrix, Vector4 *angles);

static void sefCalcRotChange(unsigned char *record, int flags, int update)
{
    __asm__ __volatile__("lqc2 $vf2, 0(%0)" : : "r"(record + SEF_ROTCHANGE_DELTA) : "memory");

    {
        register float degToRad asm("$f8") = 0.017453292f;

        __asm__ __volatile__(
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf1\n\t"
            "vmulx.xyz $vf2, $vf2, $vf1x\n\t"
            "sqc2 $vf2, 0(%0)"
            :
            : "r"(record + SEF_ROTCHANGE_DELTA), "f"(degToRad)
            : "$8", "memory");
    }

    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n\t"
        "lqc2 $vf2, 0(%2)\n\t"
        "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
        "sqc2 $vf1, 0(%0)"
        :
        : "r"(record + SEF_LOCAL_OFFSET), "r"(record + SEF_ROTCHANGE_OFFSET_BASE),
          "r"(record + SEF_ROTCHANGE_DELTA)
        : "memory");

    if (update == 0) {
        return;
    }

    if ((flags & 0xffff1fff) == 1) {
        if (flags & 0x4000) {
            MMathRotateMatrixY((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX), (Matrix4 *)0,
                               ((Vector4 *)(record + SEF_LOCAL_OFFSET))->y);
            MMathRotateMatrixZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                               (Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                               ((Vector4 *)(record + SEF_LOCAL_OFFSET))->z);
            MMathRotateMatrixX((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                               (Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                               ((Vector4 *)(record + SEF_LOCAL_OFFSET))->x);
        } else {
            MMathRotateMatrixXYZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                                 (Matrix4 *)(record + SEF_ROTCHANGE_SRC_MATRIX),
                                 (Vector4 *)(record + SEF_ROTCHANGE_DELTA));
        }
        return;
    }

    if ((flags & 0x6000) == 0) {
        MMathRotateMatrixYXZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX), (Matrix4 *)0,
                             (Vector4 *)(record + SEF_LOCAL_OFFSET));
        return;
    }

    if (flags & 0x4000) {
        MMathRotateMatrixY((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX), (Matrix4 *)0,
                           ((Vector4 *)(record + SEF_LOCAL_OFFSET))->y);
        MMathRotateMatrixZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                           (Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                           ((Vector4 *)(record + SEF_LOCAL_OFFSET))->z);
        MMathRotateMatrixX((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                           (Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX),
                           ((Vector4 *)(record + SEF_LOCAL_OFFSET))->x);
        return;
    }

    if (flags & 0x2000) {
        MMathRotateMatrixXYZ((Matrix4 *)(record + SEF_LOCAL_BASE_MATRIX), (Matrix4 *)0,
                             (Vector4 *)(record + SEF_LOCAL_OFFSET));
        return;
    }
}

typedef struct SefLightInput {
    int coordinates[3];
    int range;
} SefLightInput;

typedef struct SefKey5Prefix8 {
    unsigned int unmodeled_000[2];
    SefKey5 keys[1];
} SefKey5Prefix8;

struct SefKey5Table {
    unsigned int unmodeled_000;
    SefKey5 keys[1];
};

typedef struct SefKey3Table {
    unsigned int unmodeled_000;
    SefKey3 keys[1];
} SefKey3Table;

typedef struct SefModeKeyTable {
    short mode;
    unsigned char unmodeled_002[6];
    SefKey5 keys[1];
} SefModeKeyTable;

typedef unsigned int SefPackedAnimation __attribute__((mode(TI)));

typedef struct SefIntegerProgressState {
    int key_index;
    int frame;
    int value;
} SefIntegerProgressState;

typedef struct SefParticle {
    unsigned char unmodeled_000[0x040];
    Matrix4 transform;                               /* +0x040 */
    unsigned char unmodeled_080[0x0c0 - 0x080];
    Vector4 start_position;                           /* +0x0c0 */
    Vector4 target_position;                           /* +0x0d0 */
    Vector4 random_rotation;                           /* +0x0e0 */
    Vector4 rotated_position;                          /* +0x0f0 */
    float animated_color_xyz[3];                       /* +0x100 */
    float one_initialized_scalar;                     /* +0x10c */
    unsigned char unmodeled_110[0x130 - 0x110];
    Vector4 random_translation;                        /* +0x130 */
    union {
        SefPackedAnimation animated_scale;           /* +0x140 */
        int animated_scale_words[4];
    };
    float random_scale;                               /* +0x150 */
    float battle_scale;                               /* +0x154 */
    int kind;                                         /* +0x158 */
    short frame;                                      /* +0x15c */
    short state;                                      /* +0x15e */
    short animate_advance;                            /* +0x160 */
    unsigned char signal;                             /* +0x162 */
    unsigned char unmodeled_163[0x188 - 0x163];
    short rotation_state;                             /* +0x188 */
    unsigned char unmodeled_18a[0x1a0 - 0x18a];
    int color[4];                                     /* +0x1a0 */
    unsigned char unmodeled_1b0[0x1c0 - 0x1b0];
    Vector4 rotation;                                 /* +0x1c0 */
    unsigned char unmodeled_1d0[0x1e0 - 0x1d0];
    union {
        Vector4 color_scale;                          /* +0x1e0 */
        SefPackedAnimation color_scale_bits;
    };
    unsigned char unmodeled_1f0[0x200 - 0x1f0];
    Vector4 color_offset;                             /* +0x200 */
    unsigned char unmodeled_210[0x220 - 0x210];
    SefLightInput light;                              /* +0x220 */
    unsigned char unmodeled_230[0x240 - 0x230];
    Vector4 size;                                     /* +0x240 */
    struct {
        int key_index;
        int frame;
        int value;
    } first_integer_progress;                         /* +0x250 */
    struct {
        int key_index;
        int frame;
        int value;
    } second_integer_progress;                        /* +0x25c */
    SefFloatProgressState float_progress;             /* +0x268 */
} SefParticle;

typedef struct SefLineData {
    SefKey5Prefix8 *initial_vector_keys;
    SefModeKeyTable *rotation_vector_keys;
    SefKey5 *position_vector_keys;
    SefKey3Table *first_integer_keys_local;
    SefKey3 *second_integer_keys_local;
    SefKey5 *size_vector_keys;
    SefKey3Table *float_keys;
    SefKey5 *light_keys;
    unsigned short script_parameters[2];
    unsigned char unmodeled_024[0x0c];
    Vector4 global_position;                 /* +0x30 */
    unsigned char unmodeled_040[0x10];
    Vector4 parent_position;                 /* +0x50 */
    unsigned char unmodeled_060[0x40];
    Vector4 origin_offset;                   /* +0xa0 */
    Vector4 position;
    Vector4 position_delta;                  /* +0xc0 */
    unsigned char unmodeled_0D0[0x10];
    Vector4 scale_vector;
    Vector4 global_position_source;          /* +0xf0 */
    Vector4 rotation_vector;
    Vector4 parent_position_source;          /* +0x110 */
    Vector4 position_vector;
    unsigned char position_source[0x30];
    SefProgressState first_vector_progress;
    SefProgressState second_vector_progress;
    unsigned char unmodeled_1A0[0x0C];
    SefIntegerProgressState first_integer_progress;
    SefIntegerProgressState second_integer_progress;
    unsigned char unmodeled_1C4[0x0C];
    unsigned char *scale_keys;
    struct SefKey5Table *first_vector_keys;
    unsigned char *rotation_keys;
    struct SefKey5Table *second_vector_keys;
    SefModeKeyTable *key_1E0;
    unsigned char *key_1E4;
    struct SefKey3 *first_integer_keys;
    struct SefKey3 *second_integer_keys;
    unsigned char *key_1F0;
    short *position_keys;
    unsigned char unmodeled_1F8[0x400];
    float gravity;
    float height;
    short local_point_handles[0x100];                 /* +0x600 */
    unsigned char unmodeled_800[2];
    short global_result;
    short timer;
    unsigned short frame;
    short start_time;                        /* +0x808 */
    unsigned short loop_mode;
    unsigned short global_data_count;
    short field_80E;                         /* +0x80E */
    short link_index;                        /* +0x810 */
    unsigned short line_flags;               /* +0x812 */
    unsigned short local_result;
    short collision_progress;
    short global_data_state;                 /* +0x818 */
    unsigned char unmodeled_81A[6];
} SefLineData;

extern int D_00794334[];
static int sefFreeLocalData(unsigned char *line_data, unsigned int index);
static int sefAnimate(unsigned char *object, unsigned char *state, int advance);
static void sefAddLightActor(const Vector4 *position, const SefLightInput *light);

static int sefExecLineLocalData(SefLineData *line_data, unsigned short frame)
{
    SefParticle *particle;
    short *handle_pointer;
    int active_count = 0;
    int add_light = 0;
    SefKey5 *initial_vector_keys = line_data->initial_vector_keys != 0
        ? line_data->initial_vector_keys->keys : 0;
    SefKey5 *rotation_vector_keys = line_data->rotation_vector_keys != 0
        ? line_data->rotation_vector_keys->keys : 0;
    SefKey3 *float_keys = line_data->float_keys != 0
        ? line_data->float_keys->keys : 0;
    SefKey3 *first_integer_keys = line_data->first_integer_keys_local != 0
        ? line_data->first_integer_keys_local->keys : 0;
    SefKey5 *color_keys = line_data->key_1E0 != 0
        ? line_data->key_1E0->keys : 0;
    int loop_mode = line_data->rotation_vector_keys != 0
        ? line_data->rotation_vector_keys->mode : 0;
    int previous_mode = line_data->rotation_vector_keys != 0
        ? ((short *)line_data->rotation_vector_keys)[-1] : 0;
    int update_parent;
    int update_scaled_size;
    int update_rotation;
    int index;
    int component_result;
    int point_part;
    short reverse_direction;

    (void)frame;
    update_parent = (_nowScheduler->flags & 0x800) != 0 &&
                    (line_data->line_flags & 0x400) == 0;
    update_scaled_size = line_data->key_1E0->mode != 3 && line_data->size_vector_keys != 0;
    reverse_direction = _revDirZ;
    point_part = loop_mode & ~0xe000;
    if (point_part >= 5) {
        sefGetPoint((Vector4 *)D_00794310, (unsigned char *)_zeroPos, point_part);
    }

    index = 0;
    handle_pointer = line_data->local_point_handles;
    loop_mode = previous_mode != 0 ? loop_mode | 0x8000 : loop_mode;
    for (; index < 256; index++, handle_pointer++) {
        short handle = *handle_pointer;
        if (handle >= 0) {
            particle = (SefParticle *)&_ptAlloc[handle * 0x280];
            if (particle != 0) {
                particle->state++;
                if (particle->state >= 1024) {
                    particle->state = 1023;
                }
                if (particle->state <= particle->frame) {
                    component_result = 0;
                    update_rotation = particle->rotation_state != 4 || previous_mode != 0;
                    if (initial_vector_keys != 0) {
                        sefLerpIVector(initial_vector_keys,
                                       (SefProgressState *)((unsigned char *)particle + 0x190));
                    }
                    if (rotation_vector_keys != 0 &&
                        sefLerpVector(rotation_vector_keys,
                                      (SefProgressState *)((unsigned char *)particle + 0x1b0))) {
                        sefCalcRotChange((unsigned char *)particle, loop_mode, update_rotation);
                    }
                    if (line_data->position_vector_keys != 0) {
                        sefLerpIVector(line_data->position_vector_keys,
                                       (SefProgressState *)((unsigned char *)particle + 0x1d0));
                    }
                    if (first_integer_keys != 0) {
                        sefLerpInt(first_integer_keys,
                                   (SefProgressState *)&particle->first_integer_progress);
                    }
                    if (line_data->size_vector_keys != 0 &&
                        sefLerpVector(line_data->size_vector_keys,
                                      (SefProgressState *)((unsigned char *)particle + 0x230)) && update_scaled_size) {
                        __asm__ __volatile__(
                            "lqc2 $vf1, 0(%0)\n\t"
                            "mfc1 $8, %1\n\t"
                            "qmtc2 $8, $vf2\n\t"
                            "vmulx.xyz $vf1xyz, $vf1xyz, $vf2x\n\t"
                            "sqc2 $vf1, 0(%0)"
                            : : "r"(&particle->size), "f"(0.01f)
                            : "$8", "memory");
                    }
                    sefLerpFloat(float_keys, (SefProgressState *)&particle->float_progress);
                    if (sefLerpVector(color_keys,
                                      (SefProgressState *)((unsigned char *)particle + 0x1f0)) && reverse_direction != 0) {
                        particle->color_offset.z = -particle->color_offset.z;
                    }
                    if (line_data->light_keys != 0 && add_light == 0) {
                        unsigned long long light_xy;
                        sefLerpIVector2(line_data->light_keys,
                                        (SefProgressState *)((unsigned char *)particle + 0x210));
                        __builtin_memcpy(&light_xy, particle->light.coordinates, sizeof(light_xy));
                        if (light_xy != 0 || particle->light.coordinates[2] != 0) {
                            sefAddLightActor(&line_data->position, &particle->light);
                            add_light = 1;
                        }
                    }

                    _nowParentLocal[0] = particle->kind;
                    __asm__ __volatile__(
                        "lqc2 $vf21, 0(%0)\n\t"
                        "lqc2 $vf1, 0(%2)\n\t"
                        "lqc2 $vf3, 0(%3)\n\t"
                        "vmul.xyz $vf22, $vf21, $vf21\n\t"
                        "mfc1 $2, %4\n\t"
                        "qmtc2 $2, $vf2\n\t"
                        "vaddy.x $vf22x, $vf22x, $vf22y\n\t"
                        "vitof0.xyzw $vf1, $vf1\n\t"
                        "vaddz.x $vf22x, $vf22x, $vf22z\n\t"
                        "vmulx.xyz $vf1xyz, $vf1xyz, $vf2x\n\t"
                        "vsqrt Q, vf22x\n\t"
                        "vadd.xyz $vf3xyz, $vf1xyz, $vf3xyz\n\t"
                        "sqc2 $vf3, 0(%1)\n\t"
                        "vwaitq\n\t"
                        "vaddq.x vf22x, vf0x, Q\n\t"
                        "vdiv Q, vf0w, vf22x\n\t"
                        "vmove.w $vf22w, $vf0w"
                        :
                        : "r"(&particle->color_offset), "r"(particle->animated_color_xyz),
                          "r"(&particle->color), "r"(&particle->random_translation),
                          "f"(0.015625f)
                        : "$2", "memory");
                    /* Copy the packed integer scale lanes, then replace the
                     * fourth word with the animation value. */
                    particle->animated_scale = particle->color_scale_bits;
                    particle->animated_scale_words[3] = particle->first_integer_progress.value;
                    sefAnimate((unsigned char *)line_data, (unsigned char *)particle,
                               particle->animate_advance);
                    __asm__ __volatile__(
                        "vwaitq\n\t"
                        "vmulq.xyz vf22xyz, vf21xyz, Q\n\t"
                        "sqc2 $vf22, 0(%0)"
                        : : "r"(&particle->color_offset) : "memory");
                    if (line_data->second_integer_keys_local != 0) {
                        sefLerpInt(line_data->second_integer_keys_local,
                                   (SefProgressState *)&particle->second_integer_progress);
                        component_result = particle->second_integer_progress.value;
                    }
                    if (update_parent) {
                        __asm__ __volatile__(
                            "lqc2 $vf1, 0(%1)\n\t"
                            "lqc2 $vf2, 0(%2)\n\t"
                            "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n\t"
                            "sqc2 $vf1, 0(%0)"
                            : : "r"(&particle->start_position),
                                "r"(&particle->start_position),
                                "r"(&line_data->position_delta)
                            : "memory");
                    }
                    sefMoveParticle((unsigned char *)particle, (unsigned char *)&line_data->scale_keys,
                                    component_result);
                    if (update_rotation) {
                        sefCalcLocalMatrix((unsigned char *)particle, loop_mode,
                                           line_data->key_1E0->mode);
                    }
                    active_count++;
                } else {
                    particle->frame = 0;
                    sefFreeLocalData((unsigned char *)line_data, index);
                }
            }
        }
    }
    return (D_00794334[0] & 0x100) == 0 ? active_count : 1;
}

static void sevFreePtAllocator(unsigned int handle);

#define SEF_LOCAL_DATA_PT_TABLE 0x600

/*
 * sefFreeLocalData (main:0x002e34e0): releases the particle allocator slot
 * this line owns at index `index` (SEF_LOCAL_DATA_PT_TABLE, an array of
 * allocator handles) and clears it to -1. The slot is only touched when
 * `index` is valid (< 256); an out-of-range index still returns the byte
 * offset that would have been used, which neither caller
 * (sefExecLineLocalData, sefDestroyLocalData) treats as anything but a
 * non-(-1) sentinel.
 */
static int sefFreeLocalData(unsigned char *line_data, unsigned int index) {
    int valid = index < 0x100;
    int offset = (int)(index * 2) + SEF_LOCAL_DATA_PT_TABLE;

    if (valid) {
        short *slot = (short *)(line_data + offset);

        sevFreePtAllocator(*slot);
        offset = -1;
        *slot = -1;
    }
    return offset;
}

static int sefAllocLocalData(unsigned char *line)
{
    int i;
    int end;
    int alloc;

    i = *(short *)(line + 0x80E);
    end = (i - 1) & 0xFF;
    while (i != end) {
        if (*(short *)(line + 0x600 + i * 2) < 0) {
            break;
        }
        i = (i + 1) & 0xFF;
    }
    if (i == end) {
        return -1;
    }
    alloc = sevAllocPtAllocator();
    if (alloc < 0) {
        return -1;
    }
    *(short *)(line + 0x600 + i * 2) = alloc;
    *(short *)(line + 0x80E) = (i + 1) & 0xFF;
    return i;
}

static void sefDestroyLocalData(unsigned char *line_data) {
    short *p = (short *)(line_data + 0x600);
    int i;

    for (i = 0; i < 0x100; i++, p++) {
        if (*p >= 0) {
            sefFreeLocalData(line_data, i);
        }
    }
}

extern void *scGetImmAdrImmIdx2(int script, void *table, int index);
static inline float sefLineRandomOffset(short *range_word)
{
    int range = *range_word;
    unsigned int seed;
    int sample;

    if (range != 0) {
        seed = sefRandSeed * 0x41C64E6Du + 0x3039u;
        sefRandSeed = seed;
        sample = (seed >> 16) & 0x7fff;
        return (float)(sample % range - range / 2);
    }
    return 0.0f;
}

static void sefInitLineData(SefLineData *line_data, int start_time, int frame,
                            unsigned short *script_table, int link_index,
                            int global_data_count, int has_line_flag)
{
    struct SefLineScriptHeader {
        unsigned short table_indices[2];
        short gravity;
        short collision;
        short height;
    };
    struct SefLineScriptHeader *line_script = (struct SefLineScriptHeader *)script_table;
    unsigned short *key_table;
    unsigned short *line_table;
    short *scale_words;
    short gravity_input;
    short height_input;
    short collision_input;
    Vector4 *initial_position;
    int source_mode;
    SchedulerState *const *current_scheduler = &_nowScheduler;
    float vector_scale;

    key_table = (unsigned short *)scGetImmAdrImmIdx2(_nowScriptAdr, line_script->table_indices, 0);
    line_table = (unsigned short *)scGetImmAdrImmIdx2(_nowScriptAdr, line_script->table_indices, 1);
    gravity_input = line_script->gravity;
    collision_input = line_script->collision;
    height_input = line_script->height;

    sefMemZero(line_data, 0x820);
    memset(line_data->local_point_handles, -1, 0x200);

    if (start_time == 0x1000) {
        start_time = 0;
    }
    if (has_line_flag != 0) {
        line_data->line_flags = 0x401;
    } else {
        line_data->line_flags = 1;
    }

    line_data->timer = frame - start_time;
    line_data->link_index = link_index;
    line_data->start_time = start_time;
    line_data->loop_mode = frame;
    line_data->global_data_count = global_data_count;
    line_data->gravity = (float)gravity_input * 0.001f;
    line_data->collision_progress = collision_input;
    line_data->height = (float)height_input * 0.01f;
    line_data->field_80E = 0;
    line_data->local_result = 0;
    line_data->frame = 0;

    if ((*current_scheduler)->flags & 0x400) {
        line_data->line_flags |= 0x400;
    }

    line_data->scale_keys = (unsigned char *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 0);
    line_data->first_vector_keys = (struct SefKey5Table *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 1);
    line_data->rotation_keys = (unsigned char *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 2);
    line_data->second_vector_keys = (struct SefKey5Table *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 3);
    line_data->key_1E0 = (SefModeKeyTable *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 4);
    line_data->key_1E4 = (unsigned char *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 5);
    line_data->position_keys = (short *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 6);
    line_data->first_integer_keys = (struct SefKey3 *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 7);
    line_data->second_integer_keys = (struct SefKey3 *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 8);
    line_data->key_1F0 = (unsigned char *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 9);

    scale_words = (short *)line_data->scale_keys;
    line_data->origin_offset.x = sefLineRandomOffset(&scale_words[1]);
    line_data->origin_offset.y = sefLineRandomOffset(&scale_words[2]);
    line_data->origin_offset.z = sefLineRandomOffset(&scale_words[3]);

    if (line_data->first_vector_keys != 0 &&
        line_data->first_vector_keys->keys[1].end_frame == 0x400) {
        sefLerpIVectorA(line_data->first_vector_keys->keys[0].vector,
                        &line_data->first_vector_progress.vector);
    }
    if (line_data->second_vector_keys != 0 &&
        line_data->second_vector_keys->keys[1].end_frame == 0x400) {
        sefLerpIVectorA(line_data->second_vector_keys->keys[0].vector,
                        &line_data->second_vector_progress.vector);
    }

    if (line_data->scale_keys != 0 && ((short *)line_data->scale_keys)[9] == 0x400) {
        sefLerpVectorA(line_data->scale_keys + 8, &line_data->global_position_source);
        vector_scale = 0.1f;
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf2\n\t"
            "vmulx.xyz $vf1, $vf1, $vf2x\n\t"
            "sqc2 $vf1, 0(%0)"
            :
            : "r"(&line_data->global_position_source), "f"(vector_scale)
            : "$8", "memory");
    }
    if (line_data->rotation_keys != 0 && ((short *)line_data->rotation_keys)[9] == 0x400) {
        sefLerpVectorA(line_data->rotation_keys + 8, &line_data->parent_position_source);
        vector_scale = 0.1f;
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf2\n\t"
            "vmulx.xyz $vf1, $vf1, $vf2x\n\t"
            "sqc2 $vf1, 0(%0)"
            :
            : "r"(&line_data->parent_position_source), "f"(vector_scale)
            : "$8", "memory");
    }
    if (line_data->position_keys != 0 && line_data->position_keys[9] == 0x400) {
        sefLerpVectorA(line_data->position_keys + 4,
                       (Vector4 *)line_data->position_source);
        vector_scale = 0.1f;
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf2\n\t"
            "vmulx.xyz $vf1, $vf1, $vf2x\n\t"
            "sqc2 $vf1, 0(%0)"
            :
            : "r"((Vector4 *)line_data->position_source), "f"(vector_scale)
            : "$8", "memory");
    }

    line_data->script_parameters[0] = line_table[0];
    line_data->script_parameters[1] = line_table[1];
    line_data->initial_vector_keys = (SefKey5Prefix8 *)scGetImmAdrImmIdx2(_nowScriptAdr, line_table, 2);
    line_data->rotation_vector_keys = (SefModeKeyTable *)scGetImmAdrImmIdx2(_nowScriptAdr, line_table, 3);
    line_data->position_vector_keys = (SefKey5 *)scGetImmAdrImmIdx2(_nowScriptAdr, line_table, 4);
    line_data->first_integer_keys_local = (SefKey3Table *)scGetImmAdrImmIdx2(_nowScriptAdr, line_table, 5);
    line_data->size_vector_keys = (SefKey5 *)scGetImmAdrImmIdx2(_nowScriptAdr, line_table, 6);
    line_data->second_integer_keys_local = (SefKey3 *)scGetImmAdrImmIdx2(_nowScriptAdr, line_table, 7);
    line_data->light_keys = (SefKey5 *)scGetImmAdrImmIdx2(_nowScriptAdr, line_table, 8);
    line_data->float_keys = (SefKey3Table *)scGetImmAdrImmIdx2(_nowScriptAdr, key_table, 5);

    sefAnalyzeAnim((unsigned char *)line_data);

    initial_position = &line_data->global_position;
    source_mode = ((short *)line_data->scale_keys)[0];
    if (source_mode != 4) {
        sefGetPosition(initial_position,
                       (unsigned char *)&line_data->global_position_source,
                       source_mode);
    }
}

/*
 * sefAllocLineData (main:0x002e3bc8): scans the pool for the first unused
 * line-data record, marks its in-use halfword and returns its index, or -1
 * when every record is taken.
 */
static int sefAllocLineData(void)
{
    SefLineDataTable *lineData = (SefLineDataTable *)_lineData;
    SefLineDataEntry *entry = lineData->entries;
    int line_index;

    for (line_index = 0; line_index < SEF_LINE_DATA_COUNT; line_index++) {
        if (entry->inUse == 0) {
            entry->inUse = 1;
            return line_index;
        }
        entry++;
    }
    return -1;
}

typedef struct { unsigned char pad[0x812]; short used; unsigned char pad2[0xC]; } SefLineRec;

static inline unsigned char *sefGetLineAdr_inl(int line_index)
{
    if (line_index < 0) {
        return 0;
    }
    return &_lineData[line_index * 0x820];
}

static void sefFreeLineData(int index)
{
    if ((unsigned int)index < 0x80) {
        sefDestroyLocalData(sefGetLineAdr_inl(index));
        ((SefLineRec *)_lineData)[index].used = 0;
    }
}

typedef struct SefAnalyzeRecord {
    short time;
    short image_id;
    short alt_image_id;
    unsigned char unmodeled_06;
    signed char flag;
} SefAnalyzeRecord;
typedef struct SefImageItem {
    void *data;
    unsigned char unmodeled_04[0x08];
    unsigned short u12;
    unsigned char unmodeled_0e[4];
    unsigned short u18;
    unsigned short u20;
    unsigned short u22;
    unsigned short u24;
    unsigned short u26;
    unsigned short u28;
    unsigned short u30;
} SefImageItem;
extern void *svGetImageListItem(int type, int image_id);

static void sefAnalyzeAnim(unsigned char *object)
{
    SefAnalyzeRecord *records = *(SefAnalyzeRecord **)(object + 496);
    int accumulated = 0;
    int index = 0;
    float scale;

    if (records->image_id <= 0) {
        return;
    }
    scale = 0.005f;
    do {
        SefImageItem *item_a;
        SefImageItem *item_b;
        int type = *(short *)((unsigned char *)_nowScheduler + 2724);

        item_a = (SefImageItem *)svGetImageListItem(type, records[index].image_id);
        item_b = (SefImageItem *)svGetImageListItem(type, records[index].alt_image_id);
        if (item_a != 0 && item_b != 0) {
            int flag = records->flag;

            if (index < 32) {
                unsigned long long tex0;
                unsigned char *dest = (unsigned char *)((index << 5) + (int)object) + 512;
                int psm;

                *(float *)(dest + 8) = (float)item_a->u24 * scale;
                *(float *)(dest + 12) = (float)item_a->u26 * scale;
                *(short *)(dest + 16) = (short)item_a->u12;
                psm = (item_a->u20 == 8) ? 19 : 20;
                ((void **)dest)[-2] = item_a->data;
                ((void **)dest)[-1] = item_b->data;
                tex0 = (unsigned long long)item_a->u18 | ((unsigned long long)((unsigned int)item_a->u22 >> 6) << 14) | ((unsigned long long)psm << 20) | ((unsigned long long)item_a->u28 << 26) | ((unsigned long long)item_a->u30 << 30) | (1ULL << 34) | ((unsigned long long)item_b->u18 << 37) | (1ULL << 61);
                accumulated = (records[index].time != 1024) ? accumulated + records[index].time : 1024;
                *(unsigned long long *)(object + (index << 5) + 512) = tex0;
                *(int *)(dest + 20) = flag > 0;
                *(short *)(dest + 18) = (short)accumulated;
            }
        } else {
            memset(object + 504 + index * 32, 0, 32);
        }
        index++;
    } while (records[index].image_id > 0);
}

extern int D_00794334[];
typedef struct SefAnimKey {
    short frame;
    short unk2;
    short unk4;
    signed char next;
    signed char unk7;
} SefAnimKey;
typedef struct SefAnimFrame {
    long d[3];
    int i;
    short s[2];
} SefAnimFrame;

static int sefAnimate(unsigned char *effect, unsigned char *particle, int frame)
{
    int *current = (int *)(particle + 0x168);
    int index = *current;
    SefAnimKey *keys = *(SefAnimKey **)(effect + 0x1F0);
    SefAnimKey *key = &keys[index];
    int jumped;
    int changed = 0;
    int result;
    int flags = D_00794334[0];
    int next;
    int n;
    int i;
    int end;
    unsigned short keyframe;

    jumped = 0;
    result = 0;
    if (frame < 0 || (flags & 1)) {
        *(SefAnimFrame *)((unsigned char *)current + 8) = *(SefAnimFrame *)(effect + 0x1F8);
        *current = 0;
        *(unsigned short *)(particle + 0x160) = 0;
        return 0;
    }
    if (key->frame == 0x400) {
        return 0;
    }
    if (flags & 8) {
        i = index;
        end = 0x400;
        goto test;
    loop:
        i++;
        key = &keys[i];
    test:
        n = key->next;
        if (key->frame == end) {
            return 0;
        }
        if (n >= 0) {
            if (key[1].frame == end) {
                goto loop;
            }
            return 0;
        }
        *current = i;
        jumped = 1;
    }
found:
    if (jumped) {
        result = 1;
        changed = 1;
        *current += 1;
    } else if (frame >= *(short *)((unsigned char *)current + 0x22)) {
        next = key->next;
        if (next >= 0) {
            *current = next;
            result = 0x1000;
            if (next == 0) {
                *(unsigned short *)(particle + 0x160) = 0;
            } else {
                *(unsigned short *)(particle + 0x160) = *(unsigned short *)(effect + next * 32 + 0x1F2);
            }
        } else if (next == -1) {
            if (key[1].frame != 0x400) {
                result = 1;
                *current += 1;
            }

        } else if (next == -2) {
            result = 0;
        } else {
            if (key[1].frame == 0x400) {
                return 0;
            }
        }
        changed = 1;
    }
    if (changed) {
        *(SefAnimFrame *)((unsigned char *)current + 8) = *(SefAnimFrame *)(effect + *current * 32 + 0x1F8);
    }
    *(unsigned short *)(particle + 0x160) += 1;
    return result;
}


typedef struct SefLocalData {
    unsigned char *position_keys;                    /* +0x000 */
    unsigned char *spread_rotation_keys;             /* +0x004 */
    unsigned char *size_keys;                        /* +0x008 */
    unsigned char *signal_keys;                      /* +0x00c */
    unsigned char unmodeled_010[0x14 - 0x10];
    unsigned char *rotation_keys;                    /* +0x014 */
    unsigned char unmodeled_018[0x20 - 0x18];
    unsigned short initial_frame;                    /* +0x020 */
    short frame_count;                               /* +0x022 */
    unsigned char unmodeled_024[0x1e0 - 0x24];
    unsigned char *color_keys;                       /* +0x1e0 */
    unsigned char *uniform_scale_keys;               /* +0x1e4 */
    unsigned char unmodeled_1e8[SEF_LOCAL_DATA_PT_TABLE - 0x1e8];
    short pointHandles[0x100];                         /* +0x600 */
    unsigned char unmodeled_800[0x80e - 0x800];
    short cursor;                                      /* +0x80e */
} SefLocalData;
#define D_004D833C (0.009999999776482582f)
#define D_004D8340 (0.009999999776482582f)
#define D_004D8344 (3.0518509447574615e-05f)
#define D_004D8348 (3.0518509447574615e-05f)

int sefCreateParticle(SefLocalData *data, int kind)
{
    unsigned char *battle;
    unsigned char *params;
    SefParticle *particle;
    unsigned char *rotation_source_keys;
    int key_flag;
    Vector4 spread;
    Vector4 jitter;
    int count;
    int index;
    int result;
    int update;
    short angle;
    short handle;
    unsigned int seed_1;
    unsigned int seed_2;
    unsigned int seed_3;
    float random_x;
    float random_y;
    float random_z;
    float uniform_jitter;

    index = sefAllocLocalData((unsigned char *)data);
    params = (unsigned char *)data;
    result = -1;
    if (index >= 0) {
        handle = data->pointHandles[index];
        particle = (SefParticle *)(_ptAlloc + handle * 640);
        if (particle != 0) {
            sefMemZero(particle, 640);
            sefGetStartPos(&particle->start_position, (SefParticleOffset *)data);
            sefGetTargetPos(&particle->target_position, (SefParticleOffset *)data);
            particle->state = 0;
            particle->frame = data->initial_frame;
            count = data->frame_count;
            if (count > 0) {
                sefRandSeed = sefRandSeed * 0x41C64E6Du + 0x3039u;
                particle->frame += (short)((int)(((unsigned int)sefRandSeed >> 16) & 0x7FFF) % count);
            }
            sefAnimate(params, (unsigned char *)particle, -1);

            if (data->spread_rotation_keys != 0) {
                spread.x = (float)*(short *)(data->spread_rotation_keys + 2);
                spread.y = (float)*(short *)(data->spread_rotation_keys + 4);
                spread.z = (float)*(short *)(data->spread_rotation_keys + 6);
                jitter.x = sefRandf();
                jitter.y = sefRandf();
                jitter.z = sefRandf();
                jitter.w = 0.0f;
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%0)\n\t"
                    "lqc2 vf2, 0(%1)\n\t"
                    "vmul.xyz vf1, vf1, vf2\n\t"
                    "sqc2 vf1, 0(%0)"
                    : : "r"(&jitter), "r"(&spread) : "memory");
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%0)\n\t"
                    "mfc1 $8, %1\n\t"
                    "qmtc2 $8, vf2\n\t"
                    "vmulx.xyz vf1, vf1, vf2x\n\t"
                    "sqc2 vf1, 0(%0)"
                    : : "r"(&spread), "f"(0.5f) : "$8", "memory");
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%0)\n\t"
                    "lqc2 vf2, 0(%1)\n\t"
                    "vsub.xyz vf1, vf1, vf2\n\t"
                    "sqc2 vf1, 0(%2)\n\t"
                    "lqc2 vf2, 0(%2)"
                    : : "r"(&jitter), "r"(&spread), "r"(&particle->random_rotation) : "memory");
                {
                    register float degToRad asm("$f8") = 0.017453292f;

                    __asm__ __volatile__(
                        "mfc1 $8, %1\n\t"
                        "qmtc2 $8, vf1\n\t"
                        "vmulx.xyz vf2, vf2, vf1x\n\t"
                        "sqc2 vf2, 0(%0)"
                        : : "r"(&particle->random_rotation), "f"(degToRad) : "$8", "memory");
                }
                battle = _battleData;
                if (*(int *)(battle + 0x22C) != 0) {
                    __asm__ __volatile__(
                        "lq $8, 0(%1)\n\t"
                        "lq $9, 16(%1)\n\t"
                        "lq $10, 32(%1)\n\t"
                        "lq $11, 48(%1)\n\t"
                        "sq $8, 0(%0)\n\t"
                        "sq $9, 16(%0)\n\t"
                        "sq $10, 32(%0)\n\t"
                        "sq $11, 48(%0)"
                        : : "r"(&particle->transform), "r"(battle + 0x180) : "$8", "$9", "$10", "$11", "memory");
                    particle->battle_scale = *(float *)(battle + 0x220);
                } else if (*(short *)data->spread_rotation_keys == 1) {
                    MMathRotateMatrixYXZ(&particle->transform, 0, &particle->random_rotation);
                }
            }

            particle->random_scale = sefRandf() * (float)*(short *)data->uniform_scale_keys * D_004D833C;
            particle->kind = kind;

            if (data->position_keys == 0) {
                particle->color[2] = 64;
                particle->color[1] = 64;
                particle->color[0] = 64;
            } else if (*(short *)(data->position_keys + 18) == 1024) {
                sefLerpIVectorA(data->position_keys + 10, (Vector4 *)particle->color);
            }

            if (data->size_keys != 0 && *(short *)(data->size_keys + 10) == 1024) {
                sefLerpIVectorA(data->size_keys + 2, &particle->color_scale);
            }

            if (data->color_keys != 0 && *(short *)(data->color_keys + 18) == 1024) {
                sefLerpVectorA(data->color_keys + 8, &particle->color_offset);
                if (_revDirZ != 0) {
                    particle->color_offset.z = -particle->color_offset.z;
                }
            }

            if (data->rotation_keys != 0 && *(short *)(data->rotation_keys + 10) == 1024) {
                sefLerpVectorA(data->rotation_keys, &particle->size);
                if (*(short *)data->color_keys != 3) {
                    __asm__ __volatile__(
                        "lqc2 vf1, 0(%3)\n\t"
                        "mfc1 $8, %2\n\t"
                        "qmtc2 $8, vf2\n\t"
                        "vmulx.xyz vf1, vf1, vf2x\n\t"
                        "sqc2 vf1, 0(%3)"
                        : "=m"(particle->size)
                        : "m"(particle->size), "f"(D_004D8340), "r"(&particle->size)
                        : "$8", "memory");
                }
            }

            if (data->spread_rotation_keys != 0 && *(short *)(data->spread_rotation_keys + 18) == 1024) {
                sefLerpVectorA(data->spread_rotation_keys + 8, &particle->rotation);
                rotation_source_keys = data->spread_rotation_keys;
                update = 0;
                key_flag = *(short *)(rotation_source_keys - 2);
                angle = *(short *)rotation_source_keys;
                if (particle->rotation_state != 4 || key_flag != 0) {
                    update = 1;
                }
                sefCalcRotChange((unsigned char *)particle, angle, update);
            }
            __asm__ __volatile__(
                "lqc2 vf1, 0(%0)\n\t"
                "lqc2 vf2, 0(%1)\n\t"
                "vadd.xyz vf1, vf1, vf2\n\t"
                "sqc2 vf1, 0(%2)"
                : : "r"(&particle->random_rotation), "r"(&particle->rotation), "r"(&particle->rotated_position) : "memory");

            if (data->signal_keys != 0) {
                particle->signal = *data->signal_keys;
            }

            particle->one_initialized_scalar = 1.0f;
            particle->random_translation.w = 1.0f;
            if (data->position_keys != 0) {
                if (*(short *)(data->position_keys + 6) == 0) {
                    short range_x;
                    short range_y;
                    short range_z;
                    float scale_k;
                    range_x = *(short *)(data->position_keys + 0);
                    range_y = *(short *)(data->position_keys + 2);
                    range_z = *(short *)(data->position_keys + 4);
                    scale_k = D_004D8344;
                    seed_1 = sefRandSeed * 0x41C64E6Du + 0x3039u;
                    random_x = (float)(int)((seed_1 >> 16) & 0x7FFFu);
                    seed_2 = seed_1 * 0x41C64E6Du + 0x3039u;
                    random_y = (float)(int)((seed_2 >> 16) & 0x7FFFu);
                    seed_3 = seed_2 * 0x41C64E6Du + 0x3039u;
                    random_z = (float)(int)((seed_3 >> 16) & 0x7FFFu);
                    sefRandSeed = seed_3;
                    particle->random_translation.x = random_x * (float)range_x * scale_k * 0.015625f;
                    particle->random_translation.y = random_y * (float)range_y * scale_k * 0.015625f;
                    particle->random_translation.z = random_z * (float)range_z * scale_k * 0.015625f;
                } else {
                    short range;
                    float rand_value;
                    unsigned int seed;

                    range = *(short *)(data->position_keys + 0);
                    seed = sefRandSeed * 0x41C64E6Du + 0x3039u;
                    sefRandSeed = seed;
                    rand_value = (float)(int)((seed >> 16) & 0x7FFFu);
                    uniform_jitter = rand_value * (float)range * D_004D8348 * 0.015625f;
                    particle->random_translation.x = uniform_jitter;
                    particle->random_translation.y = uniform_jitter;
                    particle->random_translation.z = uniform_jitter;
                }
            }
        }
        result = index;
    }
    return result;
}

typedef struct SefLineGlobalView {
    Vector4 global_position;                 /* +0x00 */
    unsigned char unmodeled_010[0x10];
    Vector4 parent_position;                 /* +0x20 */
    unsigned char unmodeled_030[0x60];
    Vector4 position_delta;                  /* +0x90 */
    unsigned char unmodeled_0a0[0x20];
    Vector4 global_position_source;          /* +0xc0 */
    unsigned char unmodeled_0d0[0x10];
    Vector4 parent_position_source;          /* +0xe0 */
    unsigned char unmodeled_0f0[0x184 - 0xf0];
    int emit_count;                          /* +0x184 */
    unsigned char unmodeled_188[0x190 - 0x188];
    int particle_count;                      /* +0x190 */
} SefLineGlobalView;

static int sefExecLineGlobalData(void *line_arg, int frame)
{
    SefLineData *line_data = line_arg;
    unsigned char *battle;
    Vector4 initial_position;
    Matrix4 parent_matrix;
    int scale_is_fixed;
    int rotation_is_fixed;
    int particle_count;
    int emit_count;
    int point_index;
    int count;
    int parent_line;
    short point_handle;
    unsigned int scale_mode;
    short **keys;
    SefLineGlobalView *global;

    keys = (short **)&line_data->scale_keys;
    global = (SefLineGlobalView *)&line_data->global_position;
    particle_count = global->particle_count;
    scale_is_fixed = keys[0][0] == 4;
    rotation_is_fixed = keys[2][0] == 4;
    line_data->global_data_state = 0;
    if (particle_count <= 0) {
        return 1;
    }
    battle = _battleData;
    if ((*(int *)(battle + 0x224) & 0x100) != 0) {
        return 1;
    }
    scale_mode = (unsigned short)keys[0][0];
    if ((scale_mode & 0x1000) && _nowScheduler != 0) {
        unsigned int scheduler_flags = _nowScheduler->flags;

        _nowScheduler->flags = scheduler_flags | 0x80;
        if ((short)keys[0][0] >= 0x1001 &&
            (*(int *)battle != 0 || *(int *)(battle + 0x70) != 0)) {
            _nowScheduler->flags = scheduler_flags | 0x1080;
            sefGetParentMatrix(_nowScheduler, (short)scale_mode - 0x1000, &parent_matrix);
        }
    }
    __asm__ __volatile__(
        "lq $8, 0(%0)\n\t"
        "sq $8, 0(%1)"
        :
        : "r"(&global->global_position), "r"(&initial_position)
        : "$8", "memory");
    if (!scale_is_fixed) {
        sefGetPosition(&global->global_position, (unsigned char *)&global->global_position_source, keys[0][0]);
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "lqc2 vf2, 0(%1)\n\t"
            "vadd.xyz vf1, vf1, vf2\n\t"
            "sqc2 vf1, 0(%0)"
            :
            : "r"(&global->global_position), "r"(&line_data->origin_offset)
            : "memory");
    }
    if (!rotation_is_fixed) {
        sefGetPosition(&global->parent_position, (unsigned char *)&global->parent_position_source, keys[2][0]);
    }
    if (frame % particle_count == 0) {
        emit_count = global->emit_count;
        if (_parentLine >= 0) {
            SefLocalData *parent_data = (SefLocalData *)&_lineData[_parentLine * SEF_LINE_DATA_RECORD_SIZE];

            if (parent_data == 0) {
                __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&global->position_delta) : "memory");
                return 1;
            }
            for (parent_line = 0; parent_line < 0x100; parent_line++) {
                point_handle = parent_data->pointHandles[parent_line];
                if (point_handle >= 0) {
                    _nowParentLocal[0] = point_handle;
                    if (scale_is_fixed) {
                        sefGetPosition(&global->global_position, (unsigned char *)&global->global_position_source, keys[0][0]);
                        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&global->global_position) : "memory");
                        __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(&line_data->origin_offset) : "memory");
                        __asm__ __volatile__("vadd.xyz vf1, vf1, vf2");
                        __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&global->global_position) : "memory");
                    }
                    if (rotation_is_fixed) {
                        sefGetPosition(&global->parent_position, (unsigned char *)&global->parent_position_source, keys[2][0]);
                    }
                    if (emit_count > 0) {
                        count = emit_count;
                        do {
                            sefCreateParticle((SefLocalData *)line_data, point_handle);
                        } while (--count != 0);
                    }
                }
            }
        } else {
            if (emit_count > 0) {
                parent_line = emit_count;
                do {
                    sefCreateParticle((SefLocalData *)line_data, -1);
                } while (--parent_line != 0);
            }
        }
    }
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "vsub.xyz vf1, vf1, vf2\n\t"
        "sqc2 vf1, 0(%2)"
        :
        : "r"(&global->global_position), "r"(&initial_position), "r"(&global->position_delta)
        : "memory");
    return 1;
}

int sefExecLineGlobalData(void *arg0, int arg1);
static inline short *sefKeyPtr(void *a)
{
    return *(short **)((char *)a + 0x1F4);
}
static inline int sefKeyWord(void *a)
{
    return *(int *)((char *)a + 0x1F4);
}

static int sefExecLineData(void *arg0, int arg1)
{
  int q;
  int temp_2;
  int temp_2_2;
  int temp_3;
  unsigned short temp_17;
  *((short *) (((char *) arg0) + 0x802)) = -1;
  temp_17 = *((unsigned short *) (((char *) arg0) + 0x806));
  if ((*((unsigned short *) (((char *) arg0) + 0x80A))) != 0)
  {
    temp_2 = (*((unsigned short *) (((char *) arg0) + 0x804))) - 1;
    *((short *) (((char *) arg0) + 0x804)) = temp_2;
    if ((temp_2 << 16) < 0)
    {
      if (((*((unsigned short *) (((char *) arg0) + 0x80C))) == 0) || ((*((unsigned short *) (((char *) arg0) + 0x814))) == 0))
      {
        *((short *) (((char *) arg0) + 0x804)) = 0;
        return 0;
      }
    }
    else
    {
      goto block_7;
    }
  }
  if ((*((short *) (((char *) arg0) + 0x804))) < 0)
  {
    *((short *) (((char *) arg0) + 0x804)) = -1;
  }
  block_7:
  {
    short s816 = *((short *) (((char *) arg0) + 0x816));
    float k = 0.01f;
    void *key1d0;
    float g = *((float *) (((char *) arg0) + 0x5F8));
    float h = *((float *) (((char *) arg0) + 0x5FC));
    key1d0 = (void *) ((*((int *) (((char *) arg0) + 0x1D0))) + 8);
    _gravity = g;
    _height = h;
    _colision = ((float) s816) * k;
    sefLerpVectorSC(key1d0, (Vector4 *) (((char *) arg0) + 0xE0));
  }

  sefLerpVectorSC((void *) (((char *) (*((void **) (((char *) arg0) + 0x1D8)))) + 8), (Vector4 *) (((char *) arg0) + 0x100));
  if ((*((unsigned int *) (((char *) arg0) + 0x1F4))) != 0)
  {
    {
      int (*lf)(void *, Vector4 *);
      lf = sefLerpVectorSC;
      lf((void *) (((char *) sefKeyPtr(arg0)) + 8), (Vector4 *) (((char *) arg0) + 0x120));
    }
    {
      unsigned char *src;
      Vector4 *pos;
      pos = (Vector4 *) (((char *) arg0) + 0xB0);
      src = (unsigned char *) (((char *) arg0) + 0x130);
      temp_2_2 = sefKeyWord(arg0);
      sefGetPosition(pos, src, *((short *) ((void *) temp_2_2)));
    }
  }
  sefLerpIVector((SefKey5 *) ((*((int *) (((char *) arg0) + 0x1D4))) + 4), (SefProgressState *) (((char *) arg0) + 0x160));
  sefLerpIVector((SefKey5 *) ((*((int *) (((char *) arg0) + 0x1DC))) + 4), (SefProgressState *) (((char *) arg0) + 0x180));
  sefProgressInt((SefKey3 *) (*((int *) (((char *) arg0) + 0x1E8))), (SefProgressState *) (((char *) arg0) + 0x1AC));
  sefProgressInt((SefKey3 *) (*((int *) (((char *) arg0) + 0x1EC))), (SefProgressState *) (((char *) arg0) + 0x1B8));
  if (((*((short *) (((char *) arg0) + 0x804))) < 0) || (sefExecLineGlobalData(arg0, temp_17) != 0))
  {
    *((unsigned short *) (((char *) arg0) + 0x814)) = sefExecLineLocalData(arg0, temp_17);
    temp_3 = (*((unsigned short *) (((char *) arg0) + 0x806))) + 1;
    *((unsigned short *) (((char *) arg0) + 0x806)) = temp_3;
    if ((temp_3 & 0xFFFF) == 0x3FF)
    {
      if ((*((unsigned short *) (((char *) arg0) + 0x80A))) == 0)
      {
        temp_2_2 = temp_3 - (*((unsigned short *) (((char *) arg0) + 0x1C0)));
        *((unsigned short *) (((char *) arg0) + 0x806)) = temp_2_2;
        if (!(temp_2_2 & 0xFFFF))
        {
          *((unsigned short *) (((char *) arg0) + 0x806)) = 1;
        }
      }
      else
      {
        *((unsigned short *) (((char *) arg0) + 0x806)) = 0x3FE;
      }
    }
  }
  return 1;
}

/*
 * PARTIAL ACCESSED VIEW of an effect-table record (sefInitEffectData): the
 * first 0x20 bytes are filled with 0xff by memset and are not otherwise
 * touched by this function; owner, effect_no and category are the fields it
 * sets from its own parameters, flags and frame are reset to 0. The 2-byte
 * gap after flags is not yet evidenced by any accepted function of this TU.
 */
typedef struct EffectData {
    signed char lineHandles[0x20];
    int owner;
    short category;
    short effect_no;
    short flags;
    short active;
    short frame;
} EffectData;

/*
 * sefInitEffectData (main:0x002e49e8): fills `effect`'s first 0x20 bytes
 * with 0xff, then sets owner/effect_no/category from its own parameters and
 * resets flags/frame to 0. sefInitScheduler and sefInitEffectTbl are its
 * callers.
 */
static void sefInitEffectData(EffectData *effect, int owner, int effect_no, int category)
{
    memset(effect, -1, 0x20);
    effect->owner = owner;
    effect->effect_no = (short)effect_no;
    effect->category = (short)category;
    effect->frame = 0;
    effect->flags = 0;
}

static void sefFreeLineData(int line_index);
static void sefFreeEffectData(unsigned char *effect, unsigned int index);

/*
 * sefDestroyEffectData (main:0x002e4a50): when `effect`'s own owner (+0x20,
 * EffectData) is set, releases all 32 of its line-data handles (the
 * EffectData's own leading 0x20 bytes, one signed line index per byte,
 * sefFreeEffectData below) via sefFreeEffectData, then always clears owner
 * to 0. sefExecEffectData, sefFreeSchedulerCf, sefFreeScheduler and
 * sefExecScheduler (all still assembly except sefFreeSchedulerCf) are its
 * callers.
 */
static void sefDestroyEffectData(unsigned char *effect)
{
    EffectData *effectData = (EffectData *)effect;
    int index;

    if (effectData->owner != 0) {
        for (index = 0; index < 0x20; index++) {
            sefFreeEffectData(effect, index);
        }
    }
    effectData->owner = 0;
}

static unsigned char *sefAllocEffectData(signed char *effect)
{
    int n;
    int idx = *(short *)(effect + 0x28);
    if ((unsigned)idx >= 0x20)
        return 0;
    if (effect[idx] >= 0)
        return 0;
    n = sefAllocLineData();
    if (n < 0)
        return 0;
    effect[idx] = n;
    *(short *)(effect + 0x2a) = 1;
    *(unsigned short *)(effect + 0x28) = *(unsigned short *)(effect + 0x28) + 1;
    return &_lineData[n * 0x820];
}

/*
 * sefFreeEffectData (main:0x002e4b38): for a valid `index` (< 32) whose
 * line-data handle byte is not already -1, releases that line via
 * sefFreeLineData and resets the handle to -1. sefDestroyEffectData above
 * and sefExecEffectData (still assembly) are its callers.
 */
static void sefFreeEffectData(unsigned char *effect, unsigned int index)
{
    if (index < 0x20) {
        signed char *handle = (signed char *)effect + index;

        if (*handle >= 0) {
            sefFreeLineData(*handle);
            *handle = -1;
        }
    }
}

/*
 * sefCheckFinish (main:0x002e4b80): when `condition` carries the 0x4000
 * "loop ended" bit, marks the current scheduler's own record finished
 * (SchedulerState::flags |= 0x40, sef.h). The first parameter is not read
 * by this function.
 */
static void sefCheckFinish(int effect_no, int condition) {
    if (condition & 0x4000) {
        _nowScheduler->flags |= 0x40;
    }
}

typedef struct SefEffectRequest {
    int source;
    int target;
    unsigned char unmodeled_08[4];
    short size;
    short size_copy;
    short character_id;
    short character_id_copy;
    short effect_no;
    short special;
    unsigned char unmodeled_18[2];
    unsigned short flags;
    unsigned char unmodeled_1c[4];
    int param;
} SefEffectRequest;

typedef struct SefEffectCommand {
    short start_time;                /* +0x0 */
    short frame;                     /* +0x2 */
    unsigned short data_offset;      /* +0x4 */
    short link_index;                /* +0x6 */
    short count;                     /* +0x8 */
    short flags;                     /* +0xa */
} SefEffectCommand;
extern void tracePrint(char *fmt, ...);
static void sefSetSeSignal(SefActor *actor);

int sefExecEffectData(EffectData *effect, int frame, short *active_count)
{
    SefEffectRequest *battle;
    SefLineData *line;
    SefLineData *linked;
    int running;
    int index;
    int j;
    int link_index;
    unsigned int category_word;
    int category;
    short finish_condition;
    unsigned char battle_active;

    effect->frame = 0;
    if (effect->owner == 0) {
        return 0;
    }

    if (frame == (effect->category & 0x3ff)) {
        if ((effect->active & 0x10) != 0) {
            func_A2C408(0);
        } else if ((effect->active & 0x20) != 0) {
            func_A2C430(0);
            _nowScheduler->flags |= 0x4000;
        }
    }

    if ((effect->category & 0x1000) != 0) {
        if ((unsigned char)((SefEffectRequest *)_battleData)->special == 0) {
            sefDestroyEffectData((unsigned char *)effect);
            effect->frame = 1;
            return 0;
        }
        if (*active_count != 0) {
            sefCheckFinish(frame, effect->category);
            effect->category = (short)frame;
            *active_count = 0;
        }
    }

    category = effect->category;
    finish_condition = category;
    if ((effect->category & 0x2800) != 0 && frame >= (category & 0x3ff)) {
        battle = (SefEffectRequest *)_battleData;
        battle_active = (unsigned char)battle->special;
        sefCheckFinish(frame, finish_condition);
        if ((effect->category & 0x2000) != 0) {
            _hitSignal = 1;
            sefSetHitSignal((SefActor *)battle->target);
        }
        if ((effect->category & 0x800) != 0) {
            _seSignal = 1;
            sefSetSeSignal((SefActor *)battle->target);
        }
        if (battle_active == 0) {
            sefDestroyEffectData((unsigned char *)effect);
            effect->frame = 1;
            return 0;
        }
        effect->category = (short)frame;
        *active_count = 0;
    }

    category_word = *(unsigned int *)&effect->category;
    if ((category_word & 0x7000) == 0x4000 && frame == (effect->category & 0x3ff)) {
        effect->category = (short)frame;
        _nowScheduler->flags |= 0x40;
    }
    if (frame >= effect->category) {
        frame -= effect->category;
        if ((unsigned int)effect->owner < 3U) {
            sefDestroyEffectData((unsigned char *)effect);
            effect->frame = 1;
            return 0;
        }

        running = 0;
        for (;;) {
            SefEffectCommand *command;
            unsigned short *line_script;
            int script_index;
            int start_time;
            int command_frame;
            short count;
            short command_flags;

            script_index = effect->flags;
            command = (SefEffectCommand *)effect->owner + script_index;
            start_time = command->start_time;
            command_frame = command->frame;
            count = command->count;
            command_flags = command->flags;
            link_index = command->link_index;

            if (start_time < 1024) {
                if (frame >= start_time) {
                    if (script_index < 32) {
                        line_script = (unsigned short *)scGetImmAdrImmIdx2(_nowScriptAdr,
                            (unsigned short *)effect->owner, script_index * 6 + 2);
                        if (line_script != 0) {
                            line = (SefLineData *)sefAllocEffectData(effect);
                            if (line != 0) {
                                sefInitLineData(line, start_time, command_frame, line_script,
                                                link_index, count, command_flags);
                                if (start_time != 0x1000) {
                                    continue;
                                }
                            } else {
                                tracePrint("line empty!!");
                            }
                        }
                    }
                } else if (start_time != 0x400) {
                    running = 1;
                }
            }
            break;
        }

        effect->frame = 1;
        for (index = 0; index < 32; index++) {
            int handle;

            handle = effect->lineHandles[index];
            if (handle < 0) {
                continue;
            }
            line = (SefLineData *)&_lineData[handle * SEF_LINE_DATA_RECORD_SIZE];
            link_index = line->link_index;
            if (link_index >= 0) {
                link_index = effect->lineHandles[link_index];
            }
            _parentLine = link_index;
            if (((int (*)(SefLineData *, int))sefExecLineData)(line, frame) != 0) {
                running++;
                continue;
            }
            sefFreeEffectData((unsigned char *)effect, index);
            if (line->global_data_count != 0) {
                continue;
            }
            for (j = 0; j < 32; j++) {
                int handle;

                if (j == index) {
                    continue;
                }
                handle = effect->lineHandles[j];
                if (handle < 0) {
                    continue;
                }
                linked = (SefLineData *)&_lineData[handle * SEF_LINE_DATA_RECORD_SIZE];
                if (linked->link_index == index) {
                    sefFreeEffectData((unsigned char *)effect, j);
                }
            }
        }

        if (running == 0) {
            sefDestroyEffectData((unsigned char *)effect);
            effect->frame = 1;
            return 0;
        }
        return effect->owner != 0;
    }
    return 1;
}

void sefInitScheduler(void)
{
    int off;
    int m;
    int i;
    int j;
    unsigned char *p;
    EffectData *effect;

    m = -1;
    i = 0;
    off = 0;
    sefMemZero(_scheduler, 0x55800);
    p = _scheduler + 0xa90;
    do {
        effect = (EffectData *)(_scheduler + 0xb0 + off);
        *(unsigned short *)(p + 2) = m;
        *(unsigned short *)(p + 4) = m;
        *(unsigned short *)(p + 6) = m;
        for (j = 0; j < 0x20; j++) {
            sefInitEffectData(effect, 0, -1, 0);
            effect++;
        }
        i++;
        p += 0xab0;
        off += 0xab0;
    } while (i < 0x80);
}

extern const volatile struct { unsigned short value, pad; } _nowEvent;
extern int _nowScript;

static int sefAllocScheduler(int arg0)
{
    static int _schridx_1 __asm__("_schridx.1") = 0;
    int start = _schridx_1;
    int end = (start + 0x7f) % 0x80;
    int i = start;
    int off;
    int p_off;
    int b_off;
    int *p;
    short *b;
    unsigned char *base;
    int nxt;
    short ev;

    if (start != end) {
        base = (unsigned char *)_scheduler;
        do {
            i %= 0x80;
            off = i * 0xab0;
            p_off = off + 0x6b0;
            p = (int *)(p_off + (int)base);
            if (*p == 0) {
                b_off = p_off + 0x3e0;
                b = (short *)(b_off + (int)base);
                b[3] = -1;
                *p = arg0;
                b[1] = (short)_nowScript;
                b[2] = _nowEvent.value;
                nxt = i + 1;
                nxt %= 0x80;
                _schridx_1 = nxt;
                b[0] = 0;
                return i;
            }
            i++;
        } while (i != end);
    }
    return -1;
}

extern void xglSoundEffectStopID(int soundId, int channel);
static void sefDestroyEffectData(unsigned char *effect);

/*
 * sefFreeSchedulerCf (main:0x002e5118): releases an active scheduler
 * record -- stops its currently playing sound effect if any, destroys each
 * of its 32 effect slots, and clears the record's script binding. A NULL
 * scheduler or one that is already free (inUse == 0) does nothing.
 * sefDeleteEffect2 and sefDeleteEffectCf (this file) tail-call this with
 * their own scheduler argument unchanged.
 */
void sefFreeSchedulerCf(SchedulerState *scheduler)
{
  int soundId;
  int i;

  if (scheduler != 0 && scheduler->inUse != 0)
  {
    soundId = scheduler->soundId;
    if (soundId > 0)
    {
      xglSoundEffectStopID(soundId, 0);
      scheduler->soundId = 0;
    }
    for (i = 0; i < 32; i++)
    {
      sefDestroyEffectData(scheduler->effects[i]);
    }
    scheduler->scriptBinding.state = 0;
    scheduler->inUse = 0;
    scheduler->scriptBinding.script_id = -1;
    scheduler->scriptBinding.task_id = -1;
  }
}

typedef struct SefScriptStatus {
    unsigned char unmodeled_00[8];
    short active_tasks;
    unsigned char unmodeled_0a[6];
} SefScriptStatus;
typedef struct SefScriptReference {
    unsigned char unmodeled_000[0x440];
    SefScriptStatus status;
} SefScriptReference;
extern SefScriptReference _scriptWork[16];
extern SchedulerState sefSchedulerTable[] __asm__("_scheduler");

void sefFreeScheduler(unsigned int scheduler_index)
{
    enum {
        scheduler_stride = sizeof(SchedulerState),
        binding_offset = 0xa90
    };
    int byte_offset;
    int script_id;
    int i;
    unsigned char (*effect)[0x30];
    SefScriptStatus *status;
    ScriptBinding *binding;
    SchedulerState *scheduler;
    SchedulerState *entry;

    if (scheduler_index >= 128) {
        return;
    }
    if (sefSchedulerTable[scheduler_index].inUse == 0) {
        return;
    }
    scheduler = &sefSchedulerTable[scheduler_index];
    if ((scheduler->flags & 0x202) != 0) {
        sefFreeSchedulerCf(scheduler);
        return;
    }
    effect = (unsigned char (*)[0x30])((unsigned char *)sefSchedulerTable
        + 0xb0
        + scheduler_index * scheduler_stride);
    for (i = 31; i >= 0; i--, effect++) {
        sefDestroyEffectData(*effect);
    }
    byte_offset = scheduler_index * scheduler_stride;
    binding = (ScriptBinding *)((unsigned int)sefSchedulerTable
        + (byte_offset + binding_offset));
    script_id = binding->script_id;
    if (script_id >= 0) {
        status = &_scriptWork[script_id].status;
        --status->active_tasks;
    }
    entry = (SchedulerState *)((unsigned int)sefSchedulerTable + byte_offset);
    entry->inUse = 0;
    binding->script_id = -1;
    binding->task_id = -1;
    binding->state = 0;
}

/*
 * sefDestroyScriptScheduler (main:0x002e52b8): like sefDestroyScriptScheduler2
 * below, but matches only script_index (not also a task_index) against every
 * scheduler table record's own script_id. scDestroyScript (still assembly in
 * a different TU) is its only caller.
 */
void sefDestroyScriptScheduler(int script_index)
{
    SchedulerState *scheduler;
    ScriptBinding *record;
    int index;

    index = 0;
    scheduler = (SchedulerState *)_scheduler;
    record = &scheduler->scriptBinding;
    do {
        if (record->script_id == script_index) {
            sefFreeScheduler(index);
        }
        index++;
        record = (ScriptBinding *)((unsigned char *)record + 0xab0);
    } while (index < 0x80);
}

extern void sefFreeScheduler(unsigned int scheduler_index);

/*
 * sefDestroyScriptScheduler2 (main:0x002e5320): scDestroyScript2 (main:
 * 0x002e9e48, still assembly in a different TU) forwards its own two
 * parameters here unchanged and also to scDeleteTask, whose own accepted
 * parameters are named script_index and task_index (src/main/sc_get.c).
 * This loop walks the scheduler table's own SchedulerState records (sef.h)
 * at their established 0xab0 stride and frees the first slot whose
 * scriptBinding matches.
 */
void sefDestroyScriptScheduler2(int script_index, int task_index)
{
    SchedulerState *scheduler;
    ScriptBinding *record;
    int index;

    index = 0;
    scheduler = (SchedulerState *)_scheduler;
    record = &scheduler->scriptBinding;
    do {
        if (record->script_id == script_index && record->task_id == task_index) {
            sefFreeScheduler(index);
        }
        index++;
        record = (ScriptBinding *)((unsigned char *)record + 0xab0);
    } while (index < 0x80);
}

typedef struct SefEffectDescriptor {
    short category;
    unsigned short script_offset;
    short setting;
} SefEffectDescriptor;
static inline void sefInitializeDescriptorEffect(EffectData *effect,
                                                  SefEffectDescriptor *entry,
                                                  int scheduler_index,
                                                  ScriptBinding *binding,
                                                  SefEffectDescriptor *descriptors)
{
    int owner;
    owner = scGetImmAdrImmIdx2(_nowScriptAdr, (unsigned short *)entry, 1);

    if (owner == 0) {
        owner = 1;
    }
    sefInitEffectData(effect, owner, scheduler_index, entry->category);
    if (entry->setting >= 0) {
        binding->link_id = descriptors[entry->setting].category;
    }
}
static inline void sefApplyDescriptorCommand(EffectData *effect, short command)
{
    if (command == -0x200) {
        effect->active |= 0x10;
    } else if (command == -0x201) {
        effect->active |= 0x20;
    }
}

static void sefInitEffectTbl(SefEffectDescriptor *entry, int scheduler_index, int init_flags)
{
    int index = 0;
    EffectData *effects = (EffectData *)((SchedulerState *)_scheduler)[scheduler_index].effects;
    SefEffectDescriptor *descriptors = entry;
    int preserve_setting = -0x100;

    if (entry->category >= 0) {
        SchedulerState *scheduler = &((SchedulerState *)_scheduler)[scheduler_index];
        EffectData *effect = effects;
        ScriptBinding *binding = &scheduler->scriptBinding;
        EffectData *cursor = effects;

process_descriptor:
        {
            int next_category;
            int has_capacity;
            if (entry->setting != preserve_setting || (init_flags & 1) != 0) {
                sefInitializeDescriptorEffect(effect, entry, scheduler_index, binding, descriptors);
            }
            sefApplyDescriptorCommand(effect, entry->setting);
            entry++;
            cursor++;
            next_category = entry->category;
            has_capacity = ++index < 32;
            effect = cursor;
            if (next_category >= 0 && has_capacity) {
                goto process_descriptor;
            }
        }
    }
    if (index < 32) {
        do {
            effects[index].owner = 0;
            index++;
        } while (index < 32);
    }
}

/*
 * sefCreateScheduler2's view of a _scheduler record: the upstream layout
 * (16-byte aligned position/rotation/scale vectors, the request copy
 * typed as a SefEffectRequest). Same 0xab0-byte record as SchedulerState.
 */
typedef struct SefSchedulerCreateView {
    Matrix4 worldMatrix;
    Matrix4 localMatrix;
    Vector4 position __attribute__((aligned(16)));
    Vector4 rotation __attribute__((aligned(16)));
    Vector4 scale __attribute__((aligned(16)));
    unsigned char effects[32][0x30];
    int inUse;
    unsigned char *scriptAddress;
    void *savedActive;
    int caster;
    int target;
    void *casterUnit;
    void *targetUnit;
    int soundParameters[4];
    int soundMode;
    int soundId;
    unsigned char cameraChoice[0x44];
    unsigned char unmodeled_728[0x730 - 0x728];
    int effectValue;
    int effectScript;
    unsigned char unmodeled_738[0x740 - 0x738];
    int effectMode;
    unsigned char unmodeled_744[0xa50 - 0x744];
    int alterValue;
    unsigned char unmodeled_a54[0xa64 - 0xa54];
    SefEffectRequest request;
    float effectSpeed;
    int flags;
    ScriptBinding scriptBinding;
    short category;
    signed char display;
    unsigned char signalByte;
    short delayFrames;
    short schedulerIndex;
    short effectNumber;
    unsigned char unmodeled_aa2[0xaa4 - 0xaa2];
    short mapperIndex;
    unsigned char unmodeled_aa6[0xab0 - 0xaa6];
} SefSchedulerCreateView;
extern SefSchedulerCreateView sefSchedulerCreateTable[] __asm__("_scheduler");
int sefCreateScheduler2(int effect_no, int parameters, int position, int table, int flags, int index)
{
    int number;
    int category;
    int scheduler_index;
    unsigned char *saved_script_adr;
    int value0;
    int value1;
    int value2;
    int value3;
    void *origin;
    SefScriptStatus *work;
    int *param_block;
    int *camera_block;
    int *effect_block;
    int *tail_block;
    SefEffectRequest *request;

    value0 = scGetImmAdrImmIdx2(table, (unsigned short *)effect_no, 0);
    value1 = scGetImmAdrImmIdx2(table, (unsigned short *)effect_no, 1);
    value2 = scGetImmAdrImmIdx2(table, (unsigned short *)effect_no, 2);
    value3 = scGetImmAdrImmIdx2(table, (unsigned short *)effect_no, 3);
    effect_no += 8;

    if (index >= 0) {
        scheduler_index = index;
        sefSchedulerCreateTable[scheduler_index].inUse = effect_no;
        sefSchedulerCreateTable[scheduler_index].scriptBinding.link_id = -1;
        sefSchedulerCreateTable[scheduler_index].scriptBinding.script_id = *(const int *)&_nowScript;
        sefSchedulerCreateTable[scheduler_index].scriptBinding.task_id = *(const int *)&_nowEvent;
        sefSchedulerCreateTable[scheduler_index].scriptBinding.state = 0;
    } else {
        scheduler_index = sefAllocScheduler(effect_no);
    }
    if (scheduler_index < 0) {
        return -1;
    }

    origin = (void *)position;
    saved_script_adr = *(unsigned char **)&_nowScriptAdr;
    _nowScriptAdr = table;
    request = &sefSchedulerCreateTable[scheduler_index].request;
    *request = *((SefEffectRequest *)parameters);
    sefSchedulerCreateTable[scheduler_index].caster = ((SefEffectRequest *)parameters)->source;
    sefSchedulerCreateTable[scheduler_index].targetUnit = 0;
    sefSchedulerCreateTable[scheduler_index].casterUnit = 0;
    sefSchedulerCreateTable[scheduler_index].target = ((SefEffectRequest *)parameters)->target;
    if (origin == 0) {
        origin = (void *)_zeroPos;
    }
    __asm__ __volatile__("lq $8, 0(%1)\n" "sq $8, 0(%0)\n" : : "r"(&sefSchedulerCreateTable[scheduler_index].position), "r"(origin) : "$8", "memory");
    __asm__ __volatile__("sq $0, 0(%0)\n" : : "r"(&sefSchedulerCreateTable[scheduler_index].rotation) : "memory");
    sefSchedulerCreateTable[scheduler_index].savedActive = 0;
    sefSchedulerCreateTable[scheduler_index].scriptAddress = (unsigned char *)table;
    sefSchedulerCreateTable[scheduler_index].delayFrames = (short)flags;
    sefSchedulerCreateTable[scheduler_index].flags = 0;
    sefSchedulerCreateTable[scheduler_index].signalByte = 0;
    sefSchedulerCreateTable[scheduler_index].schedulerIndex = (short)scheduler_index;
    param_block = (int *)&sefSchedulerCreateTable[scheduler_index].soundParameters;
    memset(param_block, 0, 0x18);
    camera_block = (int *)&sefSchedulerCreateTable[scheduler_index].cameraChoice;
    memset(camera_block, 0, 0x44);
    effect_block = (int *)&sefSchedulerCreateTable[scheduler_index].effectValue;
    memset(effect_block, 0, 0x320);
    tail_block = &sefSchedulerCreateTable[scheduler_index].alterValue;
    memset(tail_block, 0, 0x14);
    param_block[0] = value0;
    param_block[4] = 4;
    tail_block[4] = 18;
    effect_block[4] = 2;
    tail_block[0] = value3;
    effect_block[1] = table;
    effect_block[0] = value2;
    sdvSelectCamera(camera_block, table, value1);
    __asm__ __volatile__("vmaxw.xyzw vf1, vf0, vf0w\n\t"
                         "sqc2 vf1, 0(%0)"
                         : : "r"(&sefSchedulerCreateTable[scheduler_index].scale) : "memory");
    sefSchedulerCreateTable[scheduler_index].display = 1;
    sefSchedulerCreateTable[scheduler_index].effectSpeed = 3.0f;
    work = &_scriptWork[sefSchedulerCreateTable[scheduler_index].scriptBinding.script_id].status;
    work->active_tasks += 1;
    sefInitEffectTbl((SefEffectDescriptor *)effect_no, scheduler_index, *(unsigned short *)((char *)parameters + 0x1e));
    _nowScriptAdr = (int)saved_script_adr;
    srsAnalyzeEftNo(((SefEffectRequest *)parameters)->effect_no, &number, &category);
    sefSchedulerCreateTable[scheduler_index].effectNumber = (short)number;
    sefSchedulerCreateTable[scheduler_index].category = (short)category;
    sefSchedulerCreateTable[scheduler_index].mapperIndex = sefSearchMapperIndex(((SefEffectRequest *)parameters)->effect_no);
    return scheduler_index;
}

extern int sefCreateScheduler2(int effect_no, int value2, int value3, int value4, int flags, int value5);

/*
 * sefCreateScheduler (main:0x002e5900): forwards to the scheduler allocator
 * sefCreateScheduler2 with a plain 0 for its own flags slot, and returns its
 * handle unchanged: scEFFECTScript/scEFFECT2Script/scEFFECT3Script store
 * that handle as `effect_scheduler` (src/main/sc_get.h). value2/value3/
 * value4/value5 are forwarded unchanged; this allocation has no further
 * evidence for them.
 */
int sefCreateScheduler(int effect_no, int value2, int value3, int value4, int value5) {
    return sefCreateScheduler2(effect_no, value2, value3, value4, 0, value5);
}


/*
 * sefGetSizeOffset (main:0x002e5920): returns the size-offset table entry at
 * `offset_2[index]` (config/symbols/main.txt, offset_2 @ 0x004CBF48, 5
 * ints); sefCreateReactionEffect and sefCnvDeathEffectNo are its callers.
 */
static int sefGetSizeOffset(int index) {
    static const int offset_2[5] __asm__("offset.2") = {0, 0, 1, 2, 2};
    return offset_2[index];
}

extern void scCreateScript();
static inline unsigned short sefReactionFlag1C(void)
{
    return *(unsigned short *)(_battleData + 0x1C);
}
typedef struct {
    unsigned char b0[0x16];
    unsigned short hw16;
    unsigned char bits;
    unsigned char b19[3];
    unsigned short flag1c;
} SefBattleView;
typedef struct {
    int a0;
    int a4;
    int a8;
    short size;
    short sizeCopy;
    unsigned short id;
    unsigned short idCopy;
    short effectNo;
    short pad16;
    short pad18;
    unsigned short flags;
    int w1c;
    int w20;
} SefReactionArg;

void sefCreateReactionEffect(void)
{
  SefReactionArg p __attribute__((aligned(8)));
  unsigned short hw16;
  int kind;
  int hi;
  int bits;
  int mask;
  unsigned int bit;
  unsigned int cnt;
  int i;
  long long *src;
  SefBattleView * const bv = (SefBattleView *) _battleData;
  hw16 = bv->hw16;
  kind = (unsigned char) hw16;
  bits = bv->bits;
  if (!kind)
  {
    return;
  }
  {
    hi = hw16 >> 8;
    src = (long long *) bv;
    __builtin_memcpy(&p, src, 36);
    cnt = 0;
    p.pad16 = 0;
    p.pad18 = 0;
    if (p.a4 == 0)
    {
      if (hi)
      {
        p.a4 = p.a0;
        p.idCopy = p.id;
        p.sizeCopy = p.size;
      }
      else
      {
        p.a4 = p.a0;
        p.idCopy = p.id;
        p.sizeCopy = p.size;
      }
    }
    if ((0x22 & hi) != 0)
    {
      p.effectNo = 0xA34 + sefGetSizeOffset(p.sizeCopy);
      scCreateScript((unsigned char *) (&p));
    }
    if ((hi & 0x10) != 0)
    {
      p.effectNo = sefGetSizeOffset(p.sizeCopy) + 0xA37;
      scCreateScript((unsigned char *) (&p));
    }
    if (bits != 0)
    {
      mask = 1;
      for (i = 0; i < 8; i++, mask <<= 1)
      {
        if (bits & mask)
        {
          p.effectNo = 0xA5B + i;
          cnt += 1;
          scCreateScript((unsigned char *) (&p));
        }
      }

    }
    if ((hi & 1) && (cnt == 0))
    {
      p.effectNo = 0xA64;
      scCreateScript((unsigned char *) (&p));
    }
    if ((kind == 3) || (kind == 1))
    {
      if (bv->flag1c & 1)
      {
        if (((unsigned) (p.idCopy - 0x22)) >= 3U)
        {
          if (((unsigned) (p.idCopy - 0x99)) > 1U)
          {
            return;
          }
        }
        p.effectNo = 0xA63;
        scCreateScript((unsigned char *) (&p));
      }
    }
  }
}

/* sefGetMatrixScale (main:0x002e5af8): scale helper for matrix records.
 * Callers pass matrix/vector storage in a0 (sefDrawSchedulerEffect passes a
 * stack record; sefGetParentMatrix passes s0) and consume the scalar result
 * in f0. The body tail-calls the resident MMathCalcLength helper and returns
 * its value unchanged; the frame only saves/restores ra per the ABI. */
float sefGetMatrixScale(float *matrix)
{
    return MMathCalcLength(matrix);
}

extern unsigned char D_00794290[];
extern unsigned char D_00794190[];
extern void MATRIX_convert4(float (*dst)[4], float (*src)[4]);
extern void xglMatrixStackLoad(float matrix[4][4]);
extern void xglMatrixStackMul(float matrix[4][4]);
extern void xglMatrixStackSave(float matrix[4][4]);
extern void xglMatrixInverse(void *dst, void *src);
static inline void sefCopyMatrix(void *dst, void *src)
{
    __asm__ __volatile__(
        "lq $8, 0(%0)\n\t"
        "lq $9, 16(%0)\n\t"
        "lq $10, 32(%0)\n\t"
        "lq $11, 48(%0)\n\t"
        "sq $8, 0(%1)\n\t"
        "sq $9, 16(%1)\n\t"
        "sq $10, 32(%1)\n\t"
        "sq $11, 48(%1)"
        :
        : "r"(src), "r"(dst)
        : "$8", "$9", "$10", "$11", "memory");
}

void sefGetParentMatrix(SchedulerState *scheduler, unsigned int id, Matrix4 *matrix)
{
    Matrix4 conv;
    Matrix4 joint;
    Matrix4 base;
    int flags;
    int inverse;
    unsigned char *actor;
    unsigned char *object;
    unsigned char *parent;
    unsigned char *node_owner;
    int node;
    int mats;

    flags = scheduler->flags;
    if (!(flags & 2)) {
        return;
    }
    actor = *(unsigned char **)((unsigned char *)scheduler + 0x6BC);
    inverse = 1;
    if (actor != 0) {
        if (flags & 0x80) {
            sefGetMotionNullMatrix(matrix, actor, id);
        } else {
            sefCalcRotTransSMatrix((Vector4 *)(actor + 0x50), (Vector4 *)(actor + 0x10),
                                   (Vector4 *)(actor + 0x60), matrix);
        }
        if (*(int *)(*(unsigned char **)((unsigned char *)scheduler + 0x6BC) + 0x8FC) != 0) {
            MATRIX_convert4(conv, *(float (**)[4])(*(unsigned char **)((unsigned char *)scheduler + 0x6BC) + 0x824));
            MMathMulMatrix(matrix, conv, matrix);
        }
        sefCopyMatrix(D_00794290, matrix);
        *(float *)(D_00794290 + 0xA0) = sefGetMatrixScale((float *)D_00794290);
    } else {
        object = *(unsigned char **)((unsigned char *)scheduler + 0x6C4);
        if (object != 0) {
            sefCalcRotTransSMatrix((Vector4 *)(object + 0x20), (Vector4 *)(object + 0x10),
                                   (Vector4 *)(object + 0x30), matrix);
            node_owner = *(unsigned char **)((unsigned char *)scheduler + 0x6C4);
            parent = *(unsigned char **)(node_owner + 0xFC);
            if (parent != 0) {
                node = *(int *)(node_owner + 0xF8);
                mats = *(int *)(parent + 0x824);
                if (node >= 0) {
                    if (node & 0x8000) {
                        sefCopyMatrix(joint, (void *)(((node & 0x7FFF) << 6) + mats));
                        joint[0][3] = joint[1][3] = joint[2][3] = 0.0f;
                        xglMatrixStackLoad(joint);
                    } else {
                        sefCopyMatrix(base, (void *)mats);
                        sefCopyMatrix(joint, (void *)(((*(int *)(*(unsigned char **)((unsigned char *)scheduler + 0x6C4) + 0xF8) & 0x7FFF) << 6) + mats));
                        base[0][3] = base[1][3] = base[2][3] = joint[0][3] = joint[1][3] = joint[2][3] = 0.0f;
                        xglMatrixStackLoad(base);
                        xglMatrixStackMul(joint);
                    }
                    xglMatrixStackMul(*matrix);
                    xglMatrixStackSave(*matrix);
                }
            }
            sefCopyMatrix(D_00794290, matrix);
            *(float *)(D_00794290 + 0xA0) = sefGetMatrixScale((float *)D_00794290);
        } else {
                inverse = 0;
                __asm__ __volatile__(
                    "vmr32.xyzw $vf1xyzw, $vf0xyzw\n\t"
                    "vmr32.xyzw $vf2xyzw, $vf1xyzw\n\t"
                    "vmr32.xyzw $vf3xyzw, $vf2xyzw\n\t"
                    "sqc2 $vf0, 48(%0)\n\t"
                    "sqc2 $vf1, 32(%0)\n\t"
                    "sqc2 $vf2, 16(%0)\n\t"
                    "sqc2 $vf3, 0(%0)"
                    :
                    : "r"(matrix)
                    : "memory");
                sefCopyMatrix(D_00794290, matrix);
                *(float *)(D_00794290 + 0xA0) = 1.0f;
        }
    }
    if (inverse) {
        xglMatrixInverse(D_00794190, matrix);
    }
}

extern int func_A33248(void);
extern int func_00A0CE30(void);
extern void sdvExecAmbient(void);
extern void sdvClearSpecialWork(void);
extern void sdvScheduleSound(void *sequence);
extern void sdvScheduleCamera(void *camera);
extern void sdvExecSpecial(void *special);
extern void sdvDrawSpecial(void *special);
extern void sdvScheduleAlter(void *alter, void *request);
typedef struct SefParentRecord {
    unsigned char unmodeled_000[0x10];
    Vector4 position;                /* +0x10 */
    Vector4 rotation;                /* +0x20 */
    Vector4 scale;                   /* +0x30 */
    unsigned char unmodeled_040[0xa4 - 0x40];
    short liveState;                 /* +0xa4 */
    unsigned char unmodeled_0a6[0xf8 - 0xa6];
    int matrixIndex;                 /* +0xf8 */
    struct SefActor *actor;          /* +0xfc */
} SefParentRecord;
extern unsigned char D_0065A9B0[];
extern unsigned char D_0065AFCC[];
extern unsigned char D_0065B030[];
#ifndef offsetof
#define offsetof(type, member) ((unsigned int)&((type *)0)->member)
#endif
int _nowEftNo = 0;
#define SLOT(f) (*(__typeof__(((SchedulerState *)0)->f) *)(control_address + (offsetof(SchedulerState, f) - 0xa80)))
#define BATTLE ((SefBattleStatus *)_battleData)

int sefExecScheduler(void)
{
    SchedulerState *scheduler;
    SchedulerState *table;
    short hit_flag;
    int active_count = 0;
    int index;
    unsigned int control_address;
    unsigned int record_offset = 0;

    table = sefSchedulerTable;

    sdvExecAmbient();
    sefInitBattlePrm();
    sdvClearSpecialWork();
    sefExecLoadQue();

    scheduler = table;
    control_address = (unsigned int)table + 0xa80;
    for (index = 0; index < 128; control_address += sizeof(SchedulerState), record_offset += sizeof(SchedulerState), scheduler++, index++) {
        int effect_alive = 0;
        int running;
        int k;
        SchedulerState *expired;
        unsigned int binding_offset;
        ScriptBinding *exec_binding;
        SchedulerState *record;

        binding_offset = index * sizeof(SchedulerState) + offsetof(SchedulerState, scriptBinding);
        running = (unsigned short)SLOT(flags) & 0x2;
        hit_flag = _hitFlag;

        if (SLOT(inUse) != 0 && SLOT(extraByte) != 0) {
            if (func_A33248() != 0) {
                active_count += 1;
                continue;
            }
            if (SLOT(delayFrames) > 0) {
                SLOT(delayFrames) -= 1;
                active_count += 1;
                continue;
            }
            if (running != 0) {
                if (SLOT(parentRecord) != 0 && SLOT(parentRecord)->liveState < 0) {
                    sefFreeScheduler(index);
                    continue;
                }
                if (SLOT(targetActor) != 0 && SLOT(targetActor)->scheduleState <= 0) {
                    sefFreeScheduler(index);
                    continue;
                }
            }

            *(SefEffectRequest *)BATTLE = *(SefEffectRequest *)SLOT(request);
            BATTLE->targetActor = SLOT(targetActor);
            /* Copied as a plain int word: the int-typed load/store dependences give the
             * original instruction order (a pointer-typed copy schedules differently). */
            *(int *)&BATTLE->sourceActor = *(int *)&SLOT(sourceActor);
            BATTLE->parentRecord = SLOT(parentRecord);
            BATTLE->savedParameter = SLOT(savedParameter);
            BATTLE->effectNo = SLOT(effectNo);
            BATTLE->flags = SLOT(flags);
            BATTLE->running = running;

            if (SLOT(effectNo) == 55 && hit_flag != 0) {
                SLOT(nusumeResult) = BATTLE->nusumeResult = func_00A0CE30();
            }

            /* _nowScript/_nowEvent are declared const for their readers (sefCreateScheduler2
             * matches only with that); this is their one writer. */
            *(int *)&_nowScript = SLOT(scriptBinding.script_id);
            *(int *)&_nowEvent = SLOT(scriptBinding.task_id);
            _nowScriptAdr = (int)SLOT(scriptAddress);
            _nowEftNo = SLOT(effectNo);
            /* Non-zero when vectors[1] (x and y read as one doubleword, then z) is not zero. */
            BATTLE->vectorActive = (((long long)*(int *)&SLOT(vectors[1].z)
                                     | *(unsigned long long *)&SLOT(vectors[1])) != 0);
            _nowScheduler = scheduler;
            sefCalcRotTransSMatrix(&scheduler->vectors[1], &scheduler->vectors[0],
                                   &scheduler->vectors[2], (Matrix4 *)&scheduler->localMatrix);

            __asm__ __volatile__("vmr32.xyzw vf1, vf0\n\tvmr32.xyzw vf2, vf1\n\tvmr32.xyzw vf3, vf2\n\tsqc2 vf0, 48(%0)\n\tsqc2 vf1, 32(%0)\n\tsqc2 vf2, 16(%0)\n\tsqc2 vf3, 0(%0)" : : "r"(&BATTLE->targetMatrix) : "memory");
            __asm__ __volatile__("vmr32.xyzw vf1, vf0\n\tvmr32.xyzw vf2, vf1\n\tvmr32.xyzw vf3, vf2\n\tsqc2 vf0, 48(%0)\n\tsqc2 vf1, 32(%0)\n\tsqc2 vf2, 16(%0)\n\tsqc2 vf3, 0(%0)" : : "r"(&BATTLE->sourceMatrix) : "memory");

            if (running != 0) {
                sefGetParentMatrix(scheduler, 0, (Matrix4 *)&scheduler->worldMatrix);
            } else {
                __asm__ __volatile__("vmr32.xyzw vf1, vf0\n\tvmr32.xyzw vf2, vf1\n\tvmr32.xyzw vf3, vf2\n\tsqc2 vf0, 48(%0)\n\tsqc2 vf1, 32(%0)\n\tsqc2 vf2, 16(%0)\n\tsqc2 vf3, 0(%0)" : : "r"(&scheduler->worldMatrix) : "memory");
                if (SLOT(targetActor) != 0) {
                    sefCalcRotTransSMatrix(&SLOT(targetActor)->rotation, &SLOT(targetActor)->position,
                                           &SLOT(targetActor)->scale, &BATTLE->targetMatrix);
                }
            }
            if (SLOT(sourceActor) != 0) {
                sefCalcRotTransSMatrix(&SLOT(sourceActor)->rotation, &SLOT(sourceActor)->position,
                                       &SLOT(sourceActor)->scale, &BATTLE->sourceMatrix);
            }

            BATTLE->effectSpeed = SLOT(effectSpeed);
            sefSetReverseDir(SLOT(effectNo), (short)SLOT(requestParam));
            for (k = 0; k < 32; k++) {
                exec_binding = (ScriptBinding *)((unsigned int)_scheduler + binding_offset);
                if (sefExecEffectData((EffectData *)(D_0065A9B0 + index * sizeof(SchedulerState) + k * sizeof(scheduler->effects[0])),
                                      exec_binding->state, &hit_flag) != 0) {
                    effect_alive += 1;
                }
            }

            /* D_0065AFCC is sefSchedulerTable[0].soundParameters (+0x6cc): +0x18 cameraChoice,
             * +0x64 the effect block (+0x730, D_0065B030), +0x384/+0x398 alter block and request. */
            sdvScheduleSound(D_0065AFCC + record_offset);
            sdvScheduleCamera(D_0065AFCC + 0x18 + record_offset);
            sdvExecSpecial(D_0065AFCC + 0x64 + record_offset);
            sdvScheduleAlter(D_0065AFCC + 0x384 + record_offset, D_0065AFCC + 0x398 + record_offset);

            if (_hitFlag != 0 || _hitSignal != 0) {
                unsigned int reaction_offset = record_offset + offsetof(SchedulerState, scriptBinding);
                signed char *binding = (signed char *)(reaction_offset + (unsigned int)table);

                if (binding[11] == 0) {
                    sefCreateReactionEffect();
                }
                binding[11] = (signed char)(binding[11] + 1);
            }
            if (!(GameLoopState.flags & SEF_DRAW_FLAG_PENDING)) {
                sdvDrawSpecial(D_0065B030 + record_offset);
            }

            if (effect_alive == 0) {
                if (_nowScheduler->flags & 0x4000) {
                    _nowScheduler->flags &= ~0x4000;
                    func_A2C408(0);
                }
                expired = (SchedulerState *)(record_offset + (unsigned int)_scheduler);
                if (expired->scriptBinding.link_id < 0) {
                    /* Rebased through the negated table address, as the original computes it. */
                unsigned int back_base = 0 - (unsigned int)_scheduler;

                    record = (SchedulerState *)(record_offset - back_base);
                    if ((record->flags & 0x20) == 0) {
                        sefFreeScheduler(index);
                        continue;
                    }
                }
                for (k = 0; k < 32; k++) {
                    sefDestroyEffectData(D_0065A9B0 + index * sizeof(SchedulerState) + k * sizeof(scheduler->effects[0]));
                }
                record_offset -= sizeof(SchedulerState);
                SLOT(scriptBinding.state) = SLOT(scriptBinding.link_id);
                {
                    SefEffectDescriptor *descriptor = (SefEffectDescriptor *)SLOT(inUse);
                    int init_flags = SLOT(initFlags);

                    control_address -= sizeof(SchedulerState);
                    scheduler--;
                    sefInitEffectTbl(descriptor, index, init_flags);
                }
                index -= 1;
                continue;
            } else {
                unsigned int state_offset = record_offset + offsetof(SchedulerState, scriptBinding);
                ScriptBinding *binding;
                SchedulerState *state;

                binding = (ScriptBinding *)(state_offset + (unsigned int)_scheduler);

                binding->state++;
                if (binding->state == 0x3ff) {
                    binding->state = 0x3fe;
                }
                state = (SchedulerState *)(record_offset + (unsigned int)_scheduler);
                if ((state->flags & 0x40) == 0) {
                    active_count += 1;
                }
            }
        }
        SLOT(flags) &= ~9;
    }

    sefSetLightFlag();
    return active_count > 0;
}

int sefIsDeadSchduler(unsigned int scheduler_index)
{
    if (scheduler_index >= 0x80) {
        return 1;
    }
    return *(int *)(_scheduler + 0x6b0 + scheduler_index * 0xab0) == 0;
}


/*
 * sefSetReverseDir (main:0x002e6380): when `category` falls in [0x18,0x20)
 * (categories 0x18..0x1F), normalizes `effect_id` (subtracting 0x64 for the
 * [0xB54,0xBB7) sub-range) and looks it up in the 10-entry revEft_3 table
 * (config/symbols/main.txt); a match sets _revDirZ and returns 1, otherwise
 * _revDirZ is cleared and the function returns 0. sefExecScheduler (still
 * assembly in this TU) is its only caller.
 */
static int sefSetReverseDir(int effect_id, int category)
{
    static int revEft_3[10] __asm__("revEft.3") = {
        0xB03, 0xB04, 0xAFD, 0xB07, 0xB05, 0xB32, 0xB31, 0xAFB, 0xAFE, 0xB06,
    };
    int normalized_id;
    unsigned int index;

    if ((unsigned int)(category - 0x18) < 8) {
        normalized_id = effect_id;
        if ((unsigned int)(effect_id - 0xB54) < 0x63) {
            normalized_id = effect_id - 0x64;
        }

        for (index = 0; index < 10; index++) {
            if (normalized_id == revEft_3[index]) {
                _revDirZ = 1;
                return 1;
            }
        }
    }

    _revDirZ = 0;
    return 0;
}

void sefCreateBattleActorTbl(int effect_no)
{
    SchedulerState *scheduler;
    int scheduler_index;
    int actor_index;

    if ((unsigned int)(effect_no - 0x9c4) < 0x11f) {
        return;
    }

    memset(_battleActor, 0, 0xda0);
    for (scheduler_index = 0; scheduler_index < 0x80; scheduler_index++) {
        scheduler = &((SchedulerState *)_scheduler)[scheduler_index];
        if (scheduler->inUse == 0) {
            continue;
        }
        if (scheduler->flags & 0x202) {
            continue;
        }
        if (scheduler->effectNo != effect_no) {
            continue;
        }

        if (scheduler->targetActor != 0) {
            for (actor_index = 0;
                 actor_index < _battleActor->count
                 && _battleActor->actors[actor_index].actor != scheduler->targetActor;
                 actor_index++) {
            }
            if (actor_index == _battleActor->count && actor_index < 24) {
                _battleActor->actors[actor_index].actor = scheduler->targetActor;
                _battleActor->actors[actor_index].schedulerLightCount = scheduler->targetLightCount;
                _battleActor->actors[actor_index].excluded = 1;
                _battleActor->count = actor_index + 1;
            }
        }

        if (scheduler->sourceActor != 0) {
            for (actor_index = 0;
                 actor_index < _battleActor->count
                 && _battleActor->actors[actor_index].actor != scheduler->sourceActor;
                 actor_index++) {
            }
            if (actor_index == _battleActor->count && actor_index < 24) {
                _battleActor->actors[actor_index].schedulerLightCount = scheduler->sourceLightCount;
                _battleActor->actors[actor_index].actor = scheduler->sourceActor;
                _battleActor->actors[actor_index].excluded = 0;
                _battleActor->count = actor_index + 1;
            }
        }
    }
}

extern Vector4 *MMathDivVectorS(Vector4 *destination, const Vector4 *source, float scale);
static inline SefBattleActorTbl *sefBattleTbl(void)
{
    return (SefBattleActorTbl *)((char *)D_00795100 - 0xD80);
}

static void sefInitBattlePrm(void)
{
    SefBattleActor *record;
    SefActor *actor;
    Vector4 *position;
    int count = 0;
    int i;

    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(D_00795100) : "memory");
    for (i = 0; i < sefBattleTbl()->count; i++) {
        record = &sefBattleTbl()->actors[i];
        actor = record->actor;
        record->lightCount = 0;
        record->hitSignal = 0;
        record->seSignal = 0;
        if (actor != 0 && *(short *)((char *)record + 0x84) == 0) {
            position = (Vector4 *)((char *)actor + 0x10);
            __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(D_00795100) : "memory");
            __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(position) : "memory");
            __asm__ __volatile__("vadd.xyz vf1, vf1, vf2" : : : "memory");
            __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(D_00795100) : "memory");
            count++;
        }
    }
    if (count != 0) {
        MMathDivVectorS((Vector4 *)D_00795100, (Vector4 *)D_00795100, (float)count);
    }
}

void sefCaclAllTarget(Vector4 *target)
{
    SefBattleActor *record;
    SefActor *actor;
    Vector4 *position;
    int count = 0;
    int i;

    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(target) : "memory");
    for (i = 0; i < _battleActor->count; i++) {
        record = &_battleActor->actors[i];
        actor = record->actor;
        if (actor != 0 && record->excluded == 0) {
            position = (Vector4 *)((char *)actor + 0x10);
            __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(target) : "memory");
            __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(position) : "memory");
            __asm__ __volatile__("vadd.xyz vf1, vf1, vf2" : : : "memory");
            __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(target) : "memory");
            count++;
        }
    }
    if (count != 0) {
        MMathDivVectorS(target, target, (float)count);
    }
}

/*
 * sefSetLightFlag (main:0x002e6738): walks every listed battle effect actor
 * and mirrors its record's light count into the actor's own flag word, so an
 * actor that no longer carries a light loses the bit again. LOCAL in the
 * original; sefAddLightActor, which keeps the counts, is its only caller.
 */
static void sefSetLightFlag(void)
{
    SefBattleActor *record;
    SefActor *actor;
    int i;

    for (i = 0; i < _battleActor->count; i++) {
        record = &_battleActor->actors[i];
        actor = record->actor;
        if (actor != 0 && record->lightCount > 0) {
            actor->flags |= SEF_ACTOR_LIGHT_FLAG;
        } else {
            actor->flags &= ~SEF_ACTOR_LIGHT_FLAG;
        }
    }
}

/*
 * sefSetHitSignal (main:0x002e67a0): raises the hit signal counter of the listed
 * battle effect actor whose handle is `actor`, and does nothing for a handle
 * the table does not list or for a null one. LOCAL in the original.
 */
static void sefSetHitSignal(SefActor *actor)
{
    SefBattleActor *record;
    int i;

    if (actor != 0) {
        for (i = 0; i < _battleActor->count; i++) {
            record = &_battleActor->actors[i];
            if (record->actor == actor) {
                record->hitSignal++;
                break;
            }
        }
    }
}

/*
 * sefIsHitActor (main:0x002e6808): reports whether the battle effect actor
 * table holds a hit signal for `actor`. sefSetHitSignal (main:0x002e67a0) is
 * the counter's only writer, and it finds its record the same way. A handle
 * the table does not list, and a null handle, report no signal.
 */
int sefIsHitActor(SefActor *actor)
{
    SefBattleActor *record;
    int i;

    if (actor != 0) {
        for (i = 0; i < _battleActor->count; i++) {
            record = &_battleActor->actors[i];
            if (record->actor == actor) {
                return record->hitSignal != 0;
            }
        }
    }
    return 0;
}

/*
 * sefSetSeSignal (main:0x002e6860): raises the sound-effect signal counter of the listed
 * battle effect actor whose handle is `actor`, and does nothing for a handle
 * the table does not list or for a null one. LOCAL in the original.
 */
static void sefSetSeSignal(SefActor *actor)
{
    SefBattleActor *record;
    int i;

    if (actor != 0) {
        for (i = 0; i < _battleActor->count; i++) {
            record = &_battleActor->actors[i];
            if (record->actor == actor) {
                record->seSignal++;
                break;
            }
        }
    }
}

/*
 * sefIsSeSignal (main:0x002e68c8): the sound-effect counterpart of
 * sefIsHitActor above, reading the second counter of the same record, the one
 * sefSetSeSignal (main:0x002e6860) increments.
 */
int sefIsSeSignal(SefActor *actor)
{
    SefBattleActor *record;
    int i;

    if (actor != 0) {
        for (i = 0; i < _battleActor->count; i++) {
            record = &_battleActor->actors[i];
            if (record->actor == actor) {
                return record->seSignal != 0;
            }
        }
    }
    return 0;
}

typedef struct SefLightSet {
    unsigned long long words[30];
} SefLightSet;
extern Vector4 _defLight[15];
extern void *MMathNormalizeVector2(void *destination, const Vector4 *source);
extern float MMathVectorDotProduct(const Vector4 *first, const Vector4 *second);

static void sefAddLightActor(const Vector4 *position, const SefLightInput *light)
{
    float range_squared = (float)light->range * 0.01f;
    int actor_index;
    SefBattleActor *battle_actor;
    Vector4 *lights;
    Vector4 actor_position;
    Vector4 difference;
    float distance_squared;
    float weight;

    range_squared *= range_squared;

    for (actor_index = 0; actor_index < _battleActor->count; actor_index++) {
        battle_actor = &_battleActor->actors[actor_index];
        if (battle_actor->actor == 0 || battle_actor->lightCount != 0) {
            continue;
        }

        sefGetNullPosition(&actor_position, battle_actor->actor, 0, 1);
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "lqc2 vf2, 0(%1)\n\t"
            "vsub.xyz vf1, vf1, vf2\n\t"
            "sqc2 vf1, 0(%2)"
            :
            : "r"(position), "r"(&actor_position), "r"(&difference)
            : "memory");
        distance_squared = MMathVectorDotProduct(&difference, &difference);
        if (!(distance_squared < range_squared)) {
            continue;
        }

        lights = battle_actor->actor->light_parameters;
        weight = (range_squared - distance_squared) / range_squared * 0.0078125f;
        if (battle_actor->lightCount == 0) {
            *(SefLightSet *)lights = *(SefLightSet *)_defLight;
            __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&battle_actor->light_accumulator) : "memory");
            MMathNormalizeVector2(&battle_actor->light_direction, &difference);
            __asm__ __volatile__("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(&lights[6]), "r"(&battle_actor->light_direction) : "$8", "memory");
            battle_actor->actor->flags |= SEF_ACTOR_LIGHT_FLAG;
            __asm__ __volatile__("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(&battle_actor->light_position), "r"(position) : "$8", "memory");
            __asm__ __volatile__("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"(&battle_actor->actor_position), "r"(&actor_position) : "$8", "memory");
        }

        __asm__ __volatile__(
            "mfc1 $2, %2\n\t"
            "lqc2 vf1, 0(%0)\n\t"
            "lqc2 vf3, 0(%1)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "qmtc2 $2, vf2\n\t"
            "vmulx.xyz vf1, vf1, vf2x\n\t"
            "vadd.xyz vf3, vf1, vf3\n\t"
            "sqc2 vf3, 0(%1)"
            :
            : "r"(light), "r"(&battle_actor->light_accumulator), "f"(weight)
            : "$2", "memory");
        battle_actor->actor->flags |= SEF_ACTOR_LIGHT_FLAG;
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "lqc2 vf2, 0(%1)\n\t"
            "vadd.xyz vf1, vf1, vf2\n\t"
            "sqc2 vf1, 0(%2)"
            :
            : "r"(&lights[5]), "r"(&battle_actor->light_accumulator), "r"(&lights[5]));
        battle_actor->lightCount++;
    }
}

extern void smInitilize(void *pool, unsigned int poolSize);
extern void sresInitMemoryRes(void);
extern void srsInitCdRead(void);
extern void svInitImageMapper(void);
extern void sefInitScheduler(void);
extern void scInitScript(void);
extern void sdvInitSpecialWork(void);
extern void sdvInitAmbient(void);
extern void sresLoadCommonMemory(void);

/*
 * sefInitEffect (main:0x002e6b60): the effect subsystem's one-time
 * bring-up. Initializes the effect memory pool (_eftBuffer, 0xd4800 bytes),
 * the resource/CD-read/particle-allocator/image-mapper/scheduler/script
 * subsystems, clears the battle parameter and battle-data tables, brings
 * up the special-work and ambient renderers and the common resource set,
 * clears the pending load queue and marks the subsystem initialized.
 */
void sefInitEffect(void)
{
    smInitilize(_eftBuffer, 0xD4800);
    sresInitMemoryRes();
    srsInitCdRead();
    sevInitPtAllocator();
    svInitImageMapper();
    sefInitScheduler();
    scInitScript();
    memset(_battlePrm, 0, 0x34);
    memset(_battleData, 0, 0x230);
    sdvInitSpecialWork();
    sdvInitAmbient();
    sresLoadCommonMemory();
    _sefLoadEftQue = 0;
    _initialize = 1;
}

typedef struct {
    unsigned char unmodeled_00[0x2c];
    void (*battleDrawCallback)(void);              /* +0x2c */
    unsigned char unmodeled_30[0x38 - 0x30];
    void (*cfDrawCallback)(void);                    /* +0x38 */
} SefRenderState;
extern SefRenderState sRender;
typedef struct {
    unsigned char unmodeled_00[0x38];
    int allocSize;                          /* +0x38 */
    unsigned char unmodeled_3c[0x10c - 0x3c];
    short pendingHandle;                      /* +0x10c */
} SefMemRes;
extern SefMemRes _srsMemRes;
extern void sdvInitAlters(void);
extern void sefDestroyEffect(void);
extern void sefDestroyEffectCf(void);
extern void sresLoadCfMemory(void);
extern void sefDrawEffect2D(void);
extern void *smAlloc(unsigned int size);

/*
 * sefInitEffectBattle (main:0x002e6c08): enters battle mode (_sefBattleMode
 * = 1), resets the alter and effect subsystems, installs sefDrawEffect2D as
 * the battle draw callback and clears the cf one, then allocates the
 * 0x40000-byte effect memory block if it is not already held.
 */
void sefInitEffectBattle(void)
{
    _sefBattleMode = 1;
    sdvInitAlters();
    sefDestroyEffect();

    sRender.battleDrawCallback = sefDrawEffect2D;
    sRender.cfDrawCallback = 0;
    if (_srsMemRes.allocSize == 0) {
        _srsMemRes.pendingHandle = -1;
        _srsMemRes.allocSize = (int)smAlloc(0x40000);
    }
}

/*
 * sefInitEffectCf (main:0x002e6c78): the character-file counterpart of
 * sefInitEffectBattle above -- leaves battle mode (_sefBattleMode = 0),
 * resets the cf-side effect and memory subsystems, then installs
 * sefDrawEffect2D as the cf draw callback and clears the battle one.
 */
void sefInitEffectCf(void)
{
    _sefBattleMode = 0;
    sefDestroyEffectCf();
    sresLoadCfMemory();

    sRender.battleDrawCallback = 0;
    sRender.cfDrawCallback = sefDrawEffect2D;
}

extern void tracePrint(char *fmt, ...);
void sefSetupEffect(void);
typedef struct {
    s16 id[3];
    s16 pad[6];
    s16 prm[3][3];
    s16 flag[3];
} SefBattlePrm;

void sefSetupPlayer(int id, int a, int b, int c)
{
  SefBattlePrm *bp;
  s16 *p;
  s16 *q;
  int i;
  if (id == 0xBD)
  {
    return;
  }
  p = ((SefBattlePrm *) _battlePrm)->id;
  for (i = 0; i < 3; i++)
  {
    if ((*(p++)) == id)
    {
      return;
    }
  }

  ;
  for (i = 0; i < 3; i++)
  {
    if (((SefBattlePrm *) _battlePrm)->id[i] == 0)
    {
      ((SefBattlePrm *) _battlePrm)->id[i] = id;
      ((SefBattlePrm *) _battlePrm)->prm[i][0] = a;
      ((SefBattlePrm *) _battlePrm)->prm[i][1] = b;
      ((SefBattlePrm *) _battlePrm)->prm[i][2] = c;
      ((SefBattlePrm *) _battlePrm)->flag[i] = 1;
      sefSetupEffect();
      return;
    }
  }

  tracePrint("\245\255\245\343\245\351\244\254\304\311\262\303\244\307\244\255\244\336\244\273\244\363", id);
}

extern void sresFreeReloaderMemoryNo(int no);
typedef struct {
    s16 pid[3];
    s16 id[3];
    s16 res[3];
    s16 pad[12];
    s16 flag[3];
    int boss;
} SefBattlePrmE;

void sefSetupEnemy(int id)
{
  s16 *p;
  int i;
  if (id <= 0)
  {
    return;
  }
  for (i = 0; i < 3; i++)
  {
    if (((SefBattlePrmE *) _battlePrm)->res[i] == id || ((SefBattlePrmE *) _battlePrm)->id[i] == id)
    {
      ((SefBattlePrmE *) _battlePrm)->id[i] = id;
      return;
    }
  }

  for (i = 0; i < 3; i++)
  {
    if (((SefBattlePrmE *) _battlePrm)->id[i] == 0)
    {
      if (((SefBattlePrmE *) _battlePrm)->res[i] > 0)
      {
        sresFreeReloaderMemoryNo(i + 3);
      }
      if (sefIsBossID(id))
      {
        ((SefBattlePrmE *) _battlePrm)->boss = 1;
      }
      ((SefBattlePrmE *) _battlePrm)->id[i] = id;
      ((SefBattlePrmE *) _battlePrm)->res[i] = id;
      ((SefBattlePrmE *) _battlePrm)->flag[i] = 1;
      sefSetupEffect();
      return;
    }
  }

  tracePrint("\305\250\245\255\245\343\245\351\244\254\304\311\262\303\244\307\244\255\244\336\244\273\244\363 : %d", id);
}


void sefReleaseID(int id)
{
  SefBattlePrm *b;
  int i;
  b = (SefBattlePrm *) _battlePrm;
  for (i = 0; i < 3; i++)
  {
    if (b->id[i] == id)
    {
      sresFreeReloaderMemoryNo(i);
      b->id[i] = 0;
      b->prm[i][0] = 0;
      b->prm[i][1] = 0;
      b->prm[i][2] = 0;
      b->flag[i] = 0;
      return;
    }
  }

  for (i = 0; i < 3; i++)
  {
    if (((SefBattlePrmE *) _battlePrm)->id[i] == id)
    {
      ((SefBattlePrmE *) _battlePrm)->res[i] = id;
      ((SefBattlePrmE *) _battlePrm)->id[i] = 0;
      ((SefBattlePrmE *) _battlePrm)->flag[i] = 0;
      return;
    }
  }

  tracePrint("\273\330\304\352\243\311\243\304\244\316\245\255\245\343\245\351(%d)\244\317\302\270\272\337\244\267\244\336\244\273\244\363!!", id);
}

extern void sresLoadBattleData(unsigned char *battle_prm);

/*
 * sefSetupEffect (main:0x002e6fe0): loads battle effect resources into
 * _battlePrm (main VA 0x00794340). sefSetupPlayer and sefSetupEnemy are its
 * only callers besides the seffectDebugBattle debug entry point.
 */
void sefSetupEffect(void) {
    sresLoadBattleData(_battlePrm);
}

/*
 * sefIsEntryBoss (main:0x002e7000): 0x00794370 is _battlePrm + 0x30
 * (_battlePrm at main VA 0x00794340, cleared to 0x34 bytes by sefInitEffect
 * and loaded by sresLoadBattleData in sefSetupEffect above). Compiled in
 * isolation, the original relocates directly against this narrower address
 * rather than folding +0x30 onto _battlePrm's own relocation, so this
 * declaration keeps the scaffold's own splat name.
 */
extern int D_00794370[];

int sefIsEntryBoss(void) {
    return D_00794370[0];
}

extern void scDestroyScriptAll(void);
extern void sdvDestroyAlters(void);
extern void sdvInitAmbient(void);
extern void sresFreeReloaderMemory(int reload_bgm);
void sefKillEffect(int effect_no);

/*
 * sefDestroyEffect (main:0x002e7010): tears down the whole battle effect
 * state -- clears the load queue, kills every running scheduler, resets the
 * ambient/script/reload-memory subsystems and the battle parameter/state/
 * actor tables, then destroys every alter. sresFreeReloaderMemory's own
 * argument selects the disk-side reloader (1). SimajiriTest and
 * sefInitEffectBattle are its callers.
 */
void sefDestroyEffect(void)
{
    _sefLoadEftQue = 0;
    sefKillEffect(-1);
    sdvInitAmbient();
    scDestroyScriptAll();
    sresFreeReloaderMemory(1);
    memset(_battlePrm, 0, 0x34);
    memset(_battleData, 0, 0x230);
    memset(_battleActor, 0, 0xDA0);
    sdvDestroyAlters();
}

/*
 * sefDestroyEffectCf (main:0x002e7088): the character-file variant of
 * sefDestroyEffect above, identical except sresFreeReloaderMemory's own
 * argument selects the memory-card-side reloader (0). seffectDebugCf,
 * seffectDebugBattle and sefInitEffectCf are its callers.
 */
void sefDestroyEffectCf(void)
{
    _sefLoadEftQue = 0;
    sefKillEffect(-1);
    sdvInitAmbient();
    scDestroyScriptAll();
    sresFreeReloaderMemory(0);
    memset(_battlePrm, 0, 0x34);
    memset(_battleData, 0, 0x230);
    memset(_battleActor, 0, 0xDA0);
    sdvDestroyAlters();
}

/*
 * sefCnvEtEffectNo (main:0x002e74e0) is still assembly in this TU and LOCAL
 * in the original, so it is declared static. It takes two parameters:
 * besides the effect number in a0 it tests a1 against 2 (effects 2024..2029
 * shift by 3) and against 1 (effects 2038..2041 shift by 2), so the same
 * encounter effect resolves to a per-character variant. Every call site
 * loads a1 first: sefIsFinishEffect2 (below) passes the scheduler record's
 * character_id; seffectDebugDb (main:0x002dd038) passes 1,
 * seffectDebugBattle (main:0x002dd568) its debug character and ov01
 * scenarioBatExecPhase10 (0x00a0bf34) calcUPGet(...)->charaId, those three
 * through sefLoadEffect.
 */
static int sefCnvEtEffectNo(int effect_no, int character_id);
extern void svFileLoadScript(int mode, int effect_no);

/*
 * sefLoadEffect (main:0x002e7100): both parameters stay live in a0/a1 across
 * the call (neither register is rewritten before the jal), so they are
 * forwarded unchanged to sefCnvEtEffectNo; all three callers load a1 before
 * calling sefLoadEffect. The tail call (j, not jal+jr) makes
 * svFileLoadScript's own return value sefLoadEffect's return value on that
 * path; the effect_no<=0 path falls straight through to the epilogue without
 * setting v0, so the original never uses this function's result and the
 * return type is void.
 */
void sefLoadEffect(int effect_no, int character_id)
{
    if (effect_no > 0) {
        svFileLoadScript(0, sefCnvEtEffectNo(effect_no, character_id));
    }
}

extern int srsLeaveCdRead(void);

int sefCheckLoad(void)
{
    if (_sefLoadEftQue != 0) {
        return 1;
    }
    return srsLeaveCdRead() > 0;
}

extern void scExecEffect(void);
extern void sdvExecAlters(void);

void sefExecEffect(void)
{
    scExecEffect();
    sdvExecAlters();
    _hitFlag = 0;
    _hitSignal = 0;
    _seSignal = 0;
}

/*
 * sefProgressEffect (main:0x002e7198): the entry test reads the parameter
 * register directly (blez a0) before it is copied into the loop counter, and
 * the counter is decremented after each call (the decrement is scheduled
 * into the call's own delay slot), matching the original's own instruction
 * order.
 */
void sefProgressEffect(int frame_count)
{
    int remaining;

    if (frame_count > 0) {
        remaining = frame_count;
        do {
            sefExecEffect();
            remaining--;
        } while (remaining != 0);
    }
}

extern Matrix4 _invView;
extern void svDrawScheduler(void);

/*
 * sefDrawEffect (main:0x002e71d0): while `_initialize` is set and
 * GameLoopState's flags word does not already carry SEF_DRAW_FLAG_PENDING,
 * recomputes the cached inverse-view matrix (_invView, sefCalcInvView) and
 * tail-calls svDrawScheduler. nmlModelFlush/nmlModelFlushSub (a different
 * TU) are its callers.
 */
void sefDrawEffect(void)
{
    if (_initialize != 0) {
        if (!(GameLoopState.flags & SEF_DRAW_FLAG_PENDING)) {
            sefCalcInvView(&_invView);
            svDrawScheduler();
        }
    }
}

extern void svDrawScheduler3D(int flags);

/*
 * sefDrawEffect3D (main:0x002e7220): while `_initialize` is set and
 * GameLoopState's flags word does not already carry SEF_DRAW_FLAG_PENDING,
 * calls svDrawScheduler3D with that same bit as its argument. xglRenderMove
 * is its only caller.
 */
void sefDrawEffect3D(void) {
    if (_initialize != 0) {
        if (!(GameLoopState.flags & SEF_DRAW_FLAG_PENDING)) {
            svDrawScheduler3D(SEF_DRAW_FLAG_PENDING);
        }
    }
}

extern void svDrawScheduler2D(int flags);

/*
 * sefDrawEffect2D (main:0x002e7260): the same gate as sefDrawEffect3D above,
 * calling svDrawScheduler2D instead.
 */
void sefDrawEffect2D(void) {
    if (_initialize != 0) {
        if (!(GameLoopState.flags & SEF_DRAW_FLAG_PENDING)) {
            svDrawScheduler2D(SEF_DRAW_FLAG_PENDING);
        }
    }
}

typedef struct {
    int a0;
    int a4;
    int a8;
    short size;
    short sizeCopy;
    short id;
    short idCopy;
    short effectNo;
    short pad16;
    short pad18;
    unsigned short flags;
} SefDeathPrm;
typedef struct {
    short id;
    short effectNo;
} SefBossDeathEnt;

static void sefCnvDeathEffectNo(SefDeathPrm *p) {
    static SefBossDeathEnt _sefBossDeathPrm_4[0x24] __asm__("_sefBossDeathPrm.4") = {
        { 0x97, 0xA7B }, { 0x98, 0xA7E }, { 0x99, 0xA81 }, { 0x9A, 0xA84 },
        { 0x9B, 0xA87 }, { 0x9C, 0xA8A }, { 0x9D, 0xA8D }, { 0x9E, 0xA90 },
        { 0x9F, 0xA93 }, { 0xA0, 0xA96 }, { 0xA1, 0xA99 }, { 0xA2, 0xA9C },
        { 0xA3, 0xA9F }, { 0xA4, 0xAA2 }, { 0xA5, 0xAA5 }, { 0xA6, 0xAA8 },
        { 0xA7, 0xAAB }, { 0xA8, 0xAAE }, { 0xA9, 0xAB1 }, { 0xAA, 0xAB4 },
        { 0xAB, 0xAB7 }, { 0xAC, 0xABA }, { 0xAD, 0xABD }, { 0xAE, 0xAC0 },
        { 0xAF, 0xAC3 }, { 0xB0, 0xAC6 }, { 0xB1, 0xAC9 }, { 0xB2, 0xACC },
        { 0xB3, 0xACF }, { 0xB4, 0xAD2 }, { 0xB5, 0xAD5 }, { 0xB6, 0xAD8 },
        { 0xB7, 0xADB }, { 0xB8, 0xA81 }, { 0xB9, 0xA84 }, { 0xBA, 0xAB4 },
    };
    int kind;
    short id;
    unsigned int i;

    if (p->effectNo != 0x4000) {
        return;
    }
    id = p->id;
    kind = 0;
    if (sefIsBossID(id)) {
        for (i = 0; i < 0x24; i++) {
            if (p->id == _sefBossDeathPrm_4[i].id) {
                p->effectNo = _sefBossDeathPrm_4[i].effectNo;
                return;
            }
        }
    } else {
        kind = 0;
        if (p->flags & 0x2000) {
            kind = 1;
        } else if (p->flags & 0x1000) {
            kind = 2;
        }
    }
    p->effectNo = kind * 3 + sefGetSizeOffset(p->size) + 0xA2B;
    p->a4 = p->a0;
    p->idCopy = p->id;
    p->sizeCopy = p->size;
}

static float sefRandf(void);
extern int ACT_jointGetAccessories(void *actor, unsigned int id);
typedef struct {
    char pad[0x80];
    int actor;
    int pad84;
    unsigned short hit;
    char pad8a[6];
} SefHitEnt;
typedef struct {
    char pad[0x80];
    int actor;
    char pad84[8];
    unsigned short sig;
    char pad8e[2];
} SefSeEnt;
static unsigned short steftTbl[17] = {
    0x0000, 0x09CE, 0x09CA, 0x09C6, 0x09DE, 0x09D6, 0x09D2, 0x09DA,
};
int srsGetBossWaitEftNo(int no);

static void sefCnvWaitEffectNo(char *p)
{
  unsigned long new_var;
  unsigned short id;
  if ((*((short *) (p + 0x14))) != 0x4001)
  {
    return;
  }
  id = *((unsigned short *) (p + 0x10));
  new_var = 0xffff;
  if (((unsigned) (id - 1)) < 0x10)
  {
    *((short *) (p + 0x14)) = steftTbl[(short) id];
  }
  else
    if (((id - 0x97) & new_var) < 0x24)
  {
    *((short *) (p + 0x14)) = srsGetBossWaitEftNo((short) id);
  }
  else
  {
    *((short *) (p + 0x14)) = 0;
  }
}

void sefDeleteEffectWait(int arg0)
{
    unsigned char buf[0x24];
    unsigned int off;
    unsigned char *base;
    int i;
    unsigned char *rec;

    memset(buf, 0, 0x24);
    *(short *)(buf + 0x10) = arg0;
    *(short *)(buf + 0x14) = 0x4001;
    sefCnvWaitEffectNo((char *)buf);
    if (*(short *)(buf + 0x14) > 0) {
        base = _scheduler;
        off = 0;
        i = 0x7F;
        rec = base + 0xA78;
        do {
            if (*(int *)(rec - 0x3C8) != 0 && *(short *)rec == *(short *)(buf + 0x14)) {
                sefDeleteEffect2((SchedulerState *)((int)off + (int)base));
            }
            i -= 1;
            rec += 0xAB0;
            off += 0xAB0;
        } while (i >= 0);
    }
}

static int sefCnvEtEffectNo(int effect_no, int character_id)
{
    if ((unsigned)(effect_no - 0x7EB) < 3) {
        if (character_id != 2) {
            effect_no -= 3;
        }
    } else if ((unsigned)(effect_no - 0x7E8) < 3) {
        if (character_id == 2) {
            effect_no += 3;
        }
    } else if ((unsigned)(effect_no - 0x7F8) < 2) {
        if (character_id != 1) {
            effect_no -= 2;
        }
    } else if ((unsigned)(effect_no - 0x7F6) < 2) {
        if (character_id == 1) {
            effect_no += 2;
        }
    }
    return effect_no;
}

extern void scCreateScript(SefEffectRequest *request);

void sefCreateEffect(SefEffectRequest *request)
{
    SefEffectRequest *script_request;

    if (request->effect_no > 0) {
        if (request->effect_no == 55) {
            request->special = 1;
        }
        script_request = (SefEffectRequest *)_battleData;
        *script_request = *request;
        __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(_battleData + 0x30) : "memory");
        sefCnvDeathEffectNo(script_request);
        scCreateScript(script_request);
    }
}

typedef struct {
    int w[9];
} SefEffectArg;
extern void *scAnalyzeScriptCf(int eftNo, int *out);

unsigned char *sefCreateEffect2(SefEffectArg *arg)
{
    int buf[4];
    unsigned char *sched;
    unsigned char *script;
    int idx;
    unsigned int flags;

    sched = 0;
    if (*(short *)((char *)arg + 0x14) <= 0
        || (*(SefEffectArg *)_battleData = *arg,
            sefCnvWaitEffectNo((char *)_battleData),
            *(short *)((char *)arg + 0x14) <= 0)) {
        return 0;
    }
    script = scAnalyzeScriptCf(*(short *)(_battleData + 0x14), buf);
    if (script == 0) {
        return 0;
    }
    {
        idx = sefCreateScheduler((int)(script + buf[0] * 2), (int)_battleData, 0, (int)script, -1);
        if (idx >= 0) {
            sched = _scheduler + idx * 0xab0;
            flags = *(unsigned int *)(sched + 0xa8c) | 0x240;
            *(unsigned int *)(sched + 0xa8c) = flags;
            if ((unsigned int)(*(unsigned short *)((char *)arg + 0x14) - 0xa46) < 7) {
                *(int *)(sched + 0x6bc) = arg->w[1];
                *(unsigned int *)(sched + 0xa8c) = (flags & ~0x400) | 2;
            }
        }
    }
    return sched;
}

/*
 * sefDeleteEffect2 (main:0x002e7748) tail-calls sefFreeSchedulerCf (main:
 * 0x002e5118, still assembly in this file) with its own argument unchanged:
 * no register is written before the j. sefDeleteEffectWait (main:0x002e7430)
 * passes the address of a _scheduler record in a0 and never reads a result
 * back from the call.
 */
extern void sefFreeSchedulerCf(SchedulerState *scheduler);

void sefDeleteEffect2(SchedulerState *scheduler)
{
    sefFreeSchedulerCf(scheduler);
}

/*
 * sefLoadEffectCf (main:0x002e7760) also sets up no registers before its
 * call: seffectDebugCf and getPeer_Effect (its only callers) load a cf id
 * into a0 and an effect number into a1 first, so both are real parameters
 * forwarded unchanged to srsFileLoadCf. The result is discarded; v0 is
 * cleared to 0 after the call rather than read from it.
 */
extern void srsFileLoadCf(int cf_id, int effect_no);

int sefLoadEffectCf(int cf_id, int effect_no)
{
    srsFileLoadCf(cf_id, effect_no);
    return 0;
}

/*
 * sefLoadEffectCfName (main:0x002e7780) converts effect_name with
 * srsEffectNameToID before forwarding to the same two-parameter
 * srsFileLoadCf established above, keeping the cf id live in a saved
 * register across the conversion call.
 */
extern int srsEffectNameToID(int effect_name);

int sefLoadEffectCfName(int cf_id, int effect_name)
{
    int effect_no = srsEffectNameToID(effect_name);

    if (effect_no > 0) {
        srsFileLoadCf(cf_id, effect_no);
    }
    return 0;
}

/*
 * sefLoadMemoryEffectCf (main:0x002e77c0) sets up no registers before
 * calling srsMemoryLoadCf, so it forwards whatever its own caller passed;
 * modeled here with the same three parameters srsMemoryLoadCf takes in
 * sefLoadMemoryEffectCfName below, the only call in this file with evidenced
 * arguments.
 */
extern void srsMemoryLoadCf(int cf_id, int effect_no, int buffer);

int sefLoadMemoryEffectCf(int cf_id, int effect_no, int buffer)
{
    srsMemoryLoadCf(cf_id, effect_no, buffer);
    return 0;
}

/*
 * sefLoadMemoryEffectCfName (main:0x002e77e0) converts effect_name with
 * srsEffectNameToID before forwarding to srsMemoryLoadCf, keeping the cf id
 * and the buffer argument live in saved registers across the conversion
 * call.
 */
int sefLoadMemoryEffectCfName(int cf_id, int effect_name, int buffer)
{
    int effect_no = srsEffectNameToID(effect_name);

    if (effect_no > 0) {
        srsMemoryLoadCf(cf_id, effect_no, buffer);
    }
    return 0;
}

extern int sefCreateScheduler(int effect_no, int value2, int value3, int value4, int value5);

static unsigned char *sefCreateEffectCf2(int effectNo, int position, void *orientation, int schedulerIndex)
{
    SchedulerState *scheduler = 0;
    short *script;
    int count;
    SefEffectRequest request;
    int index;

    if (effectNo <= 0) {
        return 0;
    }
    script = scAnalyzeScriptCf(effectNo, &count);
    if (script == 0) {
        return 0;
    }
    memset(&request, 0, sizeof(request));
    request.effect_no = effectNo;
    index = sefCreateScheduler((int)(script + count), (int)&request, position,
                               (int)script, schedulerIndex);
    if (index >= 0) {
        scheduler = (SchedulerState *)((unsigned char *)_scheduler + index * 0xab0);
        scheduler->flags |= 0x412;
        if (orientation != 0) {
            __asm__ __volatile__("lq $8, 0(%1)\n\tsq $8, 0(%0)" : : "r"((char *)scheduler + 0x90), "r"(orientation) : "$8", "memory");
        }
    }
    return (unsigned char *)scheduler;
}

extern unsigned char *sefCreateEffectCf2(int cf_id, int effect_no, void *value3, int value4);

/*
 * sefCreateEffectCf (main:0x002e7918): forwards to the CF effect allocator
 * sefCreateEffectCf2 with a fixed -1 for its own 4th parameter; cf_id and
 * effect_no follow the naming srsFileLoadCf's callers already evidence for
 * this file's other Cf functions (sefLoadEffectCf above). value3 is
 * forwarded unchanged; this allocation has no further evidence for it.
 */
void sefCreateEffectCf(int cf_id, int effect_no, int value3) {
    sefCreateEffectCf2(cf_id, effect_no, value3, -1);
}

/*
 * sefDeleteEffectCf (main:0x002e7930) is another tail call to
 * sefFreeSchedulerCf, identical in shape to sefDeleteEffect2 above;
 * CheckGameSymbol (one of its callers) loads a saved scheduler handle into
 * a0 and never reads a result back.
 */
void sefDeleteEffectCf(SchedulerState *scheduler)
{
    sefFreeSchedulerCf(scheduler);
}

/* The flag sefRewindEffectCf sets in SchedulerState's flags word (sef.h);
 * sefFreeScheduler (main:0x002e51a8) tests the same word with a wider mask
 * (0x202). */
#define SEF_SCHEDULER_REWIND_FLAG 1

void sefRewindEffectCf(SchedulerState *scheduler)
{
    if (scheduler != 0) {
        scheduler->flags |= SEF_SCHEDULER_REWIND_FLAG;
    }
}

typedef signed char s8;
typedef signed int s32;
#define NULL 0 /* draft_fix */

void sefClearEffectCf(s32 arg0)
{
  int new_var;
  s32 var_16;
  s32 var_19;
  s8 *var_17;
  s8 *var_2;
  s8 temp_2;
  u8 *temp_2_2;
  temp_2_2 = (0x820 * temp_2) + _lineData;
  if (arg0 != 0)
  {
    var_17 = arg0 + 0xB0;
    var_19 = 0x1F;
    do
    {
      if ((*((int *) (((char *) var_17) + 0x20))) != 0)
      {
        do
        {
          var_16 = 0;
          var_2 = var_17;
        }
        while (0);
        do
        {
          var_2 = var_17 + var_16;
          do
          {
            var_16 += 1;
            if (1)
            {
            }
          }
          while (0);
          temp_2 = *var_2;
          if (temp_2 >= 0)
          {
            temp_2_2 = (0x820 * temp_2) + _lineData;
            if (temp_2_2 != (new_var = 0))
            {
              sefDestroyLocalData(temp_2_2);
            }
          }
        }
        while (var_16 < 0x20);
      }
      var_19 -= 1;
      do
      {
      }
      while (0);
      var_17 += (unsigned char) 0x30;
    }
    while (var_19 >= 0);
  }
}

int sefIsFinishEffect(int effect_no)
{
    int record_offset;
    unsigned char *record;
    int index;
    int running;

    running = 0;
    if (func_A33248() != 0) {
        return 0;
    }
    record_offset = 0xa70;
    record = _scheduler + record_offset;
    for (index = 0; index < 0x80; index++) {
        if (*(int *)(record - 0x3c0) != 0
            && (*(int *)(record + 0x1c) & 0x40) == 0
            && (effect_no == 0 || *(short *)(record + 8) == effect_no)) {
            running++;
        }
        record += 0xab0;
    }
    return running == 0;
}

/*
 * The fields of a 0xab0-byte _scheduler record sefIsFinishEffect2 reads,
 * as displacements from the record + 0xa70 anchor its loop keeps in a
 * register (the same anchor sefKillEffect below and sefIsFinishEffect, still
 * assembly, use): the liveness word sefIsDeadSchduler tests at +0x6b0, the
 * owning character at +0xa74 (sefCnvEtEffectNo's character_id) and the
 * effect number at +0xa78 (the field sefKillEffect matches).
 */
#define SEF_RECORD_ANCHOR 0xa70
#define SEF_RECORD_LIVE (0x6b0 - SEF_RECORD_ANCHOR)
#define SEF_RECORD_CHARACTER_ID (0xa74 - SEF_RECORD_ANCHOR)
#define SEF_RECORD_EFFECT_NO (0xa78 - SEF_RECORD_ANCHOR)
#define SEF_SCHEDULER_RECORD_SIZE 0xab0
#define SEF_SCHEDULER_COUNT 0x80

/*
 * sefIsFinishEffect2 (main:0x002e7aa8): returns 1 when no live scheduler
 * record is still playing effect_no, comparing against each record's effect
 * number converted for its own character, so a per-character variant counts
 * as the effect it was started from. A non-positive effect_no is always
 * finished. As in sefKillEffect, record_offset stays a separate assignment:
 * folded into record's initializer, cc1 2.96 folds the 0xa70 into
 * _scheduler's own %lo relocation (one addiu instead of the original's two)
 * and no longer reproduces the original bytes. The unused index is counted
 * down (0x7f..0, bgez) by the compiler's own loop reversal.
 */
int sefIsFinishEffect2(int effect_no)
{
    int record_offset;
    unsigned char *record;
    int index;
    int running;

    running = 0;
    if (effect_no <= 0) {
        return 1;
    }
    record_offset = SEF_RECORD_ANCHOR;
    record = _scheduler + record_offset;
    for (index = 0; index < SEF_SCHEDULER_COUNT; index++) {
        if (*(int *)(record + SEF_RECORD_LIVE) != 0
            && sefCnvEtEffectNo(*(short *)(record + SEF_RECORD_EFFECT_NO),
                                *(short *)(record + SEF_RECORD_CHARACTER_ID))
                   == effect_no) {
            running++;
        }
        record += SEF_SCHEDULER_RECORD_SIZE;
    }
    return running == 0;
}

void sefSetLoadQue(int effect_no)
{
    _sefLoadEftQue = effect_no;
}

int *sefGetLoadQue(void)
{
    return &_sefLoadEftQue;
}

void sefExecLoadQue(void)
{
    if (_sefLoadEftQue > 0) {
        svFileLoadScript(0, _sefLoadEftQue);
    }
}

extern void sefFreeScheduler(unsigned int scheduler_index);

/*
 * sefKillEffect (main:0x002e7b80): the loop's running pointer is preset to
 * _scheduler + record_offset, the same anchor sefIsDeadSchduler's own 0x6b0
 * offset (this TU) and sefIsFinishEffect2's own +8 offset (still assembly in
 * this TU) resolve against, so both fields this loop dereferences -- the
 * 0x6b0 liveness word, reached at record-0x3c0 from here, and the 0xa78
 * effect number, at record+8 -- use small displacements from it.
 * record_offset stays a separate assignment (not folded into record's own
 * initializer): with it inlined, cc1 2.96 swaps which of record/index lands
 * in s0 versus s1 and no longer reproduces the original bytes.
 */
void sefKillEffect(int effect_no)
{
    int record_offset;
    unsigned char *record;
    int index;

    record_offset = 0xa70;
    record = _scheduler + record_offset;
    index = 0;
    do {
        if (*(int *)(record - 0x3c0) != 0
            && (effect_no < 0 || *(short *)(record + 8) == effect_no)) {
            sefFreeScheduler(index);
        }
        index++;
        record += 0xab0;
    } while (index < 0x80);
}

void sefHitEffect(void) {
    _hitFlag = 1;
}

typedef enum SefSaveActive {
    SEF_SAVE_INACTIVE,
    SEF_SAVE_ACTIVE
} SefSaveActive;

typedef enum SefSaveTail {
    SEF_SAVE_TAIL_NONE
} SefSaveTail;

typedef struct SefSavedEffect {
    Vector4 saved_vector;
    unsigned char unmodeled_010[0x608];
    int active;
    int copied_words[4];
    unsigned char unmodeled_62c[0x3C0];
    int tail_word;
    unsigned char unmodeled_9f0[0x0A];
    unsigned char saved_flags;
    unsigned char unmodeled_9fb[0xB5];
} SefSavedEffect;
extern SefSavedEffect D_0065A9A0[];

#ifndef offsetof
#define offsetof(type, member) ((unsigned int)&((type *)0)->member)
#endif
#define SEF_REL_FIELD(type, cursor, offset) (*(type *)((cursor) + (offset)))
#define SEF_BACK_FIELD(type, cursor, distance) (*(type *)((cursor) - (distance)))
#define SEF_SAVE_REL(member) ((int)offsetof(SefSavePrm, member) - (int)offsetof(SefSavePrm, saved_words[3]))
#define SEF_SCHED_REL(member) ((int)offsetof(SchedulerState, member) - (int)offsetof(SchedulerState, sourceActor))

void sefPushEffect(void)
{
    int index;
    Vector4 *destination_position;
    Vector4 *destination_orientation;
    Vector4 *destination_vector;
    unsigned char *scheduler;
    unsigned char *scheduler_vector0;
    unsigned char *scheduler_vector1;
    unsigned char *scheduler_vector2;
    unsigned char *save_cursor;
    unsigned char *scheduler_cursor;
    unsigned char *scheduler_base;
    unsigned char *save_base;
    int scheduler_offset;
    int save_offset;

    sefMemZero(_savePrm, sizeof(_savePrm));
    scheduler = _scheduler;
    scheduler_base = (unsigned char *)&((SchedulerState *)scheduler)->sourceActor;
    save_base = (unsigned char *)&_savePrm[0].saved_words[3];
    for (index = 0; index < SEF_SAVE_PRM_COUNT; index++) {
        scheduler_offset = index * sizeof(SchedulerState);
        save_offset = index * sizeof(SefSavePrm);
        scheduler_cursor = scheduler_base + scheduler_offset;
        save_cursor = save_base + save_offset;
        if (SEF_REL_FIELD(int, scheduler_cursor, SEF_SCHED_REL(inUse)) != 0) {
            destination_position = &_savePrm[index].position;
            scheduler_vector0 = scheduler + 0x80;
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "sq $8, 0(%0)"
                : : "r"(destination_position), "r"(scheduler_offset + (int)scheduler_vector0) : "$8", "memory");
            destination_orientation = &_savePrm[index].orientation;
            scheduler_vector1 = scheduler + 0x90;
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "sq $8, 0(%0)"
                : : "r"(destination_orientation), "r"(scheduler_offset + (int)scheduler_vector1) : "$8", "memory");
            destination_vector = &_savePrm[index].saved_vector;
            scheduler_vector2 = scheduler + 0xA0;
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "sq $8, 0(%0)"
                : : "r"(destination_vector), "r"(scheduler_offset + (int)scheduler_vector2) : "$8", "memory");
            SEF_BACK_FIELD(SefSaveActive, save_cursor, -SEF_SAVE_REL(active)) = SEF_BACK_FIELD(SefSaveActive, scheduler_cursor, -SEF_SCHED_REL(savedActive));
            SEF_REL_FIELD(int, save_cursor, SEF_SAVE_REL(saved_words[0])) = SEF_REL_FIELD(int, scheduler_cursor, SEF_SCHED_REL(targetActor));
            SEF_REL_FIELD(int, save_cursor, SEF_SAVE_REL(saved_words[1])) = SEF_REL_FIELD(int, scheduler_cursor, 0);
            SEF_REL_FIELD(int, save_cursor, SEF_SAVE_REL(saved_words[2])) = SEF_REL_FIELD(int, scheduler_cursor, SEF_SCHED_REL(parentRecord));
            SEF_REL_FIELD(int, save_cursor, 0) = SEF_REL_FIELD(int, scheduler_cursor, SEF_SCHED_REL(savedParameter));
            SEF_REL_FIELD(SefSaveTail, save_cursor, SEF_SAVE_REL(saved_words[4])) = SEF_REL_FIELD(SefSaveTail, scheduler_cursor, SEF_SCHED_REL(flags));
            SEF_REL_FIELD(unsigned short, save_cursor, SEF_SAVE_REL(effect_no)) = SEF_REL_FIELD(unsigned short, scheduler_cursor, SEF_SCHED_REL(effectNo));
            save_cursor[SEF_SAVE_REL(saved_flags)] = scheduler_cursor[SEF_SCHED_REL(extraByte)];
        }
    }
}

void sefPopEffect(void)
{
    unsigned char *save_params;
    unsigned char *save_params_10;
    unsigned char *save_params_20;
    unsigned char *effect_data;
    unsigned char *effect_cursor;
    unsigned char *save_cursor;
    int record_offset;
    int effect_offset;
    int index;

    sefKillEffect(-1);
    save_params = (unsigned char *)_savePrm;
    save_params_10 = save_params + 0x10;
    save_params_20 = save_params + 0x20;
    effect_data = (unsigned char *)D_0065A9A0;
    effect_cursor = effect_data + 0x620;
    save_cursor = save_params + 0x40;
    record_offset = 0;
    effect_offset = 0;
    for (index = 0; index < SEF_SAVE_PRM_COUNT; index++) {
        if (((SefSavePrm *)(save_cursor - 0x40))->active != 0) {
            sefCreateEffectCf2(*(short *)(save_cursor + 0x08),
                               record_offset + (int)save_params,
                               (void *)(record_offset + (int)save_params_10), index);
            __asm__ __volatile__(
                "lq $8, 0(%1)\n\t"
                "sq $8, 0(%0)"
                : : "r"(effect_offset + (int)effect_data), "r"(record_offset + (int)save_params_20) : "$8", "memory");
            *(SefSaveActive *)(effect_cursor - 0x08) = *(SefSaveActive *)(save_cursor - 0x10);
            *(int *)(effect_cursor - 0x04) = *(int *)(save_cursor - 0x0C);
            *(int *)(effect_cursor + 0x00) = *(int *)(save_cursor - 0x08);
            *(int *)(effect_cursor + 0x04) = *(int *)(save_cursor - 0x04);
            *(int *)(effect_cursor + 0x08) = *(int *)(save_cursor + 0x00);
            *(SefSaveTail *)(effect_cursor + 0x3CC) = *(SefSaveTail *)(save_cursor + 0x04);
            *(unsigned char *)(effect_cursor + 0x3DA) = *(save_cursor + 0x0A);
        }
        effect_cursor += 0xAB0;
        effect_offset += 0xAB0;
        save_cursor += 0x50;
        record_offset += 0x50;
    }
    sefMemZero(_savePrm, 0x2800);
    sefProgressEffect(10);
}

/* The effect tables are scaffold-owned, so these helpers expose their original
 * addresses without defining or renaming the backing data. */
unsigned char *sefGetBattleData(void)
{
    return _battleData;
}

/*
 * sefGetDmgNull returns the same two target ids sefCalcLocalMatrix selects
 * for its target == 529 case (this file, main:0x002e2d48, sef.c:351-352).
 * D_0079411C is _battleData + SEF_BATTLE_PHASE (declared in sef.h with the
 * measured relocation evidence for why it keeps the scaffold's own name).
 */
#define SEF_TARGET_DAMAGE_NULL_A 527
#define SEF_TARGET_DAMAGE_NULL_B 528

int sefGetDmgNull(void)
{
    return (D_0079411C[0] >= 2) ? SEF_TARGET_DAMAGE_NULL_A : SEF_TARGET_DAMAGE_NULL_B;
}

/* _ptAlloc's per-record byte size, the same 640 sefCalcLocalMatrix already
 * multiplies by at sef.c:359. */
#define SEF_PT_ALLOC_RECORD_SIZE 640

unsigned char *sevGetPtAllocator(int allocator_index)
{
    return &_ptAlloc[allocator_index * SEF_PT_ALLOC_RECORD_SIZE];
}

unsigned char *sevGetPtAllocator2(int allocator_index)
{
    return &_ptAlloc[allocator_index * SEF_PT_ALLOC_RECORD_SIZE];
}

/* _lineData's per-record byte size; evidenced only here. */
#define SEF_LINE_DATA_RECORD_SIZE 0x820

unsigned char *sefGetLineAdr(int line_index)
{
    if (line_index < 0) {
        return 0;
    }
    return &_lineData[line_index * SEF_LINE_DATA_RECORD_SIZE];
}

unsigned char *sefGetScheduler(void)
{
    return _scheduler;
}

/* Spark S1: static sbss scheduler-pointer getter.
 * Changed hypothesis: the original TU holds the current scheduler as a
 * file-static sbss pointer `_nowScheduler` (VA 0x004dc670, matching the ELF local
 * symbol) and `sefGetNowScheduler` returns it. Callers in sdvTransOffset
 * (0x002ed178) and sdvExecSpecial (0x002ed414) keep the result in s4/s3 and
 * dereference +2700, so the return type is a data pointer, not int. The
 * gp-relative `lw v0,-13568(gp)` (delay slot after `jr ra`) is EE GCC 2.96
 * small-data codegen with _gp == 0x004dfb70; hence flags -O2 -G8 with an
 * explicit _gp witness. Reviewed against the pinned m2c scaffold
 * (first_pass.c): m2c models the body as a gp-0x3500 load, untrusted only.
 * No ASM, no attributes.
 */
SchedulerState *sefGetNowScheduler(void)
{
    return _nowScheduler;
}

/* Same gate and record address as the target == 4 case in sefCalcLocalMatrix
 * (this file, sef.c:358-359), but on parameters rather than on _parentLine
 * and _nowParentLocal[0] directly. */
unsigned char *sefGetParentLine(int line, int parent_local)
{
    if (line >= 0 && parent_local >= 0) {
        return &_ptAlloc[parent_local * SEF_PT_ALLOC_RECORD_SIZE];
    }
    return 0;
}

/*
 * sefScaleIVectorAdd (main:0x002e7f18): converts the integer vector at
 * `scale_source` to float (VU0 macro-mode vitof0), scales its XYZ lanes by
 * `scale` (moved into vf2.x through the GPR the compiler's own mfc1
 * materializes for the "r" operand, matching the original's own transfer),
 * adds `addend`'s XYZ lanes and stores the sum to `dest`. Leaf, no calls,
 * no evidenced caller in this allocation.
 */
void sefScaleIVectorAdd(void *dest, void *scale_source, void *addend, float scale) {
    __asm__ __volatile__("lqc2 vf1, 0(%0)\n\tlqc2 vf3, 0(%1)\n\tvitof0.xyzw vf1, vf1\n\tqmtc2.ni %2, vf2\n\tvmulx.xyz vf1, vf1, vf2x\n\tvadd.xyz vf3, vf1, vf3\n\tsqc2 vf3, 0(%3)\n\tnop" :  : "r"(scale_source), "r"(addend), "r"(scale), "r"(dest) : "memory");
}

/*
 * config/units/math-main-w3-002e7f40.json: scale the XYZ lanes of an aligned
 * four-lane vector by the degrees-to-radians factor, leaving W untouched.
 * ee-vu-cop2 (docs/ps2-capabilities.md): the lane-selective VU0 macro-mode
 * multiply is what the C says, not an approximation of it; the compiler
 * materializes the .lit4 factor into an FPU register on its own (the `f`
 * operand), which is why the block has no lwc1 of its own.
 */
void sefDeg2RadVector(Vector4 *dst, Vector4 *src)
{
    __asm__ __volatile__("lqc2 $vf2, 0(%0)" : : "r"(src) : "memory");

    {
        register float degToRad asm("$f8") = 0.017453292f;

        __asm__ __volatile__(
            "mfc1 $8, %1\n\t"
            "qmtc2.ni $8, $vf1\n\t"
            "vmulx.xyz $vf2, $vf2, $vf1x\n\t"
            "sqc2 $vf2, 0(%0)\n\t"
            "nop"
            :
            : "r"(dst), "f"(degToRad)
            : "$8", "memory"
        );
    }
}

void sefMergeMatrixPos(Matrix4 dest, Matrix4 matrix, Vector4 *pos)
{
    __asm__ __volatile__(
        "lq $8, 0(%1)\n\t"
        "lq $9, 0x10(%1)\n\t"
        "lq $10, 0x20(%1)\n\t"
        "lq $11, 0(%2)\n\t"
        "sq $8, 0(%0)\n\t"
        "sq $9, 0x10(%0)\n\t"
        "sq $10, 0x20(%0)\n\t"
        "sq $11, 0x30(%0)\n\t"
        :
        : "r"(dest), "r"(matrix), "r"(pos)
        : "$8", "$9", "$10", "$11", "memory");
}

/*
 * sefLerpVectorB (main:0x002e7f88), identical in shape to sefLerpIVectorB
 * below, but leaves the interpolated result as float lanes (no vftoi0)
 * before storing it with sqc2. Reads two packed signed-halfword key records
 * -- `first` and, 10 bytes later, `second` -- each with the R5900 unaligned
 * doubleword ldl/ldr pair and widens the packed lanes to words with
 * pcgth/pextlh (the one evidenced MMI packed-halfword widening idiom,
 * user-authorized 2026-09-13: config/compiler-patterns.json CP-0169). Both
 * widened records are converted to float (vitof0.xyzw) and linearly
 * interpolated by `factor` (VF2 = VF1 + (VF2 - VF1) * factor, XYZ lanes
 * only), then stored as the complete four-slot result at `dest`. No caller
 * in this allocation.
 */
void sefLerpVectorB(void *first, Vector4 *dest, float factor)
{
    void *second = (char *)first + 10;

    __asm__ __volatile__(
        "mfc1 $9, %3\n\t"
        "qmtc2.ni $9, $vf3\n\t"
        "ldl $8, 7(%0)\n\t"
        "ldr $8, 0(%0)\n\t"
        "ldl $9, 7(%1)\n\t"
        "ldr $9, 0(%1)\n\t"
        "dsrl $8, $8, 16\n\t"
        "dsrl $9, $9, 16\n\t"
        "pcgth $10, $0, $8\n\t"
        "pcgth $11, $0, $9\n\t"
        "pextlh $10, $10, $8\n\t"
        "pextlh $11, $11, $9\n\t"
        "qmtc2.ni $10, $vf1\n\t"
        "qmtc2.ni $11, $vf2\n\t"
        "vitof0.xyzw $vf1, $vf1\n\t"
        "vitof0.xyzw $vf2, $vf2\n\t"
        "vsub.xyz $vf2, $vf2, $vf1\n\t"
        "vmulx.xyz $vf2, $vf2, $vf3x\n\t"
        "vadd.xyz $vf2, $vf2, $vf1\n\t"
        "sqc2 $vf2, 0(%2)\n\t"
        "nop"
        :
        : "r"(first), "r"(second), "r"(dest), "f"(factor)
        : "$8", "$9", "$10", "$11", "memory"
    );
}

/*
 * sefLerpIVectorB (main:0x002e7fe8), re-treated from the accepted assembly
 * group src/main/sef/sefLerpIVectorB.s toward readable C with constrained
 * ee-vu-cop2 inline assembly (route ee-vu-cop2, docs/ps2-capabilities.md).
 *
 * Reads two packed signed-halfword key records -- `first` and, 10 bytes
 * later, `second` (the layout the original's own `addiu $2,$4,10` computes;
 * that address is ordinary pointer arithmetic and stays in C) -- each with
 * the R5900 unaligned doubleword ldl/ldr pair and widens the packed lanes to
 * words with pcgth/pextlh (the one evidenced MMI packed-halfword widening
 * idiom, user-authorized 2026-09-13: config/compiler-patterns.json CP-0169
 * -- cc1 2.96 emits no MMI for any C spelling, so this reproduces the
 * original's own hand-written idiom, not compiler output). Both widened
 * records are converted to float (vitof0.xyzw) and linearly interpolated by
 * `factor` (VF2 = VF1 + (VF2 - VF1) * factor, XYZ lanes only), then converted
 * back to packed integer lanes (vftoi0.xyzw) and stored as the complete
 * four-slot result at `dest`. The original has a .globl directive, so the
 * emitted binding stays GLOBAL.
 */
void sefLerpIVectorB(void *first, Vector4 *dest, float factor)
{
    void *second = (char *)first + 10;

    __asm__ __volatile__(
        "ldl $8, 7(%0)\n\t"
        "ldr $8, 0(%0)\n\t"
        "ldl $9, 7(%1)\n\t"
        "ldr $9, 0(%1)\n\t"
        "mfc1 $10, %3\n\t"
        "qmtc2.ni $10, $vf3\n\t"
        "pcgth $10, $0, $8\n\t"
        "pcgth $11, $0, $9\n\t"
        "pextlh $10, $10, $8\n\t"
        "pextlh $11, $11, $9\n\t"
        "qmtc2.ni $10, $vf1\n\t"
        "qmtc2.ni $11, $vf2\n\t"
        "vitof0.xyzw $vf1, $vf1\n\t"
        "vitof0.xyzw $vf2, $vf2\n\t"
        "vsub.xyz $vf2, $vf2, $vf1\n\t"
        "vmulx.xyz $vf2, $vf2, $vf3x\n\t"
        "vadd.xyz $vf2, $vf2, $vf1\n\t"
        "vftoi0.xyzw $vf2, $vf2\n\t"
        "sqc2 $vf2, 0(%2)\n\t"
        "nop"
        :
        : "r"(first), "r"(second), "r"(dest), "f"(factor)
        : "$8", "$9", "$10", "$11", "memory"
    );
}

/* Effect names (the table in srs points at them). */
const char D_004CBFC0[16] = "boss_033";
const char D_004CBFD0[16] = "boss_032";
const char D_004CBFE0[16] = "boss_031";
const char D_004CBFF0[16] = "boss_030";
const char D_004CC000[16] = "boss_029";
const char D_004CC010[16] = "boss_028";
const char D_004CC020[16] = "boss_027";
const char D_004CC030[16] = "boss_026";
const char D_004CC040[16] = "boss_025";
const char D_004CC050[16] = "boss_024";
const char D_004CC060[16] = "boss_023";
const char D_004CC070[16] = "boss_022";
const char D_004CC080[16] = "boss_021";
const char D_004CC090[16] = "boss_020";
const char D_004CC0A0[16] = "boss_019";
const char D_004CC0B0[16] = "boss_018";
const char D_004CC0C0[16] = "boss_017";
const char D_004CC0D0[16] = "boss_016";
const char D_004CC0E0[16] = "boss_015";
const char D_004CC0F0[16] = "boss_014";
const char D_004CC100[16] = "boss_013";
const char D_004CC110[16] = "boss_012";
const char D_004CC120[16] = "boss_011";
const char D_004CC130[16] = "boss_010";
const char D_004CC140[16] = "boss_009";
const char D_004CC150[16] = "boss_008";
const char D_004CC160[16] = "boss_007";
const char D_004CC170[16] = "boss_006";
const char D_004CC180[16] = "boss_005";
const char D_004CC190[16] = "boss_004";
const char D_004CC1A0[16] = "boss_003";
const char D_004CC1B0[16] = "boss_002";
const char D_004CC1C0[16] = "boss_001";
const char D_004CC1D0[16] = "utso_007";
const char D_004CC1E0[16] = "utso_006";
const char D_004CC1F0[16] = "utso_005";
const char D_004CC200[16] = "utso_004";
const char D_004CC210[16] = "utso_003";
const char D_004CC220[16] = "utso_002";
const char D_004CC230[16] = "utso_001";
const char D_004CC240[16] = "utro_007";
const char D_004CC250[16] = "utro_006";
const char D_004CC260[16] = "utro_005";
const char D_004CC270[16] = "utro_004";
const char D_004CC280[16] = "utro_003";
const char D_004CC290[16] = "utro_002";
const char D_004CC2A0[16] = "utro_001";
const char D_004CC2B0[16] = "utre_006";
const char D_004CC2C0[16] = "utre_005";
const char D_004CC2D0[16] = "utre_004";
const char D_004CC2E0[16] = "utre_003";
const char D_004CC2F0[16] = "utre_002";
const char D_004CC300[16] = "utre_001";
const char D_004CC310[16] = "utma_014";
const char D_004CC320[16] = "utma_013";
const char D_004CC330[16] = "utma_012";
const char D_004CC340[16] = "utma_011";
const char D_004CC350[16] = "utma_010";
const char D_004CC360[16] = "utma_009";
const char D_004CC370[16] = "utma_008";
const char D_004CC380[16] = "utma_007";
const char D_004CC390[16] = "utma_006";
const char D_004CC3A0[16] = "utma_005";
const char D_004CC3B0[16] = "utma_004";
const char D_004CC3C0[16] = "utma_003";
const char D_004CC3D0[16] = "utma_002";
const char D_004CC3E0[16] = "utma_001";
const char D_004CC3F0[16] = "feso_004";
const char D_004CC400[16] = "feso_003";
const char D_004CC410[16] = "feso_002";
const char D_004CC420[16] = "feso_001";
const char D_004CC430[16] = "fero_001";
const char D_004CC440[16] = "fere_003";
const char D_004CC450[16] = "fere_002";
const char D_004CC460[16] = "fere_001";
const char D_004CC470[16] = "fema_006";
const char D_004CC480[16] = "fema_005";
const char D_004CC490[16] = "fema_004";
const char D_004CC4A0[16] = "fema_003";
const char D_004CC4B0[16] = "fema_002";
const char D_004CC4C0[16] = "fema_001";
const char D_004CC4D0[16] = "guno_114";
const char D_004CC4E0[16] = "guno_025";
const char D_004CC4F0[16] = "guno_024";
const char D_004CC500[16] = "guno_023";
const char D_004CC510[16] = "guno_022";
const char D_004CC520[16] = "guno_021";
const char D_004CC530[16] = "guno_020";
const char D_004CC540[16] = "guno_019";
const char D_004CC550[16] = "guno_018";
const char D_004CC560[16] = "guno_017";
const char D_004CC570[16] = "guno_016";
const char D_004CC580[16] = "guno_015";
const char D_004CC590[16] = "guno_014";
const char D_004CC5A0[16] = "guno_013";
const char D_004CC5B0[16] = "guno_012";
const char D_004CC5C0[16] = "guno_011";
const char D_004CC5D0[16] = "guno_010";
const char D_004CC5E0[16] = "guno_009";
const char D_004CC5F0[16] = "guno_008";
const char D_004CC600[16] = "guno_007";
const char D_004CC610[16] = "guno_006";
const char D_004CC620[16] = "guno_005";
const char D_004CC630[16] = "guno_004";
const char D_004CC640[16] = "guno_003";
const char D_004CC650[16] = "guno_002";
const char D_004CC660[16] = "guno_001";
const char D_004CC670[16] = "catherin";

/* File-scope globals, emitted after the function-local tables above. */
Matrix4 _invView __attribute__((aligned(16))) = { { 0.0f } };
Vector4 _defLight[15] = { { 0.0f } };
Matrix4 sefDummy __attribute__((aligned(64))) = { { 0.0f } };
Matrix4 sefDummy2 = { { 0.0f } };
